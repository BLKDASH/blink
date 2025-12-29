#include "action_task.h"
#include "msg_queue.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "action_task";

#define ACTION_TASK_STACK_SIZE 2048
#define ACTION_TASK_PRIORITY   5

static void action_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    msg_t msg;
    static uint8_t pwm_high = 0;

    ESP_LOGI(TAG, "Action task started");

    while (1) {
        if (msg_queue_receive(queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_PWM) {
                pwm_set_duty(msg.data.pwm.duty_percent);
                ESP_LOGD(TAG, "PWM set to %d%%", msg.data.pwm.duty_percent);
            } else if (msg.type == MSG_TYPE_KEY) {
                if (msg.data.key.event == KEY_EVENT_DOUBLE_CLICK) {
                    pwm_high = !pwm_high;
                    uint8_t duty = pwm_high ? PWM_DUTY_HIGH : PWM_DUTY_LOW;
                    pwm_set_duty(duty);
                    ESP_LOGI(TAG, "Double click: PWM toggled to %d%%", duty);
                }
            } else {
                ESP_LOGW(TAG, "Received unknown message type: %d", msg.type);
            }
        }
    }
}

BaseType_t action_task_create(QueueHandle_t queue)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Cannot create action task: queue is NULL");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    BaseType_t result = xTaskCreate(
        action_task,
        "action_task",
        ACTION_TASK_STACK_SIZE,
        (void *)queue,
        ACTION_TASK_PRIORITY,
        NULL
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create action task");
    } else {
        ESP_LOGI(TAG, "Action task created successfully");
    }

    return result;
}
