/*
 * json_min.h — 极简 JSON 取值原语（纯逻辑，无依赖）。
 *
 * 本项目两端传的都是「一层扁平对象、值为 int / string」这种最小 JSON，
 * 不需要完整解析器；这三个原语够用、易测、体积小。
 */
#ifndef CORE_JSON_MIN_H
#define CORE_JSON_MIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JSON_MIN_INT_MAX 2147483647L

/* 跳过空白（空格 / 制表 / 换行 / 回车） */
const char *json_skip_ws(const char *p);

/* 解析可选负号整数；成功则推进 *pp，并在 out 非 NULL 时写入 */
bool json_parse_int(const char **pp, int *out);

/* 解析双引号字符串（简化转义：反斜杠只跳过下一字符）；
 * 成功则推进 *pp；out 为 NULL 时只跳过，n 为缓冲区大小（含结尾 0） */
bool json_parse_str(const char **pp, char *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* CORE_JSON_MIN_H */
