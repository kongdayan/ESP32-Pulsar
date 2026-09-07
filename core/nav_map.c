#include "nav_map.h"

#include <stddef.h>

/* 行下标即 nav_screen_id_t（用指定初始化器保证对应关系），
 * 因此不需要额外的 id 字段。 */
typedef struct {
    nav_step_t  left;
    nav_step_t  right;
    const char *name;
} nav_entry_t;

#define NAV_MOVE(dst)  { (dst), NAV_ANIM_MOVE }
#define NAV_JUMP(dst)  { (dst), NAV_ANIM_NONE }
#define NAV_NOWHERE    { NAV_SCREEN_COUNT, NAV_ANIM_NONE }

/* 与旧版每个屏幕 on_gesture() 的行为逐条对应 */
static const nav_entry_t k_nav_table[NAV_SCREEN_COUNT] = {
    [NAV_SCREEN_DASHBOARD]   = { NAV_MOVE(NAV_SCREEN_INFO),         NAV_MOVE(NAV_SCREEN_CODEX_USAGE), "dashboard"   },
    [NAV_SCREEN_INFO]        = { NAV_MOVE(NAV_SCREEN_IMAGE),        NAV_MOVE(NAV_SCREEN_DASHBOARD),    "info"        },
    [NAV_SCREEN_IMAGE]       = { NAV_MOVE(NAV_SCREEN_VIDEO),        NAV_MOVE(NAV_SCREEN_INFO),         "image"       },
    [NAV_SCREEN_VIDEO]       = { NAV_MOVE(NAV_SCREEN_ABOUT),        NAV_MOVE(NAV_SCREEN_IMAGE),        "video"       },
    [NAV_SCREEN_ABOUT]       = { NAV_JUMP(NAV_SCREEN_CODEX_USAGE),  NAV_MOVE(NAV_SCREEN_VIDEO),        "about"       },
    [NAV_SCREEN_AGENT]       = { NAV_MOVE(NAV_SCREEN_MODEL3D),      NAV_MOVE(NAV_SCREEN_ABOUT),        "agent"       },
    [NAV_SCREEN_MODEL3D]     = { NAV_MOVE(NAV_SCREEN_CODEX_USAGE),  NAV_MOVE(NAV_SCREEN_ABOUT),        "3dmodel"     },
    [NAV_SCREEN_CODEX_USAGE] = { NAV_JUMP(NAV_SCREEN_ABOUT),        NAV_JUMP(NAV_SCREEN_ABOUT),        "codex_usage" },
};


bool nav_screen_is_valid(nav_screen_id_t id)
{
    return id >= NAV_SCREEN_DASHBOARD && id < NAV_SCREEN_COUNT;
}

const char *nav_screen_name(nav_screen_id_t id)
{
    if (!nav_screen_is_valid(id)) return "?";
    return k_nav_table[id].name;
}

bool nav_map_step(nav_screen_id_t from, nav_dir_t dir, nav_step_t *out)
{
    if (!nav_screen_is_valid(from) || out == NULL) return false;

    const nav_step_t *step = NULL;
    if (dir == NAV_DIR_LEFT) {
        step = &k_nav_table[from].left;
    } else if (dir == NAV_DIR_RIGHT) {
        step = &k_nav_table[from].right;
    } else {
        return false;
    }

    if (!nav_screen_is_valid(step->target)) return false;

    *out = *step;
    return true;
}

bool nav_map_has_link(nav_screen_id_t from, nav_dir_t dir)
{
    nav_step_t step;
    return nav_map_step(from, dir, &step);
}
