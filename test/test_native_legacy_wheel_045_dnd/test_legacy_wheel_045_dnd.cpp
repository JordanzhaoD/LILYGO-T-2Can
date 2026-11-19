#include <unity.h>

#include "dash_legacy_wheel_045_dnd.h"

// Wheels DND pinned to the beta02 atomic-pair behaviour: each warning episode
// emits a positive tick (+1), waits for the native confirm, then a negative
// compensation tick (-1), waits for the confirm, and only then may retry after
// the 800 ms merge-window cooldown. Net volume drift per fully-paired episode
// is zero. State 7 (REQD_STRUCK_OUT) is counted but never injected.

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
    // The warning edge is raised kFirstTickDelayMs before nowMs, so that a
    // nextFrame(nowMs) call lands exactly at the end of the deferred first-tick
    // window (the vehicle only accepts a scroll tick ~930 ms after the edge).
    const uint32_t edgeMs = nowMs - DashLegacyWheel045Dnd::kFirstTickDelayMs;
    dnd.onDasStatus(6, handsState, CAN_BUS_VEH, edgeMs);
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

// Drives one full atomic pair: positive -> confirm -> negative -> confirm.
// Returns true when the pair completed: either RetryWait (budget remains) or
// Idle/EpisodeLimit (the third and final pair of an episode).
static bool runOnePair(DashLegacyWheel045Dnd &dnd, uint32_t tStart)
{
    uint8_t out[8] = {};
    dnd.onCan1Frame(kIdleC, 8, tStart);         // refresh native freshness (150 ms window)
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, tStart); // refresh 0x399 context (1 s window)
    if (!dnd.nextFrame(tStart, out))
        return false;
    dnd.recordTxResult(true, tStart);
    feedNative(dnd, DashLegacyWheel045Dnd::counter(out), tStart + 100);
    if (!dnd.nextFrame(tStart + 100, out))
        return false;
    dnd.recordTxResult(true, tStart + 100);
    feedNative(dnd, DashLegacyWheel045Dnd::counter(out), tStart + 200);
    return dnd.diag.state == DashLegacyWheel045State::RetryWait ||
           (dnd.diag.state == DashLegacyWheel045State::Idle &&
            dnd.diag.blocked == DashLegacyWheel045Block::EpisodeLimit);
}

// Feeds a physical (human) scroll frame: valid CRC, non-zero switch word.
static void feedPhysical(DashLegacyWheel045Dnd &dnd, uint32_t nowMs)
{
    uint8_t physical[8];
    memcpy(physical, kIdleC, 8);
    physical[4] = 0x40; // left-wheel tick bits
    physical[7] = DashLegacyWheel045Dnd::crc8J1850(physical, 7);
    dnd.onCan1Frame(physical, 8, nowMs);
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

void test_negative_generation_preserves_unrelated_fields()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    feedNative(dnd, 0x0D, 1100);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::NegativeTxReady, dnd.diag.state);
    TEST_ASSERT_TRUE(dnd.nextFrame(1100, out));
    // Only the left-tick field changes: negative nibble 0x0F at bit 6.
    TEST_ASSERT_EQUAL_HEX8(0x40, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x21, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0x30, out[3]);
    TEST_ASSERT_EQUAL_HEX8(0xC0, out[4]);
    TEST_ASSERT_EQUAL_HEX8(0x03, out[5]);
    TEST_ASSERT_EQUAL_HEX8(0xE0, out[6]);
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::NegativeTxPending, dnd.diag.state);
}

void test_atomic_pair_positive_confirm_negative_confirm()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);

    // Positive leg.
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 1000);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleAfterPositive, dnd.diag.state);

    // Native confirm arms the compensation leg.
    feedNative(dnd, 0x0D, 1100);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::NegativeTxReady, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT8(1, dnd.diag.compensationPending);

    // Negative leg.
    TEST_ASSERT_TRUE(dnd.nextFrame(1100, out));
    TEST_ASSERT_EQUAL_HEX16(0x03C0, DashLegacyWheel045Dnd::switchWord(out));
    dnd.recordTxResult(true, 1100);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleAfterNegative, dnd.diag.state);

    // Confirm completes the balanced pair and starts the merge-window cooldown.
    feedNative(dnd, 0x0E, 1200);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT8(1, dnd.diag.pairsThisEpisode);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.balancedPairs);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.negativeTx);
}

