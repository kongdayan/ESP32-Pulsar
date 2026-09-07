#ifndef DISPLAY_H
#define DISPLAY_H

#include "app_config.h"
#include "pincfg.h"
#include <lvgl.h>

/* 面板分辨率统一由 core/app_config.h 提供 */
#define SCREEN_RES_HOR APP_SCREEN_PX
#define SCREEN_RES_VER APP_SCREEN_PX

#ifdef __cplusplus
extern "C" {
#endif

void display_init(void);
void display_power_tick(void);
void setRotation(uint8_t rot);
void screen_switch(bool on);
void set_brightness(uint8_t bri);

#ifdef __cplusplus
}
#endif

#endif
