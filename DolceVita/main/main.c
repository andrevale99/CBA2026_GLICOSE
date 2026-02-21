#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include <string.h>

#include "config.h"
#include "wifi_lib.h"
#include "NTP.h"

#include "patient_lib.h"
#include "spiffs_manager.h"

static const char *TAG = "[MAIN]";

ntp_config_t ntp_settings = {
    .server = "pool.ntp.org",
    .sync_timeout_ms = 2000,
    .utc_offset = NTP_OFFSET_M3,
    .is_initialized = false};

struct tm timeinfo;

static const storage_driver_t spiffs_driver = {
    .write = (storage_write_fn)spiffs_write_file,
    .read = (storage_read_fn)spiffs_read_file,
    .del = (storage_delete_fn)spiffs_delete_file,
};

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "===== Testing NTP synchronization... =====");

    wifi_net_cred_t creds[] = {
        {.ssid = "brisa-2138502",
         .password = "sapij9f7"},
        {.ssid = "LARS-301-2.4GHz",
         .password = "LARS@ROBOTICA"}};

    ESP_ERROR_CHECK(wifi_init_sta(creds, 2));
    ESP_ERROR_CHECK(wifi_connect());

    ESP_LOGI(TAG, "Starting NTP synchronization...");

    ESP_ERROR_CHECK(ntp_init(&ntp_settings));

    if (ntp_get_time(&ntp_settings, &timeinfo) == ESP_OK)
    {
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time in Brazil is: %s", strftime_buf);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to update time via NTP");
    }

    ESP_ERROR_CHECK(ntp_deinit(&ntp_settings));

    // ESP_LOGI(TAG, "=====Test Patient Library... =====");

    // ESP_ERROR_CHECK(spiffs_init());

    // patient_t p = {0};
    // p.id = 42;
    // strcpy(p.sensor_serial, "ABC123456");

    // ESP_ERROR_CHECK(patient_save(&p, &spiffs_driver));

    // patient_t loaded = {0};
    // ESP_ERROR_CHECK(patient_load(42, &loaded, &spiffs_driver));

    // ESP_LOGI(TAG, "Loaded serial: %s", loaded.sensor_serial);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}