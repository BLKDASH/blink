/**
 * @file bt_spp.h
 * @brief Bluetooth SPP (Serial Port Profile) service for ESP32-C6
 */

#ifndef BT_SPP_H
#define BT_SPP_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 蓝牙设备名称 */
#define BT_DEVICE_NAME "ESP32-DoorLock"

/* SPP服务名称 */
#define SPP_SERVER_NAME "SPP_SERVER"

/* 指令定义 */
#define BT_CMD_OPEN_DOOR "OPEN"
#define BT_CMD_MAX_LEN   32

/* 响应消息 */
#define BT_RSP_OK        "OK\r\n"
#define BT_RSP_ERROR     "ERROR\r\n"
#define BT_RSP_UNKNOWN   "UNKNOWN\r\n"

/**
 * @brief 初始化蓝牙SPP服务
 * @return ESP_OK成功, 其他失败
 */
esp_err_t bt_spp_init(void);

/**
 * @brief 检查蓝牙是否已连接
 * @return true已连接, false未连接
 */
bool bt_spp_is_connected(void);

/**
 * @brief 通过蓝牙发送数据
 * @param data 数据指针
 * @param len 数据长度
 * @return ESP_OK成功, 其他失败
 */
esp_err_t bt_spp_send(const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BT_SPP_H */
