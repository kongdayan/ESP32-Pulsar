#include "minitest.h"

#include <stddef.h>

#include "app_config.h"
#include "power_mgmt.h"

#define T0        1000u
#define TIMEOUT   30000u

MT_TEST(test_power_init_starts_screen_on)
{
    power_state_t st;
    power_state_init(&st, T0, TIMEOUT);

    CHECK(power_state_is_on(&st));
    CHECK_EQ(st.idle_timeout_ms, TIMEOUT);
    CHECK_EQ(st.last_activity_ms, T0);
    CHECK_EQ(power_idle_remaining_ms(&st, T0), TIMEOUT);
}

MT_TEST(test_power_ticks_off_after_timeout)
{
    power_state_t st;
    power_state_init(&st, T0, TIMEOUT);

    CHECK_FALSE(power_tick(&st, T0 + TIMEOUT - 1u));
    CHECK(power_state_is_on(&st));

    /* 到期：请求熄屏，且只请求一次 */
    CHECK(power_tick(&st, T0 + TIMEOUT));
    CHECK_FALSE(power_state_is_on(&st));
    CHECK_FALSE(power_tick(&st, T0 + 10u * TIMEOUT));
}

MT_TEST(test_power_touch_defers_sleep)
{
    power_state_t st;
    power_state_init(&st, T0, TIMEOUT);

    CHECK_FALSE(power_tick(&st, T0 + 20000u));
    power_notify_touch(&st, T0 + 20000u);

    /* 触摸后重新计时 */
    CHECK_FALSE(power_tick(&st, T0 + 29999u));
    CHECK(power_tick(&st, T0 + 50000u));
}

MT_TEST(test_power_touch_wakes_screen)
{
    power_state_t st;
    power_state_init(&st, T0, TIMEOUT);

    CHECK(power_tick(&st, T0 + TIMEOUT)); /* 熄屏 */
    CHECK(power_notify_touch(&st, T0 + 40000u)); /* 需要点亮背光 */
    CHECK(power_state_is_on(&st));

    /* 已点亮时的触摸只刷新时间戳 */
    CHECK_FALSE(power_notify_touch(&st, T0 + 41000u));
}

MT_TEST(test_power_remaining_counts_down)
{
    power_state_t st;
    power_state_init(&st, T0, TIMEOUT);

    CHECK_EQ(power_idle_remaining_ms(&st, T0 + 10000u), TIMEOUT - 10000u);
    CHECK_EQ(power_idle_remaining_ms(&st, T0 + TIMEOUT), 0u);
    CHECK_EQ(power_idle_remaining_ms(&st, T0 + TIMEOUT + 999u), 0u);

    power_tick(&st, T0 + TIMEOUT);
    CHECK_EQ(power_idle_remaining_ms(&st, T0 + TIMEOUT), 0u); /* 已熄屏 */
}

MT_TEST(test_power_set_screen_on_syncs_state)
{
    power_state_t st;
    power_state_init(&st, T0, TIMEOUT);

    /* 状态未变化 → 不重复操作硬件 */
    CHECK_FALSE(power_set_screen_on(&st, true, T0 + 5u));

    CHECK(power_set_screen_on(&st, false, T0 + 100u));
    CHECK_FALSE(power_state_is_on(&st));
    CHECK_FALSE(power_set_screen_on(&st, false, T0 + 200u));

    /* 重新点亮会重置活跃时间 */
    CHECK(power_set_screen_on(&st, true, T0 + 300u));
    CHECK_EQ(st.last_activity_ms, T0 + 300u);
    CHECK_EQ(power_idle_remaining_ms(&st, T0 + 300u), TIMEOUT);
}

/* millis() 约 49.7 天回绕一次，倒计时不能被打断 */
MT_TEST(test_power_survives_millis_wraparound)
{
    power_state_t st;
    const uint32_t near_max = 0xFFFFFFFFu - 1000u;
    power_state_init(&st, near_max, TIMEOUT);

    /* 跨过 0 之后仍未超时 */
    CHECK_FALSE(power_tick(&st, near_max + 1500u));
    CHECK(power_state_is_on(&st));

    /* 累计超过 TIMEOUT 后熄屏 */
    CHECK(power_tick(&st, near_max + TIMEOUT));
    CHECK_FALSE(power_state_is_on(&st));
}

MT_TEST(test_power_timeout_zero_sleeps_immediately)
{
    power_state_t st;
    power_state_init(&st, T0, 0u);

    CHECK(power_tick(&st, T0));
    CHECK_FALSE(power_state_is_on(&st));
}

/* 空指针必须安全（供硬件层误调用时不崩） */
MT_TEST(test_power_null_safety)
{
    power_state_t st;
    power_state_init(&st, T0, TIMEOUT);

    power_state_init(NULL, T0, TIMEOUT);
    CHECK_FALSE(power_tick(NULL, T0));
    CHECK_FALSE(power_notify_touch(NULL, T0));
    CHECK_FALSE(power_set_screen_on(NULL, true, T0));
    CHECK_FALSE(power_state_is_on(NULL));
    CHECK_EQ(power_idle_remaining_ms(NULL, T0), 0u);
}

/* 生产配置：30 秒息屏，与 hal/display.cpp 使用同一常量 */
MT_TEST(test_power_default_timeout_config)
{
    CHECK_EQ(APP_SCREEN_IDLE_TIMEOUT_MS, 30u * 1000u);

    power_state_t st;
    power_state_init(&st, 0u, APP_SCREEN_IDLE_TIMEOUT_MS);
    CHECK_FALSE(power_tick(&st, APP_SCREEN_IDLE_TIMEOUT_MS - 1u));
    CHECK(power_tick(&st, APP_SCREEN_IDLE_TIMEOUT_MS));
}
