/**
 * @file system_monitor_task.h
 * @brief System health monitor - memory, stack, connectivity
 */

#ifndef SYSTEM_MONITOR_TASK_H
#define SYSTEM_MONITOR_TASK_H

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建系统监控任务
 * 
 * 监控系统健康状态：内存使用、堆栈水位、连接状态等
 * 
 * @return pdPASS成功，其他失败
 */
BaseType_t system_monitor_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_MONITOR_TASK_H */
