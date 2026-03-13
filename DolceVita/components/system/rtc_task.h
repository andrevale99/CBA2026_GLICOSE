#ifndef SD_TASK_H
#define SD_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

esp_err_t rtc_start_task(void);

#endif