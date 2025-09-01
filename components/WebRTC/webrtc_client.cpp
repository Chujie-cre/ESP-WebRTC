#include "webrtc_client.hpp"

#include <string.h>
#include <stdlib.h>
#include "lwip/netdb.h"

// 日志标签
static const char *TAG = "WebRTC_Client";

// 全局WebRTC客户端实例
static webrtc_client_t g_webrtc_client;

// STUN连接状态
static bool g_stun_connected = false;
static char g_public_ip[64] = {0};

// 回调函数指针
static webrtc_state_callback_t g_state_callback = NULL;
static webrtc_audio_callback_t g_audio_callback = NULL;
static webrtc_video_callback_t g_video_callback = NULL;
static webrtc_data_callback_t g_data_callback = NULL;
static webrtc_sdp_offer_callback_t g_sdp_offer_callback = NULL;
static webrtc_ice_candidate_callback_t g_ice_candidate_callback = NULL;
static void *g_user_data = NULL;

// WiFi事件处理函数
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi连接开始...");
        g_webrtc_client.state = WEBRTC_CLIENT_STATE_WIFI_CONNECTING;
        if (g_state_callback) {
            g_state_callback(g_webrtc_client.state, g_user_data);
        }
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "WiFi连接断开，尝试重连...");
        g_webrtc_client.state = WEBRTC_CLIENT_STATE_WIFI_CONNECTING;
        if (g_state_callback) {
            g_state_callback(g_webrtc_client.state, g_user_data);
        }
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "WiFi已连接，IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
        g_webrtc_client.state = WEBRTC_CLIENT_STATE_WIFI_CONNECTED;
        if (g_state_callback) {
            g_state_callback(g_webrtc_client.state, g_user_data);
        }
    }
}

// ESP Peer状态回调函数
static int peer_state_callback(esp_peer_state_t state, void *ctx)
{
    ESP_LOGI(TAG, "Peer状态变化: %d", state);
    
    switch (state) {
        case ESP_PEER_STATE_CLOSED:
            ESP_LOGI(TAG, "Peer连接已关闭");
            g_webrtc_client.state = WEBRTC_CLIENT_STATE_IDLE;
            break;
        case ESP_PEER_STATE_DISCONNECTED:
            ESP_LOGI(TAG, "WebRTC连接已断开");
            g_webrtc_client.state = WEBRTC_CLIENT_STATE_DISCONNECTED;
            break;
        case ESP_PEER_STATE_NEW_CONNECTION:
            ESP_LOGI(TAG, "新连接创建，开始收集ICE候选...");
            g_webrtc_client.state = WEBRTC_CLIENT_STATE_PEER_CREATED;
            break;
        case ESP_PEER_STATE_PAIRING:
            ESP_LOGI(TAG, "正在配对ICE候选...");
            break;
        case ESP_PEER_STATE_PAIRED:
            ESP_LOGI(TAG, "ICE候选配对成功");
            break;
        case ESP_PEER_STATE_CONNECTING:
            ESP_LOGI(TAG, "正在建立连接...");
            g_webrtc_client.state = WEBRTC_CLIENT_STATE_CONNECTING;
            break;
        case ESP_PEER_STATE_CONNECTED:
            ESP_LOGI(TAG, "WebRTC连接已建立！");
            g_webrtc_client.state = WEBRTC_CLIENT_STATE_CONNECTED;
            break;
        case ESP_PEER_STATE_CONNECT_FAILED:
            ESP_LOGI(TAG, "连接失败");
            g_webrtc_client.state = WEBRTC_CLIENT_STATE_ERROR;
            break;
        case ESP_PEER_STATE_DATA_CHANNEL_CONNECTED:
            ESP_LOGI(TAG, "数据通道已连接");
            break;
        case ESP_PEER_STATE_DATA_CHANNEL_OPENED:
            ESP_LOGI(TAG, "数据通道已打开");
            break;
        case ESP_PEER_STATE_DATA_CHANNEL_CLOSED:
            ESP_LOGI(TAG, "数据通道已关闭");
            break;
        case ESP_PEER_STATE_DATA_CHANNEL_DISCONNECTED:
            ESP_LOGI(TAG, "数据通道已断开");
            break;
        default:
            ESP_LOGI(TAG, "未知状态: %d", state);
            break;
    }
    
    if (g_state_callback) {
        g_state_callback(g_webrtc_client.state, g_user_data);
    }
    return ESP_OK;
}

