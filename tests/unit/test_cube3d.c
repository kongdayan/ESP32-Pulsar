#include "minitest.h"

#include <math.h>
#include <stddef.h>

#include "cube3d.h"

MT_TEST(test_cube3d_default_params_match_legacy)
{
    CHECK_NEAR(k_cube3d_default_params.center_x, 180.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.center_y, 188.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.arc_center_y, 180.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.min_scale, 52.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.max_scale, 138.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.rot_gain, 0.012f, 1e-9);
    CHECK_NEAR(k_cube3d_default_params.rot_damping, 0.942f, 1e-9);
    CHECK_NEAR(k_cube3d_default_params.zoom_damping, 0.885f, 1e-9);
    CHECK_NEAR(k_cube3d_default_params.inertia_eps, 0.0008f, 1e-9);
    CHECK_NEAR(k_cube3d_default_params.zoom_eps, 0.05f, 1e-9);
    CHECK_NEAR(k_cube3d_default_params.perspective, 3.8f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.arc_radius, 142.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.arc_start_deg, 315.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.arc_end_deg, 405.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.arc_hit_width, 26.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_params.arc_zone_slop, 12.0f, 1e-6);
    CHECK_EQ(k_cube3d_default_params.nav_edge_px, 46);
    CHECK_EQ(k_cube3d_default_params.nav_swipe_min_px, 54);
    CHECK_EQ(k_cube3d_default_params.nav_swipe_slop_px, 30);
    CHECK_EQ(k_cube3d_default_params.screen_px, 360);
    CHECK_NEAR(cube3d_default_scale(&k_cube3d_default_params), 95.0f, 1e-6);
    CHECK_NEAR(k_cube3d_default_state.rot_x, -0.48f, 1e-6);
    CHECK_NEAR(k_cube3d_default_state.rot_y, 0.72f, 1e-6);
}

MT_TEST(test_cube3d_face_data_is_consistent)
{
    CHECK_EQ(CUBE3D_VERTEX_COUNT, 8);
    CHECK_EQ(CUBE3D_FACE_COUNT, 6);
    CHECK_EQ(CUBE3D_FACE_CORNERS, 4);

    for (int f = 0; f < CUBE3D_FACE_COUNT; f++) {
        CHECK(k_cube3d_face_colors[f] != 0u);
        for (int c = 0; c < CUBE3D_FACE_CORNERS; c++) {
            CHECK(k_cube3d_faces[f][c] < CUBE3D_VERTEX_COUNT);
        }
    }
    /* 顶点全部为单位立方体的角 */
    for (int v = 0; v < CUBE3D_VERTEX_COUNT; v++) {
        CHECK_NEAR(fabsf(k_cube3d_vertices[v].x), 1.0f, 1e-6);
        CHECK_NEAR(fabsf(k_cube3d_vertices[v].y), 1.0f, 1e-6);
        CHECK_NEAR(fabsf(k_cube3d_vertices[v].z), 1.0f, 1e-6);
    }
}

MT_TEST(test_cube3d_project_default_pose)
{
    cube3d_state_t s = k_cube3d_default_state;
    cube3d_point_t pts[CUBE3D_VERTEX_COUNT];
    cube3d_project(&s, &k_cube3d_default_params, pts);

    /* 投影像应在屏幕附近，且透视系数随 z 单调 */
    for (int i = 0; i < CUBE3D_VERTEX_COUNT; i++) {
        CHECK(pts[i].x > -50 && pts[i].x < 410);
        CHECK(pts[i].y > -50 && pts[i].y < 430);
    }
    /* 立方体对角顶点（0-6 / 1-7 / 2-4 / 3-5）的 z 必须相反 */
    static const int8_t opposite[CUBE3D_VERTEX_COUNT] = { 6, 7, 4, 5, 2, 3, 0, 1 };
    for (int i = 0; i < CUBE3D_VERTEX_COUNT; i++) {
        CHECK_NEAR(pts[i].z, -pts[opposite[i]].z, 1e-5);
    }
    /* 默认姿态下至少有一个顶点在中心右侧、一个在左侧 */
    int left = 0, right = 0;
    for (int i = 0; i < CUBE3D_VERTEX_COUNT; i++) {
        if (pts[i].x < 180) left++;
        if (pts[i].x > 180) right++;
    }
    CHECK(left > 0);
    CHECK(right > 0);

    cube3d_project(NULL, &k_cube3d_default_params, pts);
    cube3d_project(&s, NULL, pts);
    cube3d_project(&s, &k_cube3d_default_params, NULL);
}

