#include "msg_queue.h"
#include "esp_log.h"

static const char *TAG = "msg_queue";

/* 全局队列数组 */
static QueueHandle_t s_queues[QUEUE_MAX] = {NULL};

esp_err_t msg_queue_init_all(uint8_t queue_len)
{
    if (queue_len == 0) {
        ESP_LOGE(TAG, "Invalid queue length: 0");
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < QUEUE_MAX; i++) {
        s_queues[i] = xQueueCreate(queue_len, sizeof(msg_t));
        if (s_queues[i] == NULL) {
            ESP_LOGE(TAG, "Failed to create queue %d", i);
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(TAG, "All queues initialized with length %d", queue_len);
    return ESP_OK;
}

QueueHandle_t msg_queue_get(queue_id_t id)
{
    if (id >= QUEUE_MAX) {
        ESP_LOGE(TAG, "Invalid queue id: %d", id);
        return NULL;
    }
    return s_queues[id];
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
    
    return (result == pdTRUE);
}


// 直通函数：LED控制
bool msg_send_to_led(uint8_t gpio_num, uint8_t state)
{
    QueueHandle_t queue = msg_queue_get(QUEUE_LED);
    if (queue == NULL) {
        ESP_LOGE(TAG, "LED queue not initialized");
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

// PWM开门命令
bool msg_send_pwm_open_door(void)
{
    QueueHandle_t queue = msg_queue_get(QUEUE_PWM);
    if (queue == NULL) {
        ESP_LOGE(TAG, "PWM queue not initialized");
        return false;
    }

    msg_t msg = {
        .type = MSG_TYPE_PWM,
        .data.pwm = {
            .event = PWM_EVENT_OPEN_DOOR,
            .angle = 0
        }
    };

    return msg_queue_send(queue, &msg, 100);
}

// PWM设置角度
bool msg_send_pwm_set_angle(uint8_t angle)
{
    QueueHandle_t queue = msg_queue_get(QUEUE_PWM);
    if (queue == NULL) {
        ESP_LOGE(TAG, "PWM queue not initialized");
        return false;
    }

    msg_t msg = {
        .type = MSG_TYPE_PWM,
        .data.pwm = {
            .event = PWM_EVENT_SET_ANGLE,
            .angle = angle
        }
    };

    return msg_queue_send(queue, &msg, 100);
}

// 直通函数：WIFI cmd
bool msg_send_to_wifi(wifi_cmd_t cmd)
{
    QueueHandle_t queue = msg_queue_get(QUEUE_WIFI);
    if (queue == NULL) {
        ESP_LOGE(TAG, "WiFi queue not initialized");
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

// 按键事件发送函数
bool msg_send_key_event(queue_id_t queue_id, uint8_t gpio_num, key_event_t event)
{
    QueueHandle_t queue = msg_queue_get(queue_id);
    if (queue == NULL) {
        ESP_LOGE(TAG, "Queue %d not initialized", queue_id);
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

// MQTT 开门命令发送函数
bool msg_send_mqtt_door_cmd(mqtt_cmd_t cmd)
{
    QueueHandle_t queue = msg_queue_get(QUEUE_PWM);
    if (queue == NULL) {
        ESP_LOGE(TAG, "PWM queue not initialized");
        return false;
    }

    msg_t msg = {
        .type = MSG_TYPE_MQTT,
        .data.mqtt = {
            .cmd = cmd
        }
    };

    return msg_queue_send(queue, &msg, 100);
}

bool msg_type_is_valid(msg_type_t type)
{
    return (type > MSG_TYPE_NONE && type < MSG_TYPE_MAX);
}
