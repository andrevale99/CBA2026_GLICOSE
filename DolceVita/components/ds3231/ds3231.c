#include <stdio.h>
#include "ds3231.h"

static const char *TAG = "[DS3231]";

static i2c_master_dev_handle_t deviceHandle = NULL;

esp_err_t ds3231_init(i2c_master_bus_handle_t *handle, uint16_t address)
{

    esp_err_t ret = ESP_FAIL;

    ret = i2c_master_probe(*handle, address, pdMS_TO_TICKS(1000));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "DS3231 nao encontrado no endereco 0x%02X", address);
        return ret;
    }

    ESP_LOGI(TAG, "DS3231 Encontrado no endereco 0x%02X", address);

    i2c_device_config_t deviceConfig =
        {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = 400000, // 400 kHz
        };

    ret = i2c_master_bus_add_device(*handle, &deviceConfig, &deviceHandle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao adicionar o dispositivo DS3231 ao barramento I2C");
        return ret;
    }

    ESP_LOGI(TAG, "Dispositivo DS3231 adicionado ao barramento I2C com sucesso");

    return ESP_OK;
}