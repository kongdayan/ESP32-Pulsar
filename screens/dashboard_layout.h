/*
 * dashboard_layout.h — 首页（三层同心弧度盘）的视图常量。
 * 只放"这块屏长什么样"的数据，逻辑常量在 core/ 下。
 */
#ifndef SCREENS_DASHBOARD_LAYOUT_H
#define SCREENS_DASHBOARD_LAYOUT_H

/* 三层弧度盘：直径（px）与指示器颜色 */
#define DASH_ARC_SMALL_PX      50
#define DASH_ARC_MEDIUM_PX     90
#define DASH_ARC_LARGE_PX      130
#define DASH_ARC_COLOR_SMALL   0xFF6666u
#define DASH_ARC_COLOR_MEDIUM  0xFFFF66u
#define DASH_ARC_COLOR_LARGE   0x99CC66u
#define DASH_ARC_START_VALUE   50

/* +/- 按钮：长按连发时每一档的增量 */
#define DASH_ARC_STEP_SMALL    2
#define DASH_ARC_STEP_MEDIUM   4
#define DASH_ARC_STEP_LARGE    6

#define DASH_BTN_PX            50
#define DASH_BTN_OFFSET_X      100
#define DASH_BTN_OFFSET_Y      (-100)
#define DASH_BTN_LABEL_PLUS    "+"
#define DASH_BTN_LABEL_MINUS   "-"

#endif /* SCREENS_DASHBOARD_LAYOUT_H */
