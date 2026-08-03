#include <unity.h>

#include "dash_legacy_wheel_045_dnd.h"

static const uint8_t kIdleC[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xC0, 0xE0};
static const uint8_t kIdleD[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xD0, 0x2D};
static const uint8_t kIdleE[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xE0, 0x67};

static void arm(DashLegacyWheel045Dnd &dnd)
{
    dnd.configure(true);
    dnd.setRuntimeAllowed(true, DashLegacyWheel045Block::None);
}

static void seedAndWarn(DashLegacyWheel045Dnd &dnd, uint32_t nowMs = 1000,
                        uint8_t handsState = 3)
{
    dnd.onCan1Frame(kIdleC, 8, nowMs);
    dnd.onDasStatus(6, handsState, CAN_BUS_VEH, nowMs);
}

void test_crc_matches_vehicle_capture_vectors()
{
    static const uint8_t positive[8] = {0x40, 0x21, 0x00, 0x30, 0x40, 0x00, 0xC0, 0x80};
    static const uint8_t negative[8] = {0x40, 0x21, 0x00, 0x30, 0xC0, 0x03, 0xF0, 0xDE};
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(kIdleC));
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(kIdleD));
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(positive));
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(negative));
}

void test_positive_generation_preserves_unrelated_fields()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    TEST_ASSERT_EQUAL_HEX8(0x40, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x21, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0x30, out[3]);
    TEST_ASSERT_EQUAL_HEX8(0x40, out[4]);
    TEST_ASSERT_EQUAL_HEX8(0x00, out[5]);
    TEST_ASSERT_EQUAL_HEX8(0xD0, out[6]);
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::PositiveTxPending, dnd.diag.state);
}

void test_counter_wrap_preserves_low_nibble()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t source[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xFA, 0x00};
    source[7] = DashLegacyWheel045Dnd::crc8J1850(source, 7);
    uint8_t out[8] = {};
    arm(dnd);
    dnd.onCan1Frame(source, 8, 1000);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1000);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    TEST_ASSERT_EQUAL_HEX8(0x0A, out[6]);
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(out));
}

void test_single_positive_clear_cancels_pending_inverse()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t positive[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, positive));
    dnd.recordTxResult(true, 1000);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleAfterPositive, dnd.diag.state);
    TEST_ASSERT_FALSE(dnd.nextFrame(1050, positive));
    dnd.onCan1Frame(kIdleD, 8, 1100);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::NegativeTxReady, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT8(1, dnd.diag.compensationPending);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1150);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    TEST_ASSERT_FALSE(dnd.nextFrame(1150, positive));
    TEST_ASSERT_EQUAL_UINT8(0, dnd.diag.compensationPending);
    TEST_ASSERT_EQUAL_UINT32(150, dnd.diag.warningClearLatencyMs);
}

void test_warning_clear_before_native_release_cancels_inverse_action()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1025);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    dnd.onCan1Frame(kIdleD, 8, 1050);
    TEST_ASSERT_FALSE(dnd.nextFrame(1050, out));
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);
}

void test_failed_attempt_balances_before_retry_and_final_clear_stops()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    uint8_t native[8];
    arm(dnd);
    seedAndWarn(dnd);

    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    dnd.onCan1Frame(kIdleD, 8, 1100);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::NegativeTxReady, dnd.diag.state);
    TEST_ASSERT_TRUE(dnd.nextFrame(1100, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
    TEST_ASSERT_EQUAL_HEX8(0xE0, out[6]);
    dnd.recordTxResult(true, 1100);

    memcpy(native, kIdleE, 8);
    dnd.onCan1Frame(native, 8, 1200);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.balancedPairs);

    native[6] = 0xF0;
    native[7] = DashLegacyWheel045Dnd::crc8J1850(native, 7);
    dnd.onCan1Frame(native, 8, 2000);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 2000);
    TEST_ASSERT_TRUE(dnd.nextFrame(2000, out));
    TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 2000);
    TEST_ASSERT_EQUAL_UINT8(2, dnd.diag.positiveThisEpisode);

    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 2025);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    native[6] = 0x00;
    native[7] = DashLegacyWheel045Dnd::crc8J1850(native, 7);
    dnd.onCan1Frame(native, 8, 2100);
    TEST_ASSERT_FALSE(dnd.nextFrame(2100, out));
    TEST_ASSERT_EQUAL_UINT32(2, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.balancedPairs);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.volumeNeutralEpisodes);
    TEST_ASSERT_EQUAL_UINT8(0, dnd.diag.compensationPending);
}

