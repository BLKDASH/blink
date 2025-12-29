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

/* 发送按键事件到指定队列 */
static bool send_key_event(queue_id_t queue_id, key_event_t event)
{
    QueueHandle_t queue = msg_queue_get(queue_id);
    if (queue == NULL) {
        return false;
    }
    msg_t msg = {
        .type = MSG_TYPE_KEY,
        .data.key = {
            .gpio_num = s_gpio_num,
            .event = event
        }
    };
    return msg_queue_send(queue, &msg, 100);
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

    /* 按键扫描主循环 */
    while (1) {
        /* 读取当前按键电平状态 */
        current_key_level = gpio_get_level(gpio_num);
        TickType_t current_tick = xTaskGetTickCount();

        switch (state) {
            /* 空闲状态：等待按键按下 */
            case KEY_STATE_IDLE:
                /* 检测到下降沿（按键按下） */
                if (current_key_level == 0 && last_key_level == 1) {
                    press_start_tick = current_tick;  /* 记录按下时刻 */
                    long_press_sent = false;          /* 重置长按标志 */
                    state = KEY_STATE_PRESSED;        /* 进入按下状态 */
                }
                break;

            /* 按下状态：判断是短按还是长按 */
            case KEY_STATE_PRESSED:
                /* 检测到上升沿（按键释放） */
                if (current_key_level == 1 && last_key_level == 0) {
                    TickType_t press_duration = current_tick - press_start_tick;
                    
                    /* 长按后释放，直接回到空闲状态（长按事件已在按住时发送） */
                    if (press_duration >= pdMS_TO_TICKS(LONG_PRESS_TIME_MS)) {
                        state = KEY_STATE_IDLE;
                    } else {
                        /* 短按释放，进入等待第二次按下状态（判断是否双击） */
                        release_tick = current_tick;
                        state = KEY_STATE_WAIT_SECOND;
                    }
                } else if (current_key_level == 0) {
                    /* 按键持续按住，检测是否达到长按时间 */
                    TickType_t press_duration = current_tick - press_start_tick;
                    if (press_duration >= pdMS_TO_TICKS(LONG_PRESS_TIME_MS) && !long_press_sent) {
                        /* 超时则触发长按事件 */
                        send_key_event(QUEUE_LED, KEY_EVENT_LONG_PRESS);
                        long_press_sent = true;  /* 标记长按事件已发送，避免重复触发 */
                    }
                }
                break;

            /* 等待第二次按下状态：判断是单击还是双击 */
            case KEY_STATE_WAIT_SECOND:
                /* 检测到下降沿（第二次按下） */
                if (current_key_level == 0 && last_key_level == 1) {
                    TickType_t interval = current_tick - release_tick;
                    
                    /* 在双击间隔时间内按下，判定为双击的第二次按下 */
                    if (interval <= pdMS_TO_TICKS(DOUBLE_CLICK_INTERVAL_MS)) {
                        press_start_tick = current_tick;
                        state = KEY_STATE_DOUBLE_PRESSED;
                    } else {
                        /* 超过双击间隔，先发送单击事件，然后作为新的按下处理 */
                        send_key_event(QUEUE_LED, KEY_EVENT_SINGLE_CLICK);
                        press_start_tick = current_tick;
                        long_press_sent = false;
                        state = KEY_STATE_PRESSED;
                    }
                } else if (current_key_level == 1) {
                    /* 按键保持释放状态，检测等待是否超时 */
                    TickType_t wait_duration = current_tick - release_tick;
                    
                    /* 超过双击间隔仍未按下，判定为单击 */
                    if (wait_duration > pdMS_TO_TICKS(DOUBLE_CLICK_INTERVAL_MS)) {
                        send_key_event(QUEUE_LED, KEY_EVENT_SINGLE_CLICK);
                        state = KEY_STATE_IDLE;
                    }
                }
                break;

            /* 双击第二次按下状态：等待释放以确认双击 */
            case KEY_STATE_DOUBLE_PRESSED:
                /* 检测到上升沿（第二次按下后释放），确认双击完成 */
                if (current_key_level == 1 && last_key_level == 0) {
                    /* 双击事件发送到PWM队列 */
                    send_key_event(QUEUE_PWM, KEY_EVENT_DOUBLE_CLICK);
                    ESP_LOGI(TAG, "Double click detected");
                    state = KEY_STATE_IDLE;
                }
                break;

            default:
                state = KEY_STATE_IDLE;
                break;
        }

        /* 更新上一次按键电平，用于边沿检测 */
        last_key_level = current_key_level;
        /* 按键扫描间隔延时 */
        vTaskDelay(pdMS_TO_TICKS(KEY_SCAN_INTERVAL_MS));
    }
}

BaseType_t key_task_create(uint8_t gpio_num)
{
    s_gpio_num = gpio_num;

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
