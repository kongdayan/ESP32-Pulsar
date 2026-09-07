#include "minitest.h"

#include <math.h>

#include "ui_math.h"

MT_TEST(test_clampf)
{
    CHECK_NEAR(ui_clampf(0.5f, 0.0f, 1.0f), 0.5f, 1e-6);
    CHECK_NEAR(ui_clampf(-3.0f, 0.0f, 1.0f), 0.0f, 1e-6);
    CHECK_NEAR(ui_clampf(42.0f, 0.0f, 1.0f), 1.0f, 1e-6);
    CHECK_NEAR(ui_clampf(7.0f, 10.0f, 5.0f), 10.0f, 1e-6); /* lo > hi 时以 lo 为准 */
}

MT_TEST(test_lerp_and_zero)
{
    CHECK_NEAR(ui_lerpf(0.0f, 10.0f, 0.25f), 2.5f, 1e-6);
    CHECK_NEAR(ui_lerpf(2.0f, 2.0f, 0.9f), 2.0f, 1e-6);
    CHECK(ui_is_zero_f(1e-12f, UI_EPS_F));
    CHECK_FALSE(ui_is_zero_f(1.0f, UI_EPS_F));
}

MT_TEST(test_deg_rad_conversion)
{
    CHECK_NEAR(ui_deg_to_rad(180.0f), UI_PI_F, 1e-5);
    CHECK_NEAR(ui_rad_to_deg(UI_PI_F), 180.0f, 1e-3);
    CHECK_NEAR(ui_deg_to_rad(0.0f), 0.0f, 1e-9);
    CHECK_NEAR(ui_rad_to_deg(ui_deg_to_rad(37.0f)), 37.0f, 1e-4);
}

MT_TEST(test_normalize_deg)
{
    CHECK_NEAR(ui_normalize_deg(-45.0f), 315.0f, 1e-5);
    CHECK_NEAR(ui_normalize_deg(0.0f), 0.0f, 1e-5);
    CHECK_NEAR(ui_normalize_deg(359.0f), 359.0f, 1e-5);
    /* 与旧版 is_zoom_zone 里的两段式修正保持一致 */
    CHECK_NEAR(ui_normalize_deg(-45.0f) + UI_DEG_FULL, 675.0f, 1e-5);
}

MT_TEST(test_wrap_rad)
{
    CHECK_NEAR(ui_wrap_rad(0.0f), 0.0f, 1e-6);
    CHECK_NEAR(ui_wrap_rad(UI_PI_F - 0.1f), UI_PI_F - 0.1f, 1e-6);
    CHECK_NEAR(ui_wrap_rad(UI_PI_F + 0.1f), -UI_PI_F + 0.1f, 1e-6);
    CHECK_NEAR(ui_wrap_rad(-UI_PI_F - 0.1f), UI_PI_F - 0.1f, 1e-6);
    CHECK(fabsf(ui_wrap_rad(UI_TWO_PI_F)) <= UI_PI_F);
}

MT_TEST(test_angle_delta)
{
    /* 跨越 ±π 时要走最短路径 */
    CHECK_NEAR(ui_angle_delta_rad(3.0f, -3.0f), 0.28318534f, 1e-5);
    CHECK_NEAR(ui_angle_delta_rad(0.0f, 1.0f), 1.0f, 1e-6);
    CHECK_NEAR(ui_angle_delta_rad(1.0f, 0.0f), -1.0f, 1e-6);
}

MT_TEST(test_vector_and_angle_of)
{
    CHECK_NEAR(ui_vector_len(3.0f, 4.0f), 5.0f, 1e-6);
    CHECK_NEAR(ui_vector_len(0.0f, 0.0f), 0.0f, 1e-6);
    CHECK_NEAR(ui_angle_deg_of(1.0f, 0.0f), 0.0f, 1e-4);
    CHECK_NEAR(ui_angle_deg_of(0.0f, 1.0f), 90.0f, 1e-4);
    CHECK_NEAR(ui_angle_deg_of(-1.0f, 0.0f), 180.0f, 1e-4);
    CHECK_NEAR(ui_angle_deg_of(0.0f, -1.0f), 270.0f, 1e-4);
}

MT_TEST(test_integer_helpers)
{
    CHECK_EQ(ui_abs_i(-7), 7);
    CHECK_EQ(ui_abs_i(7), 7);
    CHECK_EQ(ui_abs_i(0), 0);
    CHECK_EQ(ui_min_i(2, 9), 2);
    CHECK_EQ(ui_max_i(2, 9), 9);
    CHECK_EQ(ui_clampi(-1, 0, 100), 0);
    CHECK_EQ(ui_clampi(1000, 0, 100), 100);
    CHECK_EQ(ui_clampi(42, 0, 100), 42);
}

MT_TEST(test_percent_mapping)
{
    CHECK_EQ(ui_percent_of(73, 100), 73);
    CHECK_EQ(ui_percent_of(1, 3), 33);      /* 四舍五入 */
    CHECK_EQ(ui_percent_of(0, 0), 0);       /* 退化 span */
    CHECK_EQ(ui_percent_of(5, -10), 0);
    CHECK_EQ(ui_steps_for_percent(25, 27), 7);   /* 25*27/100 = 6.75 → 7 */
    CHECK_EQ(ui_steps_for_percent(25, 73), 18);
    CHECK_EQ(ui_steps_for_percent(25, 0), 0);
    CHECK_EQ(ui_steps_for_percent(0, 50), 0);
}

MT_TEST(test_unit_span)
{
    CHECK_NEAR(ui_unit_span(95.0f, 52.0f, 138.0f), 0.5f, 1e-6);
    CHECK_NEAR(ui_unit_span(10.0f, 52.0f, 138.0f), 0.0f, 1e-6);
    CHECK_NEAR(ui_unit_span(999.0f, 52.0f, 138.0f), 1.0f, 1e-6);
    CHECK_NEAR(ui_unit_span(1.0f, 2.0f, 2.0f), 0.0f, 1e-6); /* 退化区间 */
}
