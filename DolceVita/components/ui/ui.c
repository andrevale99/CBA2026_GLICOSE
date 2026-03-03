#include "ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "UI";

/* =========================
 * OBJETOS
 * ========================= */

static lv_obj_t *label_pergunta;

/* =========================
 * EVENTOS
 * ========================= */

static void btn_sim_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
    {
        lv_label_set_text(label_pergunta, "Eu ja sabia! :)");
        lv_obj_set_style_text_color(
            label_pergunta,
            lv_palette_main(LV_PALETTE_GREEN),
            0);
    }
}

static void btn_nao_fuga_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_PRESSED)
        return;

    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *parent = lv_obj_get_parent(btn);

    lv_coord_t parent_w = lv_obj_get_width(parent);
    lv_coord_t parent_h = lv_obj_get_height(parent);

    lv_coord_t btn_w = lv_obj_get_width(btn);
    lv_coord_t btn_h = lv_obj_get_height(btn);

    if (parent_w <= btn_w || parent_h <= btn_h)
        return;

    lv_coord_t max_x = parent_w - btn_w;
    lv_coord_t max_y = parent_h - btn_h;

    lv_coord_t new_x = esp_random() % max_x;
    lv_coord_t new_y = esp_random() % max_y;

    lv_obj_set_pos(btn, new_x, new_y);
}

/* =========================
 * CRIAÇÃO DA UI
 * ========================= */

void ui_main_create(void)
{
    lv_obj_t *scr = lv_screen_active();

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0A0A), 0);

    /* ---------- Pergunta ---------- */
    label_pergunta = lv_label_create(scr);
    lv_label_set_text(label_pergunta, "Qual eh o melhor?");
    lv_obj_set_style_text_font(label_pergunta, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_pergunta, lv_color_white(), 0);
    lv_obj_align(label_pergunta, LV_ALIGN_TOP_MID, 0, 80);

    /* ---------- Botão SIM (fixo) ---------- */
    lv_obj_t *btn_sim = lv_button_create(scr);
    lv_obj_set_size(btn_sim, 150, 60);
    lv_obj_align(btn_sim, LV_ALIGN_CENTER, -120, 50);
    lv_obj_set_style_bg_color(
        btn_sim,
        lv_palette_main(LV_PALETTE_GREEN),
        0);

    lv_obj_add_event_cb(
        btn_sim,
        btn_sim_event_cb,
        LV_EVENT_CLICKED,
        NULL);

    lv_obj_t *lbl_sim = lv_label_create(btn_sim);
    lv_label_set_text(lbl_sim, "LASEM");
    lv_obj_center(lbl_sim);

    /* ---------- Botão NÃO (posição manual!) ---------- */
    lv_obj_t *btn_nao = lv_button_create(scr);
    lv_obj_set_size(btn_nao, 150, 60);

    /* POSIÇÃO INICIAL MANUAL — NÃO usar align aqui */
    lv_obj_set_pos(btn_nao, 450, 250);

    lv_obj_set_style_bg_color(
        btn_nao,
        lv_palette_main(LV_PALETTE_RED),
        0);

    lv_obj_add_event_cb(
        btn_nao,
        btn_nao_fuga_event_cb,
        LV_EVENT_PRESSED,
        NULL);

    lv_obj_t *lbl_nao = lv_label_create(btn_nao);
    lv_label_set_text(lbl_nao, "LIME");
    lv_obj_center(lbl_nao);

    ESP_LOGI(TAG, "UI criada com sucesso");
}