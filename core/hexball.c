#include "hexball.h"

#include <math.h>
#include <stddef.h>

const hexball_params_t k_hexball_default_params = {
    /* center_x          */ 180.0f,
    /* center_y          */ 180.0f,
    /* screen_radius     */ 172.0f,
    /* ball_radius       */   8.0f,
    /* speed             */   2.2f,
    /* shrink_speed      */   0.25f,
    /* min_ring_radius   */  28.0f,
    /* spawn_radius      */ 237.0f,   /* screen_radius + HEXBALL_SPAWN_EXTRA_R */
    /* grab_tolerance    */  35.0f,
    /* init_radius       */ { 65.0f, 110.0f, 155.0f },
};

/* 点到线段的最近点与平方距离 */
static float segment_closest_dsq(float ax, float ay, float bx, float by,
                                 float px, float py, float *out_x, float *out_y)
{
    const float dx = bx - ax;
    const float dy = by - ay;
    const float len2 = dx * dx + dy * dy;

    float t = 0.0f;
    if (len2 >= UI_EPS_F) {
        t = ui_clampf(((px - ax) * dx + (py - ay) * dy) / len2, 0.0f, 1.0f);
    }

    *out_x = ax + t * dx;
    *out_y = ay + t * dy;

    const float ex = px - *out_x;
    const float ey = py - *out_y;
    return ex * ex + ey * ey;
}

static void respawn(hexball_ring_t *ring, const hexball_params_t *params,
                    hexball_rand_fn rnd, void *user)
{
    ring->radius = params->spawn_radius;
    ring->angle  = (rnd != NULL) ? (float)rnd(user, 0u, HEXBALL_ANGLE_SPAN_UNITS) / HEXBALL_ANGLE_SCALE
                                 : 0.0f;
    ring->gap    = (rnd != NULL) ? (int)rnd(user, 0u, (uint32_t)HEXBALL_LAST_SIDE) : 0;
    ring->alive  = true;
    ring->fresh  = true;
}

void hexball_reset(hexball_game_t *game, const hexball_params_t *params)
{
    if (game == NULL || params == NULL) return;

    game->ball.x  = params->center_x;
    game->ball.y  = params->center_y;
    game->ball.vx = params->speed * cosf(HEXBALL_RESET_ANGLE_RAD);
    game->ball.vy = params->speed * sinf(HEXBALL_RESET_ANGLE_RAD);
    game->score   = HEXBALL_MIN_SCORE;
    game->dragging = false;
    game->drag_ring = -1;
    game->drag_angle = 0.0f;

    for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
        game->ring[i].radius = params->init_radius[i];
        game->ring[i].angle  = (float)i * HEXBALL_STAGGER_ANGLE_RAD;
        game->ring[i].gap    = i * HEXBALL_GAP_STAGGER;
        game->ring[i].alive  = true;
        game->ring[i].fresh  = false;
    }
}

void hexball_vertex(const hexball_ring_t *ring, const hexball_params_t *params,
                    int vertex, float *x, float *y)
{
    if (ring == NULL || params == NULL || x == NULL || y == NULL) return;

    const float a = ring->angle + (float)vertex * HEXBALL_VERTEX_STEP_RAD;
    *x = params->center_x + ring->radius * cosf(a);
    *y = params->center_y + ring->radius * sinf(a);
}

int hexball_ring_at_distance(const hexball_game_t *game, const hexball_params_t *params,
                             float dist)
{
    if (game == NULL || params == NULL) return -1;

    int   best = -1;
    float best_d = params->grab_tolerance;

    for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
        if (!game->ring[i].alive) continue;
        const float d = fabsf(game->ring[i].radius - dist);
        if (d < best_d) {
            best_d = d;
            best   = i;
        }
    }
    return best;
}

