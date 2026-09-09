/* 硬件相关模块（hal/display.cpp、hal/sd_card.cpp）的主机替身：
 * 只记录调用，不做任何真实 I/O。息屏/唤醒的决策逻辑本身在 core/power_mgmt.c，
 * 由 tests/unit/test_power_mgmt.c 完整覆盖。 */

#include <stddef.h>
#include <string.h>

#include "Arduino.h"
#include "ble_usage.h"
#include "display.h"
#include "host_tick.h"
#include "sd_card.h"
#include "host_stubs.h"

/* ── display_* ───────────────────────────────────────────────────────────── */

static struct {
    int init_calls;
    int power_tick_calls;
    int on_calls;
    int off_calls;
    int brightness_calls;
    uint8_t last_brightness;
} s_display;

void display_init(void)
{
    s_display.init_calls++;
}

void display_power_tick(void)
{
    s_display.power_tick_calls++;
}

void screen_switch(bool on)
{
    if (on) {
        s_display.on_calls++;
    } else {
        s_display.off_calls++;
    }
}

void set_brightness(uint8_t bri)
{
    s_display.brightness_calls++;
    s_display.last_brightness = bri;
}

void host_display_reset(void)
{
    memset(&s_display, 0, sizeof(s_display));
}

int host_display_init_calls(void) { return s_display.init_calls; }
int host_display_power_tick_calls(void) { return s_display.power_tick_calls; }
int host_display_on_calls(void) { return s_display.on_calls; }
int host_display_off_calls(void) { return s_display.off_calls; }

/* ── sd_card ─────────────────────────────────────────────────────────────── */

static struct {
    bool mounted;
    uint64_t size_bytes;
    int init_calls;
} s_sd;

bool sd_card_init(void)
{
    s_sd.init_calls++;
    return s_sd.mounted;
}

bool sd_card_is_mounted(void)
{
    return s_sd.mounted;
}

uint64_t sd_card_size_bytes(void)
{
    return s_sd.mounted ? s_sd.size_bytes : 0u;
}

void host_sd_card_reset(void)
{
    s_sd.mounted = false;
    s_sd.size_bytes = 0u;
    s_sd.init_calls = 0;
}

void host_sd_card_set_mounted(bool mounted) { s_sd.mounted = mounted; }
void host_sd_card_set_size_bytes(uint64_t bytes) { s_sd.size_bytes = bytes; }
int host_sd_card_init_calls(void) { return s_sd.init_calls; }

/* ── ble_usage ───────────────────────────────────────────────────────────── */

static struct {
    int  init_calls;
    bool connected;
} s_ble;

void ble_usage_init(void)
{
    s_ble.init_calls++;
}

bool ble_usage_is_connected(void)
{
    return s_ble.connected;
}

void host_ble_usage_reset(void)
{
    s_ble.init_calls = 0;
    s_ble.connected = false;
}

int host_ble_usage_init_calls(void) { return s_ble.init_calls; }
void host_ble_usage_set_connected(bool connected) { s_ble.connected = connected; }

/* ── Arduino 记录 ────────────────────────────────────────────────────────── */

static struct {
    long serial_baud;
    uint32_t delay_total_ms;
} s_arduino;

void host_arduino_reset(void)
{
    s_arduino.serial_baud = 0;
    s_arduino.delay_total_ms = 0u;
}

long host_arduino_serial_baud(void) { return s_arduino.serial_baud; }
uint32_t host_arduino_delay_total_ms(void) { return s_arduino.delay_total_ms; }
void host_arduino_record_delay(uint32_t ms) { s_arduino.delay_total_ms += ms; }
void host_arduino_record_serial_begin(long baud) { s_arduino.serial_baud = baud; }
