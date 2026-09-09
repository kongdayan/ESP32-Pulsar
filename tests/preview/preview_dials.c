/*
 * preview_dials.c — 用主机侧 LVGL 把几套"表盘切割方案"离线渲染成图片。
 *
 * 目的不是测试正确性（那在 tests/unit/test_dial_layout.c 里），而是让"这块圆屏该怎么切"
 * 变成可以看的东西：所有几何都走 core/dial_layout.h，不手写一个绝对坐标。
 *
 *   make -C tests preview            # 输出 build/preview 下的 BMP
 *   make -C tests preview-PNG        # 顺带转成 PNG（需要 macOS sips 或 ImageMagick）
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv_host.h"
#include "ui_screen.h"
#include "dial_layout.h"
#include "ui_theme.h"

#include <lvgl.h>

#define THEME UI_THEME_DARK
#define PX    APP_SCREEN_PX

typedef void (*paint_fn)(lv_draw_ctx_t *dc);

static uint32_t role(ui_color_role_t r) { return ui_theme_color(THEME, r); }
static lv_color_t col(uint32_t rgb) { return lv_color_hex(rgb); }

/* ── 绘制小工具（全部参数来自 dial_layout） ─────────────────────────────── */

static void draw_dot(lv_draw_ctx_t *dc, int x, int y, int r, uint32_t rgb, lv_opa_t opa)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = col(rgb);
    d.bg_opa = opa;
    d.radius = LV_RADIUS_CIRCLE;
    const lv_area_t area = { (lv_coord_t)(x - r), (lv_coord_t)(y - r),
                             (lv_coord_t)(x + r), (lv_coord_t)(y + r) };
    lv_draw_rect(dc, &d, &area);
}

static void draw_text(lv_draw_ctx_t *dc, const char *text, const lv_font_t *font,
                      int cx, int cy, uint32_t rgb)
{
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.color = col(rgb);
    d.font = font;
    d.align = LV_TEXT_ALIGN_CENTER;
    const int w = (int)(lv_font_get_glyph_width(font, '0', '0') * (float)strlen(text)) + 40;
    const int h = font->line_height + 4;
    const lv_area_t area = { (lv_coord_t)(cx - w / 2), (lv_coord_t)(cy - h / 2),
                             (lv_coord_t)(cx + w / 2), (lv_coord_t)(cy + h / 2) };
    lv_draw_label(dc, &d, &area, text, NULL);
}

/* 一条 arc：dial 角（12 点=0，顺时针）+ 扫角；半径由调用方给，便于画同心环 */
static void draw_dial_arc(lv_draw_ctx_t *dc, float radius, float start_deg, float sweep_deg,
                          float width, uint32_t rgb, bool rounded)
{
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.color = col(rgb);
    d.width = (lv_coord_t)width;
    d.rounded = rounded;
    const lv_point_t center = { APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y };
    const dial_arc_lvgl_t s = dial_arc_span_lvgl(start_deg, sweep_deg);
    lv_draw_arc(dc, &d, &center, (lv_coord_t)lroundf(radius), s.start_lvgl, s.end_lvgl);
}

static void draw_radial_bar(lv_draw_ctx_t *dc, float deg, float r_from, float r_to,
                             int width_px, uint32_t rgb)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = col(rgb);
    d.width = (lv_coord_t)width_px;
    d.round_end = true;
    const dial_point_t a = dial_point_at_i(r_from, deg);
    const dial_point_t b = dial_point_at_i(r_to, deg);
    const lv_point_t p1 = { a.x, a.y };
    const lv_point_t p2 = { b.x, b.y };
    lv_draw_line(dc, &d, &p1, &p2);
}

/* 边缘带刻度：个数由弧长算出来，不是手写的魔数 */
static void draw_edge_ticks(lv_draw_ctx_t *dc, uint8_t dot_r, float pitch_px)
{
    const float r = (DIAL_SAFE_R_PX + DIAL_EDGE_R_IN) * 0.5f;
    const int n = dial_tick_count(r, pitch_px);
    for (int i = 0; i < n; i++) {
        const float deg = dial_sector_center_deg(n, i);
        const dial_point_t p = dial_point_at_i(r, deg);
        const bool cardinal = (n >= 4) && ((i % (n / 4)) == 0);
        draw_dot(dc, p.x, p.y, cardinal ? dot_r + 1 : dot_r,
                 cardinal ? role(UI_ROLE_TICK_HOT) : role(UI_ROLE_TICK), LV_OPA_70);
    }
}

/* ── 方案 A：一圈进度 + 焦点大数字 ───────────────────────────────────────── */

#define A_PCT 27.0f

