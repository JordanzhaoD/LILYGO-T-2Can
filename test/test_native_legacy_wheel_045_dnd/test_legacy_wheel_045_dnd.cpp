#include <unity.h>

#include "dash_legacy_wheel_045_dnd.h"

// Spec for the atomic-pair burst redesign (6.0.0-beta03, retimed beta05):
//   f1 (+1 tick) and f2 (-1 tick) are one burst — consecutive counters,
//   >=600ms apart (2026-10-28 field verdict: beta03's 4-10ms and beta04's
//   25ms both netted -1 volume per pair; the only reverse transition the
//   car verifiably accepts is a real wheel direction change, >=600ms in the
//   2026-10-27 capture), same carrier-locked window. Volume net change is
//   zero by construction; a warning clear can never strand f1 (the old
//   "conditional balance" stranded a +1 on every short warning).
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

static void makeIdleFrame(uint8_t out[8], uint8_t counter)
{
    memcpy(out, kIdleC, 8);
    out[6] = static_cast<uint8_t>(counter << 4);
    out[7] = DashLegacyWheel045Dnd::crc8J1850(out, 7);
}

// Drives one full burst: f1 at t, f2 at t+600ms, native resume at t+650ms.
// Returns false if any step refuses to produce its frame.
static bool runBurst(DashLegacyWheel045Dnd &dnd, uint8_t out[8], uint32_t t,
                     uint8_t &counter)
{
    if (!dnd.nextFrame(t, out))
        return false;
    dnd.recordTxResult(true, t);
    if (!dnd.nextFrame(t + 600, out))
        return false;
    dnd.recordTxResult(true, t + 600);
    counter = static_cast<uint8_t>((counter + 1) & 0x0F);
    uint8_t resume[8];
    makeIdleFrame(resume, counter);
    dnd.onCan1Frame(resume, 8, t + 650);
    return true;
}

void test_crc_matches_vehicle_capture_vectors()
{
    static const uint8_t positive[8] = {0x40, 0x21, 0x00, 0x30, 0x40, 0x00, 0xC0, 0x80};
    static const uint8_t negative[8] = {0x40, 0x21, 0x00, 0x30, 0xC0, 0x03, 0xF0, 0xDE};
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(kIdleC));
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(kIdleD));
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(kIdleE));
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

void test_atomic_pair_burst_emits_consecutive_ticks()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    uint8_t f1[8] = {};
    uint8_t f2[8] = {};
    arm(dnd);
    seedAndWarn(dnd);

    TEST_ASSERT_TRUE(dnd.nextFrame(1000, f1));
    TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(f1));
    TEST_ASSERT_EQUAL_HEX8(0xD0, f1[6]);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::PositiveTxPending, dnd.diag.state);
    dnd.recordTxResult(true, 1000);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::BurstSecondDue, dnd.diag.state);

    // The second tick is owed regardless of anything else; only the >=600ms
    // intra-burst gap (real-wheel reverse-transition floor) can hold it back.
    TEST_ASSERT_FALSE(dnd.nextFrame(1599, out));
    TEST_ASSERT_TRUE(dnd.nextFrame(1600, f2));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(f2));
    TEST_ASSERT_EQUAL_HEX8(0xE0, f2[6]);
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(f2));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::NegativeTxPending, dnd.diag.state);
    dnd.recordTxResult(true, 1600);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleAfterNegative, dnd.diag.state);

    // Both echoes must be filtered by the two-slot fingerprint so f1's echo
    // is not mistaken for driver wheel input. Bus echoes trail their frame
    // by ~2ms, so f1's echo lands inside its own 30ms fingerprint window
    // even though the burst gap is 600ms.
    dnd.onCan1Frame(f1, 8, 1002);
    dnd.onCan1Frame(f2, 8, 1602);
    TEST_ASSERT_EQUAL_UINT32(2, dnd.diag.ownEchoRx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.physicalInputRx);

    uint8_t resume[8];
    makeIdleFrame(resume, 0x0F);
    dnd.onCan1Frame(resume, 8, 1650);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.balancedPairs);
}

// 2026-10-28 field verdict: beta03 (4-10ms) and beta04 (25ms, the real-wheel
// same-direction pulse floor) both netted -1 volume per pair — the car's
// reverse-merge window is wider than any pulse floor. The only reverse
// transition it verifiably accepts is a real wheel direction change, >=600ms
// (2026-10-27 capture). Pin the retimed gap so it cannot silently regress.
void test_burst_min_gap_matches_real_wheel_reverse_floor()
{
    TEST_ASSERT_EQUAL_UINT32(600, DashLegacyWheel045Dnd::kBurstMinGapMs);
}

