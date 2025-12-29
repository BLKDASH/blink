#include "board.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

#define TAG "BOARD"

// LEDC配置常量
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_10_BIT   // 10位分辨率 (0-1023)
#define LEDC_DUTY_MAX       1023                // 2^10 - 1

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

esp_err_t configure_pwm(void)
{
    // 配置LEDC Timer
    ledc_timer_config_t timer_conf = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = PWM_FREQ_HZ,
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
        .gpio_num       = PWM_GPIO,
        .duty           = 0,    // 初始占空比为0%
        .hpoint         = 0
    };
    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "PWM configured on GPIO%d at %dHz", PWM_GPIO, PWM_FREQ_HZ);
    return ESP_OK;
}

esp_err_t pwm_set_duty(uint8_t duty_percent)
{
    // 有效范围 0-100
    if (duty_percent > 100) {
        ESP_LOGW(TAG, "Duty cycle %d%% out of range, clamping to 100%%", duty_percent);
        duty_percent = 100;
    }

    // 将百分比转换为LEDC duty值 (0-8191)
    uint32_t duty = (duty_percent * LEDC_DUTY_MAX) / 100;

    // 设置占空比
    esp_err_t ret = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LEDC duty: %s", esp_err_to_name(ret));
        return ret;
    }

    // 更新占空比
    ret = ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update LEDC duty: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "PWM duty set to %d%%", duty_percent);
    return ESP_OK;
}