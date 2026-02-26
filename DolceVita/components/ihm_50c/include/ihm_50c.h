#ifndef _IHM_50C_H
#define _IHM_50C_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct ihm_50c_t ihm_50c_t;

    typedef struct
    {
        uint16_t width;
        uint16_t height;
        uint32_t pclk_hz;
        bool use_psram;
    } ihm_50c_display_cfg_t;

    typedef struct
    {
        int pin_scl;
        int pin_sda;
        int pin_rst;
        int pin_int;
        uint8_t i2c_addr;
        uint32_t i2c_freq;
        uint8_t i2c_port;
    } ihm_50c_touch_cfg_t;

    typedef struct
    {
        int pin_backlight;
        uint8_t pwm_timer;
        uint32_t pwm_freq;
        uint8_t pwm_channel;
        uint8_t pwm_duty;
    } ihm_50c_backlight_cfg_t;

    typedef struct
    {
        ihm_50c_display_cfg_t display;
        ihm_50c_touch_cfg_t touch;
        ihm_50c_backlight_cfg_t backlight;
    } ihm_50c_config_t;

    typedef struct
    {
        bool backlight_ready : 1;
        bool touch_ready : 1;
        bool display_ready : 1;
        bool initialized : 1;
    } ihm_50c_state_t;

    ihm_50c_t *ihm_50c_create(void);
    esp_err_t ihm_50c_backlight_config(ihm_50c_t *ctx, const ihm_50c_backlight_cfg_t *cfg);
    esp_err_t ihm_50c_touch_config(ihm_50c_t *ctx, const ihm_50c_touch_cfg_t *cfg);
    esp_err_t ihm_50c_display_config(ihm_50c_t *ctx, const ihm_50c_display_cfg_t *cfg);

    esp_err_t ihm_50c_init(ihm_50c_t *ctx);
    esp_err_t ihm_50c_deinit(ihm_50c_t *ctx);

    esp_err_t ihm_50c_set_brightness(ihm_50c_t *ctx, uint8_t level_percent);

    bool ihm_50c_touch_is_pressed(ihm_50c_t *ctx);
    esp_err_t ihm_50c_touch_get_coords(ihm_50c_t *ctx, uint16_t *x, uint16_t *y);

    lv_display_t *ihm_50c_get_display(ihm_50c_t *ctx);
    lv_indev_t *ihm_50c_get_touch_indev(ihm_50c_t *ctx);

#ifdef __cplusplus
}
#endif

#endif //_IHM_50C_H
