/*
 * 主机侧 sd_card.h 桩：接口与 hal/sd_card.h 完全一致，
 * 但挂载点指向可写的临时目录，并且挂载状态可由测试控制。
 */
#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SD_CARD_MOUNT_POINT "build/sdcard"

bool sd_card_init(void);
bool sd_card_is_mounted(void);
uint64_t sd_card_size_bytes(void);

/* 仅测试用 */
void host_sd_card_reset(void);
void host_sd_card_set_mounted(bool mounted);
void host_sd_card_set_size_bytes(uint64_t bytes);
int host_sd_card_init_calls(void);

#ifdef __cplusplus
}
#endif

#endif
