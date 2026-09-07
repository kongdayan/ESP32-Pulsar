#include "host_tick.h"

static uint32_t s_now_ms = 0u;

uint32_t host_millis(void)
{
    return s_now_ms;
}

void host_millis_advance(uint32_t ms)
{
    s_now_ms += ms;
}

void host_millis_set(uint32_t ms)
{
    s_now_ms = ms;
}
