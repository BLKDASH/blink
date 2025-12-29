/**
 * @file wifi_manager.c
 * @brief WiFi管理器模块 - SmartConfig配网功能实现
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "wifi_manager.h"
#include "board.h"
#include "msg_queue.h"

static const char *TAG = "wifi_manager";

/* 事件组位定义 */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const int SMARTCONFIG_RUNNING_BIT = BIT2;

/* 静态变量 */
static EventGroupHandle_t s_wifi_event_group = NULL;
static TaskHandle_t s_smartconfig_task_handle = NULL;
static TaskHandle_t s_led_blink_task_handle = NULL;
static TaskHandle_t s_wifi_msg_task_handle = NULL;
static QueueHandle_t s_wifi_queue = NULL;

/* 前向声明 */
static void smartconfig_task(void *parm);
static void led_status_task(void *parm);
static void wifi_msg_task(void *parm);
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

/**
 * @brief WiFi和SmartConfig事件处理函数
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* WiFi STA启动，创建SmartConfig任务 */
        xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, &s_smartconfig_task_handle);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        /* WiFi断开，尝试重连并清除连接标志 */
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi disconnected, attempting to reconnect...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        /* 获取IP地址，设置连接标志 */
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        /* SmartConfig扫描完成 */
        ESP_LOGI(TAG, "SmartConfig scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        /* SmartConfig找到信道 */
        ESP_LOGI(TAG, "SmartConfig found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        /* SmartConfig获取到SSID和密码 */
        ESP_LOGI(TAG, "SmartConfig got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;

        memset(&wifi_config, 0, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        ESP_LOGI(TAG, "SSID: %s", wifi_config.sta.ssid);

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        /* SmartConfig发送ACK完成 */
        ESP_LOGI(TAG, "SmartConfig send ACK done");
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

/**
 * @brief LED状态指示任务
 * 
 * SmartConfig运行时快闪红灯（200ms间隔），
 * WiFi连接成功时关闭红灯（GPIO高电平）。
 * 不修改绿灯状态。
 * 
 * Requirements: 5.1, 5.2, 5.3, 5.4
 */
static void led_status_task(void *parm)
{
    bool led_state = false;
    
    ESP_LOGI(TAG, "LED status task started");
    
    while (1) {
        /* 检查SmartConfig是否仍在运行 */
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        
        if (bits & CONNECTED_BIT) {
            /* WiFi已连接，关闭红灯（GPIO高电平）并退出 */
            gpio_set_level(LED_RED_GPIO, 1);
            ESP_LOGI(TAG, "WiFi connected, red LED off");
            break;
        }
        
        if (!(bits & SMARTCONFIG_RUNNING_BIT)) {
            /* SmartConfig已停止，关闭红灯并退出 */
            gpio_set_level(LED_RED_GPIO, 1);
            break;
        }
        
        /* SmartConfig运行中，快闪红灯（200ms间隔） */
        led_state = !led_state;
        gpio_set_level(LED_RED_GPIO, led_state ? 0 : 1);  /* 低电平点亮 */
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    /* 任务结束前清理 */
    s_led_blink_task_handle = NULL;
    vTaskDelete(NULL);
}

/**
 * @brief SmartConfig任务
 * 
 * 设置SmartConfig类型为ESPTouch，启动SmartConfig，
 * 等待连接成功或配网完成，然后停止SmartConfig并删除任务。
 * 
 * Requirements: 2.2, 2.4
 */
static void smartconfig_task(void *parm)
{
    EventBits_t uxBits;
    
    /* 设置SmartConfig运行标志 */
    xEventGroupSetBits(s_wifi_event_group, SMARTCONFIG_RUNNING_BIT);
    
    /* 启动LED状态指示任务 (Requirement 5.1) */
    xTaskCreate(led_status_task, "led_status_task", 2048, NULL, 2, &s_led_blink_task_handle);
    
    /* 设置SmartConfig类型为ESPTouch (Requirement 2.2) */
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    
    /* 启动SmartConfig */
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    ESP_LOGI(TAG, "SmartConfig started, waiting for credentials...");
    
    while (1) {
        /* 等待CONNECTED_BIT或ESPTOUCH_DONE_BIT */
        uxBits = xEventGroupWaitBits(s_wifi_event_group, 
                                     CONNECTED_BIT | ESPTOUCH_DONE_BIT, 
                                     pdTRUE,    /* 清除等待到的位 */
                                     pdFALSE,   /* 任一位满足即可 */
                                     portMAX_DELAY);
        
        if (uxBits & ESPTOUCH_DONE_BIT) {
            /* SmartConfig完成，停止并删除任务 (Requirement 2.4) */
            ESP_LOGI(TAG, "SmartConfig completed successfully");
            esp_smartconfig_stop();
            xEventGroupClearBits(s_wifi_event_group, SMARTCONFIG_RUNNING_BIT);
            
            /* 停止LED任务并关闭红灯 (Requirement 5.2) */
            if (s_led_blink_task_handle != NULL) {
                vTaskDelete(s_led_blink_task_handle);
                s_led_blink_task_handle = NULL;
            }
            gpio_set_level(LED_RED_GPIO, 1);  /* 关闭红灯（高电平） */
            
            s_smartconfig_task_handle = NULL;
            vTaskDelete(NULL);
        }
        
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi connected to AP");
        }
    }
}

/**
 * @brief WiFi消息处理任务
 * 
 * 接收来自其他任务的WiFi控制消息并处理
 * 
 * Requirements: 6.1
 */
static void wifi_msg_task(void *parm)
{
    msg_t msg;
    
    ESP_LOGI(TAG, "WiFi message task started");
    
    while (1) {
        if (s_wifi_queue == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        if (msg_queue_receive(s_wifi_queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_WIFI) {
                switch (msg.data.wifi.cmd) {
                    case WIFI_CMD_CLEAR_CREDENTIALS:
                        ESP_LOGI(TAG, "Received clear credentials command");
                        wifi_manager_clear_credentials();
                        break;
                    default:
                        ESP_LOGW(TAG, "Unknown WiFi command: %d", msg.data.wifi.cmd);
                        break;
                }
            } else {
                ESP_LOGW(TAG, "Received non-WiFi message type: %d", msg.type);
            }
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    esp_err_t ret;

    /* 1. 初始化NVS（带错误恢复） */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition issue, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "NVS initialized");

    /* 2. 创建事件组 */
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    /* 3. 初始化网络接口 */
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Network interface init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 4. 创建默认事件循环 */
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Event loop create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 5. 创建默认WiFi STA网络接口 */
    esp_netif_create_default_wifi_sta();

    /* 6. 初始化WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 7. 注册事件处理函数 */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    /* 8. 设置WiFi为STA模式 */
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 9. 启动WiFi */
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    return ESP_OK;
}

/**
 * @brief 检查WiFi是否已连接
 * 
 * 检查事件组中的CONNECTED_BIT来判断连接状态
 * 
 * @return true已连接，false未连接
 * 
 * Requirements: 4.1
 */
bool wifi_manager_is_connected(void)
{
    if (s_wifi_event_group == NULL) {
        return false;
    }
    
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & CONNECTED_BIT) != 0;
}

/**
 * @brief 清除WiFi凭据并重启SmartConfig
 * 
 * 调用esp_wifi_restore()清除NVS中存储的WiFi凭据，
 * 断开当前WiFi连接，重新启动SmartConfig流程。
 * 
 * @return ESP_OK成功，其他失败
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.5
 */
esp_err_t wifi_manager_clear_credentials(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Clearing WiFi credentials...");
    
    /* 1. 停止当前SmartConfig任务（如果正在运行） */
    if (s_smartconfig_task_handle != NULL) {
        esp_smartconfig_stop();
        xEventGroupClearBits(s_wifi_event_group, SMARTCONFIG_RUNNING_BIT);
        vTaskDelete(s_smartconfig_task_handle);
        s_smartconfig_task_handle = NULL;
    }
    
    /* 2. 停止LED闪烁任务（如果正在运行） */
    if (s_led_blink_task_handle != NULL) {
        vTaskDelete(s_led_blink_task_handle);
        s_led_blink_task_handle = NULL;
    }
    gpio_set_level(LED_RED_GPIO, 1);  /* 关闭红灯 */
    
    /* 3. 断开当前WiFi连接 (Requirement 6.2) */
    ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi disconnect failed: %s", esp_err_to_name(ret));
    }
    xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    
    /* 4. 清除WiFi凭据 (Requirement 6.1) */
    ret = esp_wifi_restore();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi restore failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WiFi credentials cleared from NVS");
    
    /* 5. 重新启动SmartConfig (Requirement 6.3) */
    xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, &s_smartconfig_task_handle);
    ESP_LOGI(TAG, "SmartConfig restarted");
    
    return ESP_OK;
}

/**
 * @brief 设置WiFi消息队列
 * 
 * 保存队列句柄到静态变量，并启动WiFi消息处理任务
 * 
 * @param queue 消息队列句柄
 * 
 * Requirements: 4.4
 */
void wifi_manager_set_queue(QueueHandle_t queue)
{
    s_wifi_queue = queue;
    ESP_LOGI(TAG, "WiFi message queue set");
    
    /* 启动WiFi消息处理任务 */
    if (queue != NULL && s_wifi_msg_task_handle == NULL) {
        xTaskCreate(wifi_msg_task, "wifi_msg_task", 2048, NULL, 4, &s_wifi_msg_task_handle);
        ESP_LOGI(TAG, "WiFi message task created");
    }
}
