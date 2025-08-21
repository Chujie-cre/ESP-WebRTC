# 鸽子对讲WebRTC客户端（已实现MQTT客户端）









| 支持的目标 | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

# ESP-MQTT 示例应用程序
（有关示例的更多信息，请参阅上级“examples”目录中的 README.md 文件。）

此示例连接到使用 `idf.py menuconfig` 选择的代理 URI（使用 mqtt tcp 传输），并作为演示订阅/取消订阅并在某些主题上发送消息。
（请注意，公共代理由社区维护，因此可能并不总是可用，详情请参阅此[免责声明](https://iot.eclipse.org/getting-started/#sandboxes)）

注意：如果 URI 等于 `FROM_STDIN`，则代理地址将在应用程序启动时从标准输入读取（用于测试）

它使用 ESP-MQTT 库，该库实现了 mqtt 客户端以连接到 mqtt 代理。

## 如何使用示例

### 硬件要求

此示例可以在任何 ESP32 板上执行，唯一需要的接口是 WiFi 和互联网连接。

### 配置项目

* 打开项目配置菜单（`idf.py menuconfig`）
* 在“示例连接配置”菜单下配置 Wi-Fi 或以太网。有关更多详细信息，请参阅 [examples/protocols/README.md](../../README.md) 中的“建立 Wi-Fi 或以太网连接”部分。

### 构建和烧录

构建项目并将其烧录到板上，然后运行监视工具以查看串行输出：

```
idf.py -p PORT flash monitor
```

（要退出串行监视器，请键入 ``Ctrl-]``。）

有关配置和使用 ESP-IDF 构建项目的完整步骤，请参阅入门指南。

## 示例输出

