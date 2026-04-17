#include "lvgl.h"
#include "ui_data.h"
#include "ui_styles.h"
#include "ui_widgets.h"
#include "ui_nav.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "[SCREEN_PROFILE]";

/* =============================================================
 * SCREEN PROFILE — 800×480
 *
 * Layout:
 *   [Topbar 800×56]
 *   [Scroll 800×424 — sem navbar, tem botão Voltar]
 *     → Botão < Voltar
 *     → Header card: avatar + nome + glicemia + pill + ts
 *     → Chart card: tabs 1h|6h|24h + lv_chart
 *     → Histórico: últimas 10 leituras
 *     → Card IA: tendência + previsão
 *
 * Timer 5 s: atualiza labels + push chart incremental.
 * ============================================================= */

#define CHART_PTS_1H   12
#define CHART_PTS_6H   36
#define CHART_PTS_24H  48

typedef enum { WIN_1H = 0, WIN_6H = 1, WIN_24H = 2 } chart_win_t;

static struct {
    lv_obj_t          *screen;
    lv_obj_t          *scroll_cont;
    lv_timer_t        *timer;
    ui_topbar_t       *topbar;

    /* Header */
    lv_obj_t *lbl_name;
    lv_obj_t *lbl_glucose_big;
    lv_obj_t *lbl_trend_big;
    lv_obj_t *lbl_pill;
    lv_obj_t *lbl_ts;
    lv_obj_t *lbl_stale;

    /* Chart */
    lv_obj_t          *chart;
    lv_chart_series_t *ser_glucose;
    lv_chart_series_t *ser_hiper;
    lv_chart_series_t *ser_hypo;
    lv_obj_t          *btn_win[3];
    chart_win_t        current_win;

    /* Histórico */
    lv_obj_t *hist_list;
    uint16_t  hist_rendered;

    /* IA */
    lv_obj_t *lbl_ai_trend;
    lv_obj_t *lbl_ai_predict;

    uint8_t patient_id;
    bool    created;
} s_profile;

/* =============================================================
 * Chart helpers
 * ============================================================= */
static uint16_t _win_points(chart_win_t w)
{
    if (w == WIN_1H)  return CHART_PTS_1H;
    if (w == WIN_6H)  return CHART_PTS_6H;
    return CHART_PTS_24H;
}

static void _chart_load(const ui_patient_t *pt, chart_win_t win)
{
    uint16_t n = _win_points(win);
    lv_chart_set_point_count(s_profile.chart, n);
    lv_chart_set_all_value(s_profile.chart, s_profile.ser_glucose,
                            LV_CHART_POINT_NONE);

    uint16_t total = pt->sample_count;
    uint16_t start = (total > n) ? (total - n) : 0;
    for (uint16_t i = start; i < total; i++) {
        uint16_t idx = (pt->sample_head + i) % UI_MAX_SAMPLES;
        if (pt->samples[idx].valid) {
            lv_chart_set_next_value(s_profile.chart, s_profile.ser_glucose,
                                     (int32_t)pt->samples[idx].value_mgdl);
        }
    }

    /* Linhas de referência — lv_chart_set_value_by_id(chart, ser, id, val)
       id é uint32_t no LVGL v9 */
    for (uint32_t i = 0; i < (uint32_t)n; i++) {
        lv_chart_set_value_by_id(s_profile.chart, s_profile.ser_hiper,
                                  i, (int32_t)UI_GLUCOSE_HYPER_CRIT);
        lv_chart_set_value_by_id(s_profile.chart, s_profile.ser_hypo,
                                  i, (int32_t)UI_GLUCOSE_HYPO_CRIT);
    }

    lv_chart_refresh(s_profile.chart);
}

static void _win_btn_cb(lv_event_t *e)
{
    chart_win_t w = (chart_win_t)(uintptr_t)lv_event_get_user_data(e);
    s_profile.current_win = w;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *lbl = lv_obj_get_child(s_profile.btn_win[i], 0);
        if (i == (int)w) {
            lv_obj_set_style_bg_color(s_profile.btn_win[i], UI_COLOR_INFO, 0);
            lv_obj_set_style_bg_opa(s_profile.btn_win[i], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        } else {
            lv_obj_set_style_bg_color(s_profile.btn_win[i], UI_COLOR_BG_CARD, 0);
            lv_obj_set_style_bg_opa(s_profile.btn_win[i], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_SECONDARY, 0);
        }
    }

    const ui_patient_t *pt = ui_data_get_patient(s_profile.patient_id);
    if (pt) _chart_load(pt, w);
}

