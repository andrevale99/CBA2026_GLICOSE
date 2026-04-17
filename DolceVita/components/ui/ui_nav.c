#include "ui_nav.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "[UI_NAV]";

/* Forward declarations das telas */
void screen_home_load(void);
void screen_alerts_load(void);
void screen_patients_load(void);
void screen_config_load(void);
void screen_profile_load(uint8_t patient_id);
void screen_register_load(void);

/* Tela ativa atual */
static ui_screen_id_t s_current = UI_SCREEN_HOME;
static uint8_t        s_profile_patient_id = 0;

ui_screen_id_t ui_nav_current(void)
{
    return s_current;
}

uint8_t ui_nav_profile_patient_id(void)
{
    return s_profile_patient_id;
}

void ui_nav_go(ui_screen_id_t id)
{
    ESP_LOGI(TAG, "Navegando para tela %d", id);
    s_current = id;
    switch (id) {
        case UI_SCREEN_HOME:     screen_home_load();     break;
        case UI_SCREEN_ALERTS:   screen_alerts_load();   break;
        case UI_SCREEN_PATIENTS: screen_patients_load(); break;
        case UI_SCREEN_CONFIG:   screen_config_load();   break;
        case UI_SCREEN_REGISTER: screen_register_load(); break;
        default: break;
    }
}

void ui_nav_go_patient(uint8_t patient_id)
{
    s_profile_patient_id = patient_id;
    s_current = UI_SCREEN_PROFILE;
    screen_profile_load(patient_id);
}
