#include "cube3d.h"

#include <math.h>
#include <stddef.h>

const cube3d_params_t k_cube3d_default_params = {
    /* center_x        */ 180.0f,
    /* center_y        */ 188.0f,
    /* arc_center_y    */ 180.0f,
    /* min_scale       */  52.0f,
    /* max_scale       */ 138.0f,
    /* rot_gain        */  0.012f,
    /* rot_damping     */  0.942f,
    /* zoom_damping    */  0.885f,
    /* inertia_eps     */  0.0008f,
    /* zoom_eps        */  0.05f,
    /* perspective     */  3.8f,
    /* arc_radius      */  142.0f,
    /* arc_start_deg   */  315.0f,
    /* arc_end_deg     */  405.0f,
    /* arc_hit_width   */  26.0f,
    /* arc_zone_slop   */  12.0f,
    /* arc_knob_size   */  8.0f,
    /* arc_rail_width  */  8.0f,
    /* nav_edge_px     */  46,
    /* nav_swipe_min_px*/  54,
    /* nav_swipe_slop  */  30,
    /* screen_px       */  360,
};

const cube3d_state_t k_cube3d_default_state = {
    /* rot_x */ -0.48f,
    /* rot_y */  0.72f,
    /* vel_x */  0.0f,
    /* vel_y */  0.0f,
    /* scale */  95.0f,   /* (min_scale + max_scale) / 2 */
    /* zoom_vel */ 0.0f,
};

const cube3d_vec3_t k_cube3d_vertices[CUBE3D_VERTEX_COUNT] = {
    { -1.0f, -1.0f, -1.0f }, {  1.0f, -1.0f, -1.0f },
    {  1.0f,  1.0f, -1.0f }, { -1.0f,  1.0f, -1.0f },
    { -1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f,  1.0f },
};

const uint8_t k_cube3d_faces[CUBE3D_FACE_COUNT][CUBE3D_FACE_CORNERS] = {
    { 0, 1, 2, 3 },
    { 4, 7, 6, 5 },
    { 0, 4, 5, 1 },
    { 3, 2, 6, 7 },
    { 1, 5, 6, 2 },
    { 0, 3, 7, 4 },
};

const uint32_t k_cube3d_face_colors[CUBE3D_FACE_COUNT] = {
    0x2F80ED, 0xEB5757, 0xF2C94C, 0x27AE60, 0x9B51E0, 0x56CCF2,
};

/* 与旧版一致：只做一次 ±2π 修正，不做完整归一化 */
static float wrap_once(float rad)
{
    if (rad > UI_PI_F) rad -= UI_TWO_PI_F;
    if (rad < -UI_PI_F) rad += UI_TWO_PI_F;
    return rad;
}

static float arc_angle_deg(const cube3d_params_t *p, int x, int y)
{
    float deg = ui_rad_to_deg(atan2f((float)y - p->arc_center_y, (float)x - p->center_x));
    if (deg < 0.0f) deg += UI_DEG_FULL;
    if (deg < p->arc_start_deg) deg += UI_DEG_FULL;
    return deg;
}

float cube3d_default_scale(const cube3d_params_t *p)
{
    if (p == NULL) return 0.0f;
    return (p->min_scale + p->max_scale) * UI_HALF_F;
}

float cube3d_scale_clamped(const cube3d_params_t *p, float scale)
{
    if (p == NULL) return scale;
    return ui_clampf(scale, p->min_scale, p->max_scale);
}

float cube3d_zoom_percent_f(const cube3d_params_t *p, float scale)
{
    if (p == NULL) return 0.0f;
    return ui_unit_span(scale, p->min_scale, p->max_scale);
}

float cube3d_zoom_ratio(const cube3d_params_t *p, float scale)
{
    if (p == NULL || p->max_scale <= p->min_scale) return 0.0f;
    return (scale - p->min_scale) / (p->max_scale - p->min_scale);
}

int cube3d_zoom_percent(const cube3d_params_t *p, float scale)
{
    if (p == NULL || p->max_scale <= p->min_scale) return CUBE3D_ZOOM_MIN_PCT;
    return (int)(((scale - p->min_scale) * (float)UI_PCT_FULL) /
                 (p->max_scale - p->min_scale) + UI_HALF_F);
}

bool cube3d_is_nav_zone(const cube3d_params_t *p, int x, int y)
{
    if (p == NULL) return false;

    const int far_edge = p->screen_px - p->nav_edge_px;
    return x < p->nav_edge_px || x > far_edge || y < p->nav_edge_px || y > far_edge;
}

bool cube3d_is_zoom_zone(const cube3d_params_t *p, int x, int y)
{
    if (p == NULL) return false;

    const float dx = (float)x - p->center_x;
    const float dy = (float)y - p->arc_center_y;
    const float r = ui_vector_len(dx, dy);
    const float deg = arc_angle_deg(p, x, y);

    return deg >= (p->arc_start_deg - p->arc_zone_slop) &&
           deg <= (p->arc_end_deg + p->arc_zone_slop) &&
           fabsf(r - p->arc_radius) <= p->arc_hit_width;
}