MT_TEST(test_cube3d_scale_affects_size)
{
    cube3d_state_t s = k_cube3d_default_state;
    cube3d_point_t small_pts[CUBE3D_VERTEX_COUNT];
    cube3d_point_t big_pts[CUBE3D_VERTEX_COUNT];

    s.scale = k_cube3d_default_params.min_scale;
    cube3d_project(&s, &k_cube3d_default_params, small_pts);
    s.scale = k_cube3d_default_params.max_scale;
    cube3d_project(&s, &k_cube3d_default_params, big_pts);

    const int small_span = small_pts[1].x - small_pts[0].x;
    const int big_span = big_pts[1].x - big_pts[0].x;
    CHECK(big_span > small_span);
}

MT_TEST(test_cube3d_sort_faces_is_back_to_front)
{
    cube3d_state_t s = k_cube3d_default_state;
    cube3d_point_t pts[CUBE3D_VERTEX_COUNT];
    cube3d_face_order_t order[CUBE3D_FACE_COUNT];

    cube3d_project(&s, &k_cube3d_default_params, pts);
    cube3d_sort_faces(pts, order);

    /* 每个面恰好出现一次 */
    for (int i = 0; i < CUBE3D_FACE_COUNT; i++) {
        CHECK(order[i].idx >= 0 && order[i].idx <= CUBE3D_LAST_FACE_IDX);
        for (int j = 0; j < CUBE3D_FACE_COUNT; j++) {
            if (i != j) CHECK_NE(order[i].idx, order[j].idx);
        }
    }
    /* z 递增：远的先画 */
    for (int i = 1; i < CUBE3D_FACE_COUNT; i++) {
        CHECK(order[i - 1].z <= order[i].z);
    }
    cube3d_sort_faces(NULL, order);
    cube3d_sort_faces(pts, NULL);
}

MT_TEST(test_cube3d_nav_zone)
{
    const cube3d_params_t *p = &k_cube3d_default_params;

    CHECK(cube3d_is_nav_zone(p, 0, 180));       /* 左边缘 */
    CHECK(cube3d_is_nav_zone(p, 359, 180));     /* 右边缘 */
    CHECK(cube3d_is_nav_zone(p, 180, 10));      /* 上边缘 */
    CHECK(cube3d_is_nav_zone(p, 180, 350));     /* 下边缘 */
    CHECK_FALSE(cube3d_is_nav_zone(p, 180, 180));
    CHECK_FALSE(cube3d_is_nav_zone(p, 46, 46)); /* 边界值本身不算边缘区 */
    CHECK_FALSE(cube3d_is_nav_zone(NULL, 0, 0));
}

MT_TEST(test_cube3d_zoom_zone)
{
    const cube3d_params_t *p = &k_cube3d_default_params;

    /* 弧中心角 360°（屏幕正右方），半径 142 处应命中 */
    CHECK(cube3d_is_zoom_zone(p, 180 + 142, 180));
    /* 315° 与 405° 端点命中 */
    CHECK(cube3d_is_zoom_zone(p, 180 + (int)(142.0f * cosf(ui_deg_to_rad(315.0f))),
                              180 + (int)(142.0f * sinf(ui_deg_to_rad(315.0f)))));
    CHECK(cube3d_is_zoom_zone(p, 180 + (int)(142.0f * cosf(ui_deg_to_rad(45.0f))),
                              180 + (int)(142.0f * sinf(ui_deg_to_rad(45.0f)))));
    /* 圆心、屏幕外、半径偏差太大都不命中 */
    CHECK_FALSE(cube3d_is_zoom_zone(p, 180, 180));
    CHECK_FALSE(cube3d_is_zoom_zone(p, 0, 0));
    CHECK_FALSE(cube3d_is_zoom_zone(p, 180 + 200, 180));
    /* 正左方（180°）不在弧范围内 */
    CHECK_FALSE(cube3d_is_zoom_zone(p, 180 - 142, 180));
    CHECK_FALSE(cube3d_is_zoom_zone(NULL, 180, 180));
}

