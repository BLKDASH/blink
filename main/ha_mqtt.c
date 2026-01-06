/**
 * @file ha_mqtt.c
 * @brief Home Assistant MQTT 客户端模块实现
 * 
 * 实现 MQTT 客户端，集成 Home Assistant 自动发现，
 * 提供开门开关远程控制功能。
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "mqtt_client.h"

#include "ha_mqtt.h"

static const char *TAG = "ha_mqtt";

/* 事件组位定义 */
#define MQTT_CONNECTED_BIT    BIT0
#define MQTT_DISCONNECTED_BIT BIT1

/* 主题缓冲区大小 */
#define TOPIC_BUF_SIZE 128
#define PAYLOAD_BUF_SIZE 512
#define DEVICE_ID_SIZE 16

/* 静态变量 */
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static EventGroupHandle_t s_mqtt_event_group = NULL;
static ha_mqtt_door_callback_t s_door_callback = NULL;
static char s_device_id[DEVICE_ID_SIZE] = {0};
static bool s_initialized = false;

/* 主题字符串 */
static char s_cmd_topic[TOPIC_BUF_SIZE] = {0};
static char s_state_topic[TOPIC_BUF_SIZE] = {0};
static char s_availability_topic[TOPIC_BUF_SIZE] = {0};
static char s_discovery_topic[TOPIC_BUF_SIZE] = {0};

/* 前向声明 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                               int32_t event_id, void *event_data);
static void generate_device_id(void);
static void build_topics(void);
static esp_err_t publish_ha_discovery(void);


/**
 * @brief 生成设备 ID
 * 
 * 如果配置了自定义设备 ID 则使用配置值，
 * 否则使用 MAC 地址后 3 字节（6 个十六进制字符）
 */
static void generate_device_id(void)
{
    /* 检查是否配置了自定义设备 ID */
    const char *custom_id = CONFIG_HA_MQTT_DEVICE_ID;
    
    if (custom_id != NULL && strlen(custom_id) > 0) {
        /* 使用自定义设备 ID */
        strncpy(s_device_id, custom_id, DEVICE_ID_SIZE - 1);
        s_device_id[DEVICE_ID_SIZE - 1] = '\0';
        ESP_LOGI(TAG, "Using custom device ID: %s", s_device_id);
    } else {
        /* 使用 MAC 地址后 3 字节生成设备 ID */
        uint8_t mac[6];
        esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
        
        if (ret == ESP_OK) {
            snprintf(s_device_id, DEVICE_ID_SIZE, "%02x%02x%02x", 
                     mac[3], mac[4], mac[5]);
            ESP_LOGI(TAG, "Generated device ID from MAC: %s", s_device_id);
        } else {
            /* 回退到默认值 */
            strncpy(s_device_id, "esp32c6", DEVICE_ID_SIZE - 1);
            ESP_LOGW(TAG, "Failed to read MAC, using default device ID: %s", s_device_id);
        }
    }
}

/**
 * @brief 构建 MQTT 主题字符串
 */
static void build_topics(void)
{
    snprintf(s_cmd_topic, TOPIC_BUF_SIZE, "esp32c6/%s/door/set", s_device_id);
    snprintf(s_state_topic, TOPIC_BUF_SIZE, "esp32c6/%s/door/state", s_device_id);
    snprintf(s_availability_topic, TOPIC_BUF_SIZE, "esp32c6/%s/availability", s_device_id);
    snprintf(s_discovery_topic, TOPIC_BUF_SIZE, "homeassistant/switch/%s/door/config", s_device_id);
    
    ESP_LOGI(TAG, "Command topic: %s", s_cmd_topic);
    ESP_LOGI(TAG, "State topic: %s", s_state_topic);
    ESP_LOGI(TAG, "Availability topic: %s", s_availability_topic);
    ESP_LOGI(TAG, "Discovery topic: %s", s_discovery_topic);
}


/**
 * @brief 发布 Home Assistant 自动发现配置
 * 
 * 生成符合 Home Assistant MQTT Discovery 规范的 JSON 配置，
 * 并发布到 Discovery 主题。
 * 
 * @return ESP_OK 成功，其他失败
 */
