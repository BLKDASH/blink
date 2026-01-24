/**
 * @file watchdog_task.c
 * @brief System watchdog task implementation
 */

#include "watchdog_task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/task.h"

static const char *TAG = "watchdog";

#define WATCHDOG_TASK_STACK_SIZE 3072
#define WATCHDOG_TASK_PRIORITY   1
#define WATCHDOG_TIMEOUT_S       30      /* 看门狗超时时间：30秒 */
#define WATCHDOG_FEED_INTERVAL_MS 10000  /* 喂狗间隔：10秒 */

static TaskHandle_t s_watchdog_task_handle = NULL;

/* 静态任务资源 */
static StackType_t watchdog_task_stack[WATCHDOG_TASK_STACK_SIZE];
static StaticTask_t watchdog_task_buffer;

/**
 * @brief 看门狗任务
 */
static void watchdog_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Watchdog task started, timeout: %d seconds, stack size: %d bytes", 
             WATCHDOG_TIMEOUT_S, WATCHDOG_TASK_STACK_SIZE * sizeof(StackType_t));
    
    /* 订阅当前任务到看门狗 */
    esp_err_t ret = esp_task_wdt_add(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add task to watchdog: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Task subscribed to watchdog, feeding every %d ms", WATCHDOG_FEED_INTERVAL_MS);
    
    while (1) {
        /* 喂狗 */
        ret = esp_task_wdt_reset();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to feed watchdog: %s", esp_err_to_name(ret));
        }
        
        /* 等待下次喂狗 */
        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_FEED_INTERVAL_MS));
    }
}

BaseType_t watchdog_task_create(void)
{
    /* 任务看门狗已经由系统初始化，直接创建任务订阅即可 */
    ESP_LOGI(TAG, "Task watchdog already initialized by system");
    
    /* 创建看门狗任务（静态分配） */
    s_watchdog_task_handle = xTaskCreateStatic(
        watchdog_task,
        "watchdog",
        WATCHDOG_TASK_STACK_SIZE,
        NULL,
        WATCHDOG_TASK_PRIORITY,
        watchdog_task_stack,
        &watchdog_task_buffer
    );

    if (s_watchdog_task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create watchdog task (static)");
        return pdFAIL;
    } else {
        ESP_LOGI(TAG, "Watchdog task created successfully (static allocation)");
        return pdPASS;
    }
}
