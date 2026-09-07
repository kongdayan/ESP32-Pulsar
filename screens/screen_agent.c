/* Hex-Ball game — three rotating hexagonal rings, one gap each.
 * Touch-drag to spin the ring nearest your finger.
 * Ball passes through a gap → ring breaks and respawns at outer edge.
 * Ball hits a solid side → elastic reflection.
 *
 * 物理与计分逻辑在 core/hexball.c，本文件只负责 LVGL 绘制与事件转发。 */

#include "ui.h"
#include "ui_screen.h"

#include "agent_layout.h"
#include "app_config.h"
#include "hexball.h"

static lv_obj_t   *scr    = NULL;
static lv_obj_t   *game   = NULL;   /* 全屏对象：自定义绘制 + 输入 */
static lv_timer_t *ticker = NULL;

static hexball_game_t model;

static void on_tick(lv_timer_t *t);

static const ui_timer_binding_t s_game_timer = {
    on_tick, APP_GAME_TICK_MS, &ticker,
};

static uint32_t lvgl_rand(void *user, uint32_t low, uint32_t high)
{
    LV_UNUSED(user);
    return (uint32_t)lv_rand((int32_t)low, (int32_t)high);
}

/* ── drawing ────────────────────────────────────────────────────────────────── */

static void on_draw(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_POST_BEGIN) return;
    lv_draw_ctx_t *dc = lv_event_get_draw_ctx(e);

    /* hexagon rings */
    lv_draw_line_dsc_t ld;
    lv_draw_line_dsc_init(&ld);
    ld.width = AGENT_LINE_WIDTH;
    ld.round_start = 1;
    ld.round_end = 1;

    for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
        if (!hexball_ring_is_drawable(&model, i)) continue;

        ld.color = lv_color_hex(k_agent_ring_colors[i]);
        for (int s = 0; s < HEXBALL_SIDES; s++) {
            if (s == model.ring[i].gap) continue;

            float ax, ay, bx, by;
            hexball_vertex(&model.ring[i], &k_hexball_default_params, s, &ax, &ay);
            hexball_vertex(&model.ring[i], &k_hexball_default_params,
                           (s + 1) % HEXBALL_SIDES, &bx, &by);

            const lv_point_t p1 = { (lv_coord_t)ax, (lv_coord_t)ay };
            const lv_point_t p2 = { (lv_coord_t)bx, (lv_coord_t)by };
            lv_draw_line(dc, &ld, &p1, &p2);
        }
    }

    /* ball */
    lv_draw_rect_dsc_t rd;
    lv_draw_rect_dsc_init(&rd);
    rd.radius = LV_RADIUS_CIRCLE;
    rd.bg_color = lv_color_hex(AGENT_BALL_COLOR);
    rd.bg_opa   = LV_OPA_COVER;
    const lv_area_t ba = {
        (lv_coord_t)(model.ball.x - k_hexball_default_params.ball_radius),
        (lv_coord_t)(model.ball.y - k_hexball_default_params.ball_radius),
        (lv_coord_t)(model.ball.x + k_hexball_default_params.ball_radius),
        (lv_coord_t)(model.ball.y + k_hexball_default_params.ball_radius),
    };
    lv_draw_rect(dc, &rd, &ba);

    /* score */
    char buf[AGENT_SCORE_BUF_SIZE];
    lv_snprintf(buf, sizeof(buf), AGENT_SCORE_FMT, hexball_score(&model));
    lv_draw_label_dsc_t lbl;
    lv_draw_label_dsc_init(&lbl);
    lbl.color = lv_color_hex(AGENT_SCORE_COLOR);
    lbl.font  = LV_FONT_DEFAULT;
    const lv_area_t la = { AGENT_SCORE_AREA_X1, AGENT_SCORE_AREA_Y1,
                           AGENT_SCORE_AREA_X2, AGENT_SCORE_AREA_Y2 };
    lv_draw_label(dc, &lbl, &la, buf, NULL);
}

/* ── input ──────────────────────────────────────────────────────────────────── */

static void on_input(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_GESTURE) {
        (void)ui_nav_go(NAV_SCREEN_AGENT, ui_nav_gesture_dir(e));
        return;
    }

    lv_point_t pt;
    lv_indev_get_point(lv_indev_get_act(), &pt);

    if (code == LV_EVENT_PRESSED) {
        hexball_begin_drag(&model, &k_hexball_default_params, (float)pt.x, (float)pt.y);
    } else if (code == LV_EVENT_PRESSING) {
        hexball_drag_to(&model, &k_hexball_default_params, (float)pt.x, (float)pt.y);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        hexball_end_drag(&model);
    }
}

/* ── timer ──────────────────────────────────────────────────────────────────── */

static void on_tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    hexball_step(&model, &k_hexball_default_params, lvgl_rand, NULL);
    lv_obj_invalidate(game);
}

/* ── public API ─────────────────────────────────────────────────────────────── */

void screen_agent_init(void)
{
    /* 手势由 on_input 统一处理，这里不再重复挂导航回调 */
    scr = ui_screen_create_ex(NAV_SCREEN_AGENT, false);
    ui_screen_set_bg(scr, AGENT_BG_COLOR);
    ui_timer_attach(scr, &s_game_timer);

    /* 全屏透明对象：负责绘制游戏与接收触摸 */
    game = ui_fullscreen_layer_create(scr, true);
    lv_obj_add_event_cb(game, on_draw,  LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(game, on_input, LV_EVENT_ALL, NULL);

    hexball_reset(&model, &k_hexball_default_params);
}

lv_obj_t **screen_agent_get_ptr(void) { return &scr; }
