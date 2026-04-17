#include "lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "config.h"

static const char *TAG = "[LVGL_PORT]";

/* =====================================================================
 * Estado interno
 * ===================================================================== */
static esp_timer_handle_t s_tick_timer  = NULL;
static TaskHandle_t       s_task_handle = NULL;
static SemaphoreHandle_t  s_mux         = NULL;
static bool               s_ready       = false;

/* Cache de touch — preenchido pela touch_task, consumido pelo _touch_cb.
 * Evita I2C bloqueante dentro do lock do LVGL. */
static struct {
    volatile int32_t x;
    volatile int32_t y;
    volatile bool    pressed;
    SemaphoreHandle_t mutex;
} s_touch = { 0, 0, false, NULL };

/* Handle do touch para a touch_task */
static esp_lcd_touch_handle_t s_touch_handle = NULL;

/* =====================================================================
 * API pública — lock / unlock
 * ===================================================================== */
void lvgl_port_lock(void)
{
    if (s_mux) xSemaphoreTakeRecursive(s_mux, portMAX_DELAY);
}

void lvgl_port_unlock(void)
{
    if (s_mux) xSemaphoreGiveRecursive(s_mux);
}

bool lvgl_port_lock_timeout(uint32_t timeout_ms)
{
    if (!s_mux) return false;
    TickType_t ticks = (timeout_ms == 0) ? 0 : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_mux, ticks) == pdTRUE;
}

bool         lvgl_port_is_ready(void)       { return s_ready; }
TaskHandle_t lvgl_port_get_task_handle(void){ return s_task_handle; }

/* =====================================================================
 * Touch task — roda FORA do lock do LVGL
 *
 * Problema resolvido:
 *   O LVGL chama _touch_cb dentro de lv_timer_handler, que está
 *   dentro de lvgl_port_lock(). O esp_lcd_touch_read_data() faz uma
 *   transação I2C bloqueante (~1-3 ms a 400 kHz). Com o período
 *   padrão do indev (igual ao tick = 2 ms), isso gera ~500 reads/s,
 *   segurando o mutex por rajadas contínuas e causando latência no
 *   VSync → glitch visual ao toque.
 *
 * Solução:
 *   Uma task dedicada faz o read_data a 60 Hz (16 ms).
 *   O resultado fica em s_touch (protegido por mutex leve).
 *   O _touch_cb apenas copia os valores — zero I2C dentro do lock.
 * ===================================================================== */
static void _touch_task(void *arg)
{
    esp_lcd_touch_point_data_t pt;
    uint8_t cnt = 0;

    while (1) {
        /* Lê o GT911 via I2C — bloqueante, mas fora do lock LVGL */
        esp_lcd_touch_read_data(s_touch_handle);

        if (esp_lcd_touch_get_data(s_touch_handle, &pt, &cnt, 1) == ESP_OK
                && cnt > 0) {
            xSemaphoreTake(s_touch.mutex, portMAX_DELAY);
            s_touch.x       = (int32_t)pt.x;
            s_touch.y       = (int32_t)pt.y;
            s_touch.pressed = true;
            xSemaphoreGive(s_touch.mutex);
        } else {
            xSemaphoreTake(s_touch.mutex, portMAX_DELAY);
            s_touch.pressed = false;
            xSemaphoreGive(s_touch.mutex);
        }

        /* 60 Hz — suave para UI médica, sem sobrecarga no I2C */
        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

/* =====================================================================
 * Touch callback — chamado pelo LVGL dentro do lock
 * Apenas copia valores do cache, zero I2C, zero bloqueio.
 * ===================================================================== */
static void _touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);

    if (xSemaphoreTake(s_touch.mutex, 0) == pdTRUE) {
        /* timeout=0: se o mutex estiver ocupado pela touch_task,
           mantém o último estado conhecido sem bloquear */
        if (s_touch.pressed) {
            data->point.x = s_touch.x;
            data->point.y = s_touch.y;
            data->state   = LV_INDEV_STATE_PRESSED;
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
        xSemaphoreGive(s_touch.mutex);
    }
    /* Se não conseguiu o mutex, mantém data como está (estado anterior) */
}

/* =====================================================================
 * Timer de tick — ISR-safe
 * ===================================================================== */
static void _lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

/* =====================================================================
 * VSync callback — ISR do painel RGB
 * Notifica a lvgl_task que o framebuffer está pronto para o próximo flush.
 * ===================================================================== */
static bool IRAM_ATTR _vsync_cb(esp_lcd_panel_handle_t panel,
                                 const esp_lcd_rgb_panel_event_data_t *edata,
                                 void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
    xTaskNotifyFromISR(s_task_handle, 0x01, eSetBits, &need_yield);
    return (need_yield == pdTRUE);
}

/* =====================================================================
 * Flush callback — chamado pelo LVGL quando um frame está pronto
 *
 * Aguarda VSync antes de sinalizar flush_ready, garantindo que o
 * controlador RGB consumiu o framebuffer antes de o LVGL reutilizá-lo.
 * ===================================================================== */
static void _flush_cb(lv_display_t *disp, const lv_area_t *area,
                       uint8_t *px_map)
{
    if (lv_display_flush_is_last(disp)) {
        uint32_t notif_val = 0;
        BaseType_t ok = xTaskNotifyWait(0, ULONG_MAX, &notif_val,
                                         pdMS_TO_TICKS(100));
        if (ok == pdFALSE) {
            static uint32_t s_vsync_err = 0;
            if ((++s_vsync_err % 60) == 0) {
                ESP_LOGW(TAG, "VSync timeout x%lu — verifique PCLK/timings",
                         (unsigned long)s_vsync_err);
            }
        }
    }
    lv_display_flush_ready(disp);
}

/* =====================================================================
 * LVGL task — core 1
 * ===================================================================== */
static void _lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task iniciada (core %d)", xPortGetCoreID());

    while (1) {
        uint32_t delay_ms;

        lvgl_port_lock();
        delay_ms = lv_timer_handler();
        lvgl_port_unlock();

        if (delay_ms > LVGL_TASK_MAX_DELAY_MS) delay_ms = LVGL_TASK_MAX_DELAY_MS;
        if (delay_ms < LVGL_TASK_MIN_DELAY_MS) delay_ms = LVGL_TASK_MIN_DELAY_MS;

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

/* =====================================================================
 * lvgl_port_init
 * ===================================================================== */
esp_err_t lvgl_port_init(ihm_50c_t *ihm)
{
    ESP_RETURN_ON_FALSE(ihm   != NULL, ESP_ERR_INVALID_ARG,   TAG, "ihm nulo");
    ESP_RETURN_ON_FALSE(!s_ready,      ESP_ERR_INVALID_STATE, TAG, "ja inicializado");

    /* 1. Mutexes */
    s_mux = xSemaphoreCreateRecursiveMutex();
    ESP_RETURN_ON_FALSE(s_mux != NULL, ESP_ERR_NO_MEM, TAG, "falha mutex LVGL");

    s_touch.mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_touch.mutex != NULL, ESP_ERR_NO_MEM, TAG, "falha mutex touch");

    /* 2. Core LVGL */
    lv_init();
    ESP_LOGI(TAG, "LVGL v%d.%d.%d", LVGL_VERSION_MAJOR,
             LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    /* 3. Display */
    lv_display_t *disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    ESP_RETURN_ON_FALSE(disp != NULL, ESP_ERR_NO_MEM, TAG, "falha lv_display");
    lv_display_set_flush_cb(disp, _flush_cb);

    void *buf1 = NULL, *buf2 = NULL;
    esp_lcd_panel_handle_t panel = ihm_50c_get_panel_handle(ihm);

#if DISPLAY_DOUBLE_FB
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &buf1, &buf2),
        TAG, "falha framebuffers");
    ESP_LOGI(TAG, "Double FB: buf1=%p buf2=%p", buf1, buf2);
#else
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &buf1, NULL),
        TAG, "falha framebuffer");
    ESP_LOGI(TAG, "Single FB: buf=%p", buf1);
