#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>

/* defined in test_runner.c */
extern int g_tests_run;
extern int g_tests_failed;
extern int g_current_fail;

/* regular assert — keeps running even if this one fails */
#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("\n      FAIL line %d: %s", __LINE__, #cond); \
        g_current_fail = 1; \
    } \
} while(0)

/* fatal assert — stops the test immediately (use after NULL checks) */
#define ASSERT_FATAL(cond) do { \
    if (!(cond)) { \
        printf("\n      FAIL line %d: %s", __LINE__, #cond); \
        g_current_fail = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b)     ASSERT((a) == (b))
#define ASSERT_NEQ(a, b)    ASSERT((a) != (b))
#define ASSERT_NULL(p)      ASSERT((p) == NULL)
#define ASSERT_NOT_NULL(p)  ASSERT((p) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

#define RUN_TEST(fn) do { \
    g_tests_run++; \
    g_current_fail = 0; \
    printf("  %-50s", #fn); \
    fn(); \
    if (!g_current_fail) printf("PASS\n"); \
    else { printf("\n"); g_tests_failed++; } \
} while(0)

#define TEST_SUITE(name) \
    printf("\n── %s ", name); \
    for (int _i = 0; _i < (int)(40 - strlen(name)); _i++) printf("─"); \
    printf("\n")

#define PRINT_SUMMARY() do { \
    printf("\n══════════════════════════════════════════════════════\n"); \
    printf("  Total: %-4d  Passed: %-4d  Failed: %d\n", \
           g_tests_run, g_tests_run - g_tests_failed, g_tests_failed); \
    printf("══════════════════════════════════════════════════════\n"); \
} while(0)

#endif /* TEST_FRAMEWORK_H */
