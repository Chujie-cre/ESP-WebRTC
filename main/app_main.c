/* MQTT（基于 TCP）示例

   此示例代码属于公共领域（或根据您的选择，采用 CC0 许可）。

   除非适用法律要求或书面同意，否则
   本软件按“原样”分发，不附带任何明示或
   暗示的保证或条件。
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_mac.h"
#include "cJSON.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt_example";

// MQTT主题定义
#define MQTT_SUBSCRIBE_TOPIC_PREFIX "/public/striped-kind-tiger/result/"
#define MQTT_PUBLISH_TOPIC "/public/striped-kind-tiger/invoke/"
#define MQTT_LAST_WILL_TOPIC "/device/end"

// GPIO定义
#define BOOT_BUTTON_GPIO 0
#define GPIO_INPUT_PIN_SEL (1ULL << BOOT_BUTTON_GPIO)

// 全局变量
static char device_id[32];
static esp_mqtt_client_handle_t mqtt_client;
static QueueHandle_t gpio_evt_queue = NULL;
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
    
    printf("\n===========================================\n");
    printf("🔥🔥🔥 强制输出：收到服务器返回消息 #%d 🔥🔥🔥\n", message_received_count);
    printf("===========================================\n");
    
    ESP_LOGI(TAG, "🎯 处理服务器响应开始");
    ESP_LOGI(TAG, "📍 主题: %s", topic ? topic : "NULL");
    ESP_LOGI(TAG, "📊 数据长度: %d", data_len);
    
    if (!data || data_len <= 0) {
        ESP_LOGE(TAG, "❌ 数据为空或长度无效 - data=%p, len=%d", data, data_len);
        printf("❌ 错误：服务器返回数据为空！\n");
        printf("===========================================\n");
        return;
    }
    
    // 强制打印原始数据
    printf("📦 服务器原始数据 (%d字节): ", data_len);
    for (int i = 0; i < data_len; i++) {
        printf("%c", data[i]);
    }
    printf("\n");
    
    // 创建以null结尾的字符串
    char *data_buffer = malloc(data_len + 1);
    if (!data_buffer) {
        ESP_LOGE(TAG, "❌ 内存分配失败 - 需要 %d 字节", data_len + 1);
        printf("❌ 错误：内存分配失败！\n");
        printf("===========================================\n");
        return;
    }
    
    memcpy(data_buffer, data, data_len);
    data_buffer[data_len] = '\0';
    
    printf("📝 处理后的数据: %s\n", data_buffer);
    ESP_LOGI(TAG, "📝 字符串形式数据: %s", data_buffer);
    
    // 打印数据的十六进制表示
    printf("🔍 数据十六进制表示: ");
    for (int i = 0; i < data_len; i++) {
        printf("%02X ", (unsigned char)data[i]);
    }
    printf("\n");
    
    // 尝试解析JSON
    printf("🔧 尝试解析JSON...\n");
    cJSON *json = cJSON_Parse(data_buffer);
    if (json) {
        printf("✅ JSON解析成功！\n");
        ESP_LOGI(TAG, "✅ JSON解析成功");
        
        // 打印格式化的JSON
        char *formatted_json = cJSON_Print(json);
        if (formatted_json) {
            printf("📋 格式化JSON:\n%s\n", formatted_json);
            ESP_LOGI(TAG, "📋 格式化JSON:\n%s", formatted_json);
            free(formatted_json);
        }
        
        // 解析各个字段
        cJSON *deviceId = cJSON_GetObjectItem(json, "deviceId");
        cJSON *type = cJSON_GetObjectItem(json, "type");
        cJSON *command = cJSON_GetObjectItem(json, "command");
        cJSON *data_field = cJSON_GetObjectItem(json, "data");
        cJSON *status = cJSON_GetObjectItem(json, "status");
        cJSON *message = cJSON_GetObjectItem(json, "message");
        
        printf("🏷️  解析字段:\n");
        
        if (cJSON_IsString(deviceId)) {
            printf("   📱 设备ID: %s\n", deviceId->valuestring);
            ESP_LOGI(TAG, "📱 设备ID: %s", deviceId->valuestring);
        }
        
        if (cJSON_IsString(type)) {
            printf("   🏷️  类型: %s\n", type->valuestring);
            ESP_LOGI(TAG, "🏷️  类型: %s", type->valuestring);
        }
        
        if (cJSON_IsString(command)) {
            printf("   🎯 命令: %s\n", command->valuestring);
            ESP_LOGI(TAG, "🎯 命令: %s", command->valuestring);
            handle_server_command(command->valuestring, json);
        }
        
        if (data_field) {
            char *data_str = cJSON_Print(data_field);
            if (data_str) {
                printf("   📊 数据字段: %s\n", data_str);
                ESP_LOGI(TAG, "📊 数据字段: %s", data_str);
                free(data_str);
            }
        }
        
        if (cJSON_IsString(status)) {
            printf("   📈 状态: %s\n", status->valuestring);
            ESP_LOGI(TAG, "📈 状态: %s", status->valuestring);
        }
        
        if (cJSON_IsString(message)) {
            printf("   💬 消息: %s\n", message->valuestring);
            ESP_LOGI(TAG, "💬 消息: %s", message->valuestring);
        }
        
        cJSON_Delete(json);
    } else {
        printf("⚠️  JSON解析失败，当作普通文本处理\n");
        ESP_LOGW(TAG, "⚠️  JSON解析失败，当作普通文本处理");
        printf("📄 纯文本内容: %s\n", data_buffer);
        ESP_LOGI(TAG, "📄 纯文本内容: %s", data_buffer);
        
        // 尝试作为键值对解析
        if (strstr(data_buffer, "deviceId=") || strstr(data_buffer, "type=")) {
            printf("🔍 检测到键值对格式\n");
            ESP_LOGI(TAG, "🔍 检测到键值对格式，可能需要转换为JSON");
        }
        
        // 检查cJSON错误
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("❌ cJSON错误: %s\n", error_ptr);
            ESP_LOGE(TAG, "❌ cJSON错误: %s", error_ptr);
        }
    }
    
    free(data_buffer);
    printf("===========================================\n");
    printf("✅ 服务器消息处理完成\n");
    printf("===========================================\n\n");
    ESP_LOGI(TAG, "✅ 消息处理完成");
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
 * @brief BOOT按钮中断服务例程
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

/**
 * @brief GPIO任务处理函数
 * 
 * 处理BOOT按钮按下事件，发布MQTT消息
 */
