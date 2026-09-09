#include "minitest.h"

#include <stddef.h>

#include "nav_map.h"

MT_TEST(test_nav_names_and_validity)
{
    CHECK(nav_screen_is_valid(NAV_SCREEN_DASHBOARD));
    CHECK(nav_screen_is_valid(NAV_SCREEN_CODEX_USAGE));
    CHECK(nav_screen_is_valid(NAV_SCREEN_CLAUDE_USAGE));
    CHECK(nav_screen_is_valid(NAV_SCREEN_BALANCE));
    CHECK_FALSE(nav_screen_is_valid(NAV_SCREEN_COUNT));
    CHECK_FALSE(nav_screen_is_valid((nav_screen_id_t)-1));
    CHECK_STR_EQ(nav_screen_name(NAV_SCREEN_DASHBOARD), "dashboard");
    CHECK_STR_EQ(nav_screen_name(NAV_SCREEN_MODEL3D), "3dmodel");
    CHECK_STR_EQ(nav_screen_name(NAV_SCREEN_COUNT), "?");
}

/* 每一条链路都对应旧版某个屏幕里写死的 if/else */
MT_TEST(test_nav_ring_forward_chain)
{
    nav_step_t step;

    CHECK(nav_map_step(NAV_SCREEN_DASHBOARD, NAV_DIR_LEFT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_INFO);
    CHECK_EQ(step.anim, NAV_ANIM_MOVE);

    CHECK(nav_map_step(NAV_SCREEN_INFO, NAV_DIR_LEFT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_IMAGE);

    CHECK(nav_map_step(NAV_SCREEN_IMAGE, NAV_DIR_LEFT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_VIDEO);

    CHECK(nav_map_step(NAV_SCREEN_VIDEO, NAV_DIR_LEFT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_ABOUT);

    /* about → codex：旧版用的是无动画直接切换 */
    CHECK(nav_map_step(NAV_SCREEN_ABOUT, NAV_DIR_LEFT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_CODEX_USAGE);
    CHECK_EQ(step.anim, NAV_ANIM_NONE);
}

MT_TEST(test_nav_back_chain)
{
    nav_step_t step;

    CHECK(nav_map_step(NAV_SCREEN_INFO, NAV_DIR_RIGHT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_DASHBOARD);

    CHECK(nav_map_step(NAV_SCREEN_IMAGE, NAV_DIR_RIGHT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_INFO);

    CHECK(nav_map_step(NAV_SCREEN_VIDEO, NAV_DIR_RIGHT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_IMAGE);

    CHECK(nav_map_step(NAV_SCREEN_ABOUT, NAV_DIR_RIGHT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_VIDEO);
    CHECK_EQ(step.anim, NAV_ANIM_MOVE);

    CHECK(nav_map_step(NAV_SCREEN_DASHBOARD, NAV_DIR_RIGHT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_CODEX_USAGE);
}

/* codex：左滑回关于页（无动画），右滑进 Claude 用量屏（位移动画） */
MT_TEST(test_nav_codex_usage_links)
{
    nav_step_t left;
    nav_step_t right;

    CHECK(nav_map_step(NAV_SCREEN_CODEX_USAGE, NAV_DIR_LEFT, &left));
    CHECK(nav_map_step(NAV_SCREEN_CODEX_USAGE, NAV_DIR_RIGHT, &right));
    CHECK_EQ(left.target, NAV_SCREEN_ABOUT);
    CHECK_EQ(left.anim, NAV_ANIM_NONE);
    CHECK_EQ(right.target, NAV_SCREEN_CLAUDE_USAGE);
    CHECK_EQ(right.anim, NAV_ANIM_MOVE);
}

/* Claude 用量屏：左滑回 codex（无动画），右滑进余额屏（位移动画） */
MT_TEST(test_nav_claude_usage_links)
{
    nav_step_t left;
    nav_step_t right;

    CHECK(nav_map_step(NAV_SCREEN_CLAUDE_USAGE, NAV_DIR_LEFT, &left));
    CHECK(nav_map_step(NAV_SCREEN_CLAUDE_USAGE, NAV_DIR_RIGHT, &right));
    CHECK_EQ(left.target, NAV_SCREEN_CODEX_USAGE);
    CHECK_EQ(left.anim, NAV_ANIM_NONE);
    CHECK_EQ(right.target, NAV_SCREEN_BALANCE);
    CHECK_EQ(right.anim, NAV_ANIM_MOVE);
}

/* 余额屏：左右都无动画跳转 */
MT_TEST(test_nav_balance_links)
{
    nav_step_t left;
    nav_step_t right;

    CHECK(nav_map_step(NAV_SCREEN_BALANCE, NAV_DIR_LEFT, &left));
    CHECK(nav_map_step(NAV_SCREEN_BALANCE, NAV_DIR_RIGHT, &right));
    CHECK_EQ(left.target, NAV_SCREEN_CLAUDE_USAGE);
    CHECK_EQ(right.target, NAV_SCREEN_ABOUT);
    CHECK_EQ(left.anim, NAV_ANIM_NONE);
    CHECK_EQ(right.anim, NAV_ANIM_NONE);
}

MT_TEST(test_nav_game_screens)
{
    nav_step_t step;

    CHECK(nav_map_step(NAV_SCREEN_AGENT, NAV_DIR_LEFT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_MODEL3D);

    CHECK(nav_map_step(NAV_SCREEN_AGENT, NAV_DIR_RIGHT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_ABOUT);

    CHECK(nav_map_step(NAV_SCREEN_MODEL3D, NAV_DIR_LEFT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_CODEX_USAGE);

    CHECK(nav_map_step(NAV_SCREEN_MODEL3D, NAV_DIR_RIGHT, &step));
    CHECK_EQ(step.target, NAV_SCREEN_ABOUT);
}

MT_TEST(test_nav_rejects_bad_input)
{
    nav_step_t step;
    step.target = NAV_SCREEN_INFO;

    /* 无方向 */
    CHECK_FALSE(nav_map_step(NAV_SCREEN_DASHBOARD, NAV_DIR_NONE, &step));
    CHECK_FALSE(nav_map_step(NAV_SCREEN_DASHBOARD, (nav_dir_t)99, &step));

    /* 非法来源 */
    CHECK_FALSE(nav_map_step(NAV_SCREEN_COUNT, NAV_DIR_LEFT, &step));

    /* 出参为空 */
    CHECK_FALSE(nav_map_step(NAV_SCREEN_DASHBOARD, NAV_DIR_LEFT, NULL));

    /* 未登记的屏幕：没有链路 */
    CHECK_FALSE(nav_map_step((nav_screen_id_t)42, NAV_DIR_LEFT, &step));
}

MT_TEST(test_nav_map_has_link)
{
    CHECK(nav_map_has_link(NAV_SCREEN_DASHBOARD, NAV_DIR_LEFT));
    CHECK(nav_map_has_link(NAV_SCREEN_DASHBOARD, NAV_DIR_RIGHT));
    CHECK_FALSE(nav_map_has_link(NAV_SCREEN_DASHBOARD, NAV_DIR_NONE));
    CHECK_FALSE(nav_map_has_link(NAV_SCREEN_COUNT, NAV_DIR_LEFT));
}

/* 结构不变式：每个方向的链路都必须指向一个真实存在的屏幕 */
MT_TEST(test_nav_table_is_closed_over_screens)
{
    for (int id = 0; id < NAV_SCREEN_COUNT; id++) {
        const nav_screen_id_t from = (nav_screen_id_t)id;

        for (int d = NAV_DIR_LEFT; d <= NAV_DIR_RIGHT; d++) {
            nav_step_t step;
            if (!nav_map_step(from, (nav_dir_t)d, &step)) continue;

            CHECK(nav_screen_is_valid(step.target));
            CHECK_NE(step.target, from);
        }
    }
}
