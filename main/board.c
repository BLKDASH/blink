#include "board.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

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
#define SERVO_HOLD_TIME_MS  500     // 舵机到位后保持PWM信号的时间(ms)

static uint8_t s_current_angle = 0; // 记录当前角度

// 前向声明
static esp_err_t servo_set_angle_direct(uint8_t angle);
static esp_err_t servo_stop_pwm(void);
static esp_err_t servo_start_pwm(void);

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
    
    // 保持PWM信号让舵机到位
    vTaskDelay(pdMS_TO_TICKS(SERVO_HOLD_TIME_MS));
    
    // 停止PWM信号，避免舵机长期受力
    servo_stop_pwm();

    ESP_LOGI(TAG, "MG995 Servo configured on GPIO%d at %dHz (PWM stopped after init)", SERVO_GPIO, SERVO_FREQ_HZ);
    return ESP_OK;
}

/**
 * @brief 直接设置舵机角度（无平滑过渡）
 */
static esp_err_t servo_set_angle_direct(uint8_t angle)
{
    // 角度范围限制 (uint8_t自动保证 >= 0)
    if (angle > SERVO_MAX_ANGLE) {
        angle = SERVO_MAX_ANGLE;
    }

    // 计算脉宽 (微秒): 线性映射 angle -> [500us, 2500us]
    uint32_t pulse_width_us = SERVO_MIN_PULSEWIDTH_US + 
        (angle * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US)) / SERVO_MAX_ANGLE;

    // 将脉宽转换为LEDC duty值
    // 周期 = 20ms = 20000us, 使用64位避免溢出
    uint32_t duty = ((uint64_t)pulse_width_us * LEDC_DUTY_MAX) / 20000;

    esp_err_t ret = ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set LEDC duty: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update LEDC duty: %s", esp_err_to_name(ret));
    }

    return ret;
}

/**
 * @brief 停止PWM输出，保护舵机
 * 
 * 舵机到位后应停止PWM信号，避免长期受力导致舵机发热损坏
 */
