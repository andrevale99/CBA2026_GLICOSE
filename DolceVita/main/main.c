#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "nvs_flash.h"

#include "WiFi.h"
#include "NTP.h"

void app_main(void)
{
    nvs_flash_init();

    wifi_init_station();
    ntp_init("pool.ntp.org");
}