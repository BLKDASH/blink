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

/* 双击计数器配置 (Requirements: 6.1, 6.4) */
#define DOUBLE_CLICK_RESET_TIMEOUT_MS  3000
#define DOUBLE_CLICK_TRIGGER_COUNT     2

/* 双击计数器结构体 */
typedef struct {
    uint8_t count;
    TickType_t last_tick;
} double_click_counter_t;

/* 任务参数结构体 */
typedef struct {
    QueueHandle_t pwm_queue;
    QueueHandle_t wifi_queue;
} pwm_task_params_t;

static pwm_task_params_t s_task_params;

/**
 * @brief 检查双击计数器是否超时
 */
static bool check_counter_timeout(double_click_counter_t *counter)
{
    if (counter->count == 0) {
        return false;
    }
    
    TickType_t current_tick = xTaskGetTickCount();
    TickType_t elapsed_ticks = current_tick - counter->last_tick;
    TickType_t timeout_ticks = pdMS_TO_TICKS(DOUBLE_CLICK_RESET_TIMEOUT_MS);
    
    return (elapsed_ticks >= timeout_ticks);
}

/**
 * @brief 递增双击计数器
 */
static void increment_counter(double_click_counter_t *counter)
{
    counter->count++;
    counter->last_tick = xTaskGetTickCount();
}

/**
 * @brief 检查是否应该触发清除凭据
 */
static bool should_trigger_clear(double_click_counter_t *counter)
{
    return (counter->count >= DOUBLE_CLICK_TRIGGER_COUNT);
}

/**
 * @brief 重置双击计数器
 */
static void reset_counter(double_click_counter_t *counter)
{
    counter->count = 0;
    counter->last_tick = 0;
}

static void pwm_task(void *pvParameters)
{
    pwm_task_params_t *params = (pwm_task_params_t *)pvParameters;
    QueueHandle_t pwm_queue = params->pwm_queue;
    QueueHandle_t wifi_queue = params->wifi_queue;
    msg_t msg;
    static uint8_t pwm_high = 0;
    
    /* 双击计数器初始化 */
    double_click_counter_t double_click_counter = {0};

    ESP_LOGI(TAG, "PWM task started");

    while (1) {
        if (msg_queue_receive(pwm_queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_PWM) {
                pwm_set_duty(msg.data.pwm.duty_percent);
                ESP_LOGD(TAG, "PWM set to %d%%", msg.data.pwm.duty_percent);
            } else if (msg.type == MSG_TYPE_KEY) {
                if (msg.data.key.event == KEY_EVENT_DOUBLE_CLICK) {
                    /* 处理PWM切换 */
                    pwm_high = !pwm_high;
                    uint8_t duty = pwm_high ? PWM_DUTY_HIGH : PWM_DUTY_LOW;
                    pwm_set_duty(duty);
                    ESP_LOGI(TAG, "Double click: PWM toggled to %d%%", duty);
                    
                    /* 处理双击计数器 (Requirements: 6.1, 6.4) */
                    if (wifi_queue != NULL) {
                        /* 检查超时并重置计数器 */
                        if (check_counter_timeout(&double_click_counter)) {
                            ESP_LOGI(TAG, "Double click counter timeout, resetting");
                            reset_counter(&double_click_counter);
                        }
                        
                        /* 递增计数器 */
                        increment_counter(&double_click_counter);
                        ESP_LOGI(TAG, "Double click count: %d/%d", 
                                 double_click_counter.count, DOUBLE_CLICK_TRIGGER_COUNT);
                        
                        /* 检查是否达到触发阈值 */
                        if (should_trigger_clear(&double_click_counter)) {
                            ESP_LOGI(TAG, "Double click trigger reached, sending clear credentials command");
                            msg_send_wifi(wifi_queue, WIFI_CMD_CLEAR_CREDENTIALS);
                            reset_counter(&double_click_counter);
                        }
                    }
                }
            } else {
                ESP_LOGW(TAG, "Received unknown message type: %d", msg.type);
            }
        }
    }
}

BaseType_t pwm_task_create(QueueHandle_t pwm_queue, QueueHandle_t wifi_queue)
{
    if (pwm_queue == NULL) {
        ESP_LOGE(TAG, "Cannot create pwm task: pwm_queue is NULL");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    /* 保存参数到静态变量 */
    s_task_params.pwm_queue = pwm_queue;
    s_task_params.wifi_queue = wifi_queue;

    BaseType_t result = xTaskCreate(
        pwm_task,
        "pwm_task",
        PWM_TASK_STACK_SIZE,
        (void *)&s_task_params,
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
