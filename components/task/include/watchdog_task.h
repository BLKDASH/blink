/**
 * @file watchdog_task.h
 * @brief System watchdog task - monitors system health and feeds hardware watchdog
 */

#ifndef WATCHDOG_TASK_H
#define WATCHDOG_TASK_H

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建看门狗任务
 * 
 * 监控系统健康状态并定期喂狗，防止系统完全卡死
 * 
 * @return pdPASS成功，其他失败
 */
BaseType_t watchdog_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_TASK_H */
