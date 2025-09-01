# 鸽子对讲WebRTC客户端（已实现MQTT客户端）

# 配置教程
- 在 menuconfig 中配置wifi名称和密码（即SSID和password）
- mqtts地址在CONFIG_BROKER_URL定义
- 在连接mqtt信令时，默认需要证书验证，可以在menuconfig中关掉：
    Component config → ESP-TLS → 
    [*] Allow potentially insecure options
        [*] Skip server certificate verification by default
- 在添加esp_peer库，构建时会报错，原因是menuconfig中默认缺少对SRTP的支持，只需要分别在
    Component config → mbedTLS → [*] Support DTLS protocol (all versions)
    Component config → mbedTLS → mbedTLS v3.x related → DTLS-based configurations
  启用第一个后再启用第二个即可
- 最后，配置型号参数，构建烧录即可