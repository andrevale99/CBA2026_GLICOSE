#include "ui.h"
#include "ui_data.h"
#include "ui_styles.h"
#include "ui_nav.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "[UI]";

/* Timer de sincronizacao com RTC — roda dentro da lvgl_task */
static void _rtc_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    ui_data_update_rtc();
}

/**
 * @brief Ponto de entrada da UI.
 *        Chamado pelo main.c dentro de lvgl_port_lock / lvgl_port_unlock.
 */
void ui_main_create(void)
{
    ESP_LOGI(TAG, "Inicializando UI...");

    /* 1. Estilos LVGL — alocacao estatica, sem heap */
    ui_styles_init();

    /* 2. Data layer — le DS3231 e carrega pacientes do SPIFFS */
    ui_data_init();

    /* 3. Timer de RTC a cada 1 segundo */
    lv_timer_create(_rtc_timer_cb, 1000, NULL);

    /* 4. Carrega tela inicial */
    ui_nav_go(UI_SCREEN_HOME);

    ESP_LOGI(TAG, "UI iniciada. %u paciente(s) carregado(s).",
             ui_data_get_state()->patient_count);
}