```
I (3714) event: sta ip: 192.168.0.139, mask: 255.255.255.0, gw: 192.168.0.2
I (3714) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (3964) MQTT_CLIENT: Sending MQTT CONNECT message, type: 1, id: 0000
I (4164) MQTT_EXAMPLE: MQTT_EVENT_CONNECTED
I (4174) MQTT_EXAMPLE: sent publish successful, msg_id=41464
I (4174) MQTT_EXAMPLE: sent subscribe successful, msg_id=17886
I (4174) MQTT_EXAMPLE: sent subscribe successful, msg_id=42970
I (4184) MQTT_EXAMPLE: sent unsubscribe successful, msg_id=50241
I (4314) MQTT_EXAMPLE: MQTT_EVENT_PUBLISHED, msg_id=41464
I (4484) MQTT_EXAMPLE: MQTT_EVENT_SUBSCRIBED, msg_id=17886
I (4484) MQTT_EXAMPLE: sent publish successful, msg_id=0
I (4684) MQTT_EXAMPLE: MQTT_EVENT_SUBSCRIBED, msg_id=42970
I (4684) MQTT_EXAMPLE: sent publish successful, msg_id=0
I (4884) MQTT_CLIENT: deliver_publish, message_length_read=19, message_length=19
I (4884) MQTT_EXAMPLE: MQTT_EVENT_DATA
TOPIC=/topic/qos0
DATA=data
I (5194) MQTT_CLIENT: deliver_publish, message_length_read=19, message_length=19
I (5194) MQTT_EXAMPLE: MQTT_EVENT_DATA
TOPIC=/topic/qos0
DATA=data

## 使用 Wi-Fi 连接 ESP32P4

可以在不支持原生 Wi-Fi 外设的目标上使用 Wi-Fi 连接。此示例演示了在测试配置中使用 `esp_wifi_remote` 在 ESP32P4 上的用法，定义为 `sdkconfig.ci.p4_wifi`。此配置需要另一个具有原生 Wi-Fi 支持的 ESP 目标物理连接到 ESP32-P4。

此示例默认使用 [esp_hosted](https://components.espressif.com/components/espressif/esp_hosted) 项目，请参阅其文档以了解更多详细信息。
注意，`esp_hosted` 库目前以明文形式传输 Wi-Fi 凭据。如果对此有顾虑，请在 `esp_wifi_remote` 配置菜单中选择 `eppp` 选项（`CONFIG_ESP_WIFI_REMOTE_LIBRARY_EPPP=y`）并设置主从验证（请参阅 [eppp: 配置主从验证](#eppp)）。

### esp-hosted: 配置从设备项目

您首先需要构建并烧录从设备项目。可以直接从主设备项目执行此操作，这些命令可用于设置从设备目标设备（例如 ESP32C6），构建并烧录从设备项目。在烧录从设备时，您需要按住 RST 按钮以保持主设备（ESP32-P4）处于复位状态。
```
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave set-target esp32c6
idf.py -C managed_components/espressif__esp_hosted/slave/ -B build_slave build flash monitor
```

### esp-hosted: 从设备的示例输出

```
I (348) cpu_start: Unicore app
I (357) cpu_start: Pro cpu start user code
I (357) cpu_start: cpu freq: 160000000 Hz
I (357) app_init: Application information:
I (360) app_init: Project name:     network_adapter
I (365) app_init: App version:      qa-test-full-master-esp32c5-202
I (372) app_init: Compile time:     Aug 30 2024 08:10:15
I (378) app_init: ELF file SHA256:  6220fafe8...
I (383) app_init: ESP-IDF:          v5.4-dev-2600-g1157a27964c-dirt
I (390) efuse_init: Min chip rev:     v0.0
I (395) efuse_init: Max chip rev:     v0.99 
I (400) efuse_init: Chip rev:         v0.1
I (405) heap_init: Initializing. RAM available for dynamic allocation:
I (412) heap_init: At 4082FCD0 len 0004C940 (306 KiB): RAM
I (418) heap_init: At 4087C610 len 00002F54 (11 KiB): RAM
I (424) heap_init: At 50000000 len 00003FE8 (15 KiB): RTCRAM
I (432) spi_flash: detected chip: generic
I (435) spi_flash: flash io: dio
I (440) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (447) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (454) coexist: coex firmware version: 8da3f50af
I (481) coexist: coexist rom version 5b8dcfa
I (481) main_task: Started on CPU0
I (481) main_task: Calling app_main()
I (482) fg_mcu_slave: *********************************************************************
I (491) fg_mcu_slave:                 ESP-Hosted-MCU Slave FW version :: 0.0.6                        
I (501) fg_mcu_slave:                 Transport used :: SDIO only                     
I (510) fg_mcu_slave: *********************************************************************
I (519) fg_mcu_slave: Supported features are:
I (524) fg_mcu_slave: - WLAN over SDIO
I (528) h_bt: - BT/BLE
I (531) h_bt:    - HCI Over SDIO
I (535) h_bt:    - BLE only
I (539) fg_mcu_slave: capabilities: 0xd
I (543) fg_mcu_slave: Supported extended features are:
I (549) h_bt: - BT/BLE (extended)
I (553) fg_mcu_slave: extended capabilities: 0x0
I (563) h_bt: ESP Bluetooth MAC addr: 40:4c:ca:5b:a0:8a
I (564) BLE_INIT: Using main XTAL as clock source
I (574) BLE_INIT: ble controller commit:[7491a85]
I (575) BLE_INIT: Bluetooth MAC: 40:4c:ca:5b:a0:8a
I (581) phy_init: phy_version 310,dde1ba9,Jun  4 2024,16:38:11
I (641) phy: libbtbb version: 04952fd, Jun  4 2024, 16:38:26
I (642) SDIO_SLAVE: Using SDIO interface
I (642) SDIO_SLAVE: sdio_init: sending mode: SDIO_SLAVE_SEND_STREAM
I (648) SDIO_SLAVE: sdio_init: ESP32-C6 SDIO RxQ[20] timing[0]

I (1155) fg_mcu_slave: Start Data Path
I (1165) fg_mcu_slave: Initial set up done
I (1165) slave_ctrl: event ESPInit
```

### esp_hosted: 主设备（ESP32-P4）的示例输出

```
I (1833) sdio_wrapper: Function 0 Blocksize: 512
I (1843) sdio_wrapper: Function 1 Blocksize: 512
I (1843) H_SDIO_DRV: SDIO Host operating in STREAMING MODE
I (1853) H_SDIO_DRV: generate slave intr
I (1863) transport: Received INIT event from ESP32 peripheral
I (1873) transport: EVENT: 12
I (1873) transport: EVENT: 11
I (1873) transport: capabilities: 0xd
I (1873) transport: Features supported are:
I (1883) transport: \t * WLAN
I (1883) transport: \t   - HCI over SDIO
I (1893) transport: \t   - BLE only
I (1893) transport: EVENT: 13
I (1893) transport: ESP board type is : 13 

