/*
 * usage_face.c — 见 usage_face.h。原 screen_codex_usage.c 的绘制逻辑搬到这里，
 * 唯一变化是把「读哪个 provider 的槽 / 显示什么标题」参数化。
 */
#include "usage_face.h"

#include <math.h>
#include <stdio.h>

#include "app_config.h"
#include "ui_math.h"
#include "usage_face_layout.h"
#include "watchface.h"

/* 徽标内部小点（相对 MARK 中心的绝对坐标） */
static const int16_t k_wf_mark_dots[WF_MARK_DOT_COUNT][2] = {
    {  94, 57 }, {  98, 61 }, { 102, 65 }, {  98, 69 },
    {  94, 73 }, { 110, 73 }, { 114, 73 }, { 118, 73 },
};

static uint32_t role_color(const usage_face_t *face, ui_color_role_t role)
{
    return ui_theme_color(face->theme, role);
}

static void on_gesture(lv_event_t *e)
{
    usage_face_t *face = (usage_face_t *)lv_event_get_user_data(e);
    if (face == NULL) return;

    const lv_dir_t raw = ui_nav_raw_gesture(e);
    if (raw == LV_DIR_NONE) return;

    /* 上下滑动：切换深浅色主题 */
    if (raw == LV_DIR_TOP || raw == LV_DIR_BOTTOM) {
        face->theme = ui_theme_toggle(face->theme);
        if (face->panel != NULL) lv_obj_invalidate(face->panel);
        return;
    }

    (void)ui_nav_go(face->screen_id, ui_nav_dir_from_lv(raw));
}

/* ── 基础绘制 ─────────────────────────────────────────────────────────────── */

static void draw_dot(lv_draw_ctx_t *dc, int x, int y, int r, uint32_t color, lv_opa_t opa)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.bg_color = lv_color_hex(color);
    dsc.bg_opa = opa;
    const lv_area_t area = { x - r, y - r, x + r, y + r };
    lv_draw_rect(dc, &dsc, &area);
}

static void draw_line(lv_draw_ctx_t *dc, int x1, int y1, int x2, int y2, int w,
                      uint32_t color, lv_opa_t opa)
{
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = lv_color_hex(color);
    dsc.opa = opa;
    dsc.width = w;
    dsc.round_start = 1;
    dsc.round_end = 1;
    const lv_point_t p1 = { x1, y1 };
    const lv_point_t p2 = { x2, y2 };
    lv_draw_line(dc, &dsc, &p1, &p2);
}

static void draw_rect(lv_draw_ctx_t *dc, int x1, int y1, int x2, int y2,
                      int radius, uint32_t color, lv_opa_t opa)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = radius;
    dsc.bg_color = lv_color_hex(color);
    dsc.bg_opa = opa;
    const lv_area_t area = { x1, y1, x2, y2 };
    lv_draw_rect(dc, &dsc, &area);
}

/* ── 点阵文本 ─────────────────────────────────────────────────────────────── */

static void draw_dot_text(lv_draw_ctx_t *dc, const char *text, int x, int y,
                          int step, int r, uint32_t color, lv_opa_t opa)
{
    for (const char *p = text; *p != '\0'; p++) {
        const uint8_t *glyph = wf_glyph_for(*p);

        for (int row = 0; row < WF_GLYPH_ROWS; row++) {
            for (int col = 0; col < WF_GLYPH_COLS; col++) {
                if (wf_glyph_pixel(glyph, row, col)) {
                    draw_dot(dc, x + col * step, y + row * step, r, color, opa);
                }
            }
        }
        x += wf_glyph_advance(step);
    }
}

static void draw_right_percent(lv_draw_ctx_t *dc, int pct, int right_x, int y,
                               uint32_t color, lv_opa_t opa)
{
    char digits[WF_DIGIT_BUFFER];
    snprintf(digits, sizeof(digits), WF_DIGIT_TEXT_FMT, pct);

    const int digits_w = wf_dot_text_width(digits, WF_PERCENT_DIGIT_STEP);
    const int pct_w = wf_dot_text_width(WF_TEXT_PERCENT, WF_PERCENT_SIGN_STEP);
    const int x = right_x - digits_w - WF_PERCENT_GAP - pct_w;

    draw_dot_text(dc, digits, x, y, WF_PERCENT_DIGIT_STEP, WF_PERCENT_DOT_R, color, opa);
    draw_dot_text(dc, WF_TEXT_PERCENT, x + digits_w + WF_PERCENT_GAP, y + WF_PERCENT_SIGN_Y_OFF,
                  WF_PERCENT_SIGN_STEP, WF_PERCENT_DOT_R, color, opa);
}

