#include "minitest.h"

#include <math.h>
#include <stddef.h>

#include "hexball.h"

/* 可复现的伪随机源：按脚本依次返回 */
static uint32_t s_rand_script[8];
static int s_rand_len;
static int s_rand_idx;
#define RAND_LOG_MAX 8
static uint32_t s_rand_low[RAND_LOG_MAX];
static uint32_t s_rand_high[RAND_LOG_MAX];
static int s_rand_calls;

static void rand_script(uint32_t a, uint32_t b)
{
    s_rand_script[0] = a;
    s_rand_script[1] = b;
    s_rand_len = 2;
    s_rand_idx = 0;
    s_rand_calls = 0;
}

static uint32_t scripted_rand(void *user, uint32_t low, uint32_t high)
{
    (void)user;
    const int n = s_rand_calls++;
    if (n < RAND_LOG_MAX) {
        s_rand_low[n] = low;
        s_rand_high[n] = high;
    }
    if (s_rand_idx >= s_rand_len) return 0u;
    return s_rand_script[s_rand_idx++];
}

static void fresh(hexball_game_t *g)
{
    hexball_reset(g, &k_hexball_default_params);
}

/* 把球停在圆心并冻结：只剩"环收缩到最小半径"一条重生路径，便于逐项隔离验证 */
static void park_ball_at_center(hexball_game_t *g)
{
    g->ball.x = k_hexball_default_params.center_x;
    g->ball.y = k_hexball_default_params.center_y;
    g->ball.vx = 0.0f;
    g->ball.vy = 0.0f;
}

MT_TEST(test_hexball_default_params_match_legacy)
{
    CHECK_NEAR(k_hexball_default_params.center_x, 180.0f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.center_y, 180.0f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.screen_radius, 172.0f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.ball_radius, 8.0f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.speed, 2.2f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.shrink_speed, 0.25f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.min_ring_radius, 28.0f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.spawn_radius, 237.0f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.grab_tolerance, 35.0f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.init_radius[0], 65.0f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.init_radius[1], 110.0f, 1e-6);
    CHECK_NEAR(k_hexball_default_params.init_radius[2], 155.0f, 1e-6);
    CHECK_EQ(HEXBALL_RING_COUNT, 3);
    CHECK_EQ(HEXBALL_SIDES, 6);
}

MT_TEST(test_hexball_reset_state)
{
    hexball_game_t g;
    fresh(&g);

    CHECK_NEAR(g.ball.x, 180.0f, 1e-6);
    CHECK_NEAR(g.ball.y, 180.0f, 1e-6);
    CHECK_NEAR(g.ball.vx, 2.2f * cosf(HEXBALL_RESET_ANGLE_RAD), 1e-6);
    CHECK_NEAR(g.ball.vy, 2.2f * sinf(HEXBALL_RESET_ANGLE_RAD), 1e-6);
    CHECK_EQ(hexball_score(&g), 0);
    CHECK_FALSE(g.dragging);
    CHECK_EQ(g.drag_ring, -1);

    for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
        CHECK(g.ring[i].alive);
        CHECK_FALSE(g.ring[i].fresh);
        CHECK_NEAR(g.ring[i].radius, k_hexball_default_params.init_radius[i], 1e-6);
        CHECK_NEAR(g.ring[i].angle, (float)i * HEXBALL_STAGGER_ANGLE_RAD, 1e-6);
        CHECK_EQ(g.ring[i].gap, i * HEXBALL_GAP_STAGGER);
    }
}

