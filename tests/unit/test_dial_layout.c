/*
 * dial_layout 的纯几何 UT：环带表、角度换算、极坐标↔屏幕坐标、
 * 分格/命中、排版可用性、预设自洽性。全部是可在主机上精确断言的数学。
 */
#include "minitest.h"

#include <math.h>
#include <stddef.h>

#include "dial_layout.h"

#define EPS 0.01f

/* ── 环带表 ──────────────────────────────────────────────────────────────── */

MT_TEST(test_dial_band_table_is_ordered_and_contiguous)
{
    CHECK_EQ(DIAL_BAND_COUNT, 4);
    CHECK_NEAR(dial_bands[DIAL_BAND_EDGE].r_out, DIAL_SAFE_R_PX, EPS);
    CHECK_NEAR(dial_bands[DIAL_BAND_FOCUS].r_in, 0.0f, EPS);

    for (int i = 0; i < DIAL_BAND_COUNT; i++) {
        CHECK_EQ((int)dial_bands[i].id, i);
        CHECK(dial_bands[i].name != NULL);
        CHECK(dial_bands[i].r_out > dial_bands[i].r_in);
        if (i > 0) {
            /* 带与带之间不留缝，否则 dial_band_at 会有落空的半径 */
            CHECK_NEAR(dial_bands[i].r_out, dial_bands[i - 1].r_in, EPS);
        }
    }
}

MT_TEST(test_dial_band_at_covers_every_radius)
{
    CHECK_EQ(dial_band_at(170.0f), DIAL_BAND_EDGE);
    CHECK_EQ(dial_band_at(160.0f), DIAL_BAND_RING);
    CHECK_EQ(dial_band_at(120.0f), DIAL_BAND_CONTENT);
    CHECK_EQ(dial_band_at(30.0f), DIAL_BAND_FOCUS);

    /* 边界：带是"上含下不含"，r 正好等于分界值时归到内侧带 */
    CHECK_EQ(dial_band_at(DIAL_SAFE_R_PX), DIAL_BAND_EDGE);
    CHECK_EQ(dial_band_at(DIAL_EDGE_R_IN), DIAL_BAND_RING);
    CHECK_EQ(dial_band_at(DIAL_RING_R_IN), DIAL_BAND_CONTENT);
    CHECK_EQ(dial_band_at(DIAL_FOCUS_R_OUT), DIAL_BAND_FOCUS);
    CHECK_EQ(dial_band_at(0.0f), DIAL_BAND_FOCUS);

    /* 非法半径 */
    CHECK_EQ(dial_band_at(-1.0f), DIAL_BAND_INVALID);
    CHECK_EQ(dial_band_at(DIAL_SAFE_R_PX + 1.0f), DIAL_BAND_INVALID);
    CHECK_EQ(dial_band_at(1e6f), DIAL_BAND_INVALID);
}

MT_TEST(test_dial_band_radii_are_inside_the_glass)
{
    CHECK(DIAL_EDGE_R_IN > DIAL_RING_R_IN);
    CHECK(DIAL_RING_R_IN > DIAL_FOCUS_R_OUT);
    CHECK(DIAL_FOCUS_R_OUT > 0.0f);
    CHECK(DIAL_SAFE_R_PX < DIAL_GLASS_R_PX);
}

/* ── 角度 ────────────────────────────────────────────────────────────────── */

MT_TEST(test_dial_norm_deg)
{
    CHECK_NEAR(dial_norm_deg(0.0f), 0.0f, EPS);
    CHECK_NEAR(dial_norm_deg(360.0f), 0.0f, EPS);
    CHECK_NEAR(dial_norm_deg(450.0f), 90.0f, EPS);
    CHECK_NEAR(dial_norm_deg(-90.0f), 270.0f, EPS);
    CHECK_NEAR(dial_norm_deg(-360.0f), 0.0f, EPS);
    CHECK_NEAR(dial_norm_deg(721.0f), 1.0f, EPS);
    CHECK(dial_norm_deg(-0.0f) >= 0.0f);
}