static void draw_dotted_hline(lv_draw_ctx_t *dc, int x1, int x2, int y,
                              uint32_t color, lv_opa_t opa)
{
    for (int x = x1; x <= x2; x += WF_DIVIDER_STEP) {
        draw_dot(dc, x, y, WF_DIVIDER_DOT_R, color, opa);
    }
}

/* ── 表盘装饰 ─────────────────────────────────────────────────────────────── */

static lv_opa_t tick_opa(const usage_face_t *face)
{
    return ui_theme_is_light(face->theme) ? WF_TICK_OPA_LIGHT : WF_TICK_OPA_DARK;
}

static lv_opa_t divider_opa(const usage_face_t *face)
{
    return ui_theme_is_light(face->theme) ? WF_DIVIDER_OPA_LIGHT : WF_DIVIDER_OPA_DARK;
}

static void draw_outer_ticks(lv_draw_ctx_t *dc, const usage_face_t *face)
{
    const uint32_t tick = role_color(face, UI_ROLE_TICK);
    const uint32_t hot = role_color(face, UI_ROLE_TICK_HOT);

    for (int i = 0; i < WF_TICK_COUNT; i++) {
        const float rad = ui_deg_to_rad((float)i * UI_DEG_FULL / (float)WF_TICK_COUNT);
        const int x = APP_SCREEN_CENTER_X + (int)(WF_TICK_RADIUS * cosf(rad));
        const int y = APP_SCREEN_CENTER_Y + (int)(WF_TICK_RADIUS * sinf(rad));
        draw_dot(dc, x, y, WF_TICK_DOT_R, tick, tick_opa(face));
    }

    draw_line(dc, WF_HOT_CENTER - WF_HOT_LINE_HALF, WF_HOT_TOP_Y,
              WF_HOT_CENTER + WF_HOT_LINE_HALF, WF_HOT_TOP_Y,
              WF_HOT_LINE_WIDTH, hot, LV_OPA_COVER);
    draw_line(dc, WF_HOT_CENTER - WF_HOT_LINE_HALF, WF_HOT_BOTTOM_Y,
              WF_HOT_CENTER + WF_HOT_LINE_HALF, WF_HOT_BOTTOM_Y,
              WF_HOT_LINE_WIDTH, hot, LV_OPA_COVER);
    draw_line(dc, WF_HOT_EDGE_NEAR, WF_HOT_SIDE_Y,
              WF_HOT_EDGE_NEAR + WF_HOT_LINE_LEN, WF_HOT_SIDE_Y,
              WF_HOT_LINE_WIDTH, hot, LV_OPA_COVER);
    draw_line(dc, WF_HOT_EDGE_FAR, WF_HOT_SIDE_Y,
              WF_HOT_EDGE_FAR + WF_HOT_LINE_LEN, WF_HOT_SIDE_Y,
              WF_HOT_LINE_WIDTH, hot, LV_OPA_COVER);
}

/* 通用 provider 徽标：六边形底 + 内部小点（沿用旧版 Codex 标志的几何） */
static void draw_provider_mark(lv_draw_ctx_t *dc, const usage_face_t *face)
{
    lv_point_t hex[WF_MARK_SIDES];
    for (int i = 0; i < WF_MARK_SIDES; i++) {
        const float rad = ui_deg_to_rad(WF_MARK_START_DEG + (float)i * WF_MARK_STEP_DEG);
        hex[i].x = WF_MARK_CX + (lv_coord_t)(WF_MARK_RADIUS * cosf(rad));
        hex[i].y = WF_MARK_CY + (lv_coord_t)(WF_MARK_RADIUS * sinf(rad));
    }

    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_color = lv_color_hex(role_color(face, UI_ROLE_MARK));
    fill.bg_opa = LV_OPA_COVER;
    lv_draw_polygon(dc, &fill, hex, WF_MARK_SIDES);

    for (int i = 0; i < WF_MARK_DOT_COUNT; i++) {
        draw_dot(dc, k_wf_mark_dots[i][0], k_wf_mark_dots[i][1], WF_MARK_DOT_R,
                 role_color(face, UI_ROLE_ON_MARK), LV_OPA_COVER);
    }
}

static void draw_clock_icon(lv_draw_ctx_t *dc, const usage_face_t *face, int x, int y)
{
    const uint32_t color = role_color(face, UI_ROLE_RESET);

    lv_point_t center = { x, y };
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.color = lv_color_hex(color);
    dsc.opa = LV_OPA_COVER;
    dsc.width = WF_RESET_CLOCK_W;
    dsc.rounded = 1;
    lv_draw_arc(dc, &dsc, &center, WF_RESET_CLOCK_R, WF_RING_START_DEG, WF_RING_END_DEG);

    draw_line(dc, x, y, x, y - WF_RESET_HOUR_LEN, WF_RESET_CLOCK_W, color, LV_OPA_COVER);
    draw_line(dc, x, y, x + WF_RESET_MIN_DX, y + WF_RESET_MIN_DY, WF_RESET_CLOCK_W, color,
              LV_OPA_COVER);
}

