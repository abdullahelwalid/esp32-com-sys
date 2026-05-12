#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rx_i2s.h"
#include "ap_wifi.h"


void app_main(void) {
    //ESP_LOGI("Main", "Starting up...");

	wifi_init();

    i2s_init();  // Initialize I2S for microphone and LED
    /* Higher priority reduces Wi-Fi jitter vs other tasks when streaming UDP. */
    xTaskCreatePinnedToCore(&MicReadTask, "Mic Read Task", 8192, NULL, 8, NULL, 1);
}
