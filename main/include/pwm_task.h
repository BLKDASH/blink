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
 * @brief 创建PWM任务
 * 
 * @param pwm_queue PWM消息队列句柄
 * @param wifi_queue WiFi消息队列句柄（可为NULL）
 * @return pdPASS成功，其他失败
 */
BaseType_t pwm_task_create(QueueHandle_t pwm_queue, QueueHandle_t wifi_queue);

#ifdef __cplusplus
}
#endif

#endif /* PWM_TASK_H */