// ESP Peer消息回调函数
static int peer_message_callback(esp_peer_msg_t *msg, void *ctx)
{
    ESP_LOGI(TAG, "收到Peer消息，类型: %d", msg->type);
    
    switch (msg->type) {
        case ESP_PEER_MSG_TYPE_SDP:
            ESP_LOGI(TAG, "收到SDP消息");
            if (msg->data && msg->size > 0) {
                // 处理收到的SDP
                strncpy(g_webrtc_client.remote_sdp, (char*)msg->data, sizeof(g_webrtc_client.remote_sdp) - 1);
                g_webrtc_client.remote_sdp[sizeof(g_webrtc_client.remote_sdp) - 1] = '\0';
                ESP_LOGI(TAG, "远程SDP: %s", g_webrtc_client.remote_sdp);
            }
            break;
        case ESP_PEER_MSG_TYPE_CANDIDATE:
            ESP_LOGI(TAG, "收到ICE候选");
            if (msg->data && msg->size > 0) {
                // 处理收到的ICE候选
                char *candidate = (char*)msg->data;
                ESP_LOGI(TAG, "ICE候选: %s", candidate);
                
                // 分析ICE候选类型，判断STUN服务器连接状态
                if (strstr(candidate, "srflx") != NULL) {
                    ESP_LOGI(TAG, "🎯 STUN服务器连接成功！检测到服务器反射候选(srflx)");
                    ESP_LOGI(TAG, "✅ 公网IP地址已获取");
                    
                    // 提取公网IP地址
                    char *ip_start = strstr(candidate, "c=IN IP4 ");
                    if (ip_start) {
                        ip_start += 9; // 跳过 "c=IN IP4 "
                        char *ip_end = strchr(ip_start, ' ');
                        if (ip_end) {
                            int ip_len = ip_end - ip_start;
                            if (ip_len < sizeof(g_public_ip)) {
                                strncpy(g_public_ip, ip_start, ip_len);
                                g_public_ip[ip_len] = '\0';
                                ESP_LOGI(TAG, "🌐 公网IP地址: %s", g_public_ip);
                            }
                        }
                    }
                    
                    g_stun_connected = true;
                } else if (strstr(candidate, "host") != NULL) {
                    ESP_LOGI(TAG, "🏠 本地候选(host)");
                } else if (strstr(candidate, "relay") != NULL) {
                    ESP_LOGI(TAG, "🔄 TURN服务器候选(relay)");
                }
                
                // 通知外部处理ICE候选
                if (g_ice_candidate_callback) {
                    g_ice_candidate_callback(candidate, g_user_data);
                }
            }
            break;
        default:
            ESP_LOGI(TAG, "未知消息类型: %d", msg->type);
            break;
    }
    
    return ESP_OK;
}

// ESP Peer音频数据回调函数
static int peer_audio_callback(esp_peer_audio_frame_t *frame, void *ctx)
{
    if (g_audio_callback && frame && frame->data && frame->size > 0) {
        g_audio_callback(frame->data, frame->size, g_user_data);
    }
    return ESP_OK;
}

// ESP Peer视频数据回调函数
static int peer_video_callback(esp_peer_video_frame_t *frame, void *ctx)
{
    if (g_video_callback && frame && frame->data && frame->size > 0) {
        g_video_callback(frame->data, frame->size, g_user_data);
    }
    return ESP_OK;
}

// ESP Peer数据通道回调函数
static int peer_data_callback(esp_peer_data_frame_t *frame, void *ctx)
{
    if (g_data_callback && frame && frame->data && frame->size > 0) {
        g_data_callback(frame->data, frame->size, g_user_data);
    }
    return ESP_OK;
}

