/**
 * @file wifi_manager.h
 * @brief WiFi管理器模块 - SmartConfig配网功能
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化WiFi管理器
 * 
 * 初始化NVS、网络接口、事件循环，配置WiFi为STA模式
 * 
 * @return ESP_OK成功，其他失败
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief 检查WiFi是否已连接
 * 
 * @return true已连接，false未连接
 */
bool wifi_manager_is_connected(void);

/**
 * @brief 清除WiFi凭据并重启SmartConfig
 * 
 * 清除NVS中存储的WiFi凭据，断开当前连接，重新启动SmartConfig
 * 
 * @return ESP_OK成功，其他失败
 */
esp_err_t wifi_manager_clear_credentials(void);

/**
 * @brief 设置WiFi消息队列
 * 
 * 用于接收来自其他任务的WiFi控制消息
 * 
 * @param queue 消息队列句柄
 */
void wifi_manager_set_queue(QueueHandle_t queue);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