/* =============================================================
 * Timer 5 s
 * ============================================================= */
static void _profile_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    const ui_app_state_t *state = ui_data_get_state();
    ui_topbar_update(s_profile.topbar, state);

    const ui_patient_t *pt = ui_data_get_patient(s_profile.patient_id);
    if (!pt) return;

    lv_color_t fg = ui_status_fg_color(pt->status);

    /* Valor */
    char vbuf[8];
    if (pt->last_sample.valid)
        snprintf(vbuf, sizeof(vbuf), "%u", pt->last_sample.value_mgdl);
    else
        snprintf(vbuf, sizeof(vbuf), "---");
    lv_label_set_text(s_profile.lbl_glucose_big, vbuf);
    lv_obj_set_style_text_color(s_profile.lbl_glucose_big, fg, 0);

    /* Tendência */
    lv_label_set_text(s_profile.lbl_trend_big, ui_data_trend_str(pt->trend));
    lv_obj_set_style_text_color(s_profile.lbl_trend_big, fg, 0);

    /* Pill */
    ui_pill_update(s_profile.lbl_pill, pt->status);

    /* Timestamp + stale */
    if (pt->last_sample.valid) {
        char tsbuf[40], tbuf[12];
        ui_data_fmt_hhmmss(&pt->last_sample.timestamp, tbuf, sizeof(tbuf));
        snprintf(tsbuf, sizeof(tsbuf), "Ultima leitura: %s", tbuf);
        lv_label_set_text(s_profile.lbl_ts, tsbuf);

        int32_t age = ui_data_seconds_between(&pt->last_sample.timestamp,
                                               &state->current_time);
        if (age > UI_STALE_THRESHOLD_S)
            lv_obj_clear_flag(s_profile.lbl_stale, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(s_profile.lbl_stale, LV_OBJ_FLAG_HIDDEN);
    }

    /* Chart push incremental */
    if (s_profile.chart && pt->last_sample.valid && pt->sample_count > 0) {
        lv_chart_set_next_value(s_profile.chart, s_profile.ser_glucose,
                                 (int32_t)pt->last_sample.value_mgdl);
        lv_chart_refresh(s_profile.chart);
    }

    /* Histórico: nova linha no topo se sample_count cresceu */
    if (pt->sample_count > s_profile.hist_rendered && s_profile.hist_list) {
        lv_obj_t *row = lv_obj_create(s_profile.hist_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_ver(row, 7, 0);
        lv_obj_set_style_border_color(row, UI_COLOR_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);

        char tbuf[10];
        ui_data_fmt_hhmm(&pt->last_sample.timestamp, tbuf, sizeof(tbuf));
        lv_obj_t *lbl_t = lv_label_create(row);
        lv_obj_add_style(lbl_t, &ui_style_label_small, 0);
        lv_label_set_text(lbl_t, tbuf);

        lv_obj_t *lbl_v = lv_label_create(row);
        lv_obj_set_style_text_font(lbl_v, UI_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(lbl_v, fg, 0);
        char vbuf2[20];
        snprintf(vbuf2, sizeof(vbuf2), "%u mg/dL %s",
                 pt->last_sample.value_mgdl, ui_data_trend_str(pt->trend));
        lv_label_set_text(lbl_v, vbuf2);

        /* LVGL v9: lv_obj_move_to_index existe e funciona corretamente */
        lv_obj_move_to_index(row, 0);
        s_profile.hist_rendered = pt->sample_count;
    }

    /* IA */
    if (pt->ai_data_valid && s_profile.lbl_ai_trend) {
        lv_label_set_text(s_profile.lbl_ai_trend, pt->ai_trend_text);
        char pbuf[52];
        snprintf(pbuf, sizeof(pbuf), "Previsao 30 min: ~%u mg/dL",
                 pt->ai_predicted_mgdl);
        lv_label_set_text(s_profile.lbl_ai_predict, pbuf);
    }
}

/* =============================================================
 * Callback Voltar
 * ============================================================= */
static void _back_btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_nav_go(UI_SCREEN_HOME);
}

/* =============================================================
 * Construtores de seção
 * ============================================================= */
static void _build_back_btn(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_add_style(btn, &ui_style_btn_ghost, 0);
    /* LVGL v9: lv_obj_set_style_margin funciona normalmente */
    lv_obj_set_style_margin_top(btn, 4, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "< Voltar");
    lv_obj_set_style_text_font(lbl, UI_FONT_NORMAL, 0);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, _back_btn_cb, LV_EVENT_CLICKED, NULL);
}

