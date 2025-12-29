#ifndef ACTION_TASK_H
#define ACTION_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

BaseType_t action_task_create(QueueHandle_t queue);

#ifdef __cplusplus
}
#endif

#endif /* ACTION_TASK_H */
