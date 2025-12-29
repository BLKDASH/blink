/**
 * @file led_task.c
 * @brief LED Task implementation
 */

#include "led_task.h"
#include "msg_queue.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "led_task";

#define LED_TASK_STACK_SIZE 2048
#define LED_TASK_PRIORITY   5

static void led_task(void *pvParameters)
{
    QueueHandle_t queue = msg_queue_get(QUEUE_LED);
    msg_t msg;
    static uint8_t red_led_state = LED_RED_OFF;
    static uint8_t green_led_state = LED_GRE_ON;

    ESP_LOGI(TAG, "LED task started");

    while (1) {
        if (msg_queue_receive(queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_LED) {
                gpio_set_level(msg.data.led.gpio_num, msg.data.led.state);
                ESP_LOGD(TAG, "LED GPIO %d set to %d", 
                         msg.data.led.gpio_num, msg.data.led.state);
            } else if (msg.type == MSG_TYPE_KEY) {
                if (msg.data.key.event == KEY_EVENT_SINGLE_CLICK) {
                    red_led_state = (red_led_state == LED_RED_OFF) ? LED_RED_ON : LED_RED_OFF;
                    gpio_set_level(LED_RED_GPIO, red_led_state);
                    ESP_LOGI(TAG, "SC: RED LED toggled to %s", red_led_state == LED_RED_ON ? "ON" : "OFF");
                }

                if (msg.data.key.event == KEY_EVENT_LONG_PRESS) {
                    green_led_state = (green_led_state == LED_GRE_OFF) ? LED_GRE_ON : LED_GRE_OFF;
                    gpio_set_level(LED_GRE_GPIO, green_led_state);
                    ESP_LOGI(TAG, "LP: GREEN LED toggled to %s", green_led_state == LED_GRE_ON ? "ON" : "OFF");
                }
            } else {
                ESP_LOGW(TAG, "Received unknown message type: %d", msg.type);
            }
        }
    }
}

BaseType_t led_task_create(void)
{
    QueueHandle_t queue = msg_queue_get(QUEUE_LED);
    if (queue == NULL) {
        ESP_LOGE(TAG, "Cannot create led task: queue not initialized");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    BaseType_t result = xTaskCreate(
        led_task,
        "led_task",
        LED_TASK_STACK_SIZE,
        NULL,
        LED_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create led task");
    } else {
        ESP_LOGI(TAG, "LED task created successfully");
    }

    return result;
}