void test_volume_stays_balanced_across_many_episodes()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    uint32_t t = 1000;
    for (int episode = 0; episode < 5; ++episode)
    {
        seedAndWarn(dnd, t, 3);
        TEST_ASSERT_TRUE(runOnePair(dnd, t));
        // Warning clears after the balanced pair.
        dnd.onDasStatus(6, 2, CAN_BUS_VEH, t + 300);
        TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
        TEST_ASSERT_EQUAL_UINT32(episode + 1, dnd.diag.balancedPairs);
        t += 2000;
    }
    TEST_ASSERT_EQUAL(dnd.diag.positiveTx, dnd.diag.negativeTx);
    TEST_ASSERT_EQUAL_UINT32(5, dnd.diag.positiveTx);
    TEST_ASSERT_EQUAL_UINT32(5, dnd.diag.negativeTx);
}

void test_retry_cooldown_blocks_second_pair_until_800ms()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(runOnePair(dnd, 1000)); // confirm at 1200 -> retry at 2000

    uint8_t expectCounter = 0x0E; // last confirm counter
    feedNative(dnd, expectCounter, 1300); // keep native fresh
    TEST_ASSERT_FALSE(dnd.nextFrame(1500, out)); // still cooling down
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);
    feedNative(dnd, expectCounter, 1990); // refresh native freshness
    TEST_ASSERT_TRUE(dnd.nextFrame(2000, out)); // cooldown elapsed
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::PositiveTxPending, dnd.diag.state);
    // retryCount includes the deferred first tick (1) plus this cooldown retry (2).
    TEST_ASSERT_EQUAL_UINT32(2, dnd.diag.retryCount);
}

void test_warning_clear_before_confirm_skips_compensation()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);

    // The positive tick itself cleared the warning before the confirm landed.
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1100);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    TEST_ASSERT_EQUAL_UINT32(100, dnd.diag.warningClearLatencyMs);

    feedNative(dnd, 0x0D, 1200); // confirm arrives after the episode closed
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    TEST_ASSERT_FALSE(dnd.nextFrame(1200, out));
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.negativeTx);
}

void test_third_pair_hits_episode_limit_then_idle_rearms()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    uint32_t t = 1000;
    for (int pair = 0; pair < 3; ++pair)
    {
        TEST_ASSERT_TRUE(runOnePair(dnd, t));
        TEST_ASSERT_EQUAL_UINT8(pair + 1, dnd.diag.pairsThisEpisode);
        t += 1000; // each pair spans confirm at +200; cooldown elapses
    }
    // The automatic RetryWait chain stops at three pairs...
    TEST_ASSERT_EQUAL_UINT8(3, dnd.diag.pairsThisEpisode);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::EpisodeLimit, dnd.diag.blocked);

    // ...but beta02's field behaviour is a SOFT budget: while the warning is
    // still active and the native stream fresh, the Idle path re-arms and
    // keeps clearing (this is why beta02 never gave up mid-drive).
    dnd.onCan1Frame(kIdleC, 8, t);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, t);
    TEST_ASSERT_TRUE(dnd.nextFrame(t, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::PositiveTxPending, dnd.diag.state);
}

void test_physical_input_abort_latches_until_next_warning_edge()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);

    feedPhysical(dnd, 1100); // human scrolls: never race the human
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Aborted, dnd.diag.state);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::PhysicalInput, dnd.diag.blocked);
    TEST_ASSERT_FALSE(dnd.nextFrame(1100, out)); // no recovery this episode

    // Next warning edge re-arms.
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1200);
    seedAndWarn(dnd, 2000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(2000, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::PositiveTxPending, dnd.diag.state);
}

