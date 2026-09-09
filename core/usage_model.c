/*
 * usage_model.c — 见 usage_model.h。全部是纯函数 + 一个 seqlock 单例存储。
 */
#include "usage_model.h"

#include <stdio.h>
#include <string.h>

#define USAGE_MS_PER_SECOND     1000u
#define USAGE_STORE_RETRIES     4

#define USAGE_TEXT_RESETTING    "Resetting..."
#define USAGE_TEXT_RESET_SOON   "Resets soon"
#define USAGE_TEXT_RESET_ABS    "Resets %s"
#define USAGE_TEXT_RESET_IN     "Resets in %dd %dh"
#define USAGE_TEXT_RESET_CLOCK  "Resets in %02d:%02d"
#define USAGE_TEXT_UNKNOWN_PLAN "?"

/* ── 共享存储（单写多读 seqlock：BLE 任务写、UI 主循环读） ────────────────── */

static volatile uint32_t s_seq;
static usage_data_t      s_data;

void usage_data_defaults(usage_data_t *d)
{
    if (d == NULL) return;
    memset(d, 0, sizeof(*d));
    d->plan = USAGE_PLAN_UNKNOWN;
}

void usage_store_set(const usage_data_t *d)
{
    if (d == NULL) return;
    s_seq++;                    /* 奇数 = 写入中 */
    __sync_synchronize();
    s_data = *d;
    __sync_synchronize();
    s_seq++;                    /* 偶数 = 完成 */
}

bool usage_store_get(usage_data_t *out)
{
    if (out == NULL) return false;
    for (int i = 0; i < USAGE_STORE_RETRIES; i++) {
        const uint32_t s1 = s_seq;
        if ((s1 & 1u) != 0u) continue;      /* 正在写，重试 */
        __sync_synchronize();
        *out = s_data;
        __sync_synchronize();
        if (s_seq == s1) return true;
    }
    return false;                            /* 一直撞上写入，放弃本次 */
}

void usage_store_reset(void)
{
    usage_data_t d;
    usage_data_defaults(&d);
    usage_store_set(&d);
}

/* ── 解析 ────────────────────────────────────────────────────────────────── */

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static bool parse_int_val(const char **pp, int *out)
{
    const char *p = *pp;
    bool neg = false;
    if (*p == '-') { neg = true; p++; }
    if (*p < '0' || *p > '9') return false;

    long v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        if (v > 2147483647L) return false;
        p++;
    }
    *out = (int)(neg ? -v : v);
    *pp = p;
    return true;
}

static bool parse_str_val(const char **pp, char *out, size_t n)
{
    const char *p = *pp;
    if (*p != '"') return false;
    p++;

    size_t i = 0;
    while (*p != '\0' && *p != '"') {
        if (*p == '\\' && p[1] != '\0') p++;        /* 简化转义：跳过反斜杠 */
        if (out != NULL && i + 1u < n) out[i++] = *p;
        p++;
    }
    if (*p != '"') return false;
    p++;
    if (out != NULL) out[i] = '\0';
    *pp = p;
    return true;
}

static int clamp_pct(int v)
{
    if (v < USAGE_PCT_MIN) return USAGE_PCT_MIN;
    if (v > USAGE_PCT_MAX) return USAGE_PCT_MAX;
    return v;
}

static int clamp_nonneg(int v)
{
    return v > 0 ? v : 0;
}

static usage_plan_t plan_from_int(int v)
{
    if (v < 0 || v >= USAGE_PLAN_COUNT) return USAGE_PLAN_UNKNOWN;
    return (usage_plan_t)v;
}

static void apply_int(usage_data_t *d, const char *key, int v, bool *saw_current, bool *saw_weekly)
{
    if (strcmp(key, USAGE_KEY_CURRENT_PCT) == 0) {
        d->current_used_pct = clamp_pct(v);
        *saw_current = true;
    } else if (strcmp(key, USAGE_KEY_CURRENT_IN) == 0) {
        d->current_resets_in = clamp_nonneg(v);
    } else if (strcmp(key, USAGE_KEY_WEEKLY_PCT) == 0) {
        d->weekly_used_pct = clamp_pct(v);
        *saw_weekly = true;
    } else if (strcmp(key, USAGE_KEY_WEEKLY_IN) == 0) {
        d->weekly_resets_in = clamp_nonneg(v);
    } else if (strcmp(key, USAGE_KEY_PLAN) == 0) {
        d->plan = plan_from_int(v);
    } else if (strcmp(key, USAGE_KEY_CREDITS) == 0) {
        d->has_credits = (v != 0);
    } else if (strcmp(key, USAGE_KEY_UNLIMITED) == 0) {
        d->unlimited = (v != 0);
    } else if (strcmp(key, USAGE_KEY_LIMIT_REACHED) == 0) {
        d->limit_reached = (v != 0);
    }
}

