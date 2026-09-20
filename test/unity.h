#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int unity_tests_run;
extern int unity_tests_failed;
extern int unity_tests_ignored;

void unity_begin(void);
int unity_end(void);
void unity_test_fail(const char *file, int line, const char *msg);
void unity_test_pass(const char *func_name);

#define UNITY_BEGIN() unity_begin()
#define UNITY_END()   unity_end()

#define RUN_TEST(func) do { \
    unity_tests_run++; \
    printf("TEST(%s): ", #func); \
    func(); \
    unity_test_pass(#func); \
} while(0)

#define TEST_ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        unity_test_fail(__FILE__, __LINE__, "Expression evaluated to false: " #condition); \
        return; \
    } \
} while(0)

#define TEST_ASSERT_FALSE(condition) TEST_ASSERT_TRUE(!(condition))
#define TEST_ASSERT_EQUAL(expected, actual) TEST_ASSERT_TRUE((expected) == (actual))

#ifdef __cplusplus
}
#endif

#endif // UNITY_H
