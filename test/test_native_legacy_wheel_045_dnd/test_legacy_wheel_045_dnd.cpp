#include <unity.h>

#include "dash_legacy_wheel_045_dnd.h"
#include "dash_legacy_wheel_045_press.h"

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

// ── TX split v2: in-event disarm self-heal ───────────────────────────────

// ── TX split v3: right-wheel PRESS sibling coordination ─────────────────
// These use the PRODUCTION bindings (DashLegacyWheel045Press::isPressFrame
// and ::siblingOwnsBeat) with both engines live, so what is pinned here is
// the coordination the car actually runs (review M2: the old tests used a
// hand-written 0x20 / bit27 filter and never touched the real one).

static DashLegacyWheel045Press *g_pressForYield = nullptr;

static bool pressOwnsBeat(uint32_t nowMs)
{
    return g_pressForYield != nullptr && g_pressForYield->siblingOwnsBeat(nowMs);
}

static void bindSiblings(DashLegacyWheel045Dnd &dnd, DashLegacyWheel045Press &press)
{
    g_pressForYield = &press;
    dnd.setSiblingCloneFilter(DashLegacyWheel045Press::isPressFrame);
    dnd.setYieldGate(pressOwnsBeat);
}

void test_dnd_ignores_right_press_sibling_frames()
{
    DashLegacyWheel045Dnd dnd;
    DashLegacyWheel045Press press;
    uint8_t out[8];
    arm(dnd);
    press.configure(true);
    press.setRuntimeAllowed(true);
    bindSiblings(dnd, press);
    seedAndWarn(dnd, 1000);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    // The right-wheel engine emits a CENTER press frame (data[4] |= 0x10).
    press.onDasStatus(6, 3, 1100);
    uint8_t pressFrame[8];
    TEST_ASSERT_TRUE(press.onNative045(kIdleC, 8, 1150, pressFrame));
    TEST_ASSERT_TRUE(DashLegacyWheel045Press::isPressFrame(pressFrame));
    // That frame arrives at the left-wheel engine: recognised as a sibling
    // press — it must NOT refresh nativeFresh nor feed the pair logic.
    uint32_t nativeMsBefore = dnd.diag.nativeMs;
    dnd.onCan1Frame(pressFrame, 8, 1150);
    TEST_ASSERT_EQUAL_UINT32(nativeMsBefore, dnd.diag.nativeMs);
    TEST_ASSERT_TRUE(dnd.diag.siblingCloneRx >= 1);
    TEST_ASSERT_EQUAL(DashLegacyWheel045State::WaitingIdleAfterPositive, dnd.diag.state);
}

// Review I5: while a click is in progress (press sent, release not yet out)
// the left-wheel DND must yield; once the release has gone out AND a native
// frame after it has been seen, it must transmit again — never starve.
void test_dnd_yields_during_click_then_resumes()
{
    DashLegacyWheel045Dnd dnd;
    DashLegacyWheel045Press press;
    uint8_t out[8];
    arm(dnd);
    press.configure(true);
    press.setRuntimeAllowed(true);
    bindSiblings(dnd, press);

    // Both engines see the same warning and the same native beats.
    dnd.onCan1Frame(kIdleC, 8, 1000);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1000);
    press.onDasStatus(6, 3, 1000);

    // The right-wheel engine presses on the 1050 beat.
    uint8_t pressFrame[8];
    TEST_ASSERT_TRUE(press.onNative045(kIdleC, 8, 1050, pressFrame));
    press.recordTxResult(true, 1050);
    // Left DND sees the press frame (sibling) and must not take the slot.
    dnd.onCan1Frame(pressFrame, 8, 1050);
    TEST_ASSERT_FALSE(dnd.nextFrame(1050, out));
    TEST_ASSERT_TRUE(dnd.diag.siblingYields >= 1);

    // The release rides the next native beat (1160).
    uint8_t rel[8];
    TEST_ASSERT_TRUE(press.onNative045(kIdleD, 8, 1160, rel));
    TEST_ASSERT_FALSE(DashLegacyWheel045Press::isPressFrame(rel));
    press.recordTxResult(true, 1160);

    // Before any further native frame the right-wheel engine still owns the
    // beat (the release went out, but no native frame has followed it).
    dnd.onCan1Frame(kIdleC, 8, 1300);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1300);
    TEST_ASSERT_TRUE(press.siblingOwnsBeat(1301));
    TEST_ASSERT_FALSE(dnd.nextFrame(1300, out));

    // The SAME native frame reaches the right-wheel engine (the wiring feeds
    // both): that is what frees the slot, and the left DND resumes.
    uint8_t nothing[8];
    TEST_ASSERT_FALSE(press.onNative045(kIdleC, 8, 1300, nothing)); // too soon to click again
    TEST_ASSERT_FALSE(press.siblingOwnsBeat(1301));
    dnd.onCan1Frame(kIdleD, 8, 1500);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1500);
    TEST_ASSERT_TRUE(dnd.nextFrame(1500, out));
}

