idf_component_register(SRCS "ha_mqtt.c" "bt_spp.c" "wifi_manager.c" "main.c" "board.c" "msg_queue.c"
                       INCLUDE_DIRS "./include"
                       REQUIRES driver esp_wifi esp_netif nvs_flash esp_event bt mqtt
                       PRIV_REQUIRES task)
