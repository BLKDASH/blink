/**
 * @file key_task.c
 * @brief Key Task implementation with gesture detection
 */

#include "key_task.h"
#include "msg_queue.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "key_task";

#define KEY_TASK_STACK_SIZE      2048
#define KEY_TASK_PRIORITY        4
#define KEY_SCAN_INTERVAL_MS     10

/* Key gesture detection timing parameters (in milliseconds) */
#define LONG_PRESS_TIME_MS       1000
#define DOUBLE_CLICK_INTERVAL_MS 300
#define CLICK_MAX_TIME_MS        500

typedef struct {
    QueueHandle_t led_queue;
    QueueHandle_t pwm_queue;
    uint8_t gpio_num;
} key_task_params_t;

static key_task_params_t s_key_task_params;

/**
 * @brief Key task function with gesture detection
 */
static void key_task(void *pvParameters)
{
    key_task_params_t *params = (key_task_params_t *)pvParameters;
    uint8_t gpio_num = params->gpio_num;
    
    key_state_t state = KEY_STATE_IDLE;
    uint8_t last_key_level = 1;
    uint8_t current_key_level;
    TickType_t press_start_tick = 0;
    TickType_t release_tick = 0;
    bool long_press_sent = false;

    ESP_LOGI(TAG, "Key task started, scanning GPIO %d", gpio_num);

    while (1) {
        current_key_level = gpio_get_level(gpio_num);
        TickType_t current_tick = xTaskGetTickCount();

        switch (state) {
            case KEY_STATE_IDLE:
                if (current_key_level == 0 && last_key_level == 1) {
                    press_start_tick = current_tick;
                    long_press_sent = false;
                    state = KEY_STATE_PRESSED;
                }
                break;

            case KEY_STATE_PRESSED:
                if (current_key_level == 1 && last_key_level == 0) {
                    TickType_t press_duration = current_tick - press_start_tick;
                    
                    if (press_duration >= pdMS_TO_TICKS(LONG_PRESS_TIME_MS)) {
                        state = KEY_STATE_IDLE;
                    } else {
                        release_tick = current_tick;
                        state = KEY_STATE_WAIT_SECOND;
                    }
                } else if (current_key_level == 0) {
                    TickType_t press_duration = current_tick - press_start_tick;
                    
                    if (press_duration >= pdMS_TO_TICKS(LONG_PRESS_TIME_MS) && !long_press_sent) {
                        msg_send_key(params->led_queue, gpio_num, KEY_EVENT_LONG_PRESS);
                        long_press_sent = true;
                        // ESP_LOGI(TAG, "Long press detected on GPIO %d", gpio_num);
                    }
                }
                break;

            case KEY_STATE_WAIT_SECOND:
                if (current_key_level == 0 && last_key_level == 1) {
                    TickType_t interval = current_tick - release_tick;
                    
                    if (interval <= pdMS_TO_TICKS(DOUBLE_CLICK_INTERVAL_MS)) {
                        press_start_tick = current_tick;
                        state = KEY_STATE_DOUBLE_PRESSED;
                    } else {
                        msg_send_key(params->led_queue, gpio_num, KEY_EVENT_SINGLE_CLICK);
                        // ESP_LOGI(TAG, "Single click detected on GPIO %d", gpio_num);
                        press_start_tick = current_tick;
                        long_press_sent = false;
                        state = KEY_STATE_PRESSED;
                    }
                } else if (current_key_level == 1) {
                    TickType_t wait_duration = current_tick - release_tick;
                    
                    if (wait_duration > pdMS_TO_TICKS(DOUBLE_CLICK_INTERVAL_MS)) {
                        msg_send_key(params->led_queue, gpio_num, KEY_EVENT_SINGLE_CLICK);
                        state = KEY_STATE_IDLE;
                        // ESP_LOGI(TAG, "Single click detected on GPIO %d", gpio_num);
                    }
                }
                break;

            case KEY_STATE_DOUBLE_PRESSED:
                if (current_key_level == 1 && last_key_level == 0) {
                    if (params->pwm_queue != NULL) {
                        msg_send_key(params->pwm_queue, gpio_num, KEY_EVENT_DOUBLE_CLICK);
                    }
                    state = KEY_STATE_IDLE;
                    // ESP_LOGI(TAG, "Double click detected on GPIO %d", gpio_num);
                }
                break;

            default:
                state = KEY_STATE_IDLE;
                break;
        }

        last_key_level = current_key_level;
        vTaskDelay(pdMS_TO_TICKS(KEY_SCAN_INTERVAL_MS));
    }
}

BaseType_t key_task_create(QueueHandle_t led_queue, QueueHandle_t pwm_queue, uint8_t gpio_num)
{
    if (led_queue == NULL) {
        ESP_LOGE(TAG, "Cannot create key task: led_queue is NULL");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    s_key_task_params.led_queue = led_queue;
    s_key_task_params.pwm_queue = pwm_queue;
    s_key_task_params.gpio_num = gpio_num;

    BaseType_t result = xTaskCreate(
        key_task,
        "key_task",
        KEY_TASK_STACK_SIZE,
        (void *)&s_key_task_params,
        KEY_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create key task");
    } else {
        ESP_LOGI(TAG, "Key task created for GPIO %d", gpio_num);
    }

    return result;
}
