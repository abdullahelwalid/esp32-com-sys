#include "nvs_flash.h"

#include "sta_wifi.h"
#include "tx_i2s.h"
#include "signal_led.h"
#include "udp_audio_rx.h"

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    sta_wifi_init();
    signal_led_init();
    tx_i2s_init();
    tx_i2s_play_test_tone();
    udp_audio_rx_start();
}
