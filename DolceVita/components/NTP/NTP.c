#include "NTP.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include <stdio.h>

static const char *TAG = "[NTP]";

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronization event occurred");
}

esp_err_t ntp_init(ntp_config_t *config)
{
    if (config == NULL || config->server == NULL)
    {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing NTP with server: %s", config->server);

    esp_sntp_config_t sntp_conf = ESP_NETIF_SNTP_DEFAULT_CONFIG(config->server);
    sntp_conf.sync_cb = time_sync_notification_cb;

    esp_err_t ret = esp_netif_sntp_init(&sntp_conf);
    if (ret == ESP_OK)
    {
        config->is_initialized = true;
        ESP_LOGI(TAG, "NTP service initialized successfully");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to initialize SNTP: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t ntp_deinit(ntp_config_t *config)
{
    if (config == NULL || !config->is_initialized)
    {
        ESP_LOGW(TAG, "NTP service is not initialized or already stopped");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Deinitializing NTP service...");
    esp_netif_sntp_deinit();

    if (esp_sntp_enabled())
    {
        ESP_LOGE(TAG, "Failed to stop SNTP service");
        return ESP_FAIL;
    }

    config->is_initialized = false;
    ESP_LOGI(TAG, "NTP service stopped successfully");
    return ESP_OK;
}

esp_err_t ntp_wait_for_sync(ntp_config_t *config)
{
    if (config == NULL || !config->is_initialized)
    {
        ESP_LOGE(TAG, "NTP not initialized. Cannot wait for sync");
        return ESP_ERR_INVALID_STATE;
    }

    for (int retry = 1; retry <= NTP_RETRY_COUNT; retry++)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, NTP_RETRY_COUNT);
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(config->sync_timeout_ms)) == ESP_OK)
        {
            ESP_LOGI(TAG, "Time synced successfully");
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "NTP sync timeout reached");
    return ESP_ERR_TIMEOUT;
}

esp_err_t ntp_get_time(ntp_config_t *config, struct tm *timeinfo)
{
    if (config == NULL || timeinfo == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (ntp_wait_for_sync(config) != ESP_OK)
    {
        return ESP_ERR_TIMEOUT;
    }

    int offset_hours = (int)config->utc_offset - 12;
    char tz_str[16];

    if (offset_hours == 0) {
        sprintf(tz_str, "UTC0");
    } else {
        sprintf(tz_str, "UTC%+d", -offset_hours);
    }

    ESP_LOGI(TAG, "Setting timezone to: %s", tz_str);
    setenv("TZ", tz_str, 1);
    tzset();

    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);

    if (timeinfo->tm_year < (2020 - 1900))
    {
        ESP_LOGE(TAG, "Obtained time is invalid (Epoch default detected)");
        return ESP_FAIL;
    }

    return ESP_OK;
}