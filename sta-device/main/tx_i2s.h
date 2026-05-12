#ifndef TX_I2S_H
#define TX_I2S_H

#include <stddef.h>

// Init I2S speaker output (MAX98357A).
void tx_i2s_init(void);

// Write PCM bytes to I2S (blocking).
void tx_i2s_write(const void *data, size_t len);

#endif /* TX_I2S_H */