MT_TEST(test_cube3d_scale_from_point)
{
    const cube3d_params_t *p = &k_cube3d_default_params;

    /* 弧起点/终点/中点分别对应最小/默认/最大缩放 */
    CHECK_NEAR(cube3d_scale_from_point(p, 180 + (int)(142.0f * cosf(ui_deg_to_rad(315.0f))),
                                       180 + (int)(142.0f * sinf(ui_deg_to_rad(315.0f)))),
               p->min_scale, 0.5f);
    CHECK_NEAR(cube3d_scale_from_point(p, 180 + 142, 180), 95.0f, 0.5f);
    CHECK_NEAR(cube3d_scale_from_point(p, 180 + (int)(142.0f * cosf(ui_deg_to_rad(45.0f))),
                                       180 + (int)(142.0f * sinf(ui_deg_to_rad(45.0f)))),
               p->max_scale, 0.5f);

    /* 弧范围外的角度会被夹到端点 */
    CHECK(cube3d_scale_from_point(p, 180 - 142, 180) >= p->min_scale);
    CHECK(cube3d_scale_from_point(p, 180 - 142, 180) <= p->max_scale);
    CHECK_NEAR(cube3d_scale_from_point(NULL, 0, 0), 0.0f, 1e-6);
}

MT_TEST(test_cube3d_zoom_percent_and_ratio)
{
    const cube3d_params_t *p = &k_cube3d_default_params;

    CHECK_EQ(cube3d_zoom_percent(p, p->min_scale), CUBE3D_ZOOM_MIN_PCT);
    CHECK_EQ(cube3d_zoom_percent(p, p->max_scale), CUBE3D_ZOOM_MAX_PCT);
    CHECK_EQ(cube3d_zoom_percent(p, 95.0f), 50);
    CHECK_EQ(cube3d_zoom_percent(NULL, 95.0f), CUBE3D_ZOOM_MIN_PCT);
    CHECK_EQ(cube3d_zoom_percent(p, 60.0f), 9);

    CHECK_NEAR(cube3d_zoom_ratio(p, 95.0f), 0.5f, 1e-6);
    CHECK_NEAR(cube3d_zoom_ratio(NULL, 95.0f), 0.0f, 1e-6);
    CHECK_NEAR(cube3d_zoom_percent_f(p, 95.0f), 0.5f, 1e-6);
    CHECK_NEAR(cube3d_zoom_percent_f(p, 0.0f), 0.0f, 1e-6);   /* 夹紧 */
    CHECK_NEAR(cube3d_zoom_percent_f(p, 999.0f), 1.0f, 1e-6); /* 夹紧 */
    CHECK_NEAR(cube3d_zoom_percent_f(NULL, 5.0f), 0.0f, 1e-6);

    /* 退化参数（max == min）不能出现除零 */
    cube3d_params_t degenerate = k_cube3d_default_params;
    degenerate.max_scale = degenerate.min_scale;
    CHECK_EQ(cube3d_zoom_percent(&degenerate, 52.0f), CUBE3D_ZOOM_MIN_PCT);
    CHECK_NEAR(cube3d_zoom_ratio(&degenerate, 52.0f), 0.0f, 1e-6);
    CHECK_NEAR(cube3d_zoom_percent_f(&degenerate, 52.0f), 0.0f, 1e-6);
    CHECK_NEAR(cube3d_default_scale(&degenerate), 52.0f, 1e-6);
    CHECK_NEAR(cube3d_default_scale(NULL), 0.0f, 1e-6);
}

MT_TEST(test_cube3d_scale_clamped)
{
    const cube3d_params_t *p = &k_cube3d_default_params;

    CHECK_NEAR(cube3d_scale_clamped(p, 10.0f), p->min_scale, 1e-6);
    CHECK_NEAR(cube3d_scale_clamped(p, 9999.0f), p->max_scale, 1e-6);
    CHECK_NEAR(cube3d_scale_clamped(p, 100.0f), 100.0f, 1e-6);
    CHECK_NEAR(cube3d_scale_clamped(NULL, 42.0f), 42.0f, 1e-6);
}

