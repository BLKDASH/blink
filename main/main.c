#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "board.h"
#include "led_task.h"
#include "key_task.h"
#include "watchdog_task.h"
#include "system_monitor_task.h"
#include "wifi_manager.h"
#include "bt_spp.h"
#include "ha_mqtt.h"
#include "bt_log_forwarder.h"

static const char *TAG = "main";

/**
 * @brief MQTT 启动任务
 * 
 * 等待 WiFi 连接成功后启动 MQTT 客户端
 */
static void mqtt_start_task(void *pvParameters)
{
    ESP_LOGI(TAG, "MQTT start task waiting for WiFi connection (stack: 3072 bytes)...");
    
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
            ESP_LOGI(TAG, "Single click detected, opening door");
            door_open();
            break;
        case KEY_EVENT_DOUBLE_CLICK:
            // 双击：切换绿色LED状态
            ESP_LOGI(TAG, "Double click detected, toggling green LED");
            led_toggle_green();
            break;
        case KEY_EVENT_LONG_PRESS:
            // 长按：清除 WiFi 凭据并重新启动 SmartConfig
            ESP_LOGI(TAG, "Long press detected, clearing WiFi credentials");
            wifi_manager_clear_credentials();
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
    
    // 门控制器初始化
    if (door_controller_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize door controller");
        return;
    }
    
    // wifi管理器初始化
    if (wifi_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
    }

    // 蓝牙SPP服务初始化
    if (bt_spp_init() != ESP_OK) {
        ESP_LOGW(TAG, "Bluetooth SPP init failed, continuing without BT");
    } else {
        // 蓝牙初始化成功后，初始化日志转发器
        if (bt_log_forwarder_init() != ESP_OK) {
            ESP_LOGW(TAG, "Bluetooth log forwarder init failed, continuing without log forwarding");
        } else {
            ESP_LOGI(TAG, "Bluetooth log forwarder initialized successfully");
        }
    }

    // MQTT 客户端初始化
    if (ha_mqtt_init() == ESP_OK) {
        // 创建 MQTT 启动任务（等待 WiFi 连接后启动）
        xTaskCreate(mqtt_start_task, "mqtt_start", 3072, NULL, 3, NULL);
        ESP_LOGI(TAG, "MQTT client initialized, waiting for WiFi to start");
    } else {
        ESP_LOGW(TAG, "MQTT client init failed, continuing without MQTT");
    }

    // 创建业务任务
    if (led_task_create() != pdPASS) {
        ESP_LOGE(TAG, "Failed to create led task");
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
    
    // 创建系统监控任务
    if (system_monitor_task_create() != pdPASS) {
        ESP_LOGE(TAG, "Failed to create system monitor task");
        return;
    }
    
    // 创建看门狗任务（最后创建，确保系统基本功能正常）
    if (watchdog_task_create() != pdPASS) {
        ESP_LOGE(TAG, "Failed to create watchdog task");
        return;
    }

    ESP_LOGI(TAG, "System initialized");
    
    vTaskDelete(NULL);
}