void test_tx_failure_twice_across_episodes_auto_disarms()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);

    // Episode 1: first failure.
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(false, 1000);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.txFailed);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Aborted, dnd.diag.state);

    // Episode 2: second consecutive failure trips the auto-disarm.
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1200);
    seedAndWarn(dnd, 2000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(2000, out));
    dnd.recordTxResult(false, 2000);
    TEST_ASSERT_TRUE(dnd.diag.autoDisarmed);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::AutoDisarmed, dnd.diag.state);

    // No further injection while disarmed.
    seedAndWarn(dnd, 3000, 3);
    TEST_ASSERT_FALSE(dnd.nextFrame(3000, out));

    // Toggling the switch re-arms — the Aborted state waits for the next
    // warning falling+rising edge to open a fresh episode (beta02 semantics).
    dnd.configure(false);
    dnd.configure(true);
    TEST_ASSERT_FALSE(dnd.diag.autoDisarmed);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 3500); // warning settles
    seedAndWarn(dnd, 4000, 3);                // new warning edge re-arms
    TEST_ASSERT_TRUE(dnd.nextFrame(4000, out));
}

void test_struck_out_state_7_counts_and_never_injects()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);

    // Warning escalates to state 7 (red wheel): count, fail closed, no inject.
    dnd.onDasStatus(6, 7, CAN_BUS_VEH, 1100);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.struckOutEpisodes);
    TEST_ASSERT_FALSE(dnd.nextFrame(1100, out));

    // Sustained 7 is not a new edge: no double counting.
    dnd.onDasStatus(6, 7, CAN_BUS_VEH, 1150);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.struckOutEpisodes);

    // Recovery: warning settles, then a normal episode injects again.
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, 1200);
    seedAndWarn(dnd, 2000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(2000, out));
}

void test_own_echo_window_drops_matching_native()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);

    // The bus echoes our own frame back inside the 30 ms window.
    dnd.onCan1Frame(out, 8, 1010);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.ownEchoRx);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleAfterPositive, dnd.diag.state);
}

void test_counter_conflict_on_confirm_aborts()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);

    // Confirm jumps the counter by more than +1: conflict, abort.
    feedNative(dnd, 0x05, 1100);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.counterConflicts);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Aborted, dnd.diag.state);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::CounterConflict, dnd.diag.blocked);
}

void test_release_timeout_aborts_waiting_confirm()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);

    // No native confirm for 300 ms: release timeout.
    uint8_t unused[8];
    (void)unused;
    TEST_ASSERT_FALSE(dnd.nextFrame(1301, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Aborted, dnd.diag.state);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::ReleaseTimeout, dnd.diag.blocked);
}

void test_counter_wrap_preserves_low_nibble()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t source[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xFA, 0x00};
    source[7] = DashLegacyWheel045Dnd::crc8J1850(source, 7);
    uint8_t out[8] = {};
    arm(dnd);
    dnd.onCan1Frame(source, 8, 1000);
    // Warning edge is raised 930 ms earlier so nextFrame(1000) lands at the
    // end of the deferred first-tick window.
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1000 - DashLegacyWheel045Dnd::kFirstTickDelayMs);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1000);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    TEST_ASSERT_EQUAL_HEX8(0x0A, out[6]);
    TEST_ASSERT_TRUE(DashLegacyWheel045Dnd::validFrame(out));
}

void test_ap_inactive_resets_episode()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    feedNative(dnd, 0x0D, 1100);

    dnd.onDasStatus(1, 3, CAN_BUS_VEH, 1200); // AP off
    TEST_ASSERT_FALSE(dnd.diag.apActive);
    TEST_ASSERT_EQUAL_UINT8(0, dnd.diag.pairsThisEpisode);
    TEST_ASSERT_EQUAL_UINT8(0, dnd.diag.compensationPending);
    TEST_ASSERT_FALSE(dnd.nextFrame(1200, out));
}

void test_context_stale_blocks_injection()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    feedNative(dnd, 0x0C, 1900); // native stays fresh
    TEST_ASSERT_FALSE(dnd.nextFrame(2100, out)); // 0x399 silent > 1 s
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingSource, dnd.diag.state);
    TEST_ASSERT_EQUAL(DashLegacyWheel045Block::ContextStale, dnd.diag.blocked);
}

void test_reset_runtime_clears_pair_state()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);
    seedAndWarn(dnd, 1000, 3);
    TEST_ASSERT_TRUE(runOnePair(dnd, 1000));
    TEST_ASSERT_EQUAL_UINT8(1, dnd.diag.pairsThisEpisode);

    dnd.resetRuntime();
    TEST_ASSERT_EQUAL_UINT8(0, dnd.diag.pairsThisEpisode);
    TEST_ASSERT_EQUAL_UINT8(0, dnd.diag.compensationPending);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::Idle, dnd.diag.state);

    // Fresh episode works from scratch.
    seedAndWarn(dnd, 3000, 3);
    TEST_ASSERT_TRUE(dnd.nextFrame(3000, out));
}

