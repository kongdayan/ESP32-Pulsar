/*
 * screen_claude_usage.c — Claude 用量表盘。
 * 绘制全部在 provider 无关的 usage_face.c；本文件只绑定 provider + 屏 id。
 */
#include "ui.h"

#include "usage_face.h"

static usage_face_t face;

void screen_claude_usage_init(void)
{
    usage_face_init(&face, USAGE_PROVIDER_CLAUDE, NAV_SCREEN_CLAUDE_USAGE);
}

lv_obj_t **screen_claude_usage_get_ptr(void) { return usage_face_ptr(&face); }
