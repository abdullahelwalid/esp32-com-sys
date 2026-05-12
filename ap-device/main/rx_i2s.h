#ifndef RX_I2S_H
#define RX_I2S_H

/* I2S configuration for microphone input */
void i2s_init(void);
void MicReadTask(void *param);

#endif // !RX_I2S_H
