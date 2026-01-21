# 系统稳定性改进方案

## 已实现的改进

### 1. ✅ 队列监控任务 (Queue Monitor)
**文件**: `components/task/queue_monitor_task.c`

**功能**:
- 每 5 秒检查所有消息队列状态
- 连续 3 次检测到队列满才触发重启（避免误判）
- 队列恢复正常时自动重置计数器

**配置参数**:
```c
#define QUEUE_CHECK_INTERVAL_MS       5000   // 检查间隔
#define QUEUE_FULL_THRESHOLD_COUNT    3      // 触发重启的连续次数
```

---

### 2. ✅ 看门狗任务 (Watchdog)
**文件**: `components/task/watchdog_task.c`

**功能**:
- 使用 ESP32 硬件看门狗定时器
- 每 10 秒喂狗一次
- 30 秒内未喂狗则触发系统重启
- 防止系统完全卡死无响应

**配置参数**:
```c
#define WATCHDOG_TIMEOUT_S       30      // 看门狗超时时间
#define WATCHDOG_FEED_INTERVAL_MS 10000  // 喂狗间隔
```

---

### 3. ✅ 系统健康监控 (System Monitor)
**文件**: `components/task/system_monitor_task.c`

**功能**:
- 每 60 秒检查一次系统状态
- 监控堆内存使用情况
- 监控内部 RAM 使用
- 记录系统运行时间
- 可选：打印任务列表（调试用）

**监控指标**:
- 当前可用堆内存
- 历史最小可用堆内存
- 内部 RAM 可用量
- 系统运行时间

**告警阈值**:
```c
#define MIN_FREE_HEAP_BYTES  20480  // 最小可用堆内存：20KB
```

---

### 4. ✅ 队列状态日志增强
**文件**: `main/msg_queue.c`

**功能**:
- 发送消息前记录队列状态
- 显示队列中消息数量和可用空间
- 帮助诊断队列满的原因

---

### 5. ✅ PWM 任务调试日志
**文件**: `components/task/pwm_task.c`

**功能**:
- 记录任务等待和接收消息的状态
- 增加堆栈大小（2048 → 4096）防止堆栈溢出

---

### 6. ✅ 蓝牙远程重启命令
**文件**: `main/bt_spp.c`

**功能**:
- 通过蓝牙发送 "RESTART" 命令远程重启设备
- 用于远程维护和故障恢复

---

## 建议的进一步改进

### 7. ⚠️ MQTT 自动重连机制
**当前问题**: MQTT 断开后不会自动重连

**建议方案**:
```c
// 在 ha_mqtt.c 的 MQTT_EVENT_DISCONNECTED 事件中添加
static int mqtt_reconnect_count = 0;
#define MAX_MQTT_RECONNECT 5

case MQTT_EVENT_DISCONNECTED:
    mqtt_reconnect_count++;
    if (mqtt_reconnect_count < MAX_MQTT_RECONNECT) {
        ESP_LOGI(TAG, "MQTT disconnected, reconnecting... (%d/%d)", 
                 mqtt_reconnect_count, MAX_MQTT_RECONNECT);
        vTaskDelay(pdMS_TO_TICKS(5000));  // 延迟5秒
        esp_mqtt_client_reconnect(s_mqtt_client);
    } else {
        ESP_LOGE(TAG, "MQTT reconnect failed after %d attempts, restarting...", 
                 MAX_MQTT_RECONNECT);
        esp_restart();
    }
    break;

case MQTT_EVENT_CONNECTED:
    mqtt_reconnect_count = 0;  // 重置计数器
    break;
```

---

### 8. ⚠️ WiFi 断线恢复优化
**当前问题**: WiFi 重试 3 次后直接重启，可能过于激进

**建议方案**:
```c
// 在 wifi_manager.c 中
#define MAX_RETRY_COUNT 5
#define RETRY_DELAY_MS 5000

// 重试失败后先尝试 SmartConfig，再考虑重启
if (s_retry_count >= MAX_RETRY_COUNT) {
    ESP_LOGW(TAG, "WiFi connection failed, trying SmartConfig...");
    s_has_saved_credentials = false;
    if (s_smartconfig_task_handle == NULL) {
        xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, 
                    &s_smartconfig_task_handle);
    }
}
```

---

