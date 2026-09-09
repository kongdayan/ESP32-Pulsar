/*
 * ui_screen.h — 各屏幕共享的 LVGL 样板：
 *   · 懒加载屏幕根对象创建
 *   · 基于 core/nav_map 的手势切屏
 *   · 常用控件（标签 / 按钮 / 全屏层）
 *   · 定时器随屏幕加载/卸载的生命周期管理
 *
 * 屏幕 .c 文件只保留"这块屏长什么样"，不再重复这些模板代码。
 */
#ifndef COMMON_UI_SCREEN_H
#define COMMON_UI_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include <lvgl.h>

#include "nav_map.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 传给 ui_label_create_aligned 表示沿用主题默认色，不覆盖 text color */
#define UI_COLOR_KEEP_DEFAULT  0xFFFFFFFFu

/* 把 nav_screen_id_t 塞进事件 user_data */
#define UI_NAV_USER_DATA(id) ((void *)(lv_uintptr_t)(id))
#define UI_NAV_EVENT_ID(e)   ((nav_screen_id_t)(lv_uintptr_t)lv_event_get_user_data(e))

/* 创建屏幕根对象：无滚动；attach_nav 为真时挂好本屏的左右滑动导航。
 * 自行处理手势的屏幕（小游戏 / 3D 模型）传 false，避免手势重复触发切屏。 */
lv_obj_t *ui_screen_create_ex(nav_screen_id_t self, bool attach_nav);

/* 创建屏幕根对象：无滚动 + 挂好本屏的左右滑动导航 */
lv_obj_t *ui_screen_create(nav_screen_id_t self);

/* 给任意对象挂导航手势（屏幕本身或其子层） */
void ui_screen_attach_nav(lv_obj_t *obj, nav_screen_id_t self);

/* 设置屏幕底色（0xRRGGBB） */
void ui_screen_set_bg(lv_obj_t *scr, uint32_t rgb);

/* 透明面板：无滚动、无背景、无边框，居中放置 */
lv_obj_t *ui_panel_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);

/* 全屏透明层：用于自定义绘制 + 触摸输入 */
lv_obj_t *ui_fullscreen_layer_create(lv_obj_t *parent, bool clickable);

/* 按对齐方式放置的标签；color 为 0xRRGGBB，font 可为 NULL（用默认字体） */
lv_obj_t *ui_label_create_aligned(lv_obj_t *parent, const char *text, uint32_t color,
                                  const lv_font_t *font, lv_coord_t x, lv_coord_t y,
                                  lv_align_t align);

/* 带居中文字的按钮 */
lv_obj_t *ui_button_create(lv_obj_t *parent, const char *text, lv_coord_t w, lv_coord_t h,
                           lv_coord_t x, lv_coord_t y, lv_align_t align, lv_event_cb_t cb);

/* 消费一个 LV_EVENT_GESTURE：内部会 wait_release，返回映射后的方向（非手势返回 NAV_DIR_NONE） */
nav_dir_t ui_nav_gesture_dir(lv_event_t *e);

/* 同上，但返回 LVGL 原始方向（需要上下滑动等额外手势的屏幕使用） */
lv_dir_t ui_nav_raw_gesture(lv_event_t *e);

/* LV_DIR_* → nav_dir_t（只识别左右） */
nav_dir_t ui_nav_dir_from_lv(lv_dir_t dir);

/* 按导航表切屏；该方向没有链路时返回 false */
bool ui_nav_go(nav_screen_id_t self, nav_dir_t dir);

/* LV_EVENT_ALL 回调：user_data 为 UI_NAV_USER_DATA(id) */
void ui_nav_event_cb(lv_event_t *e);

/* 定时器绑定：随屏幕 LOADED 创建、UNLOADED 销毁（binding 必须是静态生命周期对象） */
typedef struct {
    lv_timer_cb_t cb;
    uint32_t      period_ms;
    lv_timer_t  **handle;
    void         *user_data;   /* 传给 lv_timer_create 的 user_data，可为 NULL */
} ui_timer_binding_t;

void ui_timer_attach(lv_obj_t *scr, const ui_timer_binding_t *binding);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_UI_SCREEN_H */