void test_physical_input_aborts_pending_volume_compensation()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    dnd.onCan1Frame(kIdleD, 8, 1100);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::NegativeTxReady, dnd.diag.state);

    uint8_t physical[8];
    memcpy(physical, kIdleD, 8);
    DashLegacyWheel045Dnd::encodeTick(physical, 1);
    physical[7] = DashLegacyWheel045Dnd::crc8J1850(physical, 7);
    dnd.onCan1Frame(physical, 8, 1150);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::PhysicalInput, dnd.diag.blocked);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);
}

void test_stale_inverse_source_waits_without_sending_an_extra_positive()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    uint8_t native[8];
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    dnd.onCan1Frame(kIdleD, 8, 1100);
    TEST_ASSERT_FALSE(dnd.nextFrame(1251, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::NegativeTxReady, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT8(1, dnd.diag.compensationPending);

    memcpy(native, kIdleE, 8);
    native[7] = DashLegacyWheel045Dnd::crc8J1850(native, 7);
    dnd.onCan1Frame(native, 8, 1300);
    TEST_ASSERT_TRUE(dnd.nextFrame(1300, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
}

void test_escalated_warning_is_actionable()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 9);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
}

void test_warning_is_blocked_when_ap_inactive()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    dnd.onCan1Frame(kIdleC, 8, 1000);
    dnd.onDasStatus(2, 3, CAN_BUS_VEH, 1000);
    TEST_ASSERT_FALSE(dnd.nextFrame(1000, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::ApInactive, dnd.diag.blocked);
}

void test_stale_source_blocks_transmission()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    dnd.onCan1Frame(kIdleC, 8, 1000);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1000);
    TEST_ASSERT_FALSE(dnd.nextFrame(1151, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::StaleNative, dnd.diag.blocked);
}

void test_own_echo_does_not_replace_native_source()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    dnd.onCan1Frame(out, 8, 1005);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.ownEchoRx);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleAfterPositive, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT8(0x0C, dnd.diag.nativeCounter);
}

void test_expected_duplicate_and_counter_conflict_are_distinct()
{
    DashLegacyWheel045Dnd ok;
    uint8_t out[8] = {};
    arm(ok);
    seedAndWarn(ok);
    TEST_ASSERT_TRUE(ok.nextFrame(1000, out));
    ok.recordTxResult(true, 1000);
    ok.onCan1Frame(kIdleD, 8, 1100);
    TEST_ASSERT_EQUAL_UINT32(1, ok.diag.expectedCounterDuplicates);
    TEST_ASSERT_EQUAL_UINT32(0, ok.diag.counterConflicts);

    DashLegacyWheel045Dnd bad;
    arm(bad);
    seedAndWarn(bad);
    TEST_ASSERT_TRUE(bad.nextFrame(1000, out));
    bad.recordTxResult(true, 1000);
    uint8_t wrong[8];
    memcpy(wrong, kIdleC, 8);
    wrong[6] = 0x20;
    wrong[7] = DashLegacyWheel045Dnd::crc8J1850(wrong, 7);
    bad.onCan1Frame(wrong, 8, 1100);
    TEST_ASSERT_EQUAL_UINT32(1, bad.diag.counterConflicts);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::CounterConflict, bad.diag.blocked);
}

void test_physical_wheel_input_aborts_active_sequence()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    uint8_t physical[8];
    memcpy(physical, kIdleD, 8);
    DashLegacyWheel045Dnd::encodeTick(physical, 1);
    physical[7] = DashLegacyWheel045Dnd::crc8J1850(physical, 7);
    dnd.onCan1Frame(physical, 8, 1050);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::PhysicalInput, dnd.diag.blocked);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.physicalInputRx);
}

