#include <unity.h>
#include <initializer_list>
#include "dash_legacy_nag_v4.h"

void setUp() {}
void tearDown() {}

static DashLegacyNagV4 makeEnabled()
{
    DashLegacyNagV4 nag;
    nag.configure(true);
    return nag;
}

static void feedSteering(DashLegacyNagV4 &nag, int16_t angleX10, uint32_t nowMs,
                         uint8_t controlType = 1, uint8_t bus = 4)
{
    nag.onSteeringRequest(controlType, angleX10, bus, nowMs);
}

void test_disabled_never_sends()
{
    DashLegacyNagV4 nag;
    feedSteering(nag, 100, 100);
    nag.onContext(true, 2, 100);
    TEST_ASSERT_FALSE(nag.decide(2200, true).send);
}

void test_missing_stale_and_invalid_488_fail_closed()
{
    DashLegacyNagV4 nag = makeEnabled();
    nag.onContext(true, 2, 100);
    DashLegacyNagV4Decision missing = nag.decide(2200, true);
    TEST_ASSERT_FALSE(missing.send);
    TEST_ASSERT_EQUAL(LegacyNagV4Block::SteeringMissing, missing.blocked);

    feedSteering(nag, 100, 100, 0);
    DashLegacyNagV4Decision invalid = nag.decide(2200, true);
    TEST_ASSERT_FALSE(invalid.send);
    TEST_ASSERT_EQUAL(LegacyNagV4Block::SteeringInvalid, invalid.blocked);

    feedSteering(nag, 100, 100, 1);
    DashLegacyNagV4Decision stale = nag.decide(2200, true);
    TEST_ASSERT_FALSE(stale.send);
    TEST_ASSERT_EQUAL(LegacyNagV4Block::SteeringStale, stale.blocked);
}

void test_state2_waits_then_walks_one_sided_opposite_488()
{
    DashLegacyNagV4 nag = makeEnabled();
    nag.onContext(true, 2, 100);
    feedSteering(nag, 100, 1000);
    DashLegacyNagV4Decision delay = nag.decide(1000, true);
    TEST_ASSERT_FALSE(delay.send);
    TEST_ASSERT_EQUAL(LegacyNagV4Block::StateDelay, delay.blocked);

    for (uint32_t now = 2200; now < 3200; now += 40)
    {
        feedSteering(nag, 100, now);
        DashLegacyNagV4Decision d = nag.decide(now, true);
        TEST_ASSERT_TRUE(d.send);
        TEST_ASSERT_LESS_THAN_INT(0, d.torqueCentiNm);
        TEST_ASSERT_GREATER_OR_EQUAL_INT(-180, d.torqueCentiNm);
        TEST_ASSERT_EQUAL_UINT8(0, d.handsOnLevel);
        TEST_ASSERT_EQUAL(LegacyNagV4State::Support, d.state);
    }
    TEST_ASSERT_EQUAL_INT8(-1, nag.episodeDirection);
}

void test_center_deadband_reuses_last_valid_direction()
{
    DashLegacyNagV4 nag = makeEnabled();
    nag.onContext(true, 2, 100);
    feedSteering(nag, -100, 2000);
    TEST_ASSERT_TRUE(nag.decide(2200, true).send);
    feedSteering(nag, 0, 2240);
    DashLegacyNagV4Decision centered = nag.decide(2240, true);
    TEST_ASSERT_TRUE(centered.send);
    TEST_ASSERT_GREATER_THAN_INT(0, centered.torqueCentiNm);
    TEST_ASSERT_EQUAL_INT8(1, nag.episodeDirection);
}

void test_center_without_prior_direction_fails_closed()
{
    DashLegacyNagV4 nag = makeEnabled();
    nag.onContext(true, 2, 100);
    feedSteering(nag, 0, 2200);
    DashLegacyNagV4Decision d = nag.decide(2200, true);
    TEST_ASSERT_FALSE(d.send);
    TEST_ASSERT_EQUAL(LegacyNagV4Block::CenterNoDirection, d.blocked);
}

void test_direction_change_is_slew_limited()
{
    DashLegacyNagV4 nag = makeEnabled();
    nag.onContext(true, 2, 100);
    for (uint32_t now = 2200; now < 2600; now += 40)
    {
        feedSteering(nag, 100, now);
        nag.decide(now, true);
    }
    int before = nag.lastOutputCentiNm;
    feedSteering(nag, -100, 2600);
    DashLegacyNagV4Decision flipped = nag.decide(2600, true);
    TEST_ASSERT_EQUAL_INT(DashLegacyNagV4::kOutputSlewCentiNm,
                          flipped.torqueCentiNm - before);
    TEST_ASSERT_EQUAL_UINT32(1, nag.directionChangeCount);
}

