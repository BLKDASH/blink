#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "board.h"
#include "msg_queue.h"
#include "led_task.h"
#include "key_task.h"
#include "pwm_task.h"
#include "wifi_manager.h"

static const char *TAG = "main";

#define MSG_QUEUE_LEN 10

/**
 * @brief 按键事件回调处理函数
 */
static void key_event_handler(uint8_t gpio_num, key_event_t event)
{
    switch (event) {
        case KEY_EVENT_SINGLE_CLICK:
            // 单击：切换红灯
            msg_send_key_event(QUEUE_LED, gpio_num, event);
            break;
        case KEY_EVENT_LONG_PRESS:
            // 长按：切换绿灯
            msg_send_key_event(QUEUE_LED, gpio_num, event);
            break;
        case KEY_EVENT_DOUBLE_CLICK:
            // 双击：切换PWM档位
            msg_send_key_event(QUEUE_PWM, gpio_num, event);
            break;
        default:
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Hello ESP32-C6!");
    
    // 硬件初始化
    configure_led();
    configure_key();
    
    if (configure_pwm() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PWM");
    }
    
    // 消息队列初始化
    if (msg_queue_init_all(MSG_QUEUE_LEN) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize message queues");
        return;
    }
    
    // wifi管理器初始化
    if (wifi_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
    }
    // wifi消息处理
    wifi_manager_start_msg_task();

    // 创建业务任务
    if (led_task_create() != pdPASS) {
        ESP_LOGE(TAG, "Failed to create led task");
        return;
    }
    
    if (pwm_task_create() != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pwm task");
        return;
    }
    
    key_task_config_t key_cfg = {
        .gpio_num = KEY_GPIO,
        .callback = key_event_handler
    };
    if (key_task_create(&key_cfg) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create key task");
        return;
    }

    ESP_LOGI(TAG, "System initialized");
    
    vTaskDelete(NULL);
}
