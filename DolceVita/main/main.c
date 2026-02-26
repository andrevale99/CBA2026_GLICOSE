#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/ledc.h"
#include "driver/gpio.h"

#include "ihm_50c.h"
#include "config.h"

static const char *TAG = "APP_MAIN";

static ihm_50c_t *ihm_device = NULL;


static void touch_monitor_task(void *arg)
{
    ihm_50c_t *ctx = (ihm_50c_t *)arg;

    ihm_50c_set_brightness(ctx, 10);

    ESP_LOGI(TAG, "Iniciando monitoramento de toque...");

    while (1)
    {
        uint16_t tx = 0, ty = 0;

        if (ihm_50c_touch_is_pressed(ctx))
        {
            if (ihm_50c_touch_get_coords(ctx, &tx, &ty) == ESP_OK)
            {
                ESP_LOGI(TAG, "Toque detectado em: X=%d, Y=%d", tx, ty);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    ihm_device = ihm_50c_create();

 
    ihm_50c_backlight_cfg_t bl_cfg = {
        .pin_backlight = GPIO_NUM_2,
        .pwm_timer = LEDC_TIMER_0,
        .pwm_freq = 5000,
        .pwm_channel = LEDC_CHANNEL_0,
        .pwm_duty = 0};

    ihm_50c_backlight_config(ihm_device, &bl_cfg);


    ihm_50c_touch_cfg_t touch_cfg = {
        .pin_scl = DISPLAY_TOUCH_SCL_PIN,
        .pin_sda = DISPLAY_TOUCH_SDA_PIN,
        .pin_rst = DISPLAY_TOUCH_RESET_PIN,
        .pin_int = DISPLAY_TOUCH_INT_PIN,
        .i2c_addr = DISPLAY_TOUCH_ADDRRESS,
        .i2c_freq = DISPLAY_TOUCH_I2C_FREQ,
        .i2c_port = DISPLAY_TOUCH_I2C_PORT,
    };

    ihm_50c_touch_config(ihm_device, &touch_cfg);


    ihm_50c_display_cfg_t disp_cfg = {
        .width = 800,
        .height = 480,
        .pclk_hz = 14 * 1000 * 1000,
        .use_psram = true};

    ihm_50c_display_config(ihm_device, &disp_cfg);

    if (ihm_50c_init(ihm_device) != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha crítica na inicialização.");
        return;
    }

    ESP_LOGI(TAG, "Hardware OK. Pressione a tela para testar.");

    xTaskCreatePinnedToCore( touch_monitor_task,"TouchMonitor",4096,ihm_device, 4, NULL,0);
}