static void paint_a(lv_draw_ctx_t *dc)
{
    draw_edge_ticks(dc, 1, 9.0f);
    const float r = dial_max_arc_radius(10.0f, 0);
    draw_dial_arc(dc, r, DIAL_DEG_TOP, DIAL_DEG_FULL, 10.0f, role(UI_ROLE_BLUE_DIM), true);
    draw_dial_arc(dc, r, DIAL_DEG_TOP, dial_pct_to_deg(A_PCT, DIAL_DEG_FULL), 10.0f,
                  role(UI_ROLE_BLUE), true);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (int)A_PCT);
    draw_text(dc, buf, &lv_font_montserrat_22, APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y - 6,
              role(UI_ROLE_TEXT_PRIMARY));
    draw_text(dc, "CODEX", &lv_font_montserrat_10, APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y + 24,
              role(UI_ROLE_TEXT_ACCENT));
}

/* ── 方案 B：三层同心环（把 codex 的两行列表变成三圈弧）─────────────────── */

static const struct {
    float pct;
    float width;
    ui_color_role_t role;
} B_RINGS[] = {
    { 27.0f, 9.0f, UI_ROLE_BLUE },
    { 73.0f, 9.0f, UI_ROLE_GREEN },
    { 58.0f, 9.0f, UI_ROLE_TEXT_ACCENT },
};

static void paint_b(lv_draw_ctx_t *dc)
{
    draw_edge_ticks(dc, 1, 9.0f);
    /* 三圈同心：从外向内，每圈半径递减一个线宽 + 间隙 */
    float r = dial_max_arc_radius(B_RINGS[0].width, 0);
    for (unsigned i = 0; i < sizeof(B_RINGS) / sizeof(B_RINGS[0]); i++) {
        draw_dial_arc(dc, r, DIAL_DEG_TOP, DIAL_DEG_FULL, B_RINGS[i].width,
                      role(UI_ROLE_BLUE_DIM), false);
        draw_dial_arc(dc, r, DIAL_DEG_TOP, dial_pct_to_deg(B_RINGS[i].pct, DIAL_DEG_FULL),
                      B_RINGS[i].width, role(B_RINGS[i].role), true);
        r -= B_RINGS[i].width + 4.0f;
    }
    draw_text(dc, "3", &lv_font_montserrat_22, APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y - 4,
              role(UI_ROLE_TEXT_PRIMARY));
    draw_text(dc, "LAYER", &lv_font_montserrat_10, APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y + 22,
              role(UI_ROLE_TICK));
}

/* ── 方案 C：24 小时环（角向切格 + 中心时间）────────────────────────────── */

#define C_NOW_HOUR 14
#define C_CURRENT_DEG (DIAL_DEG_FULL * (float)C_NOW_HOUR / 24.0f)

static void paint_c(lv_draw_ctx_t *dc)
{
    draw_edge_ticks(dc, 1, 11.0f);
    const dial_div_t *d = dial_div(DIAL_DIV_24);
    const float r_from = DIAL_RING_R_IN;
    const int n = d->count;
    const float step = dial_deg_step(n);
    /* 每格长度 = 该小时的"活跃度"，这里用固定序列做示意 */
    static const int activity[24] = { 2, 1, 1, 0, 0, 1, 3, 5, 8, 6, 4, 3,
                                      4, 6, 9, 7, 5, 4, 6, 8, 5, 3, 2, 1 };
    for (int i = 0; i < n; i++) {
        const float deg = dial_sector_center_deg(n, i);
        const float len = (activity[i] / 9.0f) * (DIAL_EDGE_R_IN - r_from - 4.0f);
        const bool now = ((float)i * step <= C_CURRENT_DEG) &&
                         (C_CURRENT_DEG < ((float)i + 1.0f) * step);
        const int wpx = (int)dial_slot_width_px(r_from, n, d->gap_deg) / 2;
        draw_radial_bar(dc, deg, r_from, r_from + 6.0f + len, wpx > 2 ? wpx : 2,
                        now ? role(UI_ROLE_TICK_HOT) : role(UI_ROLE_BLUE));
    }
    draw_text(dc, "14:32", &lv_font_montserrat_22, APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y - 6,
              role(UI_ROLE_TEXT_PRIMARY));
    draw_text(dc, "24H", &lv_font_montserrat_10, APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y + 22,
              role(UI_ROLE_TICK));
}

/* ── 方案 D：环 × 扇 —— 外环 8 格（7 天 + 休），内环 24 格打卡点 ────────── */

#define D_DONE_HOURS 18

