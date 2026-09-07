#include "ui_screen.h"

#include <stddef.h>

#include "app_config.h"
#include "ui.h"
#include "ui_helpers.h"

#define UI_BORDER_WIDTH_NONE 0

typedef struct {
    lv_obj_t **(*get_ptr)(void);
    void       (*init)(void);
} ui_screen_ref_t;

/* 导航表 id → 屏幕入口，顺序必须与 nav_screen_id_t 一致 */
static const ui_screen_ref_t k_screen_refs[NAV_SCREEN_COUNT] = {
    [NAV_SCREEN_DASHBOARD]   = { screen_dashboard_get_ptr,   screen_dashboard_init   },
    [NAV_SCREEN_INFO]        = { screen_info_get_ptr,        screen_info_init        },
    [NAV_SCREEN_IMAGE]       = { screen_image_get_ptr,       screen_image_init       },
    [NAV_SCREEN_VIDEO]       = { screen_video_get_ptr,       screen_video_init       },
    [NAV_SCREEN_ABOUT]       = { screen_about_get_ptr,       screen_about_init       },
    [NAV_SCREEN_AGENT]       = { screen_agent_get_ptr,       screen_agent_init       },
    [NAV_SCREEN_MODEL3D]     = { screen_3dmodel_get_ptr,     screen_3dmodel_init     },
    [NAV_SCREEN_CODEX_USAGE] = { screen_codex_usage_get_ptr, screen_codex_usage_init },
};

static lv_scr_load_anim_t nav_load_anim(nav_dir_t dir)
{
    return (dir == NAV_DIR_RIGHT) ? LV_SCR_LOAD_ANIM_MOVE_RIGHT : LV_SCR_LOAD_ANIM_MOVE_LEFT;
}

lv_obj_t *ui_screen_create_ex(nav_screen_id_t self, bool attach_nav)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    if (scr == NULL) return NULL;

    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    if (attach_nav) ui_screen_attach_nav(scr, self);
    return scr;
}

lv_obj_t *ui_screen_create(nav_screen_id_t self)
{
    return ui_screen_create_ex(self, true);
}

void ui_screen_attach_nav(lv_obj_t *obj, nav_screen_id_t self)
{
    if (obj == NULL) return;
    lv_obj_add_event_cb(obj, ui_nav_event_cb, LV_EVENT_ALL, UI_NAV_USER_DATA(self));
}

void ui_screen_set_bg(lv_obj_t *scr, uint32_t rgb)
{
    if (scr == NULL) return;
    lv_obj_set_style_bg_color(scr, lv_color_hex(rgb), LV_PART_MAIN);
}

lv_obj_t *ui_panel_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *panel = lv_obj_create(parent);
    if (panel == NULL) return NULL;

    lv_obj_set_size(panel, w, h);
    lv_obj_center(panel);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, UI_BORDER_WIDTH_NONE, LV_PART_MAIN);
    return panel;
}

lv_obj_t *ui_fullscreen_layer_create(lv_obj_t *parent, bool clickable)
{
    lv_obj_t *layer = ui_panel_create(parent, APP_SCREEN_PX, APP_SCREEN_PX);
    if (layer == NULL) return NULL;

    lv_obj_set_pos(layer, 0, 0);
    lv_obj_set_align(layer, LV_ALIGN_DEFAULT);

    if (clickable) {
        lv_obj_add_flag(layer, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(layer, LV_OBJ_FLAG_CLICKABLE);
    }
    return layer;
}

lv_obj_t *ui_label_create_aligned(lv_obj_t *parent, const char *text, uint32_t color,
                                  const lv_font_t *font, lv_coord_t x, lv_coord_t y,
                                  lv_align_t align)
{
    lv_obj_t *lbl = lv_label_create(parent);
    if (lbl == NULL) return NULL;

    lv_label_set_text(lbl, text);
    if (color != UI_COLOR_KEEP_DEFAULT) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(color), LV_PART_MAIN);
    }
    if (font != NULL) lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_align(lbl, align);
    return lbl;
}

lv_obj_t *ui_button_create(lv_obj_t *parent, const char *text, lv_coord_t w, lv_coord_t h,
                           lv_coord_t x, lv_coord_t y, lv_align_t align, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    if (btn == NULL) return NULL;

    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_align(btn, align);
    if (cb != NULL) lv_obj_add_event_cb(btn, cb, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    return btn;
}

nav_dir_t ui_nav_dir_from_lv(lv_dir_t dir)
{
    if (dir == LV_DIR_LEFT) return NAV_DIR_LEFT;
    if (dir == LV_DIR_RIGHT) return NAV_DIR_RIGHT;
    return NAV_DIR_NONE;
}

lv_dir_t ui_nav_raw_gesture(lv_event_t *e)
{
    if (e == NULL || lv_event_get_code(e) != LV_EVENT_GESTURE) return LV_DIR_NONE;

    lv_indev_t *indev = lv_indev_get_act();
    const lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    lv_indev_wait_release(indev);
    return dir;
}

nav_dir_t ui_nav_gesture_dir(lv_event_t *e)
{
    return ui_nav_dir_from_lv(ui_nav_raw_gesture(e));
}

bool ui_nav_go(nav_screen_id_t self, nav_dir_t dir)
{
    nav_step_t step;
    if (!nav_map_step(self, dir, &step)) return false;
    if (!nav_screen_is_valid(step.target)) return false;

    const ui_screen_ref_t *ref = &k_screen_refs[step.target];
    if (ref->get_ptr == NULL || ref->init == NULL) return false;

    const bool animated = (step.anim == NAV_ANIM_MOVE);
    _ui_screen_change(ref->get_ptr(),
                      animated ? nav_load_anim(dir) : LV_SCR_LOAD_ANIM_NONE,
                      animated ? (int)APP_NAV_ANIM_MS : 0,
                      (int)APP_NAV_ANIM_DELAY_MS,
                      ref->init);
    return true;
}

void ui_nav_event_cb(lv_event_t *e)
{
    const nav_dir_t dir = ui_nav_gesture_dir(e);
    if (dir == NAV_DIR_NONE) return;
    (void)ui_nav_go(UI_NAV_EVENT_ID(e), dir);
}

static void ui_timer_loaded_cb(lv_event_t *e)
{
    const ui_timer_binding_t *binding = (const ui_timer_binding_t *)lv_event_get_user_data(e);
    if (binding == NULL || binding->handle == NULL || binding->cb == NULL) return;
    if (*binding->handle != NULL) return;

    *binding->handle = lv_timer_create(binding->cb, binding->period_ms, NULL);
}

static void ui_timer_unloaded_cb(lv_event_t *e)
{
    const ui_timer_binding_t *binding = (const ui_timer_binding_t *)lv_event_get_user_data(e);
    if (binding == NULL || binding->handle == NULL) return;
    if (*binding->handle == NULL) return;

    lv_timer_del(*binding->handle);
    *binding->handle = NULL;
}

void ui_timer_attach(lv_obj_t *scr, const ui_timer_binding_t *binding)
{
    if (scr == NULL || binding == NULL) return;

    lv_obj_add_event_cb(scr, ui_timer_loaded_cb, LV_EVENT_SCREEN_LOADED, (void *)binding);
    lv_obj_add_event_cb(scr, ui_timer_unloaded_cb, LV_EVENT_SCREEN_UNLOADED, (void *)binding);
}
