/*
 * balance_model.h — DeepSeek 账户余额的运行时模型（纯逻辑，无 LVGL / Arduino / HAL）。
 *
 * 数据来源：电脑端 client/pulsar_ble_client.py 调 DeepSeek 的
 *   GET https://api.deepseek.com/user/balance
 * 后，经 BLE 写入固件（hal/ble_usage.cpp 的余额特征值）。
 *
 * 金额统一用「分」的整数传输，避免 JSON 浮点与打印误差：
 *   {"cur":"CNY","tot":2855,"gr":0,"top":2855,"av":1}
 *   cur 货币代码（CNY/USD）  tot 总余额（分）  gr 赠金（分）
 *   top 充值余额（分）        av  is_available 0/1
 */
#ifndef CORE_BALANCE_MODEL_H
#define CORE_BALANCE_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BALANCE_CURRENCY_MAX    4
#define BALANCE_KEY_MAX         8
#define BALANCE_AMOUNT_TEXT_MAX 24
#define BALANCE_CENTS_PER_UNIT  100

/* JSON 键（客户端与固件共用） */
#define BALANCE_KEY_CURRENCY    "cur"
#define BALANCE_KEY_TOTAL       "tot"
#define BALANCE_KEY_GRANTED     "gr"
#define BALANCE_KEY_TOPPED_UP   "top"
#define BALANCE_KEY_AVAILABLE   "av"

typedef struct {
    char         currency[BALANCE_CURRENCY_MAX];   /* "CNY" / "USD" */
    int          total_cents;                      /* 总可用余额（分） */
    int          granted_cents;                    /* 未过期赠金（分） */
    int          topped_up_cents;                  /* 充值余额（分） */
    bool         is_available;                     /* 账户是否可继续调用 */
    bool         valid;                            /* 解析成功且关键字段齐全 */
    uint32_t     rx_ms;                            /* 接收时刻（写入方填 monotonic ms） */
} balance_data_t;

/* ── 共享存储（单写多读 seqlock，与 usage_model 同款） ────────────────────── */
void balance_store_set(const balance_data_t *d);
bool balance_store_get(balance_data_t *out);
void balance_store_reset(void);

/* ── 解析 ────────────────────────────────────────────────────────────────── */
void balance_data_defaults(balance_data_t *d);
bool balance_parse_json(const char *json, balance_data_t *out);

/* ── 格式化 / 过期 ───────────────────────────────────────────────────────── */
/* 分 → "28.55"；负值加 '-'；out 不足时截断 */
int  balance_format_amount(int cents, char *out, size_t n);
bool balance_is_stale(const balance_data_t *d, uint32_t now_ms, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* CORE_BALANCE_MODEL_H */
