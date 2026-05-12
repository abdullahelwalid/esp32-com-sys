#ifndef TX_I2S_H
#define TX_I2S_H

#include <stdint.h>

/* Init I2S for MAX98357A (32-bit stereo MSB — matches ESP-IDF I2S TX examples). */
void tx_i2s_init(void);

/* Optional: brief beep after init to verify amp + wiring (disable in app_main if undesired). */
void tx_i2s_play_test_tone(void);

/* Mono int16 samples from network → stereo int32 I2S frames (blocking). */
void tx_i2s_write_mono_pcm16(const int16_t *samples, int n_samples);

/* Silence for N mono-sample-times (same duration as n_samples of mono int16). */
void tx_i2s_write_silence_mono_frames(int n_mono_samples);

#endif /* TX_I2S_H */
