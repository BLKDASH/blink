#ifndef __BOARD_H__
#define __BOARD_H__

#include "esp_err.h"

// 新板子LED
#define LED_RED_GPIO 11
#define LED_GRE_GPIO 12

// 按键
#define KEY_GPIO 15

// PWM配置
#define PWM_GPIO 13
#define PWM_FREQ_HZ 10000       // 10kHz
#define PWM_DUTY_LOW 20         // 低档占空比 20%
#define PWM_DUTY_HIGH 80        // 高档占空比 80%

void configure_led(void);
void configure_key(void);

/**
 * @brief 初始化PWM输出
 * @return ESP_OK成功, 其他失败
 */
esp_err_t configure_pwm(void);

/**
 * @brief 设置PWM占空比
 * @param duty_percent 占空比 (0-100), 超出范围会被钳位
 * @return ESP_OK成功, 其他失败
 */
esp_err_t pwm_set_duty(uint8_t duty_percent);


#endif