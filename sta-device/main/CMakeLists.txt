idf_component_register(
    SRCS
        "revtalk-sta.c"
        "sta_wifi.c"
        "tx_i2s.c"
        "udp_audio_rx.c"
        "signal_led.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_netif nvs_flash driver lwip
)
