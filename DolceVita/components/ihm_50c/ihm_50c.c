#include <stdio.h>
#include "ihm_50c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_panel_ops.h" 
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_check.h"
#include "lvgl.h"
#include "driver/gpio.h"

#include "config.h"


static const char *TAG = "IHM_50C";

struct ihm_50c_t
{
    ihm_50c_config_t cfg;

    lv_display_t *disp;
    lv_indev_t *touch_indev;

    esp_lcd_panel_handle_t panel_handle;
    i2c_master_bus_handle_t i2c_bus;

    TaskHandle_t lvgl_task;
    esp_timer_handle_t tick_timer;

    SemaphoreHandle_t lvgl_mutex;

    ihm_50c_state_t state;

};

esp_err_t ihm_50c_backlight_config(ihm_50c_t *ctx,
                                   const ihm_50c_backlight_cfg_t *cfg)
{
    if (!ctx || !cfg)
        return ESP_ERR_INVALID_ARG;

    ctx->cfg.backlight = *cfg;

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = cfg->pwm_timer,
        .freq_hz = cfg->pwm_freq,
        .clk_cfg = LEDC_USE_RC_FAST_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "timer");

    ledc_channel_config_t ch = {
        .gpio_num = cfg->pin_backlight,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = cfg->pwm_channel,
        .timer_sel = cfg->pwm_timer,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch), TAG, "channel");

    ctx->state.backlight_ready = true;
    return ESP_OK;
}

esp_err_t ihm_50c_touch_config(ihm_50c_t *ctx,
                               const ihm_50c_touch_cfg_t *cfg)
{
    if (!ctx || !cfg)
        return ESP_ERR_INVALID_ARG;

    ctx->cfg.touch = *cfg;
    ctx->state.touch_ready = false;

    return ESP_OK;
}

esp_err_t ihm_50c_display_config(ihm_50c_t *ctx,
                                 const ihm_50c_display_cfg_t *cfg)
{
    if (!ctx || !cfg)
        return ESP_ERR_INVALID_ARG;

    ctx->cfg.display = *cfg;
    ctx->state.display_ready = false;

    return ESP_OK;
}

esp_err_t ihm_50c_init(ihm_50c_t *ctx)
{
    if (!ctx)
        return ESP_ERR_INVALID_ARG;

    if (ctx->state.initialized)
    {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, ctx->cfg.backlight.pwm_channel, ctx->cfg.backlight.pwm_duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, ctx->cfg.backlight.pwm_channel));

    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = ctx->cfg.touch.pin_scl,
        .sda_io_num = ctx->cfg.touch.pin_sda,
        .glitch_ignore_cnt = 7,
    };

    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&bus_cfg, &ctx->i2c_bus),
        TAG,
        "i2c bus");


    const esp_lcd_rgb_panel_config_t panel_config = {
    .data_width = LCD_DATA_WIDTH,
    .bits_per_pixel = LCD_BITS_PER_PIXEL,

#if CONFIG_SUNTON_ESP32_DOUBLE_FB
    .num_fbs = 2,
#else
    .num_fbs = 1,
#endif

#if CONFIG_SUNTON_ESP32_USE_BOUNCE_BUFFER
    .bounce_buffer_size_px = 20 * DISPLAY_WIDTH,
    .clk_src = LCD_CLK_SRC_PLL240M,
#else
    .clk_src = LCD_CLK_SRC_PLL160M,
#endif

    .timings = {
#if CONFIG_SUNTON_ESP32_USE_BOUNCE_BUFFER
        .pclk_hz = LCD_PCLK_HZ_BOUNCE,
#else
        .pclk_hz = LCD_PCLK_HZ_NORMAL,
#endif
        .hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH,
        .hsync_back_porch  = LCD_HSYNC_BACK_PORCH,
        .hsync_front_porch = LCD_HSYNC_FRONT_PORCH,
        .vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH,
        .vsync_back_porch  = LCD_VSYNC_BACK_PORCH,
        .vsync_front_porch = LCD_VSYNC_FRONT_PORCH,
        .h_res = ctx->cfg.display.width,
        .v_res = ctx->cfg.display.height,
        .flags = {
            .pclk_active_neg = true,
        },
    },

    .dma_burst_size = LCD_DMA_BURST_SIZE,

    .hsync_gpio_num = LCD_HSYNC_GPIO,
    .vsync_gpio_num = LCD_VSYNC_GPIO,
    .de_gpio_num    = LCD_DE_GPIO,
    .pclk_gpio_num  = LCD_PCLK_GPIO,

    .data_gpio_nums = {
        LCD_PIN_B0, LCD_PIN_B1, LCD_PIN_B2, LCD_PIN_B3, LCD_PIN_B4,
        LCD_PIN_G0, LCD_PIN_G1, LCD_PIN_G2, LCD_PIN_G3, LCD_PIN_G4, LCD_PIN_G5,
        LCD_PIN_R0, LCD_PIN_R1, LCD_PIN_R2, LCD_PIN_R3, LCD_PIN_R4,
    },

    .disp_gpio_num = GPIO_NUM_NC,
    .flags = {
        .fb_in_psram = ctx->cfg.display.use_psram,
    },
};

    // esp_lcd_rgb_panel_config_t panel_cfg = {
    //     .data_width = 16,
    //     .bits_per_pixel = 16,
    //     .num_fbs = ctx->cfg.display.num_framebuffers,
    //     .clk_src = LCD_CLK_SRC_PLL160M,
    //     .timings = {
    //         .pclk_hz = ctx->cfg.display.pclk_hz,
    //         .h_res = ctx->cfg.display.width,
    //         .v_res = ctx->cfg.display.height,
    //     },
    //     .flags.fb_in_psram = ctx->cfg.display.use_psram,
    // };

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_rgb_panel(&panel_config, &ctx->panel_handle),
        TAG,
        "panel");

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_init(ctx->panel_handle),
        TAG,
        "panel init");

    lv_init();

    ctx->disp = lv_display_create(
        ctx->cfg.display.width,
        ctx->cfg.display.height);

    lv_display_set_user_data(ctx->disp, ctx->panel_handle);

    ctx->state.display_ready = true;
    ctx->state.initialized = true;

    ESP_LOGI(TAG, "IHM initialized");
    return ESP_OK;
}

esp_err_t ihm_50c_deinit(ihm_50c_t *ctx)
{
    if (!ctx)
        return ESP_ERR_INVALID_ARG;

    if (ctx->tick_timer)
    {
        esp_timer_stop(ctx->tick_timer);
        esp_timer_delete(ctx->tick_timer);
    }

    if (ctx->panel_handle)
    {
        esp_lcd_panel_del(ctx->panel_handle);
    }

    if (ctx->i2c_bus)
    {
        i2c_del_master_bus(ctx->i2c_bus);
    }

    ctx->state.initialized = false;
    return ESP_OK;
}