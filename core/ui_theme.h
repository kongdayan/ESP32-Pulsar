/*
 * ui_theme.h — 集中管理主题色，避免各屏幕函数里散落 lv_color_hex(0x...)。
 *
 * 颜色以 0xRRGGBB 整数形式保存，由视图层交给 lv_color_hex()。
 */
#ifndef CORE_UI_THEME_H
#define CORE_UI_THEME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_THEME_DARK = 0,
    UI_THEME_LIGHT,
    UI_THEME_COUNT
} ui_theme_mode_t;

/* 语义化颜色角色；新增颜色请同时补 palette[][] */
typedef enum {
    UI_ROLE_BG = 0,       /* 屏幕底色 */
    UI_ROLE_TEXT_PRIMARY, /* 主文字 */
    UI_ROLE_TEXT_ACCENT,  /* 强调文字（蓝紫） */
    UI_ROLE_TEXT_GOOD,    /* 状态良好（绿） */
    UI_ROLE_RING,         /* 表盘外环 */
    UI_ROLE_TICK,         /* 刻度点 */
    UI_ROLE_TICK_HOT,     /* 四向主刻度 */
    UI_ROLE_RESET,        /* 重置提示 */
    UI_ROLE_BLUE,         /* 进度主色 */
    UI_ROLE_BLUE_DIM,     /* 进度底色 */
    UI_ROLE_GREEN,        /* 周用量主色 */
    UI_ROLE_GREEN_DIM,    /* 周用量底色 */
    UI_ROLE_MARK,         /* Codex 六边形标志 */
    UI_ROLE_ON_MARK,      /* 标志上的点 */
    UI_ROLE_COUNT
} ui_color_role_t;

bool ui_theme_is_valid(ui_theme_mode_t mode);
bool ui_theme_role_is_valid(ui_color_role_t role);
ui_theme_mode_t ui_theme_toggle(ui_theme_mode_t mode);

/* 取主题色；参数非法时返回 UI_ROLE_BG 的暗色值 */
uint32_t ui_theme_color(ui_theme_mode_t mode, ui_color_role_t role);

/* 表盘刻度透明度等按主题微调 */
bool ui_theme_is_light(ui_theme_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* CORE_UI_THEME_H */
