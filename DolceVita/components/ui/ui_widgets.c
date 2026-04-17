#include "ui_widgets.h"
#include "ui_styles.h"
#include "ui_nav.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "[UI_WIDGETS]";

/* =============================================================
 * HELPERS DE COR
 * ============================================================= */
lv_color_t ui_status_fg_color(ui_patient_status_t s)
{
    switch (s) {
        case UI_STATUS_NORMAL:   return UI_COLOR_OK;
        case UI_STATUS_WARNING:  return UI_COLOR_WARN;
        case UI_STATUS_CRITICAL: return UI_COLOR_CRIT;
        default:                 return UI_COLOR_TEXT_MUTED;
    }
}

lv_color_t ui_status_bg_color(ui_patient_status_t s)
{
    switch (s) {
        case UI_STATUS_NORMAL:   return UI_COLOR_OK_BG;
        case UI_STATUS_WARNING:  return UI_COLOR_WARN_BG;
        case UI_STATUS_CRITICAL: return UI_COLOR_CRIT_BG;
        default:                 return UI_COLOR_BG_CARD;
    }
}

/* =============================================================
 * TOPBAR
 * ============================================================= */
static ui_topbar_t s_topbar;

static void _topbar_alert_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_nav_go(UI_SCREEN_ALERTS);
}

ui_topbar_t * ui_topbar_create(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_add_style(cont, &ui_style_topbar, 0);
    lv_obj_set_size(cont, LV_PCT(100), UI_TOPBAR_H);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Coluna esquerda */
    lv_obj_t *left = lv_obj_create(cont);
    lv_obj_remove_style_all(left);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_flex_grow(left, 1);
    lv_obj_set_style_pad_row(left, 2, 0);

    s_topbar.lbl_time = lv_label_create(left);
    lv_obj_set_style_text_font(s_topbar.lbl_time, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(s_topbar.lbl_time, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(s_topbar.lbl_time, "00:00:00");

    s_topbar.lbl_status = lv_label_create(left);
    lv_obj_set_style_text_font(s_topbar.lbl_status, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(s_topbar.lbl_status, UI_COLOR_OK, 0);
    lv_label_set_text(s_topbar.lbl_status, "ONLINE");

    /* Botão de alertas */
    s_topbar.btn_alerts = lv_button_create(cont);
    lv_obj_add_style(s_topbar.btn_alerts, &ui_style_btn_danger, 0);
    lv_obj_set_height(s_topbar.btn_alerts, UI_BTN_MIN_H);
    lv_obj_add_event_cb(s_topbar.btn_alerts, _topbar_alert_cb,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_topbar.btn_alerts, LV_OBJ_FLAG_HIDDEN);

    s_topbar.lbl_alert_count = lv_label_create(s_topbar.btn_alerts);
    lv_label_set_text(s_topbar.lbl_alert_count, "0");

    s_topbar.cont = cont;
    return &s_topbar;
}

void ui_topbar_update(ui_topbar_t *tb, const ui_app_state_t *state)
{
    char buf[16];
    ui_data_fmt_hhmmss(&state->current_time, buf, sizeof(buf));
    lv_label_set_text(tb->lbl_time, buf);

    if (state->offline_mode) {
        lv_obj_set_style_text_color(tb->lbl_status, UI_COLOR_WARN, 0);
        lv_label_set_text(tb->lbl_status, "OFFLINE");
    } else if (!state->system_online) {
        lv_obj_set_style_text_color(tb->lbl_status, UI_COLOR_CRIT, 0);
        lv_label_set_text(tb->lbl_status, "FALHA");
    } else {
        lv_obj_set_style_text_color(tb->lbl_status, UI_COLOR_OK, 0);
        lv_label_set_text(tb->lbl_status, "ONLINE");
    }

    if (state->active_alert_count > 0) {
        snprintf(buf, sizeof(buf), "%u alertas", state->active_alert_count);
        lv_label_set_text(tb->lbl_alert_count, buf);
        lv_obj_clear_flag(tb->btn_alerts, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(tb->btn_alerts, LV_OBJ_FLAG_HIDDEN);
    }
}

/* =============================================================
 * NAVBAR
 * ============================================================= */
static ui_navbar_t s_navbar;

static void _navbar_btn_cb(lv_event_t *e)
{
    ui_nav_tab_t tab = (ui_nav_tab_t)(uintptr_t)lv_event_get_user_data(e);
    ui_nav_go((ui_screen_id_t)tab);
}

ui_navbar_t * ui_navbar_create(lv_obj_t *parent)
{
    static const char *labels[] = {"Home", "Alertas", "Pacientes", "Config"};

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_add_style(cont, &ui_style_navbar, 0);
    lv_obj_set_size(cont, LV_PCT(100), UI_NAVBAR_H);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_button_create(cont);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, UI_NAVBAR_H);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_font(lbl, UI_FONT_SMALL, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_SECONDARY, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, _navbar_btn_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
        s_navbar.btns[i] = btn;
    }

    s_navbar.cont = cont;
    return &s_navbar;
}

void ui_navbar_set_active(ui_navbar_t *nb, ui_nav_tab_t tab)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_t *lbl = lv_obj_get_child(nb->btns[i], 0);
        if (i == (int)tab) {
            lv_obj_add_style(nb->btns[i], &ui_style_navbar_btn_active, 0);
            lv_obj_set_style_text_color(lbl, UI_COLOR_INFO, 0);
        } else {
            lv_obj_remove_style(nb->btns[i], &ui_style_navbar_btn_active, 0);
            lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_SECONDARY, 0);
        }
    }
}

