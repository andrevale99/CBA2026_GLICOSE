
#ifndef _CONFIG_H
#define _CONFIG_H

#define CPU_CORE_0 0
#define CPU_CORE_1 1

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

// Definições do SD

#define SD_MANAGER_CS_PIN 10
#define SD_MANAGER_MOSI_PIN 11
#define SD_MANAGER_MISO_PIN 13
#define SD_MANAGER_SCLK_PIN 12

// Definicoes dos tamanhos das pilhas das tasks
#define RTC_TASK_STACK_MEMORY 4096
#define BLE_TASK_STACK_MEMORY 4096

#endif // _CONFIG_H