MT_TEST(test_dial_delta_deg_wraps_shortest_way)
{
    CHECK_NEAR(dial_delta_deg(10.0f, 40.0f), 30.0f, EPS);
    CHECK_NEAR(dial_delta_deg(40.0f, 10.0f), -30.0f, EPS);
    CHECK_NEAR(dial_delta_deg(350.0f, 10.0f), 20.0f, EPS);
    CHECK_NEAR(dial_delta_deg(10.0f, 350.0f), -20.0f, EPS);
    CHECK_NEAR(dial_delta_deg(0.0f, 180.0f), -180.0f, EPS);   /* 半圈取负方向 */
    CHECK_NEAR(dial_delta_deg(0.0f, 0.0f), 0.0f, EPS);
    CHECK(dial_delta_deg(123.0f, 777.0f) >= -180.0f);
    CHECK(dial_delta_deg(123.0f, 777.0f) < 180.0f);
}

MT_TEST(test_dial_deg_to_lvgl_matches_lvgl_convention)
{
    /* LVGL 8：0° 在 3 点钟、顺时针。实测 lv_arc_set_rotation(270) 起笔落在 12 点，
     * 所以 dial 的 12 点(0°) 必须映射到 270°。 */
    CHECK_NEAR(dial_deg_to_lvgl(DIAL_DEG_TOP), 270.0f, EPS);
    CHECK_NEAR(dial_deg_to_lvgl(DIAL_DEG_RIGHT), 0.0f, EPS);
    CHECK_NEAR(dial_deg_to_lvgl(DIAL_DEG_BOTTOM), 90.0f, EPS);
    CHECK_NEAR(dial_deg_to_lvgl(DIAL_DEG_LEFT), 180.0f, EPS);

    for (int deg = 0; deg < 360; deg += 7) {
        const float d = (float)deg;
        CHECK_NEAR(dial_deg_from_lvgl(dial_deg_to_lvgl(d)), d, EPS);
    }
}

MT_TEST(test_dial_arc_span_lvgl)
{
    dial_arc_lvgl_t s = dial_arc_span_lvgl(DIAL_DEG_TOP, 90.0f);
    CHECK_EQ(s.start_lvgl, 270);
    CHECK_EQ(s.end_lvgl, 360);

    /* 满圆：LVGL 的 start==end 会画空，所以固定给 0..360 */
    s = dial_arc_span_lvgl(DIAL_DEG_RIGHT, 360.0f);
    CHECK_EQ(s.start_lvgl, 0);
    CHECK_EQ(s.end_lvgl, 360);
    s = dial_arc_span_lvgl(DIAL_DEG_TOP, 400.0f);
    CHECK_EQ(s.end_lvgl, 360);

    /* 负扫角按 0 处理 */
    s = dial_arc_span_lvgl(DIAL_DEG_TOP, -45.0f);
    CHECK_EQ(s.start_lvgl, 270);
    CHECK_EQ(s.end_lvgl, 270);
}

MT_TEST(test_dial_deg_step_and_sector_center)
{
    CHECK_NEAR(dial_deg_step(12), 30.0f, EPS);
    CHECK_NEAR(dial_deg_step(8), 45.0f, EPS);
    CHECK_NEAR(dial_deg_step(0), 0.0f, EPS);
    CHECK_NEAR(dial_deg_step(-4), 0.0f, EPS);

    CHECK_NEAR(dial_sector_center_deg(12, 0), 0.0f, EPS);
    CHECK_NEAR(dial_sector_center_deg(12, 3), 90.0f, EPS);
    CHECK_NEAR(dial_sector_center_deg(12, 11), 330.0f, EPS);
    CHECK_NEAR(dial_sector_center_deg(12, 12), 0.0f, EPS);      /* 回绕 */
    CHECK_NEAR(dial_sector_center_deg(0, 5), 0.0f, EPS);        /* 非法 count */
}

/* ── 极坐标 ↔ 屏幕坐标 ───────────────────────────────────────────────────── */

