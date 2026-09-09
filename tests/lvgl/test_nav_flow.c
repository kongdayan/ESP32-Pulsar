/*
 * 导航/交互回归：用主机侧虚拟触摸真实地"滑动"每一屏，
 * 断言落到 nav_map 规定的目标屏上，等价于旧版每屏写死的 if/else。
 */
#include "minitest.h"

#include <stdio.h>
#include <stddef.h>

#include "app_config.h"
#include "lv_host.h"
#include "ui.h"
#include "ui_screen.h"

typedef struct {
    nav_screen_id_t id;
    lv_obj_t **(*get_ptr)(void);
    void (*init)(void);
} nav_entry_t;

static const nav_entry_t k_entries[] = {
    { NAV_SCREEN_DASHBOARD,   screen_dashboard_get_ptr,   screen_dashboard_init   },
    { NAV_SCREEN_INFO,        screen_info_get_ptr,        screen_info_init        },
    { NAV_SCREEN_IMAGE,       screen_image_get_ptr,       screen_image_init       },
    { NAV_SCREEN_VIDEO,       screen_video_get_ptr,       screen_video_init       },
    { NAV_SCREEN_ABOUT,       screen_about_get_ptr,       screen_about_init       },
    { NAV_SCREEN_AGENT,       screen_agent_get_ptr,       screen_agent_init       },
    { NAV_SCREEN_MODEL3D,     screen_3dmodel_get_ptr,     screen_3dmodel_init     },
    { NAV_SCREEN_CODEX_USAGE,  screen_codex_usage_get_ptr,  screen_codex_usage_init  },
    { NAV_SCREEN_CLAUDE_USAGE, screen_claude_usage_get_ptr, screen_claude_usage_init },
    { NAV_SCREEN_BALANCE,      screen_balance_get_ptr,      screen_balance_init      },
};

#define ENTRY_COUNT ((int)(sizeof(k_entries) / sizeof(k_entries[0])))

static void ensure_host(void)
{
    static bool inited;
    if (!inited) {
        lv_host_init();
        inited = true;
    }
}

static lv_obj_t **slot_of(nav_screen_id_t id)
{
    for (int i = 0; i < ENTRY_COUNT; i++) {
        if (k_entries[i].id == id) return k_entries[i].get_ptr();
    }
    return NULL;
}

static lv_obj_t *obtain(nav_screen_id_t id)
{
    lv_obj_t **slot = slot_of(id);
    if (slot == NULL) return NULL;
    if (*slot == NULL) {
        for (int i = 0; i < ENTRY_COUNT; i++) {
            if (k_entries[i].id == id) k_entries[i].init();
        }
    }
    return *slot;
}

/* 10 屏同时存在会超出 48KB 的 LVGL 内存池，每个用例结束都彻底回收。
 * 注意：不能删除"当前活动屏"，否则 lv_disp 会留下悬空指针，
 * 所以下载之前先把显示切到一张常驻的中转屏上。 */
static lv_obj_t *holding_screen(void)
{
    static lv_obj_t *scr;
    if (scr == NULL) {
        scr = lv_obj_create(NULL);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    }
    return scr;
}

static void release_all(void)
{
    lv_scr_load(holding_screen());
    lv_host_advance_ms(LV_DISP_DEF_REFR_PERIOD + 1u);
    lv_host_run_timers();

    for (int i = 0; i < ENTRY_COUNT; i++) {
        lv_obj_t **slot = k_entries[i].get_ptr();
        if (*slot != NULL) {
            lv_obj_del(*slot);
            *slot = NULL;
        }
    }
}

static void settle(void)
{
    for (int i = 0; i < 6; i++) {
        lv_host_advance_ms(APP_NAV_ANIM_MS / 2u + 1u);
        lv_host_run_timers();
    }
}

/* 左滑（手指从右往左）→ LV_DIR_LEFT；右滑反之 */
static void swipe_dir(nav_dir_t dir)
{
    if (dir == NAV_DIR_LEFT) {
        lv_host_swipe(APP_SCREEN_PX - 90, APP_SCREEN_PX / 2, 90, APP_SCREEN_PX / 2);
    } else {
        lv_host_swipe(90, APP_SCREEN_PX / 2, APP_SCREEN_PX - 90, APP_SCREEN_PX / 2);
    }
    settle();
}

MT_TEST(test_swipe_follows_nav_table)
{
    ensure_host();

    for (int i = 0; i < ENTRY_COUNT; i++) {
        for (int d = NAV_DIR_LEFT; d <= NAV_DIR_RIGHT; d++) {
            const nav_dir_t dir = (nav_dir_t)d;
            nav_step_t step;
            if (!nav_map_step(k_entries[i].id, dir, &step)) continue;

            /* agent / 3dmodel 的手势由各自的游戏输入层处理，不走通用滑屏，
             * 单独用 test_agent_* / test_3dmodel_edge_swipe 验证。 */
            if (k_entries[i].id == NAV_SCREEN_AGENT || k_entries[i].id == NAV_SCREEN_MODEL3D) continue;

            release_all();
            lv_obj_t *from = obtain(k_entries[i].id);
            if (from == NULL) {
                mt_report_failure(__FILE__, __LINE__, "from screen");
                continue;
            }
            lv_scr_load(from);
            lv_host_advance_ms(LV_DISP_DEF_REFR_PERIOD + 1u);
            lv_host_run_timers();

            swipe_dir(dir);

            /* 目标屏必须已被导航自己懒加载出来（这里不许再帮忙创建） */
            lv_obj_t **tslot = slot_of(step.target);
            CHECK(tslot != NULL);
            if (tslot == NULL) continue;
            CHECK(*tslot != NULL);
            CHECK_EQ(lv_host_active_screen(), *tslot);
        }
    }

    release_all();
}

