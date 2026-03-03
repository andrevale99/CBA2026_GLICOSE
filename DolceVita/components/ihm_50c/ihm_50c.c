#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ihm_50c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"

#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_lcd_panel_ops.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"

#include "config.h"

static const char *TAG = "IHM_50C";

struct ihm_50c_t
{
    ihm_50c_config_t cfg;
    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_touch_handle_t touch_handle;
    esp_lcd_panel_io_handle_t touch_io_handle;
    i2c_master_bus_handle_t i2c_bus;
    ihm_50c_state_t state;
};

ihm_50c_t *ihm_50c_create(void)
{
    ihm_50c_t *ctx = calloc(1, sizeof(ihm_50c_t));
    if (!ctx)
        ESP_LOGE(TAG, "Falha ao alocar contexto!");

    return ctx;
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
    ctx->state.touch_ready = true;

    return ESP_OK;
}

static inline uint16_t map(uint16_t n, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max)
{
    return (n - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void process_coordinates(esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)
{
    int32_t nx = (int32_t)*x;
    int32_t ny = (int32_t)*y;

    if (nx < 0)
        nx = 0;
    if (nx >= DISPLAY_WIDTH)
        nx = DISPLAY_WIDTH - 1;

    if (ny < 0)
        ny = 0;
    if (ny >= DISPLAY_HEIGHT)
        ny = DISPLAY_HEIGHT - 1;

    *x = (uint16_t)nx;
    *y = (uint16_t)ny;
}

esp_err_t ihm_50c_display_config(ihm_50c_t *ctx,
                                 const ihm_50c_display_cfg_t *cfg)
{
    if (!ctx || !cfg)
        return ESP_ERR_INVALID_ARG;

    ctx->cfg.display = *cfg;
    ctx->state.display_ready = true;

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

    if (!ctx->state.backlight_ready)
    {
        ESP_LOGE(TAG, "Backlight not configured");
        return ESP_ERR_INVALID_STATE;
    }
    if (!ctx->state.touch_ready)
    {
        ESP_LOGE(TAG, "Touch not configured");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ctx->state.display_ready)
    {
        ESP_LOGE(TAG, "Display not configured");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = ctx->cfg.touch.i2c_port,
        .scl_io_num = ctx->cfg.touch.pin_scl,
        .sda_io_num = ctx->cfg.touch.pin_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = 0,
    };

    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &ctx->i2c_bus), TAG, "i2c bus");

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
            .flags.pclk_active_neg = LCD_PLCK_ACTIVE_NEG,
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
        .flags.fb_in_psram = ctx->cfg.display.use_psram,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&panel_config, &ctx->panel_handle), TAG, "panel");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(ctx->panel_handle), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(ctx->panel_handle), TAG, "panel init");

    // uint16_t *fb;
    // esp_lcd_rgb_panel_get_frame_buffer(ctx->panel_handle, 1, (void **)&fb);
    // memset(fb, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));

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

    ESP_LOGI(TAG, "Verificando dispositivo no endereco 0x%02x...", ctx->cfg.touch.i2c_addr);
    esp_err_t probe_err = i2c_master_probe(ctx->i2c_bus, ctx->cfg.touch.i2c_addr, 100);
    if (probe_err != ESP_OK)
    {
        ESP_LOGE(TAG, "Nenhum dispositivo encontrado no endereco 0x%02x! Erro: %s",
                 ctx->cfg.touch.i2c_addr, esp_err_to_name(probe_err));
    }
    else
    {
        ESP_LOGI(TAG, "GT911 encontrado!");
    }

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(ctx->i2c_bus, &touch_io_cfg, &ctx->touch_io_handle), TAG, "Touch IO fail");

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
        .process_coordinates = process_coordinates,
        .interrupt_callback = NULL,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(ctx->touch_io_handle, &tp_cfg, &ctx->touch_handle), TAG, "Touch panel init fail");

    ctx->state.touch_ready = true;
    ctx->state.initialized = true;
    ctx->state.display_ready = true;
    ESP_LOGI(TAG, "IHM 5.0c Initialized Successfully");
    return ESP_OK;
}

