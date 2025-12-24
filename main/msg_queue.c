/**
 * @file msg_queue.c
 * @brief Message Queue System implementation
 */

#include "msg_queue.h"
#include "board.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "msg_queue";

/* LED Task configuration */
#define LED_TASK_STACK_SIZE 2048
#define LED_TASK_PRIORITY   5

/* Key Task configuration */
#define KEY_TASK_STACK_SIZE 2048
#define KEY_TASK_PRIORITY   4

/* PWM Task configuration */
#define PWM_TASK_STACK_SIZE 2048
#define PWM_TASK_PRIORITY   5
#define KEY_SCAN_INTERVAL_MS 10  /* Key scan interval in milliseconds */

/* Key gesture detection timing parameters (in milliseconds) */
#define LONG_PRESS_TIME_MS      1000  /* Long press detection time */
#define DOUBLE_CLICK_INTERVAL_MS 300  /* Double click interval time */
#define CLICK_MAX_TIME_MS        500  /* Maximum press time for single click */

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

bool msg_type_is_valid(msg_type_t type)
{
    return (type > MSG_TYPE_NONE && type < MSG_TYPE_MAX);
}

/**
 * @brief LED task function
 * 
 * Continuously receives messages from the queue and processes LED messages
 * by setting the corresponding GPIO level. Also handles key single click
 * events to toggle the LED state.
 * 
 * @param pvParameters Queue handle passed as task parameter
 */
static void led_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    msg_t msg;
    static uint8_t led_state = 0;  /* Track current LED state for toggle */

    ESP_LOGI(TAG, "LED task started");

    while (1) {
        /* Block indefinitely waiting for messages */
        if (msg_queue_receive(queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_LED) {
                /* Process LED message - set GPIO level */
                gpio_set_level(msg.data.led.gpio_num, msg.data.led.state);
                ESP_LOGD(TAG, "LED GPIO %d set to %d", 
                         msg.data.led.gpio_num, msg.data.led.state);
            } else if (msg.type == MSG_TYPE_KEY) {
                /* Process key message - only handle single click for LED toggle */
                if (msg.data.key.event == KEY_EVENT_SINGLE_CLICK) {
                    /* Toggle LED on single click */
                    led_state = !led_state;
                    gpio_set_level(LED_RED_GPIO, led_state);
                    ESP_LOGI(TAG, "Single click: LED toggled to %d", led_state);
                }
                /* Double click and long press are handled by PWM task */
            } else {
                /* Ignore other messages */
                ESP_LOGW(TAG, "LED task received unknown message type: %d", msg.type);
            }
        }
    }
}

BaseType_t led_task_create(QueueHandle_t queue)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Cannot create LED task: queue is NULL");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    BaseType_t result = xTaskCreate(
        led_task,               /* Task function */
        "led_task",             /* Task name */
        LED_TASK_STACK_SIZE,    /* Stack size */
        (void *)queue,          /* Task parameter (queue handle) */
        LED_TASK_PRIORITY,      /* Task priority */
        NULL                    /* Task handle (not needed) */
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED task");
    } else {
        ESP_LOGI(TAG, "LED task created successfully");
    }

    return result;
}

/**
 * @brief Key task parameters structure
 */
typedef struct {
    QueueHandle_t queue;      /**< Queue handle for sending key messages (LED queue) */
    QueueHandle_t pwm_queue;  /**< Queue handle for sending PWM messages */
    uint8_t gpio_num;         /**< Key GPIO number to scan */
} key_task_params_t;

/* Static storage for key task parameters */
static key_task_params_t s_key_task_params;

/**
 * @brief Key task function with gesture detection
 * 
 * Implements a state machine to detect single click, double click, and long press gestures.
 * - Single click: Press and release within 500ms, no subsequent press within 300ms
 * - Double click: Two clicks with interval less than 300ms
 * - Long press: Key held for more than 1000ms
 * 
 * Single click events are sent to LED queue, double click events are sent to PWM queue.
 * 
 * @param pvParameters Pointer to key_task_params_t structure
 */
