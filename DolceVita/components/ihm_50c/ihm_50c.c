#include <stdio.h>
#include "ihm_50c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

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

#if DISPLAY_DOUBLE_FB
static bool lvgl_port_flush_vsync_ready_callback(esp_lcd_panel_handle_t panel_io, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
    ihm_50c_t *ctx = (ihm_50c_t *)user_ctx;
    if (ctx->lvgl_task)
    {
        vTaskNotifyGiveFromISR(ctx->lvgl_task, &need_yield);
    }
    return (need_yield == pdTRUE);
}
#endif

static void lvgl_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    ihm_50c_t *ctx = (ihm_50c_t *)lv_display_get_user_data(disp);

#if DISPLAY_DOUBLE_FB
    if (lv_display_flush_is_last(disp))
    {
        esp_lcd_panel_draw_bitmap(ctx->panel_handle, 0, 0, ctx->cfg.display.width, ctx->cfg.display.height, px_map);
        ulTaskNotifyValueClear(NULL, ULONG_MAX);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
#else
    esp_lcd_panel_draw_bitmap(ctx->panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
#endif
    lv_display_flush_ready(disp);
}

static void touchpad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);

    esp_lcd_touch_point_data_t data_point;
    uint8_t touchpad_cnt = 0;

    esp_lcd_touch_read_data(tp);
    esp_lcd_touch_get_data(tp, &data_point, &touchpad_cnt, 1);

    if (touchpad_cnt > 0)
    {
        data->point.x = data_point.x;
        data->point.y = data_point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    ESP_LOGI(TAG, "Touch cnt=%d", touchpad_cnt);
}

static inline uint16_t map(uint16_t n, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max)
{
    return (n - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void process_coordinates(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{

    ESP_LOGI(TAG, "Processed coordinates: x=%d, y=%d", *x, *y);
}

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
        .flags.enable_internal_pullup = 1,
    };

    ESP_RETURN_ON_ERROR(
        i2c_new_master_bus(&bus_cfg, &ctx->i2c_bus),
        TAG,
        "i2c bus");

    esp_lcd_rgb_panel_config_t panel_config = {
        .data_width = LCD_DATA_WIDTH,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,

#if DISPLAY_DOUBLE_FB
        .num_fbs = 2,
#else
        .num_fbs = 1,
#endif

#if DISPLAY_USE_BOUNCE_BUFFER
        .bounce_buffer_size_px = 20 * DISPLAY_WIDTH,
        .clk_src = LCD_CLK_SRC_PLL240M,
#else
        .clk_src = LCD_CLK_SRC_PLL160M,
#endif

        .timings = {
#if DISPLAY_USE_BOUNCE_BUFFER
            .pclk_hz = LCD_PCLK_HZ_BOUNCE,
#else
            .pclk_hz = LCD_PCLK_HZ_NORMAL,
#endif
            .hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = LCD_VSYNC_BACK_PORCH,
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
        .de_gpio_num = LCD_DE_GPIO,
        .pclk_gpio_num = LCD_PCLK_GPIO,

        .data_gpio_nums = {
            LCD_PIN_B0,
            LCD_PIN_B1,
            LCD_PIN_B2,
            LCD_PIN_B3,
            LCD_PIN_B4,
            LCD_PIN_G0,
            LCD_PIN_G1,
            LCD_PIN_G2,
            LCD_PIN_G3,
            LCD_PIN_G4,
            LCD_PIN_G5,
            LCD_PIN_R0,
            LCD_PIN_R1,
            LCD_PIN_R2,
            LCD_PIN_R3,
            LCD_PIN_R4,
        },

        .disp_gpio_num = GPIO_NUM_NC,
        .flags = {
            .fb_in_psram = ctx->cfg.display.use_psram,
        },
    };

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_rgb_panel(&panel_config, &ctx->panel_handle),
        TAG,
        "panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(ctx->panel_handle),
                        TAG,
                        "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(ctx->panel_handle),
                        TAG,
                        "panel init");

    esp_lcd_panel_io_i2c_config_t touch_io_cfg = {
        .dev_addr = ctx->cfg.touch.i2c_addr,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 16,
        .lcd_param_bits = 0,
        .scl_speed_hz = ctx->cfg.touch.i2c_freq,
        .flags = {
            .dc_low_on_data = 0,
            .disable_control_phase = 1,
        },
    };

    esp_lcd_panel_io_handle_t tp_io_handle;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(ctx->i2c_bus, &touch_io_cfg, &tp_io_handle), TAG, "Touch IO fail");

    esp_lcd_touch_io_gt911_config_t tp_gt911_config = {
        .dev_addr = ctx->cfg.touch.i2c_addr,
    };

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = ctx->cfg.display.width,
        .y_max = ctx->cfg.display.height,
        .rst_gpio_num = ctx->cfg.touch.pin_rst,
        .int_gpio_num = ctx->cfg.touch.pin_int,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .driver_data = &tp_gt911_config,
        // .process_coordinates = process_coordinates, // callback to fix coordinates between gt911 and display
        .interrupt_callback = NULL,
    };

    esp_lcd_touch_handle_t tp_handle;
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle), TAG, "GT911 fail");

    ctx->state.touch_ready = true;

    lv_init();
    ctx->lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    ctx->disp = lv_display_create(ctx->cfg.display.width, ctx->cfg.display.height);
    lv_display_set_user_data(ctx->disp, ctx);
    lv_display_set_flush_cb(ctx->disp, lvgl_disp_flush);

    ctx->touch_indev = lv_indev_create();
    lv_indev_set_type(ctx->touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(ctx->touch_indev, tp_handle);
    lv_indev_set_read_cb(ctx->touch_indev, touchpad_read_cb);

#if DISPLAY_DOUBLE_FB
    const esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = lvgl_port_flush_vsync_ready_callback,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(ctx->panel_handle, &cbs, ctx));
