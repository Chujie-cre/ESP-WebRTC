#ifndef __MQTT_CLIENT_HPP__
#define __MQTT_CLIENT_HPP__

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
#include "esp_mac.h"
#include "cJSON.h"
#include "esp_log.h"
#include "mqtt_client.h"

// MQTT主题定义
#define MQTT_SUBSCRIBE_TOPIC_PREFIX "/public/striped-kind-tiger/result/"
#define MQTT_PUBLISH_TOPIC "/public/striped-kind-tiger/invoke/"
#define MQTT_LAST_WILL_TOPIC "/device/end"

// GPIO定义 - BOOT按钮
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define GPIO_INPUT_PIN_SEL (1ULL << BOOT_BUTTON_GPIO)

#ifdef __cplusplus
extern "C" {
#endif
static void generate_device_id(void);

static char* create_mqtt_message(void);

static void process_server_response(const char* topic, const char* data, int data_len);

static void handle_server_command(const char* command, cJSON* json_data);

static void button_poll_task(void* arg);

static void init_gpio(void);

static void log_error_if_nonzero(const char *message, int error_code);

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

static void safe_shutdown_handler(void);

static void mqtt_app_start(void);

void mqtt_DoNow();
#ifdef __cplusplus
}
#endif

#endif