static void key_task(void *pvParameters)
{
    key_task_params_t *params = (key_task_params_t *)pvParameters;
    uint8_t gpio_num = params->gpio_num;
    
    key_state_t state = KEY_STATE_IDLE;
    uint8_t last_key_level = 1;  /* Assume key is released initially (active low) */
    uint8_t current_key_level;
    TickType_t press_start_tick = 0;      /* Tick when key was pressed */
    TickType_t release_tick = 0;          /* Tick when key was released */
    bool long_press_sent = false;         /* Flag to prevent multiple long press events */

    ESP_LOGI(TAG, "Key task started with gesture detection, scanning GPIO %d", gpio_num);

    while (1) {
        /* Read current key state */
        current_key_level = gpio_get_level(gpio_num);
        TickType_t current_tick = xTaskGetTickCount();

        switch (state) {
            case KEY_STATE_IDLE:
                /* Waiting for key press */
                if (current_key_level == 0 && last_key_level == 1) {
                    /* Key pressed (active low) */
                    press_start_tick = current_tick;
                    long_press_sent = false;
                    state = KEY_STATE_PRESSED;
                    ESP_LOGD(TAG, "Key pressed, entering PRESSED state");
                }
                break;

            case KEY_STATE_PRESSED:
                /* Key is pressed, waiting for release or long press timeout */
                if (current_key_level == 1 && last_key_level == 0) {
                    /* Key released */
                    TickType_t press_duration = current_tick - press_start_tick;
                    
                    if (press_duration >= pdMS_TO_TICKS(LONG_PRESS_TIME_MS)) {
                        /* Long press already detected and sent, just go back to idle */
                        state = KEY_STATE_IDLE;
                        ESP_LOGD(TAG, "Key released after long press, returning to IDLE");
                    } else if (press_duration <= pdMS_TO_TICKS(CLICK_MAX_TIME_MS)) {
                        /* Short press - could be single or double click */
                        release_tick = current_tick;
                        state = KEY_STATE_WAIT_SECOND;
                        ESP_LOGD(TAG, "Short press detected, entering WAIT_SECOND state");
                    } else {
                        /* Press duration between 500ms and 1000ms - treat as single click */
                        release_tick = current_tick;
                        state = KEY_STATE_WAIT_SECOND;
                        ESP_LOGD(TAG, "Medium press detected, entering WAIT_SECOND state");
                    }
                } else if (current_key_level == 0) {
                    /* Key still pressed - check for long press */
                    TickType_t press_duration = current_tick - press_start_tick;
                    
                    if (press_duration >= pdMS_TO_TICKS(LONG_PRESS_TIME_MS) && !long_press_sent) {
                        /* Long press detected - send to LED queue */
                        msg_send_key(params->queue, gpio_num, KEY_EVENT_LONG_PRESS);
                        long_press_sent = true;
                        ESP_LOGI(TAG, "Long press detected on GPIO %d", gpio_num);
                    }
                }
                break;

            case KEY_STATE_WAIT_SECOND:
                /* First click released, waiting for second click or timeout */
                if (current_key_level == 0 && last_key_level == 1) {
                    /* Second key press detected */
                    TickType_t interval = current_tick - release_tick;
                    
                    if (interval <= pdMS_TO_TICKS(DOUBLE_CLICK_INTERVAL_MS)) {
                        /* Within double click interval */
                        press_start_tick = current_tick;
                        state = KEY_STATE_DOUBLE_PRESSED;
                        ESP_LOGD(TAG, "Second press detected, entering DOUBLE_PRESSED state");
                    } else {
                        /* Too long since first release - this is a new single click sequence */
                        /* First, send single click to LED queue */
                        msg_send_key(params->queue, gpio_num, KEY_EVENT_SINGLE_CLICK);
                        ESP_LOGI(TAG, "Single click detected on GPIO %d (timeout before second press)", gpio_num);
                        
                        /* Start new press sequence */
                        press_start_tick = current_tick;
                        long_press_sent = false;
                        state = KEY_STATE_PRESSED;
                    }
                } else if (current_key_level == 1) {
                    /* Key still released - check for timeout */
                    TickType_t wait_duration = current_tick - release_tick;
                    
                    if (wait_duration > pdMS_TO_TICKS(DOUBLE_CLICK_INTERVAL_MS)) {
                        /* Timeout - this was a single click, send to LED queue */
                        msg_send_key(params->queue, gpio_num, KEY_EVENT_SINGLE_CLICK);
                        state = KEY_STATE_IDLE;
                        ESP_LOGI(TAG, "Single click detected on GPIO %d", gpio_num);
                    }
                }
                break;

            case KEY_STATE_DOUBLE_PRESSED:
                /* Second key pressed, waiting for release to confirm double click */
                if (current_key_level == 1 && last_key_level == 0) {
                    /* Second key released - double click confirmed, send to PWM queue */
                    if (params->pwm_queue != NULL) {
                        msg_send_key(params->pwm_queue, gpio_num, KEY_EVENT_DOUBLE_CLICK);
                    }
                    state = KEY_STATE_IDLE;
                    ESP_LOGI(TAG, "Double click detected on GPIO %d", gpio_num);
                }
                break;

            default:
                /* Unknown state - reset to idle */
                state = KEY_STATE_IDLE;
                ESP_LOGW(TAG, "Unknown key state, resetting to IDLE");
                break;
        }

        last_key_level = current_key_level;

        /* Wait before next scan */
        vTaskDelay(pdMS_TO_TICKS(KEY_SCAN_INTERVAL_MS));
    }
}

