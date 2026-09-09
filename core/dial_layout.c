/*
 * dial_layout.c — 圆屏极坐标切割模型。
 *
 * 全部是纯函数：不碰 LVGL、不碰硬件，因此可以在主机上逐条断言。
 * 角度约定见 dial_layout.h：12 点钟 = 0°，顺时针为正。
 */
#include "dial_layout.h"

#include <math.h>

#define DIAL_PI          3.14159265358979f
#define DIAL_SQRT2       1.41421356237310f
#define DIAL_RAD_PER_DEG (DIAL_PI / 180.0f)
#define DIAL_DEG_PER_RAD (180.0f / DIAL_PI)
#define DIAL_HALF_CIRCLE 180.0f
#define DIAL_PCT_FULL    100.0f

/* 环带表：顺序必须与 dial_band_id_t 一致（UT 会逐条核对 id == 下标） */
const dial_band_t dial_bands[DIAL_BAND_COUNT] = {
    { DIAL_BAND_EDGE,    "edge",    DIAL_SAFE_R_PX,  DIAL_EDGE_R_IN    },
    { DIAL_BAND_RING,    "ring",    DIAL_EDGE_R_IN,  DIAL_RING_R_IN    },
    { DIAL_BAND_CONTENT, "content", DIAL_RING_R_IN,  DIAL_FOCUS_R_OUT  },
    { DIAL_BAND_FOCUS,   "focus",   DIAL_FOCUS_R_OUT, 0.0f              },
};

/* 分格预设：count / 格间间隙 / 单格最小净宽 / 建议落点半径 */
const dial_div_t dial_div_presets[DIAL_DIV_COUNT] = {
    { "div12", 12, 2.0f, 12, DIAL_RING_R_IN   },
    { "div8",   8, 3.0f, 16, DIAL_RING_R_IN   },
    { "div24", 24, 1.5f,  8, DIAL_EDGE_R_IN   },
    { "div48", 48, 1.0f,  4, DIAL_EDGE_R_IN   },
};

/* ── 角度 ────────────────────────────────────────────────────────────────── */

float dial_norm_deg(float deg)
{
    float m = fmodf(deg, DIAL_DEG_FULL);
    if (m < 0.0f) m += DIAL_DEG_FULL;
    return m;
}

float dial_delta_deg(float from_deg, float to_deg)
{
    float d = dial_norm_deg(to_deg) - dial_norm_deg(from_deg);
    if (d >= DIAL_HALF_CIRCLE) {
        d -= DIAL_DEG_FULL;
    } else if (d < -DIAL_HALF_CIRCLE) {
        d += DIAL_DEG_FULL;
    }
    return d;
}

float dial_deg_to_lvgl(float deg)
{
    /* LVGL 8：0° 在 3 点钟、顺时针增大（实测 lv_arc_set_rotation(270) 起笔在 12 点） */
    return dial_norm_deg(deg - DIAL_HALF_CIRCLE / 2.0f);
}

float dial_deg_from_lvgl(float lvgl_deg)
{
    return dial_norm_deg(lvgl_deg + DIAL_HALF_CIRCLE / 2.0f);
}

float dial_deg_step(int count)
{
    if (count <= 0) return 0.0f;
    return DIAL_DEG_FULL / (float)count;
}

float dial_sector_center_deg(int count, int index)
{
    float step = dial_deg_step(count);
    if (step <= 0.0f) return 0.0f;
    /* 第 0 格朝 12 点，所以格中心 = index * step（不做半格偏移） */
    return dial_norm_deg((float)index * step);
}

/* ── 极坐标 ↔ 屏幕坐标 ───────────────────────────────────────────────────── */

dial_pointf_t dial_point_at(float r, float deg)
{
    const float rad = deg * DIAL_RAD_PER_DEG;
    dial_pointf_t p;
    p.x = DIAL_CX + r * sinf(rad);
    p.y = DIAL_CY - r * cosf(rad);
    return p;
}

dial_point_t dial_point_at_i(float r, float deg)
{
    dial_pointf_t f = dial_point_at(r, deg);
    dial_point_t p;
    p.x = (int16_t)lroundf(f.x);
    p.y = (int16_t)lroundf(f.y);
    return p;
}

float dial_deg_at(int x, int y)
{
    const float dx = (float)x - DIAL_CX;
    const float dy = (float)y - DIAL_CY;
    if (dx == 0.0f && dy == 0.0f) return DIAL_DEG_TOP;
    return dial_norm_deg(atan2f(dx, -dy) * DIAL_DEG_PER_RAD);
}

float dial_radius_at(int x, int y)
{
    const float dx = (float)x - DIAL_CX;
    const float dy = (float)y - DIAL_CY;
    return sqrtf(dx * dx + dy * dy);
}

bool dial_inside_safe(int x, int y, int margin_px)
{
    const float limit = DIAL_SAFE_R_PX - (float)(margin_px > 0 ? margin_px : 0);
    if (limit <= 0.0f) return false;
    return dial_radius_at(x, y) <= limit;
}

int dial_band_at(float r)
{
    if (r < 0.0f || r > DIAL_SAFE_R_PX) return DIAL_BAND_INVALID;
    if (r > DIAL_EDGE_R_IN)   return DIAL_BAND_EDGE;
    if (r > DIAL_RING_R_IN)   return DIAL_BAND_RING;
    if (r > DIAL_FOCUS_R_OUT) return DIAL_BAND_CONTENT;
    return DIAL_BAND_FOCUS;
}

