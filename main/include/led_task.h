/**
 * @file led_task.h
 * @brief LED Task for ESP32-C6
 */

#ifndef LED_TASK_H
#define LED_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the LED handling task
 * 
 * Creates a FreeRTOS task that receives LED messages from the queue
 * and controls the corresponding GPIO levels.
 * 
 * @param queue Queue handle for receiving LED messages
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure
 */
BaseType_t led_task_create(QueueHandle_t queue);

#ifdef __cplusplus
}
#endif

#endif /* LED_TASK_H */
