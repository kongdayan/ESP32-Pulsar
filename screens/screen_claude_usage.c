/*
 * screen_claude_usage.c — Claude 用量表盘。
 * 绘制全部在 provider 无关的 usage_face.c；本文件只绑定 provider + 刷新回调。
 */
#include "ui.h"

#include "usage_face.h"
#include "usage_face_layout.h"

static usage_face_t face;

static void on_refresh(lv_timer_t *timer)
{
    (void)timer;
    usage_face_refresh(&face);
}

static const ui_timer_binding_t k_binding = {
    .cb = on_refresh,
    .period_ms = WF_REFRESH_MS,
    .handle = &face.timer,
};

void screen_claude_usage_init(void)
{
    usage_face_init(&face, USAGE_PROVIDER_CLAUDE, NAV_SCREEN_CLAUDE_USAGE, &k_binding);
}

lv_obj_t **screen_claude_usage_get_ptr(void) { return usage_face_ptr(&face); }
