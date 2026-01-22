# 蓝牙调试日志功能说明

## 概述

已为项目添加蓝牙SPP调试日志功能，可以通过蓝牙实时查看设备的重要状态信息。

## 主要改动

### 1. 蓝牙开门功能优化
- **直接控制**：蓝牙接收到 `OPEN` 命令后，直接调用 `servo_set_angle()` 控制舵机，不再使用消息队列
- **独立定时器**：蓝牙模块内部实现了自动关门定时器（2秒后自动关门）
- **响应更快**：减少了队列传递的延迟

### 2. 新增蓝牙日志函数

在 `bt_spp.h` 中新增：
```c
esp_err_t bt_spp_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
```

使用方法：
```c
bt_spp_log("[TAG] Your message: %d", value);
```

特点：
- 支持 printf 风格的格式化字符串
- 自动添加 `\r\n` 换行符
- 只在蓝牙连接且通知启用时发送
- 最大支持 256 字节消息

### 3. 各模块添加的调试日志

#### 蓝牙模块 (bt_spp.c)
- `[BT] Connected to ESP32-DoorLock` - 连接成功
- `[BT] Commands: OPEN, RESTART` - 可用命令提示
- `[BT] Opening door...` - 开门命令接收
- `[BT] Door opened (angle: XX)` - 开门成功
- `[BT] Auto close door` - 自动关门
- `[BT] ERROR: Failed to open door` - 开门失败
- `[BT] Restarting device...` - 设备重启

#### WiFi 模块 (wifi_manager.c)
- `[WiFi] Connected, IP: xxx.xxx.xxx.xxx` - WiFi 连接成功
- `[WiFi] Disconnected, retry X/3` - WiFi 断开重连
- `[WiFi] Connection failed, restarting...` - 连接失败重启
- `[WiFi] SmartConfig got credentials` - 收到配网信息
- `[WiFi] Connecting to: SSID` - 正在连接到指定 SSID
- `[WiFi] Clearing credentials...` - 清除凭据
- `[WiFi] Credentials cleared, restarting SmartConfig` - 清除成功

#### MQTT 模块 (ha_mqtt.c)
- `[MQTT] Connected to broker` - MQTT 连接成功
- `[MQTT] Disconnected from broker` - MQTT 断开
- `[MQTT] Door ON command received` - 收到开门命令
- `[MQTT] Door OFF command received` - 收到关门命令

#### 系统监控 (system_monitor_task.c)
- `[SYS] Heap: XXXXX bytes, Uptime: XX min` - 每 60 秒发送一次系统状态
- `[SYS] WARNING: Low memory! Free: XXXXX bytes` - 内存不足警告（< 20KB）

#### 按键任务 (key_task.c)
- `[KEY] Long press` - 长按检测
- `[KEY] Double click` - 双击检测

## 使用方法

1. **连接蓝牙**
   - 使用蓝牙串口工具（如 Serial Bluetooth Terminal）连接到 `ESP32-DoorLock`
   - 连接成功后会收到欢迎消息

2. **查看日志**
   - 连接后会自动接收各模块的调试信息
   - 系统状态每 60 秒自动发送一次

3. **发送命令**
   - `OPEN` - 开门（2秒后自动关门）
   - `RESTART` - 重启设备

## 注意事项

1. 日志只在蓝牙连接且通知启用时发送，不会影响未连接时的性能
2. 日志消息最大 256 字节，超长消息会被截断
3. 如果蓝牙未连接，`bt_spp_log()` 会静默失败（返回 `ESP_ERR_INVALID_STATE`）
4. 蓝牙开门和按键/MQTT开门现在是独立的路径，各自管理自己的定时器

## 调试建议

通过蓝牙日志可以实时监控：
- WiFi 连接状态和 IP 地址
- MQTT 连接状态
- 开门操作执行情况
- 系统内存使用情况
- 设备运行时间
- 按键操作事件

这对于现场调试和远程诊断非常有用。
