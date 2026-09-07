/* 重构前 screens/screen_3dmodel.c 的数学部分，逐字搬运。
 * 用途：差分测试基准。不参与固件构建，不计入覆盖率。 */
#ifndef TESTS_LEGACY_CUBE3D_H
#define TESTS_LEGACY_CUBE3D_H

#include <stdbool.h>
#include <stdint.h>

typedef struct { float x, y, z; } legacy_vec3;
typedef struct { int16_t x, y; float z; } legacy_projected;
typedef struct { int idx; float z; } legacy_face_order;

typedef struct {
    float rot_x, rot_y, vel_x, vel_y, cube_scale, zoom_vel;
} legacy_cube_state;

void legacy_project_cube(const legacy_cube_state *s, legacy_projected out[8]);
void legacy_sort_faces(const legacy_projected points[8], legacy_face_order order[6]);
bool legacy_is_nav_zone(int x, int y);
bool legacy_is_zoom_zone(int x, int y);
float legacy_zoom_scale_from_point(int x, int y);
int legacy_zoom_percent(float cube_scale);
void legacy_inertia_tick(legacy_cube_state *s, bool dragging);
void legacy_drag_rotate(legacy_cube_state *s, int dx, int dy);
void legacy_drag_zoom(legacy_cube_state *s, int x, int y);
bool legacy_swipe_navigates(int total_dx, int total_dy);

#endif /* TESTS_LEGACY_CUBE3D_H */
