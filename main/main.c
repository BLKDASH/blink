#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "board.h"
#include "msg_queue.h"
#include "gener_task.h"
#include "key_task.h"
#include "action_task.h"

static const char *TAG = "main";

static QueueHandle_t s_gener_queue = NULL;
static QueueHandle_t s_action_queue = NULL;

#define MSG_QUEUE_LEN 10
#define LED_BLINK_INTERVAL_MS 3000

void app_main(void)
{
    ESP_LOGI(TAG, "Hello ESP32-C6!");
    
    configure_led();
    configure_key();
    
    if (configure_pwm() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PWM");
    }
    
    s_gener_queue = msg_queue_init(MSG_QUEUE_LEN);
    if (s_gener_queue == NULL) {
        ESP_LOGE(TAG, "Failed to initialize gener queue");
        return;
    }
    
    s_action_queue = msg_queue_init(MSG_QUEUE_LEN);
    if (s_action_queue == NULL) {
        ESP_LOGE(TAG, "Failed to initialize action queue");
        return;
    }

    if (gener_task_create(s_gener_queue) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create gener task");
        return;
    }
    
    if (action_task_create(s_action_queue) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create action task");
        return;
    }
    
    if (key_task_create(s_gener_queue, s_action_queue, KEY_GPIO) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create key task");
        return;
    }
    
    ESP_LOGI(TAG, "System initialized");
    
    uint8_t led_state = 0;
    while (1) {
        led_state = !led_state;
        msg_send_led(s_gener_queue, LED_RED_GPIO, led_state);
        vTaskDelay(pdMS_TO_TICKS(LED_BLINK_INTERVAL_MS));
    }
}
