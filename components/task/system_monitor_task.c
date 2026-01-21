/**
 * @file system_monitor_task.c
 * @brief System health monitor implementation
 */

#include "system_monitor_task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"

static const char *TAG = "sys_monitor";

#define SYSTEM_MONITOR_TASK_STACK_SIZE 3072
#define SYSTEM_MONITOR_TASK_PRIORITY   1
#define MONITOR_INTERVAL_MS            60000  /* 每60秒检查一次 */
#define MIN_FREE_HEAP_BYTES            20480  /* 最小可用堆内存：20KB */

/**
 * @brief 系统监控任务
 */
static void system_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "System monitor task started, check interval: %d ms", MONITOR_INTERVAL_MS);
    
    /* 启动后延迟一段时间再开始监控 */
    vTaskDelay(pdMS_TO_TICKS(30000));
    
    while (1) {
        /* 1. 检查堆内存 */
        size_t free_heap = esp_get_free_heap_size();
        size_t min_free_heap = esp_get_minimum_free_heap_size();
        
        ESP_LOGI(TAG, "Heap: free=%u bytes, min_free=%u bytes", free_heap, min_free_heap);
        
        if (free_heap < MIN_FREE_HEAP_BYTES) {
            ESP_LOGW(TAG, "Low memory warning! Free heap: %u bytes", free_heap);
        }
        
        /* 2. 检查内部 RAM */
        size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Internal RAM: free=%u bytes", free_internal);
        
        /* 3. 打印任务统计信息（可选，仅在调试时启用） */
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
        char *task_list_buffer = pvPortMalloc(2048);
        if (task_list_buffer != NULL) {
            vTaskList(task_list_buffer);
            ESP_LOGI(TAG, "Task list:\n%s", task_list_buffer);
            vPortFree(task_list_buffer);
        }
#endif
        
        /* 4. 检查运行时间 */
        uint32_t uptime_sec = esp_log_timestamp() / 1000;
        uint32_t uptime_hours = uptime_sec / 3600;
        uint32_t uptime_mins = (uptime_sec % 3600) / 60;
        ESP_LOGI(TAG, "Uptime: %u hours %u minutes", uptime_hours, uptime_mins);
        
        vTaskDelay(pdMS_TO_TICKS(MONITOR_INTERVAL_MS));
    }
}

BaseType_t system_monitor_task_create(void)
{
    BaseType_t result = xTaskCreate(
        system_monitor_task,
        "sys_monitor",
        SYSTEM_MONITOR_TASK_STACK_SIZE,
        NULL,
        SYSTEM_MONITOR_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create system monitor task");
    } else {
        ESP_LOGI(TAG, "System monitor task created successfully");
    }

    return result;
}
