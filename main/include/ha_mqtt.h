/**
 * @file ha_mqtt.h
 * @brief Home Assistant MQTT 客户端模块
 * 
 * 实现 MQTT 客户端，集成 Home Assistant 自动发现，
 * 提供开门按钮远程控制功能。
 */

#ifndef HA_MQTT_H
#define HA_MQTT_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 MQTT 客户端
 * 
 * 配置 MQTT 连接参数，注册事件处理器。
 * 需要在 WiFi 初始化后调用。
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ha_mqtt_init(void);

/**
 * @brief 启动 MQTT 客户端
 * 
 * 开始连接 MQTT Broker。
 * 需要在 WiFi 连接成功后调用。
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ha_mqtt_start(void);

/**
 * @brief 停止 MQTT 客户端
 * 
 * 断开 MQTT 连接并释放资源
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ha_mqtt_stop(void);

/**
 * @brief 检查 MQTT 是否已连接
 * 
 * @return true 已连接，false 未连接
 */
bool ha_mqtt_is_connected(void);

/**
 * @brief 发布门状态到 MQTT（可选）
 * 
 * 注意：Button 类型不需要状态反馈，此函数保留用于调试或扩展
 * 
 * @param is_on true 为开，false 为关
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ha_mqtt_publish_door_state(bool is_on);

/**
 * @brief 获取设备 ID
 * 
 * 返回当前使用的设备 ID（配置的或 MAC 生成的）
 * 
 * @return 设备 ID 字符串指针（静态存储）
 */
const char* ha_mqtt_get_device_id(void);

/**
 * @brief 发布 Home Assistant 自动发现配置
 * 
 * 手动触发发布 HA Discovery 配置。
 * 通常在 MQTT 连接成功时自动调用，此函数用于手动重新发布。
 * 
 * @return ESP_OK 成功，其他失败
 */
esp_err_t ha_mqtt_publish_discovery(void);

#ifdef __cplusplus
}
#endif

#endif /* HA_MQTT_H */
