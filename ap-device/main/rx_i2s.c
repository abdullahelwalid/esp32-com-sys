#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2s_types.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "udp_stream.h"


/* Onboard LED: GPIO2 on many DevKit boards.*/
#define SIGNAL_LED_GPIO 2
/* 1 = Espressif DevKit-style (GPIO low = LED on).*/
#define SIGNAL_LED_ACTIVE_LOW 0

#define I2S_WS 14   // Word Select (WS) / LRCLK
#define I2S_SD 13   // Serial Data Input (SD)
#define I2S_SCK 12  // Bit Clock (BCLK)
#define SAMPLE_RATE 16000  // 8 kHz sample rate
#define SAMPLE_BUFFER_SIZE 512  // Number of samples in buffer

/* Relative gate: peak must exceed quiet_floor by this much (adds to quiet_floor/4). */
#define SIGNAL_MARGIN_ADD (120000)
/* Buffers to keep LED on after last over-threshold peak (~4 * 64 ms @ 8 kHz, 512 samples/buf). */
#define SIGNAL_HOLD_BUFFERS 4

static i2s_chan_handle_t rx_handle;  // handle for I2S microphone input


static i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

/* Slightly stronger than >>16 if mic energy sits low (saturate to int16). */
#define MIC_PCM_SHIFT 15

static void signal_led_set(bool on)
{
#if SIGNAL_LED_ACTIVE_LOW
    gpio_set_level(SIGNAL_LED_GPIO, on ? 0 : 1);
#else
    gpio_set_level(SIGNAL_LED_GPIO, on ? 1 : 0);
#endif
}

static void signal_led_init(void)
{
    gpio_reset_pin(SIGNAL_LED_GPIO);
    gpio_set_direction(SIGNAL_LED_GPIO, GPIO_MODE_OUTPUT);
    signal_led_set(false);
}

void i2s_init() {
    // Initialize led
    signal_led_init();
    chan_cfg.dma_desc_num = 10;
    chan_cfg.dma_frame_num = 512;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &rx_handle);  // Use rx_handle
    if (err != ESP_OK) {
        ESP_LOGE("I2S", "Failed to create I2S channel: %d", err);
        ESP_ERROR_CHECK(err);
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE), // 8 kHz sample rate
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT, // 32-bit samples
			.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT, // 32-bit slots
            .slot_mode = I2S_SLOT_MODE_MONO,           // Mono mode (since only left is used)
            .slot_mask = I2S_STD_SLOT_LEFT,            // Capture only the left channel
            .ws_pol = false,                           // WS polarity normal (Left when WS=0)
            .bit_shift = true,                         // MSB-aligned
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK,
            .ws = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_SD, // Data in from microphone
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    err = i2s_channel_init_std_mode(rx_handle, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE("I2S", "Failed to initialize I2S channel: %d", err);
        ESP_ERROR_CHECK(err);
    }
    err = i2s_channel_enable(rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE("I2S", "Failed to enable I2S channel: %d", err);
        ESP_ERROR_CHECK(err);
    }
}




void MicReadTask(void *param) {
    int32_t buffer[SAMPLE_BUFFER_SIZE];  // Buffer to hold microphone samples
    int16_t out16[SAMPLE_BUFFER_SIZE];
    uint32_t seq = 0;
    int hold = 0;
    int32_t quiet_floor = 0;
    bool floor_ready = false;

	while (1) {
		size_t bytes_read = 0;
		i2s_channel_read(rx_handle, &buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
		int samples_read = bytes_read / sizeof(int32_t);

        // Convert 32-bit I2S samples to 16-bit PCM for UDP (saves bandwidth).
        for (int i = 0; i < samples_read; i++) {
            int32_t t = buffer[i] >> MIC_PCM_SHIFT;
            if (t > 32767) {
                t = 32767;
            }
            if (t < -32768) {
                t = -32768;
            }
            out16[i] = (int16_t)t;
        }

        // One UDP packet per chunk: [u32 seq LE][int16 PCM...]
        (void)udp_stream_send_seq_payload(seq++, out16, (size_t)samples_read * sizeof(int16_t));

		int32_t peak = 0;
		for (int i = 0; i < samples_read; i++) {
			int32_t v = buffer[i];
			int32_t a = (v < 0) ? -v : v;
			if (a > peak) {
				peak = a;
			}
		}

		/* Track a slow "quiet" level so a fixed threshold is not blind to I2S scale/noise. */
		if (!floor_ready) {
			quiet_floor = peak > 0 ? peak : 1;
			floor_ready = true;
		} else if (peak < quiet_floor) {
			quiet_floor = (31 * quiet_floor + peak) / 32;
		} else {
			quiet_floor += (peak - quiet_floor) >> 11;
		}

		int64_t margin = (int64_t)SIGNAL_MARGIN_ADD + ((int64_t)quiet_floor / 4);
		if ((int64_t)peak > (int64_t)quiet_floor + margin) {
			hold = SIGNAL_HOLD_BUFFERS;
		} else if (hold > 0) {
			hold--;
		}
		signal_led_set(hold > 0);
	}

}
