#include "lvgl.h"
#include "ui_data.h"
#include "ui_styles.h"
#include "ui_widgets.h"
#include "ui_nav.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "[SCREEN_HOME]";

/* =============================================================
 * SCREEN HOME — 800 × 480
 *
 * Layout (coluna):
 *   [Topbar  800×56]
 *   [Navbar  800×52]
 *   [Scroll  800×372 — cards de pacientes]
 *
 * Atualização via lv_timer a cada 2 000 ms.
 * Apenas labels são atualizados — nenhum widget é recriado.
 * ============================================================= */

#define HOME_MAX_CARDS  UI_MAX_PATIENTS

/* Contexto de cada card (ponteiros estáticos para os widgets) */
typedef struct {
    lv_obj_t *card;
    lv_obj_t *lbl_glucose;
    lv_obj_t *lbl_trend;
    lv_obj_t *lbl_pill;
    lv_obj_t *lbl_name;
    lv_obj_t *lbl_ts;
    lv_obj_t *lbl_stale;       /* "DADOS ANTIGOS" */
    uint8_t   patient_id;
    bool      in_use;
} home_card_ctx_t;

static struct {
    lv_obj_t        *screen;
    lv_obj_t        *scroll_cont;
    lv_timer_t      *timer;
    ui_topbar_t     *topbar;
    ui_navbar_t     *navbar;
    home_card_ctx_t  cards[HOME_MAX_CARDS];
    uint8_t          card_count;
    bool             created;
} s_home;

/* =============================================================
 * Callback de clique no card
 * ============================================================= */
static void _card_click_cb(lv_event_t *e)
{
    home_card_ctx_t *ctx = (home_card_ctx_t *)lv_event_get_user_data(e);
    if (ctx) ui_nav_go_patient(ctx->patient_id);
}

/* =============================================================
 * Aplica / remove estilo de borda lateral por status
 * ============================================================= */
static void _apply_card_border(lv_obj_t *card, ui_patient_status_t s)
{
    lv_obj_remove_style(card, &ui_style_card_ok,   0);
    lv_obj_remove_style(card, &ui_style_card_warn, 0);
    lv_obj_remove_style(card, &ui_style_card_crit, 0);
    switch (s) {
        case UI_STATUS_NORMAL:   lv_obj_add_style(card, &ui_style_card_ok,   0); break;
        case UI_STATUS_WARNING:  lv_obj_add_style(card, &ui_style_card_warn, 0); break;
        case UI_STATUS_CRITICAL: lv_obj_add_style(card, &ui_style_card_crit, 0); break;
        default: break;
    }
}

/* =============================================================
 * Cria um card de paciente (uma única vez)
 * ============================================================= */
