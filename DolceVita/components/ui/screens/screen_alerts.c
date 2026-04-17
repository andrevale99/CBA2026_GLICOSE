#include "lvgl.h"
#include "ui_data.h"
#include "ui_styles.h"
#include "ui_widgets.h"
#include "ui_nav.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "[SCREEN_ALERTS]";

/* =============================================================
 * SCREEN ALERTS — Central global de alertas
 *
 * Layout:
 *   [Topbar 800×56]
 *   [Navbar 800×52]
 *   [Scroll 800×372 — itens de alerta]
 *
 * Ordenação: CRIT primeiro, depois WARN.
 * Reconstrói a lista somente quando alert_count muda.
 * ============================================================= */

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *lbl_patient;
    lv_obj_t *lbl_type;
    lv_obj_t *lbl_value;
    lv_obj_t *lbl_time;
    lv_obj_t *btn_ack;
    lv_obj_t *btn_sil;
    lv_obj_t *btn_profile;
    uint8_t   alert_idx;
} alert_item_ctx_t;

static struct {
    lv_obj_t        *screen;
    lv_obj_t        *scroll_cont;
    lv_timer_t      *timer;
    ui_topbar_t     *topbar;
    ui_navbar_t     *navbar;
    alert_item_ctx_t items[UI_MAX_ALERTS];
    uint8_t          item_count;
    uint8_t          last_alert_count;
    bool             created;
} s_alerts;

/* ============================================================= */

static bool _is_critical(ui_alert_type_t t)
{
    return (t == UI_ALERT_HYPO_CRIT  ||
            t == UI_ALERT_HYPER_CRIT ||
            t == UI_ALERT_SENSOR_FAIL);
}

