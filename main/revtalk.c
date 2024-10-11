#include <stdio.h>
#include "driver/i2s_common.h"
#include "driver/i2s_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"


#define BLINK_LED 13

#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32

i2s_chan_handle_t tx_handle;

i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

// Function to initialize I2S
void i2s_init() {
    i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_SCK,
            .ws = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din = I2S_SD,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    i2s_channel_init_std_mode(tx_handle, &std_cfg);
    i2s_channel_enable(tx_handle);
}

// Task to read audio data from the microphone
void MicReadTask(void *param) {
    int16_t buffer[1024];
    size_t bytes_read;

    while (1) {
        i2s_channel_read(tx_handle, &buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);
		for (size_t i = 0; i < bytes_read / sizeof(int16_t); i++) {
			ESP_LOGI("Mic Data", "Sample[%zu]: %d", i, buffer[i]);
		}
        // Process audio data here if needed
    }
}

// Task to blink the LED
void BlinkLEDTask(void *param) {
    gpio_reset_pin(BLINK_LED);
    gpio_set_direction(BLINK_LED, GPIO_MODE_OUTPUT);
    while (1) {
        gpio_set_level(BLINK_LED, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(BLINK_LED, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Main application entry point
void app_main(void) {
    char *ourTaskName = pcTaskGetName(NULL);
    ESP_LOGI(ourTaskName, "Hello, Starting up!\n");

    i2s_init();  // Initialize I2S
    xTaskCreatePinnedToCore(&MicReadTask, "Mic Read Task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(&BlinkLEDTask, "Blink LED", 2048, NULL, 5, NULL, 1);
}

