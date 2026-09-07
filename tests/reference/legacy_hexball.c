/* 重构前 screen_agent.c 的物理模型，逐字搬运（只把 lv_rand 换成注入式随机源）。
 * 用途：差分测试 —— 断言 core/hexball.c 与它在相同输入下结果完全一致。
 * 不参与固件构建，也不计入覆盖率。 */
#include "legacy_hexball.h"

#include <math.h>

#define CX        180.0f       /* screen centre */
#define CY        180.0f
#define SCREEN_R  172.0f       /* usable radius on round display */
#define BALL_R      8.0f
#define SPEED       2.2f
#define NUM_RINGS   3
#define PIf         3.14159265f
#define SHRINK_SPEED  0.25f      /* px/frame each ring shrinks */
#define MIN_RING_R   28.0f       /* ring auto-breaks below this radius */
#define SPAWN_R     (SCREEN_R + 65.f)  /* fixed outer spawn radius */

static const float INIT_R[NUM_RINGS] = { 65.f, 110.f, 155.f };

static void hex_pt(const legacy_ring *r, int v, float *x, float *y)
{
    float a = r->angle + v * (PIf / 3.f);
    *x = CX + r->radius * cosf(a);
    *y = CY + r->radius * sinf(a);
}

/* squared distance from P to segment AB; closest point → (*ox, *oy) */
static float seg_dsq(float ax, float ay, float bx, float by,
                     float px, float py, float *ox, float *oy)
{
    float dx = bx-ax, dy = by-ay;
    float t  = (dx*dx + dy*dy < 1e-9f) ? 0.f :
               fmaxf(0.f, fminf(1.f, ((px-ax)*dx+(py-ay)*dy)/(dx*dx+dy*dy)));
    *ox = ax + t*dx;  *oy = ay + t*dy;
    float ex = px-*ox, ey = py-*oy;
    return ex*ex + ey*ey;
}

static float vec_len(float x, float y)   { return sqrtf(x*x + y*y); }

/* ring index whose radius is closest to dist, within 35 px */
int legacy_nearest_ring(const legacy_hexball_t *s, float dist)
{
    int best = -1; float bd = 35.f;
    for (int i = 0; i < NUM_RINGS; i++) {
        if (!s->rings[i].alive) continue;
        float d = fabsf(s->rings[i].radius - dist);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static void respawn(legacy_hexball_t *s, int idx, legacy_rand_fn rnd, void *user)
{
    s->rings[idx].radius = SPAWN_R;
    s->rings[idx].angle  = (float)rnd(user, 0, 628) / 100.f;
    s->rings[idx].gap    = (int)rnd(user, 0, 5);
    s->rings[idx].alive  = true;
    s->rings[idx].fresh  = true;
}

void legacy_physics_step(legacy_hexball_t *s, legacy_rand_fn rnd, void *user)
{
    /* ── shrink all rings inward ── */
    for (int i = 0; i < NUM_RINGS; i++) {
        if (!s->rings[i].alive) continue;
        s->rings[i].radius -= SHRINK_SPEED;
        if (s->rings[i].radius < MIN_RING_R) {
            s->score++;
            respawn(s, i, rnd, user);
        }
    }

    for (int i = 0; i < NUM_RINGS; i++) s->rings[i].fresh = false;

    float prev_x = s->ball.x, prev_y = s->ball.y;
    s->ball.x += s->ball.vx;
    s->ball.y += s->ball.vy;

    /* ── bounce off circular screen edge ── */
    float bx = s->ball.x - CX, by = s->ball.y - CY;
    float bd = vec_len(bx, by);
    if (bd + BALL_R > SCREEN_R && bd > 0.f) {
        float nx = bx/bd, ny = by/bd;
        float dot = s->ball.vx*nx + s->ball.vy*ny;
        s->ball.vx -= 2.f*dot*nx;
        s->ball.vy -= 2.f*dot*ny;
        float push = bd + BALL_R - SCREEN_R;
        s->ball.x -= nx*push;  s->ball.y -= ny*push;
    }

    /* ── bounce off solid hexagon sides ── */
    for (int i = 0; i < NUM_RINGS; i++) {
        if (!s->rings[i].alive) continue;
        for (int st = 0; st < 6; st++) {
            if (st == s->rings[i].gap) continue;
            float ax, ay, bxx, byy, cx, cy;
            hex_pt(&s->rings[i], st,     &ax,  &ay);
            hex_pt(&s->rings[i], (st+1)%6, &bxx, &byy);
            float d2 = seg_dsq(ax, ay, bxx, byy, s->ball.x, s->ball.y, &cx, &cy);
            if (d2 < BALL_R * BALL_R) {
                float nx = s->ball.x-cx, ny = s->ball.y-cy;
                float nl = vec_len(nx, ny) + 1e-9f;
                nx /= nl;  ny /= nl;
                float dot = s->ball.vx*nx + s->ball.vy*ny;
                if (dot < 0.f) {
                    s->ball.vx -= 2.f*dot*nx;
                    s->ball.vy -= 2.f*dot*ny;
                }
                float pen = BALL_R - sqrtf(d2);
                s->ball.x += nx*pen;  s->ball.y += ny*pen;
            }
        }
    }

    /* ── break detection: ball crossed ring radius → passed through gap ── */
    float pd = vec_len(prev_x-CX, prev_y-CY);
    float cd = vec_len(s->ball.x -CX, s->ball.y -CY);
    for (int i = 0; i < NUM_RINGS; i++) {
        if (!s->rings[i].alive || s->rings[i].fresh) continue;
        float r = s->rings[i].radius;
        if ((pd < r && cd >= r) || (pd > r && cd <= r)) {
            s->rings[i].alive = false;
            s->score++;
            respawn(s, i, rnd, user);
        }
    }
}

void legacy_reset(legacy_hexball_t *s)
{
    s->ball.x  = CX;  s->ball.y  = CY;
    s->ball.vx = SPEED * cosf(PIf / 6.f);
    s->ball.vy = SPEED * sinf(PIf / 6.f);
    s->score   = 0;
    for (int i = 0; i < NUM_RINGS; i++) {
        s->rings[i].radius = INIT_R[i];
        s->rings[i].angle  = i * (PIf / 9.f);
        s->rings[i].gap    = i * 2;          /* gaps on different sides */
        s->rings[i].alive  = true;
        s->rings[i].fresh  = false;
    }
}