static void _card_create(lv_obj_t *parent,
                          const ui_patient_t *pt,
                          uint8_t card_idx)
{
    home_card_ctx_t *ctx = &s_home.cards[card_idx];
    ctx->patient_id = pt->id;
    ctx->in_use     = true;

    /* ---- Container do card ---- */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &ui_style_card, 0);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, _card_click_cb, LV_EVENT_CLICKED, ctx);
    _apply_card_border(card, pt->status);

    /* ---- Avatar (círculo com iniciais) ---- */
    lv_obj_t *av = lv_obj_create(card);
    lv_obj_remove_style_all(av);
    lv_obj_set_size(av, 52, 52);
    lv_obj_set_style_radius(av, 26, 0);
    lv_obj_set_style_bg_opa(av, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(av, ui_status_bg_color(pt->status), 0);
    lv_obj_clear_flag(av, LV_OBJ_FLAG_SCROLLABLE);

    char initials[4] = {0};
    initials[0] = pt->name[0];
    const char *sp = strchr(pt->name, ' ');
    if (sp && *(sp + 1)) initials[1] = *(sp + 1);

    lv_obj_t *lbl_av = lv_label_create(av);
    lv_obj_set_style_text_font(lbl_av, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_av, ui_status_fg_color(pt->status), 0);
    lv_label_set_text(lbl_av, initials);
    lv_obj_center(lbl_av);

    /* ---- Coluna central: nome + timestamp + pill ---- */
    lv_obj_t *col_info = lv_obj_create(card);
    lv_obj_remove_style_all(col_info);
    lv_obj_set_flex_grow(col_info, 1);
    lv_obj_set_flex_flow(col_info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_info, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_left(col_info, 12, 0);
    lv_obj_set_style_pad_row(col_info, 4, 0);

    ctx->lbl_name = lv_label_create(col_info);
    lv_obj_set_style_text_font(ctx->lbl_name, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(ctx->lbl_name, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(ctx->lbl_name, pt->name);

    ctx->lbl_ts = lv_label_create(col_info);
    lv_obj_add_style(ctx->lbl_ts, &ui_style_label_small, 0);
    lv_label_set_text(ctx->lbl_ts, "--:--");

    ctx->lbl_pill = ui_pill_create(col_info, pt->status);

    /* Badge de dado obsoleto (oculto por padrão) */
    ctx->lbl_stale = lv_label_create(col_info);
    lv_obj_set_style_text_font(ctx->lbl_stale, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(ctx->lbl_stale, UI_COLOR_WARN, 0);
    lv_label_set_text(ctx->lbl_stale, "! DADOS ANTIGOS");
    lv_obj_add_flag(ctx->lbl_stale, LV_OBJ_FLAG_HIDDEN);

    /* ---- Coluna direita: valor + tendência ---- */
    lv_obj_t *col_glu = lv_obj_create(card);
    lv_obj_remove_style_all(col_glu);
    lv_obj_set_flex_flow(col_glu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_glu, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_right(col_glu, 4, 0);
    lv_obj_set_style_pad_row(col_glu, 2, 0);

    /* Linha: número grande + seta */
    lv_obj_t *row_val = lv_obj_create(col_glu);
    lv_obj_remove_style_all(row_val);
    lv_obj_set_flex_flow(row_val, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_val, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_val, 4, 0);

    ctx->lbl_glucose = lv_label_create(row_val);
    lv_obj_set_style_text_font(ctx->lbl_glucose, UI_FONT_GLUCOSE_BIG, 0);
    lv_obj_set_style_text_color(ctx->lbl_glucose,
                                 ui_status_fg_color(pt->status), 0);
    lv_label_set_text(ctx->lbl_glucose, "---");

    ctx->lbl_trend = lv_label_create(row_val);
    lv_obj_set_style_text_font(ctx->lbl_trend, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(ctx->lbl_trend,
                                 ui_status_fg_color(pt->status), 0);
    lv_label_set_text(ctx->lbl_trend, "--");

    /* Unidade */
    lv_obj_t *lbl_unit = lv_label_create(col_glu);
    lv_obj_add_style(lbl_unit, &ui_style_label_small, 0);
    lv_label_set_text(lbl_unit, "mg/dL");
    lv_obj_set_style_text_align(lbl_unit, LV_TEXT_ALIGN_RIGHT, 0);

    ctx->card = card;
}

/* =============================================================
 * Timer de atualização — 2 s
 * ============================================================= */
static void _home_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    const ui_app_state_t *state = ui_data_get_state();

    ui_topbar_update(s_home.topbar, state);

    for (uint8_t i = 0; i < s_home.card_count; i++) {
        home_card_ctx_t *ctx = &s_home.cards[i];
        if (!ctx->in_use) continue;

        const ui_patient_t *pt = ui_data_get_patient(ctx->patient_id);
        if (!pt || !pt->active) {
            lv_obj_add_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);

        lv_color_t fg = ui_status_fg_color(pt->status);

        /* Valor glicemia */
        char vbuf[8];
        if (pt->last_sample.valid)
            snprintf(vbuf, sizeof(vbuf), "%u", pt->last_sample.value_mgdl);
        else
            snprintf(vbuf, sizeof(vbuf), "---");
        lv_label_set_text(ctx->lbl_glucose, vbuf);
        lv_obj_set_style_text_color(ctx->lbl_glucose, fg, 0);

        /* Tendência */
        lv_label_set_text(ctx->lbl_trend, ui_data_trend_str(pt->trend));
        lv_obj_set_style_text_color(ctx->lbl_trend, fg, 0);

        /* Pill */
        ui_pill_update(ctx->lbl_pill, pt->status);

        /* Timestamp */
        if (pt->last_sample.valid) {
            char tsbuf[32], tbuf[10];
            ui_data_fmt_hhmm(&pt->last_sample.timestamp, tbuf, sizeof(tbuf));
            snprintf(tsbuf, sizeof(tsbuf), "%s  ID %03u", tbuf, pt->id);
            lv_label_set_text(ctx->lbl_ts, tsbuf);
        }

        /* Dados obsoletos */
        if (pt->last_sample.valid) {
            int32_t age = ui_data_seconds_between(&pt->last_sample.timestamp,
                                                   &state->current_time);
            if (age > UI_STALE_THRESHOLD_S)
                lv_obj_clear_flag(ctx->lbl_stale, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(ctx->lbl_stale, LV_OBJ_FLAG_HIDDEN);
        }

        /* Borda do card */
        _apply_card_border(ctx->card, pt->status);
    }
}

/* =============================================================
 * API pública
 * ============================================================= */
static void _home_create(void)
{
    if (s_home.created) return;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &ui_style_screen, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_home.topbar = ui_topbar_create(scr);
    s_home.navbar = ui_navbar_create(scr);

    /* Área de scroll dos cards */
    lv_obj_t *scroll = lv_obj_create(scr);
    lv_obj_remove_style_all(scroll);
    /* Altura restante: 480 - 56(topbar) - 52(navbar) = 372 */
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
    s_home.scroll_cont = scroll;

    /* Cria cards com dados iniciais */
    const ui_app_state_t *state = ui_data_get_state();
    s_home.card_count = 0;
    memset(s_home.cards, 0, sizeof(s_home.cards));

    for (uint8_t i = 0; i < state->patient_count && i < HOME_MAX_CARDS; i++) {
        if (state->patients[i].active) {
            _card_create(scroll, &state->patients[i], s_home.card_count++);
        }
    }

    /* Timer — começa pausado */
    s_home.timer = lv_timer_create(_home_timer_cb, 2000, NULL);
    lv_timer_pause(s_home.timer);

    s_home.screen  = scr;
    s_home.created = true;
    ESP_LOGI(TAG, "Criada. %u card(s).", s_home.card_count);
}

void screen_home_load(void)
{
    /* Se um novo paciente foi adicionado, recria a tela */
    const ui_app_state_t *state = ui_data_get_state();
    if (s_home.created && state->patient_count != s_home.card_count) {
        s_home.created = false;
        /* O lv_screen (lv_obj) associado ainda existe — LVGL gerencia a memória.
           Ao chamar lv_scr_load_anim com uma nova tela, a antiga é deletada
           se del_prev=true. Aqui forçamos recriação. */
    }

    _home_create();
    ui_navbar_set_active(s_home.navbar, UI_NAV_HOME);
    lv_scr_load_anim(s_home.screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    lv_timer_resume(s_home.timer);
}

void screen_home_pause(void)
{
    if (s_home.timer) lv_timer_pause(s_home.timer);
}