MT_TEST(test_hexball_vertex_geometry)
{
    hexball_game_t g;
    fresh(&g);

    /* 顶点 0 落在 angle 方向的半径端点上 */
    const hexball_ring_t *r = &g.ring[0];
    float x, y;
    hexball_vertex(r, &k_hexball_default_params, 0, &x, &y);
    CHECK_NEAR(x, 180.0f + r->radius, 1e-4);
    CHECK_NEAR(y, 180.0f, 1e-4);

    /* 相邻顶点相差 60° */
    float x1, y1;
    hexball_vertex(r, &k_hexball_default_params, 1, &x1, &y1);
    const float dx = x1 - 180.0f;
    const float dy = y1 - 180.0f;
    CHECK_NEAR(ui_rad_to_deg(atan2f(dy, dx)), 60.0f, 1e-3);

    /* 第 6 个顶点与第 0 个重合 */
    float x6, y6;
    hexball_vertex(r, &k_hexball_default_params, 6, &x6, &y6);
    CHECK_NEAR(x6, x, 1e-4);
    CHECK_NEAR(y6, y, 1e-4);

    hexball_vertex(NULL, &k_hexball_default_params, 0, &x, &y); /* 不崩 */
    hexball_vertex(r, NULL, 0, &x, &y);
    hexball_vertex(r, &k_hexball_default_params, 0, NULL, &y);
}

MT_TEST(test_hexball_rings_shrink_and_score)
{
    hexball_game_t g;
    fresh(&g);
    park_ball_at_center(&g);
    rand_script(0u, 0u);

    /* 最内圈 65px，以 0.25px/帧收缩到 28px 需要 148 帧 */
    const float delta = 65.0f - k_hexball_default_params.min_ring_radius;
    const int frames = (int)(delta / k_hexball_default_params.shrink_speed) + 1;

    for (int i = 0; i < frames; i++) {
        rand_script(0u, 0u);
        hexball_step(&g, &k_hexball_default_params, scripted_rand, NULL);
    }

    CHECK(g.score >= 1);
    /* 重生到外圈，且 gap/angle 来自注入的随机数 */
    CHECK_NEAR(g.ring[0].radius, k_hexball_default_params.spawn_radius, 1e-3);
    CHECK(g.ring[0].alive);
    CHECK_EQ((int)g.ring[0].gap, 0);
    /* 只有最内圈重生了一次；重生顺序固定为先角度后缺口 */
    CHECK_EQ(s_rand_calls, 2);
    CHECK_EQ(s_rand_high[0], (uint32_t)HEXBALL_ANGLE_SPAN_UNITS);
    CHECK_EQ(s_rand_high[1], (uint32_t)HEXBALL_LAST_SIDE);
}

MT_TEST(test_hexball_respawn_uses_injected_random)
{
    hexball_game_t g;
    fresh(&g);
    park_ball_at_center(&g);
    g.ring[0].radius = k_hexball_default_params.min_ring_radius + 0.1f;

    rand_script(314u, 2u);
    hexball_step(&g, &k_hexball_default_params, scripted_rand, NULL);

    CHECK_NEAR(g.ring[0].angle, 314u / HEXBALL_ANGLE_SCALE, 1e-5);
    CHECK_EQ(g.ring[0].gap, 2);
    CHECK_NEAR(g.ring[0].radius, k_hexball_default_params.spawn_radius, 1e-3);
    CHECK_EQ(s_rand_low[0], 0u);
    CHECK_EQ(s_rand_high[0], (uint32_t)HEXBALL_ANGLE_SPAN_UNITS);
    CHECK_EQ(s_rand_high[1], (uint32_t)HEXBALL_LAST_SIDE);
}

/* 没有随机源时也必须能重生（默认值路径） */
MT_TEST(test_hexball_step_without_random_source)
{
    hexball_game_t g;
    fresh(&g);
    g.ring[1].radius = k_hexball_default_params.min_ring_radius + 0.1f;

    hexball_step(&g, &k_hexball_default_params, NULL, NULL);

    CHECK(g.ring[1].alive);
    CHECK_NEAR(g.ring[1].radius, k_hexball_default_params.spawn_radius, 1e-3);
    CHECK_NEAR(g.ring[1].angle, 0.0f, 1e-6);
    CHECK_EQ(g.ring[1].gap, 0);
}

