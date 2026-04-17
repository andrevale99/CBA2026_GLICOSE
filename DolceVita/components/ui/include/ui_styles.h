#pragma once

#include "lvgl.h"

/* =============================================================
 * PALETA CLÍNICA — display RGB 800x480, fundo escuro
 * ============================================================= */

/* Fundos */
#define UI_COLOR_BG_SCREEN    lv_color_hex(0x0D1117)
#define UI_COLOR_BG_SURFACE   lv_color_hex(0x161B22)
#define UI_COLOR_BG_CARD      lv_color_hex(0x1E2530)
#define UI_COLOR_BORDER       lv_color_hex(0x30363D)

/* Status semântico */
#define UI_COLOR_OK           lv_color_hex(0x3FB950)
#define UI_COLOR_OK_BG        lv_color_hex(0x0D2A14)
#define UI_COLOR_WARN         lv_color_hex(0xD29922)
#define UI_COLOR_WARN_BG      lv_color_hex(0x2A1F00)
#define UI_COLOR_CRIT         lv_color_hex(0xF85149)
#define UI_COLOR_CRIT_BG      lv_color_hex(0x2A0D0D)
#define UI_COLOR_INFO         lv_color_hex(0x58A6FF)
#define UI_COLOR_INFO_BG      lv_color_hex(0x0D1A2E)

/* Texto */
#define UI_COLOR_TEXT_PRIMARY   lv_color_hex(0xE6EDF3)
#define UI_COLOR_TEXT_SECONDARY lv_color_hex(0x7D8590)
#define UI_COLOR_TEXT_MUTED     lv_color_hex(0x484F58)

/* Dimensões */
#define UI_CARD_RADIUS      10
#define UI_CARD_PAD         14
#define UI_BTN_MIN_H        48
#define UI_TOPBAR_H         56
#define UI_NAVBAR_H         52

/*
 * Fontes — LVGL v9 via idf_component_manager.
 * O projeto original usava montserrat_24 (confirmado no main.c).
 * Fontes habilitadas por padrão no componente lvgl/lvgl para ESP-IDF:
 *   8, 10, 12, 14, 16, 18, 20, 24, 32
 * UI_FONT_GLUCOSE_BIG usa 32 (seguro, grande o suficiente para mg/dL).
 * Se quiser 48, habilite LV_FONT_MONTSERRAT_48=y no sdkconfig.
 */
#define UI_FONT_SMALL       &lv_font_montserrat_12
#define UI_FONT_NORMAL      &lv_font_montserrat_16
#define UI_FONT_LARGE       &lv_font_montserrat_20
#define UI_FONT_GLUCOSE_BIG &lv_font_montserrat_32

/* Estilos estáticos globais */
extern lv_style_t ui_style_screen;
extern lv_style_t ui_style_card;
extern lv_style_t ui_style_card_ok;
extern lv_style_t ui_style_card_warn;
extern lv_style_t ui_style_card_crit;
extern lv_style_t ui_style_btn_primary;
extern lv_style_t ui_style_btn_danger;
extern lv_style_t ui_style_btn_ghost;
extern lv_style_t ui_style_label_small;
extern lv_style_t ui_style_label_muted;
extern lv_style_t ui_style_topbar;
extern lv_style_t ui_style_navbar;
extern lv_style_t ui_style_navbar_btn_active;

void ui_styles_init(void);