void test_strong_state_delays_then_ramps_to_2_1_nm()
{
    DashLegacyNagV4 nag = makeEnabled();
    nag.onContext(true, 3, 100);
    feedSteering(nag, -100, 1000);
    DashLegacyNagV4Decision delay = nag.decide(1000, true);
    TEST_ASSERT_FALSE(delay.send);
    TEST_ASSERT_EQUAL(LegacyNagV4Block::StateDelay, delay.blocked);

    int maximum = 0;
    for (uint32_t now = 1200; now <= 2200; now += 40)
    {
        feedSteering(nag, -100, now);
        DashLegacyNagV4Decision d = nag.decide(now, true);
        if (d.send && d.torqueCentiNm > maximum)
            maximum = d.torqueCentiNm;
        TEST_ASSERT_LESS_OR_EQUAL_INT(210, d.torqueCentiNm);
    }
    TEST_ASSERT_EQUAL_INT(210, maximum);
    TEST_ASSERT_EQUAL(LegacyNagV4State::Escalated, nag.state);
}

void test_state1_holds_for_500_ms_then_stops()
{
    DashLegacyNagV4 nag = makeEnabled();
    nag.onContext(true, 2, 100);
    feedSteering(nag, -100, 2200);
    DashLegacyNagV4Decision active = nag.decide(2200, true);
    TEST_ASSERT_TRUE(active.send);

    nag.onContext(true, 1, 2300);
    DashLegacyNagV4Decision hold = nag.decide(2400, true);
    TEST_ASSERT_TRUE(hold.send);
    TEST_ASSERT_EQUAL_INT(active.torqueCentiNm, hold.torqueCentiNm);
    TEST_ASSERT_EQUAL(LegacyNagV4State::DetectedHold, hold.state);

    DashLegacyNagV4Decision stopped = nag.decide(2800, true);
    TEST_ASSERT_FALSE(stopped.send);
    TEST_ASSERT_EQUAL(LegacyNagV4State::Detected, stopped.state);
}

void test_states_zero_eight_and_fifteen_stop()
{
    for (uint8_t hands : {0, 8, 15})
    {
        DashLegacyNagV4 nag = makeEnabled();
        nag.onContext(true, hands, 100);
        feedSteering(nag, 100, 200);
        DashLegacyNagV4Decision d = nag.decide(200, true);
        TEST_ASSERT_FALSE(d.send);
        TEST_ASSERT_EQUAL(LegacyNagV4Block::Satisfied, d.blocked);
    }
}

void test_hard_gate_stops_immediately()
{
    DashLegacyNagV4 nag = makeEnabled();
    nag.onContext(true, 3, 100);
    feedSteering(nag, 100, 1200);
    TEST_ASSERT_FALSE(nag.decide(1200, false).send);
}

void test_absolute_encoder_uses_2050_and_preserves_wide_guard_clamps()
{
    uint8_t b2 = 0xA0;
    uint8_t b3 = 0;
    DashLegacyNagV4::applyTorque(b2, b3, 999);
    TEST_ASSERT_EQUAL_HEX8(0xA9, b2);
    TEST_ASSERT_EQUAL_HEX8(0xC4, b3);
    TEST_ASSERT_EQUAL_INT(450, DashLegacyNagV4::torqueCentiNm(b2, b3));
    DashLegacyNagV4::applyTorque(b2, b3, -999);
    TEST_ASSERT_EQUAL_INT(-270, DashLegacyNagV4::torqueCentiNm(b2, b3));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_disabled_never_sends);
    RUN_TEST(test_missing_stale_and_invalid_488_fail_closed);
    RUN_TEST(test_state2_waits_then_walks_one_sided_opposite_488);
    RUN_TEST(test_center_deadband_reuses_last_valid_direction);
    RUN_TEST(test_center_without_prior_direction_fails_closed);
    RUN_TEST(test_direction_change_is_slew_limited);
    RUN_TEST(test_strong_state_delays_then_ramps_to_2_1_nm);
    RUN_TEST(test_state1_holds_for_500_ms_then_stops);
    RUN_TEST(test_states_zero_eight_and_fifteen_stop);
    RUN_TEST(test_hard_gate_stops_immediately);
    RUN_TEST(test_absolute_encoder_uses_2050_and_preserves_wide_guard_clamps);
    return UNITY_END();
}