static void gpio_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            if (io_num == BOOT_BUTTON_GPIO) {
                // 防抖延时
                vTaskDelay(50 / portTICK_PERIOD_MS);
                
                // 检查按钮是否仍然按下
                if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                    ESP_LOGI(TAG, "BOOT按钮被按下，发布MQTT消息");
                    
                    char *message = create_mqtt_message();
                    if (message && mqtt_client) {
                        ESP_LOGI(TAG, "准备发布按钮触发的消息到主题: %s", MQTT_PUBLISH_TOPIC);
                        ESP_LOGI(TAG, "消息内容: %s", message);
                        ESP_LOGI(TAG, "消息长度: %d 字节", strlen(message));
                        
                        int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_PUBLISH_TOPIC, message, strlen(message), 1, 0);
                        if (msg_id >= 0) {
                            ESP_LOGI(TAG, "✅ 发布消息成功，msg_id=%d", msg_id);
                        } else {
                            ESP_LOGE(TAG, "❌ 发布消息失败，错误码=%d", msg_id);
                        }
                        free(message);
                    } else {
                        ESP_LOGE(TAG, "❌ 无法发布消息 - message=%p, mqtt_client=%p", message, mqtt_client);
                    }
                }
                
                // 等待按钮释放
                while(gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }
            }
        }
    }
}

/**
 * @brief 初始化GPIO和按钮中断
 */
