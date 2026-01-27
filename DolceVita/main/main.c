#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "WiFi.h"
#include "NTP.h"

char *TAG = "[MAIN]";

struct tm timeinfo;

void app_main(void)
{
    nvs_flash_init();

    wifi_init_station();
    ntp_init("pool.ntp.org");

    ntp_get_time(&timeinfo);

    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}