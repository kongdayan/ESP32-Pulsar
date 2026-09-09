/*
 * usage_face_layout.h — 用量表盘（provider 无关）的视图常量。
 * 数据来自 core/usage_model（BLE 写入），这里只放几何/文案/字体。
 */
#ifndef SCREENS_USAGE_FACE_LAYOUT_H
#define SCREENS_USAGE_FACE_LAYOUT_H

#include "app_config.h"
#include "ui_theme.h"

/* ── 数据来源 / 刷新 ──────────────────────────────────────────────────────── */
/* 真实用量由 BLE 写入（hal/ble_usage.cpp → core/usage_model）。
 * 这里只放“没数据时”的占位文案与刷新/过期阈值（阈值在 core/app_config.h）。 */
#define WF_TEXT_OFFLINE        "NO DATA"

/* ── 文本内容 ─────────────────────────────────────────────────────────────── */
/* 标题由 usage_provider_title(provider) 提供，不再写死 CODEX */
#define WF_TEXT_CURRENT        "CURRENT"
#define WF_TEXT_WEEKLY         "WEEKLY"
#define WF_TEXT_PERCENT        "%"
#define WF_TEXT_LINK_OK        "LINK OK"

/* ── 外圈 ─────────────────────────────────────────────────────────────────── */
#define WF_RING_WIDTH          7
#define WF_RING_RADIUS         180
#define WF_RING_START_DEG      0
#define WF_RING_END_DEG        360
#define WF_TICK_COUNT          148
#define WF_TICK_RADIUS         171.0f
#define WF_TICK_DOT_R          1
#define WF_HOT_LINE_WIDTH      5
#define WF_HOT_LINE_LEN        4
#define WF_HOT_LINE_HALF       2
#define WF_HOT_CENTER          APP_SCREEN_CENTER_X
#define WF_HOT_TOP_Y           5
#define WF_HOT_BOTTOM_Y        355
#define WF_HOT_SIDE_Y          APP_SCREEN_CENTER_Y
#define WF_HOT_EDGE_NEAR       5
#define WF_HOT_EDGE_FAR        351

/* ── 分隔虚线 ─────────────────────────────────────────────────────────────── */
#define WF_DIVIDER_STEP        10
#define WF_DIVIDER_DOT_R       1
#define WF_DIVIDER_1_X1        40
#define WF_DIVIDER_1_X2        320
#define WF_DIVIDER_1_Y         102
#define WF_DIVIDER_2_X1        28
#define WF_DIVIDER_2_X2        332
#define WF_DIVIDER_2_Y         203
#define WF_DIVIDER_3_X1        82
#define WF_DIVIDER_3_X2        292
#define WF_DIVIDER_3_Y         292

/* ── Codex 六边形标志 ─────────────────────────────────────────────────────── */
#define WF_MARK_SIDES          6
#define WF_MARK_CX             102
#define WF_MARK_CY             65
#define WF_MARK_RADIUS         20.0f
#define WF_MARK_START_DEG      30.0f
#define WF_MARK_STEP_DEG       60.0f
#define WF_MARK_DOT_R          2
#define WF_MARK_TEXT_X         146
#define WF_MARK_TEXT_Y         55
#define WF_MARK_TEXT_STEP      4
#define WF_MARK_TEXT_DOT_R     1

/* 通用 provider 徽标（六边形）几何；内部小点表在 usage_face.c */
#define WF_MARK_DOT_COUNT      8

/* ── 用量区块 ─────────────────────────────────────────────────────────────── */
#define WF_SECTION_LABEL_X     45
#define WF_CURRENT_LABEL_Y     122
#define WF_WEEKLY_LABEL_Y      221
#define WF_SECTION_LABEL_STEP  2
#define WF_SECTION_DOT_R       1

#define WF_PERCENT_RIGHT_X     319
#define WF_CURRENT_PERCENT_Y   116
#define WF_WEEKLY_PERCENT_Y    217
#define WF_PERCENT_DIGIT_STEP  4
#define WF_PERCENT_SIGN_STEP   3
#define WF_PERCENT_GAP         4
#define WF_PERCENT_SIGN_Y_OFF  5
#define WF_PERCENT_DOT_R       1

#define WF_PROGRESS_X          48
#define WF_CURRENT_PROGRESS_Y  156
#define WF_WEEKLY_PROGRESS_Y   248
#define WF_PROGRESS_DOTS       25
#define WF_PROGRESS_SPACING    11
#define WF_PROGRESS_DOT_R      2
#define WF_PROGRESS_ROWS       2
#define WF_WEEKLY_PROGRESS_ROWS 1

/* ── Reset 行（时钟图标 + 文本） ──────────────────────────────────────────── */
#define WF_RESET_CLOCK_X       58
#define WF_RESET_CLOCK_Y_OFF   9
#define WF_RESET_CLOCK_R       7
#define WF_RESET_CLOCK_W       2
#define WF_RESET_HOUR_LEN      5
#define WF_RESET_MIN_DX        4
#define WF_RESET_MIN_DY        2
#define WF_RESET_TEXT_X1       73
#define WF_RESET_TEXT_X2       315
#define WF_RESET_TEXT_H        20
#define WF_RESET_DAILY_Y       180
#define WF_RESET_WEEKLY_Y      266

/* ── 数字文本 ─────────────────────────────────────────────────────────────── */
#define WF_DIGIT_TEXT_FMT      "%d"

/* ── Agent 状态行 ─────────────────────────────────────────────────────────── */
#define WF_AGENT_DOT_X         100
#define WF_AGENT_DOT_Y         307
#define WF_AGENT_DOT_R         3
#define WF_AGENT_TEXT_X        115
#define WF_AGENT_TEXT_Y        300
#define WF_AGENT_TEXT_STEP     2
#define WF_AGENT_TEXT_DOT_R    1

/* ── 透明度按主题微调 ─────────────────────────────────────────────────────── */
#define WF_TICK_OPA_DARK       LV_OPA_70
#define WF_TICK_OPA_LIGHT      LV_OPA_50
#define WF_DIVIDER_OPA_DARK    LV_OPA_70
#define WF_DIVIDER_OPA_LIGHT   LV_OPA_40
#define WF_PROGRESS_IDLE_OPA   LV_OPA_60

#endif /* SCREENS_USAGE_FACE_LAYOUT_H */
