/**
 * @file bt_log_forwarder.h
 * @brief Bluetooth Log Forwarder - Forward ESP-IDF logs via Bluetooth SPP
 * 
 * This module intercepts ESP-IDF log output and forwards it to connected
 * Bluetooth clients via the SPP (Serial Port Profile) service.
 */

#ifndef BT_LOG_FORWARDER_H
#define BT_LOG_FORWARDER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Bluetooth log forwarder
 * 
 * This function registers a custom vprintf hook with the ESP-IDF log system
 * to intercept all log messages and forward them via Bluetooth.
 * 
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_NO_MEM: Failed to create mutex
 *     - ESP_FAIL: Failed to register vprintf hook
 */
esp_err_t bt_log_forwarder_init(void);

/**
 * @brief Deinitialize the Bluetooth log forwarder
 * 
 * This function restores the original vprintf function and cleans up
 * all allocated resources.
 * 
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_STATE: Not initialized
 */
esp_err_t bt_log_forwarder_deinit(void);

/**
 * @brief Enable or disable log forwarding
 * 
 * When disabled, logs are still printed to console but not forwarded
 * via Bluetooth. This provides zero overhead when forwarding is not needed.
 * 
 * @param enable true to enable forwarding, false to disable
 */
void bt_log_forwarder_enable(bool enable);

/**
 * @brief Check if log forwarding is enabled
 * 
 * @return true if forwarding is enabled, false otherwise
 */
bool bt_log_forwarder_is_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* BT_LOG_FORWARDER_H */
