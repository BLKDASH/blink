/**
 * @file led_task.h
 * @brief LED Task for ESP32-C6
 */

#ifndef LED_TASK_H
#define LED_TASK_H

#include "freertos/FreeRTOS.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建LED任务
 * 
 * @return pdPASS成功，其他失败
 */
BaseType_t led_task_create(void);

/**
 * @brief 设置红色LED状态
 * 
 * @param state LED状态 (LED_RED_ON 或 LED_RED_OFF)
 */
void led_set_red(uint8_t state);

/**
 * @brief 设置绿色LED状态
 * 
 * @param state LED状态 (LED_GRE_ON 或 LED_GRE_OFF)
 */
void led_set_green(uint8_t state);

/**
 * @brief 切换红色LED状态
 */
void led_toggle_red(void);

/**
 * @brief 切换绿色LED状态
 */
void led_toggle_green(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_TASK_H */
