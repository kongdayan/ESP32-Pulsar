/*
 * app_config.h — 全局编译期配置（纯宏，无 LVGL / 无硬件依赖）
 *
 * 所有跨模块共享的几何、时序、电源、串口参数集中在此，
 * 函数体内不允许再出现字面量。
 */
#ifndef CORE_APP_CONFIG_H
#define CORE_APP_CONFIG_H

/* ── 显示几何 ─────────────────────────────────────────────────────────────── */
#define APP_SCREEN_PX            360                 /* 圆形屏边长（宽=高） */
#define APP_SCREEN_MAX_COORD     (APP_SCREEN_PX - 1) /* 359，最大像素坐标 */
#define APP_SCREEN_CENTER_X      (APP_SCREEN_PX / 2) /* 180 */
#define APP_SCREEN_CENTER_Y      (APP_SCREEN_PX / 2) /* 180 */
#define APP_SCREEN_USABLE_RADIUS 172.0f              /* 圆形可视区半径 */
#define APP_COLOR_DEPTH_BITS     16                  /* RGB565 */
#define APP_BYTES_PER_PIXEL      (APP_COLOR_DEPTH_BITS / 8)

/* ── 主循环 / 通用时序 ────────────────────────────────────────────────────── */
#define APP_MAIN_LOOP_DELAY_MS   5u        /* loop() 中 lv_timer_handler 的间隔 */
#define APP_BOOT_DELAY_MS        200u      /* setup() 上电等待 */
#define APP_SERIAL_BAUD          115200u
#define APP_GAME_TICK_MS         16u       /* ~60fps 逻辑帧 */
#define APP_VIDEO_FRAME_MS       42u       /* ~24fps 视频帧 */
#define APP_VIDEO_RETRY_MS       500u      /* 视频不可用时的重试周期 */

/* ── 屏幕导航 ─────────────────────────────────────────────────────────────── */
#define APP_NAV_ANIM_MS          500u      /* 滑动切屏动画时长 */
#define APP_NAV_ANIM_DELAY_MS    0u

/* ── 背光 / 省电 ──────────────────────────────────────────────────────────── */
#define APP_SCREEN_IDLE_TIMEOUT_MS (30u * 1000u) /* 无触摸多久后息屏 */
#define APP_BACKLIGHT_BRIGHTNESS   100u          /* 默认亮度 0-100 */

/* ── 显示面板 / 触摸总线 ──────────────────────────────────────────────────── */
#define APP_QSPI_FREQ_HZ         (50u * 1000u * 1000u)
#define APP_TOUCH_I2C_FREQ_HZ    400000u
#define APP_LV_DRAW_CACHE_ROWS   72u       /* LVGL 局部刷新缓冲行数 */

/* ── SD / TF 卡 ───────────────────────────────────────────────────────────── */
#define APP_SD_BUS_WIDTH         4u
#define APP_KILOBYTE             1024u
#define APP_MEGABYTE             (APP_KILOBYTE * APP_KILOBYTE)

#endif /* CORE_APP_CONFIG_H */
