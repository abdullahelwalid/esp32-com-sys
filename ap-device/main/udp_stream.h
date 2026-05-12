#ifndef UDP_STREAM_H
#define UDP_STREAM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "esp_netif_ip_addr.h"

/* UDP port STA binds to; AP sends here. */
#ifndef UDP_STREAM_PORT
#define UDP_STREAM_PORT 3333
#endif

/* Create/bind UDP socket (call before esp_wifi_start() so DHCP cannot race ahead). */
void udp_stream_start(void);

/* SoftAP DHCP assigned an IP to a station — stream audio to that STA. */
void udp_stream_set_client_from_ap_sta(const esp_ip4_addr_t *ip, const uint8_t mac[6]);

/* Stop streaming if the STA that was receiving disconnects. */
void udp_stream_on_sta_disconnected(const uint8_t mac[6]);

bool udp_stream_has_client(void);

/* Send one UDP datagram: [u32 seq_le][payload bytes...] */
int udp_stream_send_seq_payload(uint32_t seq, const void *payload, size_t payload_len);

#endif /* UDP_STREAM_H */
