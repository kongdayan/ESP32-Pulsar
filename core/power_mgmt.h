/*
 * power_mgmt.h — 息屏/唤醒状态机（纯逻辑，时间由外部注入，可单元测试）。
 *
 * hal/display.cpp 只负责把这里的决策落到背光硬件上。
 */
#ifndef CORE_POWER_MGMT_H
#define CORE_POWER_MGMT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t idle_timeout_ms;   /* 无触摸多久后息屏 */
    uint32_t last_activity_ms;  /* 最近一次触摸/点亮时刻 */
    bool     screen_on;
} power_state_t;

void power_state_init(power_state_t *state, uint32_t now_ms, uint32_t idle_timeout_ms);

bool power_state_is_on(const power_state_t *state);

/* 触摸事件：刷新活跃时间；若处于息屏则请求点亮背光。返回 true 表示需要 screen_switch(true) */
bool power_notify_touch(power_state_t *state, uint32_t now_ms);

/* 主循环周期调用：超时则请求熄灭背光。返回 true 表示需要 screen_switch(false) */
bool power_tick(power_state_t *state, uint32_t now_ms);

/* 背光已被外部改变（如 screen_switch）后同步状态。返回 true 表示状态发生了变化 */
bool power_set_screen_on(power_state_t *state, bool on, uint32_t now_ms);

/* 距离下次息屏的剩余毫秒；已息屏返回 0。用于测试与调试输出 */
uint32_t power_idle_remaining_ms(const power_state_t *state, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* CORE_POWER_MGMT_H */
