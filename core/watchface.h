/*
 * watchface.h — Codex 用量表盘的点阵字体与数值映射（纯逻辑）。
 */
#ifndef CORE_WATCHFACE_H
#define CORE_WATCHFACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui_math.h"
#include "ui_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WF_GLYPH_ROWS       7
#define WF_GLYPH_COLS       5
#define WF_GLYPH_ROW_BITS   5                       /* 每行 5 个像素点 */
#define WF_GLYPH_HIGH_BIT   (1u << (WF_GLYPH_COLS - 1))
#define WF_GLYPH_ADVANCE    6                       /* 每前进一个字符：6 * step 像素 */
#define WF_GLYPH_BLANK_ROWS 7
#define WF_DIGIT_BUFFER     5                       /* "100%\0" 以内 */
#define WF_PERCENT_BUFFER   6
#define WF_BATTERY_ROUND    33                      /* 保留旧版取整方式 */
#define WF_BATTERY_DIVISOR  34
#define WF_BATTERY_MIN_PCT  0
#define WF_BATTERY_MAX_PCT  100
#define WF_DIGIT_ZERO       '0'
#define WF_LETTER_A_LOWER   'a'
#define WF_LETTER_A_UPPER   'A'
#define WF_LETTER_Z_LOWER   'z'
#define WF_LETTER_Z_UPPER   'Z'

/* 点阵字符：数字 0-9 后紧跟字母 A-Z，另有若干符号 */
#define WF_GLYPH_DIGITS     10
#define WF_GLYPH_LETTERS    26
#define WF_GLYPH_TABLE_SIZE (WF_GLYPH_DIGITS + WF_GLYPH_LETTERS)

/* 返回一个字符对应的 WF_GLYPH_ROWS 行位图；未知字符返回空格 */
const uint8_t *wf_glyph_for(char c);

/* 一行点阵在第 col 列是否有像素（col 从 0 开始，高位在左） */
bool wf_glyph_pixel(const uint8_t *glyph, int row, int col);

/* 文本像素宽度：len>0 时 len*WF_GLYPH_ADVANCE*step - step */
int wf_dot_text_width(const char *text, int step);
int wf_glyph_advance(int step);

/* 右侧对齐文本的起始 x */
int wf_right_align_x(int right_x, const char *text, int step);

/* 居中绘制时的起始 x（以 screen_px 为轴） */
int wf_center_align_x(int screen_px, const char *text, int step);

/* 进度点数量：total_steps 中有多少个点亮 */
int wf_progress_steps(int total_steps, int percent);

/* 电池格数（沿用旧版 (pct+33)/34 的映射） */
int wf_battery_cells(int percent);

/* 把百分比格式化成 "73%" 之类；返回写入长度 */
int wf_format_percent(char *out, size_t out_sz, int percent);

#ifdef __cplusplus
}
#endif

#endif /* CORE_WATCHFACE_H */
