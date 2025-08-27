#include "mqtt_client.hpp"

static const char *TAG = "mqtt_report";

// 全局变量
static char device_id[32];
static esp_mqtt_client_handle_t mqtt_client;
static int message_received_count = 0;

// 函数声明
static void handle_server_command(const char* command, cJSON* json_data);
static void process_server_response(const char* topic, const char* data, int data_len);

/**
 * @brief 生成设备唯一ID
 * 
 * 基于设备的MAC地址生成唯一标识符
 */
static void generate_device_id(void)
{
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret == ESP_OK) {
        snprintf(device_id, sizeof(device_id), "ESP32_%02X%02X%02X%02X%02X%02X", 
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        ESP_LOGI(TAG, "设备ID: %s", device_id);
    } else {
        // 失败时使用默认ID
        strcpy(device_id, "ESP32_DEFAULT");
        ESP_LOGW(TAG, "无法读取MAC地址，使用默认设备ID: %s", device_id);
    }
}

/**
 * @brief 创建MQTT消息JSON格式
 * 
 * @return 分配的JSON字符串，使用后需要释放
 */
static char* create_mqtt_message(void)
{
    cJSON *json = cJSON_CreateObject();
    cJSON *deviceId = cJSON_CreateString(device_id);
    cJSON *type = cJSON_CreateString("sfu");

    cJSON_AddItemToObject(json, "deviceId", deviceId);
    cJSON_AddItemToObject(json, "type", type);

    // 使用cJSON_PrintUnformatted确保发送紧凑的JSON格式
    char *json_string = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    // 验证生成的JSON
    if (json_string) {
        ESP_LOGI(TAG, "生成的JSON消息: %s", json_string);
        ESP_LOGI(TAG, "JSON消息长度: %d", strlen(json_string));
    }
    
    return json_string;
}

/**
 * @brief 处理服务器返回的消息
 * 
 * @param topic 消息主题
 * @param data 消息数据
 * @param data_len 数据长度
 */
static void process_server_response(const char* topic, const char* data, int data_len)
{
    message_received_count++; // 增加消息计数
    
    // 识别消息类型
    const char* message_type = "普通消息";
    if (topic) {
        if (strncmp(topic, MQTT_SUBSCRIBE_TOPIC_PREFIX, strlen(MQTT_SUBSCRIBE_TOPIC_PREFIX)) == 0) {
            message_type = "服务器响应消息";
        } else if (strcmp(topic, MQTT_LAST_WILL_TOPIC) == 0) {
            message_type = "遗嘱消息";
        } else if (strncmp(topic, MQTT_PUBLISH_TOPIC, strlen(MQTT_PUBLISH_TOPIC)) == 0) {
            message_type = "发布消息回显";
        }
    }
    
    if (!data || data_len <= 0) {
        printf("❌ 错误：服务器返回数据为空！\n");
        return;
    }
    
    // 创建以null结尾的字符串
    char *data_buffer = static_cast<char*>(malloc(data_len + 1));
    if (!data_buffer) {
        printf("❌ 错误：内存分配失败！\n");
        return;
    }
    
    memcpy(data_buffer, data, data_len);
    data_buffer[data_len] = '\0';
    
    printf("📨 收到%s: %s\n", message_type, data_buffer);
    
    // // 打印数据的十六进制表示
    // printf("🔍 数据十六进制表示: ");
    // for (int i = 0; i < data_len; i++) {
    //     printf("%02X ", (unsigned char)data[i]);
    // }
    // printf("\n");
    
    // // 尝试解析JSON
    // printf("🔧 尝试解析JSON...\n");
    // cJSON *json = cJSON_Parse(data_buffer);
    // if (json) {
    //     printf("✅ JSON解析成功！\n");
    //     ESP_LOGI(TAG, "✅ JSON解析成功");
        
    //     // 打印格式化的JSON
    //     char *formatted_json = cJSON_Print(json);
    //     if (formatted_json) {
    //         printf("📋 格式化JSON:\n%s\n", formatted_json);
    //         ESP_LOGI(TAG, "📋 格式化JSON:\n%s", formatted_json);
    //         free(formatted_json);
    //     }
        
    //     // 解析各个字段
    //     cJSON *deviceId = cJSON_GetObjectItem(json, "deviceId");
    //     cJSON *type = cJSON_GetObjectItem(json, "type");
    //     cJSON *command = cJSON_GetObjectItem(json, "command");
    //     cJSON *data_field = cJSON_GetObjectItem(json, "data");
    //     cJSON *status = cJSON_GetObjectItem(json, "status");
    //     cJSON *message = cJSON_GetObjectItem(json, "message");
        
    //     printf("🏷️  解析字段:\n");
        
    //     if (cJSON_IsString(deviceId)) {
    //         printf("   📱 设备ID: %s\n", deviceId->valuestring);
    //         ESP_LOGI(TAG, "📱 设备ID: %s", deviceId->valuestring);
    //     }
        
    //     if (cJSON_IsString(type)) {
    //         printf("   🏷️  类型: %s\n", type->valuestring);
    //         ESP_LOGI(TAG, "🏷️  类型: %s", type->valuestring);
    //     }
        
    //     if (cJSON_IsString(command)) {
    //         printf("   🎯 命令: %s\n", command->valuestring);
    //         ESP_LOGI(TAG, "🎯 命令: %s", command->valuestring);
    //         handle_server_command(command->valuestring, json);
    //     }
        
    //     if (data_field) {
    //         char *data_str = cJSON_Print(data_field);
    //         if (data_str) {
    //             printf("   📊 数据字段: %s\n", data_str);
    //             ESP_LOGI(TAG, "📊 数据字段: %s", data_str);
    //             free(data_str);
    //         }
    //     }
        
    //     if (cJSON_IsString(status)) {
    //         printf("   📈 状态: %s\n", status->valuestring);
    //         ESP_LOGI(TAG, "📈 状态: %s", status->valuestring);
    //     }
        
    //     if (cJSON_IsString(message)) {
    //         printf("   💬 消息: %s\n", message->valuestring);
    //         ESP_LOGI(TAG, "💬 消息: %s", message->valuestring);
    //     }
        
    //     cJSON_Delete(json);
    // } else {
    //     printf("⚠️  JSON解析失败，当作普通文本处理\n");
    //     ESP_LOGW(TAG, "⚠️  JSON解析失败，当作普通文本处理");
    //     printf("📄 纯文本内容: %s\n", data_buffer);
    //     ESP_LOGI(TAG, "📄 纯文本内容: %s", data_buffer);
        
    //     // 尝试作为键值对解析
    //     if (strstr(data_buffer, "deviceId=") || strstr(data_buffer, "type=")) {
    //         printf("🔍 检测到键值对格式\n");
    //         ESP_LOGI(TAG, "🔍 检测到键值对格式，可能需要转换为JSON");
    //     }
        
    //     // 检查cJSON错误
    //     const char *error_ptr = cJSON_GetErrorPtr();
    //     if (error_ptr != NULL) {
    //         printf("❌ cJSON错误: %s\n", error_ptr);
    //         ESP_LOGE(TAG, "❌ cJSON错误: %s", error_ptr);
    //     }
    // }
    
    free(data_buffer);
}