/* =============================================================
 * PILL DE STATUS
 * ============================================================= */
lv_obj_t * ui_pill_create(lv_obj_t *parent, ui_patient_status_t s)
{
    lv_obj_t *pill = lv_label_create(parent);
    lv_obj_set_style_pad_hor(pill, 10, 0);
    lv_obj_set_style_pad_ver(pill, 4, 0);
    lv_obj_set_style_radius(pill, 14, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(pill, UI_FONT_SMALL, 0);
    ui_pill_update(pill, s);
    return pill;
}

void ui_pill_update(lv_obj_t *pill, ui_patient_status_t s)
{
    lv_obj_set_style_bg_color(pill, ui_status_bg_color(s), 0);
    lv_obj_set_style_text_color(pill, ui_status_fg_color(s), 0);
    lv_label_set_text(pill, ui_data_status_str(s));
}

/* =============================================================
 * SEPARADOR
 * ============================================================= */
lv_obj_t * ui_divider_create(lv_obj_t *parent)
{
    lv_obj_t *div = lv_obj_create(parent);
    lv_obj_remove_style_all(div);
    lv_obj_set_size(div, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(div, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    return div;
}

/* =============================================================
 * MODAL DE CONFIRMAÇÃO — LVGL v9
 *
 * lv_msgbox_create(parent) → retorna o msgbox
 * lv_msgbox_add_title / lv_msgbox_add_text / lv_msgbox_add_footer_button
 * Os botões do footer são lv_obj_t comuns → usamos user_data para distinguir.
 * ============================================================= */
typedef struct {
    ui_confirm_cb_t cb;
    void           *user_data;
    bool            is_confirm_btn;  /* true = Confirmar, false = Cancelar */
} _confirm_btn_ctx_t;

/* Dois contextos estáticos para os dois botões */
static _confirm_btn_ctx_t s_ctx_confirm = { .is_confirm_btn = true };
static _confirm_btn_ctx_t s_ctx_cancel  = { .is_confirm_btn = false };
static lv_obj_t *s_active_mbox = NULL;

static void _confirm_btn_cb(lv_event_t *e)
{
    _confirm_btn_ctx_t *ctx = (_confirm_btn_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;

    bool confirmed = ctx->is_confirm_btn;
    ui_confirm_cb_t cb = ctx->cb;
    void *ud = ctx->user_data;

    if (s_active_mbox) {
        lv_msgbox_close(s_active_mbox);
        s_active_mbox = NULL;
    }
    if (cb) cb(confirmed, ud);
}

void ui_confirm_show(const char *title, const char *body,
                     ui_confirm_cb_t cb, void *user_data)
{
    s_ctx_confirm.cb        = cb;
    s_ctx_confirm.user_data = user_data;
    s_ctx_cancel.cb         = cb;
    s_ctx_cancel.user_data  = user_data;

    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, title);
    lv_msgbox_add_text(mbox, body);

    lv_obj_t *btn_c = lv_msgbox_add_footer_button(mbox, "Confirmar");
    lv_obj_t *btn_x = lv_msgbox_add_footer_button(mbox, "Cancelar");

    lv_obj_add_style(btn_c, &ui_style_btn_danger, 0);
    lv_obj_add_style(btn_x, &ui_style_btn_ghost,  0);

    lv_obj_add_event_cb(btn_c, _confirm_btn_cb, LV_EVENT_CLICKED, &s_ctx_confirm);
    lv_obj_add_event_cb(btn_x, _confirm_btn_cb, LV_EVENT_CLICKED, &s_ctx_cancel);

    lv_obj_set_width(mbox, 400);
    lv_obj_set_style_bg_color(mbox, UI_COLOR_BG_CARD, 0);
    lv_obj_set_style_border_color(mbox, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(mbox, 1, 0);
    lv_obj_set_style_radius(mbox, UI_CARD_RADIUS, 0);
    lv_obj_center(mbox);

    s_active_mbox = mbox;
    LV_UNUSED(TAG);
}
