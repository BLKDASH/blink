/**
 * @file bt_log_forwarder.c
 * @brief Bluetooth Log Forwarder Implementation
 */

#include "bt_log_forwarder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdarg.h>
#include <string.h>

static const char *TAG = "bt_log_forwarder";

/* Log buffer size from Kconfig */
#ifndef CONFIG_BT_LOG_FORWARDER_BUFFER_SIZE
#define CONFIG_BT_LOG_FORWARDER_BUFFER_SIZE 256
#endif

#ifndef CONFIG_BT_LOG_FORWARDER_TIMEOUT_MS
#define CONFIG_BT_LOG_FORWARDER_TIMEOUT_MS 10
#endif

/**
 * @brief Log forwarder state structure
 * 
 * Maintains the state of the log forwarder including initialization status,
 * enable flag, original vprintf function pointer, and mutex for thread safety.
 */
typedef struct {
    bool initialized;                    /* Whether the forwarder is initialized */
    bool enabled;                        /* Whether forwarding is enabled */
    vprintf_like_t original_vprintf;     /* Original vprintf function pointer */
    SemaphoreHandle_t mutex;             /* Mutex for thread safety */
} bt_log_forwarder_state_t;

/* Global state instance */
static bt_log_forwarder_state_t g_forwarder_state = {
    .initialized = false,
    .enabled = false,
    .original_vprintf = NULL,
    .mutex = NULL
};

/* Forward declarations */
static int bt_log_vprintf(const char *format, va_list args);
static void forward_to_bluetooth(const char *log_str, size_t len);

/**
 * @brief Forward log message to Bluetooth
 * 
 * This function handles the actual forwarding of log messages to Bluetooth.
 * It handles truncation for long messages and gracefully handles send failures.
 * 
 * Note: This function assumes Bluetooth connection has already been checked
 * by the caller for performance optimization (Requirement 6.1).
 * 
 * Error Handling (Requirement 3.4, 5.4, 6.2):
 * - Handles buffer overflow by truncation
 * - Handles send failures gracefully without blocking
 * - Never crashes or blocks on errors
 * 
 * Requirements:
 * - 3.1: Send log via Bluetooth SPP
 * - 3.3: Truncate long messages that exceed buffer size
 * - 3.4: Handle send failures gracefully without blocking
 * - 4.4: Add truncation marker for truncated messages
 * - 5.4: Non-blocking error handling
 * - 6.1: Minimize processing overhead
 * - 6.2: Non-blocking behavior when queue is full
 * 
 * @param log_str Log message string
 * @param len Length of the log message
 */
static void forward_to_bluetooth(const char *log_str, size_t len)
{
    /* Validate input parameters (Error handling - Requirement 5.4) */
    if (log_str == NULL || len == 0) {
        /* Invalid input, skip forwarding but don't crash */
        return;
    }
    
    /* Get Bluetooth send function */
    extern esp_err_t bt_spp_send(const char *data, size_t len);
    
    /* Define truncation marker */
    const char *truncation_marker = "...[truncated]\n";
    const size_t marker_len = 16; /* Length of "...[truncated]\n" */
    
    /* Handle long log truncation (Requirements 3.3, 4.4, 5.4) */
    if (len > CONFIG_BT_LOG_FORWARDER_BUFFER_SIZE - marker_len) {
        /* Need to truncate - leave room for truncation marker */
        char truncated_buffer[CONFIG_BT_LOG_FORWARDER_BUFFER_SIZE];
        size_t truncate_len = CONFIG_BT_LOG_FORWARDER_BUFFER_SIZE - marker_len;
        
        /* Copy the truncated portion */
        memcpy(truncated_buffer, log_str, truncate_len);
        
        /* Add truncation marker at the end */
        strcpy(truncated_buffer + truncate_len, truncation_marker);
        
        /* Send truncated message (Requirement 3.1) */
        esp_err_t ret = bt_spp_send(truncated_buffer, truncate_len + marker_len);
        
        /* Handle send failure gracefully (Requirement 3.4, 5.4, 6.2) */
        if (ret != ESP_OK) {
            /* Send failed - could be due to:
             * - Queue full (Requirement 6.2)
             * - Connection lost
             * - Other Bluetooth errors
             * 
             * We don't log here to avoid recursion, and we don't block.
             * The system continues to operate normally.
             */
        }
    } else {
        /* Send the full message (Requirement 3.1) */
        esp_err_t ret = bt_spp_send(log_str, len);
        
        /* Handle send failure gracefully (Requirement 3.4, 5.4, 6.2) */
        if (ret != ESP_OK) {
            /* Send failed - could be due to:
             * - Queue full (Requirement 6.2)
             * - Connection lost
             * - Other Bluetooth errors
             * 
             * We don't log here to avoid recursion, and we don't block.
             * The system continues to operate normally.
             */
        }
    }
}

