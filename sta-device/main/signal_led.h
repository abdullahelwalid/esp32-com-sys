#ifndef SIGNAL_LED_H
#define SIGNAL_LED_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void signal_led_init(void);

// Update LED state based on received PCM (int16) samples.
void signal_led_update_from_pcm16(const int16_t *samples, int sample_count);

#endif /* SIGNAL_LED_H */

