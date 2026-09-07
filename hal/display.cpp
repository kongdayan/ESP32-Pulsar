#include "display.h"

#include <Arduino.h>
#include <esp_display_panel.hpp>

#include "app_config.h"
#include "display_rotation.h"
#include "power_mgmt.h"

using namespace esp_panel::drivers;

static lv_color_t        *disp_draw_buf;
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t      disp_drv;
static lv_indev_t        *indev_touchpad;
static BacklightPWM_LEDC *backlight = nullptr;
static LCD               *lcd       = nullptr;
static Touch             *touch     = nullptr;

/* 息屏/唤醒决策全部在 core/power_mgmt.c，这里只做硬件落地 */
static power_state_t      power;

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    LCD *panel = static_cast<LCD *>(disp->user_data);
    panel->drawBitmap(area->x1, area->y1,
                      area->x2 - area->x1 + 1,
                      area->y2 - area->y1 + 1,
                      reinterpret_cast<const uint8_t *>(color_p));
}

IRAM_ATTR bool onDrawBitmapFinishCallback(void *user_data)
{
    lv_disp_drv_t *drv = static_cast<lv_disp_drv_t *>(user_data);
    lv_disp_flush_ready(drv);
    return false;
}

#if TOUCH_PIN_NUM_INT >= 0
IRAM_ATTR bool onTouchInterruptCallback(void *)
{
    return false;
}
#endif

void setRotation(uint8_t rot)
{
    disp_rotation_t mapping;
    if (!disp_rotation_resolve(rot, &mapping) || lcd == nullptr || touch == nullptr) {
        return;
    }

    lcd->swapXY(mapping.swap_xy);   lcd->mirrorX(mapping.mirror_x);   lcd->mirrorY(mapping.mirror_y);
    touch->swapXY(mapping.swap_xy); touch->mirrorX(mapping.mirror_x); touch->mirrorY(mapping.mirror_y);
}

void screen_switch(bool on)
{
    if (backlight == nullptr) return;

    if (on) {
        backlight->on();
    } else {
        backlight->off();
    }
    (void)power_set_screen_on(&power, on, millis());
}

void set_brightness(uint8_t bri)
{
    if (backlight == nullptr) return;
    backlight->setBrightness(bri);
}

void display_power_tick(void)
{
    if (backlight == nullptr) return;
    if (power_tick(&power, millis())) screen_switch(false);
}

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    Touch *tp = static_cast<Touch *>(indev_drv->user_data);
    TouchPoint point;
    data->state = LV_INDEV_STATE_RELEASED;

    if (tp->readRawData(1, 0, 0)) {
        if (tp->getPoints(&point, 1) > 0) {
            data->point.x = point.x;
            data->point.y = point.y;
            data->state = LV_INDEV_STATE_PRESSED;

            if (power_notify_touch(&power, millis())) screen_switch(true);
        }
    }
}

static lv_indev_t *indev_init(Touch *tp)
{
    if (tp == nullptr || tp->getPanelHandle() == nullptr) return nullptr;

    static lv_indev_drv_t indev_drv_tp;
    lv_indev_drv_init(&indev_drv_tp);
    indev_drv_tp.type      = LV_INDEV_TYPE_POINTER;
    indev_drv_tp.read_cb   = touchpad_read;
    indev_drv_tp.user_data = static_cast<void *>(tp);
    return lv_indev_drv_register(&indev_drv_tp);
}

void display_init()
{
    backlight = new BacklightPWM_LEDC(TFT_BLK, true);
    backlight->begin();
    backlight->off();

    BusI2C *touch_bus = new BusI2C(
        TOUCH_PIN_NUM_I2C_SCL, TOUCH_PIN_NUM_I2C_SDA,
        (BusI2C::ControlPanelFullConfig)ESP_PANEL_TOUCH_I2C_CONTROL_PANEL_CONFIG(CST816S));
    touch_bus->configI2C_FreqHz(APP_TOUCH_I2C_FREQ_HZ);

    touch = new TouchCST816S(touch_bus, SCREEN_RES_HOR, SCREEN_RES_VER,
                             TOUCH_PIN_NUM_RST, TOUCH_PIN_NUM_INT);
    touch->begin();
#if TOUCH_PIN_NUM_INT >= 0
    touch->attachInterruptCallback(onTouchInterruptCallback, nullptr);
#endif

    BusQSPI *panel_bus = new BusQSPI(
        TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
    panel_bus->configQSPI_FreqHz(APP_QSPI_FREQ_HZ);

    lcd = new LCD_ST77916(panel_bus, SCREEN_RES_HOR, SCREEN_RES_VER, APP_COLOR_DEPTH_BITS, TFT_RST);
    lcd->begin();
    lcd->invertColor(true);
    lcd->setDisplayOnOff(true);

    backlight->on();
    backlight->setBrightness(APP_BACKLIGHT_BRIGHTNESS);
    power_state_init(&power, millis(), APP_SCREEN_IDLE_TIMEOUT_MS);

    const size_t cache_bytes = (size_t)APP_LV_DRAW_CACHE_ROWS * SCREEN_RES_HOR * APP_BYTES_PER_PIXEL;
    disp_draw_buf = static_cast<lv_color_t *>(
        heap_caps_malloc(cache_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL,
                          SCREEN_RES_HOR * (size_t)APP_LV_DRAW_CACHE_ROWS);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = SCREEN_RES_HOR;
    disp_drv.ver_res   = SCREEN_RES_VER;
    disp_drv.flush_cb  = my_disp_flush;
    disp_drv.draw_buf  = &draw_buf;
    disp_drv.user_data = static_cast<void *>(lcd);
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    lcd->attachDrawBitmapFinishCallback(onDrawBitmapFinishCallback, static_cast<void *>(disp->driver));

    indev_touchpad = indev_init(touch);
}
