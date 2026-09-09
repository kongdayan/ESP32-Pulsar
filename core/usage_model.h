/*
 * usage_model.h — Codex 用量数据的运行时模型（纯逻辑，不依赖 LVGL / Arduino / HAL）。
 *
 * 数据来源见 client/pulsar_ble_client.py：电脑端从 Codex 后端取用量，经 BLE 写入固件。
 * 这里只负责「怎么存 / 怎么解析 / 怎么格式化」，不负责怎么收（收在 hal/ble_usage.cpp）。
 *
 * JSON 键（客户端与固件共用，改动需两端同步）：
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

#define USAGE_SEC_PER_MINUTE   60
#define USAGE_SEC_PER_HOUR     (60 * USAGE_SEC_PER_MINUTE)
#define USAGE_SEC_PER_DAY      (24 * USAGE_SEC_PER_HOUR)

/* 解析用 JSON 键 */
#define USAGE_KEY_CURRENT_PCT   "cu"
#define USAGE_KEY_CURRENT_IN    "ci"
#define USAGE_KEY_WEEKLY_PCT    "wu"
#define USAGE_KEY_WEEKLY_IN     "wi"
#define USAGE_KEY_WEEKLY_LABEL  "wl"
#define USAGE_KEY_PLAN          "pl"
#define USAGE_KEY_CREDITS       "cc"
#define USAGE_KEY_UNLIMITED     "un"
#define USAGE_KEY_LIMIT_REACHED "rl"

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

/* ── 共享存储（单写多读，seqlock；BLE 任务写，UI 主循环读） ───────────────── */
void usage_store_set(const usage_data_t *d);
bool usage_store_get(usage_data_t *out);   /* 读到撕裂中的快照时返回 false */
void usage_store_reset(void);

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
