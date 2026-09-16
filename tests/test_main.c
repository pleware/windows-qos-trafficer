// test_main.c
// Test harness entry point. Runs all suites; returns non-zero on failure.

#include "test.h"

int g_checks = 0;
int g_failures = 0;

void test_qos_math(void);
void test_schedule(void);
void test_token_bucket(void);

int main(void) {
    test_qos_math();
    test_schedule();
    test_token_bucket();

    if (g_failures == 0) {
        printf("All %d checks passed.\n", g_checks);
        return 0;
    }
    printf("%d/%d checks FAILED.\n", g_failures, g_checks);
    return 1;
}
