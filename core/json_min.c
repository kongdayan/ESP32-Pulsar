/* json_min.c — 见 json_min.h。 */
#include "json_min.h"

const char *json_skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

bool json_parse_int(const char **pp, int *out)
{
    const char *p = *pp;
    bool neg = false;
    if (*p == '-') {
        neg = true;
        p++;
    }
    if (*p < '0' || *p > '9') return false;

    /* 用无符号累加 + 预判，避免 long 在 32 位平台（ESP32 ILP32）上溢出 */
    const uint32_t limit = neg ? (uint32_t)JSON_MIN_INT_MAX + 1u
                               : (uint32_t)JSON_MIN_INT_MAX;
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') {
        const uint32_t d = (uint32_t)(*p - '0');
        if (v > (limit - d) / 10u) return false;
        v = v * 10u + d;
        p++;
    }
    if (out != NULL) {
        *out = neg ? (int)(0u - v) : (int)v;   /* -2147483648 也能正确表示 */
    }
    *pp = p;
    return true;
}

bool json_parse_str(const char **pp, char *out, size_t n)
{
    const char *p = *pp;
    if (*p != '"') return false;
    p++;

    size_t i = 0;
    while (*p != '\0' && *p != '"') {
        if (*p == '\\' && p[1] != '\0') p++;   /* 简化转义 */
        if (out != NULL && i + 1u < n) out[i++] = *p;
        p++;
    }
    if (*p != '"') return false;
    p++;
    if (out != NULL && n > 0u) out[i] = '\0';
    *pp = p;
    return true;
}
