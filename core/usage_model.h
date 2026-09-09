/*
 * usage_model.h — 用量数据的运行时模型（纯逻辑，不依赖 LVGL / Arduino / HAL）。
 *
 * 这是一个 **provider 无关** 的用量模型：Codex、Claude、NVIDIA、AMD、GLM……
 * 都归一到同一组字段（当前窗口 / 周窗口的已用百分比 + 重置时间），
 * 屏幕侧只认 provider id + usage_data_t，不再为每个服务写一套逻辑。
 *
 * 数据来源：电脑端 client/pulsar_ble_client.py 从各服务取数，经 BLE 写入固件
 * （hal/ble_usage.cpp 的用量特征值）。JSON 里用 "p" 指定 provider。
 *
 * JSON 键（客户端与固件共用，改动需两端同步）：
 *   p   provider id               见 usage_provider_t（缺省 0 = Codex，向后兼容）
 *   cu  current used percent      0..100
 *   ci  current resets in         seconds（接收时刻的剩余秒数）
 *   wu  weekly used percent       0..100
 *   wi  weekly resets in          seconds
 *   wl  weekly reset label        "16:14 on 18 May"（电脑端本地时区，固件不碰时区）
 *   pl  plan                      见 usage_plan_t
 *   cc  has credits               0/1
 *   un  unlimited                 0/1
 *   rl  rate limit reached        0/1
 */
#ifndef CORE_USAGE_MODEL_H
#define CORE_USAGE_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 范围 / 长度 / 时间常量（函数体内不再出现字面量） ─────────────────────── */
#define USAGE_PCT_MIN          0
#define USAGE_PCT_MAX          100
#define USAGE_KEY_MAX          8
#define USAGE_LABEL_MAX        24
#define USAGE_RESET_TEXT_MAX   40
#define USAGE_TITLE_MAX        12
#define USAGE_PROVIDER_KEY_MAX 12

#define USAGE_SEC_PER_MINUTE   60
#define USAGE_SEC_PER_HOUR     (60 * USAGE_SEC_PER_MINUTE)
#define USAGE_SEC_PER_DAY      (24 * USAGE_SEC_PER_HOUR)

/* 解析用 JSON 键 */
#define USAGE_KEY_PROVIDER      "p"
#define USAGE_KEY_CURRENT_PCT   "cu"
#define USAGE_KEY_CURRENT_IN    "ci"
#define USAGE_KEY_WEEKLY_PCT    "wu"
#define USAGE_KEY_WEEKLY_IN     "wi"
#define USAGE_KEY_WEEKLY_LABEL  "wl"
#define USAGE_KEY_PLAN          "pl"
#define USAGE_KEY_CREDITS       "cc"
#define USAGE_KEY_UNLIMITED     "un"
#define USAGE_KEY_LIMIT_REACHED "rl"

/* ── provider ────────────────────────────────────────────────────────────── */
/* 加新服务只改这里：补一个枚举值 + 在 usage_model.c 的表里补一行。 */
typedef enum {
    USAGE_PROVIDER_CODEX = 0,
    USAGE_PROVIDER_CLAUDE,
    USAGE_PROVIDER_NVIDIA,
    USAGE_PROVIDER_AMD,
    USAGE_PROVIDER_GLM,
    USAGE_PROVIDER_COUNT
} usage_provider_t;

typedef struct {
    const char *key;      /* 机器名："codex" */
    const char *title;    /* 屏幕标题："CODEX" */
    uint32_t    accent;   /* 该 provider 的强调色（0xRRGGBB，屏幕可选） */
} usage_provider_info_t;

extern const usage_provider_info_t usage_providers[USAGE_PROVIDER_COUNT];

bool        usage_provider_is_valid(usage_provider_t p);
const char *usage_provider_key(usage_provider_t p);    /* 非法返回 "unknown" */
const char *usage_provider_title(usage_provider_t p);  /* 非法返回 "?" */

typedef enum {
    USAGE_PLAN_UNKNOWN = 0,
    USAGE_PLAN_FREE,
    USAGE_PLAN_GO,
    USAGE_PLAN_PLUS,
    USAGE_PLAN_PRO,
    USAGE_PLAN_TEAM,
    USAGE_PLAN_ENTERPRISE,
    USAGE_PLAN_COUNT
} usage_plan_t;

typedef struct {
    usage_provider_t provider;                     /* 该数据属于哪个服务 */
    int          current_used_pct;                 /* 0..100 */
    int          weekly_used_pct;                  /* 0..100 */
    int          current_resets_in;                /* 秒（接收时刻） */
    int          weekly_resets_in;                 /* 秒（接收时刻） */
    char         weekly_reset_label[USAGE_LABEL_MAX]; /* 电脑端本地时间文案 */
    usage_plan_t plan;
    bool         has_credits;
    bool         unlimited;
    bool         limit_reached;
    bool         valid;                            /* 解析成功且关键字段齐全 */
    uint32_t     rx_ms;                            /* 接收时刻（写入方填 monotonic ms） */
} usage_data_t;

/* ── 共享存储（每个 provider 一个槽；单写多读 seqlock） ───────────────────── */
void usage_store_set(const usage_data_t *d);                  /* 用 d->provider 选槽 */
bool usage_store_get(usage_provider_t p, usage_data_t *out);  /* 读到撕裂快照返回 false */
void usage_store_reset(void);                                 /* 清空所有 provider */
void usage_store_reset_provider(usage_provider_t p);

/* ── 解析：扁平 JSON（键见文件头） ────────────────────────────────────────── */
void usage_data_defaults(usage_data_t *d);
bool usage_parse_json(const char *json, usage_data_t *out);

/* ── 时间 / 文案 ──────────────────────────────────────────────────────────── */
int  usage_elapsed_s(uint32_t rx_ms, uint32_t now_ms);
int  usage_countdown_s(const usage_data_t *d, uint32_t now_ms);
bool usage_is_stale(const usage_data_t *d, uint32_t now_ms, uint32_t timeout_ms);

/* "Resets in 21:59" / "Resets in 2d 3h"；countdown<=0 时 "Resetting..." */
int  usage_format_countdown(int countdown_s, char *out, size_t n);
/* "Resets 16:14 on 18 May"（label 为空时退化为 "Resets soon"） */
int  usage_format_weekly(const char *label, char *out, size_t n);

/* 计划名（用于副标题，未知返回 "?"） */
const char *usage_plan_name(usage_plan_t plan);

#ifdef __cplusplus
}
#endif

#endif /* CORE_USAGE_MODEL_H */
