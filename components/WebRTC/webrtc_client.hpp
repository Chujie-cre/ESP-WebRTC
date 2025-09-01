#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_peer.h"
#include "esp_peer_default.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#ifdef __cplusplus
extern "C" {
#endif

// WebRTC客户端状态枚举
typedef enum {
    WEBRTC_CLIENT_STATE_IDLE = 0,           // 空闲状态
    WEBRTC_CLIENT_STATE_INITIALIZING,       // 初始化中
    WEBRTC_CLIENT_STATE_WIFI_CONNECTING,    // WiFi连接中
    WEBRTC_CLIENT_STATE_WIFI_CONNECTED,     // WiFi已连接
    WEBRTC_CLIENT_STATE_PEER_CREATING,      // 创建Peer连接中
    WEBRTC_CLIENT_STATE_PEER_CREATED,       // Peer连接已创建
    WEBRTC_CLIENT_STATE_OFFER_CREATED,      // Offer已创建
    WEBRTC_CLIENT_STATE_ANSWER_RECEIVED,    // 收到Answer
    WEBRTC_CLIENT_STATE_CONNECTING,         // 连接中
    WEBRTC_CLIENT_STATE_CONNECTED,          // 已连接
    WEBRTC_CLIENT_STATE_DISCONNECTED,       // 已断开
    WEBRTC_CLIENT_STATE_ERROR               // 错误状态
} webrtc_client_state_t;

// WebRTC客户端配置结构体
typedef struct {
    char wifi_ssid[32];                     // WiFi SSID
    char wifi_password[64];                 // WiFi密码
    char stun_server[64];                   // STUN服务器地址
    uint16_t stun_port;                     // STUN服务器端口
    bool enable_audio;                      // 是否启用音频
    bool enable_video;                      // 是否启用视频
    bool enable_data_channel;               // 是否启用数据通道
} webrtc_client_config_t;

// WebRTC客户端结构体
typedef struct {
    webrtc_client_config_t config;          // 客户端配置
    webrtc_client_state_t state;            // 当前状态
    esp_peer_handle_t peer;                 // ESP Peer实例
    esp_peer_cfg_t peer_cfg;               // Peer配置
    const esp_peer_ops_t *peer_ops;        // Peer操作接口
    TaskHandle_t main_task_handle;          // 主任务句柄
    bool is_running;                        // 是否正在运行
    char local_sdp[2048];                   // 本地SDP
    char remote_sdp[2048];                  // 远程SDP
    char ice_candidates[10][256];           // ICE候选列表
    int ice_candidate_count;                // ICE候选数量
} webrtc_client_t;

// 回调函数类型定义
typedef void (*webrtc_state_callback_t)(webrtc_client_state_t state, void *user_data);
typedef void (*webrtc_audio_callback_t)(const uint8_t *data, size_t size, void *user_data);
typedef void (*webrtc_video_callback_t)(const uint8_t *data, size_t size, void *user_data);
typedef void (*webrtc_data_callback_t)(const uint8_t *data, size_t size, void *user_data);

// SDP和ICE候选回调函数类型
typedef void (*webrtc_sdp_offer_callback_t)(const char *sdp_offer, void *user_data);
typedef void (*webrtc_ice_candidate_callback_t)(const char *candidate, void *user_data);

// 函数声明
esp_err_t webrtc_client_init(webrtc_client_config_t *config);
esp_err_t webrtc_client_deinit(void);
esp_err_t webrtc_client_start(void);
esp_err_t webrtc_client_stop(void);
esp_err_t webrtc_client_set_callbacks(
    webrtc_state_callback_t state_cb,
    webrtc_audio_callback_t audio_cb,
    webrtc_video_callback_t video_cb,
    webrtc_data_callback_t data_cb,
    void *user_data
);

// WebRTC连接管理函数
esp_err_t webrtc_client_create_offer(void);
esp_err_t webrtc_client_set_answer(const char *answer_sdp);
esp_err_t webrtc_client_add_ice_candidate(const char *candidate);
esp_err_t webrtc_client_set_sdp_callbacks(
    webrtc_sdp_offer_callback_t sdp_offer_cb,
    webrtc_ice_candidate_callback_t ice_candidate_cb,
    void *user_data
);

// 获取当前状态和SDP信息
webrtc_client_state_t webrtc_client_get_state(void);
const char* webrtc_client_get_local_sdp(void);
const char* webrtc_client_get_remote_sdp(void);

// STUN服务器连接状态检测
bool webrtc_client_is_stun_connected(void);
const char* webrtc_client_get_public_ip(void);
esp_err_t webrtc_client_test_stun_connectivity(void);

#ifdef __cplusplus
}
#endif 