MT_TEST(test_hexball_ball_bounces_off_screen_edge)
{
    hexball_game_t g;
    fresh(&g);

    /* 把球放到右侧边界外，朝右飞 */
    g.ball.x = 180.0f + k_hexball_default_params.screen_radius - 1.0f;
    g.ball.y = 180.0f;
    g.ball.vx = 4.0f;
    g.ball.vy = 0.0f;
    for (int i = 0; i < HEXBALL_RING_COUNT; i++) g.ring[i].alive = false;

    const float vx_before = g.ball.vx;
    hexball_step(&g, &k_hexball_default_params, NULL, NULL);

    CHECK(g.ball.vx < vx_before); /* 已反弹 */
    CHECK(ui_vector_len(g.ball.x - 180.0f, g.ball.y - 180.0f) +
              k_hexball_default_params.ball_radius <=
          k_hexball_default_params.screen_radius + 0.01f);
}

MT_TEST(test_hexball_ball_bounces_off_solid_side)
{
    hexball_game_t g;
    fresh(&g);

    /* 只留一个环，缺口朝上，球从右侧水平飞向环 */
    for (int i = 1; i < HEXBALL_RING_COUNT; i++) g.ring[i].alive = false;
    g.ring[0].radius = 100.0f;
    g.ring[0].angle = 0.0f;
    g.ring[0].gap = 2;
    g.ring[0].alive = true;

    /* 把球放在某条实心边外侧一点点，速度指向该边：应被法向弹回 */
    const int side = (g.ring[1].gap + 3) % HEXBALL_SIDES;
    float ax, ay, bx2, by2;
    hexball_vertex(&g.ring[1], &k_hexball_default_params, side, &ax, &ay);
    hexball_vertex(&g.ring[1], &k_hexball_default_params, (side + 1) % HEXBALL_SIDES, &bx2, &by2);

    const float mx = (ax + bx2) * 0.5f;
    const float my = (ay + by2) * 0.5f;
    float nx = mx - k_hexball_default_params.center_x;
    float ny = my - k_hexball_default_params.center_y;
    const float nl = ui_vector_len(nx, ny);
    nx /= nl;
    ny /= nl;

    g.ball.x = mx + nx * 4.0f;
    g.ball.y = my + ny * 4.0f;
    g.ball.vx = -nx * 2.0f;
    g.ball.vy = -ny * 2.0f;

    const float dot_before = g.ball.vx * nx + g.ball.vy * ny; /* 指向边内 < 0 */
    hexball_step(&g, &k_hexball_default_params, NULL, NULL);

    const float dot_after = g.ball.vx * nx + g.ball.vy * ny;
    CHECK(dot_before < 0.0f);
    CHECK(dot_after > 0.0f); /* 反向 */
}

MT_TEST(test_hexball_scoring_when_ball_crosses_ring_radius)
{
    hexball_game_t g;
    fresh(&g);
    rand_script(0u, 0u);

    for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
        g.ring[i].radius = 400.0f + (float)i * 200.0f; /* 只有 ring0 参与本帧判定 */
    }
    g.ring[0].radius = 100.0f;

    /* 沿"缺口那条边"的方向往外飞：既不会被实心边弹回，又会跨过半径 */
    const int gap = g.ring[0].gap;
    float ax, ay, bx2, by2;
    hexball_vertex(&g.ring[0], &k_hexball_default_params, gap, &ax, &ay);
    hexball_vertex(&g.ring[0], &k_hexball_default_params, (gap + 1) % HEXBALL_SIDES, &bx2, &by2);
    float nx = (ax + bx2) * 0.5f - k_hexball_default_params.center_x;
    float ny = (ay + by2) * 0.5f - k_hexball_default_params.center_y;
    const float nl = ui_vector_len(nx, ny);
    nx /= nl;
    ny /= nl;

    g.ball.x = k_hexball_default_params.center_x + nx * 96.0f;
    g.ball.y = k_hexball_default_params.center_y + ny * 96.0f;
    g.ball.vx = nx * 6.0f;
    g.ball.vy = ny * 6.0f;

    const int score_before = g.score;
    hexball_step(&g, &k_hexball_default_params, scripted_rand, NULL);

    CHECK_EQ(g.score, score_before + 1);
    CHECK(g.ring[0].alive);       /* 破环后立即重生 */
    CHECK_NEAR(g.ring[0].radius, k_hexball_default_params.spawn_radius, 1e-3);
}

