/* A deliberately tiny test framework: no external dependency, no
 * discovery magic. Each test_*.c file exposes one `void run_xxx_tests
 * (int *run, int *failed)` that tests/main.c calls in turn. */
#ifndef CALM_TEST_FRAMEWORK_H
#define CALM_TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

#define CHECK(run, failed, cond)                                                     \
    do {                                                                             \
        (*(run))++;                                                                  \
        if (!(cond)) {                                                               \
            (*(failed))++;                                                           \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                            \
    } while (0)

#define CHECK_STR_EQ(run, failed, actual, expected)                                                     \
    do {                                                                                                 \
        (*(run))++;                                                                                      \
        const char *_a = (actual);                                                                       \
        const char *_e = (expected);                                                                      \
        if (_a == NULL || _e == NULL || strcmp(_a, _e) != 0) {                                             \
            (*(failed))++;                                                                                 \
            fprintf(stderr, "FAIL %s:%d: expected \"%s\", got \"%s\"\n", __FILE__, __LINE__, _e ? _e : "(null)", \
                    _a ? _a : "(null)");                                                                    \
        }                                                                                                  \
    } while (0)

#endif /* CALM_TEST_FRAMEWORK_H */