static void init_gpio(void)
{
    gpio_config_t io_conf = {};
    
    // 配置BOOT按钮（GPIO0）为输入，带上拉
    io_conf.intr_type = GPIO_INTR_NEGEDGE;  // 下降沿触发
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
    
    // 创建GPIO事件队列
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    
    // 创建GPIO任务
    xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);
    
    // 安装GPIO中断服务
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOOT_BUTTON_GPIO, gpio_isr_handler, (void*) BOOT_BUTTON_GPIO);
    
    ESP_LOGI(TAG, "GPIO初始化完成，BOOT按钮配置在GPIO%d", BOOT_BUTTON_GPIO);
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
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        printf("\n🎉🎉🎉 MQTT连接成功！🎉🎉🎉\n");
        ESP_LOGI(TAG, "MQTT连接成功");
        
        // 构建订阅主题：/public/striped-kind-tiger/result/ + 设备ID
        char subscribe_topic[128];
        snprintf(subscribe_topic, sizeof(subscribe_topic), "%s%s", MQTT_SUBSCRIBE_TOPIC_PREFIX, device_id);
        
        printf("📡 准备订阅主题: %s\n", subscribe_topic);
        ESP_LOGI(TAG, "📡 准备订阅主题: %s", subscribe_topic);
        
        // 自动订阅指定主题
        msg_id = esp_mqtt_client_subscribe(client, subscribe_topic, 1);
        if (msg_id >= 0) {
            printf("✅ 订阅请求发送成功，msg_id=%d\n", msg_id);
            ESP_LOGI(TAG, "✅ 订阅请求发送成功: %s, msg_id=%d", subscribe_topic, msg_id);
        } else {
            printf("❌ 订阅请求发送失败，错误码=%d\n", msg_id);
            ESP_LOGE(TAG, "❌ 订阅请求发送失败: %s, 错误码=%d", subscribe_topic, msg_id);
        }
        
        // 发布设备上线消息
        char *online_message = create_mqtt_message();
        if (online_message) {
            printf("📤 准备发布设备上线消息到主题: %s\n", MQTT_PUBLISH_TOPIC);
            printf("📝 上线消息内容: %s\n", online_message);
            ESP_LOGI(TAG, "准备发布设备上线消息到主题: %s", MQTT_PUBLISH_TOPIC);
            ESP_LOGI(TAG, "上线消息内容: %s", online_message);
            
            msg_id = esp_mqtt_client_publish(client, MQTT_PUBLISH_TOPIC, online_message, strlen(online_message), 1, 0);
            if (msg_id >= 0) {
                printf("✅ 设备上线消息发布成功，msg_id=%d\n", msg_id);
                ESP_LOGI(TAG, "✅ 设备上线消息发布成功，msg_id=%d", msg_id);
            } else {
                printf("❌ 设备上线消息发布失败，错误码=%d\n", msg_id);
                ESP_LOGE(TAG, "❌ 设备上线消息发布失败，错误码=%d", msg_id);
            }
            free(online_message);
        }
        printf("🔔 等待服务器返回消息...\n\n");
        
        break;
        
    case MQTT_EVENT_DISCONNECTED:
        printf("💔 MQTT连接断开！\n");
        ESP_LOGI(TAG, "MQTT连接断开");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        printf("🎯 主题订阅成功！msg_id=%d\n", event->msg_id);
        printf("✅ 现在可以接收服务器返回的消息了\n");
        ESP_LOGI(TAG, "主题订阅成功, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_UNSUBSCRIBED:
        printf("📤 取消订阅成功, msg_id=%d\n", event->msg_id);
        ESP_LOGI(TAG, "取消订阅成功, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_PUBLISHED:
        printf("📡 消息发布成功, msg_id=%d\n", event->msg_id);
        ESP_LOGI(TAG, "消息发布成功, msg_id=%d", event->msg_id);
        break;
        
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "🔔 收到MQTT消息事件!");
        ESP_LOGI(TAG, "📊 消息统计 - 主题长度: %d, 数据长度: %d", event->topic_len, event->data_len);
        
        if (event->topic_len <= 0) {
            ESP_LOGE(TAG, "❌ 主题长度无效: %d", event->topic_len);
            break;
        }
        
        if (event->data_len <= 0) {
            ESP_LOGE(TAG, "❌ 数据长度无效: %d", event->data_len);
            break;
        }
        
        // 创建主题字符串
        char *topic_buffer = malloc(event->topic_len + 1);
        if (topic_buffer) {
            memcpy(topic_buffer, event->topic, event->topic_len);
            topic_buffer[event->topic_len] = '\0';
            
            ESP_LOGI(TAG, "📝 原始主题内容: %s", topic_buffer);
            
            // 直接打印原始数据（防止非null结尾字符串）
            ESP_LOGI(TAG, "📦 原始数据内容 (%d字节):", event->data_len);
            printf(">>> ");
            for (int i = 0; i < event->data_len; i++) {
                printf("%c", event->data[i]);
            }
            printf(" <<<\n");
            
            // 使用新的处理函数
            process_server_response(topic_buffer, event->data, event->data_len);
            
            free(topic_buffer);
        } else {
            ESP_LOGE(TAG, "❌ 内存分配失败，无法处理消息 - 需要 %d 字节", event->topic_len + 1);
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
/**
 * @brief 发送遗嘱消息（设备离线通知）
 */
static void send_last_will_message(void)
{
    if (mqtt_client) {
        char *last_will_message = create_mqtt_message();
        if (last_will_message) {
            ESP_LOGI(TAG, "准备发送遗嘱消息到主题: %s", MQTT_LAST_WILL_TOPIC);
            ESP_LOGI(TAG, "遗嘱消息内容: %s", last_will_message);
            ESP_LOGI(TAG, "遗嘱消息长度: %d 字节", strlen(last_will_message));
            
            int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_LAST_WILL_TOPIC, last_will_message, strlen(last_will_message), 1, 0);
            if (msg_id >= 0) {
                ESP_LOGI(TAG, "✅ 发送遗嘱消息成功，msg_id=%d", msg_id);
            } else {
                ESP_LOGE(TAG, "❌ 发送遗嘱消息失败，错误码=%d", msg_id);
            }
            free(last_will_message);
            
            // 等待消息发送完成
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

/**
 * @brief 安全关机处理函数
 * 
 * 在设备重启或关机前执行清理操作
 */
static void safe_shutdown_handler(void)
{
    ESP_LOGI(TAG, "执行安全关机程序...");
    
    // 发送遗嘱消息
    send_last_will_message();
    
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
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .session.last_will = {
            .topic = MQTT_LAST_WILL_TOPIC,
            .msg = last_will_message,
            .msg_len = last_will_message ? strlen(last_will_message) : 0,
            .qos = 1,
            .retain = 0,
        },
        .session.keepalive = 60,
        .network.reconnect_timeout_ms = 5000,
        .network.timeout_ms = 5000,
    };

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
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    
    // 注册重启钩子函数，确保在重启前发送遗嘱消息
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
/**
 * @brief 监控任务 - 检查MQTT连接状态和消息计数
 */
static void monitor_task(void* arg)
{
    static int heartbeat_count = 0;
    
    while(1) {
        vTaskDelay(30000 / portTICK_PERIOD_MS); // 每30秒检查一次
        
        heartbeat_count++;
        printf("\n💓 心跳检查 #%d (运行时间: %d分钟)\n", heartbeat_count, heartbeat_count/2);
        printf("📊 统计信息:\n");
        printf("   - 已处理服务器消息数: %d\n", message_received_count);
        printf("   - 可用内存: %" PRIu32 " bytes\n", esp_get_free_heap_size());
        printf("   - MQTT客户端状态: %s\n", mqtt_client ? "已初始化" : "未初始化");
        printf("   - 设备ID: %s\n", device_id);
        
        if (message_received_count == 0 && heartbeat_count > 2) {
            printf("⚠️  警告：设备启动超过1分钟但未收到服务器消息！\n");
            printf("🔍 可能的原因：\n");
            printf("   1. 服务器未向设备订阅的主题发送消息\n");
            printf("   2. 订阅主题配置错误\n");
            printf("   3. MQTT服务器连接问题\n");
            printf("   4. 网络连接不稳定\n");
            printf("💡 建议：尝试按下BOOT按钮发送测试消息\n");
        }
        
        if (mqtt_client && message_received_count == 0) {
            printf("🔔 提示：按下BOOT按钮发送测试消息到服务器\n");
        }
        printf("\n");
    }
}

void app_main(void)
{
    printf("\n🚀🚀🚀 ESP32 MQTT 设备启动 🚀🚀🚀\n");
    printf("===========================================\n");
    
    ESP_LOGI(TAG, "[APP] 应用程序启动...");
    ESP_LOGI(TAG, "[APP] 可用内存: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF版本: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    printf("📶 正在建立网络连接...\n");
    /* 
     * 配置Wi-Fi或以太网连接（在menuconfig中选择）
     * 详细信息请参阅examples/protocols/README.md中的"建立Wi-Fi或以太网连接"部分
     */
    ESP_ERROR_CHECK(example_connect());
    printf("✅ 网络连接成功！\n");

    // 初始化GPIO和BOOT按钮
    printf("🔘 初始化BOOT按钮...\n");
    init_gpio();
    
    // 启动MQTT应用程序
    printf("📡 启动MQTT客户端...\n");
    mqtt_app_start();
    
    // 创建监控任务
    xTaskCreate(monitor_task, "monitor_task", 4096, NULL, 5, NULL);
    
    printf("===========================================\n");
    printf("✅ MQTT设备配置完成\n");
    printf("===========================================\n");
    printf("📋 功能说明:\n");
    printf("1. 🔗 自动连接MQTT服务器并订阅主题\n");
    printf("2. 🔘 按下BOOT按钮发布消息\n");
    printf("3. 💬 实时显示服务器返回消息\n");
    printf("4. 🔄 重启/关机前自动发送遗嘱消息\n");
    printf("5. 🛡️ 安全回路保护确保消息发送\n");
    printf("===========================================\n");
    printf("🔔 等待MQTT连接和消息...\n\n");
    
    ESP_LOGI(TAG, "=== MQTT设备配置完成 ===");
    ESP_LOGI(TAG, "功能说明:");
    ESP_LOGI(TAG, "1. 自动连接MQTT服务器并订阅主题");
    ESP_LOGI(TAG, "2. 按下BOOT按钮发布消息");
    ESP_LOGI(TAG, "3. 重启/关机前自动发送遗嘱消息");
    ESP_LOGI(TAG, "4. 安全回路保护确保消息发送");
    ESP_LOGI(TAG, "=====================");
}
