#include "udp_stream.h"

#include <string.h>
#include <errno.h>

#include "esp_log.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "esp_mac.h"

static const char *TAG = "udp_stream";

static int s_sock = -1;
static struct sockaddr_in s_client_addr;
static volatile bool s_has_client = false;
static uint8_t s_stream_mac[6];
static bool s_have_stream_mac = false;

void udp_stream_start(void)
{
    static bool started = false;
    if (started) {
        return;
    }
    started = true;

    s_has_client = false;
    s_have_stream_mac = false;
    memset(&s_client_addr, 0, sizeof(s_client_addr));

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "socket() failed: errno=%d", errno);
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
        return;
    }

    int sndbuf = 256 * 1024;
    (void)setsockopt(s_sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    ESP_LOGI(TAG, "UDP stream socket ready on port %d (target from DHCP)", UDP_STREAM_PORT);
}

void udp_stream_set_client_from_ap_sta(const esp_ip4_addr_t *ip, const uint8_t mac[6])
{
    if (s_sock < 0 || ip == NULL || mac == NULL) {
        return;
    }

    memset(&s_client_addr, 0, sizeof(s_client_addr));
    s_client_addr.sin_family = AF_INET;
    s_client_addr.sin_port = htons(UDP_STREAM_PORT);
    s_client_addr.sin_addr.s_addr = ip->addr;

    memcpy(s_stream_mac, mac, 6);
    s_have_stream_mac = true;
    s_has_client = true;

    char ipstr[16];
    inet_ntoa_r(s_client_addr.sin_addr, ipstr, sizeof(ipstr));
    ESP_LOGI(TAG, "stream target set (DHCP) %s:%d " MACSTR, ipstr, UDP_STREAM_PORT, MAC2STR(mac));
}

void udp_stream_on_sta_disconnected(const uint8_t mac[6])
{
    if (!s_have_stream_mac || mac == NULL) {
        return;
    }
    if (memcmp(mac, s_stream_mac, 6) != 0) {
        return;
    }
    s_has_client = false;
    s_have_stream_mac = false;
    ESP_LOGI(TAG, "stream target cleared (STA disconnected)");
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

    uint8_t hdr[4];
    hdr[0] = (uint8_t)(seq & 0xFF);
    hdr[1] = (uint8_t)((seq >> 8) & 0xFF);
    hdr[2] = (uint8_t)((seq >> 16) & 0xFF);
    hdr[3] = (uint8_t)((seq >> 24) & 0xFF);

    if (payload_len <= 1400) {
        uint8_t pkt[4 + 1400];
        memcpy(pkt, hdr, 4);
        memcpy(pkt + 4, payload, payload_len);
        int sent = sendto(s_sock, pkt, 4 + (int)payload_len, 0, (struct sockaddr *)&s_client_addr, sizeof(s_client_addr));
        return sent;
    }

    int sent = sendto(s_sock, hdr, 4, 0, (struct sockaddr *)&s_client_addr, sizeof(s_client_addr));
    if (sent < 0) {
        return sent;
    }
    return sendto(s_sock, payload, (int)payload_len, 0, (struct sockaddr *)&s_client_addr, sizeof(s_client_addr));
}
