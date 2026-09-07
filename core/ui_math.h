/*
 * ui_math.h — 与 LVGL / 硬件无关的纯数学工具，可主机侧单元测试。
 */
#ifndef CORE_UI_MATH_H
#define CORE_UI_MATH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_PI_F          3.14159265f
#define UI_TWO_PI_F      (2.0f * UI_PI_F)
#define UI_HALF_F        0.5f
#define UI_EPS_F         1e-9f
#define UI_DEG_FULL      360.0f
#define UI_DEG_PER_RAD_F (180.0f / UI_PI_F)
#define UI_RAD_PER_DEG_F (UI_PI_F / 180.0f)
#define UI_PCT_FULL      100
#define UI_PCT_ROUNDING  50 /* 整数百分比四舍五入用的偏置 */

/* 标量 */
float ui_clampf(float value, float lo, float hi);
float ui_lerpf(float a, float b, float t);
bool  ui_is_zero_f(float value, float eps);

/* 角度 */
float ui_deg_to_rad(float deg);
float ui_rad_to_deg(float rad);
float ui_normalize_deg(float deg);  /* 归一化到 [0, 360)，输入需在 [-360, 360) */
float ui_wrap_rad(float rad);       /* 归一化到 (-pi, pi]，输入需在 (-2pi, 2pi) */
float ui_angle_delta_rad(float from, float to); /* 最短带符号角差 */

/* 向量 */
float ui_vector_len(float x, float y);
float ui_angle_deg_of(float dx, float dy);      /* atan2，结果 [0, 360) */

/* 整数映射 */
int   ui_abs_i(int value);
int   ui_min_i(int a, int b);
int   ui_max_i(int a, int b);
int   ui_clampi(int value, int lo, int hi);
int   ui_percent_of(int value, int span);                    /* value/span*100，四舍五入 */
int   ui_steps_for_percent(int total_steps, int percent);    /* total*percent/100，四舍五入 */
float ui_unit_span(float value, float lo, float hi);         /* (value-lo)/(hi-lo) 并夹紧 */

#ifdef __cplusplus
}
#endif

#endif /* CORE_UI_MATH_H */
