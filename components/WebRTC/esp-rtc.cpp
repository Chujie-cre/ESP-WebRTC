#include <stdio.h>
#include "esp_log.h"
#include "webrtc_client.hpp"

// 全局日志标签
static const char *TAG = "Main";

// 状态回调函数
static void on_webrtc_state_change(webrtc_client_state_t state, void *user_data)
{
    ESP_LOGI(TAG, "WebRTC状态变化: %d", state);
    
    switch (state) {
        case WEBRTC_CLIENT_STATE_IDLE:
            ESP_LOGI(TAG, "状态: 空闲");
            break;
        case WEBRTC_CLIENT_STATE_INITIALIZING:
            ESP_LOGI(TAG, "状态: 初始化中");
            break;
        case WEBRTC_CLIENT_STATE_WIFI_CONNECTING:
            ESP_LOGI(TAG, "状态: WiFi连接中");
            break;
        case WEBRTC_CLIENT_STATE_WIFI_CONNECTED:
            ESP_LOGI(TAG, "状态: WiFi已连接");
            break;
        case WEBRTC_CLIENT_STATE_PEER_CREATING:
            ESP_LOGI(TAG, "状态: 创建Peer连接中");
            break;
        case WEBRTC_CLIENT_STATE_PEER_CREATED:
            ESP_LOGI(TAG, "状态: Peer连接已创建");
            break;
        case WEBRTC_CLIENT_STATE_OFFER_CREATED:
            ESP_LOGI(TAG, "状态: Offer已创建");
            break;
        case WEBRTC_CLIENT_STATE_ANSWER_RECEIVED:
            ESP_LOGI(TAG, "状态: 收到Answer");
            break;
        case WEBRTC_CLIENT_STATE_CONNECTING:
            ESP_LOGI(TAG, "状态: 连接中");
            break;
        case WEBRTC_CLIENT_STATE_CONNECTED:
            ESP_LOGI(TAG, "状态: 已连接");
            break;
        case WEBRTC_CLIENT_STATE_DISCONNECTED:
            ESP_LOGI(TAG, "状态: 已断开");
            break;
        case WEBRTC_CLIENT_STATE_ERROR:
            ESP_LOGI(TAG, "状态: 错误");
            break;
        default:
            ESP_LOGI(TAG, "状态: 未知状态 %d", state);
            break;
    }
}

// 音频数据回调函数
static void on_audio_data(const uint8_t *data, size_t size, void *user_data)
{
    ESP_LOGI(TAG, "收到音频数据，大小: %d 字节", size);
    // 这里可以处理音频数据，比如播放或转发
}

// 视频数据回调函数
static void on_video_data(const uint8_t *data, size_t size, void *user_data)
{
    ESP_LOGI(TAG, "收到视频数据，大小: %d 字节", size);
    // 这里可以处理视频数据，比如显示或转发
}

// 数据通道回调函数
static void on_data_channel_data(const uint8_t *data, size_t size, void *user_data)
{
    ESP_LOGI(TAG, "收到数据通道数据，大小: %d 字节", size);
    
    // 将接收到的数据转换为字符串并打印
    if (size > 0 && data != NULL) {
        char *str_data = (char *)malloc(size + 1);
        if (str_data) {
            memcpy(str_data, data, size);
            str_data[size] = '\0';
            ESP_LOGI(TAG, "数据内容: %s", str_data);
            free(str_data);
        }
    }
}

// SDP Offer回调函数
static void on_sdp_offer_created(const char *sdp_offer, void *user_data)
{
    ESP_LOGI(TAG, "🎯 SDP Offer已创建:");
    ESP_LOGI(TAG, "=== SDP OFFER START ===");
    ESP_LOGI(TAG, "%s", sdp_offer);
    ESP_LOGI(TAG, "=== SDP OFFER END ===");
    ESP_LOGI(TAG, "请将此SDP Offer发送给您的信令服务器");
}