I (1903) transport: Base transport is set-up

I (1903) transport: Slave chip Id[12]
I (1913) hci_stub_drv: Host BT Support: Disabled
I (1913) H_SDIO_DRV: Received INIT event
I (1923) rpc_evt: EVENT: ESP INIT

I (1923) rpc_wrap: Received Slave ESP Init
I (2703) rpc_core: <-- RPC_Req  [0x116], uid 1
I (2823) rpc_rsp:  --> RPC_Resp [0x216], uid 1
I (2823) rpc_core: <-- RPC_Req  [0x139], uid 2
I (2833) rpc_rsp:  --> RPC_Resp [0x239], uid 2
I (2833) rpc_core: <-- RPC_Req  [0x104], uid 3
I (2843) rpc_rsp:  --> RPC_Resp [0x204], uid 3
I (2843) rpc_core: <-- RPC_Req  [0x118], uid 4
I (2933) rpc_rsp:  --> RPC_Resp [0x218], uid 4
I (2933) example_connect: Connecting to Cermakowifi...
I (2933) rpc_core: <-- RPC_Req  [0x11c], uid 5
I (2943) rpc_evt: Event [0x2b] received
I (2943) rpc_evt: Event [0x2] received
I (2953) rpc_evt: EVT rcvd: Wi-Fi Start
I (2953) rpc_core: <-- RPC_Req  [0x101], uid 6
I (2973) rpc_rsp:  --> RPC_Resp [0x21c], uid 5
I (2973) H_API: esp_wifi_remote_connect
I (2973) rpc_core: <-- RPC_Req  [0x11a], uid 7
I (2983) rpc_rsp:  --> RPC_Resp [0x201], uid 6
I (3003) rpc_rsp:  --> RPC_Resp [0x21a], uid 7
I (3003) example_connect: Waiting for IP(s)
I (5723) rpc_evt: Event [0x2b] received
I (5943) esp_wifi_remote: esp_wifi_internal_reg_rxcb: sta: 0x400309fe
0x400309fe: wifi_sta_receive at /home/david/esp/idf/components/esp_wifi/src/wifi_netif.c:38

I (7573) example_connect: Got IPv6 event: Interface "example_netif_sta" address: fe80:0000:0000:0000:424c:caff:fe5b:a088, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (9943) esp_netif_handlers: example_netif_sta ip: 192.168.0.29, mask: 255.255.255.0, gw: 192.168.0.1
I (9943) example_connect: Got IPv4 event: Interface "example_netif_sta" address: 192.168.0.29
I (9943) example_common: Connected to example_netif_sta
I (9953) example_common: - IPv4 address: 192.168.0.29,
I (9963) example_common: - IPv6 address: fe80:0000:0000:0000:424c:caff:fe5b:a088, type: ESP_IP6_ADDR_IS_LINK_LOCAL
I (9973) mqtt_example: Other event id:7
I (9973) main_task: Returned from app_main()
I (10253) mqtt_example: MQTT_EVENT_CONNECTED
I (10253) mqtt_example: sent publish successful, msg_id=45053
I (10253) mqtt_example: sent subscribe successful, msg_id=34643
I (10263) mqtt_example: sent subscribe successful, msg_id=2358
I (10263) mqtt_example: sent unsubscribe successful, msg_id=57769
I (10453) mqtt_example: MQTT_EVENT_PUBLISHED, msg_id=45053
I (10603) mqtt_example: MQTT_EVENT_SUBSCRIBED, msg_id=34643
I (10603) mqtt_example: sent publish successful, msg_id=0
I (10603) mqtt_example: MQTT_EVENT_SUBSCRIBED, msg_id=2358
I (10613) mqtt_example: sent publish successful, msg_id=0
I (10613) mqtt_example: MQTT_EVENT_UNSUBSCRIBED, msg_id=57769
I (10713) mqtt_example: MQTT_EVENT_DATA
TOPIC=/topic/qos0
DATA=data
I (10863) mqtt_example: MQTT_EVENT_DATA
TOPIC=/topic/qos0
DATA=data
```