/* 实心边上穿过不应得分（会被弹开） */
MT_TEST(test_hexball_no_score_when_blocked_by_solid_side)
{
    hexball_game_t g;
    fresh(&g);

    for (int i = 0; i < HEXBALL_RING_COUNT; i++) g.ring[i].radius = 100.0f;
    const int solid = (g.ring[0].gap + 2) % HEXBALL_SIDES;

    float ax, ay, bx2, by2;
    hexball_vertex(&g.ring[0], &k_hexball_default_params, solid, &ax, &ay);
    hexball_vertex(&g.ring[0], &k_hexball_default_params, (solid + 1) % HEXBALL_SIDES, &bx2, &by2);
    float nx = (ax + bx2) * 0.5f - k_hexball_default_params.center_x;
    float ny = (ay + by2) * 0.5f - k_hexball_default_params.center_y;
    const float nl = ui_vector_len(nx, ny);
    nx /= nl;
    ny /= nl;

    g.ball.x = k_hexball_default_params.center_x + nx * 96.0f;
    g.ball.y = k_hexball_default_params.center_y + ny * 96.0f;
    g.ball.vx = nx * 6.0f;
    g.ball.vy = ny * 6.0f;

    const int score_before = g.score;
    hexball_step(&g, &k_hexball_default_params, NULL, NULL);
    CHECK(g.score <= score_before + HEXBALL_RING_COUNT); /* 至少不因这条边额外破环 */
}

MT_TEST(test_hexball_fresh_ring_skips_break_check)
{
    hexball_game_t g;
    fresh(&g);

    /* 让所有环与球同心，若不排除 fresh 会立刻被判定穿过 */
    for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
        g.ring[i].radius = 200.0f;
        g.ring[i].gap = 0;
    }
    g.ball.x = 180.0f;
    g.ball.y = 180.0f;
    g.ball.vx = 0.0f;
    g.ball.vy = 0.0f;

    hexball_step(&g, &k_hexball_default_params, NULL, NULL);
    for (int i = 0; i < HEXBALL_RING_COUNT; i++) CHECK_FALSE(g.ring[i].fresh);
}

MT_TEST(test_hexball_nearest_ring_within_tolerance)
{
    hexball_game_t g;
    fresh(&g);

    /* 65 / 110 / 155 三条环 */
    CHECK_EQ(hexball_ring_at_distance(&g, &k_hexball_default_params, 66.0f), 0);
    CHECK_EQ(hexball_ring_at_distance(&g, &k_hexball_default_params, 108.0f), 1);
    CHECK_EQ(hexball_ring_at_distance(&g, &k_hexball_default_params, 155.0f), 2);

    /* ring3 初始半径 0；155 的容差内是 2，超出所有环 35px 才算抓不到 */
    CHECK_EQ(hexball_ring_at_distance(&g, &k_hexball_default_params, 180.0f), 2);
    CHECK_EQ(hexball_ring_at_distance(&g, &k_hexball_default_params, 210.0f), -1);

    /* 死环不参与 */
    g.ring[1].alive = false;
    CHECK_EQ(hexball_ring_at_distance(&g, &k_hexball_default_params, 110.0f), -1);

    CHECK_EQ(hexball_ring_at_distance(NULL, &k_hexball_default_params, 10.0f), -1);
    CHECK_EQ(hexball_ring_at_distance(&g, NULL, 10.0f), -1);
}

