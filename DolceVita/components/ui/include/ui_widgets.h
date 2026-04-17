#pragma once

#include "lvgl.h"
#include "ui_data.h"

/* =============================================================
 * TOPBAR — barra superior fixa (800 × 56 px)
 * ============================================================= */
typedef struct {
    lv_obj_t *cont;
    lv_obj_t *lbl_time;
    lv_obj_t *lbl_status;
    lv_obj_t *btn_alerts;
    lv_obj_t *lbl_alert_count;
} ui_topbar_t;

ui_topbar_t * ui_topbar_create(lv_obj_t *parent);

/**
 * @brief Atualiza hora, status e badge de alertas.
 *        Chamar dentro de lvgl_port_lock / lvgl_port_unlock.
 */
void ui_topbar_update(ui_topbar_t *tb, const ui_app_state_t *state);

/* =============================================================
 * NAVBAR — barra de navegação inferior (800 × 52 px)
 * ============================================================= */
typedef enum {
    UI_NAV_HOME     = 0,
    UI_NAV_ALERTS   = 1,
    UI_NAV_PATIENTS = 2,
    UI_NAV_CONFIG   = 3,
} ui_nav_tab_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *btns[4];
} ui_navbar_t;

ui_navbar_t * ui_navbar_create(lv_obj_t *parent);
void          ui_navbar_set_active(ui_navbar_t *nb, ui_nav_tab_t tab);

/* =============================================================
 * STATUS PILL — badge colorido de status
 * ============================================================= */
lv_obj_t * ui_pill_create(lv_obj_t *parent, ui_patient_status_t s);
void       ui_pill_update(lv_obj_t *pill, ui_patient_status_t s);

/* =============================================================
 * SEPARADOR HORIZONTAL
 * ============================================================= */
lv_obj_t * ui_divider_create(lv_obj_t *parent);

/* =============================================================
 * MODAL DE CONFIRMAÇÃO
 * ============================================================= */
typedef void (*ui_confirm_cb_t)(bool confirmed, void *user_data);

void ui_confirm_show(const char *title,
                     const char *body,
                     ui_confirm_cb_t cb,
                     void *user_data);

/* =============================================================
 * HELPERS DE COR POR STATUS
 * ============================================================= */
lv_color_t ui_status_fg_color(ui_patient_status_t s);
lv_color_t ui_status_bg_color(ui_patient_status_t s);