/* ---- Callbacks dos botões ---- */
static void _btn_ack_cb(lv_event_t *e)
{
    alert_item_ctx_t *ctx = (alert_item_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ui_data_ack_alert(ctx->alert_idx);
    lv_obj_add_state(ctx->btn_ack, LV_STATE_DISABLED);
    lv_label_set_text(lv_obj_get_child(ctx->btn_ack, 0), "Reconhecido");
    lv_obj_set_style_opa(ctx->cont, LV_OPA_50, 0);
}

static void _btn_sil_cb(lv_event_t *e)
{
    alert_item_ctx_t *ctx = (alert_item_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    ui_data_silence_alert(ctx->alert_idx);
    lv_obj_add_state(ctx->btn_sil, LV_STATE_DISABLED);
    lv_label_set_text(lv_obj_get_child(ctx->btn_sil, 0), "Silenciado");
}

static void _btn_profile_cb(lv_event_t *e)
{
    alert_item_ctx_t *ctx = (alert_item_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) return;
    const ui_app_state_t *state = ui_data_get_state();
    ui_nav_go_patient(state->alerts[ctx->alert_idx].patient_id);
}

/* ---- Cria um item de alerta ---- */
static void _alert_item_create(lv_obj_t *parent,
                                const ui_alert_t *alert,
                                uint8_t alert_idx,
                                uint8_t item_idx)
{
    alert_item_ctx_t *ctx = &s_alerts.items[item_idx];
    ctx->alert_idx = alert_idx;

    bool crit = _is_critical(alert->type);
    const ui_patient_t *pt = ui_data_get_patient(alert->patient_id);
    const char *pt_name = pt ? pt->name : "Paciente desconhecido";

    /* Container do item */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_add_style(cont, &ui_style_card, 0);
    lv_obj_set_size(cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_row(cont, 8, 0);

    if (crit) lv_obj_add_style(cont, &ui_style_card_crit, 0);
    else      lv_obj_add_style(cont, &ui_style_card_warn, 0);

    if (alert->acknowledged) lv_obj_set_style_opa(cont, LV_OPA_50, 0);

    /* Linha topo: nome + hora */
    lv_obj_t *row_top = lv_obj_create(cont);
    lv_obj_remove_style_all(row_top);
    lv_obj_set_size(row_top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_top, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ctx->lbl_patient = lv_label_create(row_top);
    lv_obj_set_style_text_font(ctx->lbl_patient, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(ctx->lbl_patient, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(ctx->lbl_patient, pt_name);

    ctx->lbl_time = lv_label_create(row_top);
    lv_obj_add_style(ctx->lbl_time, &ui_style_label_small, 0);
    {
        char tbuf[10];
        ui_data_fmt_hhmm(&alert->timestamp, tbuf, sizeof(tbuf));
        lv_label_set_text(ctx->lbl_time, tbuf);
    }

    /* Linha meio: tipo + valor */
    lv_obj_t *row_mid = lv_obj_create(cont);
    lv_obj_remove_style_all(row_mid);
    lv_obj_set_size(row_mid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_mid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_mid, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ctx->lbl_type = lv_label_create(row_mid);
    lv_obj_add_style(ctx->lbl_type, &ui_style_label_small, 0);
    lv_label_set_text(ctx->lbl_type, ui_data_alert_str(alert->type));

    ctx->lbl_value = lv_label_create(row_mid);
    lv_obj_set_style_text_font(ctx->lbl_value, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(ctx->lbl_value,
        crit ? UI_COLOR_CRIT : UI_COLOR_WARN, 0);
    {
        char vbuf[24];
        const char *trend = pt ? ui_data_trend_str(pt->trend) : "";
        snprintf(vbuf, sizeof(vbuf), "%u mg/dL %s", alert->value_mgdl, trend);
        lv_label_set_text(ctx->lbl_value, vbuf);
    }

    /* Separador */
    ui_divider_create(cont);

    /* Linha de ações */
    lv_obj_t *row_act = lv_obj_create(cont);
    lv_obj_remove_style_all(row_act);
    lv_obj_set_size(row_act, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_act, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_act, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_act, 10, 0);

    /* Botão Reconhecer */
    ctx->btn_ack = lv_button_create(row_act);
    lv_obj_add_style(ctx->btn_ack, &ui_style_btn_ghost, 0);
    lv_obj_set_style_text_color(ctx->btn_ack, UI_COLOR_OK, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ctx->btn_ack, UI_COLOR_OK, 0);
    lv_obj_t *lbl_ack = lv_label_create(ctx->btn_ack);
    lv_label_set_text(lbl_ack, alert->acknowledged ? "Reconhecido" : "Reconhecer");
    lv_obj_center(lbl_ack);
    lv_obj_add_event_cb(ctx->btn_ack, _btn_ack_cb, LV_EVENT_CLICKED, ctx);
    if (alert->acknowledged) lv_obj_add_state(ctx->btn_ack, LV_STATE_DISABLED);

    /* Botão Silenciar */
    ctx->btn_sil = lv_button_create(row_act);
    lv_obj_add_style(ctx->btn_sil, &ui_style_btn_ghost, 0);
    lv_obj_set_style_text_color(ctx->btn_sil, UI_COLOR_WARN, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ctx->btn_sil, UI_COLOR_WARN, 0);
    lv_obj_t *lbl_sil = lv_label_create(ctx->btn_sil);
    lv_label_set_text(lbl_sil, alert->silenced ? "Silenciado" : "Silenciar");
    lv_obj_center(lbl_sil);
    lv_obj_add_event_cb(ctx->btn_sil, _btn_sil_cb, LV_EVENT_CLICKED, ctx);
    if (alert->silenced) lv_obj_add_state(ctx->btn_sil, LV_STATE_DISABLED);

    /* Botão Ver Perfil */
    ctx->btn_profile = lv_button_create(row_act);
    lv_obj_add_style(ctx->btn_profile, &ui_style_btn_ghost, 0);
    lv_obj_t *lbl_prof = lv_label_create(ctx->btn_profile);
    lv_label_set_text(lbl_prof, "Ver perfil");
    lv_obj_center(lbl_prof);
    lv_obj_add_event_cb(ctx->btn_profile, _btn_profile_cb, LV_EVENT_CLICKED, ctx);

    ctx->cont = cont;
}

/* ---- Reconstrói toda a lista ---- */
static void _rebuild_list(void)
{
    lv_obj_clean(s_alerts.scroll_cont);
    s_alerts.item_count = 0;

    const ui_app_state_t *state = ui_data_get_state();

    if (state->alert_count == 0) {
        lv_obj_t *lbl = lv_label_create(s_alerts.scroll_cont);
        lv_obj_add_style(lbl, &ui_style_label_muted, 0);
        lv_label_set_text(lbl, "Nenhum alerta ativo.");
        lv_obj_center(lbl);
        s_alerts.last_alert_count = 0;
        return;
    }

    /* Dois passes: críticos primeiro, depois warnings */
    for (uint8_t pass = 0; pass < 2; pass++) {
        for (uint8_t i = 0; i < state->alert_count; i++) {
            const ui_alert_t *a = &state->alerts[i];
            bool crit = _is_critical(a->type);
            if ((pass == 0 && !crit) || (pass == 1 && crit)) continue;
            if (s_alerts.item_count >= UI_MAX_ALERTS) break;
            _alert_item_create(s_alerts.scroll_cont, a, i,
                               s_alerts.item_count++);
        }
    }

    s_alerts.last_alert_count = state->alert_count;
    ESP_LOGI(TAG, "Lista reconstruída: %u item(s).", s_alerts.item_count);
}

/* ---- Timer 3 s ---- */
static void _alerts_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    const ui_app_state_t *state = ui_data_get_state();
    ui_topbar_update(s_alerts.topbar, state);

    if (state->alert_count != s_alerts.last_alert_count) {
        _rebuild_list();
    }
}

/* =============================================================
 * API pública
 * ============================================================= */
static void _alerts_create(void)
{
    if (s_alerts.created) return;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &ui_style_screen, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_alerts.topbar = ui_topbar_create(scr);
    s_alerts.navbar = ui_navbar_create(scr);

    lv_obj_t *scroll = lv_obj_create(scr);
    lv_obj_remove_style_all(scroll);
    lv_obj_set_size(scroll, LV_PCT(100), 372);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(scroll, 12, 0);
    lv_obj_set_style_pad_row(scroll, 10, 0);
    lv_obj_set_style_bg_color(scroll, UI_COLOR_BG_SCREEN, 0);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_COVER, 0);
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);
    s_alerts.scroll_cont = scroll;

    s_alerts.timer = lv_timer_create(_alerts_timer_cb, 3000, NULL);
    lv_timer_pause(s_alerts.timer);

    s_alerts.screen  = scr;
    s_alerts.created = true;
    ESP_LOGI(TAG, "Criada.");
}

void screen_alerts_load(void)
{
    _alerts_create();
    _rebuild_list();
    ui_navbar_set_active(s_alerts.navbar, UI_NAV_ALERTS);
    lv_scr_load_anim(s_alerts.screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    lv_timer_resume(s_alerts.timer);
}

void screen_alerts_pause(void)
{
    if (s_alerts.timer) lv_timer_pause(s_alerts.timer);
}
