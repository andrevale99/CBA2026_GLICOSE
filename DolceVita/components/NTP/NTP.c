#include "NTP.h"

static const char *TAG = "[NTP]";

static time_t now = 0;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

esp_err_t ntp_init(const char *ntp_server)
{
    ESP_LOGI(TAG, "Initializing NTP with server: %s", ntp_server);

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_server);

    config.sync_cb = time_sync_notification_cb; // Note: This is only needed if we want

    esp_netif_sntp_init(&config);

    time(&now);

    return ESP_OK;
}

esp_err_t ntp_wait_for_sync(uint32_t timeout_ms)
{
    esp_err_t err = ESP_ERR_TIMEOUT;
    for (int retry = 0; retry < NTP_RETRY_COUNT; retry++)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, NTP_RETRY_COUNT);
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms)) != ESP_ERR_TIMEOUT)
        {
            err = ESP_OK;
            break;
        }
    }

    return err;
}

void ntp_get_time(struct tm *_timeinfo)
{
    ntp_wait_for_sync(2000);
    
    time(&now);
    localtime_r(&now, _timeinfo);

    _timeinfo->tm_hour += -3; // UTC-3 for Brasilia time zone
    mktime(_timeinfo);
}