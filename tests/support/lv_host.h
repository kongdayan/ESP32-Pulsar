/*
 * lv_host.h — 主机侧 LVGL 运行环境：360x360 虚拟屏 + 指针式触摸 + 可控时钟。
 *
 * 有了它，屏幕的 init / 绘制回调 / 手势回调都能在 PC 上真实跑一遍，
 * 覆盖率统计因此可以包含 screens/ 与 ui/。
 */
#ifndef TESTS_LV_HOST_H
#define TESTS_LV_HOST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

#define LV_HOST_SCREEN_PX   360
#define LV_HOST_FB_SIZE     (LV_HOST_SCREEN_PX * LV_HOST_SCREEN_PX)
#define LV_HOST_CACHE_ROWS  40

void lv_host_init(void);

/* 可控时钟：推进 LVGL tick 并跑若干轮主循环 */
void lv_host_advance_ms(uint32_t ms);
uint32_t lv_host_now_ms(void);
void lv_host_run_timers(void);

/* 触摸：设置触点并按下/松开，然后推进 indev 读取周期 */
void lv_host_touch_move(int x, int y);
void lv_host_touch_down(int x, int y);
void lv_host_touch_up(void);
/* 完整一次滑动：按下 → 位移 → 松开，用于触发 LV_EVENT_GESTURE */
void lv_host_swipe(int x_from, int y_from, int x_to, int y_to);

const lv_color_t *lv_host_framebuffer(void);
/* 帧缓冲中与纯黑不同的像素数 / 不同颜色数，用来判断"这屏是不是黑屏" */
uint32_t lv_host_non_black_pixels(void);
uint32_t lv_host_distinct_colors(void);

lv_obj_t *lv_host_active_screen(void);
uint32_t lv_host_flush_count(void);
void lv_host_reset_flush_count(void);

#ifdef __cplusplus
}
#endif

#endif /* TESTS_LV_HOST_H */
