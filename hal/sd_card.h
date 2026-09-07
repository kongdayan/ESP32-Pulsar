#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SD_CARD_MOUNT_POINT     "/sdcard"
#define SD_CARD_FORMAT_IF_FAILED false  /* 挂载失败时不自动格式化 */
#define SD_CARD_DETECT_ONLY      false /* 非仅探测模式 */
#define SD_CARD_SIZE_BYTES_ZERO  0u

#define SD_LOG_MOUNT_FAIL       "[sd] mount failed\n"
#define SD_LOG_MOUNT_OK         "[sd] mounted, size: %llu MB\n"

bool sd_card_init(void);
bool sd_card_is_mounted(void);
uint64_t sd_card_size_bytes(void);

#ifdef __cplusplus
}
#endif

#endif