MT_TEST(test_dial_point_at_four_anchors)
{
    dial_pointf_t p = dial_point_at(100.0f, DIAL_DEG_TOP);
    CHECK_NEAR(p.x, DIAL_CX, EPS);
    CHECK_NEAR(p.y, DIAL_CY - 100.0f, EPS);

    p = dial_point_at(100.0f, DIAL_DEG_RIGHT);
    CHECK_NEAR(p.x, DIAL_CX + 100.0f, EPS);
    CHECK_NEAR(p.y, DIAL_CY, EPS);

    p = dial_point_at(100.0f, DIAL_DEG_BOTTOM);
    CHECK_NEAR(p.x, DIAL_CX, EPS);
    CHECK_NEAR(p.y, DIAL_CY + 100.0f, EPS);

    p = dial_point_at(100.0f, DIAL_DEG_LEFT);
    CHECK_NEAR(p.x, DIAL_CX - 100.0f, EPS);
    CHECK_NEAR(p.y, DIAL_CY, EPS);

    p = dial_point_at(0.0f, 137.0f);
    CHECK_NEAR(p.x, DIAL_CX, EPS);
    CHECK_NEAR(p.y, DIAL_CY, EPS);
}

MT_TEST(test_dial_point_at_i_rounds_to_pixels)
{
    dial_point_t p = dial_point_at_i(DIAL_SAFE_R_PX, DIAL_DEG_TOP);
    CHECK_EQ(p.x, 180);
    CHECK_EQ(p.y, 8);                       /* 180 - 172 */

    p = dial_point_at_i(DIAL_SAFE_R_PX, DIAL_DEG_BOTTOM);
    CHECK_EQ(p.y, 352);

    /* 45° 处半径 172 → 偏移 121.6 → 四舍五入 */
    p = dial_point_at_i(172.0f, 45.0f);
    CHECK_EQ(p.x, 302);
    CHECK_EQ(p.y, 58);
}

MT_TEST(test_dial_deg_at_matches_point_at)
{
    const int probes[] = { 0, 15, 45, 90, 135, 180, 225, 270, 315, 359 };
    for (unsigned i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        const float deg = (float)probes[i];
        dial_point_t p = dial_point_at_i(150.0f, deg);
        CHECK_NEAR(dial_deg_at(p.x, p.y), deg, 1.0f);
    }
    /* 圆心没有方向，约定为 12 点 */
    CHECK_NEAR(dial_deg_at(180, 180), DIAL_DEG_TOP, EPS);
}

MT_TEST(test_dial_radius_and_inside_safe)
{
    CHECK_NEAR(dial_radius_at(180, 180), 0.0f, EPS);
    CHECK_NEAR(dial_radius_at(280, 180), 100.0f, EPS);
    CHECK_NEAR(dial_radius_at(180, 8), 172.0f, EPS);

    CHECK_TRUE(dial_inside_safe(180, 180, 0));
    CHECK_TRUE(dial_inside_safe(180, 8, 0));            /* 正好在安全圆上 */
    CHECK_FALSE(dial_inside_safe(180, 7, 0));           /* 出界 1px */
    CHECK_TRUE(dial_inside_safe(180, 18, 10));          /* 边距 10 → 限 162 */
    CHECK_FALSE(dial_inside_safe(180, 17, 10));
    CHECK_FALSE(dial_inside_safe(180, 180, 200));       /* 边距吃掉整个圆 */
}

/* ── 弧长 / 分格 ─────────────────────────────────────────────────────────── */

MT_TEST(test_dial_arc_len)
{
    CHECK_NEAR(dial_arc_len(100.0f, 360.0f), 628.3f, 0.5f);
    CHECK_NEAR(dial_arc_len(100.0f, 90.0f), 157.08f, 0.5f);
    CHECK_NEAR(dial_arc_len(100.0f, -90.0f), 157.08f, 0.5f);   /* 与方向无关 */
    CHECK_NEAR(dial_arc_len(0.0f, 90.0f), 0.0f, EPS);
    CHECK_NEAR(dial_arc_len(-5.0f, 90.0f), 0.0f, EPS);
}