// The CH-bus view feeds the right-wheel engine's observation only: it must
// release the yield slot after a click, and never emit on it.
void test_ch_view_releases_beat_without_emitting()
{
    DashLegacyWheel045Dnd dnd;
    DashLegacyWheel045Press press;
    uint8_t out[8];
    arm(dnd);
    press.configure(true);
    press.setRuntimeAllowed(true);
    bindSiblings(dnd, press);
    dnd.onCan1Frame(kIdleC, 8, 1000);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1000);
    press.onDasStatus(6, 3, 1000);
    uint8_t pressFrame[8];
    TEST_ASSERT_TRUE(press.onNative045(kIdleC, 8, 1050, pressFrame));
    press.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(press.siblingOwnsBeat(1060));
    // A native frame arrives on the CH view: observed, nothing emitted.
    press.observeNative045(kIdleD, 8, 1160);
    TEST_ASSERT_EQUAL_UINT32(1, press.diag.pressed); // unchanged
    TEST_ASSERT_TRUE(press.releasePending());         // release still owed
    TEST_ASSERT_TRUE(press.siblingOwnsBeat(1170));    // click not finished
    // Release goes out on the VEH beat, then a CH-view frame frees the slot.
    uint8_t rel[8];
    TEST_ASSERT_TRUE(press.onNative045(kIdleD, 8, 1270, rel));
    press.recordTxResult(true, 1270);
    press.observeNative045(kIdleC, 8, 1400);
    TEST_ASSERT_FALSE(press.siblingOwnsBeat(1401));
}

// Review I5, the starvation case: a gate drops right after a click, before
// any further native frame. The right-wheel engine must stop owning the beat
// so the left-wheel DND is not wedged until reboot.
void test_dnd_not_starved_when_press_engine_loses_its_gate()
{
    DashLegacyWheel045Dnd dnd;
    DashLegacyWheel045Press press;
    uint8_t out[8];
    arm(dnd);
    press.configure(true);
    press.setRuntimeAllowed(true);
    bindSiblings(dnd, press);
    dnd.onCan1Frame(kIdleC, 8, 1000);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1000);
    press.onDasStatus(6, 3, 1000);
    uint8_t pressFrame[8];
    TEST_ASSERT_TRUE(press.onNative045(kIdleC, 8, 1050, pressFrame));
    press.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(press.siblingOwnsBeat(1060));
    // The gate drops (vehicle OTA flag, jitter procedure, ...).
    press.setRuntimeAllowed(false);
    TEST_ASSERT_FALSE(press.siblingOwnsBeat(1070));
    // ...and comes back: the beat must still not be owned.
    press.setRuntimeAllowed(true);
    TEST_ASSERT_FALSE(press.siblingOwnsBeat(1080));
    dnd.onCan1Frame(kIdleD, 8, 1200);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, 1200);
    TEST_ASSERT_TRUE(dnd.nextFrame(1200, out));
}

static void disarmTwice(DashLegacyWheel045Dnd &dnd, uint32_t &nowMs)
{
    uint8_t out[8];
    dnd.onCan1Frame(kIdleC, 8, nowMs);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, nowMs);
    TEST_ASSERT_TRUE(dnd.nextFrame(nowMs, out));
    dnd.recordTxResult(false, nowMs);
    nowMs += 100;
    dnd.onCan1Frame(kIdleD, 8, nowMs);
    dnd.onDasStatus(6, 2, CAN_BUS_VEH, nowMs);
    nowMs += 50;
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, nowMs);
    TEST_ASSERT_TRUE(dnd.nextFrame(nowMs, out));
    dnd.recordTxResult(false, nowMs);
    TEST_ASSERT_TRUE(dnd.diag.autoDisarmed);
}

