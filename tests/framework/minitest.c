#include "minitest.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MT_MAX_TESTS 256

typedef struct {
    const char *name;
    mt_test_fn fn;
} mt_test_t;

static mt_test_t s_tests[MT_MAX_TESTS];
static int s_test_count;
static int s_failures_in_test;
static int s_total_failures;
static int s_passed_tests;
static int s_failed_tests;

void mt_register(const char *name, mt_test_fn fn)
{
    if (s_test_count >= MT_MAX_TESTS) {
        fprintf(stderr, "minitest: registry full (raise MT_MAX_TESTS)\n");
        return;
    }
    s_tests[s_test_count].name = name;
    s_tests[s_test_count].fn = fn;
    s_test_count++;
}

void mt_report_failure(const char *file, int line, const char *detail)
{
    s_failures_in_test++;
    s_total_failures++;
    printf("    FAIL %s:%d: %s\n", file, line, detail);
}

void mt_check_true(bool ok, const char *file, int line, const char *expr)
{
    if (ok) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "CHECK(%s)", expr);
    mt_report_failure(file, line, buf);
}

void mt_check_int_eq(long long actual, long long expected, const char *file, int line,
                     const char *actual_expr, const char *expected_expr)
{
    if (actual == expected) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s == %s  (%lld vs %lld)",
             actual_expr, expected_expr, actual, expected);
    mt_report_failure(file, line, buf);
}

void mt_check_int_ne(long long actual, long long unexpected, const char *file, int line,
                     const char *actual_expr, const char *unexpected_expr)
{
    if (actual != unexpected) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s != %s  (both %lld)",
             actual_expr, unexpected_expr, actual);
    mt_report_failure(file, line, buf);
}

void mt_check_dbl_near(double actual, double expected, double eps, const char *file, int line,
                       const char *actual_expr, const char *expected_expr)
{
    const double diff = (actual > expected) ? (actual - expected) : (expected - actual);
    if (diff <= eps) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s ~= %s  (%.9g vs %.9g, eps %.3g)",
             actual_expr, expected_expr, actual, expected, eps);
    mt_report_failure(file, line, buf);
}

void mt_check_str_eq(const char *actual, const char *expected, const char *file, int line,
                     const char *actual_expr, const char *expected_expr)
{
    if (actual == NULL && expected == NULL) return;
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s == %s  (\"%s\" vs \"%s\")",
             actual_expr, expected_expr,
             actual != NULL ? actual : "(null)",
             expected != NULL ? expected : "(null)");
    mt_report_failure(file, line, buf);
}

int main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;

    if (filter != NULL) {
        printf("running tests matching \"%s\" (of %d registered)\n", filter, s_test_count);
    } else {
        printf("running %d test%s\n", s_test_count, s_test_count == 1 ? "" : "s");
    }

    for (int i = 0; i < s_test_count; i++) {
        if (filter != NULL && strstr(s_tests[i].name, filter) == NULL) continue;

        s_failures_in_test = 0;
        printf("  - %s\n", s_tests[i].name);
        fflush(stdout);
        s_tests[i].fn();

        if (s_failures_in_test == 0) {
            s_passed_tests++;
        } else {
            s_failed_tests++;
        }
    }

    printf("\n%d passed, %d failed, %d assertion failures\n",
           s_passed_tests, s_failed_tests, s_total_failures);

    return (s_total_failures == 0) ? 0 : 1;
}
