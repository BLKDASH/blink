/**
 * @file wifi_manager.c
 * @brief WiFi管理器模块 - SmartConfig配网功能实现
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "nvs_flash.h"

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

/* 前向声明 */
static void smartconfig_task(void *parm);
static void led_status_task(void *parm);
static void wifi_msg_task(void *parm);
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, &s_smartconfig_task_handle);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi disconnected, attempting to reconnect...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "SmartConfig scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "SmartConfig found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
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
        ESP_LOGI(TAG, "SmartConfig send ACK done");
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

static void led_status_task(void *parm)
{
    uint8_t led_state = LED_RED_OFF;
    
    ESP_LOGI(TAG, "LED status task started");
    
    while (1) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
        
        if (bits & CONNECTED_BIT) {
            msg_send_to_led(LED_RED_GPIO, LED_RED_OFF);
            ESP_LOGI(TAG, "WiFi connected, red LED off");
            break;
        }
        
        if (!(bits & SMARTCONFIG_RUNNING_BIT)) {
            msg_send_to_led(LED_RED_GPIO, LED_RED_OFF);
            break;
        }
        
        led_state = (led_state == LED_RED_OFF) ? LED_RED_ON : LED_RED_OFF;
        msg_send_to_led(LED_RED_GPIO, led_state);
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    s_led_blink_task_handle = NULL;
    vTaskDelete(NULL);
}

static void smartconfig_task(void *parm)
{
    EventBits_t uxBits;
    
    xEventGroupSetBits(s_wifi_event_group, SMARTCONFIG_RUNNING_BIT);
    xTaskCreate(led_status_task, "led_status_task", 2048, NULL, 2, &s_led_blink_task_handle);
    
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    ESP_LOGI(TAG, "SmartConfig started, waiting for credentials...");
    
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, 
                                     CONNECTED_BIT | ESPTOUCH_DONE_BIT, 
                                     pdTRUE, pdFALSE, portMAX_DELAY);
        
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "SmartConfig completed successfully");
            esp_smartconfig_stop();
            xEventGroupClearBits(s_wifi_event_group, SMARTCONFIG_RUNNING_BIT);
            
            if (s_led_blink_task_handle != NULL) {
                vTaskDelete(s_led_blink_task_handle);
                s_led_blink_task_handle = NULL;
            }
            msg_send_to_led(LED_RED_GPIO, LED_OFF);
            
            s_smartconfig_task_handle = NULL;
            vTaskDelete(NULL);
        }
        
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi connected to AP");
        }
    }
}

static void wifi_msg_task(void *parm)
{
    QueueHandle_t wifi_queue = msg_queue_get(QUEUE_WIFI);
    msg_t msg;
    
    ESP_LOGI(TAG, "WiFi message task started");
    
    while (1) {
        if (msg_queue_receive(wifi_queue, &msg, portMAX_DELAY)) {
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

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Network interface init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Event loop create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi set mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi manager initialized successfully");
    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    if (s_wifi_event_group == NULL) return false;
    EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
    return (bits & CONNECTED_BIT) != 0;
}

esp_err_t wifi_manager_clear_credentials(void)
{
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Clearing WiFi credentials...");
    
    if (s_smartconfig_task_handle != NULL) {
        esp_smartconfig_stop();
        xEventGroupClearBits(s_wifi_event_group, SMARTCONFIG_RUNNING_BIT);
        vTaskDelete(s_smartconfig_task_handle);
        s_smartconfig_task_handle = NULL;
    }
    
    if (s_led_blink_task_handle != NULL) {
        vTaskDelete(s_led_blink_task_handle);
        s_led_blink_task_handle = NULL;
    }
    msg_send_to_led(LED_RED_GPIO, LED_OFF);
    
    ret = esp_wifi_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "WiFi disconnect failed: %s", esp_err_to_name(ret));
    }
    xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    
    ret = esp_wifi_restore();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi restore failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "WiFi credentials cleared from NVS");
    
    xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, &s_smartconfig_task_handle);
    ESP_LOGI(TAG, "SmartConfig restarted");
    
    return ESP_OK;
}

void wifi_manager_start_msg_task(void)
{
    if (s_wifi_msg_task_handle == NULL) {
        xTaskCreate(wifi_msg_task, "wifi_msg_task", 2048, NULL, 4, &s_wifi_msg_task_handle);
        ESP_LOGI(TAG, "WiFi message task created");
    }
}