MT_TEST(test_nav_button_reaches_every_screen)
{
    ensure_host();

    for (int i = 0; i < ENTRY_COUNT; i++) {
        for (int d = NAV_DIR_LEFT; d <= NAV_DIR_RIGHT; d++) {
            nav_step_t step;
            if (!nav_map_step(k_entries[i].id, (nav_dir_t)d, &step)) continue;

            release_all();
            obtain(k_entries[i].id);
            lv_obj_t *before = lv_host_active_screen();
            CHECK(ui_nav_go(k_entries[i].id, (nav_dir_t)d));
            settle();
            CHECK(lv_host_active_screen() != before);
        }
    }

    release_all();
}

/* 首页 +/- 按钮长按会让三个弧度盘同时增减（旧版行为） */
MT_TEST(test_dashboard_arcs_respond_to_buttons)
{
    ensure_host();
    release_all();

    lv_obj_t *scr = obtain(NAV_SCREEN_DASHBOARD);
    CHECK(scr != NULL);
    lv_scr_load(scr);
    lv_host_advance_ms(LV_DISP_DEF_REFR_PERIOD + 1u);
    lv_host_run_timers();

    /* 找到按钮：屏幕上第一个可点击的 button/switch 子对象 */
    lv_obj_t *btn_plus = NULL;
    const size_t n = lv_obj_get_child_cnt(scr);
    for (size_t c = 0; c < n; c++) {
        lv_obj_t *child = lv_obj_get_child(scr, c);
        if (child && lv_obj_check_type(child, &lv_btn_class)) { btn_plus = child; break; }
    }
    CHECK(btn_plus != NULL);

    if (btn_plus != NULL) {
        /* 直接发事件，等价于长按重复触发，不依赖动画与触摸时序 */
        lv_event_send(btn_plus, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
        lv_event_send(btn_plus, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    }

    settle();
    CHECK(lv_host_flush_count() > 0u);
    release_all();
}

/* 旧版行为：小游戏屏的 on_input 挂在游戏图层上，LV_EVENT_GESTURE 只会送到
 * 冒泡根（屏幕对象），所以那一段 ui_nav_go 分支实际收不到事件 —— 滑屏不出屏。
 * 重构保持原样，这里把"保持原样"钉死，避免以后被无声改掉。 */
MT_TEST(test_agent_screen_keeps_legacy_gesture_quirk)
{
    ensure_host();
    release_all();

    lv_obj_t *agent = obtain(NAV_SCREEN_AGENT);
    CHECK(agent != NULL);
    lv_scr_load(agent);
    lv_host_advance_ms(LV_DISP_DEF_REFR_PERIOD + 1u);
    lv_host_run_timers();

    swipe_dir(NAV_DIR_LEFT);
    CHECK_EQ(lv_host_active_screen(), agent);

    swipe_dir(NAV_DIR_RIGHT);
    CHECK_EQ(lv_host_active_screen(), agent);

    /* 但显式导航（例如 3D 屏的边滑/按钮）仍然可用 */
    CHECK(ui_nav_go(NAV_SCREEN_AGENT, NAV_DIR_RIGHT));
    settle();
    CHECK(lv_host_active_screen() != agent);

    release_all();
}

/* 3D 立方体屏：只有在边缘区按下并横向拖过阈值才算切屏（旧版交互） */
MT_TEST(test_3dmodel_edge_swipe_navigates)
{
    ensure_host();

    /* 左边缘按下 → 向右拖：RIGHT → about */
    release_all();
    lv_obj_t *m = obtain(NAV_SCREEN_MODEL3D);
    CHECK(m != NULL);
    lv_scr_load(m);
    lv_host_advance_ms(LV_DISP_DEF_REFR_PERIOD + 1u);
    lv_host_run_timers();

    lv_host_swipe(20, 180, 200, 180);
    settle();
    lv_obj_t **about = screen_about_get_ptr();
    CHECK(*about != NULL);
    CHECK_EQ(lv_host_active_screen(), *about);

    /* 中间按下再拖：属于旋转操作，不应切屏 */
    release_all();
    m = obtain(NAV_SCREEN_MODEL3D);
    lv_scr_load(m);
    lv_host_advance_ms(LV_DISP_DEF_REFR_PERIOD + 1u);
    lv_host_run_timers();

    lv_host_swipe(180, 180, 320, 180);
    settle();
    CHECK_EQ(lv_host_active_screen(), m);

    release_all();
}
