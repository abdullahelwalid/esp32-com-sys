#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_netif_types.h"

#include "udp_stream.h"
/* WIFI AP Config */
#define WIFI_SSID "Revtalk"
#define WIFI_PASSWORD "revpassword"
#define WIFI_CHANNEL 1
#define MAX_STA_CONN 2

static const char *TAG = "Revtalk";

char ip_addrs[2][16];  // To store IP addresses of connected stations

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
		// Store the IP address of the connected station
		ip_addrs[event->aid - 1][0] = '\0';  // Clear previous IP address
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
		// Clear the stored IP address of the disconnected station
		ip_addrs[event->aid - 1][0] = '\0';
        udp_stream_on_sta_disconnected(event->mac);
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == IP_EVENT_AP_STAIPASSIGNED) {
        ip_event_ap_staipassigned_t *ev = (ip_event_ap_staipassigned_t *)event_data;
        udp_stream_set_client_from_ap_sta(&ev->ip, ev->mac);
    }
}


void wifi_init(void){
	// initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	  ESP_ERROR_CHECK(nvs_flash_erase());
	  ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    /* UDP socket must exist before Wi-Fi starts, or a fast STA can get DHCP before we bind. */
    udp_stream_start();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_AP_STAIPASSIGNED,
                                                        &ip_event_handler,
                                                        NULL,
                                                        NULL));
	wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_CHANNEL,
            .password = WIFI_PASSWORD,
            .max_connection = MAX_STA_CONN,
#ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
            .authmode = WIFI_AUTH_WPA3_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
#else /* CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT */
            .authmode = WIFI_AUTH_WPA2_PSK,
#endif
            .pmf_cfg = {
                    .required = true,
            },
#ifdef CONFIG_ESP_WIFI_BSS_MAX_IDLE_SUPPORT
            .bss_max_idle_cfg = {
                .period = WIFI_AP_DEFAULT_MAX_IDLE_PERIOD,
                .protected_keep_alive = 1,
            },
#endif
        },
    };
	if (strlen(WIFI_PASSWORD) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
			 WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL);
}