MT_TEST(test_cube3d_drag_and_zoom_apply)
{
    cube3d_state_t s = k_cube3d_default_state;
    const cube3d_params_t *p = &k_cube3d_default_params;

    cube3d_apply_rotate(&s, p, 20, -10);
    CHECK_NEAR(s.vel_x, 10.0f * p->rot_gain, 1e-6);
    CHECK_NEAR(s.vel_y, 20.0f * p->rot_gain, 1e-6);
    CHECK(s.rot_x > k_cube3d_default_state.rot_x);
    CHECK(s.rot_y > k_cube3d_default_state.rot_y);

    cube3d_apply_zoom(&s, p, 180 + 142, 180);
    CHECK_NEAR(s.scale, 95.0f, 0.6f);

    cube3d_stop_motion(&s);
    CHECK_NEAR(s.vel_x, 0.0f, 1e-9);
    CHECK_NEAR(s.vel_y, 0.0f, 1e-9);
    CHECK_NEAR(s.zoom_vel, 0.0f, 1e-9);

    cube3d_apply_rotate(NULL, p, 1, 1);
    cube3d_apply_rotate(&s, NULL, 1, 1);
    cube3d_apply_zoom(NULL, p, 1, 1);
    cube3d_apply_zoom(&s, NULL, 1, 1);
    cube3d_stop_motion(NULL);
}

MT_TEST(test_cube3d_inertia_decays_to_zero)
{
    cube3d_state_t s = k_cube3d_default_state;
    const cube3d_params_t *p = &k_cube3d_default_params;

    s.vel_x = 0.5f;
    s.vel_y = -0.5f;
    s.zoom_vel = 5.0f;
    s.scale = p->max_scale - 1.0f;

    for (int i = 0; i < 200; i++) cube3d_tick_inertia(&s, p, false);

    CHECK_NEAR(s.vel_x, 0.0f, 1e-9);
    CHECK_NEAR(s.vel_y, 0.0f, 1e-9);
    CHECK_NEAR(s.zoom_vel, 0.0f, 1e-9);
    CHECK_NEAR(s.scale, p->max_scale, 1e-3); /* 上界夹紧 */

    /* 拖拽中不衰减、不自走 */
    cube3d_state_t held = k_cube3d_default_state;
    held.vel_x = 0.5f;
    const float rot_before = held.rot_x;
    cube3d_tick_inertia(&held, p, true);
    CHECK_NEAR(held.rot_x, rot_before, 1e-9);
    CHECK_NEAR(held.vel_x, 0.5f, 1e-9);

    cube3d_tick_inertia(NULL, p, false);
    cube3d_tick_inertia(&s, NULL, false);
}

/* 旋转角必须留在 ±π 内，否则浮点精度会随运行时间劣化 */
MT_TEST(test_cube3d_angle_stays_wrapped)
{
    cube3d_state_t s = k_cube3d_default_state;
    const cube3d_params_t *p = &k_cube3d_default_params;

    s.vel_x = 1.0f;
    s.vel_y = 1.0f;
    for (int i = 0; i < 5000; i++) cube3d_tick_inertia(&s, p, false);

    CHECK(s.rot_x >= -UI_PI_F - 1e-3f && s.rot_x <= UI_PI_F + 1e-3f);
    CHECK(s.rot_y >= -UI_PI_F - 1e-3f && s.rot_y <= UI_PI_F + 1e-3f);
}

MT_TEST(test_cube3d_nav_swipe_thresholds)
{
    const cube3d_params_t *p = &k_cube3d_default_params;

    CHECK(cube3d_is_nav_swipe(p, -60, 0));
    CHECK(cube3d_is_nav_swipe(p, 60, 30));
    CHECK_FALSE(cube3d_is_nav_swipe(p, 53, 0));   /* 水平距离不足 */
    CHECK_FALSE(cube3d_is_nav_swipe(p, 60, 31));  /* 垂直抖动过大 */
    CHECK_FALSE(cube3d_is_nav_swipe(NULL, 100, 0));
}

MT_TEST(test_cube3d_reset)
{
    cube3d_state_t s = k_cube3d_default_state;
    const cube3d_params_t *p = &k_cube3d_default_params;

    s.rot_x = 2.0f;
    s.scale = p->min_scale;
    s.vel_y = 3.0f;
    cube3d_reset(&s, p);

    CHECK_NEAR(s.rot_x, -0.48f, 1e-6);
    CHECK_NEAR(s.rot_y, 0.72f, 1e-6);
    CHECK_NEAR(s.scale, 95.0f, 1e-6);
    CHECK_NEAR(s.vel_x, 0.0f, 1e-9);
    CHECK_NEAR(s.zoom_vel, 0.0f, 1e-9);

    cube3d_reset(NULL, p);
    cube3d_reset(&s, NULL);
}
