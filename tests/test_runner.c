#include "test_framework.h"
#include <stdio.h>

/* global counters — shared via extern in test_framework.h */
int g_tests_run    = 0;
int g_tests_failed = 0;
int g_current_fail = 0;

void run_protocol_tests(void);
void run_ds_tests(void);
void run_server_tests(void);

int main(void) {
    printf("══════════════════════════════════════════════════════\n");
    printf("  LAN Chat — Unit Test Suite\n");
    printf("══════════════════════════════════════════════════════\n");

    run_protocol_tests();
    run_ds_tests();
    run_server_tests();

    PRINT_SUMMARY();
    return g_tests_failed > 0 ? 1 : 0;
}
