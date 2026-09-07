/*
 * minitest.h — 零依赖的极简单元测试框架（主机侧）。
 *
 * 之所以自带而不是引入 Unity/GoogleTest：单元测试必须能在没有网络、
 * 没有额外工具链的机器上 `make test` 直接跑起来（含 CI）。
 */
#ifndef MINITEST_H
#define MINITEST_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*mt_test_fn)(void);

void mt_register(const char *name, mt_test_fn fn);
void mt_report_failure(const char *file, int line, const char *expr);

#define MT_TEST(name)                                                        \
    static void name(void);                                                  \
    static __attribute__((constructor)) void name##_mt_register(void)        \
    {                                                                        \
        mt_register(#name, name);                                            \
    }                                                                        \
    static void name(void)

#define CHECK(cond) \
    mt_check_true((cond) ? true : false, __FILE__, __LINE__, #cond)

#define CHECK_TRUE(cond) CHECK(cond)

#define CHECK_FALSE(cond) \
    mt_check_true((cond) ? false : true, __FILE__, __LINE__, "!(" #cond ")")

#define CHECK_EQ(actual, expected) \
    mt_check_int_eq((long long)(actual), (long long)(expected), __FILE__, __LINE__, \
                    #actual, #expected)

#define CHECK_NE(actual, unexpected) \
    mt_check_int_ne((long long)(actual), (long long)(unexpected), __FILE__, __LINE__, \
                    #actual, #unexpected)

#define CHECK_NEAR(actual, expected, eps)                                    \
    mt_check_dbl_near((double)(actual), (double)(expected), (double)(eps),    \
                      __FILE__, __LINE__, #actual, #expected)

#define CHECK_STR_EQ(actual, expected)                                       \
    mt_check_str_eq((actual), (expected), __FILE__, __LINE__, #actual, #expected)

void mt_check_true(bool ok, const char *file, int line, const char *expr);
void mt_check_int_eq(long long actual, long long expected, const char *file, int line,
                     const char *actual_expr, const char *expected_expr);
void mt_check_int_ne(long long actual, long long unexpected, const char *file, int line,
                     const char *actual_expr, const char *unexpected_expr);
void mt_check_dbl_near(double actual, double expected, double eps, const char *file, int line,
                       const char *actual_expr, const char *expected_expr);
void mt_check_str_eq(const char *actual, const char *expected, const char *file, int line,
                     const char *actual_expr, const char *expected_expr);

#ifdef __cplusplus
}
#endif

#endif /* MINITEST_H */
