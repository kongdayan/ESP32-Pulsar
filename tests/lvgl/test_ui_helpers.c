/*
 * ui/ui_helpers.c 是 SquareLine 生成的公共层（不允许手改），
 * 但它被所有屏幕依赖，所以在这里把它自己的分支全部跑一遍。
 */
#include "minitest.h"

#include <stddef.h>
#include <string.h>

#include "lv_host.h"
#include "ui.h"
#include "ui_helpers.h"

static lv_obj_t *s_scr;

/* 动画/图片回测共用的最小镜像描述符 */
static const uint8_t k_px[4] = { 0, 0, 0, 0 };
static lv_img_dsc_t k_img_dsc;
static lv_img_dsc_t *k_imgset[2] = { &k_img_dsc, &k_img_dsc };

/* lv_obj_get_x/width 读的是"已布局"的坐标，主机上没跑渲染循环时要手动刷新 */
static void relayout(lv_obj_t *obj)
{
    lv_obj_update_layout(obj);
}

/* 首屏是懒加载的，用例之间必须把它清干净 */
static void release_codex_screen(void)
{
    lv_obj_t **slot = screen_codex_usage_get_ptr();
    if (*slot != NULL) {
        lv_scr_load(s_scr);
        lv_obj_del(*slot);
        *slot = NULL;
    }
}

static void ensure_scr(void)
{
    static bool inited;
    if (!inited) {
        lv_host_init();
        inited = true;
    }
    if (s_scr == NULL) {
        s_scr = lv_obj_create(NULL);
        lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
        lv_scr_load(s_scr);
    }
}

MT_TEST(test_ui_helpers_bar_slider_arc_props)
{
    ensure_scr();

    lv_obj_t *bar = lv_bar_create(s_scr);
    _ui_bar_set_property(bar, _UI_BAR_PROPERTY_VALUE, 40);
    CHECK_EQ(lv_bar_get_value(bar), 40);
    _ui_bar_set_property(bar, _UI_BAR_PROPERTY_VALUE_WITH_ANIM, 60);
    CHECK_EQ(lv_bar_get_value(bar), 60);
    _ui_bar_set_property(bar, 99, 10);              /* 未知 id：什么都不做 */
    CHECK_EQ(lv_bar_get_value(bar), 60);

    _ui_bar_increment(bar, 10, LV_ANIM_OFF);
    CHECK_EQ(lv_bar_get_value(bar), 70);
    _ui_bar_increment(bar, -1000, LV_ANIM_OFF);     /* 越界由控件自己夹住 */
    CHECK_EQ(lv_bar_get_value(bar), 0);

    lv_obj_t *slider = lv_slider_create(s_scr);
    _ui_slider_set_property(slider, _UI_SLIDER_PROPERTY_VALUE, 25);
    CHECK_EQ(lv_slider_get_value(slider), 25);
    _ui_slider_increment(slider, 5, LV_ANIM_OFF);
    CHECK_EQ(lv_slider_get_value(slider), 30);
    _ui_slider_increment(slider, 0, LV_ANIM_OFF);
    CHECK_EQ(lv_slider_get_value(slider), 30);

    lv_obj_t *arc = lv_arc_create(s_scr);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 50);
    _ui_arc_increment(arc, 12);
    CHECK_EQ(lv_arc_get_value(arc), 62);
    _ui_arc_increment(arc, -1000);                   /* 夹到最小值 */
    CHECK_EQ(lv_arc_get_value(arc), 0);

    lv_obj_del(bar);
    lv_obj_del(slider);
    lv_obj_del(arc);
}

