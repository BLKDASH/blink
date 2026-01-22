/**
 * @file system_monitor_task.c
 * @brief System health monitor implementation
 */

#include "system_monitor_task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"
#include "bt_spp.h"

static const char *TAG = "sys_monitor";

#define SYSTEM_MONITOR_TASK_STACK_SIZE 4096
#define SYSTEM_MONITOR_TASK_PRIORITY   1
#define MONITOR_INTERVAL_MS            60000  /* 每60秒检查一次 */
#define MIN_FREE_HEAP_BYTES            20480  /* 最小可用堆内存：20KB */

/* 需要监控的任务名称列表 */
static const char *s_monitored_tasks[] = {
    "led_task",
    "key_task",
    "servo_task",
    "queue_monitor",
    "watchdog",
    "sys_monitor",
    "wifi_msg_task",
    "mqtt_start",
    "Tmr Svc",  /* FreeRTOS Timer Service */
};
static const size_t s_monitored_tasks_count = sizeof(s_monitored_tasks) / sizeof(s_monitored_tasks[0]);

/**
 * @brief 系统监控任务
 */
static void system_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "System monitor task started, check interval: %d ms, stack size: %d bytes", 
             MONITOR_INTERVAL_MS, SYSTEM_MONITOR_TASK_STACK_SIZE * sizeof(StackType_t));
    
    /* 启动后延迟一段时间再开始监控 */
    vTaskDelay(pdMS_TO_TICKS(30000));
    
    while (1) {
        /* 1. 检查堆内存 */
        size_t free_heap = esp_get_free_heap_size();
        size_t min_free_heap = esp_get_minimum_free_heap_size();
        
        ESP_LOGI(TAG, "Heap: free=%u bytes, min_free=%u bytes", (unsigned int)free_heap, (unsigned int)min_free_heap);
        
        if (free_heap < MIN_FREE_HEAP_BYTES) {
            ESP_LOGW(TAG, "Low memory warning! Free heap: %u bytes", (unsigned int)free_heap);
            bt_spp_log("[SYS] WARNING: Low memory! Free: %u bytes", (unsigned int)free_heap);
        }
        
        /* 2. 检查内部 RAM */
        size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Internal RAM: free=%u bytes", (unsigned int)free_internal);
        
        /* 3. 检查所有监控任务的栈使用情况 */
        ESP_LOGI(TAG, "=== Task Stack Usage ===");
        for (size_t i = 0; i < s_monitored_tasks_count; i++) {
            TaskHandle_t task_handle = xTaskGetHandle(s_monitored_tasks[i]);
            if (task_handle != NULL) {
                UBaseType_t stack_left = uxTaskGetStackHighWaterMark(task_handle);
                ESP_LOGI(TAG, "Task '%s': %u bytes stack remaining", 
                         s_monitored_tasks[i], (unsigned int)(stack_left * sizeof(StackType_t)));
                
                /* 如果栈剩余小于 512 字节，发出警告 */
                if (stack_left * sizeof(StackType_t) < 512) {
                    ESP_LOGW(TAG, "WARNING: Task '%s' has low stack! Only %u bytes left", 
                             s_monitored_tasks[i], (unsigned int)(stack_left * sizeof(StackType_t)));
                    bt_spp_log("[SYS] WARNING: Task '%s' low stack: %u bytes", 
                               s_monitored_tasks[i], (unsigned int)(stack_left * sizeof(StackType_t)));
                }
            }
        }
        
        /* 通过蓝牙发送系统状态 */
        bt_spp_log("[SYS] Heap: %u bytes, Uptime: %lu min", 
                   (unsigned int)free_heap, (unsigned long)((esp_log_timestamp() / 1000) / 60));
        
        /* 4. 打印任务统计信息（可选，仅在调试时启用） */
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
        char *task_list_buffer = pvPortMalloc(2048);
        if (task_list_buffer != NULL) {
            vTaskList(task_list_buffer);
            ESP_LOGI(TAG, "Task list:\n%s", task_list_buffer);
            vPortFree(task_list_buffer);
        }
#endif
        
        /* 5. 检查运行时间 */
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
