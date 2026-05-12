#include "udp_audio_rx.h"

#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "lwip/sockets.h"

#include "sta_wifi.h"
#include "tx_i2s.h"
#include "signal_led.h"

#define UDP_PORT       3333

#define AUDIO_BYTES_PER_SAMPLE 2

#define SAMPLE_RATE_HZ         16000

/*
 * Recv timeout must be > one UDP audio frame period (~32 ms @ 16 kHz, 512 samples),
 * or recv will time out *between* packets and we inject silence → choppy/breaking audio.
 */
#define UDP_RECV_TIMEOUT_MS    80
#define NO_DATA_WARN_INTERVAL_MS 2000
#define RX_STATS_INTERVAL_MS     5000

#define SILENCE_SAMPLES_PER_POLL ((SAMPLE_RATE_HZ * UDP_RECV_TIMEOUT_MS) / 1000)

/* Must match AP `SAMPLE_BUFFER_SIZE` (samples per UDP frame). */
#define MAX_SAMPLES_PER_FRAME 512

/* If seq jumps more than this, resync (avoid long PLC bursts). */
#define MAX_PLC_GAP_FRAMES 16

static const char *TAG = "udp_audio_rx";

static void udp_rx_task(void *param)
{
    (void)param;

    sta_wifi_wait_connected(portMAX_DELAY);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) != 0) {
        ESP_LOGE(TAG, "bind() failed: errno=%d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "listening on UDP %d (AP will target this IP after DHCP)", UDP_PORT);

    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = UDP_RECV_TIMEOUT_MS * 1000,
    };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int rcvbuf = 256 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    uint32_t expected_seq = 0;
    bool have_seq = false;
    int16_t last_frame[MAX_SAMPLES_PER_FRAME];
    int last_frame_samples = 0;
    uint32_t plc_count_window = 0;
    bool logged_first_audio = false;
    bool got_valid_audio = false;
    uint32_t rx_packets_window = 0;
    TickType_t last_stat_log = xTaskGetTickCount();
    TickType_t last_no_data_warn = xTaskGetTickCount();

    uint8_t pkt[2048];

    while (1) {
        int r = recvfrom(sock, pkt, sizeof(pkt), 0, NULL, NULL);
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* No UDP: keep feeding zeros so I2S/DAC does not underrun (hash/static). */
                tx_i2s_write_silence_mono_frames(SILENCE_SAMPLES_PER_POLL);

                TickType_t now = xTaskGetTickCount();
                if ((now - last_no_data_warn) >= pdMS_TO_TICKS(NO_DATA_WARN_INTERVAL_MS)) {
                    last_no_data_warn = now;
                    if (!got_valid_audio) {
                        ESP_LOGW(TAG, "No audio yet (UDP recv timeout). Check AP is streaming after DHCP to this STA.");
                    } else {
                        ESP_LOGW(TAG, "Audio stalled: no UDP (feeding silence to I2S)");
                    }
                }
                continue;
            }
            ESP_LOGE(TAG, "recvfrom() failed: errno=%d", errno);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (r < 4) {
            continue;
        }

        uint32_t seq = (uint32_t)pkt[0] |
                       ((uint32_t)pkt[1] << 8) |
                       ((uint32_t)pkt[2] << 16) |
                       ((uint32_t)pkt[3] << 24);

        const uint8_t *payload = pkt + 4;
        int payload_len = r - 4;
        if ((payload_len % AUDIO_BYTES_PER_SAMPLE) != 0) {
            ESP_LOGW(TAG, "ignored malformed UDP: len=%d (payload not multiple of 2)", r);
            continue;
        }

        const int16_t *samples = (const int16_t *)payload;
        int sample_count = payload_len / 2;
        if (sample_count > MAX_SAMPLES_PER_FRAME) {
            sample_count = MAX_SAMPLES_PER_FRAME;
        }

        signal_led_update_from_pcm16(samples, sample_count);

        if (!have_seq) {
            expected_seq = seq;
            have_seq = true;
        }

        /* Late / duplicate packet: already played (or too old) — drop. */
        if (seq < expected_seq) {
            continue;
        }

        /* Lost frames: repeat last good frame (PLC) instead of silence — much less "swallowed" speech. */
        if (seq > expected_seq) {
            uint32_t gap = seq - expected_seq;
            if (gap > MAX_PLC_GAP_FRAMES) {
                ESP_LOGW(TAG, "seq jump %" PRIu32 " → resync (drop PLC)", gap);
                expected_seq = seq;
            } else {
                while (expected_seq < seq) {
                    if (last_frame_samples > 0) {
                        tx_i2s_write_mono_pcm16(last_frame, last_frame_samples);
                        plc_count_window++;
                    } else {
                        tx_i2s_write_silence_mono_frames(sample_count);
                    }
                    expected_seq++;
                }
            }
        }

        if (seq == expected_seq) {
            tx_i2s_write_mono_pcm16(samples, sample_count);
            memcpy(last_frame, samples, (size_t)sample_count * sizeof(int16_t));
            last_frame_samples = sample_count;
            expected_seq++;
        }

        got_valid_audio = true;
        rx_packets_window++;

        if (!logged_first_audio) {
            logged_first_audio = true;
            ESP_LOGI(TAG, "Receiving audio: first seq=%" PRIu32 " payload=%d bytes", seq, payload_len);
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_stat_log) >= pdMS_TO_TICKS(RX_STATS_INTERVAL_MS)) {
            ESP_LOGI(TAG, "RX ok: %" PRIu32 " pkts, PLC fills=%" PRIu32 " (last seq=%" PRIu32 ")",
                     rx_packets_window, plc_count_window, seq);
            rx_packets_window = 0;
            plc_count_window = 0;
            last_stat_log = now;
        }
    }
}

void udp_audio_rx_start(void)
{
    xTaskCreatePinnedToCore(udp_rx_task, "udp_rx", 8192, NULL, 8, NULL, 1);
}