void hexball_step(hexball_game_t *game, const hexball_params_t *params,
                  hexball_rand_fn rnd, void *rnd_user)
{
    if (game == NULL || params == NULL) return;

    /* ── 所有环向内收缩，缩到最小半径即算通过并重生 ── */
    for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
        if (!game->ring[i].alive) continue;
        game->ring[i].radius -= params->shrink_speed;
        if (game->ring[i].radius < params->min_ring_radius) {
            game->score++;
            respawn(&game->ring[i], params, rnd, rnd_user);
        }
    }

    for (int i = 0; i < HEXBALL_RING_COUNT; i++) game->ring[i].fresh = false;

    const float prev_x = game->ball.x;
    const float prev_y = game->ball.y;
    game->ball.x += game->ball.vx;
    game->ball.y += game->ball.vy;

    /* ── 圆形屏幕边界弹性反弹 ── */
    const float bx = game->ball.x - params->center_x;
    const float by = game->ball.y - params->center_y;
    const float bd = ui_vector_len(bx, by);
    if (bd + params->ball_radius > params->screen_radius && bd > 0.0f) {
        const float nx = bx / bd;
        const float ny = by / bd;
        const float dot = game->ball.vx * nx + game->ball.vy * ny;
        game->ball.vx -= 2.0f * dot * nx;
        game->ball.vy -= 2.0f * dot * ny;
        const float push = bd + params->ball_radius - params->screen_radius;
        game->ball.x -= nx * push;
        game->ball.y -= ny * push;
    }

    /* ── 六边形实心边弹性反弹 ── */
    for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
        if (!game->ring[i].alive) continue;
        for (int s = 0; s < HEXBALL_SIDES; s++) {
            if (s == game->ring[i].gap) continue;

            float ax, ay, ex, ey, cx, cy;
            hexball_vertex(&game->ring[i], params, s, &ax, &ay);
            hexball_vertex(&game->ring[i], params, (s + 1) % HEXBALL_SIDES, &ex, &ey);

            const float d2 = segment_closest_dsq(ax, ay, ex, ey,
                                                 game->ball.x, game->ball.y, &cx, &cy);
            if (d2 < params->ball_radius * params->ball_radius) {
                float nx = game->ball.x - cx;
                float ny = game->ball.y - cy;
                const float nl = ui_vector_len(nx, ny) + UI_EPS_F;
                nx /= nl;
                ny /= nl;

                const float dot = game->ball.vx * nx + game->ball.vy * ny;
                if (dot < 0.0f) {
                    game->ball.vx -= 2.0f * dot * nx;
                    game->ball.vy -= 2.0f * dot * ny;
                }

                const float pen = params->ball_radius - sqrtf(d2);
                game->ball.x += nx * pen;
                game->ball.y += ny * pen;
            }
        }
    }

    /* ── 破环判定：球在本帧跨越了某个环的半径 ── */
    const float pd = ui_vector_len(prev_x - params->center_x, prev_y - params->center_y);
    const float cd = ui_vector_len(game->ball.x - params->center_x, game->ball.y - params->center_y);

    for (int i = 0; i < HEXBALL_RING_COUNT; i++) {
        if (!game->ring[i].alive || game->ring[i].fresh) continue;

        const float r = game->ring[i].radius;
        if ((pd < r && cd >= r) || (pd > r && cd <= r)) {
            game->ring[i].alive = false;
            game->score++;
            respawn(&game->ring[i], params, rnd, rnd_user);
        }
    }
}

void hexball_begin_drag(hexball_game_t *game, const hexball_params_t *params, float x, float y)
{
    if (game == NULL || params == NULL) return;

    const float dx = x - params->center_x;
    const float dy = y - params->center_y;

    game->dragging   = true;
    game->drag_angle = atan2f(dy, dx);
    game->drag_ring  = hexball_ring_at_distance(game, params, ui_vector_len(dx, dy));
}

void hexball_drag_to(hexball_game_t *game, const hexball_params_t *params, float x, float y)
{
    if (game == NULL || params == NULL) return;
    if (!game->dragging || game->drag_ring < 0 || game->drag_ring >= HEXBALL_RING_COUNT) return;

    const float angle = atan2f(y - params->center_y, x - params->center_x);
    const float delta = ui_angle_delta_rad(game->drag_angle, angle);

    game->ring[game->drag_ring].angle += delta;
    game->drag_angle = angle;
}

void hexball_end_drag(hexball_game_t *game)
{
    if (game == NULL) return;
    game->dragging  = false;
    game->drag_ring = -1;
}

int hexball_score(const hexball_game_t *game)
{
    return (game != NULL) ? game->score : HEXBALL_MIN_SCORE;
}

bool hexball_ring_is_drawable(const hexball_game_t *game, int index)
{
    if (game == NULL || index < 0 || index >= HEXBALL_RING_COUNT) return false;
    return game->ring[index].alive;
}
