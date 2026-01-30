#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "WiFi.h"
#include "NTP.h"
#include "ds3231.h"

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

    ntp_deinit();

    i2c_master_bus_config_t i2c_mst_config_1 = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = 22,
        .sda_io_num = 21,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    i2c_new_master_bus(&i2c_mst_config_1, &bus_handle);

    esp_err_t ret = ds3231_init(&bus_handle, DS3231_ADDRESS);

    ds3231_set_time(&timeinfo);

    while (1)
    {
        ds3231_get_time(&timeinfo);

        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time of DS3231 is: %s", strftime_buf);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}