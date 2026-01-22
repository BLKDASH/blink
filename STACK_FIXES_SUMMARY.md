# 栈溢出问题修复总结

## 修复的问题

### 1. Timer Service 任务栈溢出 ✅
**文件**: `sdkconfig`
**修改**: `CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH: 2048 → 4096`
**原因**: 定时器回调 `bt_close_door_timer_callback` 调用了 servo、MQTT 和 BT 日志函数

### 2. wifi_msg_task 栈不足 ✅
**文件**: `main/wifi_manager.c`
**修改**: 栈大小 `2048 → 4096`
**原因**: `wifi_manager_clear_credentials()` 调用多个 WiFi API

### 3. mqtt_start_task 栈不足 ✅
**文件**: `main/main.c`
**修改**: 栈大小 `2048 → 3072`
**原因**: 调用 `ha_mqtt_start()` 启动 MQTT 客户端

### 4. 其他任务预防性增加 ✅
- **key_task**: `2048 → 3072` (调用回调函数，可能触发复杂操作)
- **queue_monitor**: `2048 → 3072` (日志输出较多)
- **watchdog**: `2048 → 3072` (预防性增加)
- **system_monitor**: `3072 → 4096` (新增栈监控功能，需要更多空间)

## 新增功能

### 1. 栈使用监控 📊
所有任务启动时都会打印栈大小信息：
```
ESP_LOGI(TAG, "Task started, stack size: %d bytes", STACK_SIZE * sizeof(StackType_t));
```

### 2. 系统监控增强 🔍
`system_monitor_task` 现在每 60 秒监控所有任务的栈使用情况：
- 打印每个任务的剩余栈空间
- 当剩余栈 < 512 字节时发出警告
- 通过蓝牙发送警告信息

监控的任务列表：
- led_task
- key_task
- servo_task
- queue_monitor
- watchdog
- sys_monitor
- wifi_msg_task
- mqtt_start
- Tmr Svc (FreeRTOS Timer Service)

### 3. 运行时诊断 🩺
在 `wifi_msg_task` 启动时打印栈水位线：
```c
ESP_LOGI(TAG, "WiFi message task started (stack: %d bytes)", uxTaskGetStackHighWaterMark(NULL));
```

## 最终栈配置

| 任务名称 | 栈大小 (字节) | 优先级 | 说明 |
|---------|--------------|--------|------|
| Timer Service | 4096 | 1 | FreeRTOS 定时器服务 |
| led_task | 2048 | 5 | LED 控制 |
| key_task | 3072 | 4 | 按键扫描 |
| servo_task | 4096 | 5 | 舵机控制 + MQTT |
| queue_monitor | 3072 | 2 | 队列监控 |
| watchdog | 3072 | 1 | 看门狗 |
| sys_monitor | 4096 | 1 | 系统监控 |
| wifi_msg_task | 4096 | 4 | WiFi 命令处理 |
| smartconfig_task | 4096 | 3 | SmartConfig 配网 |
| led_status_task | 2048 | 2 | WiFi 状态 LED |
| mqtt_start | 3072 | 3 | MQTT 启动 |

## 总内存使用

**任务栈总计**: 约 39 KB (39,936 字节)

这对于 ESP32-C6 (512KB SRAM) 来说是合理的。

## 调试建议

### 查看实时栈使用
系统监控任务每 60 秒会打印所有任务的栈使用情况。你也可以在任何任务中添加：
```c
UBaseType_t stack_left = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Stack remaining: %u bytes", stack_left * sizeof(StackType_t));
```

### 启用 FreeRTOS 栈溢出检测
在 `menuconfig` 中启用：
```
Component config → FreeRTOS → Kernel → 
  [*] Check for stack overflow (Method 2)
```

### 查看任务列表
如果启用了 `CONFIG_FREERTOS_USE_TRACE_FACILITY`，系统监控会打印完整的任务列表。

## 预防措施

1. **避免大型局部变量**: 使用静态变量或动态分配
2. **限制日志输出**: 特别是在中断和定时器回调中
3. **监控栈使用**: 定期检查 `uxTaskGetStackHighWaterMark()`
4. **测试极端情况**: 确保在最坏情况下也不会溢出

## 下次崩溃时的诊断步骤

1. 查看崩溃日志中的任务名称
2. 检查 Stack pointer 和 Stack bounds
3. 查看系统监控日志中该任务的栈使用情况
4. 如果栈剩余 < 1KB，考虑增加栈大小
5. 分析该任务的调用链，找出栈消耗大的函数