### 9. ⚠️ 舵机控制非阻塞化
**当前问题**: `servo_set_angle()` 阻塞 PWM 任务 560ms

**建议方案 A - 使用定时器**:
```c
// 使用 FreeRTOS 软件定时器逐步移动舵机
static TimerHandle_t s_servo_move_timer = NULL;

static void servo_move_timer_callback(TimerHandle_t xTimer) {
    // 每次移动一小步
    if (s_current_angle != s_target_angle) {
        // 移动逻辑
        servo_set_angle_direct(s_current_angle);
    } else {
        xTimerStop(s_servo_move_timer, 0);
    }
}
```

**建议方案 B - 去掉平滑移动**:
```c
// 直接跳转到目标角度
esp_err_t servo_set_angle(uint8_t target_angle) {
    s_current_angle = target_angle;
    return servo_set_angle_direct(target_angle);
}
```

---

### 10. ⚠️ 错误重试机制
**建议**: 对关键操作失败进行重试

```c
// 示例：MQTT 发布重试
esp_err_t ha_mqtt_publish_door_state_with_retry(bool is_on) {
    int retry = 0;
    esp_err_t ret;
    
    while (retry < 3) {
        ret = ha_mqtt_publish_door_state(is_on);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        retry++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    return ret;
}
```

---

## 系统架构总览

```
┌─────────────────────────────────────────────────────────┐
│                    应用层任务                              │
│  LED Task │ PWM Task │ Key Task │ WiFi Msg Task          │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│                    消息队列层                              │
│  QUEUE_LED │ QUEUE_PWM │ QUEUE_WIFI │ QUEUE_MQTT        │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│                    监控层                                  │
│  Queue Monitor │ System Monitor │ Watchdog              │
│  (队列监控)     │  (系统监控)     │  (看门狗)              │
└─────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────┐
│                    硬件层                                  │
│  WiFi │ Bluetooth │ MQTT │ GPIO │ PWM                   │
└─────────────────────────────────────────────────────────┘
```

---

## 监控任务优先级

| 任务 | 优先级 | 说明 |
|------|--------|------|
| Watchdog Task | 1 | 最低优先级，确保不影响业务 |
| System Monitor | 1 | 最低优先级，定期检查 |
| Queue Monitor | 2 | 较低优先级，定期检查 |
| Key Task | 4 | 中等优先级，响应用户输入 |
| LED/PWM Task | 5 | 较高优先级，执行控制 |

---

## 配置建议

### sdkconfig 配置
```ini
# 启用任务列表功能（用于调试）
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y

# 启用堆栈溢出检测
CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y

# 启用任务看门狗
CONFIG_ESP_TASK_WDT=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=30
```

---

## 故障恢复流程

```
系统异常
    ↓
队列监控检测到队列满（连续3次）
    ↓
记录日志 → 等待2秒 → 重启系统
    ↓
系统重新初始化
    ↓
恢复正常运行
```

```
系统完全卡死
    ↓
看门狗30秒未被喂狗
    ↓
硬件看门狗触发 panic
    ↓
系统强制重启
    ↓
恢复正常运行
```

---

## 测试建议

1. **队列满测试**: 快速连续发送大量 MQTT 命令，观察队列监控是否正常工作
2. **内存泄漏测试**: 长时间运行（24小时+），观察内存使用趋势
3. **WiFi 断线测试**: 断开 WiFi，观察重连机制
4. **MQTT 断线测试**: 断开 MQTT broker，观察重连机制
5. **看门狗测试**: 人为制造死循环，验证看门狗是否触发重启

---

## 日志级别建议

**生产环境**:
```c
esp_log_level_set("*", ESP_LOG_INFO);
esp_log_level_set("queue_monitor", ESP_LOG_WARN);
esp_log_level_set("sys_monitor", ESP_LOG_INFO);
esp_log_level_set("watchdog", ESP_LOG_INFO);
```

**调试环境**:
```c
esp_log_level_set("*", ESP_LOG_DEBUG);
```

---

## 总结

通过以上改进，系统稳定性得到显著提升：

✅ **自动故障恢复**: 队列满、系统卡死都能自动重启
✅ **主动监控**: 实时监控系统健康状态
✅ **远程维护**: 支持蓝牙远程重启
✅ **详细日志**: 便于问题诊断和追踪

建议优先实现标记为 ⚠️ 的改进项，进一步提升系统可靠性。
