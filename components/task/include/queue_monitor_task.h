/**
 * @file queue_monitor_task.h
 * @brief Queue health monitor task - restarts system if queues are full
 */

#ifndef QUEUE_MONITOR_TASK_H
#define QUEUE_MONITOR_TASK_H

#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建队列监控任务
 * 
 * 定期检查所有消息队列的健康状态，如果发现队列满了则重启系统
 * 
 * @return pdPASS成功，其他失败
 */
BaseType_t queue_monitor_task_create(void);

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_MONITOR_TASK_H */