// WebRTC客户端主任务
static void webrtc_client_main_task(void *pvParameters)
{
    ESP_LOGI(TAG, "WebRTC客户端主任务启动");
    
    while (g_webrtc_client.is_running) {
        if (g_webrtc_client.peer) {
            esp_peer_main_loop(g_webrtc_client.peer);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "WebRTC客户端主任务退出");
    vTaskDelete(NULL);
}

// 初始化WebRTC客户端
esp_err_t webrtc_client_init(webrtc_client_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "配置参数为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "初始化WebRTC客户端...");
    
    // 初始化全局结构体
    memset(&g_webrtc_client, 0, sizeof(webrtc_client_t));
    memcpy(&g_webrtc_client.config, config, sizeof(webrtc_client_config_t));
    g_webrtc_client.state = WEBRTC_CLIENT_STATE_INITIALIZING;
    
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    
    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 创建默认网络接口
    esp_netif_create_default_wifi_sta();
    
    // 初始化WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // 注册WiFi事件处理器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                       ESP_EVENT_ANY_ID,
                                                       &wifi_event_handler,
                                                       NULL,
                                                       NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                       IP_EVENT_STA_GOT_IP,
                                                       &wifi_event_handler,
                                                       NULL,
                                                       NULL));
    
    // 设置WiFi模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // 配置WiFi连接参数
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    strcpy((char*)wifi_config.sta.ssid, config->wifi_ssid);
    strcpy((char*)wifi_config.sta.password, config->wifi_password);
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    ESP_LOGI(TAG, "WebRTC客户端初始化完成");
    return ESP_OK;
}

// 反初始化WebRTC客户端
esp_err_t webrtc_client_deinit(void)
{
    ESP_LOGI(TAG, "反初始化WebRTC客户端...");
    
    // 停止客户端
    webrtc_client_stop();
    
    // 关闭WiFi
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // 销毁事件循环
    esp_event_loop_delete_default();
    
    // 销毁网络接口
    esp_netif_deinit();
    
    // 擦除NVS
    nvs_flash_erase();
    
    ESP_LOGI(TAG, "WebRTC客户端反初始化完成");
    return ESP_OK;
}

// 启动WebRTC客户端
esp_err_t webrtc_client_start(void)
{
    ESP_LOGI(TAG, "启动WebRTC客户端...");
    
    if (g_webrtc_client.is_running) {
        ESP_LOGW(TAG, "WebRTC客户端已经在运行");
        return ESP_OK;
    }
    
    // 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // 创建ESP Peer配置
    memset(&g_webrtc_client.peer_cfg, 0, sizeof(esp_peer_cfg_t));
    g_webrtc_client.peer_cfg.role = ESP_PEER_ROLE_CONTROLLING;  // 作为控制端
    g_webrtc_client.peer_cfg.ice_trans_policy = ESP_PEER_ICE_TRANS_POLICY_ALL;
    
    // 配置STUN服务器
    static esp_peer_ice_server_cfg_t ice_server;
    ice_server.stun_url = g_webrtc_client.config.stun_server;
    ice_server.user = NULL;
    ice_server.psw = NULL;
    
    g_webrtc_client.peer_cfg.server_lists = &ice_server;
    g_webrtc_client.peer_cfg.server_num = 1;  // 配置一个STUN服务器
    ESP_LOGI(TAG, "配置STUN服务器: %s:%d", g_webrtc_client.config.stun_server, g_webrtc_client.config.stun_port);
    
    // 确保配置完整
    g_webrtc_client.peer_cfg.role = ESP_PEER_ROLE_CONTROLLING;
    g_webrtc_client.peer_cfg.ice_trans_policy = ESP_PEER_ICE_TRANS_POLICY_ALL;
    g_webrtc_client.peer_cfg.no_auto_reconnect = false;
    g_webrtc_client.peer_cfg.enable_data_channel = true;
    g_webrtc_client.peer_cfg.manual_ch_create = false;
    
    // 配置音频（如果启用）
    if (g_webrtc_client.config.enable_audio) {
        g_webrtc_client.peer_cfg.audio_info.codec = ESP_PEER_AUDIO_CODEC_OPUS;
        g_webrtc_client.peer_cfg.audio_info.sample_rate = 48000;
        g_webrtc_client.peer_cfg.audio_info.channel = 1;
        g_webrtc_client.peer_cfg.audio_dir = ESP_PEER_MEDIA_DIR_SEND_RECV;
    }
    
    // 配置视频（如果启用）
    if (g_webrtc_client.config.enable_video) {
        g_webrtc_client.peer_cfg.video_info.codec = ESP_PEER_VIDEO_CODEC_H264;
        g_webrtc_client.peer_cfg.video_info.width = 640;
        g_webrtc_client.peer_cfg.video_info.height = 480;
        g_webrtc_client.peer_cfg.video_info.fps = 30;
        g_webrtc_client.peer_cfg.video_dir = ESP_PEER_MEDIA_DIR_SEND_RECV;
    }
    
    // 配置数据通道（如果启用）
    if (g_webrtc_client.config.enable_data_channel) {
        g_webrtc_client.peer_cfg.enable_data_channel = true;
    }
    
    // 设置回调函数
    g_webrtc_client.peer_cfg.on_state = peer_state_callback;
    g_webrtc_client.peer_cfg.on_msg = peer_message_callback;
    g_webrtc_client.peer_cfg.on_audio_data = peer_audio_callback;
    g_webrtc_client.peer_cfg.on_video_data = peer_video_callback;
    g_webrtc_client.peer_cfg.on_data = peer_data_callback;
    g_webrtc_client.peer_cfg.ctx = NULL;
    
    // 获取默认的Peer操作接口
    g_webrtc_client.peer_ops = esp_peer_get_default_impl();
    if (!g_webrtc_client.peer_ops) {
        ESP_LOGE(TAG, "获取Peer操作接口失败");
        return ESP_FAIL;
    }
    
    // 创建Peer连接
    int ret = esp_peer_open(&g_webrtc_client.peer_cfg, g_webrtc_client.peer_ops, &g_webrtc_client.peer);
    if (ret != 0) {
        ESP_LOGE(TAG, "创建Peer连接失败: %d", ret);
        return ESP_FAIL;
    }
    
    // 创建新连接，开始收集ICE候选
    ret = esp_peer_new_connection(g_webrtc_client.peer);
    if (ret != 0) {
        ESP_LOGE(TAG, "创建新连接失败: %d", ret);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "开始收集ICE候选...");
    
    // 启动主任务
    g_webrtc_client.is_running = true;
    xTaskCreate(webrtc_client_main_task, "webrtc_main", 8192, NULL, 5, &g_webrtc_client.main_task_handle);
    
    ESP_LOGI(TAG, "WebRTC客户端启动完成");
    return ESP_OK;
}