/**
 * @brief 处理服务器命令
 * 
 * @param command 命令字符串
 * @param json_data 完整的JSON数据
 */
static void handle_server_command(const char* command, cJSON* json_data)
{
    ESP_LOGI(TAG, "处理服务器命令: %s", command);
    
    if (strcmp(command, "ping") == 0) {
        // 响应ping命令
        ESP_LOGI(TAG, "收到ping命令，发送pong响应");
        char *pong_message = create_mqtt_message();
        if (pong_message && mqtt_client) {
            ESP_LOGI(TAG, "发送pong响应: %s", pong_message);
            int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_PUBLISH_TOPIC, pong_message, strlen(pong_message), 1, 0);
            ESP_LOGI(TAG, "pong响应发送结果: msg_id=%d", msg_id);
            free(pong_message);
        }
    } else if (strcmp(command, "restart") == 0) {
        ESP_LOGI(TAG, "收到重启命令，准备重启设备");
        // 发送确认消息后重启
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    } else if (strcmp(command, "status") == 0) {
        ESP_LOGI(TAG, "收到状态查询命令");
        // 发送设备状态信息
        char *status_message = create_mqtt_message();
        if (status_message && mqtt_client) {
            ESP_LOGI(TAG, "发送设备状态: %s", status_message);
            int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_PUBLISH_TOPIC, status_message, strlen(status_message), 1, 0);
            ESP_LOGI(TAG, "设备状态发送结果: msg_id=%d", msg_id);
            free(status_message);
        }
    } else {
        ESP_LOGW(TAG, "未知命令: %s", command);
    }
}

/**
 * @brief BOOT按钮轮询任务（无中断）
 * 
 * 处理BOOT按钮按下事件：
 * - 短按：订阅主题
 * - 长按：发布MQTT消息
 */
