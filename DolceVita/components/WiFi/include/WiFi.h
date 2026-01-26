#ifndef WIFI_H
#define WIFI_H

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "esp_log.h"

#define WIFI_SSID "Cerberus"
#define WIFI_PASSWORD "Lime@302"
#define WIFI_MAX_RETRY_CONNECTION   5

esp_err_t wifi_init_station(void);


#endif