// The inverse tick is aborted (stranding the +1 on the bus, volume drifts
// UP one step per pair) whenever the release timeout fires before the gap
// elapses. The timeout must stay strictly above the burst gap.
void test_release_timeout_covers_burst_gap()
{
    TEST_ASSERT_EQUAL_UINT32(1200, DashLegacyWheel045Dnd::kReleaseTimeoutMs);
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::kReleaseTimeoutMs >
                     DashLegacyWheel045Dnd::kBurstMinGapMs);
}

void test_warning_clear_mid_burst_still_completes_inverse_tick()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);

    // Warning clears between f1 and f2: the inverse tick is still owed —
    // this is the fix for the runaway "+1 per short warning" drift.
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1400);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::BurstSecondDue, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.midBurstWarningClears);
    TEST_ASSERT_EQUAL_UINT32(400, dnd.diag.warningClearLatencyMs);

    TEST_ASSERT_TRUE(dnd.nextFrame(1600, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 1600);

    uint8_t resume[8];
    makeIdleFrame(resume, 0x0F);
    dnd.onCan1Frame(resume, 8, 1650);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.balancedPairs);
}

void test_episode_limit_holds_with_fresh_native_and_active_warning()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);

    uint32_t now = 1000;
    uint8_t counter = 0x0C;
    for (int pair = 0; pair < 3; ++pair)
    {
        now += 1500; // pass RetryWait pacing (burst is ~650ms + 800ms retry)
        dnd.onDasStatus(6, 3, CAN_BUS_VEH, now);
        makeIdleFrame(out, static_cast<uint8_t>((counter + 1) & 0x0F));
        dnd.onCan1Frame(out, 8, now); // idle beat keeps native fresh
        TEST_ASSERT_TRUE(runBurst(dnd, out, now, counter));
    }
    TEST_ASSERT_EQUAL_UINT8(3, dnd.diag.pairsThisEpisode);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::EpisodeLimit, dnd.diag.blocked);

    // Real-bus conditions: warning still active, native still fresh. The
    // 4th burst must stay blocked (old code refired positives from Idle).
    now += 50;
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, now);
    makeIdleFrame(out, static_cast<uint8_t>((counter + 1) & 0x0F));
    dnd.onCan1Frame(out, 8, now);
    TEST_ASSERT_FALSE(dnd.nextFrame(now, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::EpisodeLimit, dnd.diag.blocked);
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.balancedPairs);
}

void test_burst_start_requires_carrier_lock_freshness()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    // 50ms-old native template: the old 150ms gate allowed this and its
    // counter was usually already consumed by the controller (dedup race).
    TEST_ASSERT_FALSE(dnd.nextFrame(1050, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::StaleNative, dnd.diag.blocked);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.carrierLockBlocks);
}

void test_failed_first_tick_never_strands_and_next_episode_retries()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(false, 1000); // f1 never reached the bus: nothing owed
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::TxFailed, dnd.diag.blocked);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);

    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1100);
    dnd.onCan1Frame(kIdleD, 8, 1200);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1200);
    TEST_ASSERT_TRUE(dnd.nextFrame(1200, out));
    TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 1200);
    TEST_ASSERT_TRUE(dnd.nextFrame(1800, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 1800);
    uint8_t resume[8];
    makeIdleFrame(resume, 0x00);
    dnd.onCan1Frame(resume, 8, 1875);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.balancedPairs);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.negativeTx);
}

void test_failed_second_tick_retries_inverse_without_disarm()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    TEST_ASSERT_TRUE(dnd.nextFrame(1600, out));
    dnd.recordTxResult(false, 1600); // f2 failed: still owed, retry after gap
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::BurstSecondDue, dnd.diag.state);
    TEST_ASSERT_FALSE(dnd.diag.autoDisarmed);

    TEST_ASSERT_TRUE(dnd.nextFrame(1650, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 1650);
    uint8_t resume[8];
    makeIdleFrame(resume, 0x0F);
    dnd.onCan1Frame(resume, 8, 1700);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.balancedPairs);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.negativeTx);
}

