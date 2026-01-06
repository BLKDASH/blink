#ifndef __BOARD_H__
#define __BOARD_H__

#include "esp_err.h"

// LED GPIO定义
#define LED_RED_GPIO 11 // 低电平亮
#define LED_GRE_GPIO 12 // 高电平亮

// LED状态定义（低电平有效）
#define LED_RED_ON  0
#define LED_RED_OFF 1

#define LED_GRE_ON  1
#define LED_GRE_OFF 0

// 按键
#define KEY_GPIO 2

// MG995舵机配置 (GPIO13)
#define SERVO_GPIO          13
#define SERVO_FREQ_HZ       50      // 舵机标准频率50Hz (周期20ms)

// MG995舵机角度配置 - 双击切换的两个固定角度
#define SERVO_ANGLE_POS1    135       // 位置1: 135度
#define SERVO_ANGLE_POS2    80      // 位置2: 45度

#define OPEN_TIME 2000 //开门持续时间:2s

// MG995舵机脉宽范围 (微秒)
#define SERVO_MIN_PULSEWIDTH_US     500     // 0度对应脉宽 0.5ms
#define SERVO_MAX_PULSEWIDTH_US     2500    // 180度对应脉宽 2.5ms
#define SERVO_MAX_ANGLE             180     // 最大角度

void configure_led(void);
void configure_key(void);

/**
 * @brief 初始化MG995舵机PWM输出
 * @return ESP_OK成功, 其他失败
 */
esp_err_t configure_servo(void);

/**
 * @brief 设置舵机角度
 * @param angle 角度 (0-180), 超出范围会被钳位
 * @return ESP_OK成功, 其他失败
 */
esp_err_t servo_set_angle(uint8_t angle);


#endif