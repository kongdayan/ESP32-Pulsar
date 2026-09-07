/*
 * hexball.h — Hex-Ball 小游戏的纯逻辑模型（无 LVGL、无硬件）。
 *
 * 物理、计分、环重生等全部在这里，屏幕层只做绘制与触摸事件转发。
 * 随机数通过 hexball_rand_fn 注入，保证单元测试可复现。
 */
#ifndef CORE_HEXBALL_H
#define CORE_HEXBALL_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEXBALL_RING_COUNT        3
#define HEXBALL_SIDES             6
#define HEXBALL_LAST_SIDE         (HEXBALL_SIDES - 1)
#define HEXBALL_VERTEX_STEP_RAD   (UI_PI_F / 3.0f)              /* 正六边形相邻顶点夹角 */
#define HEXBALL_RESET_ANGLE_RAD   (UI_PI_F / 6.0f)              /* 初始球速方向 */
#define HEXBALL_STAGGER_ANGLE_RAD (UI_PI_F / 9.0f)              /* 环之间的初始相位差 */
#define HEXBALL_GAP_STAGGER       2                             /* 环之间缺口错开的边数 */
#define HEXBALL_ANGLE_SPAN_UNITS  628u                          /* 随机角度：0..628 → /100 得弧度 */
#define HEXBALL_ANGLE_SCALE       100.0f
#define HEXBALL_SPAWN_EXTRA_R     65.0f                         /* 重生半径 = 可视半径 + 该值 */
#define HEXBALL_MIN_SCORE         0

typedef struct {
    float x, y;
    float vx, vy;
} hexball_ball_t;

typedef struct {
    float radius;
    float angle;
    int   gap;    /* 0..HEXBALL_LAST_SIDE 为开口边，-1 表示无缺口 */
    bool  alive;
    bool  fresh;  /* 刚重生：本帧跳过破环判定 */
} hexball_ring_t;

typedef struct {
    hexball_ball_t ball;
    hexball_ring_t ring[HEXBALL_RING_COUNT];
    int            score;
    bool           dragging;
    int            drag_ring;
    float          drag_angle;
} hexball_game_t;

typedef struct {
    float center_x;
    float center_y;
    float screen_radius;
    float ball_radius;
    float speed;
    float shrink_speed;
    float min_ring_radius;
    float spawn_radius;
    float grab_tolerance;
    float init_radius[HEXBALL_RING_COUNT];
} hexball_params_t;

/* 与旧版 screen_agent.c 中的宏取值一致 */
extern const hexball_params_t k_hexball_default_params;

/* 返回 [low, high] 区间内的伪随机整数 */
typedef uint32_t (*hexball_rand_fn)(void *user, uint32_t low, uint32_t high);

void hexball_reset(hexball_game_t *game, const hexball_params_t *params);

/* 推进一帧：收缩 → 移动 → 边界反弹 → 边反弹 → 破环判定 */
void hexball_step(hexball_game_t *game, const hexball_params_t *params,
                  hexball_rand_fn rnd, void *rnd_user);

/* 环上第 vertex 个顶点的屏幕坐标 */
void hexball_vertex(const hexball_ring_t *ring, const hexball_params_t *params,
                    int vertex, float *x, float *y);

/* 距离中心 dist 处最接近、且在抓取容差内的环；无则返回 -1 */
int hexball_ring_at_distance(const hexball_game_t *game, const hexball_params_t *params,
                             float dist);

void hexball_begin_drag(hexball_game_t *game, const hexball_params_t *params, float x, float y);
void hexball_drag_to(hexball_game_t *game, const hexball_params_t *params, float x, float y);
void hexball_end_drag(hexball_game_t *game);

int  hexball_score(const hexball_game_t *game);
bool hexball_ring_is_drawable(const hexball_game_t *game, int index);

#ifdef __cplusplus
}
#endif

#endif /* CORE_HEXBALL_H */
