#include "ui.h"

#include "app_config.h"
#include "nav_map.h"

#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH must be 16"
#endif
#if LV_COLOR_16_SWAP != 1
    #error "LV_COLOR_16_SWAP must be 1"
#endif

/* 主题默认色与首屏：集中在此，避免散落在函数体里 */
#define UI_DEFAULT_PRIMARY_PALETTE   LV_PALETTE_BLUE
#define UI_DEFAULT_SECONDARY_PALETTE LV_PALETTE_RED
#define UI_DEFAULT_DARK_MODE         true
#define UI_STARTUP_SCREEN            NAV_SCREEN_CODEX_USAGE

void ui_init(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(
        disp,
        lv_palette_main(UI_DEFAULT_PRIMARY_PALETTE),
        lv_palette_main(UI_DEFAULT_SECONDARY_PALETTE),
        UI_DEFAULT_DARK_MODE,
        LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, theme);

    screen_codex_usage_init();
    lv_disp_load_scr(*screen_codex_usage_get_ptr());
}
