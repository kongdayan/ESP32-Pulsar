/*
 * display_rotation.h — 旋转值 → swap/mirror 组合（纯逻辑，可单元测试）。
 */
#ifndef CORE_DISPLAY_ROTATION_H
#define CORE_DISPLAY_ROTATION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISP_ROTATION_MAX 3u /* 支持 0/1/2/3 */

typedef struct {
    bool swap_xy;
    bool mirror_x;
    bool mirror_y;
} disp_rotation_t;

/* rotation > DISP_ROTATION_MAX 或参数为空时返回 false，且不写 *out */
bool disp_rotation_resolve(uint32_t rotation, disp_rotation_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CORE_DISPLAY_ROTATION_H */