static esp_err_t servo_stop_pwm(void)
{
    esp_err_t ret = ledc_stop(LEDC_MODE, LEDC_CHANNEL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop PWM: %s", esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief 启动PWM输出
 * 
 * 在需要移动舵机前重新启动PWM
 */
static esp_err_t servo_start_pwm(void)
{
    // 重新配置LEDC Timer以恢复PWM输出
    ledc_timer_config_t timer_conf = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = SERVO_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to restart PWM timer: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t servo_set_angle(uint8_t target_angle)
{
    // 角度范围限制 0-180
    if (target_angle > SERVO_MAX_ANGLE) {
        ESP_LOGW(TAG, "Angle %d out of range, clamping to %d", target_angle, SERVO_MAX_ANGLE);
        target_angle = SERVO_MAX_ANGLE;
    }

    ESP_LOGI(TAG, "Servo moving: %d -> %d degrees", s_current_angle, target_angle);

    // 启动PWM输出
    esp_err_t ret = servo_start_pwm();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start PWM");
        return ret;
    }

    // 平滑过渡到目标角度
    while (s_current_angle != target_angle) {
        // 计算下一步角度
        uint8_t next_angle;
        if (s_current_angle < target_angle) {
            // 向上移动
            next_angle = s_current_angle + SERVO_STEP_ANGLE;
            if (next_angle > target_angle) {
                next_angle = target_angle;
            }
        } else {
            // 向下移动
            if (s_current_angle <= SERVO_STEP_ANGLE) {
                next_angle = 0;
            } else {
                next_angle = s_current_angle - SERVO_STEP_ANGLE;
            }
            if (next_angle < target_angle) {
                next_angle = target_angle;
            }
        }

        // 尝试设置角度
        ret = servo_set_angle_direct(next_angle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set servo angle at %d degrees", next_angle);
            return ret;
        }

        // 更新当前角度（只有在硬件设置成功后才更新）
        s_current_angle = next_angle;

        vTaskDelay(pdMS_TO_TICKS(SERVO_STEP_DELAY_MS));
    }

    ESP_LOGI(TAG, "Servo reached %d degrees", s_current_angle);
    
    // 保持PWM信号让舵机稳定到位
    vTaskDelay(pdMS_TO_TICKS(SERVO_HOLD_TIME_MS));
    
    // 停止PWM信号，避免舵机长期受力损坏
    servo_stop_pwm();
    ESP_LOGI(TAG, "Servo PWM stopped to protect servo");
    
    return ESP_OK;
}

// ============================================================================
// Door Controller Implementation
// ============================================================================

/**
 * @brief 门控制器状态结构
 */
typedef struct {
    SemaphoreHandle_t mutex;      /* 互斥锁 */
    bool is_open;                 /* 当前门状态 */
    uint8_t open_angle;           /* 开门角度 */
    uint8_t close_angle;          /* 关门角度 */
    uint32_t open_duration_ms;    /* 开门持续时间 */
} door_controller_t;

static door_controller_t s_door_ctrl = {0};

esp_err_t door_controller_init(void)
{
    ESP_LOGI(TAG, "Initializing door controller");

    // 创建互斥锁
    s_door_ctrl.mutex = xSemaphoreCreateMutex();
    if (s_door_ctrl.mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex for door controller");
        return ESP_ERR_NO_MEM;
    }

    // 使用 board.h 中定义的固定参数
    s_door_ctrl.open_angle = SERVO_ANGLE_POS2;      // 开门角度：80度
    s_door_ctrl.close_angle = SERVO_ANGLE_POS1;     // 关门角度：135度
    s_door_ctrl.open_duration_ms = OPEN_TIME;       // 开门持续时间：2000ms
    s_door_ctrl.is_open = false;

    ESP_LOGI(TAG, "Door controller initialized: open_angle=%d, close_angle=%d, duration=%lu ms",
             s_door_ctrl.open_angle, s_door_ctrl.close_angle, s_door_ctrl.open_duration_ms);

    return ESP_OK;
}

esp_err_t door_open(void)
{
    ESP_LOGI(TAG, "Door open requested");

    // 获取互斥锁（无限等待）
    if (xSemaphoreTake(s_door_ctrl.mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_FAIL;
    }

    esp_err_t ret = ESP_OK;

    // 1. 设置舵机到开门角度
    ESP_LOGI(TAG, "Opening door to %d degrees", s_door_ctrl.open_angle);
    ret = servo_set_angle(s_door_ctrl.open_angle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set servo to open angle");
        xSemaphoreGive(s_door_ctrl.mutex);
        return ret;
    }

    // 2. 更新状态
    s_door_ctrl.is_open = true;

    // 3. 阻塞等待配置的开门时间
    ESP_LOGI(TAG, "Door open, waiting %lu ms before closing", s_door_ctrl.open_duration_ms);
    vTaskDelay(pdMS_TO_TICKS(s_door_ctrl.open_duration_ms));

    // 4. 设置舵机到关门角度
    ESP_LOGI(TAG, "Closing door to %d degrees", s_door_ctrl.close_angle);
    ret = servo_set_angle(s_door_ctrl.close_angle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set servo to close angle");
        xSemaphoreGive(s_door_ctrl.mutex);
        return ret;
    }

    // 5. 更新状态
    s_door_ctrl.is_open = false;

    // 释放互斥锁
    xSemaphoreGive(s_door_ctrl.mutex);

    ESP_LOGI(TAG, "Door operation completed");
    return ESP_OK;
}

esp_err_t door_close(void)
{
    ESP_LOGI(TAG, "Door close requested");

    // 获取互斥锁（无限等待）
    if (xSemaphoreTake(s_door_ctrl.mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to acquire mutex");
        return ESP_FAIL;
    }

    // 设置舵机到关门角度
    esp_err_t ret = servo_set_angle(s_door_ctrl.close_angle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set servo to close angle");
        xSemaphoreGive(s_door_ctrl.mutex);
        return ret;
    }

    // 更新状态
    s_door_ctrl.is_open = false;

    // 释放互斥锁
    xSemaphoreGive(s_door_ctrl.mutex);

    ESP_LOGI(TAG, "Door closed");
    return ESP_OK;
}

bool door_is_open(void)
{
    return s_door_ctrl.is_open;
}