#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_master.h"

#include "lvgl.h"
#include "ui.h"
#include "lvgl_port.h"
#include "ihm_50c.h"
#include "ds3231.h"
#include "spiffs_manager.h"
#include "config.h"

static const char *TAG = "APP_MAIN";

/* =====================================================================
 * app_main
 *
 * Ordem de inicialização obrigatória:
 *   1. spiffs_init()      — monta o sistema de arquivos
 *   2. ds3231_init()      — conecta o RTC ao barramento I2C
 *   3. ihm_50c_*          — inicializa display + touch
 *   4. lvgl_port_init()   — inicia LVGL, task, touch task
 *   5. ui_main_create()   — cria a UI (dentro do lock)
 *   6. set_brightness()   — acende o display só após UI pronta
 * ===================================================================== */

void app_main(void)
{
    /* ------------------------------------------------------------------
     * 1. SPIFFS — deve ser montado ANTES de ui_data_init()
     *    que carrega pacientes do SPIFFS.
     * ------------------------------------------------------------------ */
    if (spiffs_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao montar SPIFFS. Dados de pacientes indisponiveis.");
        /* Não fatal: o sistema pode operar sem histórico persistido */
    }

    /* ------------------------------------------------------------------
     * 2. DS3231 — barramento I2C dedicado (porta %d, pinos SCL=%d SDA=%d)
     *
     *    ATENÇÃO: ajuste DS3231_SCL_PIN, DS3231_SDA_PIN e DS3231_I2C_PORT
     *    em config.h para corresponder à sua ligação física.
     *    O touch GT911 usa porta 0 (pinos 19/20) — use porta 1 aqui
     *    a menos que o DS3231 esteja no mesmo barramento físico.
     * ------------------------------------------------------------------ */
    i2c_master_bus_handle_t rtc_bus = NULL;

    i2c_master_bus_config_t rtc_bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = DS3231_I2C_PORT,
        .scl_io_num = DS3231_SCL_PIN,
        .sda_io_num = DS3231_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    if (i2c_new_master_bus(&rtc_bus_cfg, &rtc_bus) != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao criar barramento I2C para DS3231.");
        ESP_LOGE(TAG, "Verifique DS3231_SCL_PIN e DS3231_SDA_PIN em config.h");
        /* Não fatal: RTC indisponível, UI usará tempo zerado */
    }
    else
    {
        if (ds3231_init(&rtc_bus, DS3231_ADDRESS) != ESP_OK)
        {
            ESP_LOGW(TAG, "DS3231 nao encontrado. Verifique cabeamento e endereco 0x%02X.",
                     DS3231_ADDRESS);
            /* Não fatal: timestamps mostrarão 00:00:00 até RTC ser conectado */
        }
        else
        {
            ESP_LOGI(TAG, "DS3231 inicializado.");
        }
    }

    /* ------------------------------------------------------------------
     * 3. Hardware IHM (display RGB + touch GT911)
     * ------------------------------------------------------------------ */
    ihm_50c_t *ihm = ihm_50c_create();
    if (!ihm)
    {
        ESP_LOGE(TAG, "Falha ao alocar contexto IHM.");
        return;
    }

    ihm_50c_backlight_cfg_t bl_cfg = {
        .pin_backlight = DISPLAY_BCKL_PIN,
        .pwm_timer = DISPLAY_BCKL_LEDC_TIMER,
        .pwm_freq = 5000,
        .pwm_channel = DISPLAY_BCKL_LEDC_CHANNEL,
        .pwm_duty = 0,
    };
    ESP_ERROR_CHECK(ihm_50c_backlight_config(ihm, &bl_cfg));

    ihm_50c_touch_cfg_t touch_cfg = {
        .pin_scl = DISPLAY_TOUCH_SCL_PIN,
        .pin_sda = DISPLAY_TOUCH_SDA_PIN,
        .pin_rst = DISPLAY_TOUCH_RESET_PIN,
        .pin_int = DISPLAY_TOUCH_INT_PIN,
        .i2c_addr = DISPLAY_TOUCH_ADDRRESS,
        .i2c_freq = DISPLAY_TOUCH_I2C_FREQ,
        .i2c_port = DISPLAY_TOUCH_I2C_PORT,
    };
    ESP_ERROR_CHECK(ihm_50c_touch_config(ihm, &touch_cfg));

    ihm_50c_display_cfg_t disp_cfg = {
        .width = DISPLAY_WIDTH,
        .height = DISPLAY_HEIGHT,
        .pclk_hz = LCD_PCLK_HZ_NORMAL,
        .use_psram = true,
    };
    ESP_ERROR_CHECK(ihm_50c_display_config(ihm, &disp_cfg));

    if (ihm_50c_init(ihm) != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha na inicializacao do IHM.");
        ihm_50c_deinit(ihm);
        return;
    }

    /* ------------------------------------------------------------------
     * 4. LVGL port (display + touch task + LVGL task)
     * ------------------------------------------------------------------ */
    if (lvgl_port_init(ihm) != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha na inicializacao do LVGL port.");
        ihm_50c_deinit(ihm);
        return;
    }

    /* ------------------------------------------------------------------
     * 5. UI — SEMPRE dentro do lock
     * ------------------------------------------------------------------ */
    lvgl_port_lock();
    ui_main_create();
    lvgl_port_unlock();

    /* ------------------------------------------------------------------
     * 6. Backlight — liga após o primeiro frame ser composto
     * ------------------------------------------------------------------ */
    vTaskDelay(pdMS_TO_TICKS(50));
    ihm_50c_set_brightness(ihm, 70);

    ESP_LOGI(TAG, "Sistema iniciado. UI: core 1 | Touch: core 0.");

    /* Loop principal — livre para WiFi, BLE, MQTT, etc. */
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}