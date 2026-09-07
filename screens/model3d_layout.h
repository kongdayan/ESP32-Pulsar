/*
 * model3d_layout.h — 3D 模型屏的视图常量。
 * 投影/缩放/手势判定参数在 core/cube3d.h（k_cube3d_default_params）。
 */
#ifndef SCREENS_MODEL3D_LAYOUT_H
#define SCREENS_MODEL3D_LAYOUT_H

#include "cube3d.h"

/* 面颜色见 core/cube3d.h 的 k_cube3d_face_colors */

/* 背景与文字 */
#define M3D_BG_COLOR           0x10131Au
#define M3D_TEXT_COLOR         0xFFFFFFu
#define M3D_TITLE_TEXT         "3D Model"
#define M3D_TITLE_X            0
#define M3D_TITLE_Y            18
#define M3D_RESET_TEXT         "Reset"
#define M3D_RESET_BTN_W        58
#define M3D_RESET_BTN_H        30
#define M3D_RESET_BTN_X        0
#define M3D_RESET_BTN_Y        315
#define M3D_ZOOM_LABEL_X       292
#define M3D_ZOOM_LABEL_Y       40
#define M3D_ZOOM_FMT           "%d%%"
#define M3D_ZOOM_BUF_SIZE      16

/* 缩放弧 */
#define M3D_RAIL_COLOR         0x2A2D34u
#define M3D_RAIL_OPA           LV_OPA_70
#define M3D_FILL_COLOR         0xFFFFFFu
#define M3D_FILL_OPA           LV_OPA_90
#define M3D_KNOB_COLOR         0xFFFFFFu
#define M3D_EDGE_COLOR         0xEAF2FFu
#define M3D_EDGE_OPA           LV_OPA_90
#define M3D_EDGE_WIDTH         2
#define M3D_FACE_OPA           LV_OPA_80
#define M3D_ARC_ROUNDED        1
#define M3D_KNOB_RADIUS        LV_RADIUS_CIRCLE

#endif /* SCREENS_MODEL3D_LAYOUT_H */