/**
 * @brief Custom vprintf hook function
 * 
 * This function intercepts all log output from ESP-IDF log system.
 * It formats the log message, preserves console output, and forwards
 * to Bluetooth if enabled and connected.
 * 
 * Performance Optimization (Requirement 6.1, 6.4):
 * - Fast path: Early return when forwarding is disabled (zero overhead)
 * - Fast path: Early return when Bluetooth is not connected
 * - Minimizes unnecessary function calls and processing
 * - Uses static buffer to reduce stack usage
 * 
 * Thread Safety (Requirement 5.1, 5.3):
 * - Uses mutex with timeout to protect shared state
 * - Static buffer is protected by mutex (only one task uses it at a time)
 * - Ensures mutex is released in all exit paths
 * - Non-blocking behavior on mutex timeout
 * 
 * Error Handling (Requirement 3.4, 5.4, 6.2):
 * - Handles mutex acquisition timeout gracefully
 * - Handles buffer overflow by truncation
 * - Continues operation on Bluetooth send failures
 * - Never blocks the caller
 * 
 * Requirements:
 * - 1.2-1.6: Intercept all log levels (ERROR, WARN, INFO, DEBUG, VERBOSE)
 * - 1.7: Preserve console output by calling original vprintf
 * - 2.1-2.5: Forward all log levels to Bluetooth
 * - 4.2: Preserve existing newlines in log messages
 * - 4.3: Add newline if message doesn't end with one
 * - 5.1: Thread-safe access to shared state
 * - 5.3: Mutual exclusion for Bluetooth send operations
 * - 5.4: Non-blocking error handling
 * - 6.1: Complete processing within 10ms
 * - 6.2: Non-blocking behavior when queue is full
 * - 6.4: Zero overhead when disabled
 * 
 * @param format Format string
 * @param args Variable argument list
 * @return Number of characters written
 */
static int bt_log_vprintf(const char *format, va_list args)
{
    int ret = 0;
    bool mutex_acquired = false;
    
    /* Always call original vprintf to preserve console output (Requirement 1.7) */
    if (g_forwarder_state.original_vprintf) {
        va_list args_copy;
        va_copy(args_copy, args);
        ret = g_forwarder_state.original_vprintf(format, args_copy);
        va_end(args_copy);
    } else {
        va_list args_copy;
        va_copy(args_copy, args);
        ret = vprintf(format, args_copy);
        va_end(args_copy);
    }
    
    /* Fast path optimization: Check if forwarding is enabled (Requirement 6.1, 6.4) */
    /* This provides zero overhead when disabled - just the cost of checking a boolean */
    if (!g_forwarder_state.enabled) {
        return ret;
    }
    
    /* Fast path optimization: Check if Bluetooth is connected (Requirement 6.1) */
    /* Avoid unnecessary processing if no client is connected */
    extern bool bt_spp_is_connected(void);
    if (!bt_spp_is_connected()) {
        /* No Bluetooth connection, skip all forwarding logic */
        return ret;
    }
    
    /* Fast path: Check if mutex is available (Error handling - Requirement 5.4) */
    if (g_forwarder_state.mutex == NULL) {
        /* Mutex not initialized, skip forwarding but don't block */
        return ret;
    }
    
    /* Try to acquire mutex with timeout to avoid deadlock (Requirement 5.1, 5.3) */
    TickType_t timeout = pdMS_TO_TICKS(CONFIG_BT_LOG_FORWARDER_TIMEOUT_MS);
    if (xSemaphoreTake(g_forwarder_state.mutex, timeout) != pdTRUE) {
        /* Timeout acquiring mutex - skip forwarding to avoid blocking (Requirement 5.4, 6.2) */
        /* This ensures non-blocking behavior even under high contention */
        return ret;
    }
    
    /* Mark that we acquired the mutex so we can release it in all exit paths */
    mutex_acquired = true;
    
    /* Use static buffer to reduce stack usage - protected by mutex */
    static char log_buffer[CONFIG_BT_LOG_FORWARDER_BUFFER_SIZE];
    
    /* Format log message to buffer */
    int formatted_len = vsnprintf(log_buffer, sizeof(log_buffer), format, args);
    
    /* Handle buffer overflow (Error handling - Requirement 5.4) */
    if (formatted_len < 0) {
        /* vsnprintf error - skip forwarding but don't crash */
        goto cleanup;
    }
    
    if (formatted_len > 0) {
        /* Calculate actual length (may be truncated by vsnprintf) */
        size_t actual_len = (size_t)formatted_len;
        if (actual_len >= sizeof(log_buffer)) {
            /* Buffer overflow - vsnprintf truncated the output (Requirement 5.4) */
            actual_len = sizeof(log_buffer) - 1;
        }
        
        /* Check if message already ends with newline (Requirement 4.2) */
        bool has_newline = (actual_len > 0 && log_buffer[actual_len - 1] == '\n');
        
        /* Add newline if missing and there's room (Requirement 4.3) */
        if (!has_newline && actual_len < sizeof(log_buffer) - 1) {
            log_buffer[actual_len] = '\n';
            log_buffer[actual_len + 1] = '\0';
            actual_len++;
        }
        
        /* Forward to Bluetooth (Requirements 2.1-2.5) */
        /* Note: forward_to_bluetooth handles Bluetooth send failures gracefully (Requirement 3.4) */
        forward_to_bluetooth(log_buffer, actual_len);
    }
    
cleanup:
    /* Always release mutex before returning (Requirement 5.1) */
    if (mutex_acquired) {
        xSemaphoreGive(g_forwarder_state.mutex);
    }
    
    return ret;
}

