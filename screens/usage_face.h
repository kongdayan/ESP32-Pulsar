/*
 * usage_face.h — provider 无关的「用量表盘」共用实现。
 *
 * 每个服务（Codex / Claude / NVIDIA / AMD / GLM …）只有一个薄屏文件：
 * 声明一份静态 usage_face_t + 一个刷新回调，其余绘制全在这里。
 * 数据来自 core/usage_model 的按 provider 分槽的共享存储。
 */
#ifndef SCREENS_USAGE_FACE_H
#define SCREENS_USAGE_FACE_H

#include <lvgl.h>

#include "nav_map.h"
#include "ui_screen.h"
#include "ui_theme.h"
#include "usage_model.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    usage_provider_t provider;
    nav_screen_id_t  screen_id;
    ui_theme_mode_t  theme;
    lv_obj_t        *scr;
    lv_obj_t        *panel;
    lv_timer_t      *timer;
} usage_face_t;

/* binding 由各屏提供（静态生命周期），handle 指向 face->timer */
void       usage_face_init(usage_face_t *face, usage_provider_t provider,
                           nav_screen_id_t screen_id, const ui_timer_binding_t *binding);
void       usage_face_refresh(usage_face_t *face);
lv_obj_t **usage_face_ptr(usage_face_t *face);

#ifdef __cplusplus
}
#endif

#endif /* SCREENS_USAGE_FACE_H */
