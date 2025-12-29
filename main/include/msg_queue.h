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

/* 队列ID枚举 */
typedef enum {
    QUEUE_LED = 0,
    QUEUE_PWM,
    QUEUE_WIFI,
    QUEUE_MAX
} queue_id_t;

typedef enum {
    MSG_TYPE_NONE = 0,
    MSG_TYPE_LED,
    MSG_TYPE_KEY,
    MSG_TYPE_PWM,
    MSG_TYPE_WIFI,
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

typedef struct {
    uint8_t gpio_num;
    uint8_t duty_percent;
} pwm_msg_data_t;

typedef enum {
    WIFI_CMD_CLEAR_CREDENTIALS = 0,
} wifi_cmd_t;

typedef struct {
    wifi_cmd_t cmd;
} wifi_msg_data_t;

typedef struct {
    msg_type_t type;
    union {
        led_msg_data_t led;
        key_msg_data_t key;
        pwm_msg_data_t pwm;
        wifi_msg_data_t wifi;
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

/* 便捷发送函数 - 自动获取对应队列 */
bool msg_send_led(uint8_t gpio_num, uint8_t state);
bool msg_send_key(uint8_t gpio_num, key_event_t event);
bool msg_send_pwm(uint8_t gpio_num, uint8_t duty_percent);
bool msg_send_wifi(wifi_cmd_t cmd);

bool msg_type_is_valid(msg_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* MSG_QUEUE_H */