void test_physical_input_mid_burst_completes_inverse_then_stops()
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
    dnd.onCan1Frame(physical, 8, 1400);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.physicalInputRx);
    // The owed inverse tick survives the touch: it cancels OUR +1 whatever
    // the driver does. The episode just ends after this pair.
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::BurstSecondDue, dnd.diag.state);

    TEST_ASSERT_TRUE(dnd.nextFrame(1600, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 1600);
    uint8_t resume[8];
    makeIdleFrame(resume, 0x0F);
    dnd.onCan1Frame(resume, 8, 1650);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::PhysicalInput, dnd.diag.blocked);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.balancedPairs);
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
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::BurstSecondDue, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT8(0x0C, dnd.diag.nativeCounter);
}

void test_counter_duplicate_and_conflict_are_diagnostic_only()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    TEST_ASSERT_TRUE(dnd.nextFrame(1600, out));
    dnd.recordTxResult(true, 1600);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleAfterNegative, dnd.diag.state);

    // Resume with the counter we already used: duplicate, still a resume.
    uint8_t resume[8];
    makeIdleFrame(resume, 0x0E);
    dnd.onCan1Frame(resume, 8, 1700);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.expectedCounterDuplicates);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.counterConflicts);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.balancedPairs);

    // Diverged counter: count it, but the pair is volume-neutral either
    // way — never abort into a potentially stranded state.
    DashLegacyWheel045Dnd bad;
    arm(bad);
    seedAndWarn(bad);
    TEST_ASSERT_TRUE(bad.nextFrame(1000, out));
    bad.recordTxResult(true, 1000);
    TEST_ASSERT_TRUE(bad.nextFrame(1600, out));
    bad.recordTxResult(true, 1600);
    uint8_t wrong[8];
    makeIdleFrame(wrong, 0x02);
    bad.onCan1Frame(wrong, 8, 1700);
    TEST_ASSERT_EQUAL_UINT32(1, bad.diag.counterConflicts);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, bad.diag.state);
    TEST_ASSERT_EQUAL_UINT32(1, bad.diag.balancedPairs);
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
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1150);
    dnd.onCan1Frame(kIdleD, 8, 1200);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1200);
    TEST_ASSERT_TRUE(dnd.nextFrame(1200, out));
    dnd.recordTxResult(false, 1200);
    TEST_ASSERT_TRUE(dnd.diag.autoDisarmed);
    TEST_ASSERT_FALSE(dnd.diag.runtimeArmed);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::AutoDisarmed, dnd.diag.blocked);
}

void test_release_timeout_mid_burst_aborts_and_counts_split()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    // 0x399 keeps flowing in the real car; without this beat the context
    // gate (1s) would mask the release-timeout verdict we are asserting.
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 2200);
    TEST_ASSERT_FALSE(dnd.nextFrame(2201, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::ReleaseTimeout, dnd.diag.blocked);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.burstSplitAborts);
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
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::BurstSecondDue, dnd.diag.state);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_matches_vehicle_capture_vectors);
    RUN_TEST(test_positive_generation_preserves_unrelated_fields);
    RUN_TEST(test_counter_wrap_preserves_low_nibble);
    RUN_TEST(test_atomic_pair_burst_emits_consecutive_ticks);
    RUN_TEST(test_burst_min_gap_matches_real_wheel_reverse_floor);
    RUN_TEST(test_release_timeout_covers_burst_gap);
    RUN_TEST(test_warning_clear_mid_burst_still_completes_inverse_tick);
    RUN_TEST(test_episode_limit_holds_with_fresh_native_and_active_warning);
    RUN_TEST(test_burst_start_requires_carrier_lock_freshness);
    RUN_TEST(test_failed_first_tick_never_strands_and_next_episode_retries);
    RUN_TEST(test_failed_second_tick_retries_inverse_without_disarm);
    RUN_TEST(test_physical_input_mid_burst_completes_inverse_then_stops);
    RUN_TEST(test_escalated_warning_is_actionable);
    RUN_TEST(test_warning_is_blocked_when_ap_inactive);
    RUN_TEST(test_stale_source_blocks_transmission);
    RUN_TEST(test_own_echo_does_not_replace_native_source);
    RUN_TEST(test_counter_duplicate_and_conflict_are_diagnostic_only);
    RUN_TEST(test_two_tx_failures_auto_disarm);
    RUN_TEST(test_release_timeout_mid_burst_aborts_and_counts_split);
    RUN_TEST(test_gateway_mirror_is_diagnostic_only);
    return UNITY_END();
}