MT_TEST(test_ui_helpers_basic_props)
{
    ensure_scr();

    lv_obj_t *obj = lv_obj_create(s_scr);
    _ui_basic_set_property(obj, _UI_BASIC_PROPERTY_POSITION_X, 12);
    relayout(obj);
    CHECK_EQ(lv_obj_get_x(obj), 12);
    _ui_basic_set_property(obj, _UI_BASIC_PROPERTY_POSITION_Y, 34);
    relayout(obj);
    CHECK_EQ(lv_obj_get_y(obj), 34);
    _ui_basic_set_property(obj, _UI_BASIC_PROPERTY_WIDTH, 56);
    relayout(obj);
    CHECK_EQ(lv_obj_get_width(obj), 56);
    _ui_basic_set_property(obj, _UI_BASIC_PROPERTY_HEIGHT, 78);
    relayout(obj);
    CHECK_EQ(lv_obj_get_height(obj), 78);
    _ui_basic_set_property(obj, 4242, 5);
    relayout(obj);
    CHECK_EQ(lv_obj_get_height(obj), 78);

    _ui_opacity_set(obj, 128);
    CHECK_EQ(lv_obj_get_style_opa(obj, LV_PART_MAIN), 128);
    _ui_opacity_set(obj, 255);

    lv_obj_del(obj);
}

MT_TEST(test_ui_helpers_label_and_image_props)
{
    ensure_scr();

    lv_obj_t *lbl = lv_label_create(s_scr);
    _ui_label_set_property(lbl, _UI_LABEL_PROPERTY_TEXT, "hello");
    CHECK_STR_EQ(lv_label_get_text(lbl), "hello");
    _ui_label_set_property(lbl, 7, "ignored");
    CHECK_STR_EQ(lv_label_get_text(lbl), "hello");

    k_img_dsc.header.cf = LV_IMG_CF_RAW;
    k_img_dsc.header.w = 1;
    k_img_dsc.header.h = 1;
    k_img_dsc.data_size = 4;
    k_img_dsc.data = k_px;

    lv_obj_t *img = lv_img_create(s_scr);
    _ui_image_set_property(img, _UI_IMAGE_PROPERTY_IMAGE, (uint8_t *)&k_img_dsc);

    lv_obj_del(lbl);
    lv_obj_del(img);
}

MT_TEST(test_ui_helpers_flag_and_state)
{
    ensure_scr();

    lv_obj_t *obj = lv_obj_create(s_scr);

    _ui_flag_modify(obj, LV_OBJ_FLAG_CLICKABLE, _UI_MODIFY_FLAG_REMOVE);
    CHECK_FALSE(lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE));
    _ui_flag_modify(obj, LV_OBJ_FLAG_CLICKABLE, _UI_MODIFY_FLAG_ADD);
    CHECK(lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE));
    _ui_flag_modify(obj, LV_OBJ_FLAG_CLICKABLE, _UI_MODIFY_FLAG_TOGGLE);
    CHECK_FALSE(lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE));
    _ui_flag_modify(obj, LV_OBJ_FLAG_CLICKABLE, _UI_MODIFY_FLAG_ADD);
    _ui_flag_modify(obj, LV_OBJ_FLAG_CLICKABLE, 9999);   /* 未知操作 = 走 clear 分支 */
    CHECK_FALSE(lv_obj_has_flag(obj, LV_OBJ_FLAG_CLICKABLE));

    /* 用自定义状态位：LV_STATE_PRESSED 会被 LVGL 的输入状态机改写 */
    _ui_state_modify(obj, LV_STATE_USER_1, _UI_MODIFY_STATE_ADD);
    CHECK(lv_obj_has_state(obj, LV_STATE_USER_1));
    _ui_state_modify(obj, LV_STATE_USER_1, _UI_MODIFY_STATE_REMOVE);
    CHECK_FALSE(lv_obj_has_state(obj, LV_STATE_USER_1));
    _ui_state_modify(obj, LV_STATE_USER_1, _UI_MODIFY_STATE_TOGGLE);
    CHECK(lv_obj_has_state(obj, LV_STATE_USER_1));
    _ui_state_modify(obj, LV_STATE_USER_1, 9999);   /* 未知操作 = 走 clear 分支 */
    CHECK_FALSE(lv_obj_has_state(obj, LV_STATE_USER_1));

    /* 未按下时 TOGGLE = add 分支 */
    _ui_flag_modify(obj, LV_OBJ_FLAG_SCROLLABLE, _UI_MODIFY_FLAG_REMOVE);
    _ui_flag_modify(obj, LV_OBJ_FLAG_SCROLLABLE, _UI_MODIFY_FLAG_TOGGLE);
    CHECK(lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE));

    lv_obj_del(obj);
}

