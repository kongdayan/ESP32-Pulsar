#include "ui_theme.h"

/* palette[theme][role] — 与旧版各屏 color_xxx() 的取值逐一对应 */
static const uint32_t k_palette[UI_THEME_COUNT][UI_ROLE_COUNT] = {
    /* UI_THEME_DARK  */ { 0x02050A, 0xFFFFFF, 0x7CAEF6, 0x70F52A, 0x0B254D,
                           0x1F7AFF, 0x1684FF, 0xFFFFFF, 0x1E9BFF, 0x17446D,
                           0x70F52A, 0x315C43, 0x0B6CFF, 0xFFFFFF },
    /* UI_THEME_LIGHT */ { 0xF8FBFF, 0x061B4D, 0x1467F2, 0x2BA70E, 0xD8E8FA,
                           0x7CAEF6, 0x1467F2, 0x16264C, 0x075FF0, 0xD8E7F8,
                           0x2BA70E, 0xDCEED5, 0x0B6CFF, 0xFFFFFF },
};

bool ui_theme_is_valid(ui_theme_mode_t mode)
{
    return mode >= UI_THEME_DARK && mode < UI_THEME_COUNT;
}

bool ui_theme_role_is_valid(ui_color_role_t role)
{
    return role >= UI_ROLE_BG && role < UI_ROLE_COUNT;
}

ui_theme_mode_t ui_theme_toggle(ui_theme_mode_t mode)
{
    if (!ui_theme_is_valid(mode)) return UI_THEME_DARK;
    return (mode == UI_THEME_LIGHT) ? UI_THEME_DARK : UI_THEME_LIGHT;
}

uint32_t ui_theme_color(ui_theme_mode_t mode, ui_color_role_t role)
{
    if (!ui_theme_is_valid(mode) || !ui_theme_role_is_valid(role)) {
        return k_palette[UI_THEME_DARK][UI_ROLE_BG];
    }
    return k_palette[mode][role];
}

bool ui_theme_is_light(ui_theme_mode_t mode)
{
    return mode == UI_THEME_LIGHT;
}
