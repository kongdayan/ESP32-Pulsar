/*
 * 差分测试：把重构后的 core/hexball.c、core/cube3d.c 与重构前的原始实现
 * （tests/reference/legacy_*.c，逐字搬运）在相同输入下逐帧对比。
 *
 * 这是"界面行为不变"最直接的机器证明。
 */
#include "minitest.h"

#include <math.h>
#include <stddef.h>

#include "cube3d.h"
#include "hexball.h"
#include "legacy_cube3d.h"
#include "legacy_hexball.h"

/* ── 供两边共用的确定性随机源 ─────────────────────────────────────────────── */

typedef struct { uint32_t state; } lcg_t;

static uint32_t lcg_next(void *user, uint32_t low, uint32_t high)
{
    lcg_t *rng = (lcg_t *)user;
    rng->state = rng->state * 1664525u + 1013904223u;
    const uint32_t span = high - low + 1u;
    return low + (rng->state >> 8) % span;
}

/* ── Hex-Ball ─────────────────────────────────────────────────────────────── */

MT_TEST(test_hexball_matches_legacy_physics)
{
    hexball_game_t next;
    legacy_hexball_t prev;
    lcg_t rng_a = { 12345u };
    lcg_t rng_b = { 12345u };

    hexball_reset(&next, &k_hexball_default_params);
    legacy_reset(&prev);

    for (int frame = 0; frame < 6000; frame++) {
        hexball_step(&next, &k_hexball_default_params, lcg_next, &rng_a);
        legacy_physics_step(&prev, lcg_next, &rng_b);

        CHECK_EQ(next.score, prev.score);
        CHECK_NEAR(next.ball.x, prev.ball.x, 1e-4);
        CHECK_NEAR(next.ball.y, prev.ball.y, 1e-4);
        CHECK_NEAR(next.ball.vx, prev.ball.vx, 1e-4);
        CHECK_NEAR(next.ball.vy, prev.ball.vy, 1e-4);

        for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
            CHECK_EQ(next.ring[i].alive, prev.rings[i].alive);
            CHECK_EQ(next.ring[i].gap, prev.rings[i].gap);
            CHECK_NEAR(next.ring[i].radius, prev.rings[i].radius, 1e-4);
            CHECK_NEAR(next.ring[i].angle, prev.rings[i].angle, 1e-4);
        }
    }

    /* 长跑之后必须有得分，否则说明两边都在空转 */
    CHECK(next.score > 0);
}

MT_TEST(test_hexball_grab_matches_legacy)
{
    hexball_game_t next;
    legacy_hexball_t prev;
    hexball_reset(&next, &k_hexball_default_params);
    legacy_reset(&prev);

    for (float dist = 0.0f; dist < 260.0f; dist += 0.5f) {
        CHECK_EQ(hexball_ring_at_distance(&next, &k_hexball_default_params, dist),
                 legacy_nearest_ring(&prev, dist));
    }
}

MT_TEST(test_hexball_vertices_match_legacy)
{
    hexball_game_t next;
    legacy_hexball_t prev;
    hexball_reset(&next, &k_hexball_default_params);
    legacy_reset(&prev);

    /* 人为制造一个带旋转的环，覆盖各象限 */
    next.ring[0].angle = 0.7f;
    prev.rings[0].angle = 0.7f;
    next.ring[0].radius = 123.0f;
    prev.rings[0].radius = 123.0f;

    for (int v = 0; v < HEXBALL_SIDES * 2; v++) {
        float x = 0.0f, y = 0.0f;
        hexball_vertex(&next.ring[0], &k_hexball_default_params, v, &x, &y);

        const float expected_x = 180.0f + 123.0f * cosf(0.7f + v * (3.14159265f / 3.0f));
        const float expected_y = 180.0f + 123.0f * sinf(0.7f + v * (3.14159265f / 3.0f));
        CHECK_NEAR(x, expected_x, 1e-4);
        CHECK_NEAR(y, expected_y, 1e-4);
    }
}

/* ── 3D 立方体 ────────────────────────────────────────────────────────────── */

