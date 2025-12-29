idf_component_register(SRCS "main.c" "board.c" "msg_queue.c" "gener_task.c" "key_task.c" "action_task.c"
                       INCLUDE_DIRS "./include"
                       REQUIRES driver)
