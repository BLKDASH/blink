#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "board.h"
#include "msg_queue.h"
#include "led_task.h"
#include "key_task.h"
#include "pwm_task.h"
#include "wifi_manager.h"
#include "bt_spp.h"
#include "ha_mqtt.h"

static const char *TAG = "main";

#define MSG_QUEUE_LEN 10

/**
 * @brief MQTT 门命令回调函数
 * 
 * 当收到 MQTT 开关命令时，发送消息到 PWM 队列
 */
static void mqtt_door_callback(bool is_on)
{
    if (is_on) {
        msg_send_mqtt_door_cmd(MQTT_CMD_DOOR_ON);
    } else {
        msg_send_mqtt_door_cmd(MQTT_CMD_DOOR_OFF);
    }
}

/**
 * @brief MQTT 启动任务
 * 
 * 等待 WiFi 连接成功后启动 MQTT 客户端
 */
static void mqtt_start_task(void *pvParameters)
{
    ESP_LOGI(TAG, "MQTT start task waiting for WiFi connection...");
    
    /* 等待 WiFi 连接 */
    while (!wifi_manager_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "WiFi connected, starting MQTT client...");
    
    /* 启动 MQTT 客户端 */
    if (ha_mqtt_start() == ESP_OK) {
        ESP_LOGI(TAG, "MQTT client started successfully");
    } else {
        ESP_LOGE(TAG, "Failed to start MQTT client");
    }
    
    /* 任务完成，删除自己 */
    vTaskDelete(NULL);
}

/**
 * @brief 按键事件回调处理函数
 */
static void key_event_handler(uint8_t gpio_num, key_event_t event)
{
    switch (event) {
        case KEY_EVENT_SINGLE_CLICK:
            // 单击：执行开门操作
            msg_send_key_event(QUEUE_PWM, gpio_num, event);
            break;
        case KEY_EVENT_LONG_PRESS:
            // 长按：切换绿灯
            msg_send_key_event(QUEUE_LED, gpio_num, event);
            break;
        default:
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Hello ESP32-C6!");
    
    // 硬件初始化
    configure_led();
    configure_key();
    
    if (configure_servo() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure servo");
    }
    
    // 消息队列初始化
    if (msg_queue_init_all(MSG_QUEUE_LEN) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize message queues");
        return;
    }
    
    // wifi管理器初始化
    if (wifi_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
    }
    // wifi消息处理
    wifi_manager_start_msg_task();

    // 蓝牙SPP服务初始化
    if (bt_spp_init() != ESP_OK) {
        ESP_LOGW(TAG, "Bluetooth SPP init failed, continuing without BT");
    }

    // MQTT 客户端初始化
    if (ha_mqtt_init() == ESP_OK) {
        // 注册门命令回调
        ha_mqtt_register_door_callback(mqtt_door_callback);
        // 创建 MQTT 启动任务（等待 WiFi 连接后启动）
        xTaskCreate(mqtt_start_task, "mqtt_start", 2048, NULL, 3, NULL);
        ESP_LOGI(TAG, "MQTT client initialized, waiting for WiFi to start");
    } else {
        ESP_LOGW(TAG, "MQTT client init failed, continuing without MQTT");
    }

    // 创建业务任务
    if (led_task_create() != pdPASS) {
        ESP_LOGE(TAG, "Failed to create led task");
        return;
    }
    
    if (pwm_task_create() != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pwm task");
        return;
    }
    
    key_task_config_t key_cfg = {
        .gpio_num = KEY_GPIO,
        .callback = key_event_handler
    };
    if (key_task_create(&key_cfg) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create key task");
        return;
    }

    ESP_LOGI(TAG, "System initialized");
    
    vTaskDelete(NULL);
}
