#include "lvgl.h"
#include "ui_data.h"
#include "ui_styles.h"
#include "ui_widgets.h"
#include "ui_nav.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "[SCREEN_CONFIG]";

/* =============================================================
 * SCREEN CONFIG — Configurações do sistema
 *
 * Layout:
 *   [Topbar 800×56]
 *   [Navbar 800×52]
 *   [Scroll 800×372]
 *     → Seção: Sistema  (switches)
 *     → Seção: Limiares (sliders)
 *     → Seção: Interface (switches)
 * ============================================================= */

/* Configuração persistida (pode ser expandida para NVS depois) */
static struct {
    bool  alertas_sonoros;
    bool  modo_offline;
    bool  analise_ia;
    bool  tempo_relativo;
    uint16_t limiar_hiper;   /* mg/dL */
    uint16_t limiar_hypo;   /* mg/dL */
} s_cfg = {
    .alertas_sonoros = true,
    .modo_offline    = true,
    .analise_ia      = true,
    .tempo_relativo  = false,
    .limiar_hiper    = 250,
    .limiar_hypo     = 70,
};

static struct {
    lv_obj_t   *screen;
    ui_topbar_t *topbar;
    ui_navbar_t *navbar;
    /* Labels dos sliders (atualizados em tempo real) */
    lv_obj_t   *lbl_hiper_val;
    lv_obj_t   *lbl_hypo_val;
    bool        created;
} s_config;

/* ============================================================= */

static lv_obj_t * _section_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_pad_top(lbl, 8, 0);
    lv_label_set_text(lbl, text);
    return lbl;
}

/* Cria um card de seção que agrupa linhas de configuração */
static lv_obj_t * _section_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &ui_style_card, 0);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_pad_row(card, 0, 0);
    return card;
}

/* Cria uma linha com label + switch */
static lv_obj_t * _row_switch(lv_obj_t *parent,
                               const char *label,
                               const char *desc,
                               bool initial_val,
                               lv_event_cb_t cb,
                               void *user_data)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(row, 14, 0);
    lv_obj_set_style_pad_ver(row, 12, 0);
    lv_obj_set_style_border_color(row, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);

    /* Coluna de texto */
    lv_obj_t *col = lv_obj_create(row);
    lv_obj_remove_style_all(col);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(col, 2, 0);

    lv_obj_t *lbl = lv_label_create(col);
    lv_obj_set_style_text_font(lbl, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(lbl, label);

    if (desc) {
        lv_obj_t *lbl_d = lv_label_create(col);
        lv_obj_add_style(lbl_d, &ui_style_label_small, 0);
        lv_label_set_text(lbl_d, desc);
    }

    /* Switch */
    lv_obj_t *sw = lv_switch_create(row);
    lv_obj_set_size(sw, 52, 28);
    lv_obj_set_style_bg_color(sw, UI_COLOR_BORDER, 0);
    lv_obj_set_style_bg_color(sw, UI_COLOR_OK, LV_STATE_CHECKED);
    if (initial_val) lv_obj_add_state(sw, LV_STATE_CHECKED);
    if (cb) lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, user_data);

    return sw;
}

/* Cria uma linha com label + slider */
static lv_obj_t * _row_slider(lv_obj_t *parent,
                               lv_obj_t **out_lbl_val,
                               const char *label,
                               int32_t min, int32_t max, int32_t val,
                               lv_event_cb_t cb,
                               void *user_data)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(row, 14, 0);
    lv_obj_set_style_pad_ver(row, 14, 0);
    lv_obj_set_style_border_color(row, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);

    /* Linha topo: label + valor atual */
    lv_obj_t *row_top = lv_obj_create(row);
    lv_obj_remove_style_all(row_top);
    lv_obj_set_size(row_top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_top, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl = lv_label_create(row_top);
    lv_obj_set_style_text_font(lbl, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(lbl, label);

    lv_obj_t *lbl_val = lv_label_create(row_top);
    lv_obj_set_style_text_font(lbl_val, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_val, UI_COLOR_CRIT, 0);
    {
        char buf[12];
        snprintf(buf, sizeof(buf), "%ld mg/dL", (long)val);
        lv_label_set_text(lbl_val, buf);
    }
    if (out_lbl_val) *out_lbl_val = lbl_val;

    /* Slider */
    lv_obj_t *slider = lv_slider_create(row);
    lv_obj_set_size(slider, LV_PCT(100), 8);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, UI_COLOR_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, UI_COLOR_INFO, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 8, LV_PART_KNOB);
    if (cb) lv_obj_add_event_cb(slider, cb, LV_EVENT_VALUE_CHANGED, user_data);

    return slider;
}

