#include "ui.h"
#include "ui_screen.h"

#include "about_layout.h"
#include "ui_theme.h"

static lv_obj_t *scr = NULL;

void screen_about_init(void)
{
    scr = ui_screen_create(NAV_SCREEN_ABOUT);
    ui_screen_set_bg(scr, ui_theme_color(UI_THEME_DARK, UI_ROLE_BG));

    /* 中间透明面板：让手势在屏幕中心区域也能被接收 */
    lv_obj_t *panel = ui_panel_create(scr, ABOUT_PANEL_W, ABOUT_PANEL_H);
    ui_screen_attach_nav(panel, NAV_SCREEN_ABOUT);

    (void)ui_label_create_aligned(scr, ABOUT_TEXT_DESIGNED,
                                  ui_theme_color(UI_THEME_DARK, UI_ROLE_TEXT_ACCENT),
                                  &lv_font_montserrat_16,
                                  ABOUT_LABEL_DESIGNED_X, ABOUT_LABEL_DESIGNED_Y, LV_ALIGN_CENTER);
    (void)ui_label_create_aligned(scr, ABOUT_TEXT_NAME,
                                  ui_theme_color(UI_THEME_DARK, UI_ROLE_TEXT_PRIMARY),
                                  &lv_font_montserrat_32,
                                  ABOUT_LABEL_NAME_X, ABOUT_LABEL_NAME_Y, LV_ALIGN_CENTER);
    (void)ui_label_create_aligned(scr, ABOUT_TEXT_HINT,
                                  ui_theme_color(UI_THEME_DARK, UI_ROLE_TEXT_GOOD),
                                  &lv_font_montserrat_16,
                                  ABOUT_LABEL_HINT_X, ABOUT_LABEL_HINT_Y, LV_ALIGN_CENTER);
}

lv_obj_t **screen_about_get_ptr(void) { return &scr; }
