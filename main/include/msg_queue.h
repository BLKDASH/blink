#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 队列ID枚举
 * 
 * 队列按消费者任务划分，每个任务监听自己的队列。
 * 任意消息类型都可以发送到任意队列，由消费者决定如何处理。
 */

typedef enum {
    QUEUE_LED = 0,
    QUEUE_PWM,
    QUEUE_WIFI,
    QUEUE_MQTT,    /* MQTT 队列 */
    QUEUE_MAX
} queue_id_t;

typedef enum {
    MSG_TYPE_NONE = 0,
    MSG_TYPE_LED,
    MSG_TYPE_KEY,
    MSG_TYPE_PWM,
    MSG_TYPE_WIFI,
    MSG_TYPE_MQTT,  /* MQTT 消息类型 */
    MSG_TYPE_MAX
} msg_type_t;

typedef struct {
    uint8_t gpio_num;
    uint8_t state;
} led_msg_data_t;

typedef enum {
    KEY_EVENT_SINGLE_CLICK = 0,
    KEY_EVENT_DOUBLE_CLICK,
    KEY_EVENT_LONG_PRESS
} key_event_t;

typedef enum {
    KEY_STATE_IDLE = 0,
    KEY_STATE_PRESSED,
    KEY_STATE_WAIT_SECOND,
    KEY_STATE_DOUBLE_PRESSED
} key_state_t;

typedef struct {
    uint8_t gpio_num;
    key_event_t event;
} key_msg_data_t;

typedef enum {
    PWM_EVENT_OPEN_DOOR = 0,
    PWM_EVENT_SET_ANGLE,
} pwm_event_t;

typedef struct {
    pwm_event_t event;
    uint8_t angle;          /* 用于 PWM_EVENT_SET_ANGLE */
} pwm_msg_data_t;

typedef enum {
    WIFI_CMD_CLEAR_CREDENTIALS = 0,
} wifi_cmd_t;

typedef struct {
    wifi_cmd_t cmd;
} wifi_msg_data_t;

typedef enum {
    MQTT_CMD_DOOR_ON = 0,
    MQTT_CMD_DOOR_OFF,
} mqtt_cmd_t;

typedef struct {
    mqtt_cmd_t cmd;
} mqtt_msg_data_t;

typedef struct {
    msg_type_t type;
    union {
        led_msg_data_t led;
        key_msg_data_t key;
        pwm_msg_data_t pwm;
        wifi_msg_data_t wifi;
        mqtt_msg_data_t mqtt;
        uint8_t raw[8];
    } data;
} msg_t;

/* 初始化所有队列 */
esp_err_t msg_queue_init_all(uint8_t queue_len);

/* 获取指定队列句柄 */
QueueHandle_t msg_queue_get(queue_id_t id);

/* 队列操作函数 */
bool msg_queue_send(QueueHandle_t queue, const msg_t *msg, uint32_t timeout_ms);
bool msg_queue_receive(QueueHandle_t queue, msg_t *msg, uint32_t timeout_ms);

/* 便捷发送函数 - 发送到指定队列 */
bool msg_send_to_led(uint8_t gpio_num, uint8_t state);
bool msg_send_pwm_open_door(void);
bool msg_send_pwm_set_angle(uint8_t angle);
bool msg_send_to_wifi(wifi_cmd_t cmd);
bool msg_send_mqtt_door_cmd(mqtt_cmd_t cmd);

/* 发送按键事件到指定队列 */
bool msg_send_key_event(queue_id_t queue_id, uint8_t gpio_num, key_event_t event);

bool msg_type_is_valid(msg_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* MSG_QUEUE_H */
