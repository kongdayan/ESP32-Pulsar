/*
 * 真机渲染回归：用主机侧 LVGL（360x360 虚拟帧缓冲）把每一屏完整画一遍，
 * 断言"不是黑屏"。这是"外观不变"最接近实机验证的自动化检查。
 */
#include "minitest.h"

#include <stddef.h>
#include <string.h>

#include "lv_host.h"
#include "ui.h"

typedef struct {
    const char *name;
    lv_obj_t **(*get_ptr)(void);
    void (*init)(void);
} screen_entry_t;

static const screen_entry_t k_screens[] = {
    { "dashboard",   screen_dashboard_get_ptr,   screen_dashboard_init   },
    { "info",        screen_info_get_ptr,        screen_info_init        },
    { "image",       screen_image_get_ptr,       screen_image_init       },
    { "video",       screen_video_get_ptr,       screen_video_init       },
    { "about",       screen_about_get_ptr,       screen_about_init       },
    { "agent",       screen_agent_get_ptr,       screen_agent_init       },
    { "3dmodel",     screen_3dmodel_get_ptr,     screen_3dmodel_init     },
    { "codex_usage", screen_codex_usage_get_ptr, screen_codex_usage_init },
    { "balance",     screen_balance_get_ptr,     screen_balance_init     },
};

#define SCREEN_COUNT ((int)(sizeof(k_screens) / sizeof(k_screens[0])))

/* 每个屏幕至少要有这么多可见像素/颜色，低于它说明画空了 */
#define MIN_VISIBLE_PX 200u
#define MIN_COLORS       2u

static void ensure_host(void)
{
    static bool inited;
    if (!inited) {
        lv_host_init();
        inited = true;
    }
}

/* 常驻中转屏：删除当前活动屏会让 lv_disp 的 act_scr 悬空 */
static lv_obj_t *holding_screen(void)
{
    static lv_obj_t *scr;
    if (scr == NULL) {
        scr = lv_obj_create(NULL);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    }
    return scr;
}

static void destroy_screen(lv_obj_t **slot)
{
    if (slot == NULL || *slot == NULL) return;
    lv_scr_load(holding_screen());
    lv_obj_del(*slot);
    *slot = NULL;   /* 恢复懒加载状态，下一个用例从零开始 */
}

MT_TEST(test_lvgl_host_environment_works)
{
    ensure_host();
    CHECK(lv_disp_get_default() != NULL);
    CHECK_EQ(lv_disp_get_hor_res(NULL), LV_HOST_SCREEN_PX);
    CHECK_EQ(lv_disp_get_ver_res(NULL), LV_HOST_SCREEN_PX);
}

MT_TEST(test_every_screen_builds_a_real_object)
{
    ensure_host();

    for (int i = 0; i < SCREEN_COUNT; i++) {
        lv_obj_t **slot = k_screens[i].get_ptr();
        CHECK(slot != NULL);
        if (slot == NULL) continue;
        CHECK(*slot == NULL);          /* 懒加载：init 之前必须是 NULL */

        k_screens[i].init();
        lv_obj_t *scr = *slot;
        CHECK(scr != NULL);
        if (scr == NULL) continue;

        CHECK(lv_obj_is_valid(scr));
        CHECK_EQ(lv_obj_get_width(scr), LV_HOST_SCREEN_PX);
        CHECK_EQ(lv_obj_get_height(scr), LV_HOST_SCREEN_PX);
        CHECK((int)lv_obj_get_child_cnt(scr) > 0);

        destroy_screen(slot);
    }
}

MT_TEST(test_every_screen_renders_visible_content)
{
    ensure_host();

    for (int i = 0; i < SCREEN_COUNT; i++) {
        k_screens[i].init();
        lv_obj_t **slot = k_screens[i].get_ptr();
        if (*slot == NULL) {
            mt_report_failure(__FILE__, __LINE__, k_screens[i].name);
            continue;
        }

        lv_host_reset_flush_count();
        lv_scr_load(*slot);
        for (int round = 0; round < 2; round++) {
            lv_host_advance_ms(LV_DISP_DEF_REFR_PERIOD + 1u);
            lv_host_run_timers();
        }

        CHECK(lv_host_flush_count() > 0u);
        CHECK(lv_host_non_black_pixels() >= MIN_VISIBLE_PX);
        CHECK(lv_host_distinct_colors() >= MIN_COLORS);

        destroy_screen(slot);
    }
}

/* 反复进入/退出不能耗尽 LVGL 内存池（48KB），也不能泄漏对象 */
MT_TEST(test_screen_reinit_does_not_leak_mem)
{
    ensure_host();

    lv_mem_monitor_t m_before;
    lv_mem_monitor(&m_before);

    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < SCREEN_COUNT; i++) {
            k_screens[i].init();
            lv_obj_t **slot = k_screens[i].get_ptr();
            if (*slot != NULL) {
                lv_scr_load(*slot);
                lv_host_advance_ms(LV_DISP_DEF_REFR_PERIOD + 1u);
                lv_host_run_timers();
            }
            destroy_screen(slot);
        }
    }

    lv_mem_monitor_t m_after;
    lv_mem_monitor(&m_after);
    /* 五轮建屏/删屏之后，可用堆应当回到起点附近（允许 2KB 抖动） */
    CHECK((long)m_after.free_size >= (long)m_before.free_size - 2048);
    CHECK(m_after.frag_pct < 60u);
}
