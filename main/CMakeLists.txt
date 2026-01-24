idf_component_register(SRCS "ha_mqtt.c" "bt_spp.c" "wifi_manager.c" "main.c" "board.c"
                       INCLUDE_DIRS "./include"
                       REQUIRES driver esp_wifi esp_netif nvs_flash esp_event bt mqtt bt_log_forwarder
                       PRIV_REQUIRES task)