static esp_err_t publish_ha_discovery(void)
{
    if (!ha_mqtt_is_connected()) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish discovery");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* 构建 Discovery JSON 配置 */
    char discovery_payload[PAYLOAD_BUF_SIZE];
    int len = snprintf(discovery_payload, PAYLOAD_BUF_SIZE,
        "{"
        "\"name\":\"Door Switch\","
        "\"unique_id\":\"%s_door\","
        "\"command_topic\":\"%s\","
        "\"state_topic\":\"%s\","
        "\"availability_topic\":\"%s\","
        "\"payload_on\":\"ON\","
        "\"payload_off\":\"OFF\","
        "\"payload_available\":\"online\","
        "\"payload_not_available\":\"offline\","
        "\"device\":{"
            "\"identifiers\":[\"%s\"],"
            "\"name\":\"ESP32-C6 Door Controller\","
            "\"model\":\"ESP32-C6\","
            "\"manufacturer\":\"Espressif\""
        "}"
        "}",
        s_device_id,
        s_cmd_topic,
        s_state_topic,
        s_availability_topic,
        s_device_id
    );
    
    if (len < 0 || len >= PAYLOAD_BUF_SIZE) {
        ESP_LOGE(TAG, "Discovery payload buffer overflow");
        return ESP_ERR_NO_MEM;
    }
    
    /* 发布 Discovery 配置（retain=true 确保 HA 重启后仍能发现设备） */
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, s_discovery_topic,
                                          discovery_payload, 0, 1, 1);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish HA discovery config");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Published HA discovery config to %s, msg_id=%d", 
             s_discovery_topic, msg_id);
    ESP_LOGD(TAG, "Discovery payload: %s", discovery_payload);
    
    return ESP_OK;
}


/**
 * @brief MQTT 事件处理器
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected to broker");
            xEventGroupSetBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            xEventGroupClearBits(s_mqtt_event_group, MQTT_DISCONNECTED_BIT);
            
            /* 发布在线状态 */
            esp_mqtt_client_publish(s_mqtt_client, s_availability_topic, 
                                    "online", 0, 1, 1);
            
            /* 发布 Home Assistant 自动发现配置 */
            publish_ha_discovery();
            
            /* 订阅命令主题 */
            int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, s_cmd_topic, 1);
            ESP_LOGI(TAG, "Subscribed to %s, msg_id=%d", s_cmd_topic, msg_id);
            
            /* 发布初始门状态（默认为 OFF） */
            esp_mqtt_client_publish(s_mqtt_client, s_state_topic, "OFF", 0, 1, 1);
            ESP_LOGI(TAG, "Published initial door state: OFF");
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected from broker");
            xEventGroupClearBits(s_mqtt_event_group, MQTT_CONNECTED_BIT);
            xEventGroupSetBits(s_mqtt_event_group, MQTT_DISCONNECTED_BIT);
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT unsubscribed, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT published, msg_id=%d", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT data received on topic: %.*s", 
                     event->topic_len, event->topic);
            ESP_LOGI(TAG, "Data: %.*s", event->data_len, event->data);
            
            /* 检查是否是命令主题 */
            if (event->topic_len > 0 && 
                strncmp(event->topic, s_cmd_topic, event->topic_len) == 0) {
                
                /* 解析命令 */
                if (event->data_len >= 2 && strncmp(event->data, "ON", 2) == 0) {
                    ESP_LOGI(TAG, "Received door ON command");
                    if (s_door_callback != NULL) {
                        s_door_callback(true);
                    }
                } else if (event->data_len >= 3 && strncmp(event->data, "OFF", 3) == 0) {
                    ESP_LOGI(TAG, "Received door OFF command");
                    if (s_door_callback != NULL) {
                        s_door_callback(false);
                    }
                } else {
                    ESP_LOGW(TAG, "Unknown command: %.*s", event->data_len, event->data);
                }
            }
            break;
            
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error occurred");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "TCP transport error: %s", 
                         strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;
            
        default:
            ESP_LOGD(TAG, "MQTT event: %d", event->event_id);
            break;
    }
}


