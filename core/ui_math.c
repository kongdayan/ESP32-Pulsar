#include "ui_math.h"

#include <math.h>

float ui_clampf(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

float ui_lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

bool ui_is_zero_f(float value, float eps)
{
    return fabsf(value) < eps;
}

float ui_deg_to_rad(float deg)
{
    return deg * UI_RAD_PER_DEG_F;
}

float ui_rad_to_deg(float rad)
{
    return rad * UI_DEG_PER_RAD_F;
}

float ui_normalize_deg(float deg)
{
    /* 输入范围 [-360, 360)（atan2 派生角度）时足够，与旧版一致地不调用 fmodf */
    if (deg < 0.0f) deg += UI_DEG_FULL;
    if (deg >= UI_DEG_FULL) deg -= UI_DEG_FULL;
    return deg;
}

float ui_wrap_rad(float rad)
{
    /* 输入为两次 atan2 之差（|rad| < 2π）时足够，与旧版逐条 if 的写法等价 */
    if (rad > UI_PI_F) rad -= UI_TWO_PI_F;
    if (rad < -UI_PI_F) rad += UI_TWO_PI_F;
    return rad;
}

float ui_angle_delta_rad(float from, float to)
{
    return ui_wrap_rad(to - from);
}

float ui_vector_len(float x, float y)
{
    return sqrtf(x * x + y * y);
}

float ui_angle_deg_of(float dx, float dy)
{
    return ui_normalize_deg(ui_rad_to_deg(atan2f(dy, dx)));
}

int ui_abs_i(int value)
{
    return value < 0 ? -value : value;
}

int ui_min_i(int a, int b)
{
    return a < b ? a : b;
}

int ui_max_i(int a, int b)
{
    return a > b ? a : b;
}

int ui_clampi(int value, int lo, int hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

int ui_percent_of(int value, int span)
{
    if (span <= 0) return 0;
    return (value * UI_PCT_FULL + span / 2) / span;
}

int ui_steps_for_percent(int total_steps, int percent)
{
    return (total_steps * percent + UI_PCT_ROUNDING) / UI_PCT_FULL;
}

float ui_unit_span(float value, float lo, float hi)
{
    if (hi <= lo) return 0.0f;
    return ui_clampf((value - lo) / (hi - lo), 0.0f, 1.0f);
}
