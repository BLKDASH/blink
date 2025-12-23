/**
 * @file msg_queue.h
 * @brief Message Queue System for ESP32-C6
 * 
 * This module provides a unified message queue architecture based on FreeRTOS
 * for inter-task communication including LED control and key events.
 */

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Message type enumeration
 */
typedef enum {
    MSG_TYPE_NONE = 0,      /**< Invalid message */
    MSG_TYPE_LED,           /**< LED control message */
    MSG_TYPE_KEY,           /**< Key event message */
    MSG_TYPE_PWM,           /**< PWM control message */
    MSG_TYPE_MAX            /**< Message type upper limit */
} msg_type_t;

/**
 * @brief LED message data structure
 */
typedef struct {
    uint8_t gpio_num;       /**< LED GPIO number */
    uint8_t state;          /**< LED state: 0=off, 1=on */
} led_msg_data_t;

/**
 * @brief Key event type enumeration
 */
typedef enum {
    KEY_EVENT_SINGLE_CLICK = 0,  /**< Single click event */
    KEY_EVENT_DOUBLE_CLICK,      /**< Double click event */
    KEY_EVENT_LONG_PRESS         /**< Long press event */
} key_event_t;

/**
 * @brief Key gesture detection state machine enumeration
 */
typedef enum {
    KEY_STATE_IDLE = 0,          /**< Idle state - waiting for key press */
    KEY_STATE_PRESSED,           /**< Key pressed - waiting for release or long press timeout */
    KEY_STATE_WAIT_SECOND,       /**< First click released - waiting for second click or timeout */
    KEY_STATE_DOUBLE_PRESSED     /**< Second key pressed - waiting for release to confirm double click */
} key_state_t;

/**
 * @brief Key message data structure
 */
typedef struct {
    uint8_t gpio_num;       /**< Key GPIO number */
    key_event_t event;      /**< Event type: single_click/double_click/long_press */
} key_msg_data_t;

/**
 * @brief PWM message data structure
 */
typedef struct {
    uint8_t gpio_num;       /**< PWM GPIO number */
    uint8_t duty_percent;   /**< Duty cycle 0-100% */
} pwm_msg_data_t;

/**
 * @brief Unified message structure
 */
typedef struct {
    msg_type_t type;        /**< Message type */
    union {
        led_msg_data_t led; /**< LED message data */
        key_msg_data_t key; /**< Key message data */
        pwm_msg_data_t pwm; /**< PWM message data */
        uint8_t raw[8];     /**< Raw data (reserved for extension) */
    } data;
} msg_t;

/**
 * @brief Initialize the message queue
 * 
 * Creates a FreeRTOS queue for message passing between tasks.
 * 
 * @param queue_len Number of messages the queue can hold
 * @return QueueHandle_t Queue handle on success, NULL on failure
 */
QueueHandle_t msg_queue_init(uint8_t queue_len);

/**
 * @brief Send a message to the queue
 * 
 * Sends a message to the specified queue with configurable timeout.
 * 
 * @param queue Queue handle
 * @param msg Pointer to the message to send
 * @param timeout_ms Timeout in milliseconds (0 for no wait, portMAX_DELAY for infinite)
 * @return true if message was sent successfully, false on timeout or error
 */
bool msg_queue_send(QueueHandle_t queue, const msg_t *msg, uint32_t timeout_ms);

/**
 * @brief Receive a message from the queue
 * 
 * Receives a message from the specified queue with configurable timeout.
 * 
 * @param queue Queue handle
 * @param msg Pointer to message structure to populate
 * @param timeout_ms Timeout in milliseconds (0 for no wait, portMAX_DELAY for infinite)
 * @return true if message was received successfully, false on timeout or error
 */
bool msg_queue_receive(QueueHandle_t queue, msg_t *msg, uint32_t timeout_ms);

/**
 * @brief Send an LED control message to the queue
 * 
 * Convenience function that creates and sends an LED control message.
 * 
 * @param queue Queue handle
 * @param gpio_num LED GPIO number
 * @param state LED state: 0=off, 1=on
 * @return true if message was sent successfully, false on error
 */
bool msg_send_led(QueueHandle_t queue, uint8_t gpio_num, uint8_t state);

/**
 * @brief Send a key event message to the queue
 * 
 * Convenience function that creates and sends a key event message.
 * 
 * @param queue Queue handle
 * @param gpio_num Key GPIO number
 * @param event Event type: KEY_EVENT_SINGLE_CLICK/KEY_EVENT_DOUBLE_CLICK/KEY_EVENT_LONG_PRESS
 * @return true if message was sent successfully, false on error
 */
bool msg_send_key(QueueHandle_t queue, uint8_t gpio_num, key_event_t event);

/**
 * @brief Send a PWM control message to the queue
 * 
 * Convenience function that creates and sends a PWM control message.
 * 
 * @param queue Queue handle
 * @param gpio_num PWM GPIO number
 * @param duty_percent Duty cycle (0-100%)
 * @return true if message was sent successfully, false on error
 */
bool msg_send_pwm(QueueHandle_t queue, uint8_t gpio_num, uint8_t duty_percent);

/**
 * @brief Validate if a message type is valid
 * 
 * Checks if the given message type is within the valid range
 * (between MSG_TYPE_NONE exclusive and MSG_TYPE_MAX exclusive).
 * 
 * @param type Message type to validate
 * @return true if the message type is valid, false otherwise
 */
bool msg_type_is_valid(msg_type_t type);

/**
 * @brief Create the LED handling task
 * 
 * Creates a FreeRTOS task that receives LED messages from the queue
 * and controls the corresponding GPIO levels.
 * 
 * @param queue Queue handle for receiving LED messages
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure
 */
BaseType_t led_task_create(QueueHandle_t queue);

/**
 * @brief Create the key scanning task
 * 
 * Creates a FreeRTOS task that scans the specified GPIO for key press
 * and release events, sending key messages to the queue.
 * 
 * @param queue Queue handle for sending key messages
 * @param gpio_num GPIO number of the key to scan
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure
 */
BaseType_t key_task_create(QueueHandle_t queue, uint8_t gpio_num);

/**
 * @brief Create the PWM handling task
 * 
 * Creates a FreeRTOS task that receives PWM messages from the queue
 * and controls the PWM duty cycle.
 * 
 * @param queue Queue handle for receiving PWM messages
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure
 */
BaseType_t pwm_task_create(QueueHandle_t queue);

/**
 * @brief Set the PWM queue handle for key task
 * 
 * Allows key task to send PWM control messages to a separate PWM queue.
 * 
 * @param queue PWM queue handle
 */
void key_task_set_pwm_queue(QueueHandle_t queue);

#ifdef __cplusplus
}
#endif

#endif /* MSG_QUEUE_H */