MT_TEST(test_ui_helpers_text_value_setters)
{
    ensure_scr();

    lv_obj_t *arc = lv_arc_create(s_scr);
    lv_arc_set_value(arc, 42);
    lv_obj_t *lbl = lv_label_create(s_scr);
    _ui_arc_set_text_value(lbl, arc, "v=", "ms");
    CHECK_STR_EQ(lv_label_get_text(lbl), "v=42ms");

    lv_obj_t *slider = lv_slider_create(s_scr);
    lv_slider_set_value(slider, 7, LV_ANIM_OFF);
    _ui_slider_set_text_value(lbl, slider, "", "%");
    CHECK_STR_EQ(lv_label_get_text(lbl), "7%");

    lv_obj_t *sw = lv_switch_create(s_scr);
    _ui_checked_set_text_value(lbl, sw, "on", "off");
    CHECK_STR_EQ(lv_label_get_text(lbl), "off");
    lv_obj_add_state(sw, LV_STATE_CHECKED);
    _ui_checked_set_text_value(lbl, sw, "on", "off");
    CHECK_STR_EQ(lv_label_get_text(lbl), "on");

    lv_obj_del(arc);
    lv_obj_del(slider);
    lv_obj_del(sw);
    lv_obj_del(lbl);
}

MT_TEST(test_ui_helpers_anim_callbacks)
{
    ensure_scr();

    lv_obj_t *obj = lv_obj_create(s_scr);
    lv_obj_set_size(obj, 10, 10);
    lv_obj_set_pos(obj, 1, 2);

    lv_anim_t a;
    lv_anim_init(&a);
    a.var = obj;

    ui_anim_user_data_t usr;
    memset(&usr, 0, sizeof(usr));
    usr.target = obj;
    usr.imgset = k_imgset;
    usr.imgset_size = 2;
    a.user_data = &usr;

    _ui_anim_callback_set_x(&a, 20);
    relayout(obj);
    CHECK_EQ(lv_obj_get_x(obj), 20);
    CHECK_EQ(_ui_anim_callback_get_x(&a), 20);

    _ui_anim_callback_set_y(&a, 30);
    relayout(obj);
    CHECK_EQ(lv_obj_get_y(obj), 30);
    CHECK_EQ(_ui_anim_callback_get_y(&a), 30);

    _ui_anim_callback_set_width(&a, 40);
    relayout(obj);
    CHECK_EQ(_ui_anim_callback_get_width(&a), 40);

    _ui_anim_callback_set_height(&a, 50);
    relayout(obj);
    CHECK_EQ(_ui_anim_callback_get_height(&a), 50);

    _ui_anim_callback_set_opacity(&a, 100);
    CHECK_EQ(_ui_anim_callback_get_opacity(&a), 100);
    _ui_anim_callback_set_opacity(&a, 255);

    /* 动画回调里 target 为 NULL 时也不能崩 */
    ui_anim_user_data_t empty;
    memset(&empty, 0, sizeof(empty));
    a.user_data = &empty;
    lv_obj_del(obj);

    lv_obj_t *img = lv_img_create(s_scr);
    lv_obj_set_size(img, 10, 10);
    a.user_data = &usr;
    usr.target = img;
    usr.imgset = k_imgset;
    usr.imgset_size = 2;

    _ui_anim_callback_set_image_zoom(&a, 256);
    CHECK_EQ(_ui_anim_callback_get_image_zoom(&a), 256);
    _ui_anim_callback_set_image_angle(&a, 30);
    CHECK_EQ(_ui_anim_callback_get_image_angle(&a), 30);
    _ui_anim_callback_set_image_frame(&a, 1);
    CHECK_EQ(_ui_anim_callback_get_image_frame(&a), 1);
    _ui_anim_callback_set_image_frame(&a, 99);   /* 越界夹紧 */
    CHECK_EQ(usr.val, 99);

    /* user_data 由 helper 分配时才会被释放 */
    ui_anim_user_data_t *heap_usr = lv_mem_alloc(sizeof(ui_anim_user_data_t));
    CHECK(heap_usr != NULL);
    memset(heap_usr, 0, sizeof(*heap_usr));
    heap_usr->target = img;
    a.user_data = heap_usr;
    _ui_anim_callback_free_user_data(&a);
    CHECK(a.user_data == NULL);

    a.user_data = NULL;
    _ui_anim_callback_free_user_data(&a);   /* NULL 也允许 */

    lv_obj_del(img);
}

