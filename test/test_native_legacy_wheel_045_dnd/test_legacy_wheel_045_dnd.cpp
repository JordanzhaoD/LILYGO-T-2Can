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

// Feeds a fresh idle native frame carrying the given counter value.
static void feedNative(DashLegacyWheel045Dnd &dnd, uint8_t counterValue, uint32_t nowMs)
{
    uint8_t native[8];
    memcpy(native, kIdleC, 8);
    native[6] = static_cast<uint8_t>(counterValue << 4);
    native[7] = DashLegacyWheel045Dnd::crc8J1850(native, 7);
    dnd.onCan1Frame(native, 8, nowMs);
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
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::TxPending, dnd.diag.state);
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

void test_first_episode_sends_positive_second_alternates_negative()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);

    // Episode 1: fresh direction cursor (+1) -> positive tick.
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 1000);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.directionFlips);
    TEST_ASSERT_FALSE(dnd.diag.nextDirectionPositive);
    feedNative(dnd, 0x0D, 1100);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);

    // Warning clears -> episode 1 done with a single frame.
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1150);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT32(150, dnd.diag.warningClearLatencyMs);

    // Episode 2: cursor flipped -> negative tick.
    seedAndWarn(dnd, 2000, 3);
    TEST_ASSERT_TRUE(dnd.diag.directionPositive == false);
    TEST_ASSERT_TRUE(dnd.nextFrame(2000, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 2000);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(2, dnd.diag.directionFlips);
    TEST_ASSERT_TRUE(dnd.diag.nextDirectionPositive);
    feedNative(dnd, 0x0E, 2100);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 2150);

    // Episode 3: back to positive — net drift across the three = +1.
    seedAndWarn(dnd, 3000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(3000, out));
    TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
}

void test_warning_clear_before_confirm_sends_no_more_frames()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1025);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    feedNative(dnd, 0x0D, 1050);
    TEST_ASSERT_FALSE(dnd.nextFrame(1050, out));
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.framesThisEpisode);
}

void test_persistent_warning_retries_same_direction_until_budget()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    uint32_t now = 1000;
    uint8_t nativeCounter = 0x0C;

    for (uint8_t frame = 0; frame < DashLegacyWheel045Dnd::kMaxFramesPerEpisode; ++frame)
    {
        TEST_ASSERT_TRUE(dnd.nextFrame(now, out));
        // Every retry in one episode keeps the episode direction.
        TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
        dnd.recordTxResult(true, now);
        nativeCounter = static_cast<uint8_t>((nativeCounter + 1) & 0x0F);
        now += 100;
        feedNative(dnd, nativeCounter, now);
        if (frame < DashLegacyWheel045Dnd::kMaxFramesPerEpisode - 1)
        {
            TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);
            now += DashLegacyWheel045Dnd::kRetryDelayMs;
            nativeCounter = static_cast<uint8_t>((nativeCounter + 1) & 0x0F);
            feedNative(dnd, nativeCounter, now);
            dnd.onDasStatus(6, 3, CAN_BUS_VEH, now);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT8(3, dnd.diag.framesThisEpisode);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::EpisodeLimit, dnd.diag.blocked);

    // Airtight cap: even a fresh native while the warning persists must not
    // produce a fourth frame from this episode.
    now += 100;
    nativeCounter = static_cast<uint8_t>((nativeCounter + 1) & 0x0F);
    feedNative(dnd, nativeCounter, now);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, now);
    TEST_ASSERT_FALSE(dnd.nextFrame(now, out));
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.positiveTx);
}

