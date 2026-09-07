#include "ui.h"
#include "ui_screen.h"

#include "dashboard_layout.h"

static lv_obj_t *scr   = NULL;  /* 50x50  red */
static lv_obj_t *arc_s = NULL;
static lv_obj_t *arc_m = NULL;  /* 90x90  yellow */
static lv_obj_t *arc_l = NULL;  /* 130x130 green */

static void on_btn_plus(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED_REPEAT) return;
    _ui_arc_increment(arc_l, DASH_ARC_STEP_LARGE);
    _ui_arc_increment(arc_m, DASH_ARC_STEP_MEDIUM);
    _ui_arc_increment(arc_s, DASH_ARC_STEP_SMALL);
}

static void on_btn_minus(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED_REPEAT) return;
    _ui_arc_increment(arc_l, -DASH_ARC_STEP_LARGE);
    _ui_arc_increment(arc_m, -DASH_ARC_STEP_MEDIUM);
    _ui_arc_increment(arc_s, -DASH_ARC_STEP_SMALL);
}

static lv_obj_t *make_arc(lv_obj_t *parent, lv_coord_t size, uint32_t color)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_set_align(arc, LV_ALIGN_CENTER);
    lv_arc_set_value(arc, DASH_ARC_START_VALUE);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    return arc;
}

void screen_dashboard_init(void)
{
    scr = ui_screen_create(NAV_SCREEN_DASHBOARD);

    arc_s = make_arc(scr, DASH_ARC_SMALL_PX,  DASH_ARC_COLOR_SMALL);
    arc_m = make_arc(scr, DASH_ARC_MEDIUM_PX, DASH_ARC_COLOR_MEDIUM);
    arc_l = make_arc(scr, DASH_ARC_LARGE_PX,  DASH_ARC_COLOR_LARGE);

    (void)ui_button_create(scr, DASH_BTN_LABEL_PLUS, DASH_BTN_PX, DASH_BTN_PX,
                           -DASH_BTN_OFFSET_X, DASH_BTN_OFFSET_Y, LV_ALIGN_CENTER, on_btn_plus);
    (void)ui_button_create(scr, DASH_BTN_LABEL_MINUS, DASH_BTN_PX, DASH_BTN_PX,
                           DASH_BTN_OFFSET_X, DASH_BTN_OFFSET_Y, LV_ALIGN_CENTER, on_btn_minus);
}

lv_obj_t **screen_dashboard_get_ptr(void) { return &scr; }
