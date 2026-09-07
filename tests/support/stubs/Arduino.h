/* 主机侧 Arduino 桩：让 main.cpp 能在 PC 上编译，从而验证 setup/loop 的装配顺序 */
#ifndef TESTS_STUB_ARDUINO_H
#define TESTS_STUB_ARDUINO_H

#include <stdint.h>

#include "host_tick.h"

#ifdef __cplusplus
extern "C" {
#endif

void host_arduino_reset(void);
long host_arduino_serial_baud(void);
uint32_t host_arduino_delay_total_ms(void);
void host_arduino_record_delay(uint32_t ms);
void host_arduino_record_serial_begin(long baud);

#ifdef __cplusplus
}
#endif

static inline void delay(uint32_t ms)
{
    host_arduino_record_delay(ms);
    host_millis_advance(ms);
}

static inline void vTaskDelay(uint32_t ticks)
{
    host_millis_advance(ticks);
}

static inline uint32_t millis(void)
{
    return host_millis();
}

#ifdef __cplusplus
struct host_serial_t {
    void begin(unsigned long baud) { host_arduino_record_serial_begin((long)baud); }
    void println(const char *s) { (void)s; }
    void printf(const char *fmt, ...) { (void)fmt; }
};

static inline host_serial_t host_serial_instance(void)
{
    host_serial_t s;
    return s;
}
#define Serial host_serial_instance()
#endif

#endif /* TESTS_STUB_ARDUINO_H */
