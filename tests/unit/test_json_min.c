/* json_min 原语 UT：两个数据模型（usage / balance）共用这一层。 */
#include "minitest.h"

#include <string.h>

#include "json_min.h"

MT_TEST(test_json_skip_ws)
{
    const char *p = " \t\n\r{\"a\"";
    CHECK_EQ(*json_skip_ws(p), '{');
    CHECK_EQ(*json_skip_ws(""), '\0');
    CHECK_EQ(*json_skip_ws("x"), 'x');
}

MT_TEST(test_json_parse_int)
{
    const char *p;
    int v;

    p = "42,";  CHECK(json_parse_int(&p, &v)); CHECK_EQ(v, 42);  CHECK_EQ(*p, ',');
    p = "-7}";  CHECK(json_parse_int(&p, &v)); CHECK_EQ(v, -7);  CHECK_EQ(*p, '}');
    p = "0";    CHECK(json_parse_int(&p, &v)); CHECK_EQ(v, 0);
    p = "2147483647"; CHECK(json_parse_int(&p, &v)); CHECK_EQ(v, 2147483647);

    /* out 为 NULL 时只跳过 */
    p = "123x"; CHECK(json_parse_int(&p, NULL)); CHECK_EQ(*p, 'x');

    /* 非法 */
    p = "x";  CHECK_FALSE(json_parse_int(&p, &v));
    p = "-";  CHECK_FALSE(json_parse_int(&p, &v));
    p = "";   CHECK_FALSE(json_parse_int(&p, &v));
    p = "9999999999"; CHECK_FALSE(json_parse_int(&p, &v));   /* 溢出 int */
}

MT_TEST(test_json_parse_str)
{
    const char *p;
    char buf[16];

    p = "\"CNY\"";   CHECK(json_parse_str(&p, buf, sizeof(buf))); CHECK_STR_EQ(buf, "CNY"); CHECK_EQ(*p, '\0');
    p = "\"a b\",";  CHECK(json_parse_str(&p, buf, sizeof(buf))); CHECK_STR_EQ(buf, "a b"); CHECK_EQ(*p, ',');
    p = "\"\"";      CHECK(json_parse_str(&p, buf, sizeof(buf))); CHECK_STR_EQ(buf, "");

    /* 截断但不溢出 */
    p = "\"0123456789abcdef\"";
    CHECK(json_parse_str(&p, buf, sizeof(buf)));
    CHECK_EQ(strlen(buf), sizeof(buf) - 1u);

    /* out 为 NULL 只跳过 */
    p = "\"skip\"x"; CHECK(json_parse_str(&p, NULL, 0)); CHECK_EQ(*p, 'x');

    /* 非法 */
    p = "no";   CHECK_FALSE(json_parse_str(&p, buf, sizeof(buf)));
    p = "\"abc"; CHECK_FALSE(json_parse_str(&p, buf, sizeof(buf)));
}
