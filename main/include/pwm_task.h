/**
 * @file pwm_task.h
 * @brief PWM Task for ESP32-C6
 */

#ifndef PWM_TASK_H
#define PWM_TASK_H

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建PWM任务
 * 
 * @return pdPASS成功，其他失败
 */
BaseType_t pwm_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* PWM_TASK_H */
