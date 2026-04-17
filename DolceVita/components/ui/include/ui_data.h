#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>         /* struct tm — padrão do DS3231 do projeto */
#include "esp_err.h"

/* =============================================================
 * CONSTANTES CLÍNICAS
 * ============================================================= */
#define UI_GLUCOSE_HYPO_CRIT    70
#define UI_GLUCOSE_HYPO_WARN    80
#define UI_GLUCOSE_NORMAL_HI   180
#define UI_GLUCOSE_HYPER_CRIT  250

#define UI_MAX_PATIENTS         8
#define UI_MAX_SAMPLES         288    /* 24h @ 5 min */
#define UI_MAX_ALERTS           32
#define UI_PATIENT_NAME_LEN     48
#define UI_AI_TEXT_LEN         128
#define UI_STALE_THRESHOLD_S   120    /* dados > 2 min = obsoleto */

/* =============================================================
 * TIPOS
 * ============================================================= */
typedef enum {
    UI_STATUS_NORMAL   = 0,
    UI_STATUS_WARNING  = 1,
    UI_STATUS_CRITICAL = 2,
    UI_STATUS_OFFLINE  = 3,
} ui_patient_status_t;

typedef enum {
    UI_TREND_RISING_FAST = 0,   /* > +2 mg/dL/min */
    UI_TREND_RISING      = 1,
    UI_TREND_STABLE      = 2,
    UI_TREND_FALLING     = 3,
    UI_TREND_FALLING_FAST= 4,
    UI_TREND_UNKNOWN     = 5,
} ui_glucose_trend_t;

typedef enum {
    UI_ALERT_HYPO_CRIT   = 0,
    UI_ALERT_HYPO_WARN   = 1,
    UI_ALERT_HYPER_CRIT  = 2,
    UI_ALERT_HYPER_WARN  = 3,
    UI_ALERT_SENSOR_FAIL = 4,
    UI_ALERT_STALE_DATA  = 5,
} ui_alert_type_t;

typedef struct {
    uint16_t   value_mgdl;
    struct tm  timestamp;    /* preenchido pelo DS3231 via ds3231_get_time() */
    bool       valid;
} ui_glucose_sample_t;

typedef struct {
    uint8_t               id;
    char                  name[UI_PATIENT_NAME_LEN];
    uint8_t               age;
    ui_patient_status_t   status;
    ui_glucose_trend_t    trend;
    ui_glucose_sample_t   samples[UI_MAX_SAMPLES];
    uint16_t              sample_count;
    uint16_t              sample_head;          /* buffer circular */
    ui_glucose_sample_t   last_sample;
    bool                  active;
    /* IA */
    char                  ai_trend_text[UI_AI_TEXT_LEN];
    uint16_t              ai_predicted_mgdl;
    bool                  ai_data_valid;
} ui_patient_t;

typedef struct {
    ui_alert_type_t  type;
    uint8_t          patient_id;
    uint16_t         value_mgdl;
    struct tm        timestamp;
    bool             acknowledged;
    bool             silenced;
} ui_alert_t;

typedef struct {
    ui_patient_t  patients[UI_MAX_PATIENTS];
    uint8_t       patient_count;
    ui_alert_t    alerts[UI_MAX_ALERTS];
    uint8_t       alert_count;
    uint8_t       active_alert_count;
    struct tm     current_time;     /* atualizado pelo DS3231 */
    bool          system_online;
    bool          offline_mode;
} ui_app_state_t;

/* =============================================================
 * API DO DATA LAYER
 * Implementada em ui_data.c — usa ds3231 e spiffs do projeto
 * ============================================================= */

/**
 * @brief Inicializa o data layer — alocação estática, sem heap.
 *        Carrega pacientes do SPIFFS (/spiffs/patients/).
 */
void ui_data_init(void);

/**
 * @brief Atualiza current_time a partir do DS3231.
 *        Deve ser chamado periodicamente (ex: lv_timer de 1s).
 */
void ui_data_update_rtc(void);

/** Acesso ao estado global (somente leitura pela UI) */
const ui_app_state_t * ui_data_get_state(void);

/** Acesso a paciente por ID */
const ui_patient_t * ui_data_get_patient(uint8_t id);

/** Ações sobre alertas */
void ui_data_ack_alert(uint8_t alert_idx);
void ui_data_silence_alert(uint8_t alert_idx);

/** CRUD de pacientes (persiste no SPIFFS) */
esp_err_t ui_data_add_patient(const char *name, uint8_t age);
esp_err_t ui_data_remove_patient(uint8_t id);
esp_err_t ui_data_update_patient_name(uint8_t id, const char *name);

/**
 * @brief Recebe nova amostra para um paciente (chamado pela camada BLE/sensor).
 *        Classifica status, atualiza tendência, gera alerta se necessário.
 */
void ui_data_push_sample(uint8_t patient_id, uint16_t value_mgdl);

/* =============================================================
 * UTILITÁRIOS (usados pelos arquivos de tela)
 * ============================================================= */

/** Classifica faixa glicêmica */
ui_patient_status_t ui_data_classify(uint16_t mgdl);

/** Seta string "↑↑" / "↑" / "→" / "↓" / "↓↓" */
const char * ui_data_trend_str(ui_glucose_trend_t t);

/** "NORMAL" / "ATENÇÃO" / "CRÍTICO" / "OFFLINE" */
const char * ui_data_status_str(ui_patient_status_t s);

/** "HIPERGLICEMIA CRÍTICA" etc. */
const char * ui_data_alert_str(ui_alert_type_t t);

/**
 * @brief Segundos entre dois struct tm.
 *        Usado para detectar dados obsoletos.
 */
int32_t ui_data_seconds_between(const struct tm *past, const struct tm *now);

/** Formata "14:32" */
void ui_data_fmt_hhmm(const struct tm *t, char *buf, size_t len);

/** Formata "14:32:08" */
void ui_data_fmt_hhmmss(const struct tm *t, char *buf, size_t len);
