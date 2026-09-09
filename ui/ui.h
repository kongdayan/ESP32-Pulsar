#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include "ui_helpers.h"

LV_IMG_DECLARE(ui_img_1539399133);

// Each screen exposes an init function and a pointer to its lv_obj_t*.
// Screens are lazy-created: the lv_obj_t* is NULL until first visited.
void screen_dashboard_init(void);
void screen_info_init(void);
void screen_image_init(void);
void screen_video_init(void);
void screen_about_init(void);
void screen_agent_init(void);
void screen_3dmodel_init(void);
void screen_codex_usage_init(void);
void screen_claude_usage_init(void);
void screen_balance_init(void);

lv_obj_t **screen_dashboard_get_ptr(void);
lv_obj_t **screen_info_get_ptr(void);
lv_obj_t **screen_image_get_ptr(void);
lv_obj_t **screen_video_get_ptr(void);
lv_obj_t **screen_about_get_ptr(void);
lv_obj_t **screen_agent_get_ptr(void);
lv_obj_t **screen_3dmodel_get_ptr(void);
lv_obj_t **screen_codex_usage_get_ptr(void);
lv_obj_t **screen_claude_usage_get_ptr(void);
lv_obj_t **screen_balance_get_ptr(void);

void ui_init(void);

#ifdef __cplusplus
}
#endif

#endif