/* ---- Callbacks de switch ---- */
static void _sw_alertas_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    s_cfg.alertas_sonoros = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void _sw_offline_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    s_cfg.modo_offline = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void _sw_ia_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    s_cfg.analise_ia = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

static void _sw_relativo_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    s_cfg.tempo_relativo = lv_obj_has_state(sw, LV_STATE_CHECKED);
}

/* ---- Callbacks de slider ---- */
static void _sl_hiper_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    s_cfg.limiar_hiper = (uint16_t)lv_slider_get_value(sl);
    char buf[12];
    snprintf(buf, sizeof(buf), "%u mg/dL", s_cfg.limiar_hiper);
    lv_label_set_text(s_config.lbl_hiper_val, buf);
}

static void _sl_hypo_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    s_cfg.limiar_hypo = (uint16_t)lv_slider_get_value(sl);
    char buf[12];
    snprintf(buf, sizeof(buf), "%u mg/dL", s_cfg.limiar_hypo);
    lv_label_set_text(s_config.lbl_hypo_val, buf);
}

/* =============================================================
 * Criação
 * ============================================================= */
static void _config_create(void)
{
    if (s_config.created) return;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &ui_style_screen, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_config.topbar = ui_topbar_create(scr);
    s_config.navbar = ui_navbar_create(scr);

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

    /* ---- Seção: Sistema ---- */
    _section_label(scroll, "SISTEMA");
    lv_obj_t *sec_sys = _section_card(scroll);
    _row_switch(sec_sys, "Alertas sonoros",
                "Buzzer para alertas criticos",
                s_cfg.alertas_sonoros, _sw_alertas_cb, NULL);
    _row_switch(sec_sys, "Modo offline",
                "Acumular dados sem rede",
                s_cfg.modo_offline, _sw_offline_cb, NULL);
    _row_switch(sec_sys, "Analise por IA",
                "Previsao e tendencia",
                s_cfg.analise_ia, _sw_ia_cb, NULL);

    /* ---- Seção: Limiares ---- */
    _section_label(scroll, "LIMIARES DE ALERTA");
    lv_obj_t *sec_thr = _section_card(scroll);
    _row_slider(sec_thr, &s_config.lbl_hiper_val,
                "Hiperglicemia critica",
                180, 400, s_cfg.limiar_hiper,
                _sl_hiper_cb, NULL);
    _row_slider(sec_thr, &s_config.lbl_hypo_val,
                "Hipoglicemia critica",
                40, 100, s_cfg.limiar_hypo,
                _sl_hypo_cb, NULL);

    /* ---- Seção: Interface ---- */
    _section_label(scroll, "INTERFACE");
    lv_obj_t *sec_ui = _section_card(scroll);
    _row_switch(sec_ui, "Tempo relativo",
                "Exibir \"ha X min\" alem do RTC",
                s_cfg.tempo_relativo, _sw_relativo_cb, NULL);

    s_config.screen  = scr;
    s_config.created = true;
    ESP_LOGI(TAG, "Criada.");
}

/* =============================================================
 * API pública
 * ============================================================= */
void screen_config_load(void)
{
    _config_create();
    ui_navbar_set_active(s_config.navbar, UI_NAV_CONFIG);
    const ui_app_state_t *state = ui_data_get_state();
    ui_topbar_update(s_config.topbar, state);
    lv_scr_load_anim(s_config.screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}
