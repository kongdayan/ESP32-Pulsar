#include "sd_card.h"
#include "pincfg.h"

#include <Arduino.h>
#include <SD_MMC.h>

#include "app_config.h"

static bool mounted = false;

bool sd_card_init(void)
{
    if (mounted) return true;

    SD_MMC.setPins(SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN,
                   SD_MMC_D1_PIN, SD_MMC_D2_PIN, SD_MMC_D3_PIN);

    mounted = SD_MMC.begin(SD_CARD_MOUNT_POINT, SD_CARD_FORMAT_IF_FAILED,
                           SD_CARD_DETECT_ONLY, SDMMC_FREQ_HIGHSPEED, APP_SD_BUS_WIDTH);
    if (!mounted) {
        Serial.println(SD_LOG_MOUNT_FAIL);
        return false;
    }

    Serial.printf(SD_LOG_MOUNT_OK,
                  (unsigned long long)(SD_MMC.cardSize() / APP_MEGABYTE));
    return true;
}

bool sd_card_is_mounted(void)
{
    return mounted;
}

uint64_t sd_card_size_bytes(void)
{
    if (!mounted) return 0u;
    return SD_MMC.cardSize();
}
