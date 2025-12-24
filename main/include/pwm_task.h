/**
 * @file pwm_task.h
 * @brief PWM Task for ESP32-C6
 */

#ifndef PWM_TASK_H
#define PWM_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the PWM handling task
 * 
 * Creates a FreeRTOS task that receives PWM messages from the queue
 * and controls the PWM duty cycle.
 * 
 * @param queue Queue handle for receiving PWM messages
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure
 */
BaseType_t pwm_task_create(QueueHandle_t queue);

#ifdef __cplusplus
}
#endif

#endif /* PWM_TASK_H */
