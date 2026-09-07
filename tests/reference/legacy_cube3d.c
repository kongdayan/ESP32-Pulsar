/* 重构前 screen_3dmodel.c 的数学部分，逐字搬运（lv_point_t 换成 x/y 两个整数）。
 * 用途：差分测试 —— 断言 core/cube3d.c 与它结果完全一致。
 * 不参与固件构建，也不计入覆盖率。 */
#include "legacy_cube3d.h"

#include <math.h>
#include <stdlib.h>

#define CX              180.0f
#define CY              188.0f
#define CUBE_MIN_SCALE   52.0f
#define CUBE_MAX_SCALE  138.0f
#define ROT_GAIN          0.012f
#define ROT_DAMPING       0.942f
#define ZOOM_DAMPING      0.885f
#define INERTIA_EPS       0.0008f
#define ZOOM_EPS          0.05f
#define PI_F              3.14159265f
#define NAV_EDGE_PX       46
#define NAV_SWIPE_MIN_PX  54
#define NAV_SWIPE_SLOP_PX 30
#define ZOOM_ARC_R       142.0f
#define ZOOM_ARC_START   315.0f
#define ZOOM_ARC_END     405.0f
#define ZOOM_ARC_HIT_W    26.0f

static const legacy_vec3 cube_vertices[8] = {
    { -1.0f, -1.0f, -1.0f }, {  1.0f, -1.0f, -1.0f },
    {  1.0f,  1.0f, -1.0f }, { -1.0f,  1.0f, -1.0f },
    { -1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f,  1.0f },
};

static const uint8_t cube_faces[6][4] = {
    { 0, 1, 2, 3 },
    { 4, 7, 6, 5 },
    { 0, 4, 5, 1 },
    { 3, 2, 6, 7 },
    { 1, 5, 6, 2 },
    { 0, 3, 7, 4 },
};

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool legacy_is_nav_zone(int px, int py)
{
    return px < NAV_EDGE_PX || px > (360 - NAV_EDGE_PX) ||
           py < NAV_EDGE_PX || py > (360 - NAV_EDGE_PX);
}

bool legacy_is_zoom_zone(int px, int py)
{
    float dx = (float)px - CX;
    float dy = (float)py - 180.0f;
    float r = sqrtf(dx * dx + dy * dy);
    float deg = atan2f(dy, dx) * 180.0f / PI_F;
    if (deg < 0.0f) deg += 360.0f;
    if (deg < ZOOM_ARC_START) deg += 360.0f;

    return deg >= (ZOOM_ARC_START - 12.0f) &&
           deg <= (ZOOM_ARC_END + 12.0f) &&
           fabsf(r - ZOOM_ARC_R) <= ZOOM_ARC_HIT_W;
}

float legacy_zoom_scale_from_point(int px, int py)
{
    float dx = (float)px - CX;
    float dy = (float)py - 180.0f;
    float deg = atan2f(dy, dx) * 180.0f / PI_F;
    if (deg < 0.0f) deg += 360.0f;
    if (deg < ZOOM_ARC_START) deg += 360.0f;
    float pct = (clampf(deg, ZOOM_ARC_START, ZOOM_ARC_END) - ZOOM_ARC_START) /
                (ZOOM_ARC_END - ZOOM_ARC_START);

    return CUBE_MIN_SCALE + pct * (CUBE_MAX_SCALE - CUBE_MIN_SCALE);
}

void legacy_project_cube(const legacy_cube_state *s, legacy_projected out[8])
{
    float sx = sinf(s->rot_x), cx = cosf(s->rot_x);
    float sy = sinf(s->rot_y), cy = cosf(s->rot_y);

    for (int i = 0; i < 8; i++) {
        float x = cube_vertices[i].x;
        float y = cube_vertices[i].y;
        float z = cube_vertices[i].z;

        float y1 = y * cx - z * sx;
        float z1 = y * sx + z * cx;
        float x2 = x * cy + z1 * sy;
        float z2 = -x * sy + z1 * cy;

        float perspective = 3.8f / (3.8f + z2);
        out[i].x = (int16_t)(CX + x2 * s->cube_scale * perspective);
        out[i].y = (int16_t)(CY + y1 * s->cube_scale * perspective);
        out[i].z = z2;
    }
}

void legacy_sort_faces(const legacy_projected points[8], legacy_face_order order[6])
{
    for (int i = 0; i < 6; i++) {
        float z = 0.0f;
        for (int j = 0; j < 4; j++) z += points[cube_faces[i][j]].z;
        order[i].idx = i;
        order[i].z = z * 0.25f;
    }

    for (int i = 1; i < 6; i++) {
        legacy_face_order item = order[i];
        int j = i - 1;
        while (j >= 0 && order[j].z > item.z) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = item;
    }
}

int legacy_zoom_percent(float cube_scale)
{
    return (int)(((cube_scale - CUBE_MIN_SCALE) * 100.0f) /
                 (CUBE_MAX_SCALE - CUBE_MIN_SCALE) + 0.5f);
}

void legacy_inertia_tick(legacy_cube_state *s, bool dragging)
{
    if (!dragging) {
        s->rot_x += s->vel_x;
        s->rot_y += s->vel_y;
        s->cube_scale = clampf(s->cube_scale + s->zoom_vel, CUBE_MIN_SCALE, CUBE_MAX_SCALE);

        s->vel_x *= ROT_DAMPING;
        s->vel_y *= ROT_DAMPING;
        s->zoom_vel *= ZOOM_DAMPING;

        if (fabsf(s->vel_x) < INERTIA_EPS) s->vel_x = 0.0f;
        if (fabsf(s->vel_y) < INERTIA_EPS) s->vel_y = 0.0f;
        if (fabsf(s->zoom_vel) < ZOOM_EPS) s->zoom_vel = 0.0f;
    }

    if (s->rot_x > PI_F) s->rot_x -= 2.0f * PI_F;
    if (s->rot_x < -PI_F) s->rot_x += 2.0f * PI_F;
    if (s->rot_y > PI_F) s->rot_y -= 2.0f * PI_F;
    if (s->rot_y < -PI_F) s->rot_y += 2.0f * PI_F;
}

void legacy_drag_rotate(legacy_cube_state *s, int dx, int dy)
{
    float rx = (float)-dy * ROT_GAIN;
    float ry = (float)dx * ROT_GAIN;
    s->rot_x += rx;
    s->rot_y += ry;
    s->vel_x = rx;
    s->vel_y = ry;
}

void legacy_drag_zoom(legacy_cube_state *s, int x, int y)
{
    float next_scale = legacy_zoom_scale_from_point(x, y);
    s->zoom_vel = next_scale - s->cube_scale;
    s->cube_scale = next_scale;
}

bool legacy_swipe_navigates(int total_dx, int total_dy)
{
    return abs(total_dx) >= NAV_SWIPE_MIN_PX && abs(total_dy) <= NAV_SWIPE_SLOP_PX;
}