static void button_poll_task(void* arg)
{
    TickType_t last_operation_time = 0;
    const TickType_t min_operation_interval = pdMS_TO_TICKS(3000); // 3秒最小间隔，防止重复触发
    bool last_button_state = true; // GPIO0默认高电平（上拉）
    
    printf("🔘 按钮轮询任务启动\n");
    
    for(;;) {
        // 每50ms检查一次按钮状态
        vTaskDelay(50 / portTICK_PERIOD_MS);
        
        bool current_button_state = gpio_get_level(BOOT_BUTTON_GPIO);
        
        // 检测按钮按下（从高到低的跳变）
        if (last_button_state == true && current_button_state == false) {
            TickType_t current_time = xTaskGetTickCount();
            
            // 检查是否在最小间隔内，防止重复触发
            if (current_time - last_operation_time < min_operation_interval) {
                last_button_state = current_button_state;
                continue;
            }
            
            printf("🔘 按钮按下，检测按压时间...\n");
            
            // 防抖延时
            vTaskDelay(30 / portTICK_PERIOD_MS);
            
            // 再次确认按钮确实被按下
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                TickType_t press_start_time = xTaskGetTickCount();
                
                // 等待按钮释放，同时计算按压时间
                while(gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                    TickType_t current_press_time = xTaskGetTickCount();
                    
                    // 防止无限等待（最大3秒）
                    if ((current_press_time - press_start_time) > pdMS_TO_TICKS(3000)) {
                        printf("⚠️ 按钮长时间按下，跳过处理\n");
                        break;
                    }
                }
                
                TickType_t press_end_time = xTaskGetTickCount();
                int press_time_ms = (press_end_time - press_start_time) * portTICK_PERIOD_MS;
                
                last_operation_time = current_time;
                
                printf("🔘 按钮释放，按压时间: %d毫秒\n", press_time_ms);
                
                if (press_time_ms >= 1000) {
                    // 长按：发布消息
                    printf("🔘 长按检测到，发布消息\n");
                    
                    char *message = create_mqtt_message();
                    if (message && mqtt_client) {
                        int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_PUBLISH_TOPIC, message, strlen(message), 1, 0);
                        if (msg_id >= 0) {
                            printf("📤 消息发布成功\n");
                        } else {
                            printf("❌ 消息发布失败\n");
                        }
                        free(message);
                    }
                } else if (press_time_ms >= 50) { // 至少50ms才算有效按键
                    // 短按：先退出房间(发布遗嘱消息)，再加入房间(订阅主题)
                    printf("🔘 短按检测到，执行退出→加入房间流程\n");
                    
                    if (mqtt_client) {
                        // 步骤1：先发布遗嘱消息，表示退出房间
                        printf("⚰️ 步骤1: 发布遗嘱消息(退出房间)\n");
                        char *will_message = create_mqtt_message();
                        if (will_message) {
                            int will_msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_LAST_WILL_TOPIC, will_message, strlen(will_message), 1, 0);
                            if (will_msg_id >= 0) {
                                printf("✅ 遗嘱消息发布成功(已退出房间)\n");
                            } else {
                                printf("❌ 遗嘱消息发布失败\n");
                            }
                            free(will_message);
                        }
                        
                        // 小延时，确保遗嘱消息先发送
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        
                        // 步骤2：订阅主题，表示加入房间
                        printf("🏠 步骤2: 订阅主题(加入房间)\n");
                        char subscribe_topic[128];
                        snprintf(subscribe_topic, sizeof(subscribe_topic), "%s%s", MQTT_SUBSCRIBE_TOPIC_PREFIX, device_id);
                        
                        printf("📡 订阅服务器响应主题: %s\n", subscribe_topic);
                        
                        int msg_id = esp_mqtt_client_subscribe(mqtt_client, subscribe_topic, 1);
                        if (msg_id >= 0) {
                            printf("✅ 主题订阅成功(已加入房间)，等待服务器响应\n");
                        } else {
                            printf("❌ 主题订阅失败\n");
                        }
                    }
                }
            }
        }
        
        last_button_state = current_button_state;
    }
}

/**
 * @brief 初始化GPIO（无中断方式）
 */
