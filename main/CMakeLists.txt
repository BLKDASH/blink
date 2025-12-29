idf_component_register(SRCS "wifi_manager.c" "main.c" "board.c" "msg_queue.c"
                       INCLUDE_DIRS "./include"
                       REQUIRES driver esp_wifi esp_netif nvs_flash esp_event
                       PRIV_REQUIRES task)
