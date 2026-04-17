#include "ui_data.h"
#include "ds3231.h"
#include "spiffs_manager.h"
#include "config.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *TAG = "[UI_DATA]";

/* Estado global — alocação estática, zero heap */
static ui_app_state_t s_state;

/* =============================================================
 * PERSISTÊNCIA NO SPIFFS
 * Formato do arquivo: "/spiffs/patients/NNN.txt"
 *   Linha 1: nome
 *   Linha 2: idade
 * ============================================================= */

static void _load_patients_from_spiffs(void)
{
    char path[SPIFFS_FULL_PATH_SIZE];
    char *buf = NULL;
    size_t len = 0;

    s_state.patient_count = 0;

    for (uint8_t id = 0; id < UI_MAX_PATIENTS; id++) {
        snprintf(path, sizeof(path), "patients/%03u.txt", id);
        if (!spiffs_file_exists(path)) continue;

        if (spiffs_read_file(path, &buf, &len) != ESP_OK || !buf) continue;

        ui_patient_t *pt = &s_state.patients[s_state.patient_count];
        memset(pt, 0, sizeof(ui_patient_t));

        /* Parse: nome\nidade\n */
        char *line2 = strchr(buf, '\n');
        if (line2) {
            size_t name_len = (size_t)(line2 - buf);
            if (name_len >= UI_PATIENT_NAME_LEN) name_len = UI_PATIENT_NAME_LEN - 1;
            memcpy(pt->name, buf, name_len);
            pt->name[name_len] = '\0';
            pt->age = (uint8_t)atoi(line2 + 1);
        } else {
            strlcpy(pt->name, buf, UI_PATIENT_NAME_LEN);
            pt->age = 0;
        }

        pt->id     = id;
        pt->active = true;
        pt->status = UI_STATUS_OFFLINE;
        pt->trend  = UI_TREND_UNKNOWN;

        free(buf); buf = NULL;
        s_state.patient_count++;

        ESP_LOGI(TAG, "Paciente carregado: [%u] %s (%u anos)", id, pt->name, pt->age);
    }
}

static esp_err_t _save_patient_to_spiffs(const ui_patient_t *pt)
{
    char path[SPIFFS_FULL_PATH_SIZE];
    char data[UI_PATIENT_NAME_LEN + 8];

    snprintf(path, sizeof(path), "patients/%03u.txt", pt->id);
    snprintf(data, sizeof(data), "%s\n%u\n", pt->name, pt->age);

    return spiffs_write_file(path, data, strlen(data));
}

static esp_err_t _delete_patient_from_spiffs(uint8_t id)
{
    char path[SPIFFS_FULL_PATH_SIZE];
    snprintf(path, sizeof(path), "patients/%03u.txt", id);
    return spiffs_delete_file(path);
}

/* =============================================================
 * CLASSIFICAÇÃO E TENDÊNCIA
 * ============================================================= */

ui_patient_status_t ui_data_classify(uint16_t mgdl)
{
    if (mgdl < UI_GLUCOSE_HYPO_CRIT || mgdl > UI_GLUCOSE_HYPER_CRIT)
        return UI_STATUS_CRITICAL;
    if (mgdl < UI_GLUCOSE_HYPO_WARN || mgdl > UI_GLUCOSE_NORMAL_HI)
        return UI_STATUS_WARNING;
    return UI_STATUS_NORMAL;
}

static ui_glucose_trend_t _calc_trend(const ui_patient_t *pt)
{
    if (pt->sample_count < 3) return UI_TREND_UNKNOWN;

    /* Usa as últimas 3 amostras para calcular variação média */
    uint16_t idx_last = (pt->sample_head + pt->sample_count - 1) % UI_MAX_SAMPLES;
    uint16_t idx_prev = (pt->sample_head + pt->sample_count - 3) % UI_MAX_SAMPLES;

    int32_t delta = (int32_t)pt->samples[idx_last].value_mgdl -
                    (int32_t)pt->samples[idx_prev].value_mgdl;

    /* Delta em 2 intervalos (aprox 10 min) → mg/dL/min */
    float rate = (float)delta / 10.0f;

    if (rate >  2.0f) return UI_TREND_RISING_FAST;
    if (rate >  1.0f) return UI_TREND_RISING;
    if (rate < -2.0f) return UI_TREND_FALLING_FAST;
    if (rate < -1.0f) return UI_TREND_FALLING;
    return UI_TREND_STABLE;
}

