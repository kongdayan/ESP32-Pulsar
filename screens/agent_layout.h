/*
 * agent_layout.h — Hex-Ball 小游戏屏的视图常量。
 * 物理/几何参数在 core/hexball.h（k_hexball_default_params）。
 */
#ifndef SCREENS_AGENT_LAYOUT_H
#define SCREENS_AGENT_LAYOUT_H

#include "hexball.h"

/* 环描边宽度 */
#define AGENT_LINE_WIDTH       4
#define AGENT_BG_COLOR         0x000000u
#define AGENT_BALL_COLOR       0xFFFFFFu
#define AGENT_SCORE_COLOR      0xFFFFFFu

/* 分数文本绘制区域与格式 */
#define AGENT_SCORE_AREA_X1    10
#define AGENT_SCORE_AREA_Y1    8
#define AGENT_SCORE_AREA_X2    170
#define AGENT_SCORE_AREA_Y2    26
#define AGENT_SCORE_BUF_SIZE   24
#define AGENT_SCORE_FMT        "Score: %d"

/* 三条环的描边颜色，顺序与 HEXBALL_RING_COUNT 对应 */
static const uint32_t k_agent_ring_colors[HEXBALL_RING_COUNT] = {
    0x00BFFF, 0xFF8C00, 0x39FF14,
};

#endif /* SCREENS_AGENT_LAYOUT_H */
