#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MSG_TYPE_NONE = 0,
    MSG_TYPE_LED,
    MSG_TYPE_KEY,
    MSG_TYPE_PWM,
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

typedef struct {
    msg_type_t type;
    union {
        led_msg_data_t led;
        key_msg_data_t key;
        pwm_msg_data_t pwm;
        uint8_t raw[8];
    } data;
} msg_t;

QueueHandle_t msg_queue_init(uint8_t queue_len);
bool msg_queue_send(QueueHandle_t queue, const msg_t *msg, uint32_t timeout_ms);
bool msg_queue_receive(QueueHandle_t queue, msg_t *msg, uint32_t timeout_ms);
bool msg_send_led(QueueHandle_t queue, uint8_t gpio_num, uint8_t state);
bool msg_send_key(QueueHandle_t queue, uint8_t gpio_num, key_event_t event);
bool msg_send_pwm(QueueHandle_t queue, uint8_t gpio_num, uint8_t duty_percent);
bool msg_type_is_valid(msg_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* MSG_QUEUE_H */
