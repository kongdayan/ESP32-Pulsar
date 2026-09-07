/* 主机替身的查询接口（供测试断言调用次数/参数） */
#ifndef TESTS_HOST_STUBS_H
#define TESTS_HOST_STUBS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void host_display_reset(void);
int host_display_init_calls(void);
int host_display_power_tick_calls(void);
int host_display_on_calls(void);
int host_display_off_calls(void);

void host_sd_card_reset(void);
void host_sd_card_set_mounted(bool mounted);
void host_sd_card_set_size_bytes(uint64_t bytes);
int host_sd_card_init_calls(void);

void host_arduino_reset(void);
long host_arduino_serial_baud(void);
uint32_t host_arduino_delay_total_ms(void);

#ifdef __cplusplus
}
#endif

#endif
