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

/**
 * @brief LED task function
 */
static void led_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    msg_t msg;
    static uint8_t led_state = 0;

    ESP_LOGI(TAG, "LED task started");

    while (1) {
        if (msg_queue_receive(queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_LED) {
                gpio_set_level(msg.data.led.gpio_num, msg.data.led.state);
                ESP_LOGD(TAG, "LED GPIO %d set to %d", 
                         msg.data.led.gpio_num, msg.data.led.state);
            } else if (msg.type == MSG_TYPE_KEY) {
                if (msg.data.key.event == KEY_EVENT_SINGLE_CLICK) {
                    led_state = !led_state;
                    gpio_set_level(LED_RED_GPIO, led_state);
                    ESP_LOGI(TAG, "Single click: LED toggled to %d", led_state);
                }
            } else {
                ESP_LOGW(TAG, "Received unknown message type: %d", msg.type);
            }
        }
    }
}

BaseType_t led_task_create(QueueHandle_t queue)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Cannot create LED task: queue is NULL");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    BaseType_t result = xTaskCreate(
        led_task,
        "led_task",
        LED_TASK_STACK_SIZE,
        (void *)queue,
        LED_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED task");
    } else {
        ESP_LOGI(TAG, "LED task created successfully");
    }

    return result;
}
