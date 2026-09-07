#include "lv_host.h"

#include <string.h>

#include "host_tick.h"

static lv_color_t s_fb[LV_HOST_FB_SIZE];
static lv_color_t s_cache[LV_HOST_SCREEN_PX * LV_HOST_CACHE_ROWS];
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;

static struct {
    bool pressed;
    int x;
    int y;
    uint32_t flushes;
} s_host;

static void host_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *colors)
{
    /* 把局部刷新缓冲写回"虚拟帧缓冲"，顺便统计 flush 次数 */
    for (lv_coord_t y = area->y1; y <= area->y2; y++) {
        for (lv_coord_t x = area->x1; x <= area->x2; x++) {
            const lv_coord_t w = (lv_coord_t)(area->x2 - area->x1 + 1);
            s_fb[y * LV_HOST_SCREEN_PX + x] = colors[(y - area->y1) * w + (x - area->x1)];
        }
    }
    s_host.flushes++;
    lv_disp_flush_ready(drv);
}

static void host_indev_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    data->point.x = (lv_coord_t)s_host.x;
    data->point.y = (lv_coord_t)s_host.y;
    data->state = s_host.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void lv_host_init(void)
{
    static bool started;

    memset(&s_host, 0, sizeof(s_host));
    memset(s_fb, 0, sizeof(s_fb));

    /* 多个测试文件都会调用 lv_host_init()：LVGL 与输入/显示驱动只能注册一次，
     * 否则一个触摸点会被多个 indev 处理，产生重复手势事件。 */
    if (started) return;
    started = true;

    host_millis_set(0u);
    lv_init();

    lv_disp_draw_buf_init(&s_draw_buf, s_cache, NULL, LV_HOST_SCREEN_PX * LV_HOST_CACHE_ROWS);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = LV_HOST_SCREEN_PX;
    s_disp_drv.ver_res = LV_HOST_SCREEN_PX;
    s_disp_drv.flush_cb = host_flush;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = host_indev_read;
    lv_indev_drv_register(&s_indev_drv);
}

void lv_host_advance_ms(uint32_t ms)
{
    host_millis_advance(ms);
}

uint32_t lv_host_now_ms(void)
{
    return host_millis();
}

void lv_host_run_timers(void)
{
    lv_timer_handler();
}

void lv_host_touch_move(int x, int y)
{
    s_host.x = x;
    s_host.y = y;
}

void lv_host_touch_down(int x, int y)
{
    s_host.x = x;
    s_host.y = y;
    s_host.pressed = true;

    /* 让 indev 读到按下 */
    host_millis_advance(LV_INDEV_DEF_READ_PERIOD + 1u);
    lv_timer_handler();
}

void lv_host_touch_up(void)
{
    s_host.pressed = false;
    host_millis_advance(LV_INDEV_DEF_READ_PERIOD + 1u);
    lv_timer_handler();
}

void lv_host_swipe(int x_from, int y_from, int x_to, int y_to)
{
    lv_host_touch_down(x_from, y_from);

    /* 分几步移动，累计位移超过 gesture_limit 即产生手势 */
    for (int i = 1; i <= 4; i++) {
        s_host.x = x_from + ((x_to - x_from) * i) / 4;
        s_host.y = y_from + ((y_to - y_from) * i) / 4;
        host_millis_advance(LV_INDEV_DEF_READ_PERIOD + 1u);
        lv_timer_handler();
    }

    lv_host_touch_up();
}

const lv_color_t *lv_host_framebuffer(void)
{
    return s_fb;
}

uint32_t lv_host_non_black_pixels(void)
{
    uint32_t n = 0;
    for (int i = 0; i < LV_HOST_FB_SIZE; i++) {
        if (s_fb[i].full != 0u) n++;
    }
    return n;
}

/* 16bit 颜色做集合基数统计：最多 65536 种，用位图足够 */
uint32_t lv_host_distinct_colors(void)
{
    static uint8_t seen[65536 / 8];
    memset(seen, 0, sizeof(seen));

    uint32_t n = 0;
    for (int i = 0; i < LV_HOST_FB_SIZE; i++) {
        const uint16_t c = s_fb[i].full;
        if (!(seen[c >> 3] & (1u << (c & 7u)))) {
            seen[c >> 3] |= (uint8_t)(1u << (c & 7u));
            n++;
        }
    }
    return n;
}

lv_obj_t *lv_host_active_screen(void)
{
    return lv_disp_get_scr_act(lv_disp_get_default());
}

uint32_t lv_host_flush_count(void)
{
    return s_host.flushes;
}

void lv_host_reset_flush_count(void)
{
    s_host.flushes = 0u;
}
