#include "msg_queue.h"
#include "esp_log.h"

static const char *TAG = "msg_queue";

QueueHandle_t msg_queue_init(uint8_t queue_len)
{
    if (queue_len == 0) {
        ESP_LOGE(TAG, "Invalid queue length: 0");
        return NULL;
    }

    QueueHandle_t queue = xQueueCreate(queue_len, sizeof(msg_t));
    
    if (queue == NULL) {
        ESP_LOGE(TAG, "Failed to create message queue with length %d", queue_len);
        return NULL;
    }

    ESP_LOGI(TAG, "Message queue created with length %d", queue_len);
    return queue;
}

bool msg_queue_send(QueueHandle_t queue, const msg_t *msg, uint32_t timeout_ms)
{
    if (queue == NULL || msg == NULL) {
        ESP_LOGE(TAG, "Invalid parameters: queue or msg is NULL");
        return false;
    }

    TickType_t ticks_to_wait = (timeout_ms == portMAX_DELAY) 
                               ? portMAX_DELAY 
                               : pdMS_TO_TICKS(timeout_ms);

    BaseType_t result = xQueueSend(queue, msg, ticks_to_wait);
    
    if (result != pdTRUE) {
        ESP_LOGW(TAG, "Failed to send message (type=%d), queue full or timeout", msg->type);
        return false;
    }

    return true;
}

bool msg_queue_receive(QueueHandle_t queue, msg_t *msg, uint32_t timeout_ms)
{
    if (queue == NULL || msg == NULL) {
        ESP_LOGE(TAG, "Invalid parameters: queue or msg is NULL");
        return false;
    }

    TickType_t ticks_to_wait = (timeout_ms == portMAX_DELAY) 
                               ? portMAX_DELAY 
                               : pdMS_TO_TICKS(timeout_ms);

    BaseType_t result = xQueueReceive(queue, msg, ticks_to_wait);
    
    if (result != pdTRUE) {
        return false;
    }

    return true;
}

bool msg_send_led(QueueHandle_t queue, uint8_t gpio_num, uint8_t state)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Invalid parameter: queue is NULL");
        return false;
    }

    msg_t msg = {
        .type = MSG_TYPE_LED,
        .data.led = {
            .gpio_num = gpio_num,
            .state = state
        }
    };

    return msg_queue_send(queue, &msg, 100);
}

bool msg_send_key(QueueHandle_t queue, uint8_t gpio_num, key_event_t event)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Invalid parameter: queue is NULL");
        return false;
    }

    msg_t msg = {
        .type = MSG_TYPE_KEY,
        .data.key = {
            .gpio_num = gpio_num,
            .event = event
        }
    };

    return msg_queue_send(queue, &msg, 100);
}

bool msg_send_pwm(QueueHandle_t queue, uint8_t gpio_num, uint8_t duty_percent)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Invalid parameter: queue is NULL");
        return false;
    }

    msg_t msg = {
        .type = MSG_TYPE_PWM,
        .data.pwm = {
            .gpio_num = gpio_num,
            .duty_percent = duty_percent
        }
    };

    return msg_queue_send(queue, &msg, 100);
}

bool msg_send_wifi(QueueHandle_t queue, wifi_cmd_t cmd)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Invalid parameter: queue is NULL");
        return false;
    }

    msg_t msg = {
        .type = MSG_TYPE_WIFI,
        .data.wifi = {
            .cmd = cmd
        }
    };

    return msg_queue_send(queue, &msg, 100);
}

bool msg_type_is_valid(msg_type_t type)
{
    return (type > MSG_TYPE_NONE && type < MSG_TYPE_MAX);
}
