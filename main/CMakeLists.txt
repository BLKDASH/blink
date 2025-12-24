idf_component_register(SRCS "main.c" "board.c" "msg_queue.c" "led_task.c" "key_task.c" "pwm_task.c"
                       INCLUDE_DIRS "./include"
                       REQUIRES driver)
