# ESP32 WebRTC Client with MQTT Signaling

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.4.1-blue)](https://github.com/espressif/esp-idf)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

一个基于ESP32的完整WebRTC客户端实现，支持MQTT信令服务器进行SDP交换和ICE候选协商。

## 🎯 项目特性

- ✅ **完整的WebRTC支持**：音频、视频、数据通道
- ✅ **STUN服务器连通性测试**：自动验证网络连接
- ✅ **MQTT信令集成**：支持分布式WebRTC连接
- ✅ **WiFi自动连接**：STA模式，支持WPA2-PSK
- ✅ **详细日志输出**：便于调试和监控
- ✅ **模块化设计**：易于扩展和定制

## 📋 系统要求

### 硬件要求
- ESP32 / ESP32-S3 开发板
- 2MB+ Flash存储
- WiFi连接能力

### 软件要求
- ESP-IDF v5.4.1 或更高版本
- MQTT服务器（用于信令）
- STUN服务器（用于NAT穿越）

## 🚀 快速开始

### 1. 环境准备

```bash
# 安装ESP-IDF
git clone https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
. ./export.sh

# 克隆项目
git clone <your-repo-url>
cd esp-rtc
```

### 2. 基础配置

编辑 `esp-rtc.cpp`，修改WiFi和STUN服务器配置：

```cpp
webrtc_client_config_t config = {
    .wifi_ssid = "Your_WiFi_SSID",           // 修改为您的WiFi名称
    .wifi_password = "Your_WiFi_Password",    // 修改为您的WiFi密码
    .stun_server = "stun.freeswitch.org",     // STUN服务器地址
    .stun_port = 3478,                        // STUN服务器端口
    .enable_audio = true,                     // 启用音频
    .enable_video = false,                    // 禁用视频（可选）
    .enable_data_channel = true               // 启用数据通道
};
```

### 3. 编译和烧录

```bash
# 使用提供的构建脚本（推荐）
chmod +x build.sh
./build.sh

# 或者手动构建
idf.py build
idf.py flash monitor
```

## 🔄 MQTT信令集成

当前项目已实现WebRTC客户端基础功能，**需要集成MQTT信令服务器以实现完整的WebRTC连接**。

### 架构图

```
┌─────────────┐    MQTT     ┌─────────────────┐    WebSocket    ┌──────────────┐
│   ESP32     │◄──────────►│  MQTT Broker    │◄──────────────►│   Web Client │
│  WebRTC     │   Signaling │  (Signaling)    │    Signaling   │   (Browser)  │
│  Client     │             │                 │                │              │
└─────────────┘             └─────────────────┘                └──────────────┘
       │                                                               │
       └───────────────────── P2P WebRTC Connection ──────────────────┘
                                (through STUN/TURN)
```

### MQTT主题设计

| 主题 | 方向 | 描述 | 消息格式 |
|------|------|------|----------|
| `webrtc/device/{device_id}/offer` | ESP32 → 服务器 | 发送SDP Offer | JSON |
| `webrtc/device/{device_id}/answer` | 服务器 → ESP32 | 接收SDP Answer | JSON |
| `webrtc/device/{device_id}/ice/local` | ESP32 → 服务器 | 发送本地ICE候选 | JSON |
| `webrtc/device/{device_id}/ice/remote` | 服务器 → ESP32 | 接收远程ICE候选 | JSON |
| `webrtc/device/{device_id}/status` | ESP32 ↔ 服务器 | 设备状态更新 | JSON |

### 消息格式规范

#### 1. SDP Offer消息
```json
{
  "type": "offer",
  "device_id": "esp32_001",
  "session_id": "session_12345",
  "sdp": "v=0\r\no=- 1234567890 2 IN IP4 127.0.0.1\r\n...",
  "timestamp": 1640995200,
  "capabilities": {
    "audio": true,
    "video": false,
    "data_channel": true
  }
}
```

#### 2. SDP Answer消息
```json
{
  "type": "answer",
  "device_id": "esp32_001", 
  "session_id": "session_12345",
  "sdp": "v=0\r\no=- 9876543210 2 IN IP4 192.168.1.100\r\n...",
  "timestamp": 1640995201
}
```

#### 3. ICE候选消息
```json
{
  "type": "ice_candidate",
  "device_id": "esp32_001",
  "session_id": "session_12345", 
  "candidate": "candidate:1 1 UDP 2122252543 192.168.1.100 12345 typ host",
  "sdp_m_line_index": 0,
  "sdp_mid": "0",
  "timestamp": 1640995202
}
```

#### 4. 设备状态消息
```json
{
  "type": "status",
  "device_id": "esp32_001",
  "status": "online|offline|connecting|connected",
  "ip_address": "192.168.1.100",
  "capabilities": {
    "audio": true,
    "video": false, 
    "data_channel": true
  },
  "timestamp": 1640995203
}
```

## 🛠️ 实施指南

### 第一阶段：MQTT客户端集成

#### 1. 添加MQTT依赖

编辑 `main/CMakeLists.txt`，添加MQTT组件：

```cmake
idf_component_register(
    SRCS "esp-rtc.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        sdp_ice
        esp_wifi
        esp_event
        nvs_flash
        driver
        freertos
        lwip
        esp_log
        mqtt        # 添加MQTT支持
)
```

#### 2. 创建MQTT信令组件


### 第二阶段：WebRTC与MQTT集成

#### 1. 修改WebRTC客户端

在 `components/sdp_ice/webrtc_client.cpp` 中集成MQTT信令：

```cpp
#include "mqtt_signaling.h"

// MQTT信令回调函数
static void on_mqtt_answer_received(const char *sdp, void *user_data)
{
    ESP_LOGI(TAG, "收到SDP Answer，开始设置远程描述");
    webrtc_client_set_answer(sdp);
}

static void on_mqtt_ice_received(const char *candidate, void *user_data)
{
    ESP_LOGI(TAG, "收到远程ICE候选: %s", candidate);
    webrtc_client_add_ice_candidate(candidate);
}

// 在webrtc_client_start中初始化MQTT
esp_err_t webrtc_client_start(void)
{
    // ... 现有代码 ...
    
    // 初始化MQTT信令
    mqtt_signaling_config_t mqtt_config = {
        .broker_uri = "mqtt://your-mqtt-broker.com:1883",
        .username = "esp32_user",
        .password = "esp32_password", 
        .device_id = "esp32_001",  // 可以使用MAC地址
        .client_id = "esp32_webrtc_001",
        .keepalive = 60,
        .qos = 1
    };
    
    ESP_ERROR_CHECK(mqtt_signaling_init(&mqtt_config));
    ESP_ERROR_CHECK(mqtt_signaling_set_callbacks(
        NULL,  // 不需要处理收到的offer
        on_mqtt_answer_received,
        on_mqtt_ice_received,
        NULL
    ));
    ESP_ERROR_CHECK(mqtt_signaling_start());
    
    // ... 现有代码 ...
}
```

#### 2. 修改SDP和ICE回调

```cpp
// 修改SDP Offer回调
static void on_sdp_offer_created(const char *sdp_offer, void *user_data)
{
    ESP_LOGI(TAG, "SDP Offer已创建，发送到MQTT服务器");
    
    // 发送到MQTT服务器
    esp_err_t ret = mqtt_signaling_send_offer(sdp_offer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送SDP Offer失败: %s", esp_err_to_name(ret));
    }
}

// 修改ICE候选回调  
static void on_ice_candidate_received(const char *candidate, void *user_data)
{
    ESP_LOGI(TAG, "本地ICE候选生成: %s", candidate);
    
    // 发送到MQTT服务器
    esp_err_t ret = mqtt_signaling_send_ice_candidate(candidate);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "发送ICE候选失败: %s", esp_err_to_name(ret));
    }
}
```

### 第三阶段：完整连接流程

#### WebRTC连接时序图

```mermaid
sequenceDiagram
    participant ESP32
    participant MQTT
    participant Backend
    participant WebClient

    ESP32->>MQTT: 连接MQTT服务器
    ESP32->>MQTT: 订阅answer/ice主题
    ESP32->>MQTT: 发送设备状态(online)
    
    ESP32->>ESP32: 创建WebRTC连接
    ESP32->>ESP32: 生成SDP Offer
    ESP32->>MQTT: 发布SDP Offer
    
    MQTT->>Backend: 转发SDP Offer
    Backend->>WebClient: 发送SDP Offer
    
    WebClient->>WebClient: 创建Answer
    WebClient->>Backend: 发送SDP Answer
    Backend->>MQTT: 转发SDP Answer
    MQTT->>ESP32: 接收SDP Answer
    
    ESP32->>ESP32: 设置远程描述
    ESP32->>ESP32: 生成ICE候选
    ESP32->>MQTT: 发布ICE候选
    
    WebClient->>Backend: 发送ICE候选
    Backend->>MQTT: 转发ICE候选  
    MQTT->>ESP32: 接收ICE候选
    
    ESP32->>ESP32: 添加远程ICE候选
    ESP32<-->>WebClient: 建立P2P连接
```


**注意**：此README描述了完整的WebRTC+MQTT信令解决方案。当前项目已实现WebRTC客户端基础功能，MQTT信令集成需要按照上述指南实施。