esp_err_t ha_mqtt_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "MQTT client already initialized");
        return ESP_OK;
    }
    
    /* 检查 Broker URI 配置 */
    const char *broker_uri = CONFIG_HA_MQTT_BROKER_URI;
    if (broker_uri == NULL || strlen(broker_uri) == 0) {
        ESP_LOGE(TAG, "MQTT Broker URI not configured");
        return ESP_ERR_INVALID_ARG;
    }
    
    /* 生成设备 ID */
    generate_device_id();
    
    /* 构建主题 */
    build_topics();
    
    /* 创建事件组 */
    s_mqtt_event_group = xEventGroupCreate();
    if (s_mqtt_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }
    
    /* 配置 MQTT 客户端 - ESP-IDF 5.x 风格 */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = broker_uri,
            },
        },
        .session = {
            .last_will = {
                .topic = s_availability_topic,
                .msg = "offline",
                .qos = 1,
                .retain = 1,
            },
        },
    };
    
    /* 配置认证（如果有） */
    const char *username = CONFIG_HA_MQTT_USERNAME;
    const char *password = CONFIG_HA_MQTT_PASSWORD;
    
    if (username != NULL && strlen(username) > 0) {
        mqtt_cfg.credentials.username = username;
        if (password != NULL && strlen(password) > 0) {
            mqtt_cfg.credentials.authentication.password = password;
        }
        ESP_LOGI(TAG, "MQTT authentication configured with username: %s", username);
    }
    
    /* 创建 MQTT 客户端 */
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        vEventGroupDelete(s_mqtt_event_group);
        s_mqtt_event_group = NULL;
        return ESP_FAIL;
    }
    
    /* 注册事件处理器 */
    esp_err_t ret = esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, 
                                                    mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(ret));
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        vEventGroupDelete(s_mqtt_event_group);
        s_mqtt_event_group = NULL;
        return ret;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "MQTT client initialized, broker: %s", broker_uri);
    
    return ESP_OK;
}


esp_err_t ha_mqtt_start(void)
{
    if (!s_initialized || s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_mqtt_client_start(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "MQTT client started");
    return ESP_OK;
}

esp_err_t ha_mqtt_stop(void)
{
    if (!s_initialized || s_mqtt_client == NULL) {
        ESP_LOGW(TAG, "MQTT client not initialized");
        return ESP_OK;
    }
    
    /* 发布离线状态 */
    if (ha_mqtt_is_connected()) {
        esp_mqtt_client_publish(s_mqtt_client, s_availability_topic, 
                                "offline", 0, 1, 1);
        vTaskDelay(pdMS_TO_TICKS(100)); /* 等待消息发送 */
    }
    
    esp_err_t ret = esp_mqtt_client_stop(s_mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "MQTT client stopped");
    return ESP_OK;
}

bool ha_mqtt_is_connected(void)
{
    if (s_mqtt_event_group == NULL) {
        return false;
    }
    
    EventBits_t bits = xEventGroupGetBits(s_mqtt_event_group);
    return (bits & MQTT_CONNECTED_BIT) != 0;
}

esp_err_t ha_mqtt_publish_door_state(bool is_on)
{
    if (!s_initialized || s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!ha_mqtt_is_connected()) {
        ESP_LOGW(TAG, "MQTT not connected, cannot publish state");
        return ESP_ERR_INVALID_STATE;
    }
    
    const char *state = is_on ? "ON" : "OFF";
    int msg_id = esp_mqtt_client_publish(s_mqtt_client, s_state_topic, 
                                          state, 0, 1, 1);
    
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish door state");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Published door state: %s, msg_id=%d", state, msg_id);
    return ESP_OK;
}

void ha_mqtt_register_door_callback(ha_mqtt_door_callback_t callback)
{
    s_door_callback = callback;
    ESP_LOGI(TAG, "Door callback %s", callback ? "registered" : "unregistered");
}

const char* ha_mqtt_get_device_id(void)
{
    return s_device_id;
}

esp_err_t ha_mqtt_publish_discovery(void)
{
    if (!s_initialized || s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    return publish_ha_discovery();
}
