#ifndef TESTS_HOST_TICK_H
#define TESTS_HOST_TICK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 由 lv_conf_host.h 里的 LV_TICK_CUSTOM_SYS_TIME_EXPR 使用 */
uint32_t host_millis(void);
void host_millis_advance(uint32_t ms);
void host_millis_set(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif
