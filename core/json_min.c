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

    long v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        if (v > JSON_MIN_INT_MAX) return false;
        p++;
    }
    if (out != NULL) *out = (int)(neg ? -v : v);
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
    if (out != NULL) out[i] = '\0';
    *pp = p;
    return true;
}
