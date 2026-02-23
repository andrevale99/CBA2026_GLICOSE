#ifndef DS3231_H
#define DS3231_H

#include <time.h>

#include "freertos/FreeRTOS.h"

#include "driver/i2c_master.h"

#include "esp_err.h"
#include "esp_log.h"

#define DS3231_ADDRESS 0x68

#define DS3231_REG_TIME_SECONDS 0x00
#define DS3231_REG_TIME_MINUTES 0x01
#define DS3231_REG_TIME_HOURS   0x02
#define DS3231_REG_DATE         0x04
#define DS3231_REG_MONTH       0x05
#define DS3231_REG_YEAR        0x06

#define DS3231_TIMEOUT_MS 100

esp_err_t ds3231_init(i2c_master_bus_handle_t *handle, uint16_t address);
esp_err_t ds3231_set_time(const struct tm *timeinfo);
esp_err_t ds3231_get_time(struct tm *timeinfo);


#endif