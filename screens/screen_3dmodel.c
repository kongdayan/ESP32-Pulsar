#include "ui.h"
#include "ui_screen.h"

#include <math.h>

#include "app_config.h"
#include "cube3d.h"
#include "model3d_layout.h"
#include "ui_math.h"

static lv_obj_t   *scr        = NULL;
static lv_obj_t   *model      = NULL;
static lv_obj_t   *zoom_label = NULL;
static lv_timer_t *ticker     = NULL;

static cube3d_state_t state;   /* 初值由 cube3d_reset() 在屏幕首次创建时写入 */

/* 视图层输入状态 */
static bool        dragging     = false;
static bool        zooming      = false;
static bool        nav_candidate = false;
static lv_point_t  last_pt;
static lv_point_t  press_pt;

static void on_tick(lv_timer_t *t);

static const ui_timer_binding_t s_model_timer = {
    on_tick, APP_GAME_TICK_MS, &ticker,
};

static void update_zoom_label(void)
{
    if (zoom_label == NULL) return;

    char buf[M3D_ZOOM_BUF_SIZE];
    lv_snprintf(buf, sizeof(buf), M3D_ZOOM_FMT, cube3d_zoom_percent(&k_cube3d_default_params,
                                                                    state.scale));
    lv_label_set_text(zoom_label, buf);
}

static void draw_zoom_rail(lv_draw_ctx_t *dc)
{
    const float pct = cube3d_zoom_ratio(&k_cube3d_default_params, state.scale);
    const float span = k_cube3d_default_params.arc_end_deg - k_cube3d_default_params.arc_start_deg;
    const float value_deg = k_cube3d_default_params.arc_start_deg + pct * span;

    const lv_point_t center = { (lv_coord_t)k_cube3d_default_params.center_x,
                                (lv_coord_t)k_cube3d_default_params.arc_center_y };

    lv_draw_arc_dsc_t rail;
    lv_draw_arc_dsc_init(&rail);
    rail.color = lv_color_hex(M3D_RAIL_COLOR);
    rail.opa = M3D_RAIL_OPA;
    rail.width = (uint16_t)k_cube3d_default_params.arc_rail_width;
    rail.rounded = M3D_ARC_ROUNDED;
    lv_draw_arc(dc, &rail, &center, (uint16_t)k_cube3d_default_params.arc_radius,
                (uint16_t)k_cube3d_default_params.arc_start_deg,
                (uint16_t)k_cube3d_default_params.arc_end_deg);

    lv_draw_arc_dsc_t fill;
    lv_draw_arc_dsc_init(&fill);
    fill.color = lv_color_hex(M3D_FILL_COLOR);
    fill.opa = M3D_FILL_OPA;
    fill.width = rail.width;
    fill.rounded = M3D_ARC_ROUNDED;
    lv_draw_arc(dc, &fill, &center, (uint16_t)k_cube3d_default_params.arc_radius,
                (uint16_t)k_cube3d_default_params.arc_start_deg, (uint16_t)value_deg);

    const float knob_deg = ui_deg_to_rad(value_deg);
    const lv_coord_t kx = (lv_coord_t)(k_cube3d_default_params.center_x +
                                       cosf(knob_deg) * k_cube3d_default_params.arc_radius);
    const lv_coord_t ky = (lv_coord_t)(k_cube3d_default_params.arc_center_y +
                                       sinf(knob_deg) * k_cube3d_default_params.arc_radius);
    const lv_coord_t knob = (lv_coord_t)k_cube3d_default_params.arc_knob_size;

    lv_draw_rect_dsc_t knob_dsc;
    lv_draw_rect_dsc_init(&knob_dsc);
    knob_dsc.radius = M3D_KNOB_RADIUS;
    knob_dsc.bg_color = lv_color_hex(M3D_KNOB_COLOR);
    knob_dsc.bg_opa = LV_OPA_COVER;
    const lv_area_t knob_area = { kx - knob, ky - knob, kx + knob, ky + knob };
    lv_draw_rect(dc, &knob_dsc, &knob_area);
}

static void draw_cube(lv_draw_ctx_t *dc)
{
    cube3d_point_t points[CUBE3D_VERTEX_COUNT];
    cube3d_face_order_t order[CUBE3D_FACE_COUNT];
    cube3d_project(&state, &k_cube3d_default_params, points);
    cube3d_sort_faces(points, order);

    lv_draw_rect_dsc_t face_dsc;
    lv_draw_rect_dsc_init(&face_dsc);
    face_dsc.bg_opa = M3D_FACE_OPA;

    lv_draw_line_dsc_t edge_dsc;
    lv_draw_line_dsc_init(&edge_dsc);
    edge_dsc.color = lv_color_hex(M3D_EDGE_COLOR);
    edge_dsc.opa = M3D_EDGE_OPA;
    edge_dsc.width = M3D_EDGE_WIDTH;
    edge_dsc.round_start = M3D_ARC_ROUNDED;
    edge_dsc.round_end = M3D_ARC_ROUNDED;

    for (int i = 0; i < CUBE3D_FACE_COUNT; i++) {
        const int f = order[i].idx;
        lv_point_t poly[CUBE3D_FACE_CORNERS];
        for (int j = 0; j < CUBE3D_FACE_CORNERS; j++) {
            poly[j].x = points[k_cube3d_faces[f][j]].x;
            poly[j].y = points[k_cube3d_faces[f][j]].y;
        }

        face_dsc.bg_color = lv_color_hex(k_cube3d_face_colors[f]);
        lv_draw_polygon(dc, &face_dsc, poly, CUBE3D_FACE_CORNERS);

        for (int j = 0; j < CUBE3D_FACE_CORNERS; j++) {
            lv_draw_line(dc, &edge_dsc, &poly[j], &poly[(j + 1) % CUBE3D_FACE_CORNERS]);
        }
    }
}

