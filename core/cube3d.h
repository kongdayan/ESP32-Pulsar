/*
 * cube3d.h — 3D 立方体渲染/交互的纯逻辑（无 LVGL、无硬件）。
 *
 * 投影、面排序、缩放弧命中、惯性衰减都在这里，屏幕层只做绘制与事件转发。
 */
#ifndef CORE_CUBE3D_H
#define CORE_CUBE3D_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CUBE3D_VERTEX_COUNT   8
#define CUBE3D_FACE_COUNT     6
#define CUBE3D_FACE_CORNERS   4
#define CUBE3D_LAST_FACE_IDX  (CUBE3D_FACE_COUNT - 1)
#define CUBE3D_Z_FACE_SCALE   0.25f /* 面深度 = 4 顶点 z 的平均值 */
#define CUBE3D_ZOOM_MIN_PCT   0
#define CUBE3D_ZOOM_MAX_PCT   100

typedef struct { float x, y, z; } cube3d_vec3_t;

typedef struct { int16_t x, y; float z; } cube3d_point_t;

typedef struct { int idx; float z; } cube3d_face_order_t;

typedef struct {
    float rot_x;
    float rot_y;
    float vel_x;
    float vel_y;
    float scale;
    float zoom_vel;
} cube3d_state_t;

typedef struct {
    /* 立方体中心 / 缩放弧圆心（两者 y 不同，沿用旧版取值） */
    float center_x;
    float center_y;
    float arc_center_y;
    /* 缩放范围 */
    float min_scale;
    float max_scale;
    /* 手感参数 */
    float rot_gain;
    float rot_damping;
    float zoom_damping;
    float inertia_eps;
    float zoom_eps;
    float perspective;
    /* 缩放弧 */
    float arc_radius;
    float arc_start_deg;
    float arc_end_deg;
    float arc_hit_width;
    float arc_zone_slop;
    float arc_knob_size;
    float arc_rail_width;
    /* 边缘导航手势 */
    int nav_edge_px;
    int nav_swipe_min_px;
    int nav_swipe_slop_px;
    int screen_px;
} cube3d_params_t;

extern const cube3d_params_t   k_cube3d_default_params;
extern const cube3d_state_t    k_cube3d_default_state;
extern const cube3d_vec3_t     k_cube3d_vertices[CUBE3D_VERTEX_COUNT];
extern const uint8_t           k_cube3d_faces[CUBE3D_FACE_COUNT][CUBE3D_FACE_CORNERS];
extern const uint32_t          k_cube3d_face_colors[CUBE3D_FACE_COUNT];

/* 默认缩放对应的百分比（Reset 后显示值） */
float cube3d_default_scale(const cube3d_params_t *p);
float cube3d_scale_clamped(const cube3d_params_t *p, float scale);
int   cube3d_zoom_percent(const cube3d_params_t *p, float scale);
float cube3d_zoom_percent_f(const cube3d_params_t *p, float scale);
/* 未夹紧的缩放比例 (scale-min)/(max-min)，与旧版进度弧计算完全一致 */
float cube3d_zoom_ratio(const cube3d_params_t *p, float scale);

/* 命中区判定 */
bool  cube3d_is_nav_zone(const cube3d_params_t *p, int x, int y);
bool  cube3d_is_zoom_zone(const cube3d_params_t *p, int x, int y);
float cube3d_scale_from_point(const cube3d_params_t *p, int x, int y);

/* 一次拖拽产生的旋转增量 */
void  cube3d_apply_rotate(cube3d_state_t *s, const cube3d_params_t *p, int dx, int dy);
/* 拖到缩放弧上的某个位置 */
void  cube3d_apply_zoom(cube3d_state_t *s, const cube3d_params_t *p, int x, int y);
/* 一帧惯性推进（拖拽中应跳过） */
void  cube3d_tick_inertia(cube3d_state_t *s, const cube3d_params_t *p, bool dragging);
/* 释放时的位移是否构成切屏滑动 */
bool  cube3d_is_nav_swipe(const cube3d_params_t *p, int total_dx, int total_dy);

/* 只清速度，保留当前姿态（按下新一次拖拽时用） */
void  cube3d_stop_motion(cube3d_state_t *s);
void  cube3d_reset(cube3d_state_t *s, const cube3d_params_t *p);
void  cube3d_project(const cube3d_state_t *s, const cube3d_params_t *p, cube3d_point_t out[CUBE3D_VERTEX_COUNT]);
void  cube3d_sort_faces(const cube3d_point_t points[CUBE3D_VERTEX_COUNT],
                        cube3d_face_order_t out[CUBE3D_FACE_COUNT]);

#ifdef __cplusplus
}
#endif

#endif /* CORE_CUBE3D_H */
