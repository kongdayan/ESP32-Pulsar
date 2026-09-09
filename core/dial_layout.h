/*
 * dial_layout.h — 圆形表盘的极坐标切割模型（纯几何，不依赖 LVGL / Arduino / HAL）。
 *
 * 解决的问题：圆屏上"哪一块能放什么"不该靠肉眼抠绝对坐标。
 * 本文件只回答三件事：
 *   1. 半径怎么分带（dial_band_t：边缘带 / 数据环 / 内容带 / 焦点区）
 *   2. 角度怎么切格（扇区数、单格净宽、触点落在第几格）
 *   3. 某个位置到底可用不可用（行跨度、内接正方形、线宽居中的 arc 最大半径）
 *
 * 角度约定（与屏幕视觉一致，方便口算）：**12 点钟方向为 0°，顺时针为正**。
 * 要喂给 LVGL 8 的 lv_draw_arc 时用 dial_deg_to_lvgl()（LVGL 的 0° 在 3 点钟）。
 */
#ifndef CORE_DIAL_LAYOUT_H
#define CORE_DIAL_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 基准圆 ──────────────────────────────────────────────────────────────── */
/* 玻璃物理半径（360px 屏 = 180），只用来算"看得见但不该用"的区域 */
#define DIAL_GLASS_R_PX ((float)APP_SCREEN_PX / 2.0f)
/* 安全半径：圆形可视区内切的最大半径，所有内容与触摸都以此为界 */
#define DIAL_SAFE_R_PX  APP_SCREEN_USABLE_RADIUS
#define DIAL_CX         ((float)APP_SCREEN_CENTER_X)
#define DIAL_CY         ((float)APP_SCREEN_CENTER_Y)

/* 12/3/6/9 点方向 */
#define DIAL_DEG_TOP     0.0f
#define DIAL_DEG_RIGHT   90.0f
#define DIAL_DEG_BOTTOM  180.0f
#define DIAL_DEG_LEFT    270.0f
#define DIAL_DEG_FULL    360.0f

/* 圆形表盘从外到内的四个环带（半径写成占安全半径的比例，屏幕尺寸变了也能跟着缩放） */
#define DIAL_EDGE_R_IN_F   0.95f    /* 边缘带内沿：只放 tick / 状态点，不放文字，触摸不可靠 */
#define DIAL_RING_R_IN_F   0.872f   /* 数据环内沿：约 150px */
#define DIAL_FOCUS_R_OUT_F 0.36f    /* 焦点区外沿：约 62px */

#define DIAL_EDGE_R_IN   (DIAL_SAFE_R_PX * DIAL_EDGE_R_IN_F)
#define DIAL_RING_R_OUT  DIAL_EDGE_R_IN
#define DIAL_RING_R_IN   (DIAL_SAFE_R_PX * DIAL_RING_R_IN_F)
#define DIAL_FOCUS_R_OUT (DIAL_SAFE_R_PX * DIAL_FOCUS_R_OUT_F)

/* 内容带（B2）是"圆内唯一可以放心排版文字"的带：r ∈ [DIAL_FOCUS_R_OUT, DIAL_RING_R_IN] */

typedef enum {
    DIAL_BAND_EDGE = 0,  /* 边缘带：tick、状态点、时间戳 */
    DIAL_BAND_RING,      /* 数据环：arc / 进度条 / 频谱条 */
    DIAL_BAND_CONTENT,   /* 内容带：文字与图形主体 */
    DIAL_BAND_FOCUS,     /* 焦点区：只放一个主角（大数字 / 图标 / 时间） */
    DIAL_BAND_COUNT
} dial_band_id_t;

/* dial_band_at() 的非法返回：半径为负或已超出安全圆 */
#define DIAL_BAND_INVALID (-1)

typedef struct {
    dial_band_id_t id;
    const char    *name;
    float          r_out;
    float          r_in;
} dial_band_t;

typedef struct {
    float x, y;
} dial_pointf_t;

typedef struct {
    int16_t x, y;
} dial_point_t;