esp_err_t ihm_50c_deinit(ihm_50c_t *ctx)
{
    if (!ctx)
        return ESP_ERR_INVALID_ARG;

    if (ctx->touch_handle)
    {
        esp_lcd_touch_del(ctx->touch_handle);
        ctx->touch_handle = NULL;
    }

    if (ctx->panel_handle)
    {
        esp_lcd_panel_del(ctx->panel_handle);
        ctx->panel_handle = NULL;
    }

    if (ctx->i2c_bus)
    {
        i2c_del_master_bus(ctx->i2c_bus);
        ctx->i2c_bus = NULL;
    }

    ctx->state.initialized = false;
    ctx->state.display_ready = false;
    ctx->state.touch_ready = false;
    ctx->state.backlight_ready = false;
    free(ctx);
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

    esp_lcd_touch_handle_t tp = ctx->touch_handle;

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

esp_lcd_panel_handle_t ihm_50c_get_panel_handle(ihm_50c_t *ctx)
{
    return ctx->panel_handle;
}

esp_lcd_touch_handle_t ihm_50c_get_touch_handle(ihm_50c_t *ctx)
{
    return ctx->touch_handle;
}

esp_err_t ihm_50c_draw_bitmap(ihm_50c_t *ctx, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    if (!ctx || !ctx->state.display_ready || !ctx->panel_handle)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // ESP_LOGW(TAG, "Drawing bitmap: (%d, %d) - (%d, %d)", x_start, y_start, x_end, y_end);

    if (x_start < 0)
        x_start = 0;
    if (y_start < 0)
        y_start = 0;

    if (x_end > 800)
        x_end = 800;
    if (y_end > 480)
        y_end = 480;

    if (x_start >= x_end || y_start >= y_end)
    {
        return ESP_OK;
    }

    return esp_lcd_panel_draw_bitmap(ctx->panel_handle, x_start, y_start, x_end, y_end, color_data);
}

esp_err_t ihm_50c_draw_test_pattern(ihm_50c_t *ctx)
{
    if (!ctx)
        return ESP_ERR_INVALID_ARG;

    const int lines_per_block = 20;
    uint16_t *block_buf = malloc(DISPLAY_WIDTH * lines_per_block * sizeof(uint16_t));
    if (!block_buf)
        return ESP_ERR_NO_MEM;

    uint16_t colors[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
    int bar_width = DISPLAY_WIDTH / 4;

    ESP_LOGI(TAG, "Filling block buffer with test pattern...");
    for (int y = 0; y < lines_per_block; y++)
    {
        for (int x = 0; x < DISPLAY_WIDTH; x++)
        {
            block_buf[y * DISPLAY_WIDTH + x] = colors[x / bar_width];
        }
    }
    ESP_LOGI(TAG, "Drawing test pattern...");
    for (int y = 0; y < DISPLAY_HEIGHT; y += lines_per_block)
    {
        ihm_50c_draw_bitmap(ctx, 0, y, DISPLAY_WIDTH, y + lines_per_block, block_buf);
    }
    ESP_LOGI(TAG, "Test pattern drawn.");

    free(block_buf);
    return ESP_OK;
}
esp_err_t ihm_50c_draw_checkerboard(ihm_50c_t *ctx)
{
    if (!ctx)
        return ESP_ERR_INVALID_ARG;

    const int box_size = 40;
    const int lines_per_block = 40;

    uint16_t *block_buf = malloc(DISPLAY_WIDTH * lines_per_block * sizeof(uint16_t));
    if (!block_buf)
        return ESP_ERR_NO_MEM;

    uint16_t color1 = 0xF800; // Vermelho
    uint16_t color2 = 0x07E0; // Verde

    for (int y_start = 0; y_start < DISPLAY_HEIGHT; y_start += lines_per_block)
    {

        for (int y = 0; y < lines_per_block; y++)
        {
            int global_y = y_start + y;
            for (int x = 0; x < DISPLAY_WIDTH; x++)
            {
                if (((x / box_size) + (global_y / box_size)) % 2 == 0)
                {
                    block_buf[y * DISPLAY_WIDTH + x] = color1;
                }
                else
                {
                    block_buf[y * DISPLAY_WIDTH + x] = color2;
                }
            }
        }

        ihm_50c_draw_bitmap(ctx, 0, y_start, DISPLAY_WIDTH, y_start + lines_per_block, block_buf);
    }
    free(block_buf);
    ESP_LOGI(TAG, "Checkerboard pattern drawn.");

    return ESP_OK;
}