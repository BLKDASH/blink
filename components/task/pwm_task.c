/**
 * @file pwm_task.c
 * @brief MG995舵机控制任务 - 双击切换两个固定角度
 */

#include "pwm_task.h"
#include "msg_queue.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "servo_task";

#define SERVO_TASK_STACK_SIZE 2048
#define SERVO_TASK_PRIORITY   5

/* 双击计数器配置 - 连续双击触发WiFi凭据清除 */
#define DOUBLE_CLICK_RESET_TIMEOUT_MS  2000
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

static void servo_task(void *pvParameters)
{
    QueueHandle_t pwm_queue = msg_queue_get(QUEUE_PWM);
    msg_t msg;
    bool servo_pos_high = false;  // false=位置1, true=位置2
    double_click_counter_t double_click_counter = {0};

    ESP_LOGI(TAG, "Servo task started (Pos1: %d°, Pos2: %d°)", 
             SERVO_ANGLE_POS1, SERVO_ANGLE_POS2);

    while (1) {
        if (msg_queue_receive(pwm_queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_KEY && msg.data.key.event == KEY_EVENT_DOUBLE_CLICK) {
                // 开门（移动到位置2）
                servo_pos_high = !servo_pos_high;
                uint8_t angle = servo_pos_high ? SERVO_ANGLE_POS2 : SERVO_ANGLE_POS1;
                servo_set_angle(angle);
                ESP_LOGI(TAG, "Double click: Servo set to %d degrees", angle);
                vTaskDelay(pdMS_TO_TICKS(OPEN_TIME));
                // 2秒后关门（移动到位置1）
                servo_pos_high = !servo_pos_high;
                angle = servo_pos_high ? SERVO_ANGLE_POS2 : SERVO_ANGLE_POS1;
                servo_set_angle(angle);
                ESP_LOGI(TAG, "Double click: Servo set to %d degrees", angle);
                
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
                /* 直接设置舵机角度 (复用pwm消息，duty_percent作为角度) */
                uint8_t angle = msg.data.pwm.duty_percent;
                if (angle > 180) angle = 180;
                servo_set_angle(angle);
                ESP_LOGI(TAG, "Servo set to %d degrees", angle);
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
        ESP_LOGE(TAG, "Cannot create servo task: queue not initialized");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    BaseType_t result = xTaskCreate(
        servo_task,
        "servo_task",
        SERVO_TASK_STACK_SIZE,
        NULL,
        SERVO_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create servo task");
    } else {
        ESP_LOGI(TAG, "Servo task created successfully");
    }

    return result;
}
