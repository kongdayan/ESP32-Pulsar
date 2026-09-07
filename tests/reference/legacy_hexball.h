/*
 * legacy_hexball.h — 重构前 screens/screen_agent.c 中的物理模型（原样保留，作为差分测试基准）。
 *
 * 这份文件只在主机测试里编译，不参与固件构建，也不计入覆盖率。
 * 唯一改动：lv_rand() → 可注入的 legacy_rand_fn，其余公式/常量/顺序逐字保留。
 */
#ifndef TESTS_LEGACY_HEXBALL_H
#define TESTS_LEGACY_HEXBALL_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t (*legacy_rand_fn)(void *user, uint32_t low, uint32_t high);

typedef struct { float x, y, vx, vy; } legacy_ball;
typedef struct { float radius, angle; int gap; bool alive; bool fresh; } legacy_ring;

typedef struct {
    legacy_ball ball;
    legacy_ring rings[3];
    int score;
} legacy_hexball_t;

void legacy_reset(legacy_hexball_t *s);
void legacy_physics_step(legacy_hexball_t *s, legacy_rand_fn rnd, void *user);
int  legacy_nearest_ring(const legacy_hexball_t *s, float dist);

#endif /* TESTS_LEGACY_HEXBALL_H */