static void draw_centered_reset(lv_draw_ctx_t *dc, const usage_face_t *face,
                                const char *text, int y)
{
    draw_clock_icon(dc, face, WF_RESET_CLOCK_X, y + WF_RESET_CLOCK_Y_OFF);

    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = lv_color_hex(role_color(face, UI_ROLE_RESET));
    dsc.opa = LV_OPA_COVER;
    dsc.font = &lv_font_montserrat_16;
    dsc.align = LV_TEXT_ALIGN_LEFT;
    const lv_area_t area = { WF_RESET_TEXT_X1, y, WF_RESET_TEXT_X2, y + WF_RESET_TEXT_H };
    lv_draw_label(dc, &dsc, &area, text, NULL);
}

static void draw_progress_dots(lv_draw_ctx_t *dc, int x, int y, int count, int active, int rows,
                               uint32_t active_color, uint32_t inactive_color)
{
    for (int row = 0; row < rows; row++) {
        for (int i = 0; i < count; i++) {
            const bool on = (i < active);
            draw_dot(dc, x + i * WF_PROGRESS_SPACING, y + row * WF_PROGRESS_SPACING,
                     WF_PROGRESS_DOT_R, on ? active_color : inactive_color,
                     on ? LV_OPA_COVER : WF_PROGRESS_IDLE_OPA);
        }
    }
}

/* ── 整屏绘制 ─────────────────────────────────────────────────────────────── */