void test_disarm_heals_after_800ms()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8];
    arm(dnd);
    uint32_t nowMs = 1000;
    disarmTwice(dnd, nowMs); // disarmed at ~1150
    // 700 ms later: still disarmed.
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, nowMs + 700);
    TEST_ASSERT_TRUE(dnd.diag.autoDisarmed);
    TEST_ASSERT_FALSE(dnd.nextFrame(nowMs + 700, out));
    // 810 ms later: the heal fires inside onDasStatus and the same
    // warning edge restarts the episode.
    dnd.onCan1Frame(kIdleC, 8, nowMs + 810);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, nowMs + 810);
    TEST_ASSERT_FALSE(dnd.diag.autoDisarmed);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.disarmHeals);
    TEST_ASSERT_TRUE(dnd.nextFrame(nowMs + 810, out));
}

void test_disarm_heal_capped_three_per_window()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8];
    arm(dnd);
    uint32_t nowMs = 1000;
    for (int round = 0; round < 3; round++)
    {
        disarmTwice(dnd, nowMs);
        nowMs += 810;
        dnd.onCan1Frame(kIdleC, 8, nowMs);
        dnd.onDasStatus(6, 3, CAN_BUS_VEH, nowMs);
        TEST_ASSERT_FALSE(dnd.diag.autoDisarmed); // healed
    }
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.disarmHeals);
    // Fourth disarm inside the 60 s window: the cap holds.
    disarmTwice(dnd, nowMs);
    nowMs += 810;
    dnd.onCan1Frame(kIdleC, 8, nowMs);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, nowMs);
    TEST_ASSERT_TRUE(dnd.diag.autoDisarmed); // window cap held
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.disarmHeals);
}

void test_disarm_heal_window_slides_reopens()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8];
    arm(dnd);
    uint32_t nowMs = 1000;
    for (int round = 0; round < 4; round++)
    {
        disarmTwice(dnd, nowMs);
        nowMs += 810;
        dnd.onCan1Frame(kIdleC, 8, nowMs);
        dnd.onDasStatus(6, 3, CAN_BUS_VEH, nowMs);
    }
    TEST_ASSERT_EQUAL_UINT32(3, dnd.diag.disarmHeals);
    TEST_ASSERT_TRUE(dnd.diag.autoDisarmed); // capped at three
    // Once the 60 s window slides past, the budget reopens.
    nowMs += 61000;
    dnd.onCan1Frame(kIdleC, 8, nowMs);
    dnd.onDasStatus(6, 3, CAN_BUS_VEH, nowMs);
    TEST_ASSERT_FALSE(dnd.diag.autoDisarmed);
    TEST_ASSERT_EQUAL_UINT32(4, dnd.diag.disarmHeals);
}

void test_healthy_engine_edge_no_heal_reset()
{
    DashLegacyWheel045Dnd dnd;
    uint8_t out[8];
    arm(dnd);
    seedAndWarn(dnd, 1000);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.warningEpisodes);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.disarmHeals);
    TEST_ASSERT_TRUE(dnd.nextFrame(1000, out));
    dnd.recordTxResult(true, 1000);
    TEST_ASSERT_FALSE(dnd.diag.autoDisarmed);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.warningEpisodes); // untouched
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
    RUN_TEST(test_disarm_heals_after_800ms);
    RUN_TEST(test_dnd_ignores_right_press_sibling_frames);
    RUN_TEST(test_dnd_yields_during_click_then_resumes);
    RUN_TEST(test_dnd_not_starved_when_press_engine_loses_its_gate);
    RUN_TEST(test_ch_view_releases_beat_without_emitting);
    RUN_TEST(test_disarm_heal_capped_three_per_window);
    RUN_TEST(test_disarm_heal_window_slides_reopens);
    RUN_TEST(test_healthy_engine_edge_no_heal_reset);
    RUN_TEST(test_gateway_mirror_is_diagnostic_only);
    RUN_TEST(test_release_timeout_aborts_without_free_running_send);
    RUN_TEST(test_warning_episode_is_limited_to_three_positive_attempts);
    return UNITY_END();
}