#endif

    ctx->state.initialized = true;
    ctx->state.display_ready = true;
    ESP_LOGI(TAG, "IHM 5.0c Initialized Successfully");
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

esp_err_t ihm_50c_set_brightness(ihm_50c_t *ctx, uint8_t level_percent)
{
    if (!ctx->state.backlight_ready)
    {
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t duty = (level_percent * 255) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ctx->cfg.backlight.pwm_channel, duty);
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, ctx->cfg.backlight.pwm_channel);
}

esp_err_t ihm_50c_touch_get_coords(ihm_50c_t *ctx, uint16_t *x, uint16_t *y)
{
    if (!ctx->state.touch_ready)
        return ESP_ERR_INVALID_STATE;

    esp_lcd_touch_handle_t tp =
        (esp_lcd_touch_handle_t)lv_indev_get_user_data(ctx->touch_indev);

    esp_lcd_touch_point_data_t point;
    uint8_t cnt = 0;

    ESP_RETURN_ON_ERROR(esp_lcd_touch_read_data(tp), TAG, "touch read");

    esp_err_t err = esp_lcd_touch_get_data(tp, &point, &cnt, 1);
    if (err != ESP_OK)
        return err;

    if (cnt == 0)
        return ESP_ERR_NOT_FOUND;

    *x = point.x;
    *y = point.y;

    return ESP_OK;
}

bool ihm_50c_touch_is_pressed(ihm_50c_t *ctx)
{
    uint16_t x, y;
    return (ihm_50c_touch_get_coords(ctx, &x, &y) == ESP_OK);
}

lv_display_t *ihm_50c_get_display(ihm_50c_t *ctx)
{
    if (!ctx->state.display_ready)
        return NULL;
    return ctx->disp;
}

lv_indev_t *ihm_50c_get_touch_indev(ihm_50c_t *ctx)
{
    if (!ctx->state.touch_ready)
        return NULL;
    return ctx->touch_indev;
}