#include "display_rotation.h"

#include <stddef.h>

bool disp_rotation_resolve(uint32_t rotation, disp_rotation_t *out)
{
    if (out == NULL || rotation > DISP_ROTATION_MAX) return false;

    switch (rotation) {
    case 1u:
        out->swap_xy = true;  out->mirror_x = true;  out->mirror_y = false;
        break;
    case 2u:
        out->swap_xy = false; out->mirror_x = true;  out->mirror_y = true;
        break;
    case 3u:
        out->swap_xy = true;  out->mirror_x = false; out->mirror_y = true;
        break;
    default: /* 0u */
        out->swap_xy = false; out->mirror_x = false; out->mirror_y = false;
        break;
    }
    return true;
}
