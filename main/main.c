/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "sdkconfig.h"
#include "board.h"
#include "msg_queue.h"

static const char *TAG = "main";

/* Message queue handles */
static QueueHandle_t s_led_queue = NULL;
static QueueHandle_t s_pwm_queue = NULL;

/* Default queue length */
#define MSG_QUEUE_LEN 10

/* LED blink interval in milliseconds */
#define LED_BLINK_INTERVAL_MS 3000

void app_main(void)
{
    ESP_LOGI(TAG, "Hello ESP32-C6!");
    
    /* Configure LED and key GPIOs */
    configure_led();
    configure_key();
    
    /* Configure PWM output */
    if (configure_pwm() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PWM");
    } else {
        ESP_LOGI(TAG, "PWM configured successfully");
    }
    
    /* Initialize LED message queue */
    s_led_queue = msg_queue_init(MSG_QUEUE_LEN);
    if (s_led_queue == NULL) {
        ESP_LOGE(TAG, "Failed to initialize LED message queue");
        return;
    }
    ESP_LOGI(TAG, "LED message queue initialized");
    
    /* Initialize PWM message queue */
    s_pwm_queue = msg_queue_init(MSG_QUEUE_LEN);
    if (s_pwm_queue == NULL) {
        ESP_LOGE(TAG, "Failed to initialize PWM message queue");
        return;
    }
    ESP_LOGI(TAG, "PWM message queue initialized");
    
    /* Create LED handling task */
    if (led_task_create(s_led_queue) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED task");
        return;
    }
    ESP_LOGI(TAG, "LED task created successfully");
    
    /* Create PWM handling task */
    if (pwm_task_create(s_pwm_queue) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create PWM task");
        return;
    }
    ESP_LOGI(TAG, "PWM task created successfully");
    
    /* Create key scanning task (sends to LED queue) */
    if (key_task_create(s_led_queue, KEY_GPIO) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create key task");
        return;
    }
    /* Set PWM queue for key task (double click events) */
    key_task_set_pwm_queue(s_pwm_queue);
    ESP_LOGI(TAG, "Key task created successfully");
    
    ESP_LOGI(TAG, "Message queue system initialized (LED and PWM queues separated)");
    
    /* Main loop - demonstrate LED control via message queue */
    uint8_t led_state = 0;
    while (1) {
        /* Toggle LED state */
        led_state = !led_state;
        
        /* Send LED control message through the LED queue */
        if (msg_send_led(s_led_queue, LED_RED_GPIO, led_state)) {
            ESP_LOGI(TAG, "Sent LED message: GPIO=%d, state=%d", LED_RED_GPIO, led_state);
        } else {
            ESP_LOGW(TAG, "Failed to send LED message");
        }
        
        vTaskDelay(pdMS_TO_TICKS(LED_BLINK_INTERVAL_MS));
    }
}