void test_two_tx_failures_auto_disarm()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(false, 1000);
    TEST_ASSERT_FALSE(dnd.diag.autoDisarmed);
    dnd.onCan1Frame(kIdleD, 8, 1100);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1150);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1200);
    TEST_ASSERT_TRUE(dnd.nextFrame(1200, out));
    dnd.recordTxResult(false, 1200);
    TEST_ASSERT_TRUE(dnd.diag.autoDisarmed);
    TEST_ASSERT_FALSE(dnd.diag.runtimeArmed);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::AutoDisarmed, dnd.diag.blocked);
}

void test_gateway_mirror_is_diagnostic_only()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    dnd.observeGatewayMirror(out, 8, 1002);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.gatewayMirrorRx);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleAfterPositive, dnd.diag.state);
}

void test_release_timeout_aborts_without_free_running_send()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    TEST_ASSERT_FALSE(dnd.nextFrame(1301, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::ReleaseTimeout, dnd.diag.blocked);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);
}

void test_warning_episode_is_limited_to_three_positive_attempts()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    uint32_t now = 1000;
    uint8_t native[8];
    memcpy(native, kIdleC, 8);
    uint8_t nativeCounter = 0x0C;
    for (uint8_t attempt = 0; attempt < 3; ++attempt)
    {
        TEST_ASSERT_TRUE(dnd.nextFrame(now, out));
        TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
        dnd.recordTxResult(true, now);
        nativeCounter = static_cast<uint8_t>((nativeCounter + 1) & 0x0F);
        native[6] = static_cast<uint8_t>(nativeCounter << 4);
        native[7] = DashLegacyWheel045Dnd::crc8J1850(native, 7);
        now += 100;
        dnd.onCan1Frame(native, 8, now);
        TEST_ASSERT_TRUE(dnd.nextFrame(now, out));
        TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
        dnd.recordTxResult(true, now);
        nativeCounter = static_cast<uint8_t>((nativeCounter + 1) & 0x0F);
        native[6] = static_cast<uint8_t>(nativeCounter << 4);
        native[7] = DashLegacyWheel045Dnd::crc8J1850(native, 7);
        now += 100;
        dnd.onCan1Frame(native, 8, now);
        if (attempt < 2)
        {
            now += DashLegacyWheel045Dnd::kRetryDelayMs;
            nativeCounter = static_cast<uint8_t>((nativeCounter + 1) & 0x0F);
            native[6] = static_cast<uint8_t>(nativeCounter << 4);
            native[7] = DashLegacyWheel045Dnd::crc8J1850(native, 7);
            dnd.onCan1Frame(native, 8, now);
            dnd.onDasStatus(6, 3, CAN_BUS_VEH, now);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.balancedPairs);
    TEST_ASSERT_EQUAL_UINT8(3, dnd.diag.pairsThisEpisode);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::EpisodeLimit, dnd.diag.blocked);
    now += DashLegacyWheel045Dnd::kRetryDelayMs;
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, now);
    TEST_ASSERT_FALSE(dnd.nextFrame(now, out));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_matches_vehicle_capture_vectors);
    RUN_TEST(test_positive_generation_preserves_unrelated_fields);
    RUN_TEST(test_counter_wrap_preserves_low_nibble);
    RUN_TEST(test_single_positive_clear_cancels_pending_inverse);
    RUN_TEST(test_warning_clear_before_native_release_cancels_inverse_action);
    RUN_TEST(test_failed_attempt_balances_before_retry_and_final_clear_stops);
    RUN_TEST(test_physical_input_aborts_pending_volume_compensation);
    RUN_TEST(test_stale_inverse_source_waits_without_sending_an_extra_positive);
    RUN_TEST(test_escalated_warning_is_actionable);
    RUN_TEST(test_warning_is_blocked_when_ap_inactive);
    RUN_TEST(test_stale_source_blocks_transmission);
    RUN_TEST(test_own_echo_does_not_replace_native_source);
    RUN_TEST(test_expected_duplicate_and_counter_conflict_are_distinct);
    RUN_TEST(test_physical_wheel_input_aborts_active_sequence);
    RUN_TEST(test_two_tx_failures_auto_disarm);
    RUN_TEST(test_gateway_mirror_is_diagnostic_only);
    RUN_TEST(test_release_timeout_aborts_without_free_running_send);
    RUN_TEST(test_warning_episode_is_limited_to_three_positive_attempts);
    return UNITY_END();
}
