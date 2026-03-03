#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "lvgl.h"
#include "ui.h"
#include "lvgl_port.h"
#include "ihm_50c.h"
#include "config.h"

static const char *TAG = "APP_MAIN";

volatile bool synced = false;

void clear_screen(ihm_50c_t *ctx, uint16_t color)
{
    const int lines_per_block = 40;
    size_t block_size = 800 * lines_per_block * sizeof(uint16_t);

    uint16_t *block_buf = malloc(block_size);
    if (!block_buf)
        return;

    for (int i = 0; i < (800 * lines_per_block); i++)
    {
        block_buf[i] = color;
    }

    for (int y = 0; y < 480; y += lines_per_block)
    {
        ihm_50c_draw_bitmap(ctx, 0, y, 800, y + lines_per_block, block_buf);
    }

    free(block_buf);
}

static void touch_diagnostic_task(void *arg)
{
    ihm_50c_t *ctx = (ihm_50c_t *)arg;

    const int brush_size = 3;
    uint16_t *brush_data = malloc(brush_size * brush_size * sizeof(uint16_t));

    if (!brush_data)
    {
        vTaskDelete(NULL);
    }

    for (int i = 0; i < brush_size * brush_size; i++)
        brush_data[i] = 0x07E0; // Verde

    while (1)
    {
        uint16_t tx, ty;

        if (ihm_50c_touch_is_pressed(ctx))
        {
            if (ihm_50c_touch_get_coords(ctx, &tx, &ty) == ESP_OK)
            {
                ESP_LOGI(TAG, "Touch em: (%d, %d)", tx, ty);
                // ihm_50c_draw_bitmap(ctx, tx, ty, tx + 1, ty + 1, brush_data);
                // ihm_50c_draw_bitmap(ctx, 400, 240, 400 + brush_size, 240 + brush_size, brush_data);
                // ihm_50c_draw_test_pattern(ctx);
                ihm_50c_draw_checkerboard(ctx);
                vTaskDelay(pdMS_TO_TICKS(3000));
                clear_screen(ctx, 0x0000);

                // ihm_50c_draw_bitmap(ctx, tx, ty, tx + brush_size, ty + brush_size, brush_data);
            }
        }

        // if (ihm_50c_touch_get_coords(ctx, &tx, &ty) == ESP_OK)
        // {
        //     ESP_LOGI(TAG, "Touch em: (%d, %d)", tx, ty);
        //     // ihm_50c_draw_bitmap(ctx, tx, ty, tx + 1, ty + 1, brush_data);
        //     // ihm_50c_draw_bitmap(ctx, tx, ty, tx + brush_size, ty + brush_size, brush_data);
        // }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    ihm_50c_t *ihm_device = ihm_50c_create();

    ihm_50c_backlight_cfg_t bl_cfg = {
        .pin_backlight = GPIO_NUM_2,
        .pwm_timer = LEDC_TIMER_0,
        .pwm_freq = 5000,
        .pwm_channel = LEDC_CHANNEL_0,
        .pwm_duty = 0,
    };
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
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT,
        .pclk_hz = 14 * 1000 * 1000,
        .use_psram = true,
    };
    ihm_50c_display_config(ihm_device, &disp_cfg);

    if (ihm_50c_init(ihm_device) != ESP_OK)
    {
        ESP_LOGE(TAG, "Erro hardware.");
        ihm_50c_deinit(ihm_device);
        return;
    }
    lvgl_port_init(ihm_device);

    // 3. Cria a sua Interface de Usuário
    lvgl_port_lock();
    ui_main_create();
    lvgl_port_unlock();

     ihm_50c_set_brightness(ihm_device, 70);

    // O loop principal fica livre para outras tarefas ou apenas espera
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // clear_screen(ihm_device, 0x0000);

    // ihm_50c_draw_test_pattern(ihm_device);

    ihm_50c_set_brightness(ihm_device, 70);

    // xTaskCreatePinnedToCore(touch_diagnostic_task, "TouchDiag", 4096, ihm_device, 5, NULL, 1);

    // while (1)
    // {
    //     vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}