#endif

    lv_display_set_buffers(disp, buf1, buf2,
                            DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(lv_color_t),
                            LV_DISPLAY_RENDER_MODE_DIRECT);

    /* 4. VSync callback */
    const esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = _vsync_cb,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, NULL),
        TAG, "falha vsync cb");

    /* 5. Input device — callback leve (sem I2C) */
    lv_indev_t *indev = lv_indev_create();
    ESP_RETURN_ON_FALSE(indev != NULL, ESP_ERR_NO_MEM, TAG, "falha lv_indev");
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, _touch_cb);
    /* Sem user_data aqui: _touch_cb usa s_touch (cache global) */

    /* 6. Touch task dedicada — faz I2C fora do lock LVGL */
    s_touch_handle = ihm_50c_get_touch_handle(ihm);
    ESP_RETURN_ON_FALSE(s_touch_handle != NULL, ESP_ERR_INVALID_STATE,
                        TAG, "touch handle nulo");

    BaseType_t ret = xTaskCreatePinnedToCore(
        _touch_task,
        "touch_read",
        2048,               /* stack: I2C + estrutura de ponto = ~600 bytes */
        NULL,
        LVGL_TASK_PRIORITY + 1, /* 1 acima da lvgl_task: garante read antes do cb */
        NULL,
        0                   /* core 0: separa do LVGL (core 1) */
    );
    ESP_RETURN_ON_FALSE(ret == pdPASS, ESP_FAIL, TAG, "falha touch_task");

    /* 7. Tick timer */
    const esp_timer_create_args_t tick_args = {
        .callback              = _lvgl_tick_cb,
        .name                  = "lvgl_tick",
        .skip_unhandled_events = true,
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_args, &s_tick_timer),
                        TAG, "falha tick timer");
    ESP_RETURN_ON_ERROR(
        esp_timer_start_periodic(s_tick_timer, LVGL_TICK_PERIOD_MS * 1000ULL),
        TAG, "falha iniciar tick");

    /* 8. LVGL task — core 1 */
    ret = xTaskCreatePinnedToCore(
        _lvgl_task,
        "lvgl_main",
        LVGL_TASK_STACK_SIZE_KB * 1024,
        NULL,
        LVGL_TASK_PRIORITY,
        &s_task_handle,
        1
    );
    ESP_RETURN_ON_FALSE(ret == pdPASS, ESP_FAIL, TAG, "falha lvgl_task");

    s_ready = true;
    ESP_LOGI(TAG, "Port pronto. Display %dx%d, touch@60Hz em core 0.",
             DISPLAY_WIDTH, DISPLAY_HEIGHT);

    return ESP_OK;
}