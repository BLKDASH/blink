/**
 * @file led_task.h
 * @brief LED Task for ESP32-C6
 */

#ifndef LED_TASK_H
#define LED_TASK_H

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建LED任务
 * 
 * @return pdPASS成功，其他失败
 */
BaseType_t led_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_TASK_H */