static void on_draw(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_POST_BEGIN) return;

    usage_face_t *face = (usage_face_t *)lv_event_get_user_data(e);
    if (face == NULL) return;
    lv_draw_ctx_t *dc = lv_event_get_draw_ctx(e);

    /* 真实数据来自 BLE 写入的共享存储；没有/过期时回退到占位文案 */
    const uint32_t now_ms = (uint32_t)lv_tick_get();
    usage_data_t usage;
    const bool live = usage_store_get(face->provider, &usage) && usage.valid &&
                      !usage_is_stale(&usage, now_ms, APP_BLE_STALE_MS);
    const int current_pct = live ? usage.current_used_pct : 0;
    const int weekly_pct  = live ? usage.weekly_used_pct : 0;

    char current_reset[USAGE_RESET_TEXT_MAX];
    char weekly_reset[USAGE_RESET_TEXT_MAX];
    if (live) {
        usage_format_countdown(usage_countdown_s(&usage, now_ms),
                               current_reset, sizeof(current_reset));
        usage_format_weekly(usage.weekly_reset_label, weekly_reset, sizeof(weekly_reset));
    } else {
        snprintf(current_reset, sizeof(current_reset), "%s", APP_TEXT_WAITING_BLE);
        snprintf(weekly_reset, sizeof(weekly_reset), "%s", APP_TEXT_WAITING_BLE);
    }

    draw_rect(dc, 0, 0, APP_SCREEN_MAX_COORD, APP_SCREEN_MAX_COORD, LV_RADIUS_CIRCLE,
              role_color(face, UI_ROLE_BG), LV_OPA_COVER);

    lv_point_t center = { APP_SCREEN_CENTER_X, APP_SCREEN_CENTER_Y };
    lv_draw_arc_dsc_t ring;
    lv_draw_arc_dsc_init(&ring);
    ring.color = lv_color_hex(role_color(face, UI_ROLE_RING));
    ring.opa = LV_OPA_COVER;
    ring.width = WF_RING_WIDTH;
    ring.rounded = 1;
    lv_draw_arc(dc, &ring, &center, WF_RING_RADIUS, WF_RING_START_DEG, WF_RING_END_DEG);

    draw_outer_ticks(dc, face);
    draw_dotted_hline(dc, WF_DIVIDER_1_X1, WF_DIVIDER_1_X2, WF_DIVIDER_1_Y,
                      role_color(face, UI_ROLE_TICK), divider_opa(face));
    draw_dotted_hline(dc, WF_DIVIDER_2_X1, WF_DIVIDER_2_X2, WF_DIVIDER_2_Y,
                      role_color(face, UI_ROLE_TICK), divider_opa(face));
    draw_dotted_hline(dc, WF_DIVIDER_3_X1, WF_DIVIDER_3_X2, WF_DIVIDER_3_Y,
                      role_color(face, UI_ROLE_TICK), divider_opa(face));

    draw_provider_mark(dc, face);
    draw_dot_text(dc, usage_provider_title(face->provider), WF_MARK_TEXT_X, WF_MARK_TEXT_Y,
                  WF_MARK_TEXT_STEP, WF_MARK_TEXT_DOT_R,
                  role_color(face, UI_ROLE_TEXT_PRIMARY), LV_OPA_COVER);

    draw_dot_text(dc, WF_TEXT_CURRENT, WF_SECTION_LABEL_X, WF_CURRENT_LABEL_Y,
                  WF_SECTION_LABEL_STEP, WF_SECTION_DOT_R, role_color(face, UI_ROLE_BLUE),
                  LV_OPA_COVER);
    draw_right_percent(dc, current_pct, WF_PERCENT_RIGHT_X, WF_CURRENT_PERCENT_Y,
                       role_color(face, UI_ROLE_BLUE), LV_OPA_COVER);
    draw_progress_dots(dc, WF_PROGRESS_X, WF_CURRENT_PROGRESS_Y, WF_PROGRESS_DOTS,
                       wf_progress_steps(WF_PROGRESS_DOTS, current_pct), WF_PROGRESS_ROWS,
                       role_color(face, UI_ROLE_BLUE), role_color(face, UI_ROLE_BLUE_DIM));
    draw_centered_reset(dc, face, current_reset, WF_RESET_DAILY_Y);

    draw_dot_text(dc, WF_TEXT_WEEKLY, WF_SECTION_LABEL_X, WF_WEEKLY_LABEL_Y,
                  WF_SECTION_LABEL_STEP, WF_SECTION_DOT_R, role_color(face, UI_ROLE_GREEN),
                  LV_OPA_COVER);
    draw_right_percent(dc, weekly_pct, WF_PERCENT_RIGHT_X, WF_WEEKLY_PERCENT_Y,
                       role_color(face, UI_ROLE_GREEN), LV_OPA_COVER);
    draw_progress_dots(dc, WF_PROGRESS_X, WF_WEEKLY_PROGRESS_Y, WF_PROGRESS_DOTS,
                       wf_progress_steps(WF_PROGRESS_DOTS, weekly_pct), WF_WEEKLY_PROGRESS_ROWS,
                       role_color(face, UI_ROLE_GREEN), role_color(face, UI_ROLE_GREEN_DIM));
    draw_centered_reset(dc, face, weekly_reset, WF_RESET_WEEKLY_Y);

    draw_dot(dc, WF_AGENT_DOT_X, WF_AGENT_DOT_Y, WF_AGENT_DOT_R,
             role_color(face, UI_ROLE_GREEN), LV_OPA_COVER);
    draw_dot_text(dc, live ? WF_TEXT_LINK_OK : WF_TEXT_OFFLINE, WF_AGENT_TEXT_X, WF_AGENT_TEXT_Y,
                  WF_AGENT_TEXT_STEP, WF_AGENT_TEXT_DOT_R, role_color(face, UI_ROLE_GREEN),
                  LV_OPA_COVER);
}

/* ── 生命周期 ─────────────────────────────────────────────────────────────── */

void usage_face_refresh(usage_face_t *face)
{
    if (face == NULL || face->panel == NULL) return;
    lv_obj_invalidate(face->panel);
}

static void on_refresh(lv_timer_t *timer)
{
    usage_face_refresh((usage_face_t *)timer->user_data);
}

void usage_face_init(usage_face_t *face, usage_provider_t provider, nav_screen_id_t screen_id)
{
    if (face == NULL) return;

    face->provider = provider;
    face->screen_id = screen_id;
    face->theme = UI_THEME_DARK;
    face->scr = NULL;
    face->panel = NULL;
    face->timer = NULL;

    face->binding.cb = on_refresh;
    face->binding.period_ms = APP_BLE_REFRESH_MS;
    face->binding.handle = &face->timer;
    face->binding.user_data = face;

    /* 这屏的手势要自己处理（上下滑 = 切主题），所以不挂通用导航回调，
     * 且回调必须挂在屏幕根对象上 —— LVGL 的 GESTURE 只会发给冒泡根。 */
    face->scr = ui_screen_create_ex(screen_id, false);
    ui_screen_set_bg(face->scr, role_color(face, UI_ROLE_BG));
    lv_obj_add_event_cb(face->scr, on_gesture, LV_EVENT_ALL, face);

    face->panel = ui_fullscreen_layer_create(face->scr, true);
    lv_obj_add_event_cb(face->panel, on_draw, LV_EVENT_ALL, face);

    ui_timer_attach(face->scr, &face->binding);
}

lv_obj_t **usage_face_ptr(usage_face_t *face)
{
    return (face != NULL) ? &face->scr : NULL;
}
