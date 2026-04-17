#include "lvgl.h"
#include "ui_data.h"
#include "ui_styles.h"
#include "ui_widgets.h"
#include "ui_nav.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

/* Forward declaration necessaria para _del_confirm_cb */
static void _patients_create(void);
void screen_patients_load(void);

static const char *TAG = "[SCREEN_PATIENTS]";

/* =============================================================
 * SCREEN PATIENTS — Gerenciamento de pacientes
 *
 * Layout:
 *   [Topbar 800×56]
 *   [Navbar 800×52]
 *   [Scroll 800×372]
 *     → Lista de pacientes com botões Editar / Excluir
 *     → Botão "+ Adicionar paciente"
 *
 * Reconstrói a lista ao carregar e após CRUD.
 * ============================================================= */

static struct {
    lv_obj_t   *screen;
    lv_obj_t   *scroll_cont;
    ui_topbar_t *topbar;
    ui_navbar_t *navbar;
    bool        created;
} s_patients;

/* ---- Contexto para modal de exclusão ---- */
typedef struct {
    uint8_t patient_id;
} del_ctx_t;

static del_ctx_t s_del_ctx;

static void _del_confirm_cb(bool confirmed, void *user_data)
{
    LV_UNUSED(user_data);
    if (confirmed) {
        ui_data_remove_patient(s_del_ctx.patient_id);
        /* Recarrega a tela para refletir a remoção */
        screen_patients_load();
    }
}

/* ---- Callback Excluir ---- */
static void _btn_del_cb(lv_event_t *e)
{
    uint8_t pid = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    const ui_patient_t *pt = ui_data_get_patient(pid);
    if (!pt) return;

    s_del_ctx.patient_id = pid;

    char body[128];
    snprintf(body, sizeof(body),
             "Remover \"%s\" (ID %03u)?\nTodos os dados serao perdidos.",
             pt->name, pt->id);
    ui_confirm_show("Excluir paciente", body, _del_confirm_cb, NULL);
}

/* ---- Callback Editar → abre tela de cadastro em modo edição ---- */
static void _btn_edit_cb(lv_event_t *e)
{
    uint8_t pid = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    /* screen_register suporta modo edição via variável global */
    extern void screen_register_load_edit(uint8_t patient_id);
    screen_register_load_edit(pid);
}

/* ---- Reconstrói a lista ---- */
static void _btn_add_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_nav_go(UI_SCREEN_REGISTER);
}