// 停止WebRTC客户端
esp_err_t webrtc_client_stop(void)
{
    ESP_LOGI(TAG, "停止WebRTC客户端...");
    
    if (!g_webrtc_client.is_running) {
        ESP_LOGW(TAG, "WebRTC客户端未在运行");
        return ESP_OK;
    }
    
    // 停止主任务
    g_webrtc_client.is_running = false;
    if (g_webrtc_client.main_task_handle) {
        vTaskDelete(g_webrtc_client.main_task_handle);
        g_webrtc_client.main_task_handle = NULL;
    }
    
    // 关闭Peer连接
    if (g_webrtc_client.peer) {
        esp_peer_close(g_webrtc_client.peer);
        g_webrtc_client.peer = NULL;
    }
    
    g_webrtc_client.state = WEBRTC_CLIENT_STATE_IDLE;
    if (g_state_callback) {
        g_state_callback(g_webrtc_client.state, g_user_data);
    }
    
    ESP_LOGI(TAG, "WebRTC客户端停止完成");
    return ESP_OK;
}

// 设置回调函数
esp_err_t webrtc_client_set_callbacks(
    webrtc_state_callback_t state_cb,
    webrtc_audio_callback_t audio_cb,
    webrtc_video_callback_t video_cb,
    webrtc_data_callback_t data_cb,
    void *user_data)
{
    g_state_callback = state_cb;
    g_audio_callback = audio_cb;
    g_video_callback = video_cb;
    g_data_callback = data_cb;
    g_user_data = user_data;
    
    ESP_LOGI(TAG, "回调函数设置完成");
    return ESP_OK;
}

// 设置SDP和ICE候选回调函数
esp_err_t webrtc_client_set_sdp_callbacks(
    webrtc_sdp_offer_callback_t sdp_offer_cb,
    webrtc_ice_candidate_callback_t ice_candidate_cb,
    void *user_data)
{
    g_sdp_offer_callback = sdp_offer_cb;
    g_ice_candidate_callback = ice_candidate_cb;
    g_user_data = user_data;
    
    ESP_LOGI(TAG, "SDP回调函数设置完成");
    return ESP_OK;
}