static void on_draw(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_POST_BEGIN) return;
    lv_draw_ctx_t *dc = lv_event_get_draw_ctx(e);

    draw_zoom_rail(dc);
    draw_cube(dc);
}

static void on_input(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();

    lv_point_t pt = { 0, 0 };
    if (indev != NULL) lv_indev_get_point(indev, &pt);

    if (code == LV_EVENT_GESTURE) {
        const nav_dir_t dir = ui_nav_gesture_dir(e);
        if (nav_candidate) (void)ui_nav_go(NAV_SCREEN_MODEL3D, dir);
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        zooming = cube3d_is_zoom_zone(&k_cube3d_default_params, pt.x, pt.y);
        nav_candidate = !zooming && cube3d_is_nav_zone(&k_cube3d_default_params, pt.x, pt.y);
        dragging = !nav_candidate;
        press_pt = pt;
        last_pt = pt;
        cube3d_stop_motion(&state);
        return;
    }

    if (code == LV_EVENT_PRESSING && dragging) {
        const int dx = pt.x - last_pt.x;
        const int dy = pt.y - last_pt.y;

        if (zooming) {
            cube3d_apply_zoom(&state, &k_cube3d_default_params, pt.x, pt.y);
            update_zoom_label();
        } else {
            cube3d_apply_rotate(&state, &k_cube3d_default_params, dx, dy);
        }

        last_pt = pt;
        lv_obj_invalidate(model);
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (nav_candidate && code == LV_EVENT_RELEASED) {
            const int total_dx = pt.x - press_pt.x;
            const int total_dy = pt.y - press_pt.y;

            if (cube3d_is_nav_swipe(&k_cube3d_default_params, total_dx, total_dy)) {
                lv_indev_wait_release(indev);
                (void)ui_nav_go(NAV_SCREEN_MODEL3D,
                                (total_dx < 0) ? NAV_DIR_LEFT : NAV_DIR_RIGHT);
            }
        }

        dragging = false;
        zooming = false;
        nav_candidate = false;
    }
}

static void on_tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    cube3d_tick_inertia(&state, &k_cube3d_default_params, dragging);
    update_zoom_label();
    lv_obj_invalidate(model);
}

static void on_reset(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    cube3d_reset(&state, &k_cube3d_default_params);
    update_zoom_label();
    lv_obj_invalidate(model);
}

void screen_3dmodel_init(void)
{
    /* 手势与拖拽都在 on_input 里统一处理，屏幕根对象不再挂导航 */
    cube3d_reset(&state, &k_cube3d_default_params);

    scr = ui_screen_create_ex(NAV_SCREEN_MODEL3D, false);
    ui_screen_set_bg(scr, M3D_BG_COLOR);
    ui_timer_attach(scr, &s_model_timer);

    model = ui_fullscreen_layer_create(scr, true);
    lv_obj_add_event_cb(model, on_draw, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(model, on_input, LV_EVENT_ALL, NULL);

    (void)ui_label_create_aligned(scr, M3D_TITLE_TEXT, M3D_TEXT_COLOR, NULL,
                                  M3D_TITLE_X, M3D_TITLE_Y, LV_ALIGN_TOP_MID);
    /* 按钮上也挂 on_input：按住按钮拖动同样能旋转模型（与旧版一致） */
    lv_obj_t *reset_btn = ui_button_create(scr, M3D_RESET_TEXT, M3D_RESET_BTN_W, M3D_RESET_BTN_H,
                                           M3D_RESET_BTN_X, M3D_RESET_BTN_Y, LV_ALIGN_TOP_MID,
                                           on_reset);
    lv_obj_add_event_cb(reset_btn, on_input, LV_EVENT_ALL, NULL);

    zoom_label = ui_label_create_aligned(scr, "", M3D_TEXT_COLOR, &lv_font_montserrat_10,
                                        M3D_ZOOM_LABEL_X, M3D_ZOOM_LABEL_Y, LV_ALIGN_DEFAULT);
    update_zoom_label();
}

lv_obj_t **screen_3dmodel_get_ptr(void) { return &scr; }
