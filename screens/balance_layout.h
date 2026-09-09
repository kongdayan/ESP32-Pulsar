/*
 * balance_layout.h — DeepSeek 余额屏的视图常量。
 * 数据来自 core/balance_model（BLE 写入），这里只放几何/文案/字体。
 */
#ifndef SCREENS_BALANCE_LAYOUT_H
#define SCREENS_BALANCE_LAYOUT_H

#include "app_config.h"
#include "ui_theme.h"

/* ── 文案 ─────────────────────────────────────────────────────────────────── */
#define BAL_TEXT_TITLE        "DEEPSEEK"
#define BAL_TEXT_TOTAL        "TOTAL"
#define BAL_TEXT_TOPPED       "TOP-UP"
#define BAL_TEXT_GRANTED      "GRANTED"
#define BAL_TEXT_AVAILABLE    "AVAILABLE"
#define BAL_TEXT_UNAVAILABLE  "LIMITED"
#define BAL_TEXT_WAITING      "Waiting for BLE"
#define BAL_TEXT_NO_AMOUNT    "--"
#define BAL_FMT_ROW           "%s  %s"

/* ── 刷新 / 过期 ──────────────────────────────────────────────────────────── */
#define BAL_REFRESH_MS        1000u
#define BAL_STALE_MS          (90u * 1000u)
#define BAL_ROW_TEXT_MAX      32

/* ── 布局（相对屏幕中心的 y 偏移） ───────────────────────────────────────── */
#define BAL_TITLE_Y           (-132)
#define BAL_AMOUNT_Y          (-40)
#define BAL_CURRENCY_Y        20
#define BAL_STATUS_Y          62
#define BAL_ROW1_Y            104
#define BAL_ROW2_Y            124
#define BAL_ROW3_Y            144

/* ── 字体 ─────────────────────────────────────────────────────────────────── */
#define BAL_FONT_TITLE        (&lv_font_montserrat_14)
#define BAL_FONT_AMOUNT       (&lv_font_montserrat_44)
#define BAL_FONT_CURRENCY     (&lv_font_montserrat_16)
#define BAL_FONT_ROW          (&lv_font_montserrat_10)

#endif /* SCREENS_BALANCE_LAYOUT_H */
