#include "rtc_task.h"
#include "config.h"

static const char *TAG = "RTC_TASK";

static void vRTCTask(void *pvArgs)
{


    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t rtc_start_task(void)
{
    xTaskCreate(vRTCTask,
                "RTC_task",
                RTC_TASK_STACK_MEMORY,
                NULL,
                1,
                NULL);

    return ESP_OK;
}