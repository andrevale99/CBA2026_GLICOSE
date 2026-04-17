#pragma once

#include <stdint.h>

typedef enum {
    UI_SCREEN_HOME     = 0,
    UI_SCREEN_ALERTS   = 1,
    UI_SCREEN_PATIENTS = 2,
    UI_SCREEN_CONFIG   = 3,
    UI_SCREEN_PROFILE  = 4,
    UI_SCREEN_REGISTER = 5,
} ui_screen_id_t;

/** Navega para uma tela pelo ID */
void ui_nav_go(ui_screen_id_t id);

/** Atalho para abrir perfil de um paciente */
void ui_nav_go_patient(uint8_t patient_id);
