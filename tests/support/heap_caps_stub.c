/* heap_caps 替身：默认退化成 malloc，但允许测试注入"分配失败"。 */
#include <stdlib.h>

#include "esp_heap_caps.h"

enum {
    HEAP_OK = 0,
    HEAP_FAIL_SPIRAM_ONCE,
    HEAP_FAIL_ALL,
};

static int s_fail_mode;

void host_heap_caps_fail_none(void)      { s_fail_mode = HEAP_OK; }
void host_heap_caps_fail_spiram_once(void) { s_fail_mode = HEAP_FAIL_SPIRAM_ONCE; }
void host_heap_caps_fail_all(bool fail)  { s_fail_mode = fail ? HEAP_FAIL_ALL : HEAP_OK; }

void *heap_caps_malloc(size_t size, uint32_t caps)
{
    if (s_fail_mode == HEAP_FAIL_ALL) return NULL;
    if (s_fail_mode == HEAP_FAIL_SPIRAM_ONCE && (caps & MALLOC_CAP_SPIRAM) != 0u) {
        s_fail_mode = HEAP_OK;
        return NULL;
    }
    return malloc(size);
}

void heap_caps_free(void *ptr)
{
    free(ptr);
}