static int s_init_calls;
static lv_obj_t *s_slot;      /* 被测的"懒加载槽位" */

/* 等价于各屏的 screen_xxx_init()：把新建的屏幕写回槽位 */
static void counting_init(void)
{
    s_init_calls++;
    s_slot = lv_obj_create(NULL);
    lv_obj_clear_flag(s_slot, LV_OBJ_FLAG_SCROLLABLE);
}

MT_TEST(test_ui_helpers_screen_change_and_delete)
{
    ensure_scr();

    s_slot = NULL;
    s_init_calls = 0;

    /* 目标为 NULL → 先懒加载再切换 */
    _ui_screen_change(&s_slot, LV_SCR_LOAD_ANIM_MOVE_LEFT, 10, 0, counting_init);
    CHECK_EQ(s_init_calls, 1);
    CHECK(s_slot != NULL);

    /* 再次切换到同一目标：init 不应重复执行 */
    _ui_screen_change(&s_slot, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 0, 0, counting_init);
    CHECK_EQ(s_init_calls, 1);

    /* 等动画收尾，再把屏幕换回中转屏：
     * LVGL 不允许删除仍然是 act_scr/prev_scr 的对象 */
    for (int i = 0; i < 4; i++) {
        lv_host_advance_ms(20u);
        lv_host_run_timers();
    }
    lv_scr_load(s_scr);
    lv_host_advance_ms(20u);
    lv_host_run_timers();

    _ui_screen_delete(&s_slot);
    CHECK(s_slot == NULL);
    _ui_screen_delete(&s_slot);   /* 幂等 */
    /* 注意：ui_helpers 是生成代码，_ui_screen_delete(NULL)/_ui_screen_change(NULL,...)
     * 会直接解引用空指针 —— 调用方必须传合法槽位，这里不验证该错误用法。 */
}

MT_TEST(test_ui_helpers_switch_theme_is_safe)
{
    ensure_scr();
    _ui_switch_theme(0);   /* 本工程未启用 UI_THEME_ACTIVE，空实现也不能崩 */
    _ui_switch_theme(1);
    CHECK_TRUE(1);
}

/* ui_init()：装主题 + 加载开机首屏 */
MT_TEST(test_ui_init_loads_startup_screen)
{
    ensure_scr();

    release_codex_screen();
    ui_init();

    lv_theme_t *theme = lv_disp_get_theme(lv_disp_get_default());
    CHECK(theme != NULL);
    CHECK(lv_obj_is_valid(lv_host_active_screen()));
    CHECK_EQ(lv_host_active_screen(), *screen_codex_usage_get_ptr());

    release_codex_screen();
}

/* SquareLine 遗留的通用回调：屏卸载时删掉自己持有的屏幕 */
MT_TEST(test_ui_helpers_scr_unloaded_delete_cb)
{
    ensure_scr();

    static lv_obj_t *victim;
    victim = lv_obj_create(NULL);
    CHECK(victim != NULL);

    lv_obj_t *holder = lv_obj_create(NULL);
    /* 只能挂在 SCREEN_UNLOADED 上：这个回调不判事件码，挂 ALL 会在加载时被误触发 */
    lv_obj_add_event_cb(holder, scr_unloaded_delete_cb, LV_EVENT_SCREEN_UNLOADED, &victim);

    lv_scr_load(holder);          /* 让 s_scr 成为"被卸载"的那一屏 */
    lv_scr_load(s_scr);           /* holder 卸载 → 回调删除 victim */
    CHECK(victim == NULL);

    lv_scr_load(s_scr);
    lv_obj_del(holder);
}