MT_TEST(test_dial_slot_width_px)
{
    /* 12 格、间隙 2°、r=150 → 每格 30°-2° = 28° 弧长 */
    CHECK_NEAR(dial_slot_width_px(150.0f, 12, 2.0f), 73.3f, 0.5f);
    CHECK_NEAR(dial_slot_width_px(150.0f, 0, 2.0f), 0.0f, EPS);      /* 非法 count */
    CHECK_NEAR(dial_slot_width_px(0.0f, 12, 2.0f), 0.0f, EPS);       /* 非法半径 */
    CHECK_NEAR(dial_slot_width_px(150.0f, 12, 40.0f), 0.0f, EPS);    /* 间隙吃掉整格 */
    CHECK_NEAR(dial_slot_width_px(150.0f, 12, -5.0f),                /* 负间隙按 0 */
               dial_slot_width_px(150.0f, 12, 0.0f), EPS);
}

MT_TEST(test_dial_sector_count)
{
    /* r=150 上放至少 12px 净宽、2° 间隙：360/(2+12/2.618) = 54 格 */
    CHECK_EQ(dial_sector_count(150.0f, 12.0f, 2.0f), 54);
    /* 半径越大越能塞 */
    CHECK(dial_sector_count(DIAL_SAFE_R_PX, 12.0f, 2.0f) >
          dial_sector_count(80.0f, 12.0f, 2.0f));
    /* 间隙主导（每格 45° 间隙 + 微小净宽）→ 只塞得下 7 格 */
    CHECK_EQ(dial_sector_count(150.0f, 1.0f, 45.0f), 7);
    /* 非法输入 */
    CHECK_EQ(dial_sector_count(0.0f, 12.0f, 2.0f), 0);
    CHECK_EQ(dial_sector_count(150.0f, 0.0f, 2.0f), 0);
    CHECK_EQ(dial_sector_count(150.0f, -3.0f, 2.0f), 0);
    /* 负间隙按 0 处理，结果与 gap=0 一致 */
    CHECK_EQ(dial_sector_count(150.0f, 12.0f, -2.0f), dial_sector_count(150.0f, 12.0f, 0.0f));
}

MT_TEST(test_dial_tick_count)
{
    /* 周长 628.3px，每 10px 一个点 → 62 个 */
    CHECK_EQ(dial_tick_count(100.0f, 10.0f), 62);
    CHECK_EQ(dial_tick_count(171.0f, 7.26f), 147);
    CHECK_EQ(dial_tick_count(0.0f, 10.0f), 0);
    CHECK_EQ(dial_tick_count(100.0f, 0.0f), 0);
    CHECK_EQ(dial_tick_count(100.0f, -1.0f), 0);
}

/* ── 命中测试 ────────────────────────────────────────────────────────────── */

MT_TEST(test_dial_hit_sector_finds_the_slot)
{
    const float r_out = DIAL_RING_R_IN;
    const float r_in = DIAL_FOCUS_R_OUT;

    dial_point_t top = dial_point_at_i(120.0f, 0.0f);
    CHECK_EQ(dial_hit_sector(top.x, top.y, r_out, r_in, 12, 2.0f), 0);

    dial_point_t right = dial_point_at_i(120.0f, 90.0f);
    CHECK_EQ(dial_hit_sector(right.x, right.y, r_out, r_in, 12, 2.0f), 3);

    dial_point_t bottom = dial_point_at_i(120.0f, 180.0f);
    CHECK_EQ(dial_hit_sector(bottom.x, bottom.y, r_out, r_in, 12, 2.0f), 6);

    dial_point_t left = dial_point_at_i(120.0f, 270.0f);
    CHECK_EQ(dial_hit_sector(left.x, left.y, r_out, r_in, 12, 2.0f), 9);

    /* 跨 0° 的那一格：355° 属于第 0 格，不是第 11 格 */
    dial_point_t near_top = dial_point_at_i(120.0f, 355.0f);
    CHECK_EQ(dial_hit_sector(near_top.x, near_top.y, r_out, r_in, 12, 2.0f), 0);
}

