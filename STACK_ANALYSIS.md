# 任务栈大小分析与优化建议

## 当前任务栈配置

| 任务名称 | 当前栈大小 | 位置 | 主要功能 | 风险评估 |
|---------|-----------|------|---------|---------|
| **Timer Service** | 2048 → 4096 | sdkconfig | FreeRTOS定时器回调 | ⚠️ **已修复** - 调用servo/MQTT/BT日志 |
| led_task | 2048 | led_task.c | LED控制 | ✅ 低风险 - 简单GPIO操作 |
| key_task | 2048 | key_task.c | 按键扫描+手势检测 | ✅ 低风险 - 状态机逻辑 |
| servo_task | 4096 | pwm_task.c | 舵机控制+MQTT发布 | ✅ 合理 - 调用MQTT API |
| queue_monitor | 2048 | queue_monitor_task.c | 队列健康监控 | ✅ 低风险 - 简单检查 |
| watchdog | 2048 | watchdog_task.c | 看门狗喂狗 | ✅ 低风险 - 简单API调用 |
| sys_monitor | 3072 | system_monitor_task.c | 系统资源监控 | ⚠️ **需检查** - 可能有日志输出 |
| wifi_msg_task | 2048 → 4096 | wifi_manager.c | WiFi命令处理 | ⚠️ **已修复** - 调用WiFi API |
| smartconfig_task | 4096 | wifi_manager.c | SmartConfig配网 | ✅ 合理 - WiFi配网操作 |
| led_status_task | 2048 | wifi_manager.c | WiFi状态LED闪烁 | ✅ 低风险 - 简单消息发送 |
| mqtt_start | 2048 | main.c | MQTT启动等待 | ⚠️ **需增加** - 调用MQTT启动 |
| NimBLE Host | (系统) | bt_spp.c | BLE协议栈 | ℹ️ 系统管理 |

## 问题分析

### 1. Timer Service 栈溢出 (已修复)
**原因**: `bt_close_door_timer_callback` 调用链：
- `servo_set_angle()` - 硬件控制
- `bt_spp_log()` - 256字节缓冲区
- `ha_mqtt_publish_door_state()` - MQTT操作

**修复**: 2048 → 4096 字节

### 2. wifi_msg_task 栈不足 (已修复)
**原因**: `wifi_manager_clear_credentials()` 调用：
- `esp_wifi_disconnect()`
- `esp_wifi_restore()`
- `esp_wifi_set_mode()`
- `esp_wifi_start()`
- 多个日志和BT日志输出

**修复**: 2048 → 4096 字节

### 3. mqtt_start_task 栈可能不足
**原因**: 调用 `ha_mqtt_start()` 启动MQTT客户端
**建议**: 2048 → 3072 字节

### 4. system_monitor_task 需要验证
**当前**: 3072 字节
**需要检查**: 是否有大量日志输出或复杂计算

## 栈溢出的常见原因

1. **大型局部变量/缓冲区** (如 256 字节的字符串缓冲区)
2. **深层函数调用链** (每层调用都消耗栈空间)
3. **日志输出** (ESP_LOG 宏会使用栈空间)
4. **递归调用** (应避免)
5. **中断/回调中的操作** (定时器回调在 Timer Service 任务中执行)

## 推荐的栈大小标准

- **简单任务** (GPIO、状态机): 2048 字节
- **中等任务** (消息处理、简单API): 3072 字节
- **复杂任务** (网络、MQTT、WiFi): 4096 字节
- **协议栈任务** (BLE、TCP/IP): 4096-8192 字节

## 调试工具

### 1. 栈使用监控
```c
UBaseType_t stack_left = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Stack high water mark: %d bytes", stack_left * sizeof(StackType_t));
```

### 2. 启用栈溢出检测
在 `sdkconfig` 中:
```
CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY=y
```

### 3. 运行时监控
在 system_monitor_task 中添加所有任务的栈监控。
