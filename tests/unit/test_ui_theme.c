#include "minitest.h"

#include "ui_theme.h"

MT_TEST(test_theme_validity)
{
    CHECK(ui_theme_is_valid(UI_THEME_DARK));
    CHECK(ui_theme_is_valid(UI_THEME_LIGHT));
    CHECK_FALSE(ui_theme_is_valid(UI_THEME_COUNT));
    CHECK_FALSE(ui_theme_is_valid((ui_theme_mode_t)-1));
    CHECK(ui_theme_role_is_valid(UI_ROLE_BG));
    CHECK_FALSE(ui_theme_role_is_valid(UI_ROLE_COUNT));
}

MT_TEST(test_theme_toggle)
{
    CHECK_EQ(ui_theme_toggle(UI_THEME_DARK), UI_THEME_LIGHT);
    CHECK_EQ(ui_theme_toggle(UI_THEME_LIGHT), UI_THEME_DARK);
    CHECK_EQ(ui_theme_toggle(UI_THEME_COUNT), UI_THEME_DARK); /* 非法输入回落到暗色 */
}

MT_TEST(test_theme_is_light)
{
    CHECK_FALSE(ui_theme_is_light(UI_THEME_DARK));
    CHECK(ui_theme_is_light(UI_THEME_LIGHT));
}

/* 逐条锁定与旧版 color_xxx() 相同的取值，防止改色 */
MT_TEST(test_theme_dark_palette_matches_legacy)
{
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_BG), 0x02050Au);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_TEXT_PRIMARY), 0xFFFFFFu);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_TEXT_ACCENT), 0x7CAEF6u);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_TEXT_GOOD), 0x70F52Au);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_RING), 0x0B254Du);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_TICK), 0x1F7AFFu);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_TICK_HOT), 0x1684FFu);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_RESET), 0xFFFFFFu);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_BLUE), 0x1E9BFFu);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_BLUE_DIM), 0x17446Du);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_GREEN), 0x70F52Au);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_GREEN_DIM), 0x315C43u);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_MARK), 0x0B6CFFu);
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_ON_MARK), 0xFFFFFFu);
}

MT_TEST(test_theme_light_palette_matches_legacy)
{
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_BG), 0xF8FBFFu);
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_TEXT_PRIMARY), 0x061B4Du);
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_TEXT_GOOD), 0x2BA70Eu);
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_RING), 0xD8E8FAu);
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_TICK), 0x7CAEF6u);
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_RESET), 0x16264Cu);
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_BLUE), 0x075FF0u);
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_BLUE_DIM), 0xD8E7F8u);
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_GREEN), 0x2BA70Eu);
    CHECK_EQ(ui_theme_color(UI_THEME_LIGHT, UI_ROLE_GREEN_DIM), 0xDCEED5u);
}

/* 越界访问必须落到安全值，而不是读数组外存 */
MT_TEST(test_theme_out_of_range_fallback)
{
    const uint32_t safe = ui_theme_color(UI_THEME_COUNT, UI_ROLE_BG);
    CHECK_EQ(safe, ui_theme_color(UI_THEME_DARK, UI_ROLE_BG));
    CHECK_EQ(ui_theme_color(UI_THEME_DARK, UI_ROLE_COUNT), safe);
    CHECK_EQ(ui_theme_color((ui_theme_mode_t)-1, UI_ROLE_TICK), safe);
}
