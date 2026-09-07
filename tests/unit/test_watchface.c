#include "minitest.h"

#include <stddef.h>
#include <string.h>

#include "watchface.h"

MT_TEST(test_wf_glyph_rows_are_valid)
{
    for (char c = '0'; c <= '9'; c++) {
        const uint8_t *g = wf_glyph_for(c);
        CHECK(g != NULL);
    }
    /* 数字 0 与字母 O 形状不同（旧版即如此） */
    CHECK(wf_glyph_for('0') != wf_glyph_for('O'));
    CHECK_EQ(wf_glyph_for('0')[0], 0x0E);
    CHECK_EQ(wf_glyph_for('O')[0], 0x0E); /* 首行相同，但整体不同 */
    CHECK(wf_glyph_for('0')[2] != wf_glyph_for('O')[2]);
}

MT_TEST(test_wf_unknown_glyphs_fall_back_to_blank)
{
    const uint8_t *blank = wf_glyph_for(' ');
    const uint8_t *tilde = wf_glyph_for('~');
    const uint8_t *nul   = wf_glyph_for('\0');

    CHECK(blank != NULL);
    for (int i = 0; i < WF_GLYPH_ROWS; i++) {
        CHECK_EQ(blank[i], 0);
        CHECK_EQ(tilde[i], 0);
        CHECK_EQ(nul[i], 0);
    }
}

MT_TEST(test_wf_symbol_glyphs_are_distinct)
{
    const uint8_t *pct = wf_glyph_for('%');
    const uint8_t *colon = wf_glyph_for(':');
    const uint8_t *arrow = wf_glyph_for('>');
    const uint8_t *under = wf_glyph_for('_');

    CHECK_EQ(pct[0], 0x19);
    CHECK_EQ(colon[0], 0x00);
    CHECK_EQ(arrow[0], 0x10);
    CHECK_EQ(under[WF_GLYPH_ROWS - 1], 0x1F);
}

MT_TEST(test_wf_lowercase_maps_to_uppercase_glyph)
{
    CHECK(wf_glyph_for('a') == wf_glyph_for('A'));
    CHECK(wf_glyph_for('z') == wf_glyph_for('Z'));
    CHECK(wf_glyph_for('m') == wf_glyph_for('M'));
}

MT_TEST(test_wf_glyph_pixel_mask)
{
    const uint8_t glyph[WF_GLYPH_ROWS] = { 0x1F, 0x11, 0x00, 0x04, 0x0A, 0x11, 0x0E };

    CHECK(wf_glyph_pixel(glyph, 0, 0));
    CHECK(wf_glyph_pixel(glyph, 0, WF_GLYPH_COLS - 1));
    CHECK_FALSE(wf_glyph_pixel(glyph, 2, 0));   /* 全 0 行 */
    CHECK_FALSE(wf_glyph_pixel(glyph, 1, 1));   /* 0x11 的第 2 列为 0 */
    CHECK(wf_glyph_pixel(glyph, 1, 4));
    CHECK(wf_glyph_pixel(glyph, 3, 2));         /* 0x04 → 中间列 */

    /* 越界与空指针都返回 false */
    CHECK_FALSE(wf_glyph_pixel(NULL, 0, 0));
    CHECK_FALSE(wf_glyph_pixel(glyph, -1, 0));
    CHECK_FALSE(wf_glyph_pixel(glyph, WF_GLYPH_ROWS, 0));
    CHECK_FALSE(wf_glyph_pixel(glyph, 0, -1));
    CHECK_FALSE(wf_glyph_pixel(glyph, 0, WF_GLYPH_COLS));
}

MT_TEST(test_wf_text_width)
{
    CHECK_EQ(wf_dot_text_width("", 4), 0);
    CHECK_EQ(wf_dot_text_width(NULL, 4), 0);
    CHECK_EQ(wf_dot_text_width("A", 4), 1 * WF_GLYPH_ADVANCE * 4 - 4);
    CHECK_EQ(wf_dot_text_width("AB", 4), 2 * WF_GLYPH_ADVANCE * 4 - 4);
    CHECK_EQ(wf_dot_text_width("ABC", 1), 3 * WF_GLYPH_ADVANCE - 1);
    CHECK_EQ(wf_glyph_advance(4), WF_GLYPH_ADVANCE * 4);
}

MT_TEST(test_wf_alignment_helpers)
{
    CHECK_EQ(wf_right_align_x(319, "27%", 4), 319 - wf_dot_text_width("27%", 4));
    CHECK_EQ(wf_center_align_x(360, "CODEX", 4), (360 - wf_dot_text_width("CODEX", 4)) / 2);
    /* 空串居中即屏幕中心 */
    CHECK_EQ(wf_center_align_x(360, "", 4), 180);
}

MT_TEST(test_wf_progress_steps_rounds_half_up)
{
    CHECK_EQ(wf_progress_steps(25, 27), 7);
    CHECK_EQ(wf_progress_steps(25, 73), 18);
    CHECK_EQ(wf_progress_steps(25, 0), 0);
    CHECK_EQ(wf_progress_steps(25, 100), 25);
    CHECK_EQ(wf_progress_steps(0, 50), 0);
    /* 与旧版表达式 (DOTS * PCT + 50) / 100 完全一致 */
    for (int pct = 0; pct <= UI_PCT_FULL; pct++) {
        CHECK_EQ(wf_progress_steps(25, pct), (25 * pct + 50) / 100);
    }
}

MT_TEST(test_wf_battery_cells_matches_legacy_formula)
{
    /* 旧版：(pct + 33) / 34，100% 时是 3 格 */
    CHECK_EQ(wf_battery_cells(100), 3);
    CHECK_EQ(wf_battery_cells(0), 0);
    CHECK_EQ(wf_battery_cells(1), 1);
    CHECK_EQ(wf_battery_cells(34), 1);
    CHECK_EQ(wf_battery_cells(35), 2);
    CHECK_EQ(wf_battery_cells(68), 2);
    CHECK_EQ(wf_battery_cells(69), 3);
    /* 越界输入先夹紧，不产生负数格 */
    CHECK_EQ(wf_battery_cells(-50), 0);
    CHECK_EQ(wf_battery_cells(500), 3);
}

MT_TEST(test_wf_format_percent)
{
    char buf[WF_PERCENT_BUFFER];

    CHECK_EQ(wf_format_percent(NULL, 0u, 42), 0);
    CHECK_EQ(wf_format_percent(buf, 0u, 42), 0);

    (void)wf_format_percent(buf, sizeof(buf), 27);
    CHECK_STR_EQ(buf, "27%");
    (void)wf_format_percent(buf, sizeof(buf), 100);
    CHECK_STR_EQ(buf, "100%");
    (void)wf_format_percent(buf, sizeof(buf), 0);
    CHECK_STR_EQ(buf, "0%");
}
