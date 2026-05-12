#ifndef UDP_STREAM_H
#define UDP_STREAM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* UDP port used for hello + audio stream */
#ifndef UDP_STREAM_PORT
#define UDP_STREAM_PORT 3333
#endif

/* Start a UDP listener task. The host must send a "hello" packet first. */
void udp_stream_start(void);

/* True once we've received a hello and learned the host IP:port. */
bool udp_stream_has_client(void);

/* Send one UDP datagram: [u32 seq_le][payload bytes...] */
int udp_stream_send_seq_payload(uint32_t seq, const void *payload, size_t payload_len);

#endif /* UDP_STREAM_H */

