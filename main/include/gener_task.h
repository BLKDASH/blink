#ifndef GENER_TASK_H
#define GENER_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

BaseType_t gener_task_create(QueueHandle_t queue);

#ifdef __cplusplus
}
#endif

#endif /* GENER_TASK_H */
