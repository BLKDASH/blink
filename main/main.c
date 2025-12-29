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
// #define LED_BLINK_INTERVAL_MS 3000
// #define LED_BLINK_STACK_SIZE 2048
// #define LED_BLINK_PRIORITY 2

// static void led_blink_task(void *pvParameters)
// {
//     uint8_t led_state = 0;
    
//     while (1) {
//         led_state = !led_state;
//         msg_send_led(LED_RED_GPIO, led_state);
//         vTaskDelay(pdMS_TO_TICKS(LED_BLINK_INTERVAL_MS));
//     }
// }

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
    
    if (key_task_create(KEY_GPIO) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create key task");
        return;
    }

    // if (xTaskCreate(led_blink_task, "led_blink", LED_BLINK_STACK_SIZE, NULL, LED_BLINK_PRIORITY, NULL) != pdPASS) {
    //     ESP_LOGE(TAG, "Failed to create LED blink task");
    //     return;
    // }
    
    ESP_LOGI(TAG, "System initialized");
    
    vTaskDelete(NULL);
}
