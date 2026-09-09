/*
 * 屏幕交互回归：真实触摸事件 + 真实 TF 卡文件（build/sdcard/video.rgb），
 * 覆盖各屏除"纯绘制"以外的输入处理与定时器链路。
 *
 * 文件名以 test_a_ 开头是有意为之：视频帧缓冲是 screen_video.c 的静态变量，
 * "分配失败/回落"两条分支只有在进程内第一次建视频屏时才走得到，
 * 所以本文件必须先于其他会创建视频屏的用例运行。
 */
#include "minitest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app_config.h"
#include "codex_usage_layout.h"
#include "esp_heap_caps.h"
#include "lv_host.h"
#include "sd_card.h"
#include "ui.h"
#include "ui_screen.h"
#include "usage_model.h"
#include "video_layout.h"
#include "video_source.h"

static void ensure_host(void)
{
    static bool inited;
    if (!inited) {
        lv_host_init();
        inited = true;
    }
}

static lv_obj_t *s_holding;

static void release_slot(lv_obj_t **slot)
{
    if (s_holding == NULL) {
        s_holding = lv_obj_create(NULL);
        lv_obj_clear_flag(s_holding, LV_OBJ_FLAG_SCROLLABLE);
    }
    if (*slot != NULL) {
        lv_scr_load(s_holding);
        lv_obj_del(*slot);
        *slot = NULL;
    }
}

/* 递归找第一个指定类型的子对象 */
static lv_obj_t *find_child(lv_obj_t *parent, const lv_obj_class_t *cls)
{
    if (parent == NULL) return NULL;

    const size_t n = lv_obj_get_child_cnt(parent);
    for (size_t i = 0; i < n; i++) {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (child == NULL) continue;
        if (lv_obj_check_type(child, cls)) return child;
        lv_obj_t *deep = find_child(child, cls);
        if (deep != NULL) return deep;
    }
    return NULL;
}

/* 把触摸状态彻底归零：清掉 wait_until_release / act_obj 等残留 */
static void settle_input(void)
{
    lv_host_touch_up();
    for (int i = 0; i < 4; i++) {
        lv_host_advance_ms(LV_INDEV_DEF_READ_PERIOD + 1u);
        lv_host_run_timers();
    }
}

static void render_a_bit(void)
{
    lv_host_advance_ms(LV_DISP_DEF_REFR_PERIOD + 1u);
    lv_host_run_timers();
}

/* ── 首页 +/- 按钮 ────────────────────────────────────────────────────────── */

