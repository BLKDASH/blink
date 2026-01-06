/**
 * @file pwm_task.c
 * @brief MG995舵机控制任务 - 双击切换两个固定角度
 */

#include "pwm_task.h"
#include "msg_queue.h"
#include "board.h"
#include "ha_mqtt.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/timers.h"

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

/* 自动关门定时器 */
static TimerHandle_t s_close_door_timer = NULL;
static bool s_door_open = false;

static void close_door_timer_callback(TimerHandle_t xTimer)
{
    if (s_door_open) {
        servo_set_angle(SERVO_ANGLE_POS1);
        s_door_open = false;
        ESP_LOGI(TAG, "Auto close door: Servo set to %d degrees", SERVO_ANGLE_POS1);
        
        /* 发布门状态到 MQTT */
        ha_mqtt_publish_door_state(false);
    }
}

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

/* 非阻塞开门，定时器自动关门 */
static void open_door_non_blocking(void)
{
    servo_set_angle(SERVO_ANGLE_POS2);
    s_door_open = true;
    ESP_LOGI(TAG, "Open door: Servo set to %d degrees", SERVO_ANGLE_POS2);
    
    /* 发布门状态到 MQTT */
    ha_mqtt_publish_door_state(true);
    
    /* 重置并启动关门定时器 */
    if (s_close_door_timer != NULL) {
        xTimerStop(s_close_door_timer, 0);
        xTimerChangePeriod(s_close_door_timer, pdMS_TO_TICKS(OPEN_TIME), 0);
        xTimerStart(s_close_door_timer, 0);
    }
}

/* 关门操作 */
static void close_door(void)
{
    /* 停止自动关门定时器 */
    if (s_close_door_timer != NULL) {
        xTimerStop(s_close_door_timer, 0);
    }
    
    if (s_door_open) {
        servo_set_angle(SERVO_ANGLE_POS1);
        s_door_open = false;
        ESP_LOGI(TAG, "Close door: Servo set to %d degrees", SERVO_ANGLE_POS1);
        
        /* 发布门状态到 MQTT */
        ha_mqtt_publish_door_state(false);
    }
}

static void servo_task(void *pvParameters)
{
    QueueHandle_t pwm_queue = msg_queue_get(QUEUE_PWM);
    msg_t msg;
    double_click_counter_t double_click_counter = {0};

    /* 创建自动关门定时器 */
    s_close_door_timer = xTimerCreate("close_door", pdMS_TO_TICKS(OPEN_TIME), 
                                       pdFALSE, NULL, close_door_timer_callback);

    ESP_LOGI(TAG, "Servo task started (Pos1: %d°, Pos2: %d°)", 
             SERVO_ANGLE_POS1, SERVO_ANGLE_POS2);

    while (1) {
        if (msg_queue_receive(pwm_queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_KEY && msg.data.key.event == KEY_EVENT_DOUBLE_CLICK) {
                /* 双击计数器 - 先处理计数 */
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
                

            }
            else if (msg.type == MSG_TYPE_KEY && msg.data.key.event == KEY_EVENT_SINGLE_CLICK)
            {
                /* 非阻塞开门 */
                open_door_non_blocking();
            } else if (msg.type == MSG_TYPE_PWM) {
                if (msg.data.pwm.event == PWM_EVENT_OPEN_DOOR) {
                    /* 蓝牙开门命令 - 非阻塞 */
                    open_door_non_blocking();
                } else if (msg.data.pwm.event == PWM_EVENT_SET_ANGLE) {
                    /* 直接设置舵机角度 */
                    uint8_t angle = msg.data.pwm.angle;
                    if (angle > 180) angle = 180;
                    servo_set_angle(angle);
                    ESP_LOGI(TAG, "Servo set to %d degrees", angle);
                }
            } else if (msg.type == MSG_TYPE_MQTT) {
                /* MQTT 开门/关门命令 */
                if (msg.data.mqtt.cmd == MQTT_CMD_DOOR_ON) {
                    ESP_LOGI(TAG, "MQTT door ON command received");
                    open_door_non_blocking();
                } else if (msg.data.mqtt.cmd == MQTT_CMD_DOOR_OFF) {
                    ESP_LOGI(TAG, "MQTT door OFF command received");
                    close_door();
                }
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