float cube3d_scale_from_point(const cube3d_params_t *p, int x, int y)
{
    if (p == NULL) return 0.0f;

    const float deg = arc_angle_deg(p, x, y);
    const float pct = ui_clampf(deg, p->arc_start_deg, p->arc_end_deg) - p->arc_start_deg;
    const float span = p->arc_end_deg - p->arc_start_deg;
    const float norm = (span > 0.0f) ? (pct / span) : 0.0f;

    return p->min_scale + norm * (p->max_scale - p->min_scale);
}

void cube3d_apply_rotate(cube3d_state_t *s, const cube3d_params_t *p, int dx, int dy)
{
    if (s == NULL || p == NULL) return;

    s->vel_x = (float)-dy * p->rot_gain;
    s->vel_y = (float)dx * p->rot_gain;
    s->rot_x += s->vel_x;
    s->rot_y += s->vel_y;
}

void cube3d_apply_zoom(cube3d_state_t *s, const cube3d_params_t *p, int x, int y)
{
    if (s == NULL || p == NULL) return;

    const float next = cube3d_scale_from_point(p, x, y);
    s->zoom_vel = next - s->scale;
    s->scale = next;
}

void cube3d_tick_inertia(cube3d_state_t *s, const cube3d_params_t *p, bool dragging)
{
    if (s == NULL || p == NULL) return;

    if (!dragging) {
        s->rot_x += s->vel_x;
        s->rot_y += s->vel_y;
        s->scale = cube3d_scale_clamped(p, s->scale + s->zoom_vel);

        s->vel_x *= p->rot_damping;
        s->vel_y *= p->rot_damping;
        s->zoom_vel *= p->zoom_damping;

        if (fabsf(s->vel_x) < p->inertia_eps) s->vel_x = 0.0f;
        if (fabsf(s->vel_y) < p->inertia_eps) s->vel_y = 0.0f;
        if (fabsf(s->zoom_vel) < p->zoom_eps) s->zoom_vel = 0.0f;
    }

    s->rot_x = wrap_once(s->rot_x);
    s->rot_y = wrap_once(s->rot_y);
}

bool cube3d_is_nav_swipe(const cube3d_params_t *p, int total_dx, int total_dy)
{
    if (p == NULL) return false;
    return ui_abs_i(total_dx) >= p->nav_swipe_min_px &&
           ui_abs_i(total_dy) <= p->nav_swipe_slop_px;
}

void cube3d_stop_motion(cube3d_state_t *s)
{
    if (s == NULL) return;
    s->vel_x = 0.0f;
    s->vel_y = 0.0f;
    s->zoom_vel = 0.0f;
}

void cube3d_reset(cube3d_state_t *s, const cube3d_params_t *p)
{
    if (s == NULL || p == NULL) return;

    s->rot_x = k_cube3d_default_state.rot_x;
    s->rot_y = k_cube3d_default_state.rot_y;
    s->scale = cube3d_default_scale(p);
    cube3d_stop_motion(s);
}

void cube3d_project(const cube3d_state_t *s, const cube3d_params_t *p,
                    cube3d_point_t out[CUBE3D_VERTEX_COUNT])
{
    if (s == NULL || p == NULL || out == NULL) return;

    const float sx = sinf(s->rot_x);
    const float cx = cosf(s->rot_x);
    const float sy = sinf(s->rot_y);
    const float cy = cosf(s->rot_y);

    for (int i = 0; i < CUBE3D_VERTEX_COUNT; i++) {
        const float x = k_cube3d_vertices[i].x;
        const float y = k_cube3d_vertices[i].y;
        const float z = k_cube3d_vertices[i].z;

        const float y1 = y * cx - z * sx;
        const float z1 = y * sx + z * cx;
        const float x2 = x * cy + z1 * sy;
        const float z2 = -x * sy + z1 * cy;

        const float perspective = p->perspective / (p->perspective + z2);
        out[i].x = (int16_t)(p->center_x + x2 * s->scale * perspective);
        out[i].y = (int16_t)(p->center_y + y1 * s->scale * perspective);
        out[i].z = z2;
    }
}

void cube3d_sort_faces(const cube3d_point_t points[CUBE3D_VERTEX_COUNT],
                       cube3d_face_order_t out[CUBE3D_FACE_COUNT])
{
    if (points == NULL || out == NULL) return;

    for (int i = 0; i < CUBE3D_FACE_COUNT; i++) {
        float z = 0.0f;
        for (int j = 0; j < CUBE3D_FACE_CORNERS; j++) {
            z += points[k_cube3d_faces[i][j]].z;
        }
        out[i].idx = i;
        out[i].z = z * CUBE3D_Z_FACE_SCALE;
    }

    for (int i = 1; i < CUBE3D_FACE_COUNT; i++) {
        cube3d_face_order_t item = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].z > item.z) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = item;
    }
}