bool usage_parse_json(const char *json, usage_data_t *out)
{
    if (json == NULL || out == NULL) return false;
    usage_data_defaults(out);

    const char *p = skip_ws(json);
    if (*p != '{') return false;
    p++;

    bool saw_current = false;
    bool saw_weekly  = false;

    while (*p != '\0') {
        p = skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }
        if (*p != '"') return false;

        char key[USAGE_KEY_MAX];
        if (!parse_str_val(&p, key, sizeof(key))) return false;

        p = skip_ws(p);
        if (*p != ':') return false;
        p++;
        p = skip_ws(p);

        if (*p == '"') {
            char val[USAGE_LABEL_MAX];
            if (!parse_str_val(&p, val, sizeof(val))) return false;
            if (strcmp(key, USAGE_KEY_WEEKLY_LABEL) == 0) {
                memcpy(out->weekly_reset_label, val, sizeof(out->weekly_reset_label));
                out->weekly_reset_label[sizeof(out->weekly_reset_label) - 1u] = '\0';
            }
        } else {
            int v = 0;
            if (!parse_int_val(&p, &v)) return false;
            apply_int(out, key, v, &saw_current, &saw_weekly);
        }
    }

    out->valid = saw_current && saw_weekly;
    return out->valid;
}

/* ── 时间 / 文案 ──────────────────────────────────────────────────────────── */

int usage_elapsed_s(uint32_t rx_ms, uint32_t now_ms)
{
    const uint32_t dt = now_ms - rx_ms;     /* 无符号回绕安全 */
    return (int)(dt / USAGE_MS_PER_SECOND);
}

int usage_countdown_s(const usage_data_t *d, uint32_t now_ms)
{
    if (d == NULL) return 0;
    const int left = d->current_resets_in - usage_elapsed_s(d->rx_ms, now_ms);
    return left > 0 ? left : 0;
}

bool usage_is_stale(const usage_data_t *d, uint32_t now_ms, uint32_t timeout_ms)
{
    if (d == NULL || !d->valid) return true;
    return (uint32_t)(now_ms - d->rx_ms) > timeout_ms;
}

int usage_format_countdown(int countdown_s, char *out, size_t n)
{
    if (out == NULL || n == 0u) return 0;
    if (countdown_s <= 0) return snprintf(out, n, "%s", USAGE_TEXT_RESETTING);

    const int days  = countdown_s / USAGE_SEC_PER_DAY;
    const int hours = (countdown_s % USAGE_SEC_PER_DAY) / USAGE_SEC_PER_HOUR;
    const int mins  = (countdown_s % USAGE_SEC_PER_HOUR) / USAGE_SEC_PER_MINUTE;
    if (days > 0) return snprintf(out, n, USAGE_TEXT_RESET_IN, days, hours);
    return snprintf(out, n, USAGE_TEXT_RESET_CLOCK, hours, mins);
}

int usage_format_weekly(const char *label, char *out, size_t n)
{
    if (out == NULL || n == 0u) return 0;
    if (label == NULL || label[0] == '\0') return snprintf(out, n, "%s", USAGE_TEXT_RESET_SOON);
    return snprintf(out, n, USAGE_TEXT_RESET_ABS, label);
}

const char *usage_plan_name(usage_plan_t plan)
{
    switch (plan) {
    case USAGE_PLAN_FREE:       return "FREE";
    case USAGE_PLAN_GO:         return "GO";
    case USAGE_PLAN_PLUS:       return "PLUS";
    case USAGE_PLAN_PRO:        return "PRO";
    case USAGE_PLAN_TEAM:       return "TEAM";
    case USAGE_PLAN_ENTERPRISE: return "ENT";
    default:                    return USAGE_TEXT_UNKNOWN_PLAN;
    }
}
