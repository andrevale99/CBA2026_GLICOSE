#include "ui_styles.h"

lv_style_t ui_style_screen;
lv_style_t ui_style_card;
lv_style_t ui_style_card_ok;
lv_style_t ui_style_card_warn;
lv_style_t ui_style_card_crit;
lv_style_t ui_style_btn_primary;
lv_style_t ui_style_btn_danger;
lv_style_t ui_style_btn_ghost;
lv_style_t ui_style_label_small;
lv_style_t ui_style_label_muted;
lv_style_t ui_style_topbar;
lv_style_t ui_style_navbar;
lv_style_t ui_style_navbar_btn_active;

void ui_styles_init(void)
{
    /* ---- Fundo de tela ---- */
    lv_style_init(&ui_style_screen);
    lv_style_set_bg_color(&ui_style_screen, UI_COLOR_BG_SCREEN);
    lv_style_set_bg_opa(&ui_style_screen, LV_OPA_COVER);
    lv_style_set_pad_all(&ui_style_screen, 0);
    lv_style_set_border_width(&ui_style_screen, 0);

    /* ---- Card base ---- */
    lv_style_init(&ui_style_card);
    lv_style_set_bg_color(&ui_style_card, UI_COLOR_BG_CARD);
    lv_style_set_bg_opa(&ui_style_card, LV_OPA_COVER);
    lv_style_set_border_color(&ui_style_card, UI_COLOR_BORDER);
    lv_style_set_border_width(&ui_style_card, 1);
    lv_style_set_radius(&ui_style_card, UI_CARD_RADIUS);
    lv_style_set_pad_all(&ui_style_card, UI_CARD_PAD);

    /* ---- Cards com borda lateral de status ---- */
    lv_style_init(&ui_style_card_ok);
    lv_style_set_border_color(&ui_style_card_ok, UI_COLOR_OK);
    lv_style_set_border_width(&ui_style_card_ok, 3);
    lv_style_set_border_side(&ui_style_card_ok, LV_BORDER_SIDE_LEFT);

    lv_style_init(&ui_style_card_warn);
    lv_style_set_border_color(&ui_style_card_warn, UI_COLOR_WARN);
    lv_style_set_border_width(&ui_style_card_warn, 3);
    lv_style_set_border_side(&ui_style_card_warn, LV_BORDER_SIDE_LEFT);

    lv_style_init(&ui_style_card_crit);
    lv_style_set_border_color(&ui_style_card_crit, UI_COLOR_CRIT);
    lv_style_set_border_width(&ui_style_card_crit, 3);
    lv_style_set_border_side(&ui_style_card_crit, LV_BORDER_SIDE_LEFT);

    /* ---- Botão primário (azul info) ---- */
    lv_style_init(&ui_style_btn_primary);
    lv_style_set_bg_color(&ui_style_btn_primary, UI_COLOR_INFO);
    lv_style_set_bg_opa(&ui_style_btn_primary, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_btn_primary, lv_color_white());
    lv_style_set_radius(&ui_style_btn_primary, 8);
    lv_style_set_pad_ver(&ui_style_btn_primary, 12);
    lv_style_set_pad_hor(&ui_style_btn_primary, 20);
    lv_style_set_min_height(&ui_style_btn_primary, UI_BTN_MIN_H);
    lv_style_set_border_width(&ui_style_btn_primary, 0);

    /* ---- Botão danger (vermelho) ---- */
    lv_style_init(&ui_style_btn_danger);
    lv_style_set_bg_color(&ui_style_btn_danger, UI_COLOR_CRIT);
    lv_style_set_bg_opa(&ui_style_btn_danger, LV_OPA_COVER);
    lv_style_set_text_color(&ui_style_btn_danger, lv_color_white());
    lv_style_set_radius(&ui_style_btn_danger, 8);
    lv_style_set_pad_ver(&ui_style_btn_danger, 10);
    lv_style_set_pad_hor(&ui_style_btn_danger, 16);
    lv_style_set_min_height(&ui_style_btn_danger, UI_BTN_MIN_H);
    lv_style_set_border_width(&ui_style_btn_danger, 0);

    /* ---- Botão ghost (contorno) ---- */
    lv_style_init(&ui_style_btn_ghost);
    lv_style_set_bg_opa(&ui_style_btn_ghost, LV_OPA_TRANSP);
    lv_style_set_border_color(&ui_style_btn_ghost, UI_COLOR_BORDER);
    lv_style_set_border_width(&ui_style_btn_ghost, 1);
    lv_style_set_text_color(&ui_style_btn_ghost, UI_COLOR_TEXT_SECONDARY);
    lv_style_set_radius(&ui_style_btn_ghost, 8);
    lv_style_set_pad_ver(&ui_style_btn_ghost, 10);
    lv_style_set_pad_hor(&ui_style_btn_ghost, 14);
    lv_style_set_min_height(&ui_style_btn_ghost, UI_BTN_MIN_H);

    /* ---- Labels auxiliares ---- */
    lv_style_init(&ui_style_label_small);
    lv_style_set_text_color(&ui_style_label_small, UI_COLOR_TEXT_SECONDARY);
    lv_style_set_text_font(&ui_style_label_small, UI_FONT_SMALL);

    lv_style_init(&ui_style_label_muted);
    lv_style_set_text_color(&ui_style_label_muted, UI_COLOR_TEXT_MUTED);
    lv_style_set_text_font(&ui_style_label_muted, UI_FONT_SMALL);

    /* ---- Topbar ---- */
    lv_style_init(&ui_style_topbar);
    lv_style_set_bg_color(&ui_style_topbar, UI_COLOR_BG_SURFACE);
    lv_style_set_bg_opa(&ui_style_topbar, LV_OPA_COVER);
    lv_style_set_border_color(&ui_style_topbar, UI_COLOR_BORDER);
    lv_style_set_border_width(&ui_style_topbar, 1);
    lv_style_set_border_side(&ui_style_topbar, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_radius(&ui_style_topbar, 0);
    lv_style_set_pad_hor(&ui_style_topbar, 16);
    lv_style_set_pad_ver(&ui_style_topbar, 0);

    /* ---- Navbar ---- */
    lv_style_init(&ui_style_navbar);
    lv_style_set_bg_color(&ui_style_navbar, UI_COLOR_BG_SURFACE);
    lv_style_set_bg_opa(&ui_style_navbar, LV_OPA_COVER);
    lv_style_set_border_color(&ui_style_navbar, UI_COLOR_BORDER);
    lv_style_set_border_width(&ui_style_navbar, 1);
    lv_style_set_border_side(&ui_style_navbar, LV_BORDER_SIDE_TOP);
    lv_style_set_radius(&ui_style_navbar, 0);
    lv_style_set_pad_all(&ui_style_navbar, 0);

    /* ---- Navbar botão ativo ---- */
    lv_style_init(&ui_style_navbar_btn_active);
    lv_style_set_text_color(&ui_style_navbar_btn_active, UI_COLOR_INFO);
    lv_style_set_border_color(&ui_style_navbar_btn_active, UI_COLOR_INFO);
    lv_style_set_border_width(&ui_style_navbar_btn_active, 2);
    lv_style_set_border_side(&ui_style_navbar_btn_active, LV_BORDER_SIDE_TOP);
}