static void _build_header(lv_obj_t *parent, const ui_patient_t *pt)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &ui_style_card, 0);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_row(card, 10, 0);

    /* Linha: avatar + nome + ID */
    lv_obj_t *row_name = lv_obj_create(card);
    lv_obj_remove_style_all(row_name);
    lv_obj_set_size(row_name, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_name, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_name, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_name, 14, 0);

    lv_obj_t *av = lv_obj_create(row_name);
    lv_obj_remove_style_all(av);
    lv_obj_set_size(av, 56, 56);
    lv_obj_set_style_radius(av, 28, 0);
    lv_obj_set_style_bg_opa(av, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(av, ui_status_bg_color(pt->status), 0);
    lv_obj_clear_flag(av, LV_OBJ_FLAG_SCROLLABLE);

    char ini[4] = {0};
    ini[0] = pt->name[0];
    const char *sp = strchr(pt->name, ' ');
    if (sp && *(sp + 1)) ini[1] = *(sp + 1);
    lv_obj_t *lbl_av = lv_label_create(av);
    lv_obj_set_style_text_font(lbl_av, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl_av, ui_status_fg_color(pt->status), 0);
    lv_label_set_text(lbl_av, ini);
    lv_obj_center(lbl_av);

    lv_obj_t *col_name = lv_obj_create(row_name);
    lv_obj_remove_style_all(col_name);
    lv_obj_set_flex_flow(col_name, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_name, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col_name, 3, 0);

    s_profile.lbl_name = lv_label_create(col_name);
    lv_obj_set_style_text_font(s_profile.lbl_name, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(s_profile.lbl_name, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(s_profile.lbl_name, pt->name);

    lv_obj_t *lbl_id = lv_label_create(col_name);
    lv_obj_add_style(lbl_id, &ui_style_label_small, 0);
    {
        char idbuf[24];
        snprintf(idbuf, sizeof(idbuf), "ID %03u  %u anos", pt->id, pt->age);
        lv_label_set_text(lbl_id, idbuf);
    }

    /* Linha: valor grande + unidade + tendência */
    lv_obj_t *row_glu = lv_obj_create(card);
    lv_obj_remove_style_all(row_glu);
    lv_obj_set_size(row_glu, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_glu, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_glu, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row_glu, 8, 0);

    s_profile.lbl_glucose_big = lv_label_create(row_glu);
    lv_obj_set_style_text_font(s_profile.lbl_glucose_big, UI_FONT_GLUCOSE_BIG, 0);
    lv_obj_set_style_text_color(s_profile.lbl_glucose_big,
                                 ui_status_fg_color(pt->status), 0);
    {
        char vbuf[8];
        snprintf(vbuf, sizeof(vbuf), pt->last_sample.valid ? "%u" : "---",
                 pt->last_sample.value_mgdl);
        lv_label_set_text(s_profile.lbl_glucose_big, vbuf);
    }

    lv_obj_t *lbl_unit = lv_label_create(row_glu);
    lv_obj_add_style(lbl_unit, &ui_style_label_muted, 0);
    lv_label_set_text(lbl_unit, "mg/dL");

    s_profile.lbl_trend_big = lv_label_create(row_glu);
    lv_obj_set_style_text_font(s_profile.lbl_trend_big, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(s_profile.lbl_trend_big,
                                 ui_status_fg_color(pt->status), 0);
    lv_label_set_text(s_profile.lbl_trend_big, ui_data_trend_str(pt->trend));

    /* Linha: pill + timestamp */
    lv_obj_t *row_meta = lv_obj_create(card);
    lv_obj_remove_style_all(row_meta);
    lv_obj_set_size(row_meta, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_meta, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_meta, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_profile.lbl_pill = ui_pill_create(row_meta, pt->status);

    lv_obj_t *col_ts = lv_obj_create(row_meta);
    lv_obj_remove_style_all(col_ts);
    lv_obj_set_flex_flow(col_ts, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_ts, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(col_ts, 2, 0);

    s_profile.lbl_ts = lv_label_create(col_ts);
    lv_obj_add_style(s_profile.lbl_ts, &ui_style_label_small, 0);
    lv_label_set_text(s_profile.lbl_ts, "Ultima leitura: --:--:--");

    s_profile.lbl_stale = lv_label_create(col_ts);
    lv_obj_set_style_text_font(s_profile.lbl_stale, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(s_profile.lbl_stale, UI_COLOR_WARN, 0);
    lv_label_set_text(s_profile.lbl_stale, "! DADOS ANTIGOS");
    lv_obj_add_flag(s_profile.lbl_stale, LV_OBJ_FLAG_HIDDEN);
}

static void _build_chart(lv_obj_t *parent, const ui_patient_t *pt)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &ui_style_card, 0);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_row(card, 10, 0);

    /* Tabs */
    lv_obj_t *tabs = lv_obj_create(card);
    lv_obj_remove_style_all(tabs);
    lv_obj_set_size(tabs, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(tabs, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(tabs, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(tabs, 8, 0);

    static const char *wlabels[] = {"1h", "6h", "24h"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_button_create(tabs);
        lv_obj_set_style_radius(btn, 16, 0);
        lv_obj_set_style_pad_hor(btn, 16, 0);
        lv_obj_set_style_pad_ver(btn, 7, 0);
        lv_obj_set_style_min_height(btn, 34, 0);
        lv_obj_set_style_border_color(btn, UI_COLOR_BORDER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_bg_color(btn,
            i == 0 ? UI_COLOR_INFO : UI_COLOR_BG_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, UI_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(lbl,
            i == 0 ? lv_color_white() : UI_COLOR_TEXT_SECONDARY, 0);
        lv_label_set_text(lbl, wlabels[i]);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, _win_btn_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);
        s_profile.btn_win[i] = btn;
    }

    /* lv_chart LVGL v9 */
    lv_obj_t *chart = lv_chart_create(card);
    lv_obj_set_size(chart, LV_PCT(100), 200);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 40, 350);

    lv_obj_set_style_bg_color(chart, UI_COLOR_BG_SCREEN, 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(chart, 1, 0);
    lv_obj_set_style_radius(chart, 6, 0);
    /* Ponto de 4px via LV_PART_INDICATOR */
    lv_obj_set_style_width(chart, 4, LV_PART_INDICATOR);
    lv_obj_set_style_height(chart, 4, LV_PART_INDICATOR);

    lv_chart_set_div_line_count(chart, 5, 0);
    lv_obj_set_style_line_color(chart, UI_COLOR_BORDER, LV_PART_MAIN);


    s_profile.ser_glucose = lv_chart_add_series(
        chart, UI_COLOR_INFO, LV_CHART_AXIS_PRIMARY_Y);
    s_profile.ser_hiper = lv_chart_add_series(
        chart, UI_COLOR_CRIT, LV_CHART_AXIS_PRIMARY_Y);
    s_profile.ser_hypo = lv_chart_add_series(
        chart, UI_COLOR_WARN, LV_CHART_AXIS_PRIMARY_Y);

    s_profile.chart = chart;
    s_profile.current_win = WIN_1H;
    _chart_load(pt, WIN_1H);
}

static void _build_history(lv_obj_t *parent, const ui_patient_t *pt)
{
    lv_obj_t *lbl_sec = lv_label_create(parent);
    lv_obj_add_style(lbl_sec, &ui_style_label_muted, 0);
    lv_label_set_text(lbl_sec, "HISTORICO RECENTE");

    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_add_style(list, &ui_style_card, 0);
    lv_obj_set_size(list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_row(list, 0, 0);
    s_profile.hist_list = list;

    uint16_t show  = (pt->sample_count < 10) ? pt->sample_count : 10;
    uint16_t start = (pt->sample_count > show) ? (pt->sample_count - show) : 0;

    for (uint16_t i = pt->sample_count; i > start; i--) {
        uint16_t idx = (pt->sample_head + i - 1) % UI_MAX_SAMPLES;
        if (!pt->samples[idx].valid) continue;

        ui_patient_status_t st = ui_data_classify(pt->samples[idx].value_mgdl);

        lv_obj_t *row = lv_obj_create(list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_ver(row, 8, 0);
        lv_obj_set_style_pad_hor(row, 0, 0);
        lv_obj_set_style_border_color(row, UI_COLOR_BORDER, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);

        char tbuf[10];
        ui_data_fmt_hhmm(&pt->samples[idx].timestamp, tbuf, sizeof(tbuf));
        lv_obj_t *lbl_t = lv_label_create(row);
        lv_obj_add_style(lbl_t, &ui_style_label_small, 0);
        lv_label_set_text(lbl_t, tbuf);

        lv_obj_t *lbl_v = lv_label_create(row);
        lv_obj_set_style_text_font(lbl_v, UI_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(lbl_v, ui_status_fg_color(st), 0);
        char vbuf[14];
        snprintf(vbuf, sizeof(vbuf), "%u mg/dL", pt->samples[idx].value_mgdl);
        lv_label_set_text(lbl_v, vbuf);
    }

    s_profile.hist_rendered = pt->sample_count;
}

static void _build_ai_card(lv_obj_t *parent, const ui_patient_t *pt)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_radius(card, UI_CARD_RADIUS, 0);
    lv_obj_set_style_bg_color(card, UI_COLOR_INFO_BG, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, UI_COLOR_INFO, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, UI_CARD_PAD, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_title, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_INFO, 0);
    lv_label_set_text(lbl_title, "ANALISE IA - TENDENCIA");

    s_profile.lbl_ai_trend = lv_label_create(card);
    lv_obj_set_style_text_font(s_profile.lbl_ai_trend, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(s_profile.lbl_ai_trend, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_long_mode(s_profile.lbl_ai_trend, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_profile.lbl_ai_trend, LV_PCT(100));
    lv_label_set_text(s_profile.lbl_ai_trend,
                       pt->ai_data_valid ? pt->ai_trend_text
                                         : "Aguardando dados...");

    s_profile.lbl_ai_predict = lv_label_create(card);
    lv_obj_set_style_text_font(s_profile.lbl_ai_predict, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(s_profile.lbl_ai_predict, UI_COLOR_INFO, 0);
    if (pt->ai_data_valid) {
        char pbuf[52];
        snprintf(pbuf, sizeof(pbuf), "Previsao 30 min: ~%u mg/dL",
                 pt->ai_predicted_mgdl);
        lv_label_set_text(s_profile.lbl_ai_predict, pbuf);
    } else {
        lv_label_set_text(s_profile.lbl_ai_predict, "");
    }
}

/* =============================================================
 * API pública
 * ============================================================= */
static void _profile_create(void)
{
    if (s_profile.created) return;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &ui_style_screen, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_profile.topbar = ui_topbar_create(scr);

    lv_obj_t *scroll = lv_obj_create(scr);
    lv_obj_remove_style_all(scroll);
    /* 480 - 56(topbar) = 424 */
    lv_obj_set_size(scroll, LV_PCT(100), 424);
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
    s_profile.scroll_cont = scroll;

    s_profile.timer = lv_timer_create(_profile_timer_cb, 5000, NULL);
    lv_timer_pause(s_profile.timer);

    s_profile.screen  = scr;
    s_profile.created = true;
}

void screen_profile_load(uint8_t patient_id)
{
    _profile_create();

    s_profile.patient_id = patient_id;
    lv_obj_clean(s_profile.scroll_cont);
    s_profile.chart = NULL;

    const ui_patient_t *pt = ui_data_get_patient(patient_id);
    if (!pt) {
        lv_obj_t *lbl = lv_label_create(s_profile.scroll_cont);
        lv_obj_add_style(lbl, &ui_style_label_muted, 0);
        lv_label_set_text(lbl, "Paciente nao encontrado.");
        lv_obj_center(lbl);
        return;
    }

    _build_back_btn(s_profile.scroll_cont);
    _build_header(s_profile.scroll_cont, pt);
    _build_chart(s_profile.scroll_cont, pt);
    _build_history(s_profile.scroll_cont, pt);
    _build_ai_card(s_profile.scroll_cont, pt);

    lv_scr_load_anim(s_profile.screen,
                     LV_SCR_LOAD_ANIM_MOVE_LEFT, 220, 0, false);
    lv_timer_resume(s_profile.timer);
    ESP_LOGI(TAG, "Perfil ID %u - %s", patient_id, pt->name);
}

void screen_profile_pause(void)
{
    if (s_profile.timer) lv_timer_pause(s_profile.timer);
}