static void paint_d(lv_draw_ctx_t *dc)
{
    draw_edge_ticks(dc, 1, 9.0f);
    const dial_div_t *week = dial_div(DIAL_DIV_8);
    const float step = dial_deg_step(week->count);
    const float half = (step - week->gap_deg) * 0.5f;
    const float seg_r = dial_max_arc_radius(14.0f, 0);
    static const int day_pct[8] = { 100, 80, 46, 0, 0, 0, 0, 0 };
    for (int i = 0; i < week->count; i++) {
        const float center = dial_sector_center_deg(week->count, i);
        /* 底槽 + 已完成部分，都从各自的格心起算 */
        draw_dial_arc(dc, seg_r, center - half, half * 2.0f, 14.0f, role(UI_ROLE_BLUE_DIM), false);
        draw_dial_arc(dc, seg_r, center - half, dial_pct_to_deg((float)day_pct[i], half * 2.0f),
                      14.0f, role(UI_ROLE_BLUE), true);
    }

    /* 内环：24 小时打卡点，点亮前 N 个 */
    const dial_div_t *hours = dial_div(DIAL_DIV_24);
    const float r = DIAL_FOCUS_R_OUT + 16.0f;
    for (int i = 0; i < hours->count; i++) {
        const float deg = dial_sector_center_deg(hours->count, i);
        const dial_point_t p = dial_point_at_i(r, deg);
        const bool done = i < D_DONE_HOURS;
        draw_dot(dc, p.x, p.y, done ? 3 : 2,
                 done ? role(UI_ROLE_GREEN) : role(UI_ROLE_BLUE_DIM),
                 done ? LV_OPA_COVER : LV_OPA_50);
    }
    draw_text(dc, "18", &lv_font_montserrat_22, APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y - 8,
              role(UI_ROLE_TEXT_PRIMARY));
    draw_text(dc, "/ 24", &lv_font_montserrat_10, APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y + 16,
              role(UI_ROLE_TICK));
}

/* ── 渲染与落盘 ──────────────────────────────────────────────────────────── */

static paint_fn g_paint;
static lv_obj_t *g_transit;   /* 中转屏，保证删除对象时它已不是 act_scr */

static void on_draw_post(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_POST_BEGIN) return;
    g_paint(lv_event_get_draw_ctx(e));
}

static void write_bmp(const char *path, const lv_color_t *fb)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        printf("  ! 无法写入 %s\n", path);
        return;
    }
    const uint32_t row_bytes = (uint32_t)PX * 3u;         /* 360*3 已是 4 的倍数，无需填充 */
    const uint32_t img = row_bytes * (uint32_t)PX;
    uint8_t hdr[54];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    const uint32_t file_size = 54u + img;
    memcpy(&hdr[2], &file_size, 4);
    const uint32_t data_off = 54u;
    memcpy(&hdr[10], &data_off, 4);
    const uint32_t dib = 40u;
    memcpy(&hdr[14], &dib, 4);
    const int32_t w = PX, h = PX;
    memcpy(&hdr[18], &w, 4);
    memcpy(&hdr[22], &h, 4);
    hdr[26] = 1; hdr[28] = 24;
    memcpy(&hdr[34], &img, 4);
    fwrite(hdr, 1, sizeof(hdr), fp);
    for (int y = PX - 1; y >= 0; y--) {
        for (int x = 0; x < PX; x++) {
            const uint32_t c = lv_color_to32(fb[y * PX + x]);
            uint8_t bgr[3];
            bgr[0] = (uint8_t)(c & 0xFF);          /* blue  */
            bgr[1] = (uint8_t)((c >> 8) & 0xFF);   /* green */
            bgr[2] = (uint8_t)((c >> 16) & 0xFF);  /* red   */
            fwrite(bgr, 1, 3, fp);
        }
    }
    fclose(fp);
}

static void render(const char *name, paint_fn fn)
{
    g_paint = fn;

    lv_obj_t *scr = lv_obj_create(NULL);
    ui_screen_set_bg(scr, role(UI_ROLE_BG));
    lv_obj_t *panel = ui_fullscreen_layer_create(scr, false);
    lv_obj_add_event_cb(panel, on_draw_post, LV_EVENT_ALL, NULL);
    lv_scr_load(scr);

    lv_host_advance_ms(100u);
    lv_host_run_timers();
    lv_host_run_timers();

    char path[128];
    snprintf(path, sizeof(path), "build/preview/%s.bmp", name);
    write_bmp(path, lv_host_framebuffer());
    printf("  %-14s 可见像素 %6u 种 %3u 色 -> %s\n", name,
           lv_host_non_black_pixels(), lv_host_distinct_colors(), path);

    /* 切到中转屏再删，避免删掉仍是 act_scr 的对象；上一张中转屏此时已非 active */
    lv_obj_t *transit = lv_obj_create(NULL);
    lv_scr_load(transit);
    lv_host_run_timers();
    lv_obj_del(scr);
    if (g_transit != NULL) lv_obj_del(g_transit);
    g_transit = transit;
}

int main(int argc, char **argv)
{
    static const struct { const char *name; paint_fn fn; } k_all[] = {
        { "A_minimal_ring", paint_a },
        { "B_three_rings",  paint_b },
        { "C_hours24",      paint_c },
        { "D_week_hours",   paint_d },
    };

    lv_host_init();
    for (unsigned i = 0; i < sizeof(k_all) / sizeof(k_all[0]); i++) {
        if (argc > 1 && strstr(k_all[i].name, argv[1]) == NULL) continue;
        render(k_all[i].name, k_all[i].fn);
    }
    return 0;
}
