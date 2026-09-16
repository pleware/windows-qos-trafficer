#ifndef TEST_H
#define TEST_H

// Minimal self-contained test harness (no external deps).

#include <stdio.h>

extern int g_checks;
extern int g_failures;

#define CHECK(cond) do { \
    g_checks++; \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while (0)

#define CHECK_INT(a, b) do { \
    g_checks++; \
    long long _a = (long long)(a); \
    long long _b = (long long)(b); \
    if (_a != _b) { \
        g_failures++; \
        fprintf(stderr, "FAIL %s:%d: %s == %s  (%lld != %lld)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
    } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
    g_checks++; \
    double _a = (double)(a); \
    double _b = (double)(b); \
    double _e = (double)(eps); \
    if ((_a > _b + _e) || (_a < _b - _e)) { \
        g_failures++; \
        fprintf(stderr, "FAIL %s:%d: %s ~= %s  (%.6f vs %.6f)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
    } \
} while (0)

#endif // TEST_H
