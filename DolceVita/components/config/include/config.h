
#ifndef _CONFIG_H
#define _CONFIG_H

// WiFi configuration parameters
#define WIFI_MAX_RETRIES 5
#define WIFI_CRED_MAX_NUM 10
#define WIFI_CRED_MAX_LINE 128

// MQTT configuration parameters
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com:1883"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "esp32_client"
#define MQTT_BROKER_USERNAME "SEI"
#define MQTT_BROKER_KEY "SEI"
#define MQTT_BUFFER_SIZE_LEN 256
#define MQTT_TOPIC "esp32/data"
#define MQTT_KEEPALIVE_INTERVAL 20 // seconds

// SPIFFS configuration parameters
#define SPIFFS_BASE_PATH "/spiffs"
#define SPIFFS_MAX_FILES 5
#define SPIFFS_FULL_PATH_SIZE 64
#define WIFI_CRED_FILE "/spiffs/wifi_creds.txt"

#define PATIENT_DATA_DIR "/spiffs/patients"

#define DISPLAY_WIDTH 800
#define DISPLAY_HEIGHT 480
#define DISPLAY_PIN_BCKL GPIO_NUM_2
#define DISPLAY_PIN_RESET GPIO_NUM_38
#define DISPLAY_TOUCH_ADDRRESS
#define DISPLAY_TOUCH_PIN_SCL GPIO_NUM_20
#define DISPLAY_TOUCH_PIN_SDA GPIO_NUM_19
#define DISPLAY_TOUCH_PIN_INT GPIO_NUM_NC
#define DISPLAY_DOUBLE_FB true
#define DISPLAY_DOUBLE_FB_TEARING true
#define DISPLAY_USE_BOUNCE_BUFFER false

#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE_KB 8
#define LVGL_TASK_PRIORITY 3

#endif // _CONFIG_H