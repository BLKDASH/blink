#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "board.h"
#include "msg_queue.h"
#include "led_task.h"
#include "key_task.h"
#include "pwm_task.h"
#include "wifi_manager.h"

static const char *TAG = "main";

static QueueHandle_t s_led_queue = NULL;
static QueueHandle_t s_pwm_queue = NULL;
static QueueHandle_t s_wifi_queue = NULL;

#define MSG_QUEUE_LEN 10


#define LED_BLINK_INTERVAL_MS 3000
#define LED_TASK_STACK_SIZE 2048
#define LED_TASK_PRIORITY 2

static void led_blink_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    uint8_t led_state = 0;
    
    while (1) {
        led_state = !led_state;
        msg_send_led(queue, LED_RED_GPIO, led_state);
        vTaskDelay(pdMS_TO_TICKS(LED_BLINK_INTERVAL_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Hello ESP32-C6!");
    
    configure_led();
    configure_key();
    
    if (configure_pwm() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PWM");
    }
    
    /* 初始化WiFi管理器 (Requirement 4.2) */
    if (wifi_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
        /* WiFi初始化失败不阻止其他功能运行 */
    }
    
    s_led_queue = msg_queue_init(MSG_QUEUE_LEN);
    if (s_led_queue == NULL) {
        ESP_LOGE(TAG, "Failed to initialize led queue");
        return;
    }
    
    s_pwm_queue = msg_queue_init(MSG_QUEUE_LEN);
    if (s_pwm_queue == NULL) {
        ESP_LOGE(TAG, "Failed to initialize pwm queue");
        return;
    }
    
    /* 创建WiFi消息队列 */
    s_wifi_queue = msg_queue_init(MSG_QUEUE_LEN);
    if (s_wifi_queue == NULL) {
        ESP_LOGE(TAG, "Failed to initialize wifi queue");
        return;
    }
    
    /* 将WiFi队列传递给wifi_manager */
    wifi_manager_set_queue(s_wifi_queue);



    if (led_task_create(s_led_queue) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create led task");
        return;
    }
    if (pwm_task_create(s_pwm_queue, s_wifi_queue) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create pwm task");
        return;
    }
    if (key_task_create(s_led_queue, s_pwm_queue, s_wifi_queue, KEY_GPIO) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create key task");
        return;
    }
    

    // 创建 LED 闪烁任务
    if (xTaskCreate(led_blink_task,"led_blink",LED_TASK_STACK_SIZE,(void *)s_led_queue,LED_TASK_PRIORITY,NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED blink task");
        return;
    }
    
    ESP_LOGI(TAG, "System initialized");
    
    // 删除 app_main 任务，释放栈内存
    vTaskDelete(NULL);
}