/**
 * @brief Initialize the Bluetooth log forwarder
 * 
 * Error Handling (Requirement 7.4):
 * - Validates state before initialization
 * - Handles mutex creation failure
 * - Handles vprintf hook registration failure
 * - Cleans up resources on failure
 * - Returns appropriate error codes
 * 
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if already initialized
 *         ESP_ERR_NO_MEM if mutex creation fails
 *         ESP_FAIL if vprintf hook registration fails
 */
esp_err_t bt_log_forwarder_init(void)
{
    ESP_LOGI(TAG, "Initializing Bluetooth log forwarder");
    
    /* Check if already initialized (Error handling - Requirement 7.4) */
    if (g_forwarder_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Create mutex for thread safety (Requirement 5.1) */
    g_forwarder_state.mutex = xSemaphoreCreateMutex();
    if (g_forwarder_state.mutex == NULL) {
        /* Mutex creation failed (Error handling - Requirement 7.4) */
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    /* Save original vprintf function pointer and register our hook (Requirement 1.1) */
    g_forwarder_state.original_vprintf = esp_log_set_vprintf(bt_log_vprintf);
    if (g_forwarder_state.original_vprintf == NULL) {
        /* Hook registration failed - clean up and return error (Error handling - Requirement 7.4) */
        ESP_LOGE(TAG, "Failed to register vprintf hook");
        vSemaphoreDelete(g_forwarder_state.mutex);
        g_forwarder_state.mutex = NULL;
        return ESP_FAIL;
    }
    
    /* Mark as initialized and enabled by default */
    g_forwarder_state.initialized = true;
    g_forwarder_state.enabled = true;
    
    ESP_LOGI(TAG, "Bluetooth log forwarder initialized successfully");
    return ESP_OK;
}

/**
 * @brief Deinitialize the Bluetooth log forwarder
 * 
 * Error Handling (Requirement 7.4):
 * - Validates state before deinitialization
 * - Safely restores original vprintf
 * - Properly cleans up mutex
 * - Ensures consistent state after cleanup
 * 
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t bt_log_forwarder_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing Bluetooth log forwarder");
    
    /* Check if initialized (Error handling - Requirement 7.4) */
    if (!g_forwarder_state.initialized) {
        ESP_LOGW(TAG, "Not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    /* Disable forwarding first to prevent new operations */
    g_forwarder_state.enabled = false;
    
    /* Restore original vprintf function */
    if (g_forwarder_state.original_vprintf != NULL) {
        esp_log_set_vprintf(g_forwarder_state.original_vprintf);
        g_forwarder_state.original_vprintf = NULL;
    }
    
    /* Delete mutex - ensure no one is using it */
    if (g_forwarder_state.mutex != NULL) {
        vSemaphoreDelete(g_forwarder_state.mutex);
        g_forwarder_state.mutex = NULL;
    }
    
    /* Clear state */
    g_forwarder_state.initialized = false;
    
    ESP_LOGI(TAG, "Bluetooth log forwarder deinitialized successfully");
    return ESP_OK;
}

/**
 * @brief Enable or disable log forwarding
 */
void bt_log_forwarder_enable(bool enable)
{
    if (!g_forwarder_state.initialized) {
        ESP_LOGW(TAG, "Not initialized, cannot change enable state");
        return;
    }
    
    g_forwarder_state.enabled = enable;
    ESP_LOGI(TAG, "Log forwarding %s", enable ? "enabled" : "disabled");
}

/**
 * @brief Check if log forwarding is enabled
 */
bool bt_log_forwarder_is_enabled(void)
{
    return g_forwarder_state.initialized && g_forwarder_state.enabled;
}