BaseType_t key_task_create(QueueHandle_t queue, uint8_t gpio_num)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Cannot create key task: queue is NULL");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    /* Store parameters in static storage */
    s_key_task_params.queue = queue;
    s_key_task_params.pwm_queue = NULL;  /* Will be set later via key_task_set_pwm_queue */
    s_key_task_params.gpio_num = gpio_num;

    BaseType_t result = xTaskCreate(
        key_task,               /* Task function */
        "key_task",             /* Task name */
        KEY_TASK_STACK_SIZE,    /* Stack size */
        (void *)&s_key_task_params,  /* Task parameter */
        KEY_TASK_PRIORITY,      /* Task priority */
        NULL                    /* Task handle (not needed) */
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create key task");
    } else {
        ESP_LOGI(TAG, "Key task created successfully for GPIO %d", gpio_num);
    }

    return result;
}

void key_task_set_pwm_queue(QueueHandle_t queue)
{
    s_key_task_params.pwm_queue = queue;
    ESP_LOGI(TAG, "PWM queue set for key task");
}

/**
 * @brief PWM task function
 * 
 * Continuously receives messages from the PWM queue and processes PWM messages
 * by setting the duty cycle. Also handles key double click events to toggle PWM.
 * 
 * @param pvParameters Queue handle passed as task parameter
 */
static void pwm_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    msg_t msg;
    static uint8_t pwm_high = 0;   /* PWM state: 0=low(20%), 1=high(80%) */

    ESP_LOGI(TAG, "PWM task started");

    while (1) {
        /* Block indefinitely waiting for messages */
        if (msg_queue_receive(queue, &msg, portMAX_DELAY)) {
            if (msg.type == MSG_TYPE_PWM) {
                /* Process PWM message - set duty cycle */
                pwm_set_duty(msg.data.pwm.duty_percent);
                ESP_LOGD(TAG, "PWM GPIO %d set to %d%%", 
                         msg.data.pwm.gpio_num, msg.data.pwm.duty_percent);
            } else if (msg.type == MSG_TYPE_KEY) {
                /* Process key message - handle double click for PWM toggle */
                if (msg.data.key.event == KEY_EVENT_DOUBLE_CLICK) {
                    /* Toggle PWM duty cycle on double click */
                    pwm_high = !pwm_high;
                    uint8_t duty = pwm_high ? PWM_DUTY_HIGH : PWM_DUTY_LOW;
                    pwm_set_duty(duty);
                    ESP_LOGI(TAG, "Double click: PWM toggled to %d%%", duty);
                }
            } else {
                /* Ignore other messages */
                ESP_LOGW(TAG, "PWM task received unknown message type: %d", msg.type);
            }
        }
    }
}

BaseType_t pwm_task_create(QueueHandle_t queue)
{
    if (queue == NULL) {
        ESP_LOGE(TAG, "Cannot create PWM task: queue is NULL");
        return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }

    BaseType_t result = xTaskCreate(
        pwm_task,               /* Task function */
        "pwm_task",             /* Task name */
        PWM_TASK_STACK_SIZE,    /* Stack size */
        (void *)queue,          /* Task parameter (queue handle) */
        PWM_TASK_PRIORITY,      /* Task priority */
        NULL                    /* Task handle (not needed) */
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create PWM task");
    } else {
        ESP_LOGI(TAG, "PWM task created successfully");
    }

    return result;
}