// 创建SDP Offer
esp_err_t webrtc_client_create_offer(void)
{
    ESP_LOGI(TAG, "创建SDP Offer...");
    
    if (!g_webrtc_client.peer) {
        ESP_LOGE(TAG, "Peer连接未创建");
        return ESP_FAIL;
    }
    
    // 生成一个简单的SDP Offer（实际应该由esp_peer生成）
    const char *sdp_offer = 
        "v=0\r\n"
        "o=- 1234567890 2 IN IP4 127.0.0.1\r\n"
        "s=-\r\n"
        "t=0 0\r\n"
        "a=group:BUNDLE 0\r\n"
        "m=application 9 DTLS/SCTP 5000\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "a=mid:0\r\n"
        "a=sctp-port:5000\r\n"
        "a=max-message-size:262144\r\n";
    
    strncpy(g_webrtc_client.local_sdp, sdp_offer, sizeof(g_webrtc_client.local_sdp) - 1);
    g_webrtc_client.local_sdp[sizeof(g_webrtc_client.local_sdp) - 1] = '\0';
    
    // 更新状态
    g_webrtc_client.state = WEBRTC_CLIENT_STATE_OFFER_CREATED;
    if (g_state_callback) {
        g_state_callback(g_webrtc_client.state, g_user_data);
    }
    
    // 通知外部SDP Offer已创建
    if (g_sdp_offer_callback) {
        g_sdp_offer_callback(g_webrtc_client.local_sdp, g_user_data);
    }
    
    ESP_LOGI(TAG, "SDP Offer创建完成");
    return ESP_OK;
}

// 设置Answer SDP
esp_err_t webrtc_client_set_answer(const char *answer_sdp)
{
    if (!answer_sdp) {
        ESP_LOGE(TAG, "Answer SDP为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "设置Answer SDP...");
    strncpy(g_webrtc_client.remote_sdp, answer_sdp, sizeof(g_webrtc_client.remote_sdp) - 1);
    g_webrtc_client.remote_sdp[sizeof(g_webrtc_client.remote_sdp) - 1] = '\0';
    
    // 更新状态
    g_webrtc_client.state = WEBRTC_CLIENT_STATE_ANSWER_RECEIVED;
    if (g_state_callback) {
        g_state_callback(g_webrtc_client.state, g_user_data);
    }
    
    ESP_LOGI(TAG, "Answer SDP设置完成");
    return ESP_OK;
}

// 添加ICE候选
esp_err_t webrtc_client_add_ice_candidate(const char *candidate)
{
    if (!candidate) {
        ESP_LOGE(TAG, "ICE候选为空");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "添加ICE候选: %s", candidate);
    
    // 存储ICE候选
    if (g_webrtc_client.ice_candidate_count < 10) {
        strncpy(g_webrtc_client.ice_candidates[g_webrtc_client.ice_candidate_count], 
                candidate, 255);
        g_webrtc_client.ice_candidates[g_webrtc_client.ice_candidate_count][255] = '\0';
        g_webrtc_client.ice_candidate_count++;
    }
    
    return ESP_OK;
}

// 获取当前状态
webrtc_client_state_t webrtc_client_get_state(void)
{
    return g_webrtc_client.state;
}

// 获取本地SDP
const char* webrtc_client_get_local_sdp(void)
{
    return g_webrtc_client.local_sdp;
}

// 获取远程SDP
const char* webrtc_client_get_remote_sdp(void)
{
    return g_webrtc_client.remote_sdp;
}

// 检查STUN服务器是否连接成功
bool webrtc_client_is_stun_connected(void)
{
    return g_stun_connected;
}

// 获取公网IP地址
const char* webrtc_client_get_public_ip(void)
{
    return g_public_ip;
}

// 测试STUN服务器连通性
esp_err_t webrtc_client_test_stun_connectivity(void)
{
    ESP_LOGI(TAG, "🧪 测试STUN服务器连通性...");
    ESP_LOGI(TAG, "目标STUN服务器: %s:%d", g_webrtc_client.config.stun_server, g_webrtc_client.config.stun_port);
    
    // 使用ESP-IDF的DNS解析来测试连通性
    struct hostent *he = gethostbyname(g_webrtc_client.config.stun_server);
    if (he == NULL) {
        ESP_LOGW(TAG, "❌ 无法解析STUN服务器域名: %s", g_webrtc_client.config.stun_server);
        ESP_LOGW(TAG, "建议使用IP地址而不是域名");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ STUN服务器域名解析成功: %s", g_webrtc_client.config.stun_server);
    
    // 显示解析到的IP地址
    struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
    if (addr_list[0] != NULL) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntoa_r(*addr_list[0], ip_str, INET_ADDRSTRLEN);
        ESP_LOGI(TAG, "🌐 解析到的IP地址: %s", ip_str);
    }
    
    return ESP_OK;
}


