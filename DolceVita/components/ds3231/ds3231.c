#include <stdio.h>
#include "ds3231.h"

static const char *TAG = "[DS3231]";

static i2c_master_dev_handle_t deviceHandle = NULL;

static uint8_t dec_to_bcd(int val)
{
    return (uint8_t)((val / 10 * 16) + (val % 10));
}

static int bcd_to_dec(uint8_t val)
{
    return (int)((val / 16 * 10) + (val % 16));
}

esp_err_t ds3231_init(i2c_master_bus_handle_t *handle, uint16_t address)
{

    esp_err_t ret = ESP_FAIL;

    ret = i2c_master_probe(*handle, address, pdMS_TO_TICKS(DS3231_TIMEOUT_MS));
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
            .scl_speed_hz = 100000, // 100 kHz
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

esp_err_t ds3231_set_time(const struct tm *timeinfo)
{
    uint8_t data[8];
    esp_err_t ret = ESP_FAIL;

    data[0] = DS3231_REG_TIME_SECONDS; // endereço inicial (segundos)
    data[1] = dec_to_bcd(timeinfo->tm_sec);
    data[2] = dec_to_bcd(timeinfo->tm_min);
    data[3] = dec_to_bcd(timeinfo->tm_hour);     // modo 24h
    data[4] = dec_to_bcd(timeinfo->tm_wday + 1); // DS3231: 1–7
    data[5] = dec_to_bcd(timeinfo->tm_mday);
    data[6] = dec_to_bcd(timeinfo->tm_mon + 1);    // tm_mon começa em 0
    data[7] = dec_to_bcd(timeinfo->tm_year - 100); // 2025 → 25

    ret = i2c_master_transmit(deviceHandle, data, sizeof(data),
                              pdMS_TO_TICKS(DS3231_TIMEOUT_MS));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao configurar o tempo no DS3231");
        return ret;
    }

    ESP_LOGI(TAG, "Tempo configurado no DS3231 com sucesso");

    return ret;
}

esp_err_t ds3231_get_time(struct tm *timeinfo)
{
    uint8_t data[7];
    esp_err_t ret = ESP_FAIL;

    data[0] = 0x00; // Endereco do registrador de segundos

    ret = i2c_master_transmit_receive(deviceHandle, data, 1, data, 7,
                                      pdMS_TO_TICKS(DS3231_TIMEOUT_MS));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao ler o tempo do DS3231");
        return ret;
    }

    timeinfo->tm_sec = bcd_to_dec(data[0] & 0x7F);
    timeinfo->tm_min = bcd_to_dec(data[1] & 0x7F);
    timeinfo->tm_hour = bcd_to_dec(data[2] & 0x3F); // modo 24h

    timeinfo->tm_wday = bcd_to_dec(data[3] & 0x07) - 1; // DS3231: 1–7 → tm: 0–6
    timeinfo->tm_mday = bcd_to_dec(data[4] & 0x3F);

    timeinfo->tm_mon = bcd_to_dec(data[5] & 0x1F) - 1; // 1–12 → 0–11

    timeinfo->tm_year = bcd_to_dec(data[6]) + 100; // 2000 + ano → desde 1900

    return ret;
}