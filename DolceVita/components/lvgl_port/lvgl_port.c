#include "lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_timer.h"
#include "config.h"

static const char *TAG = "[LVGL_PORT]";

static esp_timer_handle_t lvgl_tick_timer = NULL;
static TaskHandle_t lvgl_task_handle = NULL;
static SemaphoreHandle_t lvgl_mux = NULL;

void lvgl_port_lock(void) {
    if (lvgl_mux) xSemaphoreTakeRecursive(lvgl_mux, portMAX_DELAY);
}

void lvgl_port_unlock(void) {
    if (lvgl_mux) xSemaphoreGiveRecursive(lvgl_mux);
}

static bool IRAM_ATTR lvgl_port_flush_vsync_ready_callback(esp_lcd_panel_handle_t panel_io,
                                                  const esp_lcd_rgb_panel_event_data_t *edata,
                                                  void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
    xTaskNotifyFromISR(lvgl_task_handle, 0x01, eSetBits, &need_yield);
    return (need_yield == pdTRUE);
}

static void lvgl_tick(void *arg) {
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_task(void *arg) {
    uint32_t delay_ms;
    ESP_LOGI(TAG, "LVGL task started");

    while (1) {
        lvgl_port_lock();
        delay_ms = lv_timer_handler();
        lvgl_port_unlock();

        if (delay_ms < LVGL_TASK_MIN_DELAY_MS) delay_ms = LVGL_TASK_MIN_DELAY_MS;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    if (lv_display_flush_is_last(disp)) {
        uint32_t notify_val;
        BaseType_t received = xTaskNotifyWait(0, ULONG_MAX, &notify_val, pdMS_TO_TICKS(100));
        
        if (received == pdFALSE) {
            static uint32_t error_cnt = 0;
            if (++error_cnt >= 60) { 
                ESP_LOGW(TAG, "VSync Sync lost (Check RGB Timings/PCLK)");
                error_cnt = 0;
            }
        }
    }
    lv_display_flush_ready(disp);
}

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
    esp_lcd_touch_point_data_t pt;
    uint8_t cnt = 0;

    esp_lcd_touch_read_data(tp);
    if (esp_lcd_touch_get_data(tp, &pt, &cnt, 1) == ESP_OK && cnt > 0) {
        data->point.x = pt.x;
        data->point.y = pt.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

esp_err_t lvgl_port_init(ihm_50c_t *ihm) {
    if (!ihm) return ESP_ERR_INVALID_ARG;
    
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    lv_init();

    lv_display_t *disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_user_data(disp, ihm);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    void *buf1 = NULL, *buf2 = NULL;
    esp_lcd_panel_handle_t panel = ihm_50c_get_panel_handle(ihm);

    #if DISPLAY_DOUBLE_FB
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &buf1, &buf2));
    #else
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &buf1, NULL));
    #endif

    lv_display_set_buffers(disp, buf1, buf2, DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_DIRECT);

    const esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = lvgl_port_flush_vsync_ready_callback
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, NULL));

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(indev, ihm_50c_get_touch_handle(ihm));
    lv_indev_set_read_cb(indev, lvgl_touch_cb);

    const esp_timer_create_args_t tick_args = {.callback = lvgl_tick, .name = "lvgl_tick"};
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    xTaskCreatePinnedToCore(lvgl_task, "lvgl_main", 8192, NULL, LVGL_TASK_PRIORITY, &lvgl_task_handle, 1);
    
    return ESP_OK;
}