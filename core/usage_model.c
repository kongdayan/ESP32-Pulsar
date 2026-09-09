/*
 * usage_model.c — 见 usage_model.h。纯函数 + 每个 provider 一个 seqlock 槽。
 */
#include "usage_model.h"

#include <stdio.h>
#include <string.h>

#include "json_min.h"

#define USAGE_MS_PER_SECOND     1000u
#define USAGE_STORE_RETRIES     4

#define USAGE_TEXT_RESETTING    "Resetting..."
#define USAGE_TEXT_RESET_SOON   "Resets soon"
#define USAGE_TEXT_RESET_ABS    "Resets %s"
#define USAGE_TEXT_RESET_IN     "Resets in %dd %dh"
#define USAGE_TEXT_RESET_CLOCK  "Resets in %02d:%02d"
#define USAGE_TEXT_UNKNOWN      "?"
#define USAGE_TEXT_UNKNOWN_PROV "unknown"

/* ── provider 表（加新服务：补枚举 + 这里补一行） ─────────────────────────── */
const usage_provider_info_t usage_providers[USAGE_PROVIDER_COUNT] = {
    [USAGE_PROVIDER_CODEX]  = { "codex",  "CODEX"  },
    [USAGE_PROVIDER_CLAUDE] = { "claude", "CLAUDE" },
    [USAGE_PROVIDER_NVIDIA] = { "nvidia", "NVIDIA" },
    [USAGE_PROVIDER_AMD]    = { "amd",    "AMD"    },
    [USAGE_PROVIDER_GLM]    = { "glm",    "GLM"    },
};

bool usage_provider_is_valid(usage_provider_t p)
{
    return (unsigned)p < (unsigned)USAGE_PROVIDER_COUNT;
}

const char *usage_provider_key(usage_provider_t p)
{
    return usage_provider_is_valid(p) ? usage_providers[p].key : USAGE_TEXT_UNKNOWN_PROV;
}

const char *usage_provider_title(usage_provider_t p)
{
    return usage_provider_is_valid(p) ? usage_providers[p].title : USAGE_TEXT_UNKNOWN;
}

/* ── 共享存储（每个 provider 一个 seqlock 槽） ────────────────────────────── */

static volatile uint32_t s_seq[USAGE_PROVIDER_COUNT];
static usage_data_t      s_slots[USAGE_PROVIDER_COUNT];

void usage_data_defaults(usage_data_t *d)
{
    if (d == NULL) return;
    memset(d, 0, sizeof(*d));
    d->provider = USAGE_PROVIDER_CODEX;
    d->plan = USAGE_PLAN_UNKNOWN;
}

void usage_store_set(const usage_data_t *d)
{
    /* 非法 provider 直接丢弃，不落槽（避免误写覆盖 Codex） */
    if (d == NULL || !usage_provider_is_valid(d->provider)) return;
    const usage_provider_t p = d->provider;
    s_seq[p]++;                 /* 奇数 = 写入中 */
    __sync_synchronize();
    s_slots[p] = *d;
    __sync_synchronize();
    s_seq[p]++;                 /* 偶数 = 完成 */
}

bool usage_store_get(usage_provider_t p, usage_data_t *out)
{
    if (out == NULL || !usage_provider_is_valid(p)) return false;
    for (int i = 0; i < USAGE_STORE_RETRIES; i++) {
        const uint32_t s1 = s_seq[p];
        if ((s1 & 1u) != 0u) continue;
        __sync_synchronize();
        *out = s_slots[p];
        __sync_synchronize();
        if (s_seq[p] == s1) return true;
    }
    return false;
}

void usage_store_reset_provider(usage_provider_t p)
{
    if (!usage_provider_is_valid(p)) return;
    usage_data_t d;
    usage_data_defaults(&d);
    d.provider = p;
    usage_store_set(&d);
}

void usage_store_reset(void)
{
    for (int p = 0; p < USAGE_PROVIDER_COUNT; p++) {
        usage_store_reset_provider((usage_provider_t)p);
    }
}

/* ── 解析 ────────────────────────────────────────────────────────────────── */

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
    if (strcmp(key, USAGE_KEY_PROVIDER) == 0) {
        const usage_provider_t p = (usage_provider_t)v;
        if (usage_provider_is_valid(p)) d->provider = p;
    } else if (strcmp(key, USAGE_KEY_CURRENT_PCT) == 0) {
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

    const char *p = json_skip_ws(json);
    if (*p != '{') return false;
    p++;

    bool saw_current = false;
    bool saw_weekly  = false;

    while (*p != '\0') {
        p = json_skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }
        if (*p != '"') return false;

        char key[USAGE_KEY_MAX];
        if (!json_parse_str(&p, key, sizeof(key))) return false;

        p = json_skip_ws(p);
        if (*p != ':') return false;
        p++;
        p = json_skip_ws(p);

        if (*p == '"') {
            if (strcmp(key, USAGE_KEY_WEEKLY_LABEL) == 0) {
                if (!json_parse_str(&p, out->weekly_reset_label,
                                    sizeof(out->weekly_reset_label))) return false;
            } else {
                if (!json_parse_str(&p, NULL, 0)) return false;
            }
        } else {
            int v = 0;
            if (!json_parse_int(&p, &v)) return false;
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
    default:                    return USAGE_TEXT_UNKNOWN;
    }
}
