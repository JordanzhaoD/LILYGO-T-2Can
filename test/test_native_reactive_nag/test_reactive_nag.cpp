#include <unity.h>
#include "dash_reactive_nag.h"

void setUp() {}
void tearDown() {}

void test_inactive_resets_without_burst()
{
    DashReactiveNagBurst nag;
    nag.init(1);
    nag.onNagSample(5, 100, false);
    TEST_ASSERT_EQUAL(NagMode::Idle, nag.mode());
    TEST_ASSERT_FALSE(nag.shouldEcho());
    TEST_ASSERT_EQUAL_UINT32(0, nag.reactiveBursts);
}

void test_reactive_starts_immediately()
{
    DashReactiveNagBurst nag;
    nag.init(2);
    nag.onNagSample(5, 100, true);
    TEST_ASSERT_EQUAL(NagMode::Reactive, nag.mode());
    TEST_ASSERT_TRUE(nag.shouldEcho());
    TEST_ASSERT_EQUAL_UINT32(1, nag.reactiveBursts);
}

void test_reactive_hold_is_positive_and_bounded()
{
    DashReactiveNagBurst nag;
    nag.init(3);
    nag.onNagSample(5, 0, true);
    int sum = 0;
    for (unsigned long now = 0; now < 1200 && nag.shouldEcho(); now += 20)
    {
        int value = nag.computeHold(now);
        TEST_ASSERT_GREATER_THAN_INT(0, value);
        TEST_ASSERT_LESS_OR_EQUAL_INT(DashReactiveNagBurst::kAmplitudeCap, value);
        sum += value;
    }
    TEST_ASSERT_GREATER_THAN_INT(0, sum);
}

void test_proactive_starts_gentle_hold()
{
    DashReactiveNagBurst nag;
    nag.init(4);
    nag.onNagSample(0, 100, true);
    TEST_ASSERT_EQUAL(NagMode::Proactive, nag.mode());
    TEST_ASSERT_TRUE(nag.shouldEcho());
    TEST_ASSERT_EQUAL_UINT32(1, nag.proactiveWiggles);
}

void test_nag_clear_removes_stale_reactive_gap()
{
    DashReactiveNagBurst nag;
    nag.init(5);
    nag.onNagSample(5, 100, true);
    for (unsigned long now = 100; nag.shouldEcho(); now += 500)
        nag.computeHold(now);
    unsigned long clearedAt = nag.lastReactiveEndMs + 200;
    nag.onNagSample(0, clearedAt, true);
    nag.onNagSample(5, clearedAt + 100, true);
    TEST_ASSERT_TRUE(nag.shouldEcho());
    TEST_ASSERT_EQUAL_UINT32(2, nag.reactiveBursts);
}

void test_reactive_gap_limits_repeat_rate()
{
    DashReactiveNagBurst nag;
    nag.init(6);
    nag.onNagSample(5, 100, true);
    for (unsigned long now = 100; nag.shouldEcho(); now += 500)
        nag.computeHold(now);
    unsigned long endedAt = nag.lastReactiveEndMs;
    nag.onNagSample(5, endedAt + 300, true);
    TEST_ASSERT_FALSE(nag.shouldEcho());
    nag.onNagSample(5, endedAt + 900, true);
    TEST_ASSERT_TRUE(nag.shouldEcho());
}

void test_apply_to_frame_preserves_12_bit_range()
{
    DashReactiveNagBurst nag;
    uint8_t data2Low = 0x08;
    uint8_t data3 = 0x12;
    nag.applyToFrame(data2Low, data3, 70);
    TEST_ASSERT_EQUAL_INT(0x0812 + 8 + 70, (data2Low << 8) | data3);

    data2Low = 0x0F;
    data3 = 0xF0;
    nag.applyToFrame(data2Low, data3, 95);
    TEST_ASSERT_EQUAL_HEX16(0x0FFF, (data2Low << 8) | data3);
}

void test_echo_counters_distinguish_success_and_failure()
{
    DashReactiveNagBurst nag;
    nag.recordEcho(true);
    nag.recordEcho(false);
    TEST_ASSERT_EQUAL_UINT32(1, nag.echoSent);
    TEST_ASSERT_EQUAL_UINT32(1, nag.echoFailed);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_inactive_resets_without_burst);
    RUN_TEST(test_reactive_starts_immediately);
    RUN_TEST(test_reactive_hold_is_positive_and_bounded);
    RUN_TEST(test_proactive_starts_gentle_hold);
    RUN_TEST(test_nag_clear_removes_stale_reactive_gap);
    RUN_TEST(test_reactive_gap_limits_repeat_rate);
    RUN_TEST(test_apply_to_frame_preserves_12_bit_range);
    RUN_TEST(test_echo_counters_distinguish_success_and_failure);
    return UNITY_END();
}
