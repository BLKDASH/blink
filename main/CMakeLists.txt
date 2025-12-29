idf_component_register(SRCS "pwm_task.c" "led_task.c" "wifi_manager.c" "main.c" "board.c" "msg_queue.c" "key_task.c"
                       INCLUDE_DIRS "./include"
                       REQUIRES driver esp_wifi esp_netif nvs_flash esp_event)