MT_TEST(test_dashboard_plus_minus_roundtrip)
{
    ensure_host();

    screen_dashboard_init();
    lv_obj_t **slot = screen_dashboard_get_ptr();
    CHECK(*slot != NULL);
    if (*slot == NULL) return;

    lv_obj_t *scr = *slot;
    lv_scr_load(scr);
    render_a_bit();

    lv_obj_t *arc = find_child(scr, &lv_arc_class);
    lv_obj_t *btn = find_child(scr, &lv_btn_class);
    CHECK(arc != NULL);
    CHECK(btn != NULL);

    if (arc != NULL && btn != NULL) {
        const int before = lv_arc_get_value(arc);

        lv_event_send(btn, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
        const int after_plus = lv_arc_get_value(arc);
        CHECK_NE(after_plus, before);

        /* 第二个按钮是 "-"：按同样次数应回到原值 */
        lv_obj_t *minus = NULL;
        const size_t cnt = lv_obj_get_child_cnt(scr);
        for (size_t i = 0; i < cnt; i++) {
            lv_obj_t *c = lv_obj_get_child(scr, i);
            if (c == btn) continue;
            if (lv_obj_check_type(c, &lv_btn_class)) { minus = c; break; }
        }
        CHECK(minus != NULL);
        if (minus != NULL) {
            lv_event_send(minus, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
            CHECK_EQ(lv_arc_get_value(arc), before);
        }

        /* 非长按重复事件不应改变数值 */
        lv_event_send(btn, LV_EVENT_CLICKED, NULL);
        CHECK_EQ(lv_arc_get_value(arc), before);
    }

    release_slot(slot);
}

/* ── TF 卡视频 ───────────────────────────────────────────────────────────── */

#define VIDEO_PATH_LEN 64

static void make_video_file(int frames, uint16_t rgb565be)
{
    char path[VIDEO_PATH_LEN];
    video_join_path(path, sizeof(path), SD_CARD_MOUNT_POINT, VIDEO_FILE_NAME);

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        mt_report_failure(__FILE__, __LINE__, "cannot write test video");
        return;
    }
    const size_t bytes = video_frame_bytes(VIDEO_FRAME_W, VIDEO_FRAME_H, VIDEO_FRAME_BPP);
    for (size_t i = 0; i < bytes; i++) {
        fputc((int)((i & 1) ? (rgb565be & 0xFF) : (rgb565be >> 8)), fp);
    }
    for (int f = 1; f < frames; f++) {
        const uint16_t c = (uint16_t)(rgb565be + (uint16_t)f);
        for (size_t i = 0; i < bytes; i++) {
            fputc((int)((i & 1) ? (c & 0xFF) : (c >> 8)), fp);
        }
    }
    fclose(fp);
}

static void remove_video_file(void)
{
    char path[VIDEO_PATH_LEN];
    video_join_path(path, sizeof(path), SD_CARD_MOUNT_POINT, VIDEO_FILE_NAME);
    (void)remove(path);
}

/* 两种内存都拿不到：只能显示"内存不足"，且不能崩 */
MT_TEST(test_video_without_memory_shows_hint)
{
    ensure_host();
    host_heap_caps_fail_all(true);

    screen_video_init();
    lv_obj_t **slot = screen_video_get_ptr();
    CHECK(*slot != NULL);
    if (*slot == NULL) { host_heap_caps_fail_all(false); return; }

    /* 建屏时就该把状态写成"无内存"（还没触发加载事件） */
    lv_obj_t *lbl = find_child(*slot, &lv_label_class);
    CHECK(lbl != NULL);
    if (lbl != NULL) CHECK_STR_EQ(lv_label_get_text(lbl), VIDEO_STATUS_NO_MEM);

    lv_scr_load(*slot);
    render_a_bit();
    CHECK(lv_obj_is_valid(*slot));

    release_slot(slot);
    host_heap_caps_fail_all(false);
}

/* PSRAM 失败时回落到内部 RAM（帧缓冲仍然可用） */
MT_TEST(test_video_plays_frames_from_sd_card)
{
    ensure_host();
    host_heap_caps_fail_spiram_once();
    remove_video_file();
    make_video_file(3, 0xF800);            /* 全红 */
    host_sd_card_set_mounted(true);

    screen_video_init();
    lv_obj_t **slot = screen_video_get_ptr();
    CHECK(*slot != NULL);
    if (*slot == NULL) { host_sd_card_set_mounted(false); return; }

    lv_host_reset_flush_count();
    lv_scr_load(*slot);                     /* 触发 SCREEN_LOADED → open_video */
    for (int i = 0; i < 8; i++) {
        lv_host_advance_ms(APP_VIDEO_FRAME_MS);
        lv_host_run_timers();
    }

    CHECK(lv_host_flush_count() > 0u);
    CHECK(lv_host_non_black_pixels() >= 1000u);
    const lv_color_t center = lv_host_framebuffer()[180 * 360 + 180];
    CHECK(center.ch.red > 0);

    release_slot(slot);
    host_sd_card_set_mounted(false);
    remove_video_file();
}

/* 文件被截断 → 读取失败 → 关闭并降级为"重试"节奏（旧版行为） */
MT_TEST(test_video_recovers_from_read_error)
{
    ensure_host();
    remove_video_file();
    make_video_file(3, 0x07E0);
    host_sd_card_set_mounted(true);

    screen_video_init();
    lv_obj_t **slot = screen_video_get_ptr();
    if (*slot == NULL) { host_sd_card_set_mounted(false); return; }

    lv_scr_load(*slot);
    lv_host_advance_ms(APP_VIDEO_FRAME_MS);
    lv_host_run_timers();

    char path[VIDEO_PATH_LEN];
    video_join_path(path, sizeof(path), SD_CARD_MOUNT_POINT, VIDEO_FILE_NAME);
    CHECK_EQ(truncate(path, 8), 0);

    for (int i = 0; i < 8; i++) {
        lv_host_advance_ms(APP_VIDEO_RETRY_MS);
        lv_host_run_timers();
    }

    /* 出错之后屏幕仍然可用（状态标签 + 黑色画面），不会崩 */
    CHECK(lv_obj_is_valid(*slot));

    release_slot(slot);
    host_sd_card_set_mounted(false);
    remove_video_file();
}

/* 没插卡：只显示提示，不崩 */
MT_TEST(test_video_without_card_shows_hint)
{
    ensure_host();
    host_sd_card_set_mounted(false);

    screen_video_init();
    lv_obj_t **slot = screen_video_get_ptr();
    CHECK(*slot != NULL);
    if (*slot == NULL) return;

    lv_scr_load(*slot);
    for (int i = 0; i < 4; i++) {
        lv_host_advance_ms(APP_VIDEO_RETRY_MS);
        lv_host_run_timers();
    }
    CHECK(lv_obj_is_valid(*slot));

    release_slot(slot);
}

/* ── 3D 立方体：缩放弧拖动 + 复位按钮 ─────────────────────────────────────── */

MT_TEST(test_3dmodel_zoom_drag_and_reset_button)
{
    ensure_host();

    screen_3dmodel_init();
    lv_obj_t **slot = screen_3dmodel_get_ptr();
    CHECK(*slot != NULL);
    if (*slot == NULL) return;

    lv_scr_load(*slot);
    render_a_bit();

    /* 在缩放弧上按下并拖动（半径 142、角度 360° 附近） */
    lv_host_touch_down(180 + 142, 180);
    lv_host_touch_move(180 + 142, 150);
    render_a_bit();
    lv_host_touch_up();
    render_a_bit();
    CHECK(lv_host_flush_count() > 0u);

    /* 复位按钮 */
    lv_obj_t *btn = find_child(*slot, &lv_btn_class);
    CHECK(btn != NULL);
    if (btn != NULL) {
        lv_event_send(btn, LV_EVENT_CLICKED, NULL);
        render_a_bit();
        /* 只响应 CLICKED */
        lv_event_send(btn, LV_EVENT_RELEASED, NULL);
    }

    release_slot(slot);
}

/* ── Codex 表盘：上下滑切主题 ─────────────────────────────────────────────── */

static uint32_t fb_checksum(void)
{
    const lv_color_t *fb = lv_host_framebuffer();
    uint32_t h = 2166136261u;
    for (int i = 0; i < LV_HOST_FB_SIZE; i += 37) {
        h = (h ^ fb[i].full) * 16777619u;
    }
    return h;
}

MT_TEST(test_codex_swipe_up_toggles_theme)
{
    ensure_host();

    settle_input();
    screen_codex_usage_init();
    lv_obj_t **slot = screen_codex_usage_get_ptr();
    CHECK(*slot != NULL);
    if (*slot == NULL) return;

    lv_scr_load(*slot);
    for (int i = 0; i < 3; i++) render_a_bit();
    const uint32_t dark_sum = fb_checksum();
    CHECK(dark_sum != 0u);

    /* 向上滑（手指从下往上）= LV_DIR_TOP → 切到浅色 */
    lv_host_swipe(180, 300, 180, 90);
    for (int i = 0; i < 3; i++) render_a_bit();
    CHECK_NE(fb_checksum(), dark_sum);

    /* 再滑回来 */
    lv_host_swipe(180, 90, 180, 300);
    for (int i = 0; i < 3; i++) render_a_bit();
    CHECK_EQ(fb_checksum(), dark_sum);

    release_slot(slot);
}

MT_TEST(test_codex_usage_renders_live_data)
{
    ensure_host();
    settle_input();

    usage_data_t data;
    usage_data_defaults(&data);
    data.current_used_pct = 27;
    data.weekly_used_pct = 73;
    data.current_resets_in = 2 * USAGE_SEC_PER_HOUR;
    data.weekly_resets_in = 3 * USAGE_SEC_PER_DAY;
    snprintf(data.weekly_reset_label, sizeof(data.weekly_reset_label), "16:14 on 18 May");
    data.plan = USAGE_PLAN_PRO;
    data.has_credits = true;
    data.valid = true;
    data.rx_ms = (uint32_t)lv_tick_get();
    usage_store_set(&data);

    screen_codex_usage_init();
    lv_obj_t **slot = screen_codex_usage_get_ptr();
    CHECK(*slot != NULL);
    if (*slot == NULL) {
        usage_store_reset();
        return;
    }

    lv_scr_load(*slot);
    for (int i = 0; i < 3; i++) render_a_bit();
    CHECK(lv_host_non_black_pixels() >= 200u);

    /* 数据过期后回落到占位文案，屏幕仍必须能画 */
    lv_host_advance_ms(WF_USAGE_STALE_MS + 1000u);
    render_a_bit();
    CHECK(lv_host_non_black_pixels() >= 200u);

    release_slot(slot);
    usage_store_reset();
}

/* ── 公共控件工厂的两条分支 ───────────────────────────────────────────────── */

MT_TEST(test_fullscreen_layer_clickability_option)
{
    ensure_host();

    lv_obj_t *parent = lv_obj_create(NULL);

    lv_obj_t *grab = ui_fullscreen_layer_create(parent, true);
    CHECK(lv_obj_has_flag(grab, LV_OBJ_FLAG_CLICKABLE));

    lv_obj_t *passthrough = ui_fullscreen_layer_create(parent, false);
    CHECK(passthrough != NULL);
    CHECK_FALSE(lv_obj_has_flag(passthrough, LV_OBJ_FLAG_CLICKABLE));

    /* parent == NULL 在 LVGL 里表示"再开一张屏"，是合法用法 */
    lv_obj_t *as_screen = ui_fullscreen_layer_create(NULL, true);
    CHECK(as_screen != NULL);
    if (as_screen != NULL) lv_obj_del(as_screen);
    CHECK(ui_fullscreen_layer_create(parent, true) != NULL);
    lv_obj_del(parent);
}
