#ifndef DS3231_H
#define DS3231_H

#include "freertos/FreeRTOS.h"

#include "driver/i2c_master.h"

#include "esp_err.h"
#include "esp_log.h"

#define DS3231_ADDRESS 0x68

esp_err_t ds3231_init(i2c_master_bus_handle_t *handle, uint16_t address);

#endif