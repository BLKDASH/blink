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

static uint8_t s_gpio_num;
static QueueHandle_t s_led_queue = NULL;

/* 发送按键事件到LED队列 */
static bool send_key_event(uint8_t gpio_num, key_event_t event)
{
    if (s_led_queue == NULL) {
        return false;
    }
    msg_t msg = {
        .type = MSG_TYPE_KEY,
        .data.key = {
            .gpio_num = gpio_num,
            .event = event
        }
    };
    return msg_queue_send(s_led_queue, &msg, 100);
}

/**
 * @brief Key task function with gesture detection
 */
static void key_task(void *pvParameters)
{
    uint8_t gpio_num = s_gpio_num;
    
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
                        send_key_event(gpio_num, KEY_EVENT_LONG_PRESS);
                        long_press_sent = true;
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
                        send_key_event(gpio_num, KEY_EVENT_SINGLE_CLICK);
                        press_start_tick = current_tick;
                        long_press_sent = false;
                        state = KEY_STATE_PRESSED;
                    }
                } else if (current_key_level == 1) {
                    TickType_t wait_duration = current_tick - release_tick;
                    
                    if (wait_duration > pdMS_TO_TICKS(DOUBLE_CLICK_INTERVAL_MS)) {
                        send_key_event(gpio_num, KEY_EVENT_SINGLE_CLICK);
                        state = KEY_STATE_IDLE;
                    }
                }
                break;

            case KEY_STATE_DOUBLE_PRESSED:
                if (current_key_level == 1 && last_key_level == 0) {
                    /* 双击事件发送到PWM队列 */
                    msg_send_to_pwm(KEY_EVENT_DOUBLE_CLICK);
                    ESP_LOGI(TAG, "Double click detected");
                    state = KEY_STATE_IDLE;
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

BaseType_t key_task_create(uint8_t gpio_num)
{
    s_gpio_num = gpio_num;
    s_led_queue = msg_queue_get(QUEUE_LED);
    
    if (s_led_queue == NULL) {
        ESP_LOGE(TAG, "Cannot create key task: LED queue not initialized");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    BaseType_t result = xTaskCreate(
        key_task,
        "key_task",
        KEY_TASK_STACK_SIZE,
        NULL,
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
