/**
 * @file led_task.c
 * @brief LED Task implementation
 * 
 * LED任务通过全局状态变量控制LED状态
 */

#include "led_task.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "led_task";

#define LED_TASK_STACK_SIZE 2048
#define LED_TASK_PRIORITY   5

/* 全局LED状态变量 */
static volatile uint8_t g_red_led_state = LED_RED_OFF;
static volatile uint8_t g_green_led_state = LED_GRE_ON;

/* 静态任务资源 */
static StackType_t led_task_stack[LED_TASK_STACK_SIZE];
static StaticTask_t led_task_buffer;

/**
 * @brief 设置红色LED状态
 */
void led_set_red(uint8_t state)
{
    g_red_led_state = state;
}

/**
 * @brief 设置绿色LED状态
 */
void led_set_green(uint8_t state)
{
    g_green_led_state = state;
}

/**
 * @brief 切换红色LED状态
 */
void led_toggle_red(void)
{
    g_red_led_state = (g_red_led_state == LED_RED_OFF) ? LED_RED_ON : LED_RED_OFF;
}

/**
 * @brief 切换绿色LED状态
 */
void led_toggle_green(void)
{
    g_green_led_state = (g_green_led_state == LED_GRE_OFF) ? LED_GRE_ON : LED_GRE_OFF;
}

/**
 * @brief LED任务主循环
 * 
 * 周期性检查全局状态变量并更新LED硬件状态
 */
static void led_task(void *pvParameters)
{
    static uint8_t last_red_state = LED_RED_OFF;
    static uint8_t last_green_state = LED_GRE_ON;
    
    ESP_LOGI(TAG, "LED task started (static allocation), monitoring LED state variables");
    
    // 初始化LED状态
    gpio_set_level(LED_RED_GPIO, g_red_led_state);
    gpio_set_level(LED_GRE_GPIO, g_green_led_state);
    
    while (1) {
        // 检查红色LED状态是否变化
        if (g_red_led_state != last_red_state) {
            gpio_set_level(LED_RED_GPIO, g_red_led_state);
            last_red_state = g_red_led_state;
            ESP_LOGD(TAG, "Red LED set to %s", g_red_led_state == LED_RED_ON ? "ON" : "OFF");
        }
        
        // 检查绿色LED状态是否变化
        if (g_green_led_state != last_green_state) {
            gpio_set_level(LED_GRE_GPIO, g_green_led_state);
            last_green_state = g_green_led_state;
            ESP_LOGD(TAG, "Green LED set to %s", g_green_led_state == LED_GRE_ON ? "ON" : "OFF");
        }
        
        // 50ms检查一次，响应足够快且不占用太多CPU
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

BaseType_t led_task_create(void)
{
    TaskHandle_t task_handle = xTaskCreateStatic(
        led_task,
        "led_task",
        LED_TASK_STACK_SIZE,
        NULL,
        LED_TASK_PRIORITY,
        led_task_stack,
        &led_task_buffer
    );

    if (task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create led task (static)");
        return pdFAIL;
    } else {
        ESP_LOGI(TAG, "LED task created successfully (static allocation)");
        return pdPASS;
    }
}
