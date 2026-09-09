/* balance_model.c — 见 balance_model.h。 */
#include "balance_model.h"

#include <stdio.h>
#include <string.h>

#include "json_min.h"

#define BALANCE_STORE_RETRIES 4
#define BALANCE_TEXT_NEGATIVE "-"

/* ── 共享存储（单写多读 seqlock） ─────────────────────────────────────────── */

static volatile uint32_t s_seq;
static balance_data_t    s_data;

void balance_data_defaults(balance_data_t *d)
{
    if (d == NULL) return;
    memset(d, 0, sizeof(*d));
}

void balance_store_set(const balance_data_t *d)
{
    if (d == NULL) return;
    s_seq++;
    __sync_synchronize();
    s_data = *d;
    __sync_synchronize();
    s_seq++;
}

bool balance_store_get(balance_data_t *out)
{
    if (out == NULL) return false;
    for (int i = 0; i < BALANCE_STORE_RETRIES; i++) {
        const uint32_t s1 = s_seq;
        if ((s1 & 1u) != 0u) continue;
        __sync_synchronize();
        *out = s_data;
        __sync_synchronize();
        if (s_seq == s1) return true;
    }
    return false;
}

void balance_store_reset(void)
{
    balance_data_t d;
    balance_data_defaults(&d);
    balance_store_set(&d);
}

/* ── 解析 ────────────────────────────────────────────────────────────────── */

static int clamp_nonneg(int v)
{
    return v > 0 ? v : 0;
}

static void apply_int(balance_data_t *d, const char *key, int v, bool *saw_total)
{
    if (strcmp(key, BALANCE_KEY_TOTAL) == 0) {
        d->total_cents = clamp_nonneg(v);
        *saw_total = true;
    } else if (strcmp(key, BALANCE_KEY_GRANTED) == 0) {
        d->granted_cents = clamp_nonneg(v);
    } else if (strcmp(key, BALANCE_KEY_TOPPED_UP) == 0) {
        d->topped_up_cents = clamp_nonneg(v);
    } else if (strcmp(key, BALANCE_KEY_AVAILABLE) == 0) {
        d->is_available = (v != 0);
    }
}

bool balance_parse_json(const char *json, balance_data_t *out)
{
    if (json == NULL || out == NULL) return false;
    balance_data_defaults(out);

    const char *p = json_skip_ws(json);
    if (*p != '{') return false;
    p++;

    bool saw_total = false;

    while (*p != '\0') {
        p = json_skip_ws(p);
        if (*p == '}') break;
        if (*p == ',') { p++; continue; }
        if (*p != '"') return false;

        char key[BALANCE_KEY_MAX];
        if (!json_parse_str(&p, key, sizeof(key))) return false;

        p = json_skip_ws(p);
        if (*p != ':') return false;
        p++;
        p = json_skip_ws(p);

        if (*p == '"') {
            if (strcmp(key, BALANCE_KEY_CURRENCY) == 0) {
                if (!json_parse_str(&p, out->currency, sizeof(out->currency))) return false;
            } else {
                if (!json_parse_str(&p, NULL, 0)) return false;
            }
        } else {
            int v = 0;
            if (!json_parse_int(&p, &v)) return false;
            apply_int(out, key, v, &saw_total);
        }
    }

    out->valid = saw_total;
    return out->valid;
}

/* ── 格式化 / 过期 ───────────────────────────────────────────────────────── */

int balance_format_amount(int cents, char *out, size_t n)
{
    if (out == NULL || n == 0u) return 0;
    const bool neg = cents < 0;
    /* 用 64 位取绝对值，避免 INT_MIN 在 32 位平台上取负溢出 */
    const unsigned long v = neg ? (unsigned long)(-(int64_t)cents) : (unsigned long)cents;
    return snprintf(out, n, "%s%lu.%02lu", neg ? BALANCE_TEXT_NEGATIVE : "",
                    v / (unsigned long)BALANCE_CENTS_PER_UNIT,
                    v % (unsigned long)BALANCE_CENTS_PER_UNIT);
}

bool balance_is_stale(const balance_data_t *d, uint32_t now_ms, uint32_t timeout_ms)
{
    if (d == NULL || !d->valid) return true;
    return (uint32_t)(now_ms - d->rx_ms) > timeout_ms;
}
