/**
 * @file queue_monitor_task.c
 * @brief Queue health monitor task implementation
 */

#include "queue_monitor_task.h"
#include "msg_queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/task.h"

static const char *TAG = "queue_monitor";

#define QUEUE_MONITOR_TASK_STACK_SIZE 2048
#define QUEUE_MONITOR_TASK_PRIORITY   2
#define QUEUE_CHECK_INTERVAL_MS       5000   /* 每5秒检查一次 */
#define QUEUE_FULL_THRESHOLD_COUNT    3      /* 连续3次检测到队列满才重启 */

/**
 * @brief 队列监控任务
 */
static void queue_monitor_task(void *pvParameters)
{
    uint8_t full_count = 0;
    
    ESP_LOGI(TAG, "Queue monitor task started, check interval: %d ms", QUEUE_CHECK_INTERVAL_MS);
    
    /* 启动后延迟一段时间再开始检查，让系统稳定运行 */
    vTaskDelay(pdMS_TO_TICKS(10000));
    
    while (1) {
        bool healthy = msg_queue_check_health();
        
        if (!healthy) {
            full_count++;
            ESP_LOGW(TAG, "Queue health check failed! Count: %d/%d", 
                     full_count, QUEUE_FULL_THRESHOLD_COUNT);
            
            if (full_count >= QUEUE_FULL_THRESHOLD_COUNT) {
                ESP_LOGE(TAG, "Queues have been full for %d consecutive checks!", 
                         QUEUE_FULL_THRESHOLD_COUNT);
                ESP_LOGE(TAG, "System appears to be stuck, restarting in 2 seconds...");
                
                /* 延迟一下让日志输出完成 */
                vTaskDelay(pdMS_TO_TICKS(2000));
                
                /* 重启系统 */
                esp_restart();
            }
        } else {
            /* 队列健康，重置计数器 */
            if (full_count > 0) {
                ESP_LOGI(TAG, "Queues recovered, resetting full count");
                full_count = 0;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(QUEUE_CHECK_INTERVAL_MS));
    }
}

BaseType_t queue_monitor_task_create(void)
{
    BaseType_t result = xTaskCreate(
        queue_monitor_task,
        "queue_monitor",
        QUEUE_MONITOR_TASK_STACK_SIZE,
        NULL,
        QUEUE_MONITOR_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create queue monitor task");
    } else {
        ESP_LOGI(TAG, "Queue monitor task created successfully");
    }

    return result;
}
