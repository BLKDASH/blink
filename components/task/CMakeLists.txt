idf_component_register(SRCS "key_task.c" "led_task.c" "pwm_task.c"
                       INCLUDE_DIRS "./include"
                       REQUIRES driver main)
