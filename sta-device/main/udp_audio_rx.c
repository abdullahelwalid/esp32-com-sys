#include "udp_audio_rx.h"

#include <string.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

#include "sta_wifi.h"
#include "tx_i2s.h"
#include "signal_led.h"

// AP device default SoftAP IP from ESP-IDF
#define AP_DEVICE_IP   "192.168.4.1"
#define UDP_PORT       3333

// Packet format: [u32 seq little-endian][int16 payload...]
#define AUDIO_BYTES_PER_SAMPLE 2

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

    // Tell the AP device where to stream to (hello packet).
    struct sockaddr_in ap = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_PORT),
        .sin_addr.s_addr = inet_addr(AP_DEVICE_IP),
    };
    const char hello[] = "hello";
    (void)sendto(sock, hello, sizeof(hello), 0, (struct sockaddr *)&ap, sizeof(ap));
    ESP_LOGI(TAG, "sent hello to %s:%d; waiting for audio...", AP_DEVICE_IP, UDP_PORT);

    uint32_t expected_seq = 0;
    bool have_seq = false;

    uint8_t pkt[2048];

    while (1) {
        int r = recvfrom(sock, pkt, sizeof(pkt), 0, NULL, NULL);
        if (r < 0) {
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
            continue;
        }

        const int16_t *samples = (const int16_t *)payload;
        int sample_count = payload_len / 2;
        signal_led_update_from_pcm16(samples, sample_count);

        if (!have_seq) {
            expected_seq = seq;
            have_seq = true;
        }

        // Best-effort loss concealment: if we missed packets, play silence for the missing duration.
        if (seq > expected_seq) {
            uint32_t missing = seq - expected_seq;
            size_t silence_bytes = (size_t)payload_len * (size_t)missing;
            static uint8_t silence[512];
            memset(silence, 0, sizeof(silence));
            while (silence_bytes > 0) {
                size_t chunk = (silence_bytes > sizeof(silence)) ? sizeof(silence) : silence_bytes;
                tx_i2s_write(silence, chunk);
                silence_bytes -= chunk;
            }
        }
        expected_seq = seq + 1;

        tx_i2s_write(payload, (size_t)payload_len);
    }
}

void udp_audio_rx_start(void)
{
    xTaskCreatePinnedToCore(udp_rx_task, "udp_rx", 6144, NULL, 5, NULL, 1);
}

