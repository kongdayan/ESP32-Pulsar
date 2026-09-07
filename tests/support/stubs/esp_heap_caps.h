/*
 * 主机侧 esp_heap_caps.h 桩：接口与真机一致，但分配退化成 malloc；
 * 具体实现见 support/heap_caps_stub.c，可注入"分配失败"以覆盖回落路径。
 */
#ifndef TESTS_STUB_ESP_HEAP_CAPS_H
#define TESTS_STUB_ESP_HEAP_CAPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MALLOC_CAP_8BIT     0x00000001u
#define MALLOC_CAP_SPIRAM   0x00000002u
#define MALLOC_CAP_INTERNAL 0x00000004u
#define MALLOC_CAP_DMA      0x00000008u

void *heap_caps_malloc(size_t size, uint32_t caps);
void  heap_caps_free(void *ptr);

/* 仅测试用 */
void host_heap_caps_fail_none(void);
void host_heap_caps_fail_spiram_once(void);
void host_heap_caps_fail_all(bool fail);

#endif /* TESTS_STUB_ESP_HEAP_CAPS_H */