MT_TEST(test_cube3d_projection_matches_legacy)
{
    const cube3d_params_t *p = &k_cube3d_default_params;

    for (int step = 0; step < 400; step++) {
        const float rot = (float)step * 0.05f;
        const float scale = p->min_scale + fmodf((float)step, p->max_scale - p->min_scale);

        cube3d_state_t next_s = { rot, -rot * 0.5f, 0.0f, 0.0f, scale, 0.0f };
        legacy_cube_state prev_s = { rot, -rot * 0.5f, 0.0f, 0.0f, scale, 0.0f };

        cube3d_point_t a[CUBE3D_VERTEX_COUNT];
        legacy_projected b[CUBE3D_VERTEX_COUNT];
        cube3d_face_order_t order_a[CUBE3D_FACE_COUNT];
        legacy_face_order order_b[CUBE3D_FACE_COUNT];

        cube3d_project(&next_s, p, a);
        legacy_project_cube(&prev_s, b);
        for (int i = 0; i < CUBE3D_VERTEX_COUNT; i++) {
            CHECK_EQ(a[i].x, b[i].x);
            CHECK_EQ(a[i].y, b[i].y);
            CHECK_NEAR(a[i].z, b[i].z, 1e-5);
        }

        cube3d_sort_faces(a, order_a);
        legacy_sort_faces(b, order_b);
        for (int i = 0; i < CUBE3D_FACE_COUNT; i++) {
            CHECK_EQ(order_a[i].idx, order_b[i].idx);
            CHECK_NEAR(order_a[i].z, order_b[i].z, 1e-5);
        }

        CHECK_EQ(cube3d_zoom_percent(p, scale), legacy_zoom_percent(scale));
        CHECK_NEAR(cube3d_zoom_ratio(p, scale),
                   (scale - p->min_scale) / (p->max_scale - p->min_scale), 1e-6);
    }
}

MT_TEST(test_cube3d_hit_zones_match_legacy)
{
    const cube3d_params_t *p = &k_cube3d_default_params;

    for (int y = 0; y < 360; y += 7) {
        for (int x = 0; x < 360; x += 11) {
            CHECK_EQ(cube3d_is_nav_zone(p, x, y), legacy_is_nav_zone(x, y));
            CHECK_EQ(cube3d_is_zoom_zone(p, x, y), legacy_is_zoom_zone(x, y));
            CHECK_NEAR(cube3d_scale_from_point(p, x, y), legacy_zoom_scale_from_point(x, y), 1e-4);
        }
    }
}

MT_TEST(test_cube3d_motion_matches_legacy)
{
    const cube3d_params_t *p = &k_cube3d_default_params;
    cube3d_state_t next_s = k_cube3d_default_state;
    legacy_cube_state prev_s = { -0.48f, 0.72f, 0.0f, 0.0f, 95.0f, 0.0f };

    for (int frame = 0; frame < 500; frame++) {
        const int dx = (frame % 13) - 6;
        const int dy = (frame % 7) - 3;

        if (frame % 3 == 0) {
            cube3d_apply_rotate(&next_s, p, dx, dy);
            legacy_drag_rotate(&prev_s, dx, dy);
        } else if (frame % 3 == 1) {
            cube3d_apply_zoom(&next_s, p, 180 + 142 - (frame % 40), 180 + (frame % 20));
            legacy_drag_zoom(&prev_s, 180 + 142 - (frame % 40), 180 + (frame % 20));
        }

        const bool dragging = (frame % 5) == 0;
        cube3d_tick_inertia(&next_s, p, dragging);
        legacy_inertia_tick(&prev_s, dragging);

        CHECK_NEAR(next_s.rot_x, prev_s.rot_x, 1e-6);
        CHECK_NEAR(next_s.rot_y, prev_s.rot_y, 1e-6);
        CHECK_NEAR(next_s.vel_x, prev_s.vel_x, 1e-6);
        CHECK_NEAR(next_s.vel_y, prev_s.vel_y, 1e-6);
        CHECK_NEAR(next_s.scale, prev_s.cube_scale, 1e-5);
        CHECK_NEAR(next_s.zoom_vel, prev_s.zoom_vel, 1e-5);
        CHECK_EQ(cube3d_zoom_percent(p, next_s.scale), legacy_zoom_percent(next_s.scale));
    }
}

MT_TEST(test_cube3d_swipe_rule_matches_legacy)
{
    const cube3d_params_t *p = &k_cube3d_default_params;

    for (int dx = -120; dx <= 120; dx += 6) {
        for (int dy = -60; dy <= 60; dy += 5) {
            CHECK_EQ(cube3d_is_nav_swipe(p, dx, dy), legacy_swipe_navigates(dx, dy));
        }
    }
}
