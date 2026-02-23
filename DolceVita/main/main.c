#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "sd_manager.h"

// Pin assignments can be set in menuconfig, see "SD SPI Example Configuration" menu.
// You can also change the pin assignments here by changing the following 4 lines.
#define PIN_NUM_MISO 4
#define PIN_NUM_MOSI 17
#define PIN_NUM_CLK 14
#define PIN_NUM_CS 13

static const char *TAG = "[MAIN]";

sd_manager_config_t sd_config = {
    .mosi_pin = PIN_NUM_MOSI,
    .miso_pin = PIN_NUM_MISO,
    .sclk_pin = PIN_NUM_CLK,
    .cs_pin = PIN_NUM_CS,
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

    ESP_LOGI(TAG, "SD card initialization: %s", esp_err_to_name(sd_init(&sd_config)));

    int cont = 0;
    while (1)
    {
        // First create a file.
        const char *file_hello = MOUNT_POINT "/hello.txt";
        char data[SD_MAX_BUFFER_SIZE];
        snprintf(data, SD_MAX_BUFFER_SIZE, "%s %s %i!\n", "Hello", sd_config.card->cid.name, cont);
        ret = sd_write_file(&sd_config, file_hello, data);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to write file: %s", esp_err_to_name(ret));
            return;
        }

        cont++;
        vTaskDelay(pdMS_TO_TICKS(1000));

        sd_read_file(&sd_config, file_hello, data);
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "Checking if file exists: %s", sd_file_exists(&sd_config, file_hello) ? "Yes" : "No");
        vTaskDelay(pdMS_TO_TICKS(1000));

        if(sd_delete_file(&sd_config, file_hello) == ESP_OK)
        {
            ESP_LOGI(TAG, "File deleted successfully");
            cont = 0;
        }

    }
}