static void init_gpio(void)
{
    gpio_config_t io_conf = {};
    
    // 配置BOOT按钮（GPIO0）为输入，带上拉，无中断
    io_conf.intr_type = GPIO_INTR_DISABLE;  // 完全禁用中断
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    
    // 创建按钮轮询任务（无中断）
    xTaskCreate(button_poll_task, "button_poll", 4096, NULL, 5, NULL);
    
    printf("🔘 BOOT按钮轮询模式已配置在GPIO%d\n", BOOT_BUTTON_GPIO);
}

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief 注册以接收 MQTT 事件的事件处理程序
 *
 *  此函数由 MQTT 客户端事件循环调用。
 *
 * @param handler_args 注册到事件的用户数据。
 * @param base 事件的基础（在此示例中始终为 MQTT Base）。
 * @param event_id 接收到的事件的 ID。
 * @param event_data 事件的数据，esp_mqtt_event_handle_t。
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG , "🎉 MQTT连接成功！\n");
        
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        printf("💔 MQTT连接断开\n");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        break; // 静默处理订阅成功
        
    case MQTT_EVENT_UNSUBSCRIBED:
        break; // 静默处理取消订阅
        
    case MQTT_EVENT_PUBLISHED:
        printf("📡 消息发布确认\n");
        break;
        
    case MQTT_EVENT_DATA:
        if (event->topic_len > 0 && event->data_len > 0) {
            // 创建主题字符串
            char *topic_buffer = static_cast<char*>(malloc(event->topic_len + 1));
            if (topic_buffer) {
                memcpy(topic_buffer, event->topic, event->topic_len);
                topic_buffer[event->topic_len] = '\0';
                
                // 使用新的处理函数
                process_server_response(topic_buffer, event->data, event->data_len);
                
                free(topic_buffer);
            }
        }
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT连接错误");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
        
    default:
        ESP_LOGI(TAG, "其他MQTT事件 id:%d", event->event_id);
        break;
    }
}

/*
 * @brief 启动 MQTT 应用程序
 *
 *  此函数初始化 MQTT 客户端并启动事件循环。
 */
// 注意：遗嘱消息由MQTT broker在设备意外断线时自动发送，无需手动发送函数

/**
 * @brief 安全关机处理函数
 * 
 * 在设备重启或关机前执行清理操作
 */
static void safe_shutdown_handler(void)
{
    ESP_LOGI(TAG, "执行安全关机程序...");
    
    // 注意：正常关机时不发送遗嘱消息，遗嘱消息只在意外断线时由MQTT broker自动发送
    
    // 断开MQTT连接
    if (mqtt_client) {
        esp_mqtt_client_disconnect(mqtt_client);
        vTaskDelay(500 / portTICK_PERIOD_MS);  // 等待断开完成
    }
    
    ESP_LOGI(TAG, "安全关机程序完成");
}

static void mqtt_app_start(void)
{
    // 生成设备ID
    generate_device_id();
    
    // 准备遗嘱消息
    char *last_will_message = create_mqtt_message();
    
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = CONFIG_BROKER_URL;
    mqtt_cfg.session.last_will.topic = MQTT_LAST_WILL_TOPIC;
    mqtt_cfg.session.last_will.msg = last_will_message;
    mqtt_cfg.session.last_will.msg_len = last_will_message ? strlen(last_will_message) : 0;
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 0;
    mqtt_cfg.session.keepalive = 60;
    mqtt_cfg.network.reconnect_timeout_ms = 5000;
    mqtt_cfg.network.timeout_ms = 5000;

#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("请输入MQTT代理URL\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("代理URL: %s\n", line);
    } else {
        ESP_LOGE(TAG, "配置错误：代理URL配置错误");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    ESP_LOGI(TAG, "初始化MQTT客户端，设备ID: %s", device_id);
    ESP_LOGI(TAG, "遗嘱消息主题: %s", MQTT_LAST_WILL_TOPIC);
    if (last_will_message) {
        ESP_LOGI(TAG, "遗嘱消息内容: %s", last_will_message);
    }

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT客户端初始化失败");
        if (last_will_message) {
            free(last_will_message);
        }
        return;
    }

    /* 注册MQTT事件处理程序 */
    esp_mqtt_client_register_event(mqtt_client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    
    // 注册重启钩子函数，确保正常关机时清理资源（遗嘱消息仅在意外断线时自动发送）
    esp_register_shutdown_handler(safe_shutdown_handler);
    
    ESP_LOGI(TAG, "MQTT客户端启动完成");
    
    // 清理临时分配的遗嘱消息内存
    if (last_will_message) {
        free(last_will_message);
    }
}


/*
 * @brief 应用程序的主入口点
 *
 *  此函数初始化系统并启动 MQTT 应用程序。
 */
// 心跳检查功能已移除，保持输出简洁

void mqtt_DoNow(){
    printf("\n🚀 ESP32 MQTT设备启动\n");
    
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_report", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());
    printf("📶 网络连接成功\n");

    // 延时确保网络稳定
    vTaskDelay(pdMS_TO_TICKS(1000));

    mqtt_app_start();
    
    // 等待MQTT稳定后再初始化GPIO（无中断轮询模式更安全）
    vTaskDelay(pdMS_TO_TICKS(3000));
    init_gpio();
    
    printf("✅ 设备配置完成\n");
    printf("   🔘 短按BOOT键(<1秒): 退出房间→加入房间\n");
    printf("   🔘 长按BOOT键(>=1秒): 在房间内发布消息\n");
}