// ICE候选回调函数
static void on_ice_candidate_received(const char *candidate, void *user_data)
{
    ESP_LOGI(TAG, "🧊 收到ICE候选: %s", candidate);
    ESP_LOGI(TAG, "请将此ICE候选发送给您的信令服务器");
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "🚀 ESP32 WebRTC客户端启动...");
    
    // 配置WebRTC客户端
    webrtc_client_config_t config = {
        .wifi_ssid = "HOSHOO",           // 请修改为您的WiFi SSID
        .wifi_password = "yc8449fyc",        // 请修改为您的WiFi密码
        .stun_server = "stun.freeswitch.org",    // 使用项目的STUN服务器
        .stun_port = 3478,                    // STUN服务器端口
        .enable_audio = true,                  // 启用音频
        .enable_video = false,                 // 暂时禁用视频（简化实现）
        .enable_data_channel = true            // 启用数据通道
    };
    
    ESP_LOGI(TAG, "配置信息:");
    ESP_LOGI(TAG, "  WiFi SSID: %s", config.wifi_ssid);
    ESP_LOGI(TAG, "  STUN服务器: %s:%d", config.stun_server, config.stun_port);
    ESP_LOGI(TAG, "  音频: %s", config.enable_audio ? "启用" : "禁用");
    ESP_LOGI(TAG, "  视频: %s", config.enable_video ? "启用" : "禁用");
    ESP_LOGI(TAG, "  数据通道: %s", config.enable_data_channel ? "启用" : "禁用");
    
    // 初始化WebRTC客户端
    esp_err_t ret = webrtc_client_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WebRTC客户端初始化失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ WebRTC客户端初始化成功");
    
    // 设置基本回调函数
    ret = webrtc_client_set_callbacks(
        on_webrtc_state_change,    // 状态变化回调
        on_audio_data,             // 音频数据回调
        on_video_data,             // 视频数据回调
        on_data_channel_data,      // 数据通道回调
        NULL                       // 用户数据
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 设置基本回调函数失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ 基本回调函数设置成功");
    
    // 设置SDP和ICE候选回调函数
    ret = webrtc_client_set_sdp_callbacks(
        on_sdp_offer_created,      // SDP Offer创建回调
        on_ice_candidate_received, // ICE候选接收回调
        NULL                       // 用户数据
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 设置SDP回调函数失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ SDP回调函数设置成功");
    
    // 启动WebRTC客户端
    ret = webrtc_client_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WebRTC客户端启动失败: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "✅ WebRTC客户端启动成功");
    
    ESP_LOGI(TAG, "🎉 WebRTC客户端已启动，准备与STUN服务器交互...");
    ESP_LOGI(TAG, "📋 使用说明:");
    ESP_LOGI(TAG, "1. 确保WiFi配置正确");
    ESP_LOGI(TAG, "2. 观察串口日志了解连接状态");
    ESP_LOGI(TAG, "3. ESP32将自动尝试与STUN服务器建立连接");
    ESP_LOGI(TAG, "4. 当SDP Offer创建后，请将其发送给您的信令服务器");
    ESP_LOGI(TAG, "5. 通过您的信令服务器接收Answer SDP和ICE候选");
    
    // 等待WiFi连接完成
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // 立即测试STUN服务器连通性
    ESP_LOGI(TAG, "🔍 立即测试STUN服务器连通性...");
    webrtc_client_test_stun_connectivity();
    
    // 尝试创建WebRTC连接
    ESP_LOGI(TAG, "🔗 开始创建WebRTC连接...");
    ret = webrtc_client_create_offer();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ 创建Offer失败: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "✅ Offer创建成功");
    }
    
    // 主循环 - 保持程序运行
    int loop_count = 0;
    while (1) {
        loop_count++;
        ESP_LOGI(TAG, "💓 主程序运行中... (循环 %d)", loop_count);
        
        // 检查STUN连接状态
        if (webrtc_client_is_stun_connected()) {
            ESP_LOGI(TAG, "✅ STUN服务器连接状态: 已连接");
            ESP_LOGI(TAG, "🌐 公网IP地址: %s", webrtc_client_get_public_ip());
        } else {
            ESP_LOGI(TAG, "❌ STUN服务器连接状态: 未连接");
            
            // 每5次循环测试一次STUN连通性
            if (loop_count % 5 == 0) {
                ESP_LOGI(TAG, "🔍 开始测试STUN服务器连通性...");
                webrtc_client_test_stun_connectivity();
            }
        }
        
        // 检查WebRTC状态
        webrtc_client_state_t state = webrtc_client_get_state();
        ESP_LOGI(TAG, "📊 WebRTC状态: %d", state);
        
        vTaskDelay(pdMS_TO_TICKS(30000)); // 每30秒打印一次心跳
    }
}