typedef struct {
    int16_t x1;
    int16_t x2;
    bool    valid;   /* false = 该行已在安全圆之外 */
} dial_row_span_t;

extern const dial_band_t dial_bands[DIAL_BAND_COUNT];

/* ── 角度 ────────────────────────────────────────────────────────────────── */
float dial_norm_deg(float deg);                 /* 归一化到 [0,360) */
float dial_delta_deg(float from_deg, float to_deg);  /* 带符号最小夹角，[-180,180) */
float dial_deg_to_lvgl(float deg);              /* 12 点基准 → LVGL 3 点基准 */
float dial_deg_from_lvgl(float lvgl_deg);       /* 反向 */
float dial_deg_step(int count);                 /* 每格多少度，count<=0 时返回 0 */
float dial_sector_center_deg(int count, int index);

/* ── 极坐标 ↔ 屏幕坐标 ───────────────────────────────────────────────────── */
dial_pointf_t dial_point_at(float r, float deg);
dial_point_t  dial_point_at_i(float r, float deg);
float         dial_deg_at(int x, int y);        /* 屏幕上任意点的角度（12 点=0，顺时针+） */
float         dial_radius_at(int x, int y);     /* 到圆心的距离 */
bool          dial_inside_safe(int x, int y, int margin_px);
/* 半径属于哪个带（dial_band_id_t）；负数或超出安全半径返回 -1 */
int           dial_band_at(float r);

/* dial 角区间 → LVGL 8 lv_draw_arc 的 (start, end)：0° 在 3 点、顺时针，
 * end 可以大于 360（LVGL 内部按象限分块绘制）；满圆固定返回 0..360 */
typedef struct {
    int16_t start_lvgl;
    int16_t end_lvgl;
} dial_arc_lvgl_t;
dial_arc_lvgl_t dial_arc_span_lvgl(float start_deg, float sweep_deg);

/* ── 弧长 / 分格 / 命中 ──────────────────────────────────────────────────── */
float dial_arc_len(float r, float deg);         /* 半径 r 上 deg 度对应的像素弧长 */
float dial_slot_width_px(float r, int count, float gap_deg);
int   dial_sector_count(float r, float min_slot_px, float gap_deg);
int   dial_tick_count(float r, float dot_pitch_px);
/* 触点 (x,y) 命中第几格；落在环带外或格间间隙返回 -1 */
/* dial_hit_sector() 的未命中返回：落在环带外或格间间隙 */
#define DIAL_SECTOR_MISS (-1)
int   dial_hit_sector(int x, int y, float r_out, float r_in, int count, float gap_deg);

/* ── 排版可用性 ──────────────────────────────────────────────────────────── */
dial_row_span_t dial_row_span(int y, int margin_px);
int             dial_inset_side(int margin_px); /* 圆内接正方形边长 */
float           dial_max_arc_radius(float line_width, int margin_px);
float           dial_pct_clamped(float pct);    /* 夹到 [0,100] */
float           dial_pct_to_deg(float pct, float sweep_deg);

/* ── 现成的分格预设（把"切几格"变成查表，不用现场算）───────────────────── */
typedef enum {
    DIAL_DIV_12 = 0,   /* 时钟刻度 / 12 个月 */
    DIAL_DIV_8,        /* 一周七天 + 休息，45° 一格 */
    DIAL_DIV_24,       /* 24 小时环 */
    DIAL_DIV_48,       /* 能量条 / 频谱下限 */
    DIAL_DIV_COUNT
} dial_div_preset_t;

typedef struct {
    const char *name;
    int         count;
    float       gap_deg;
    int         min_slot_px;   /* 该预设要求的最小单格净宽 */
    float       r_min;         /* 用多大的半径才够宽 */
} dial_div_t;

extern const dial_div_t dial_div_presets[DIAL_DIV_COUNT];

int                dial_div_count(dial_div_preset_t preset);   /* 预设的格数（非数组下标） */
const dial_div_t  *dial_div(dial_div_preset_t preset);

#ifdef __cplusplus
}
#endif

#endif /* CORE_DIAL_LAYOUT_H */
