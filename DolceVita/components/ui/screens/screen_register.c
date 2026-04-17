#include "lvgl.h"
#include "ui_data.h"
#include "ui_styles.h"
#include "ui_widgets.h"
#include "ui_nav.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "[SCREEN_REGISTER]";

/* =============================================================
 * SCREEN REGISTER — Cadastro / Edição de paciente
 *
 * Layout:
 *   [Topbar 800×56]
 *   [Card central com form]
 *     → Campo Nome (lv_textarea)
 *     → Campo Idade (lv_textarea)
 *     → Label de erro (oculto)
 *     → Botões Salvar / Cancelar
 *
 * Modo edição: patient_id != 0xFF → preenche campos
 * ============================================================= */

#define REGISTER_NO_EDIT  0xFF

static struct {
    lv_obj_t   *screen;
    ui_topbar_t *topbar;
    lv_obj_t   *ta_name;
    lv_obj_t   *ta_age;
    lv_obj_t   *lbl_error;
    lv_obj_t   *kb;           /* teclado virtual */
    uint8_t     edit_id;      /* REGISTER_NO_EDIT = novo */
    bool        created;
} s_register;

/* ---- Teclado virtual ---- */
static void _ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    if (s_register.kb) {
        lv_keyboard_set_textarea(s_register.kb, ta);
        lv_obj_clear_flag(s_register.kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void _kb_ready_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (s_register.kb) lv_obj_add_flag(s_register.kb, LV_OBJ_FLAG_HIDDEN);
}

/* ---- Validação e salvamento ---- */
static void _btn_save_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    const char *name = lv_textarea_get_text(s_register.ta_name);
    const char *age_str = lv_textarea_get_text(s_register.ta_age);

    /* Validação: nome não vazio */
    if (!name || strlen(name) < 2) {
        lv_obj_set_style_text_color(s_register.lbl_error, UI_COLOR_CRIT, 0);
        lv_label_set_text(s_register.lbl_error, "Nome deve ter ao menos 2 caracteres.");
        lv_obj_clear_flag(s_register.lbl_error, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* Validação: idade numérica 1–120 */
    int age = atoi(age_str);
    if (age < 1 || age > 120) {
        lv_obj_set_style_text_color(s_register.lbl_error, UI_COLOR_CRIT, 0);
        lv_label_set_text(s_register.lbl_error, "Idade invalida (1 a 120).");
        lv_obj_clear_flag(s_register.lbl_error, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(s_register.lbl_error, LV_OBJ_FLAG_HIDDEN);

    esp_err_t ret;
    if (s_register.edit_id == REGISTER_NO_EDIT) {
        ret = ui_data_add_patient(name, (uint8_t)age);
    } else {
        ret = ui_data_update_patient_name(s_register.edit_id, name);
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Paciente salvo: %s", name);
        ui_nav_go(UI_SCREEN_PATIENTS);
    } else if (ret == ESP_ERR_NO_MEM) {
        lv_label_set_text(s_register.lbl_error,
                           "Limite de pacientes atingido.");
        lv_obj_clear_flag(s_register.lbl_error, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(s_register.lbl_error,
                           "Erro ao salvar. Verifique o SPIFFS.");
        lv_obj_clear_flag(s_register.lbl_error, LV_OBJ_FLAG_HIDDEN);
    }
}

static void _btn_cancel_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_nav_go(UI_SCREEN_PATIENTS);
}

/* =============================================================
 * Criação da tela (uma única vez)
 * ============================================================= */
static void _register_create(void)
{
    if (s_register.created) return;

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &ui_style_screen, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_register.topbar = ui_topbar_create(scr);

    /* Card central do formulário */
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_add_style(card, &ui_style_card, 0);
    /* Centralizado horizontalmente, largura 480 px (bom para tablet/display 800px) */
    lv_obj_set_size(card, 480, LV_SIZE_CONTENT);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 16, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Título do form */
    lv_obj_t *lbl_title = lv_label_create(card);
    lv_obj_set_style_text_font(lbl_title, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_TEXT_PRIMARY, 0);
    lv_label_set_text(lbl_title, "Cadastrar Paciente");

    ui_divider_create(card);

    /* Campo Nome */
    lv_obj_t *lbl_name = lv_label_create(card);
    lv_obj_add_style(lbl_name, &ui_style_label_small, 0);
    lv_label_set_text(lbl_name, "Nome completo");

    s_register.ta_name = lv_textarea_create(card);
    lv_obj_set_size(s_register.ta_name, LV_PCT(100), 48);
    lv_textarea_set_one_line(s_register.ta_name, true);
    lv_textarea_set_max_length(s_register.ta_name, UI_PATIENT_NAME_LEN - 1);
    lv_textarea_set_placeholder_text(s_register.ta_name, "Ex: Joao Silva");
    lv_obj_set_style_bg_color(s_register.ta_name, UI_COLOR_BG_SCREEN, 0);
    lv_obj_set_style_text_color(s_register.ta_name, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_border_color(s_register.ta_name, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_color(s_register.ta_name, UI_COLOR_INFO,
                                   LV_STATE_FOCUSED);
    lv_obj_set_style_radius(s_register.ta_name, 6, 0);
    lv_obj_add_event_cb(s_register.ta_name, _ta_focus_cb,
                        LV_EVENT_FOCUSED, NULL);

    /* Campo Idade */
    lv_obj_t *lbl_age = lv_label_create(card);
    lv_obj_add_style(lbl_age, &ui_style_label_small, 0);
    lv_label_set_text(lbl_age, "Idade");

    s_register.ta_age = lv_textarea_create(card);
    lv_obj_set_size(s_register.ta_age, LV_PCT(100), 48);
    lv_textarea_set_one_line(s_register.ta_age, true);
    lv_textarea_set_max_length(s_register.ta_age, 3);
    lv_textarea_set_accepted_chars(s_register.ta_age, "0123456789");
    lv_textarea_set_placeholder_text(s_register.ta_age, "Ex: 58");
    lv_obj_set_style_bg_color(s_register.ta_age, UI_COLOR_BG_SCREEN, 0);
    lv_obj_set_style_text_color(s_register.ta_age, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_border_color(s_register.ta_age, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_color(s_register.ta_age, UI_COLOR_INFO,
                                   LV_STATE_FOCUSED);
    lv_obj_set_style_radius(s_register.ta_age, 6, 0);
    lv_obj_add_event_cb(s_register.ta_age, _ta_focus_cb,
                        LV_EVENT_FOCUSED, NULL);

    /* Label de erro (oculto por padrão) */
    s_register.lbl_error = lv_label_create(card);
    lv_obj_set_style_text_font(s_register.lbl_error, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(s_register.lbl_error, UI_COLOR_CRIT, 0);
    lv_label_set_text(s_register.lbl_error, "");
    lv_obj_add_flag(s_register.lbl_error, LV_OBJ_FLAG_HIDDEN);

    ui_divider_create(card);

    /* Linha de botões */
    lv_obj_t *row_btns = lv_obj_create(card);
    lv_obj_remove_style_all(row_btns);
    lv_obj_set_size(row_btns, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row_btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_btns, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_btns, 12, 0);

    lv_obj_t *btn_cancel = lv_button_create(row_btns);
    lv_obj_add_style(btn_cancel, &ui_style_btn_ghost, 0);
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancelar");
    lv_obj_center(lbl_cancel);
    lv_obj_add_event_cb(btn_cancel, _btn_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_save = lv_button_create(row_btns);
    lv_obj_add_style(btn_save, &ui_style_btn_primary, 0);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Salvar");
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, _btn_save_cb, LV_EVENT_CLICKED, NULL);

    /* Teclado virtual na metade inferior */
    s_register.kb = lv_keyboard_create(scr);
    lv_obj_set_size(s_register.kb, LV_PCT(100), 200);
    lv_obj_align(s_register.kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(s_register.kb, UI_COLOR_BG_SURFACE, 0);
    lv_obj_set_style_text_color(s_register.kb, UI_COLOR_TEXT_PRIMARY, 0);
    lv_obj_add_event_cb(s_register.kb, _kb_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_register.kb, _kb_ready_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(s_register.kb, LV_OBJ_FLAG_HIDDEN);

    s_register.screen  = scr;
    s_register.edit_id = REGISTER_NO_EDIT;
    s_register.created = true;
    ESP_LOGI(TAG, "Criada.");
}

/* =============================================================
 * API pública
 * ============================================================= */
void screen_register_load(void)
{
    _register_create();

    s_register.edit_id = REGISTER_NO_EDIT;

    /* Limpa os campos */
    lv_textarea_set_text(s_register.ta_name, "");
    lv_textarea_set_text(s_register.ta_age,  "");
    lv_obj_add_flag(s_register.lbl_error, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_register.kb, LV_OBJ_FLAG_HIDDEN);

    /* Atualiza título */
    lv_obj_t *title = lv_obj_get_child(
        lv_obj_get_child(s_register.screen, 1), 0);
    if (title) lv_label_set_text(title, "Cadastrar Paciente");

    const ui_app_state_t *state = ui_data_get_state();
    ui_topbar_update(s_register.topbar, state);

    lv_scr_load_anim(s_register.screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void screen_register_load_edit(uint8_t patient_id)
{
    _register_create();

    s_register.edit_id = patient_id;

    const ui_patient_t *pt = ui_data_get_patient(patient_id);
    if (!pt) {
        screen_register_load();
        return;
    }

    lv_textarea_set_text(s_register.ta_name, pt->name);
    char age_buf[4];
    snprintf(age_buf, sizeof(age_buf), "%u", pt->age);
    lv_textarea_set_text(s_register.ta_age, age_buf);
    lv_obj_add_flag(s_register.lbl_error, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_register.kb, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_obj_get_child(
        lv_obj_get_child(s_register.screen, 1), 0);
    if (title) lv_label_set_text(title, "Editar Paciente");

    const ui_app_state_t *state = ui_data_get_state();
    ui_topbar_update(s_register.topbar, state);

    lv_scr_load_anim(s_register.screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
    ESP_LOGI(TAG, "Modo edicao: ID %u - %s", patient_id, pt->name);
}
