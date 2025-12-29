idf_component_register(SRCS "wifi_manager.c" "main.c" "board.c" "msg_queue.c"
                             "task/key_task.c" "task/led_task.c" "task/pwm_task.c"
                       INCLUDE_DIRS "./include" "./task/include"
                       REQUIRES driver esp_wifi esp_netif nvs_flash esp_event)