MT_TEST(test_dial_hit_sector_rejects_gaps_and_radii)
{
    const float r_out = DIAL_RING_R_IN;
    const float r_in = DIAL_FOCUS_R_OUT;

    /* 15° 正好在 12 格（每格 30°、间隙 2°）的间隙里 */
    dial_point_t in_gap = dial_point_at_i(120.0f, 15.0f);
    CHECK_EQ(dial_hit_sector(in_gap.x, in_gap.y, r_out, r_in, 12, 2.0f), DIAL_SECTOR_MISS);

    /* 环带之外 / 之内 */
    dial_point_t too_far = dial_point_at_i(170.0f, 0.0f);
    CHECK_EQ(dial_hit_sector(too_far.x, too_far.y, r_out, r_in, 12, 2.0f), DIAL_SECTOR_MISS);
    dial_point_t too_near = dial_point_at_i(20.0f, 0.0f);
    CHECK_EQ(dial_hit_sector(too_near.x, too_near.y, r_out, r_in, 12, 2.0f), DIAL_SECTOR_MISS);

    /* 内外半径写反也要能用 */
    const dial_point_t p = dial_point_at_i(120.0f, 0.0f);

    /* 非法 count */
    CHECK_EQ(dial_hit_sector(p.x, p.y, r_out, r_in, 0, 2.0f), DIAL_SECTOR_MISS);
    CHECK_EQ(dial_hit_sector(p.x, p.y, r_in, r_out, 12, 2.0f), 0);

    /* 间隙 ≥ 整格宽：只有正对格心的点能命中 */
    CHECK_EQ(dial_hit_sector(p.x, p.y, r_out, r_in, 12, 30.0f), 0);
    dial_point_t off = dial_point_at_i(120.0f, 5.0f);
    CHECK_EQ(dial_hit_sector(off.x, off.y, r_out, r_in, 12, 30.0f), DIAL_SECTOR_MISS);
}

/* ── 排版可用性 ──────────────────────────────────────────────────────────── */

MT_TEST(test_dial_row_span_follows_the_circle)
{
    dial_row_span_t s = dial_row_span(180, 0);
    CHECK_TRUE(s.valid);
    CHECK_EQ(s.x1, 8);
    CHECK_EQ(s.x2, 352);

    /* 越靠近上下边缘越窄，且始终落在屏幕内 */
    dial_row_span_t top = dial_row_span(20, 0);
    CHECK_TRUE(top.valid);
    CHECK(top.x2 - top.x1 < s.x2 - s.x1);
    CHECK(top.x1 >= 0);
    CHECK(top.x2 <= APP_SCREEN_MAX_COORD);

    /* 圆外与退化情况 */
    CHECK_FALSE(dial_row_span(0, 0).valid);
    CHECK_FALSE(dial_row_span(359, 0).valid);
    CHECK_FALSE(dial_row_span(180, 172).valid);
    CHECK_FALSE(dial_row_span(180, 200).valid);
    /* 负边距按 0 处理 */
    CHECK_EQ(dial_row_span(180, -10).x1, dial_row_span(180, 0).x1);
}

MT_TEST(test_dial_inset_side_is_the_largest_square)
{
    const int side = dial_inset_side(0);
    CHECK_EQ(side, 243);
    /* 正方形的角必须仍在安全圆内 */
    const int half = side / 2;
    CHECK_TRUE(dial_inside_safe(180 + half, 180 + half, 0));
    /* 再加一圈就出界：说明这个值确实是"最大"的 */
    CHECK_FALSE(dial_inside_safe(180 + half + 1, 180 + half + 1, 0));

    CHECK(dial_inset_side(20) < side);
    CHECK_EQ(dial_inset_side(172), 0);
    CHECK_EQ(dial_inset_side(200), 0);
    CHECK_EQ(dial_inset_side(-10), side);
}

