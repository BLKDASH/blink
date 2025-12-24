/**
 * @file key_task.h
 * @brief Key Task for ESP32-C6
 */

#ifndef KEY_TASK_H
#define KEY_TASK_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the key scanning task
 * 
 * Creates a FreeRTOS task that scans the specified GPIO for key press
 * and release events, sending key messages to the queue.
 * 
 * @param led_queue Queue handle for sending LED-related key messages (single click, long press)
 * @param pwm_queue Queue handle for sending PWM-related key messages (double click)
 * @param gpio_num GPIO number of the key to scan
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure
 */
BaseType_t key_task_create(QueueHandle_t led_queue, QueueHandle_t pwm_queue, uint8_t gpio_num);

#ifdef __cplusplus
}
#endif

#endif /* KEY_TASK_H */