static void _rebuild_list(void)
{
    lv_obj_clean(s_patients.scroll_cont);

    const ui_app_state_t *state = ui_data_get_state();

    if (state->patient_count == 0) {
        lv_obj_t *lbl = lv_label_create(s_patients.scroll_cont);
        lv_obj_add_style(lbl, &ui_style_label_muted, 0);
        lv_label_set_text(lbl, "Nenhum paciente cadastrado.");
        lv_obj_center(lbl);
    }

    /* Lista de pacientes */
    lv_obj_t *list_card = lv_obj_create(s_patients.scroll_cont);
    lv_obj_add_style(list_card, &ui_style_card, 0);
    lv_obj_set_size(list_card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(list_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(list_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_row(list_card, 0, 0);
    lv_obj_set_style_pad_all(list_card, 0, 0);

    for (uint8_t i = 0; i < state->patient_count; i++) {
        const ui_patient_t *pt = &state->patients[i];
        if (!pt->active) continue;

        lv_obj_t *row = lv_obj_create(list_card);
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

        /* Avatar + info */
        lv_obj_t *av = lv_obj_create(row);
        lv_obj_remove_style_all(av);
        lv_obj_set_size(av, 40, 40);
        lv_obj_set_style_radius(av, 20, 0);
        lv_obj_set_style_bg_opa(av, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(av, ui_status_bg_color(pt->status), 0);
        lv_obj_clear_flag(av, LV_OBJ_FLAG_SCROLLABLE);
        {
            char ini[4] = {0};
            ini[0] = pt->name[0];
            const char *sp = strchr(pt->name, ' ');
            if (sp && *(sp+1)) ini[1] = *(sp+1);
            lv_obj_t *lbl_i = lv_label_create(av);
            lv_obj_set_style_text_font(lbl_i, UI_FONT_SMALL, 0);
            lv_obj_set_style_text_color(lbl_i, ui_status_fg_color(pt->status), 0);
            lv_label_set_text(lbl_i, ini);
            lv_obj_center(lbl_i);
        }

        lv_obj_t *col_info = lv_obj_create(row);
        lv_obj_remove_style_all(col_info);
        lv_obj_set_flex_grow(col_info, 1);
        lv_obj_set_flex_flow(col_info, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_left(col_info, 12, 0);
        lv_obj_set_style_pad_row(col_info, 2, 0);

        lv_obj_t *lbl_name = lv_label_create(col_info);
        lv_obj_set_style_text_font(lbl_name, UI_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(lbl_name, UI_COLOR_TEXT_PRIMARY, 0);
        lv_label_set_text(lbl_name, pt->name);

        lv_obj_t *lbl_meta = lv_label_create(col_info);
        lv_obj_add_style(lbl_meta, &ui_style_label_small, 0);
        {
            char mbuf[24];
            snprintf(mbuf, sizeof(mbuf), "ID %03u  %u anos", pt->id, pt->age);
            lv_label_set_text(lbl_meta, mbuf);
        }

        /* Botões de ação */
        lv_obj_t *row_act = lv_obj_create(row);
        lv_obj_remove_style_all(row_act);
        lv_obj_set_flex_flow(row_act, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_act, LV_FLEX_ALIGN_END,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row_act, 8, 0);

        lv_obj_t *btn_edit = lv_button_create(row_act);
        lv_obj_add_style(btn_edit, &ui_style_btn_ghost, 0);
        lv_obj_t *lbl_edit = lv_label_create(btn_edit);
        lv_label_set_text(lbl_edit, "Editar");
        lv_obj_set_style_text_font(lbl_edit, UI_FONT_SMALL, 0);
        lv_obj_center(lbl_edit);
        lv_obj_add_event_cb(btn_edit, _btn_edit_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)pt->id);

        lv_obj_t *btn_del = lv_button_create(row_act);
        lv_obj_add_style(btn_del, &ui_style_btn_danger, 0);
        lv_obj_t *lbl_del = lv_label_create(btn_del);
        lv_label_set_text(lbl_del, "Excluir");
        lv_obj_set_style_text_font(lbl_del, UI_FONT_SMALL, 0);
        lv_obj_center(lbl_del);
        lv_obj_add_event_cb(btn_del, _btn_del_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)pt->id);
    }

    /* Botão Adicionar */
    lv_obj_t *btn_add = lv_button_create(s_patients.scroll_cont);
    lv_obj_set_size(btn_add, LV_PCT(100), UI_BTN_MIN_H);
    lv_obj_set_style_bg_opa(btn_add, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn_add, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(btn_add, 1, 0);
    lv_obj_set_style_border_opa(btn_add, LV_OPA_50, 0);
    lv_obj_set_style_radius(btn_add, UI_CARD_RADIUS, 0);

    lv_obj_t *lbl_add = lv_label_create(btn_add);
    lv_obj_set_style_text_font(lbl_add, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_add, UI_COLOR_TEXT_SECONDARY, 0);
    lv_label_set_text(lbl_add, "+ Adicionar paciente");
    lv_obj_center(lbl_add);

    lv_obj_add_event_cb(btn_add, _btn_add_cb, LV_EVENT_CLICKED, NULL);
}

/* =============================================================
 * API pública
 * ============================================================= */
static void _patients_create(void)
{
    if (s_patients.created) return;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &ui_style_screen, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_patients.topbar = ui_topbar_create(scr);
    s_patients.navbar = ui_navbar_create(scr);

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
    s_patients.scroll_cont = scroll;

    s_patients.screen  = scr;
    s_patients.created = true;
    ESP_LOGI(TAG, "Criada.");
}

void screen_patients_load(void)
{
    _patients_create();
    _rebuild_list();
    ui_navbar_set_active(s_patients.navbar, UI_NAV_PATIENTS);
    const ui_app_state_t *state = ui_data_get_state();
    ui_topbar_update(s_patients.topbar, state);
    lv_scr_load_anim(s_patients.screen, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
}