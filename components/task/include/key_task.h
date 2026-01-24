/**
 * @file key_task.h
 * @brief Key Task for ESP32-C6
 */

#ifndef KEY_TASK_H
#define KEY_TASK_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 按键事件类型
 */
typedef enum {
    KEY_EVENT_SINGLE_CLICK,  /**< 单击事件 */
    KEY_EVENT_DOUBLE_CLICK,  /**< 双击事件 */
    KEY_EVENT_LONG_PRESS     /**< 长按事件 */
} key_event_t;

/**
 * @brief 按键状态机状态
 */
typedef enum {
    KEY_STATE_IDLE,           /**< 空闲状态 */
    KEY_STATE_PRESSED,        /**< 按下状态 */
    KEY_STATE_WAIT_SECOND,    /**< 等待第二次按下（判断双击） */
    KEY_STATE_DOUBLE_PRESSED  /**< 双击第二次按下 */
} key_state_t;

/**
 * @brief 按键事件回调函数类型
 * 
 * @param gpio_num 触发事件的GPIO编号
 * @param event 按键事件类型
 */
typedef void (*key_event_callback_t)(uint8_t gpio_num, key_event_t event);

/**
 * @brief 按键任务配置结构体
 */
typedef struct {
    uint8_t gpio_num;              /**< 按键GPIO编号 */
    key_event_callback_t callback; /**< 事件回调函数 */
} key_task_config_t;

/**
 * @brief Create the key scanning task
 * 
 * Creates a FreeRTOS task that scans the specified GPIO for key press
 * and release events, invoking the callback on detected gestures.
 * 
 * @param config 按键任务配置，包含GPIO和回调函数
 * @return pdPASS on success, errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY on failure
 */
BaseType_t key_task_create(const key_task_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* KEY_TASK_H */
