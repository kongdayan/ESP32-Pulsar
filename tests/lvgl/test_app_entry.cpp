/*
 * main.cpp 的启动装配回归：在主机上跑一遍 setup()/loop()，
 * 验证"初始化顺序 + 每帧心跳"没有被打散（这两件事以前只存在于真机上）。
 */
#include "minitest.h"

#include "app_config.h"
#include "host_stubs.h"
#include "lv_host.h"
#include "ui.h"

extern void setup();
extern void loop();

/* lv_init() 只能跑一次：重复初始化会让已建屏幕变成悬空指针 */
static void ensure_host(void)
{
    static bool inited;
    if (!inited) {
        lv_host_init();
        inited = true;
    }
}

MT_TEST(test_setup_wires_boot_in_legacy_order)
{
    ensure_host();
    host_display_reset();
    host_sd_card_reset();
    host_arduino_reset();

    /* 开机首屏若是懒加载遗留的，先清掉，保证 ui_init 真的建了屏 */
    lv_obj_t **first = screen_codex_usage_get_ptr();
    if (*first != NULL) {
        lv_obj_del(*first);
        *first = NULL;
    }

    setup();

    CHECK(host_arduino_delay_total_ms() >= 1u);      /* 开机延时 */
    CHECK_EQ(host_arduino_serial_baud(), APP_SERIAL_BAUD);
    CHECK_EQ(host_display_init_calls(), 1);
    CHECK_EQ(host_sd_card_init_calls(), 1);
    CHECK(*first != NULL);
    CHECK_EQ(lv_host_active_screen(), *first);
}

MT_TEST(test_loop_beats_lvgl_and_display_together)
{
    ensure_host();
    host_display_reset();
    setup();

    /* loop() 每帧都必须喂 LVGL 并检查息屏逻辑 */
    const int ticks_before = host_display_power_tick_calls();
    loop();
    loop();
    CHECK(host_display_power_tick_calls() >= ticks_before + 2);
}