dial_arc_lvgl_t dial_arc_span_lvgl(float start_deg, float sweep_deg)
{
    dial_arc_lvgl_t span;
    float sweep = sweep_deg;
    if (sweep < 0.0f) sweep = 0.0f;
    if (sweep >= DIAL_DEG_FULL) {
        span.start_lvgl = 0;
        span.end_lvgl = (int16_t)DIAL_DEG_FULL;
        return span;
    }
    const float start = dial_deg_to_lvgl(start_deg);
    span.start_lvgl = (int16_t)lroundf(start);
    span.end_lvgl = (int16_t)lroundf(start + sweep);
    return span;
}

/* ── 弧长 / 分格 / 命中 ──────────────────────────────────────────────────── */

float dial_arc_len(float r, float deg)
{
    if (r <= 0.0f) return 0.0f;
    return r * fabsf(deg) * DIAL_RAD_PER_DEG;   /* 弧长与旋转方向无关 */
}

float dial_slot_width_px(float r, int count, float gap_deg)
{
    const float step = dial_deg_step(count);
    if (step <= 0.0f || r <= 0.0f) return 0.0f;
    float gap = gap_deg;
    if (gap < 0.0f) gap = 0.0f;
    const float span = step - gap;
    if (span <= 0.0f) return 0.0f;              /* 间隙吃掉整格 */
    return r * span * DIAL_RAD_PER_DEG;
}

int dial_sector_count(float r, float min_slot_px, float gap_deg)
{
    if (r <= 0.0f || min_slot_px <= 0.0f) return 0;
    float gap = gap_deg;
    if (gap < 0.0f) gap = 0.0f;

    /* 每格至少占「间隙 + 净宽对应角度」，所以 360/per 天然不会让间隙超过一格 */
    const float slot_deg_needed = min_slot_px / (r * DIAL_RAD_PER_DEG);
    const int n = (int)floorf(DIAL_DEG_FULL / (gap + slot_deg_needed));
    return n > 0 ? n : 0;
}

int dial_tick_count(float r, float dot_pitch_px)
{
    if (r <= 0.0f || dot_pitch_px <= 0.0f) return 0;
    return (int)floorf(dial_arc_len(r, DIAL_DEG_FULL) / dot_pitch_px);
}

int dial_hit_sector(int x, int y, float r_out, float r_in, int count, float gap_deg)
{
    float hi = r_out;
    float lo = r_in;
    if (hi < lo) {                              /* 参数写反也不崩 */
        const float t = hi;
        hi = lo;
        lo = t;
    }
    const float r = dial_radius_at(x, y);
    if (r > hi || r < lo) return DIAL_SECTOR_MISS;

    const float step = dial_deg_step(count);
    if (step <= 0.0f) return DIAL_SECTOR_MISS;

    float gap = gap_deg;
    if (gap < 0.0f) gap = 0.0f;
    const float half = (step > gap) ? (step - gap) * 0.5f : 0.0f;

    const float deg = dial_deg_at(x, y);
    int idx = (int)floorf(deg / step + 0.5f) % count;   /* 取最近的一格（可跨 0°） */

    const float center = dial_sector_center_deg(count, idx);
    if (fabsf(dial_delta_deg(center, deg)) > half) return DIAL_SECTOR_MISS;   /* 落在格间间隙 */
    return idx;
}

/* ── 排版可用性 ──────────────────────────────────────────────────────────── */

dial_row_span_t dial_row_span(int y, int margin_px)
{
    dial_row_span_t span = { 0, 0, false };
    const float r = DIAL_SAFE_R_PX - (float)(margin_px > 0 ? margin_px : 0);
    const float dy = (float)y - DIAL_CY;
    if (r <= 0.0f || fabsf(dy) >= r) return span;

    const float hw = sqrtf(r * r - dy * dy);
    int x1 = (int)ceilf(DIAL_CX - hw);
    int x2 = (int)floorf(DIAL_CX + hw);
    if (x1 < 0) x1 = 0;
    if (x2 > APP_SCREEN_MAX_COORD) x2 = APP_SCREEN_MAX_COORD;

    span.x1 = (int16_t)x1;
    span.x2 = (int16_t)x2;
    span.valid = x2 > x1;
    return span;
}

int dial_inset_side(int margin_px)
{
    const float r = DIAL_SAFE_R_PX - (float)(margin_px > 0 ? margin_px : 0);
    if (r <= 0.0f) return 0;
    return (int)floorf(2.0f * r / DIAL_SQRT2);
}

float dial_max_arc_radius(float line_width, int margin_px)
{
    const float r = DIAL_SAFE_R_PX - (float)(margin_px > 0 ? margin_px : 0)
                    - fabsf(line_width) * 0.5f;
    return r > 0.0f ? r : 0.0f;
}

float dial_pct_clamped(float pct)
{
    if (pct < 0.0f) return 0.0f;
    if (pct > DIAL_PCT_FULL) return DIAL_PCT_FULL;
    return pct;
}

float dial_pct_to_deg(float pct, float sweep_deg)
{
    float sweep = sweep_deg;
    if (sweep < 0.0f) sweep = 0.0f;
    return dial_pct_clamped(pct) * sweep * 0.01f;
}

/* ── 预设查表 ────────────────────────────────────────────────────────────── */

const dial_div_t *dial_div(dial_div_preset_t preset)
{
    const int i = (int)preset;
    if (i < 0 || i >= DIAL_DIV_COUNT) return &dial_div_presets[0];
    return &dial_div_presets[i];
}

int dial_div_count(dial_div_preset_t preset)
{
    return dial_div(preset)->count;
}