MT_TEST(test_dial_max_arc_radius_prevents_clipping)
{
    /* 线宽以 radius 为中心铺开，所以 7px 的环最多画到 168.5 */
    CHECK_NEAR(dial_max_arc_radius(7.0f, 0), 168.5f, EPS);
    CHECK_NEAR(dial_max_arc_radius(7.0f, 8), 160.5f, EPS);
    CHECK_NEAR(dial_max_arc_radius(-9.0f, 0), 167.5f, EPS);   /* 线宽取绝对值 */
    CHECK_NEAR(dial_max_arc_radius(400.0f, 0), 0.0f, EPS);    /* 粗到放不下 */

    /* 说明：codex 表盘的 r=180 / width=7 是**刻意贴边**（外沿 183.5 会超出 360px 画布），
     * 属于既有设计，不用本函数；新表盘若要在安全圆内不被裁切，请用它算半径。 */
    CHECK(180.0f > dial_max_arc_radius(7.0f, 0));
}

MT_TEST(test_dial_pct_helpers)
{
    CHECK_NEAR(dial_pct_clamped(50.0f), 50.0f, EPS);
    CHECK_NEAR(dial_pct_clamped(-10.0f), 0.0f, EPS);
    CHECK_NEAR(dial_pct_clamped(110.0f), 100.0f, EPS);
    CHECK_NEAR(dial_pct_clamped(0.0f), 0.0f, EPS);

    CHECK_NEAR(dial_pct_to_deg(50.0f, 270.0f), 135.0f, EPS);
    CHECK_NEAR(dial_pct_to_deg(100.0f, 360.0f), 360.0f, EPS);
    CHECK_NEAR(dial_pct_to_deg(-5.0f, 360.0f), 0.0f, EPS);
    CHECK_NEAR(dial_pct_to_deg(50.0f, -90.0f), 0.0f, EPS);    /* 负扫角按 0 */
}

/* ── 预设自洽 ────────────────────────────────────────────────────────────── */

MT_TEST(test_dial_div_presets_are_usable_at_their_hint_radius)
{
    for (int i = 0; i < DIAL_DIV_COUNT; i++) {
        const dial_div_t *d = &dial_div_presets[i];
        CHECK(d->count > 0);
        CHECK(d->gap_deg > 0.0f);
        CHECK(d->r_min > 0.0f);
        CHECK(d->r_min <= DIAL_SAFE_R_PX);
        /* 在它自己建议的半径上，单格净宽必须达标 */
        CHECK(dial_slot_width_px(d->r_min, d->count, d->gap_deg) >= (float)d->min_slot_px);
        /* 该半径也确实放得下这么多格 */
        CHECK(dial_sector_count(d->r_min, (float)d->min_slot_px, d->gap_deg) >= d->count);
    }
}

MT_TEST(test_dial_div_lookup)
{
    CHECK_EQ(dial_div_count(DIAL_DIV_12), 12);
    CHECK_EQ(dial_div_count(DIAL_DIV_8), 8);
    CHECK_EQ(dial_div_count(DIAL_DIV_24), 24);
    CHECK_EQ(dial_div_count(DIAL_DIV_48), 48);
    CHECK_STR_EQ(dial_div(DIAL_DIV_24)->name, "div24");

    /* 越界不崩，退回第一个预设 */
    CHECK_EQ(dial_div_count((dial_div_preset_t)DIAL_DIV_COUNT), 12);
    CHECK_EQ(dial_div_count((dial_div_preset_t)-3), 12);
}

MT_TEST(test_dial_layout_agrees_with_codex_screen_ring)
{
    /* codex 屏的 tick 环：r=171、148 个 1px 点。用模型反推，间距约 7.26px，
     * 说明"148"这个数本来就是按弧长排的——新屏照抄这套算法即可。 */
    const int ticks = 148;
    const float pitch = dial_arc_len(171.0f, DIAL_DEG_FULL) / (float)ticks;
    CHECK_NEAR(pitch, 7.26f, 0.02f);
    /* 浮点取整会让这里恰好落在 147/148 边界上，断言"不偏离一格"即可 */
    CHECK(dial_tick_count(171.0f, pitch) >= ticks - 1);
    CHECK(dial_tick_count(171.0f, pitch) <= ticks);
    /* 但 tick 环所在的 r=171 属于边缘带：只准画点，不准放文字/按钮 */
    CHECK_EQ(dial_band_at(171.0f), DIAL_BAND_EDGE);
}