// Regression: resetRuntime() must clear the strike-out edge latch. Before the
// fix, lastHandsState_ survived the reset, so a red wheel that was already
// latched before the toggle would never re-arm the strike-out edge detection
// (7 == 7 -> no new edge -> no count, and the abort path stayed stale).
void test_reset_runtime_clears_strike_out_edge_latch()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);

    // First red wheel: counted as an edge.
    dnd.onDasStatus(6, 7, CAN_BUS_VEH, 1000);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.struckOutEpisodes);

    // Toggle the switch off/on (resetRuntime) while the vehicle still reports
    // the same strike-out state 7. The cumulative counter persists, but the
    // edge latch must clear.
    dnd.resetRuntime();

    // The same state 7 is now a fresh edge again and must re-count, proving
    // the latch was cleared. (Without the fix, lastHandsState_ stayed 7 and
    // this rising edge was swallowed, leaving the count stuck at 1.)
    dnd.onDasStatus(6, 7, CAN_BUS_VEH, 1100);
    TEST_ASSERT_EQUAL_UINT32(2, dnd.diag.struckOutEpisodes);

    // And it never injects against the strike-out.
    TEST_ASSERT_FALSE(dnd.nextFrame(1100, out));
}

// The vehicle only accepts a scroll tick ~930 ms after the warning edge
// (2026-11-19 captures: 0-135 ms ticks dropped 13/13, 927-976 ms cleared 7/7).
// The first positive tick must therefore be deferred, not fired at the edge.
void test_first_tick_deferred_to_nag_clear_window()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8] = {};
    arm(dnd);

    // Warning edge at t=1000.
    dnd.onCan1Frame(kIdleC, 8, 1000);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1000);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);

    // Inside the defer window: no tick yet.
    dnd.onCan1Frame(kIdleC, 8, 1500);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1500);
    TEST_ASSERT_FALSE(dnd.nextFrame(1500, out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::RetryWait, dnd.diag.state);

    // At the end of the defer window: first positive tick fires.
    dnd.onCan1Frame(kIdleC, 8, 1930);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1930);
    TEST_ASSERT_TRUE(dnd.nextFrame(1930, out));
    TEST_ASSERT_EQUAL_HEX16(0x0040, DashLegacyWheel045Dnd::switchWord(out));
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::PositiveTxPending, dnd.diag.state);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_matches_vehicle_capture_vectors);
    RUN_TEST(test_positive_generation_preserves_unrelated_fields);
    RUN_TEST(test_negative_generation_preserves_unrelated_fields);
    RUN_TEST(test_atomic_pair_positive_confirm_negative_confirm);
    RUN_TEST(test_volume_stays_balanced_across_many_episodes);
    RUN_TEST(test_retry_cooldown_blocks_second_pair_until_800ms);
    RUN_TEST(test_warning_clear_before_confirm_skips_compensation);
    RUN_TEST(test_third_pair_hits_episode_limit_then_idle_rearms);
    RUN_TEST(test_physical_input_abort_latches_until_next_warning_edge);
    RUN_TEST(test_tx_failure_twice_across_episodes_auto_disarms);
    RUN_TEST(test_struck_out_state_7_counts_and_never_injects);
    RUN_TEST(test_own_echo_window_drops_matching_native);
    RUN_TEST(test_counter_conflict_on_confirm_aborts);
    RUN_TEST(test_release_timeout_aborts_waiting_confirm);
    RUN_TEST(test_counter_wrap_preserves_low_nibble);
    RUN_TEST(test_ap_inactive_resets_episode);
    RUN_TEST(test_context_stale_blocks_injection);
    RUN_TEST(test_reset_runtime_clears_pair_state);
    RUN_TEST(test_reset_runtime_clears_strike_out_edge_latch);
    RUN_TEST(test_first_tick_deferred_to_nag_clear_window);
    return UNITY_END();
}