/* =============================================================
 * ALERTAS
 * ============================================================= */

static void _push_alert(uint8_t patient_id, ui_alert_type_t type,
                        uint16_t value)
{
    if (s_state.alert_count >= UI_MAX_ALERTS) {
        /* Remove o mais antigo (shift) */
        memmove(&s_state.alerts[0], &s_state.alerts[1],
                sizeof(ui_alert_t) * (UI_MAX_ALERTS - 1));
        s_state.alert_count = UI_MAX_ALERTS - 1;
    }

    ui_alert_t *a = &s_state.alerts[s_state.alert_count];
    a->type         = type;
    a->patient_id   = patient_id;
    a->value_mgdl   = value;
    a->timestamp    = s_state.current_time;
    a->acknowledged = false;
    a->silenced     = false;

    s_state.alert_count++;
    s_state.active_alert_count++;
}

/* =============================================================
 * API PÚBLICA
 * ============================================================= */

void ui_data_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.system_online = true;

    /* Lê hora atual do DS3231 */
    ds3231_get_time(&s_state.current_time);

    /* Carrega pacientes persistidos no SPIFFS */
    _load_patients_from_spiffs();

    ESP_LOGI(TAG, "Data layer inicializado. %u paciente(s).",
             s_state.patient_count);
}

void ui_data_update_rtc(void)
{
    ds3231_get_time(&s_state.current_time);
}

const ui_app_state_t * ui_data_get_state(void)
{
    return &s_state;
}

const ui_patient_t * ui_data_get_patient(uint8_t id)
{
    for (uint8_t i = 0; i < s_state.patient_count; i++) {
        if (s_state.patients[i].id == id && s_state.patients[i].active)
            return &s_state.patients[i];
    }
    return NULL;
}

void ui_data_ack_alert(uint8_t alert_idx)
{
    if (alert_idx >= s_state.alert_count) return;
    if (!s_state.alerts[alert_idx].acknowledged) {
        s_state.alerts[alert_idx].acknowledged = true;
        if (s_state.active_alert_count > 0) s_state.active_alert_count--;
    }
}

void ui_data_silence_alert(uint8_t alert_idx)
{
    if (alert_idx >= s_state.alert_count) return;
    s_state.alerts[alert_idx].silenced = true;
}

esp_err_t ui_data_add_patient(const char *name, uint8_t age)
{
    if (s_state.patient_count >= UI_MAX_PATIENTS) return ESP_ERR_NO_MEM;
    if (!name || name[0] == '\0') return ESP_ERR_INVALID_ARG;

    /* Encontra ID livre */
    bool id_used[UI_MAX_PATIENTS] = {false};
    for (uint8_t i = 0; i < s_state.patient_count; i++)
        id_used[s_state.patients[i].id] = true;

    uint8_t new_id = 0;
    while (new_id < UI_MAX_PATIENTS && id_used[new_id]) new_id++;
    if (new_id >= UI_MAX_PATIENTS) return ESP_ERR_NO_MEM;

    ui_patient_t *pt = &s_state.patients[s_state.patient_count];
    memset(pt, 0, sizeof(ui_patient_t));
    pt->id     = new_id;
    pt->age    = age;
    pt->active = true;
    pt->status = UI_STATUS_OFFLINE;
    pt->trend  = UI_TREND_UNKNOWN;
    strlcpy(pt->name, name, UI_PATIENT_NAME_LEN);

    esp_err_t ret = _save_patient_to_spiffs(pt);
    if (ret == ESP_OK) {
        s_state.patient_count++;
        ESP_LOGI(TAG, "Paciente adicionado: [%u] %s", new_id, name);
    }
    return ret;
}

