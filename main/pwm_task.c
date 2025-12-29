/**
 * @file pwm_task.c
 * @brief PWM Task implementation
 */

#include "pwm_task.h"
#include "msg_queue.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "pwm_task";

#define PWM_TASK_STACK_SIZE 2048
#define PWM_TASK_PRIORITY   5

/* 双击计数器配置 - 连续双击触发WiFi凭据清除 */
#define DOUBLE_CLICK_RESET_TIMEOUT_MS  3000
#define DOUBLE_CLICK_TRIGGER_COUNT     2

typedef struct {
    uint8_t count;
    TickType_t last_tick;
} double_click_counter_t;

static bool check_counter_timeout(double_click_counter_t *counter)
{
    if (counter->count == 0) return false;
    TickType_t elapsed = xTaskGetTickCount() - counter->last_tick;
    return (elapsed >= pdMS_TO_TICKS(DOUBLE_CLICK_RESET_TIMEOUT_MS));
}

static void increment_counter(double_click_counter_t *counter)
{
    counter->count++;
    counter->last_tick = xTaskGetTickCount();
}

static void reset_counter(double_click_counter_t *counter)
{
    counter->count = 0;
    counter->last_tick = 0;
}

static void pwm_task(void *pvParameters)
{
    QueueHandle_t pwm_queue = msg_queue_get(QUEUE_PWM);
    msg_t msg;
    bool pwm_high = false;
    double_click_counter_t double_click_counter = {0};

    ESP_LOGI(TAG, "PWM task started");

    while (1) {
        if (msg_queue_receive(pwm_queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_KEY && msg.data.key.event == KEY_EVENT_DOUBLE_CLICK) {
                /* 双击切换PWM高低档 */
                pwm_high = !pwm_high;
                uint8_t duty = pwm_high ? PWM_DUTY_HIGH : PWM_DUTY_LOW;
                pwm_set_duty(duty);
                ESP_LOGI(TAG, "Double click: PWM set to %d%%", duty);
                
                /* 双击计数器 - 连续双击触发WiFi凭据清除 */
                if (check_counter_timeout(&double_click_counter)) {
                    ESP_LOGI(TAG, "Double click counter timeout, resetting");
                    reset_counter(&double_click_counter);
                }
                
                increment_counter(&double_click_counter);
                ESP_LOGI(TAG, "Double click count: %d/%d", 
                         double_click_counter.count, DOUBLE_CLICK_TRIGGER_COUNT);
                
                if (double_click_counter.count >= DOUBLE_CLICK_TRIGGER_COUNT) {
                    ESP_LOGI(TAG, "Trigger reached, clearing WiFi credentials");
                    msg_send_to_wifi(WIFI_CMD_CLEAR_CREDENTIALS);
                    reset_counter(&double_click_counter);
                }
            } else if (msg.type == MSG_TYPE_PWM) {
                /* 直接设置PWM占空比 */
                uint8_t duty = msg.data.pwm.duty_percent;
                pwm_set_duty(duty);
                ESP_LOGI(TAG, "PWM set to %d%%", duty);
            } else {
                ESP_LOGW(TAG, "Received unknown message type: %d", msg.type);
            }
        }
    }
}

BaseType_t pwm_task_create(void)
{
    QueueHandle_t queue = msg_queue_get(QUEUE_PWM);
    if (queue == NULL) {
        ESP_LOGE(TAG, "Cannot create pwm task: queue not initialized");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    BaseType_t result = xTaskCreate(
        pwm_task,
        "pwm_task",
        PWM_TASK_STACK_SIZE,
        NULL,
        PWM_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pwm task");
    } else {
        ESP_LOGI(TAG, "PWM task created successfully");
    }

    return result;
}
