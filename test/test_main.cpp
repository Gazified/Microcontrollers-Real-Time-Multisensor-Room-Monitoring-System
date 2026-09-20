#include <unity.h>
#include "alarm.h"
#include "display.h"
#include "system_state.h"

// Category A: Temperature Alarm Logic (5 required tests)
void test_alarm_below_lower_threshold(void)
{
    // Test below 18.0 C
    TEST_ASSERT_TRUE(evaluateTemperature(17.9f) == AlarmState::LOW_TEMPERATURE);
    TEST_ASSERT_TRUE(evaluateTemperature(10.0f) == AlarmState::LOW_TEMPERATURE);
}

void test_alarm_exactly_lower_threshold(void)
{
    // Boundary test: exactly 18.0 C must be NORMAL
    TEST_ASSERT_TRUE(evaluateTemperature(18.0f) == AlarmState::NORMAL);
}

void test_alarm_normal_value(void)
{
    // Test inside normal range (18.0 C to 30.0 C)
    TEST_ASSERT_TRUE(evaluateTemperature(24.5f) == AlarmState::NORMAL);
}

void test_alarm_exactly_upper_threshold(void)
{
    // Boundary test: exactly 30.0 C must be NORMAL
    TEST_ASSERT_TRUE(evaluateTemperature(30.0f) == AlarmState::NORMAL);
}

void test_alarm_above_upper_threshold(void)
{
    // Test above 30.0 C
    TEST_ASSERT_TRUE(evaluateTemperature(30.1f) == AlarmState::HIGH_TEMPERATURE);
    TEST_ASSERT_TRUE(evaluateTemperature(42.0f) == AlarmState::HIGH_TEMPERATURE);
}

// Category B: Display Navigation & Wraparound (4 required tests)
void test_navigation_forward_transition(void)
{
    TEST_ASSERT_TRUE(nextDisplayMode(DisplayMode::TEMPERATURE) == DisplayMode::HUMIDITY);
    TEST_ASSERT_TRUE(nextDisplayMode(DisplayMode::HUMIDITY) == DisplayMode::LIGHT);
    TEST_ASSERT_TRUE(nextDisplayMode(DisplayMode::LIGHT) == DisplayMode::MOTION);
}

void test_navigation_reverse_transition(void)
{
    TEST_ASSERT_TRUE(previousDisplayMode(DisplayMode::MOTION) == DisplayMode::LIGHT);
    TEST_ASSERT_TRUE(previousDisplayMode(DisplayMode::LIGHT) == DisplayMode::HUMIDITY);
    TEST_ASSERT_TRUE(previousDisplayMode(DisplayMode::HUMIDITY) == DisplayMode::TEMPERATURE);
}

void test_navigation_forward_wraparound(void)
{
    // Clockwise wraparound from MOTION to TEMPERATURE
    TEST_ASSERT_TRUE(nextDisplayMode(DisplayMode::MOTION) == DisplayMode::TEMPERATURE);
}

void test_navigation_reverse_wraparound(void)
{
    // Counter-clockwise wraparound from TEMPERATURE to MOTION
    TEST_ASSERT_TRUE(previousDisplayMode(DisplayMode::TEMPERATURE) == DisplayMode::MOTION);
}

// Category C: System State Machine (4 required tests)
void test_state_active_no_timeout(void)
{
    // System is ACTIVE, no motion, but elapsed time (5s) < timeout (15s) -> remain ACTIVE
    TEST_ASSERT_TRUE(evaluateSystemState(SystemState::ACTIVE, false, 5, 15) == SystemState::ACTIVE);
}

void test_state_active_timeout(void)
{
    // System is ACTIVE, no motion, elapsed time (15s) >= timeout (15s) -> transition to INACTIVE
    TEST_ASSERT_TRUE(evaluateSystemState(SystemState::ACTIVE, false, 15, 15) == SystemState::INACTIVE);
    TEST_ASSERT_TRUE(evaluateSystemState(SystemState::ACTIVE, false, 20, 15) == SystemState::INACTIVE);
}

void test_state_inactive_no_motion(void)
{
    // System is INACTIVE, no motion -> remain INACTIVE
    TEST_ASSERT_TRUE(evaluateSystemState(SystemState::INACTIVE, false, 30, 15) == SystemState::INACTIVE);
}

void test_state_inactive_motion(void)
{
    // System is INACTIVE, motion occurs -> restore to ACTIVE
    TEST_ASSERT_TRUE(evaluateSystemState(SystemState::INACTIVE, true, 30, 15) == SystemState::ACTIVE);
}

void runAllUnitTests(void)
{
    UNITY_BEGIN();

    // Category A
    RUN_TEST(test_alarm_below_lower_threshold);
    RUN_TEST(test_alarm_exactly_lower_threshold);
    RUN_TEST(test_alarm_normal_value);
    RUN_TEST(test_alarm_exactly_upper_threshold);
    RUN_TEST(test_alarm_above_upper_threshold);

    // Category B
    RUN_TEST(test_navigation_forward_transition);
    RUN_TEST(test_navigation_reverse_transition);
    RUN_TEST(test_navigation_forward_wraparound);
    RUN_TEST(test_navigation_reverse_wraparound);

    // Category C
    RUN_TEST(test_state_active_no_timeout);
    RUN_TEST(test_state_active_timeout);
    RUN_TEST(test_state_inactive_no_motion);
    RUN_TEST(test_state_inactive_motion);

    UNITY_END();
}

#if defined(ESP_PLATFORM)
extern "C" void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    runAllUnitTests();
}
#else
int main(int argc, char **argv)
{
    runAllUnitTests();
    return 0;
}
#endif
