#include "unity.h"

int unity_tests_run = 0;
int unity_tests_failed = 0;
int unity_tests_ignored = 0;
static bool s_current_test_failed = false;

void unity_begin(void)
{
    unity_tests_run = 0;
    unity_tests_failed = 0;
    unity_tests_ignored = 0;
    s_current_test_failed = false;
    printf("\n=======================================================\n");
    printf("        BCA152 FIRMWARE LOGIC AUTOMATED UNIT TESTS     \n");
    printf("=======================================================\n");
}

void unity_test_fail(const char *file, int line, const char *msg)
{
    unity_tests_failed++;
    s_current_test_failed = true;
    printf("FAIL (%s:%d: %s)\n", file, line, msg);
}

void unity_test_pass(const char *func_name)
{
    if (!s_current_test_failed) {
        printf("PASS\n");
    }
    s_current_test_failed = false;
}

int unity_end(void)
{
    printf("-------------------------------------------------------\n");
    printf("%d Tests %d Failures %d Ignored\n", unity_tests_run, unity_tests_failed, unity_tests_ignored);
    if (unity_tests_failed == 0) {
        printf("RESULT: ALL UNIT TESTS PASSED (OK)\n");
        printf("=======================================================\n\n");
        return 0;
    } else {
        printf("RESULT: UNIT TESTS FAILED\n");
        printf("=======================================================\n\n");
        return 1;
    }
}
