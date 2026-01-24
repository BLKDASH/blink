#ifndef __BOARD_H__
#define __BOARD_H__

#include "esp_err.h"
#include <stdbool.h>

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
#define SERVO_ANGLE_POS2    80      // 位置2: 80度

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

/**
 * @brief 初始化门控制器
 * 
 * 初始化舵机和互斥锁
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t door_controller_init(void);

/**
 * @brief 执行开门操作（阻塞）
 * 
 * 执行完整的开门周期：
 * 1. 设置舵机到开门角度
 * 2. 阻塞等待配置的开门时间
 * 3. 设置舵机到关门角度
 * 
 * 此函数是线程安全的，使用互斥锁保护。
 * 如果另一个任务正在执行开门，此函数会等待。
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t door_open(void);

/**
 * @brief 执行关门操作
 * 
 * 立即设置舵机到关门角度
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t door_close(void);

/**
 * @brief 获取当前门状态
 * 
 * @return true 门开启，false 门关闭
 */
bool door_is_open(void);


#endif