# 订阅主题
Topic ： /public/striped-kind-tiger/result/ + "唯一设备ID"
发送数据格式：
{
  "deviceId": "设备唯一ID",
  "type": "sfu"
}

# 发布主题
Topic ： /public/striped-kind-tiger/invoke/
发送数据格式：
{
  "deviceId": "设备唯一ID",
  "type": "sfu"
}

# 遗嘱消息
Topic : /device/end
发送数据格式：
{
  "deviceId": "设备唯一ID",
  "type": "sfu"
}



# ESP32 MQTT SSL证书配置指南

## 概述

根据ESP-IDF官方文档，您的项目已经配置好了MQTT SSL证书验证。以下是如何获取和配置实际SSL证书的详细步骤。

## 已完成的配置

✅ **CMakeLists.txt配置**: 已添加`EMBED_TXTFILES "mqtt_server_cert.pem"`来嵌入证书文件
✅ **代码配置**: 已在`app_main.c`中配置SSL证书验证
✅ **证书声明**: 已添加外部证书文件声明

## 需要您完成的步骤

### 1. 获取MQTT服务器的SSL证书

您需要从您的MQTT broker服务器获取SSL证书。有几种方法：

#### 方法1: 使用OpenSSL获取服务器证书
```bash
# 替换 mqtt.your-server.com:8883 为您的实际MQTT服务器地址和端口
openssl s_client -showcerts -connect mqtt.your-server.com:8883 < /dev/null 2> /dev/null | openssl x509 -outform PEM > main/certs/mqtt_server_cert.pem
```

#### 方法2: 如果您有CA证书
如果您的MQTT broker使用自签名证书或您有CA证书，请将CA证书内容保存为PEM格式。

### 2. 创建证书文件

在`main/certs/`目录下创建`mqtt_server_cert.pem`文件，内容格式如下：
```
-----BEGIN CERTIFICATE-----
[您的证书内容]
-----END CERTIFICATE-----
```

### 3. 验证配置

#### 检查您的MQTT URL配置
确保您的`sdkconfig`文件中的`CONFIG_BROKER_URL`使用了`mqtts://`协议：
```
CONFIG_BROKER_URL="mqtts://您的服务器地址:8883"
```

#### 编译测试
```bash
idf.py build
```

## 证书格式要求

- **格式**: PEM格式（文本格式，以`-----BEGIN CERTIFICATE-----`开头）
- **编码**: UTF-8
- **文件名**: `mqtt_server_cert.pem`
- **位置**: `main/certs/mqtt_server_cert.pem`

## 安全注意事项

1. **证书验证**: 当前配置会验证服务器证书，确保连接的安全性
2. **证书更新**: 如果服务器证书过期或更换，需要更新此文件
3. **域名匹配**: 确保证书的Common Name (CN)与服务器域名匹配

## 可选配置

### 跳过证书验证（仅用于测试）
如果需要临时跳过证书验证（不推荐用于生产环境），可以修改`app_main.c`：
```c
esp_mqtt_client_config_t mqtt_cfg = {
    .broker = {
        .address.uri = CONFIG_BROKER_URL,
        .verification.skip_cert_common_name_check = true
    },
};
```

### 使用自定义CA证书包
如果需要使用ESP32内置的证书包：
```c
esp_mqtt_client_config_t mqtt_cfg = {
    .broker = {
        .address.uri = CONFIG_BROKER_URL,
        .verification.use_global_ca_store = true
    },
};
```

## 故障排除

### 常见错误
1. **证书格式错误**: 确保PEM格式正确
2. **域名不匹配**: 检查证书CN是否与服务器域名匹配
3. **证书过期**: 检查证书有效期

### 调试日志
代码中已启用详细日志，可以查看连接过程中的详细信息：
```
esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
esp_log_level_set("transport", ESP_LOG_VERBOSE);
```

## 参考资料

- [ESP-IDF MQTT SSL官方文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/mqtt.html#verification)
- [ESP-IDF SSL示例](https://github.com/espressif/esp-idf/tree/master/examples/protocols/mqtt/ssl)
- [ESP-TLS服务器验证](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/esp_tls.html#esp-tls-server-verification)


