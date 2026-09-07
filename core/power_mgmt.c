#include "power_mgmt.h"

#include <stddef.h>

void power_state_init(power_state_t *state, uint32_t now_ms, uint32_t idle_timeout_ms)
{
    if (state == NULL) return;
    state->idle_timeout_ms  = idle_timeout_ms;
    state->last_activity_ms = now_ms;
    state->screen_on        = true;
}

bool power_state_is_on(const power_state_t *state)
{
    return (state != 0) && state->screen_on;
}

bool power_notify_touch(power_state_t *state, uint32_t now_ms)
{
    if (state == NULL) return false;

    state->last_activity_ms = now_ms;
    if (state->screen_on) return false;

    state->screen_on = true;
    return true;
}

bool power_tick(power_state_t *state, uint32_t now_ms)
{
    if (state == NULL || !state->screen_on) return false;

    /* uint32 相减天然处理 millis() 回绕 */
    if ((uint32_t)(now_ms - state->last_activity_ms) >= state->idle_timeout_ms) {
        state->screen_on = false;
        return true;
    }
    return false;
}

bool power_set_screen_on(power_state_t *state, bool on, uint32_t now_ms)
{
    if (state == NULL || state->screen_on == on) return false;

    state->screen_on = on;
    if (on) state->last_activity_ms = now_ms;
    return true;
}

uint32_t power_idle_remaining_ms(const power_state_t *state, uint32_t now_ms)
{
    if (state == NULL || !state->screen_on) return 0u;

    const uint32_t elapsed = (uint32_t)(now_ms - state->last_activity_ms);
    if (elapsed >= state->idle_timeout_ms) return 0u;
    return state->idle_timeout_ms - elapsed;
}