MT_TEST(test_hexball_drag_rotates_grabbed_ring)
{
    hexball_game_t g;
    fresh(&g);

    /* 在半径 110 的环上按下（y 轴正方向，角度 90°） */
    hexball_begin_drag(&g, &k_hexball_default_params, 180.0f, 180.0f + 110.0f);
    CHECK(g.dragging);
    CHECK_EQ(g.drag_ring, 1);

    const float angle_before = g.ring[1].angle;
    const float other_before = g.ring[0].angle;

    /* 拖到 180° 方向：环应转过 90° */
    hexball_drag_to(&g, &k_hexball_default_params, 180.0f - 110.0f, 180.0f);
    /* 按下点在正下方(90°)，拖到正左方(180°)：环应逆时针转过 +90° */
    CHECK_NEAR(ui_wrap_rad(g.ring[1].angle - angle_before), UI_PI_F / 2.0f, 1e-3);
    CHECK_NEAR(g.ring[0].angle, other_before, 1e-9); /* 其他环不受影响 */

    hexball_end_drag(&g);
    CHECK_FALSE(g.dragging);
    CHECK_EQ(g.drag_ring, -1);

    /* 未按下时拖动无效果 */
    hexball_drag_to(&g, &k_hexball_default_params, 0.0f, 0.0f);
    CHECK_NEAR(g.ring[1].angle, angle_before + UI_PI_F / 2.0f, 1e-3);
}

/* 跨 ±π 时不能突然反向转一圈 */
MT_TEST(test_hexball_drag_takes_shortest_path)
{
    hexball_game_t g;
    fresh(&g);

    hexball_begin_drag(&g, &k_hexball_default_params, 180.0f, 180.0f + 65.0f); /* 90° */
    CHECK_EQ(g.drag_ring, 0);
    const float before = g.ring[0].angle;

    /* 拖到略低于 -90°（即 270°）：短路径约为 180°，不产生跳变 */
    hexball_drag_to(&g, &k_hexball_default_params, 180.0f, 180.0f - 65.0f);
    CHECK(ui_vector_len(g.ring[0].angle - before, 0.0f) <= UI_PI_F + 1e-3f);
}

MT_TEST(test_hexball_drawable_and_score_helpers)
{
    hexball_game_t g;
    fresh(&g);

    CHECK(hexball_ring_is_drawable(&g, 0));
    CHECK(hexball_ring_is_drawable(&g, HEXBALL_RING_COUNT - 1));
    CHECK_FALSE(hexball_ring_is_drawable(&g, HEXBALL_RING_COUNT));
    CHECK_FALSE(hexball_ring_is_drawable(&g, -1));
    CHECK_FALSE(hexball_ring_is_drawable(NULL, 0));

    g.ring[2].alive = false;
    CHECK_FALSE(hexball_ring_is_drawable(&g, 2));
    CHECK_EQ(hexball_score(&g), 0);
    CHECK_EQ(hexball_score(NULL), 0);
}

MT_TEST(test_hexball_null_safety)
{
    hexball_game_t g;
    fresh(&g);

    hexball_reset(NULL, &k_hexball_default_params);
    hexball_reset(&g, NULL);
    hexball_step(NULL, &k_hexball_default_params, NULL, NULL);
    hexball_step(&g, NULL, NULL, NULL);
    hexball_begin_drag(NULL, &k_hexball_default_params, 1.0f, 1.0f);
    hexball_begin_drag(&g, NULL, 1.0f, 1.0f);
    hexball_drag_to(NULL, &k_hexball_default_params, 1.0f, 1.0f);
    hexball_drag_to(&g, NULL, 1.0f, 1.0f);
    hexball_end_drag(NULL);

    CHECK_EQ(hexball_score(&g), 0); /* 全部空操作，不应污染状态 */
}