esp_err_t ui_data_remove_patient(uint8_t id)
{
    for (uint8_t i = 0; i < s_state.patient_count; i++) {
        if (s_state.patients[i].id != id) continue;

        _delete_patient_from_spiffs(id);

        /* Remove do array com shift */
        memmove(&s_state.patients[i],
                &s_state.patients[i + 1],
                sizeof(ui_patient_t) * (s_state.patient_count - i - 1));
        s_state.patient_count--;

        ESP_LOGI(TAG, "Paciente removido: ID %u", id);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t ui_data_update_patient_name(uint8_t id, const char *name)
{
    for (uint8_t i = 0; i < s_state.patient_count; i++) {
        if (s_state.patients[i].id != id) continue;
        strlcpy(s_state.patients[i].name, name, UI_PATIENT_NAME_LEN);
        return _save_patient_to_spiffs(&s_state.patients[i]);
    }
    return ESP_ERR_NOT_FOUND;
}

void ui_data_push_sample(uint8_t patient_id, uint16_t value_mgdl)
{
    ui_patient_t *pt = NULL;
    for (uint8_t i = 0; i < s_state.patient_count; i++) {
        if (s_state.patients[i].id == patient_id) {
            pt = &s_state.patients[i];
            break;
        }
    }
    if (!pt) return;

    /* Insere no buffer circular */
    uint16_t idx = (pt->sample_head + pt->sample_count) % UI_MAX_SAMPLES;
    if (pt->sample_count == UI_MAX_SAMPLES) {
        pt->sample_head = (pt->sample_head + 1) % UI_MAX_SAMPLES;
    } else {
        pt->sample_count++;
    }

    pt->samples[idx].value_mgdl = value_mgdl;
    pt->samples[idx].timestamp  = s_state.current_time;
    pt->samples[idx].valid      = true;
    pt->last_sample             = pt->samples[idx];

    /* Atualiza status e tendência */
    ui_patient_status_t new_status = ui_data_classify(value_mgdl);
    ui_glucose_trend_t  new_trend  = _calc_trend(pt);

    /* Gera alertas apenas na transição de status */
    if (new_status != pt->status) {
        if (value_mgdl < UI_GLUCOSE_HYPO_CRIT)
            _push_alert(patient_id, UI_ALERT_HYPO_CRIT, value_mgdl);
        else if (value_mgdl < UI_GLUCOSE_HYPO_WARN)
            _push_alert(patient_id, UI_ALERT_HYPO_WARN, value_mgdl);
        else if (value_mgdl > UI_GLUCOSE_HYPER_CRIT)
            _push_alert(patient_id, UI_ALERT_HYPER_CRIT, value_mgdl);
        else if (value_mgdl > UI_GLUCOSE_NORMAL_HI)
            _push_alert(patient_id, UI_ALERT_HYPER_WARN, value_mgdl);
    }

    pt->status = new_status;
    pt->trend  = new_trend;
}

/* =============================================================
 * UTILITÁRIOS
 * ============================================================= */

const char * ui_data_trend_str(ui_glucose_trend_t t)
{
    switch (t) {
        case UI_TREND_RISING_FAST:  return "\xE2\x86\x91\xE2\x86\x91"; /* ↑↑ */
        case UI_TREND_RISING:       return "\xE2\x86\x91";              /* ↑  */
        case UI_TREND_STABLE:       return "\xE2\x86\x92";              /* →  */
        case UI_TREND_FALLING:      return "\xE2\x86\x93";              /* ↓  */
        case UI_TREND_FALLING_FAST: return "\xE2\x86\x93\xE2\x86\x93"; /* ↓↓ */
        default:                    return "--";
    }
}

const char * ui_data_status_str(ui_patient_status_t s)
{
    switch (s) {
        case UI_STATUS_NORMAL:   return "NORMAL";
        case UI_STATUS_WARNING:  return "ATENCAO";
        case UI_STATUS_CRITICAL: return "CRITICO";
        default:                 return "OFFLINE";
    }
}

const char * ui_data_alert_str(ui_alert_type_t t)
{
    switch (t) {
        case UI_ALERT_HYPO_CRIT:   return "HIPOGLICEMIA CRITICA";
        case UI_ALERT_HYPO_WARN:   return "Hipoglicemia - atencao";
        case UI_ALERT_HYPER_CRIT:  return "HIPERGLICEMIA CRITICA";
        case UI_ALERT_HYPER_WARN:  return "Hiperglicemia - atencao";
        case UI_ALERT_SENSOR_FAIL: return "Falha de sensor";
        case UI_ALERT_STALE_DATA:  return "Dados desatualizados";
        default:                   return "Alerta";
    }
}

int32_t ui_data_seconds_between(const struct tm *past, const struct tm *now)
{
    time_t t_past = mktime((struct tm *)past);
    time_t t_now  = mktime((struct tm *)now);
    return (int32_t)difftime(t_now, t_past);
}

void ui_data_fmt_hhmm(const struct tm *t, char *buf, size_t len)
{
    snprintf(buf, len, "%02d:%02d", t->tm_hour, t->tm_min);
}

void ui_data_fmt_hhmmss(const struct tm *t, char *buf, size_t len)
{
    snprintf(buf, len, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
}