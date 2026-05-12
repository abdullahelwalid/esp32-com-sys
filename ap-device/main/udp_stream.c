#include "udp_stream.h"

#include <string.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"

static const char *TAG = "udp_stream";

static int s_sock = -1;
static struct sockaddr_in s_client_addr;
static volatile bool s_has_client = false;

static void udp_hello_task(void *arg)
{
    (void)arg;

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in bind_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(UDP_STREAM_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    if (bind(s_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
        ESP_LOGE(TAG, "bind() failed: errno=%d", errno);
        close(s_sock);
        s_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "listening for hello on UDP port %d", UDP_STREAM_PORT);

    uint8_t buf[64];
    while (1) {
        struct sockaddr_in from = {0};
        socklen_t from_len = sizeof(from);
        int r = recvfrom(s_sock, buf, sizeof(buf), 0, (struct sockaddr *)&from, &from_len);
        if (r < 0) {
            ESP_LOGE(TAG, "recvfrom() failed: errno=%d", errno);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        // Any packet counts as hello; remember sender as stream destination.
        s_client_addr = from;
        s_has_client = true;

        char ip[16];
        inet_ntoa_r(from.sin_addr, ip, sizeof(ip));
        ESP_LOGI(TAG, "hello from %s:%d (stream target set)", ip, ntohs(from.sin_port));
    }
}

void udp_stream_start(void)
{
    static bool started = false;
    if (started) {
        return;
    }
    started = true;

    s_has_client = false;
    memset(&s_client_addr, 0, sizeof(s_client_addr));

    xTaskCreatePinnedToCore(udp_hello_task, "udp_hello", 4096, NULL, 5, NULL, 0);
}

bool udp_stream_has_client(void)
{
    return s_has_client;
}

int udp_stream_send_seq_payload(uint32_t seq, const void *payload, size_t payload_len)
{
    if (!s_has_client || s_sock < 0) {
        return 0;
    }

    // Packet = [seq_le][payload]
    uint8_t hdr[4];
    hdr[0] = (uint8_t)(seq & 0xFF);
    hdr[1] = (uint8_t)((seq >> 8) & 0xFF);
    hdr[2] = (uint8_t)((seq >> 16) & 0xFF);
    hdr[3] = (uint8_t)((seq >> 24) & 0xFF);

    // Use a small stack buffer when possible; otherwise send in 2 calls.
    if (payload_len <= 1400) {
        uint8_t pkt[4 + 1400];
        memcpy(pkt, hdr, 4);
        memcpy(pkt + 4, payload, payload_len);
        int sent = sendto(s_sock, pkt, 4 + (int)payload_len, 0, (struct sockaddr *)&s_client_addr, sizeof(s_client_addr));
        return sent;
    }

    int sent = sendto(s_sock, hdr, 4, 0, (struct sockaddr *)&s_client_addr, sizeof(s_client_addr));
    if (sent < 0) return sent;
    return sendto(s_sock, payload, (int)payload_len, 0, (struct sockaddr *)&s_client_addr, sizeof(s_client_addr));
}

