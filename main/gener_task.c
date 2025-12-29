#include "gener_task.h"
#include "msg_queue.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "gener_task";

#define GENER_TASK_STACK_SIZE 2048
#define GENER_TASK_PRIORITY   5

static void gener_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    msg_t msg;
    static uint8_t led_state = 0;
    static uint8_t green_led_state = 1;

    ESP_LOGI(TAG, "Gener task started");

    while (1) {
        if (msg_queue_receive(queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_LED) 
            {
                gpio_set_level(msg.data.led.gpio_num, msg.data.led.state);
                ESP_LOGD(TAG, "LED GPIO %d set to %d", 
                         msg.data.led.gpio_num, msg.data.led.state);
            } else if (msg.type == MSG_TYPE_KEY) 
            {
                if (msg.data.key.event == KEY_EVENT_SINGLE_CLICK) {
                    led_state = !led_state;
                    gpio_set_level(LED_RED_GPIO, led_state);
                    ESP_LOGI(TAG, "SC: RED LED toggled to %d", led_state);
                }

                if (msg.data.key.event == KEY_EVENT_LONG_PRESS) {
                    green_led_state = !green_led_state;
                    gpio_set_level(LED_GRE_GPIO, green_led_state);
                    ESP_LOGI(TAG, "LP: GREEN LED toggled to %d", green_led_state);
                }
            } else 
            {
                ESP_LOGW(TAG, "Received unknown message type: %d", msg.type);
            }
        }
    }
}

BaseType_t gener_task_create(QueueHandle_t queue)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Cannot create gener task: queue is NULL");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    BaseType_t result = xTaskCreate(
        gener_task,
        "gener_task",
        GENER_TASK_STACK_SIZE,
        (void *)queue,
        GENER_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create gener task");
    } else {
        ESP_LOGI(TAG, "Gener task created successfully");
    }

    return result;
}
