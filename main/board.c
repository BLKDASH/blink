#include "board.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "BOARD"

// LEDC配置常量 - 用于MG995舵机控制
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_14_BIT   // 14位分辨率，提高舵机控制精度
#define LEDC_DUTY_MAX       16383               // 2^14 - 1

// 舵机平滑移动参数
#define SERVO_STEP_DELAY_MS 20      // 每步延时(ms)，越大越慢
#define SERVO_STEP_ANGLE    2       // 每步角度增量，越小越平滑

static uint8_t s_current_angle = 0; // 记录当前角度

// 前向声明
static esp_err_t servo_set_angle_direct(uint8_t angle);

void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(LED_RED_GPIO);
    gpio_reset_pin(LED_GRE_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED_RED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GRE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_RED_GPIO, LED_RED_OFF);  // 默认灭灯
    gpio_set_level(LED_GRE_GPIO, LED_GRE_ON);   // 默认亮灯
}

void configure_key(void)
{
    ESP_LOGI(TAG, "Configured GPIO%d for key input", KEY_GPIO);
    gpio_reset_pin(KEY_GPIO);
    gpio_set_direction(KEY_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(KEY_GPIO, GPIO_FLOATING);
}

esp_err_t configure_servo(void)
{
    // 配置LEDC Timer - 50Hz用于舵机控制
    ledc_timer_config_t timer_conf = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = SERVO_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置LEDC Channel
    ledc_channel_config_t channel_conf = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_GPIO,
        .duty           = 0,
        .hpoint         = 0
    };
    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始化到位置1（直接设置，不走平滑）
    servo_set_angle_direct(SERVO_ANGLE_POS1);
    s_current_angle = SERVO_ANGLE_POS1;

    ESP_LOGI(TAG, "MG995 Servo configured on GPIO%d at %dHz", SERVO_GPIO, SERVO_FREQ_HZ);
    return ESP_OK;
}

/**
 * @brief 直接设置舵机角度（无平滑过渡）
 */
static esp_err_t servo_set_angle_direct(uint8_t angle)
{
    if (angle > SERVO_MAX_ANGLE) {
        angle = SERVO_MAX_ANGLE;
    }

    // 计算脉宽 (微秒): 线性映射 angle -> [500us, 2500us]
    uint32_t pulse_width_us = SERVO_MIN_PULSEWIDTH_US + 
        (angle * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US)) / SERVO_MAX_ANGLE;

    // 将脉宽转换为LEDC duty值
    uint32_t duty = (pulse_width_us * LEDC_DUTY_MAX) / 20000;

    esp_err_t ret = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    if (ret != ESP_OK) return ret;

    return ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

esp_err_t servo_set_angle(uint8_t target_angle)
{
    // 角度范围限制 0-180
    if (target_angle > SERVO_MAX_ANGLE) {
        ESP_LOGW(TAG, "Angle %d out of range, clamping to %d", target_angle, SERVO_MAX_ANGLE);
        target_angle = SERVO_MAX_ANGLE;
    }

    ESP_LOGI(TAG, "Servo moving: %d -> %d degrees", s_current_angle, target_angle);

    // 平滑过渡到目标角度
    while (s_current_angle != target_angle) {
        if (s_current_angle < target_angle) {
            // 向上移动
            s_current_angle += SERVO_STEP_ANGLE;
            if (s_current_angle > target_angle) {
                s_current_angle = target_angle;
            }
        } else {
            // 向下移动
            if (s_current_angle < SERVO_STEP_ANGLE) {
                s_current_angle = 0;
            } else {
                s_current_angle -= SERVO_STEP_ANGLE;
            }
            if (s_current_angle < target_angle) {
                s_current_angle = target_angle;
            }
        }

        esp_err_t ret = servo_set_angle_direct(s_current_angle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set servo angle");
            return ret;
        }

        vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_DELAY_MS));
    }

    ESP_LOGI(TAG, "Servo reached %d degrees", s_current_angle);
    return ESP_OK;
}