#include "tx_i2s.h"

#include "freertos/FreeRTOS.h"

#include "driver/i2s_std.h"

// Stream format (must match AP device streamer)
#define AUDIO_SAMPLE_RATE_HZ  16000

// MAX98357A pins (edit to match your wiring)
#define I2S_BCLK_GPIO  19
#define I2S_WS_GPIO    21
#define I2S_DOUT_GPIO  18

static i2s_chan_handle_t s_tx_handle;

void tx_i2s_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx_handle, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE_HZ),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_pol = false,
            .bit_shift = true,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_GPIO,
            .ws = I2S_WS_GPIO,
            .dout = I2S_DOUT_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx_handle));
}

void tx_i2s_write(const void *data, size_t len)
{
    size_t written = 0;
    ESP_ERROR_CHECK(i2s_channel_write(s_tx_handle, data, len, &written, portMAX_DELAY));
}

