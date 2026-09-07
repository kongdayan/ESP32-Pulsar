/*
 * nav_map.h — 屏幕导航拓扑（纯数据，无 LVGL）。
 *
 * 旧版把 "左滑去哪 / 右滑去哪 / 用什么动画" 硬编码在每个屏幕的 on_gesture() 里，
 * 这里集中成一张表，视图层只负责把 LV_DIR_* 翻译成 nav_dir_t。
 */
#ifndef CORE_NAV_MAP_H
#define CORE_NAV_MAP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NAV_SCREEN_DASHBOARD = 0,
    NAV_SCREEN_INFO,
    NAV_SCREEN_IMAGE,
    NAV_SCREEN_VIDEO,
    NAV_SCREEN_ABOUT,
    NAV_SCREEN_AGENT,
    NAV_SCREEN_MODEL3D,
    NAV_SCREEN_CODEX_USAGE,
    NAV_SCREEN_COUNT
} nav_screen_id_t;

typedef enum {
    NAV_DIR_NONE = 0,
    NAV_DIR_LEFT,
    NAV_DIR_RIGHT
} nav_dir_t;

typedef enum {
    NAV_ANIM_MOVE = 0, /* 带 APP_NAV_ANIM_MS 时长的位移动画 */
    NAV_ANIM_NONE      /* 直接切换 */
} nav_anim_t;

typedef struct {
    nav_screen_id_t target;
    nav_anim_t      anim;
} nav_step_t;

/* 查表：from + dir → 目标屏幕。不可达时返回 false 且不写 *out */
bool nav_map_step(nav_screen_id_t from, nav_dir_t dir, nav_step_t *out);

/* 该屏幕是否挂了某个方向的导航（用于测试拓扑完整性） */
bool nav_map_has_link(nav_screen_id_t from, nav_dir_t dir);

const char *nav_screen_name(nav_screen_id_t id);
bool        nav_screen_is_valid(nav_screen_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* CORE_NAV_MAP_H */