void test_tx_failure_keeps_direction_for_next_episode()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);

    // Episode 1: +1 delivered, cursor flips to negative.
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    feedNative(dnd, 0x0D, 1100);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1150);

    // Episode 2: negative direction, but the send fails -> no flip.
    seedAndWarn(dnd, 2000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(2000, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(false, 2000);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.directionFlips);
    TEST_ASSERT_FALSE(dnd.diag.nextDirectionPositive);
    feedNative(dnd, 0x0D, 2100);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 2150);

    // Episode 3: cursor untouched by the failure -> negative again.
    seedAndWarn(dnd, 3000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(3000, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
}

void test_undelivered_episode_keeps_direction_for_next_episode()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);

    // Episode 1 delivers +1 (cursor flips to negative).
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    feedNative(dnd, 0x0D, 1100);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1150);

    // Episode 2 rises and clears without a single frame going out
    // (nothing polls nextFrame) -> cursor must not move.
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 2000);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.directionFlips);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 2100);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.directionFlips);

    // Episode 3 still owes the negative tick.
    seedAndWarn(dnd, 3000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(3000, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
}

void test_physical_input_aborts_pending_frame()
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
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
}

void test_confirmed_frame_cools_down_before_same_direction_retry()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    // Waiting for the native confirm: no extra frames meanwhile.
    TEST_ASSERT_FALSE(dnd.nextFrame(1151, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleConfirm, dnd.diag.state);

    // Confirm arrives -> retry cooldown (kRetryDelayMs) before the next
    // same-direction frame.
    feedNative(dnd, 0x0D, 1200);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);
    feedNative(dnd, 0x0E, 1900);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1900);
    TEST_ASSERT_FALSE(dnd.nextFrame(1999, out));
    TEST_ASSERT_TRUE(dnd.nextFrame(2000, out));
    TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
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
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleConfirm, dnd.diag.state);
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
    feedNative(ok, 0x0D, 1100);
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

void test_two_tx_failures_auto_disarm()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(false, 1000);
    TEST_ASSERT_FALSE(dnd.diag.autoDisarmed);
    feedNative(dnd, 0x0D, 1100);
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
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleConfirm, dnd.diag.state);
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

void test_release_timeout_ceiling_protects_fsd_activation_window()
{
    // beta05 P0: raising the release timeout to 1200ms overlapped the FSD
    // activation landing window and blocked activation with the AP gate on.
    // 300ms is the hard ceiling carried over from beta02.
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(300, DashLegacyWheel045Dnd::kReleaseTimeoutMs);
    // Drift bound: the per-episode frame budget stays at beta02's scale.
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(3, DashLegacyWheel045Dnd::kMaxFramesPerEpisode);
}

void test_reset_runtime_restarts_alternation_from_positive()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    TEST_ASSERT_FALSE(dnd.diag.nextDirectionPositive);
    dnd.resetRuntime();
    TEST_ASSERT_TRUE(dnd.diag.nextDirectionPositive);
    TEST_ASSERT_EQUAL_UINT8(0, dnd.diag.framesThisEpisode);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_matches_vehicle_capture_vectors);
    RUN_TEST(test_positive_generation_preserves_unrelated_fields);
    RUN_TEST(test_counter_wrap_preserves_low_nibble);
    RUN_TEST(test_first_episode_sends_positive_second_alternates_negative);
    RUN_TEST(test_warning_clear_before_confirm_sends_no_more_frames);
    RUN_TEST(test_persistent_warning_retries_same_direction_until_budget);
    RUN_TEST(test_tx_failure_keeps_direction_for_next_episode);
    RUN_TEST(test_undelivered_episode_keeps_direction_for_next_episode);
    RUN_TEST(test_physical_input_aborts_pending_frame);
    RUN_TEST(test_confirmed_frame_cools_down_before_same_direction_retry);
    RUN_TEST(test_escalated_warning_is_actionable);
    RUN_TEST(test_warning_is_blocked_when_ap_inactive);
    RUN_TEST(test_stale_source_blocks_transmission);
    RUN_TEST(test_own_echo_does_not_replace_native_source);
    RUN_TEST(test_expected_duplicate_and_counter_conflict_are_distinct);
    RUN_TEST(test_two_tx_failures_auto_disarm);
    RUN_TEST(test_gateway_mirror_is_diagnostic_only);
    RUN_TEST(test_release_timeout_aborts_without_free_running_send);
    RUN_TEST(test_release_timeout_ceiling_protects_fsd_activation_window);
    RUN_TEST(test_reset_runtime_restarts_alternation_from_positive);
    return UNITY_END();
}
