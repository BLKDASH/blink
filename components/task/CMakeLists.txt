idf_component_register(SRCS "system_monitor_task.c" "watchdog_task.c" "key_task.c" "led_task.c"
                       INCLUDE_DIRS "./include"
                       REQUIRES driver main esp_system)
