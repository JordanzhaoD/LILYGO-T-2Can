#include <unity.h>

#include "dash_legacy_nag_kill_045.h"
#include "dash_legacy_wheel_045_dnd.h" // crc8J1850 for expected values

// Native 0x045 idle frames (same vector family as the wheel-DND suite).
static const uint8_t kIdleC[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xC0, 0xE0};
static const uint8_t kIdleD[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xD0, 0x2D};

static void arm(DashLegacyNagKill045 &nk)
{
    nk.configure(true);
    nk.setRuntimeAllowed(true);
}

static void warn(DashLegacyNagKill045 &nk, uint32_t nowMs, uint8_t handsState)
{
    nk.onDasStatus(6, handsState, nowMs);
}

void test_warning_edge_plus_native_frame_clones_exactly_one()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    // bit27 (byte3 bit3) set
    TEST_ASSERT_EQUAL_UINT8(kIdleC[3] | 0x08, out[3]);
    // counter +1 in b6 high nibble
    TEST_ASSERT_EQUAL_UINT8(((kIdleC[6] >> 4) + 1) & 0x0F, (out[6] >> 4) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[6] & 0x0F, out[6] & 0x0F);
    // CRC8-J1850 over the rewritten frame
    TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.cloned);
    // single-shot: the next native frame within the same edge does not clone
    uint8_t out2[8];
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1100, out2));
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.cloned);
}

void test_window_excludes_hands2_and_hands6()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 2);
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1050, out));
    warn(nk, 2000, 6); // strike-out domain: hard-off
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 2050, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
}

void test_own_echo_does_not_chain()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(true, 1050);
    // the echoed clone comes back: recognised, no new trigger, no clone
    TEST_ASSERT_TRUE(DashLegacyNagKill045::isCloneFrame(out));
    warn(nk, 1200, 3); // fresh edge
    uint8_t out2[8];
    TEST_ASSERT_FALSE(nk.onNative045(out, 8, 1250, out2));
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.echoSuppressed);
}

void test_trigger_expires_without_native_frame()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    // no native frame for 600 ms: the trigger flag expires
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1601, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
}

void test_two_tx_failures_disarm_then_heal_800ms()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    // fail 1 (edge #1)
    warn(nk, 1000, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(false, 1050);
    TEST_ASSERT_FALSE(nk.diag.disarmed);
    // fail 2 needs a NEW warning edge (edge-triggered engine): the
    // warning clears and returns.
    warn(nk, 1100, 1);
    warn(nk, 1150, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1200, out));
    nk.recordTxResult(false, 1200);
    TEST_ASSERT_TRUE(nk.diag.disarmed);
    // 700 ms after the disarm: still down (no new edge can arm while disarmed)
    warn(nk, 1200 + 700, 1);
    warn(nk, 1200 + 720, 3);
    TEST_ASSERT_TRUE(nk.diag.disarmed);
    // 810 ms after the disarm: healed; a fresh edge + native frame clones
    warn(nk, 1200 + 810, 3);
    TEST_ASSERT_FALSE(nk.diag.disarmed);
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.disarmHeals);
    warn(nk, 1200 + 815, 1);
    warn(nk, 1200 + 820, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1200 + 850, out));
}

void test_disabled_by_default_no_activity()
{
    DashLegacyNagKill045 nk; // never configured
    nk.setRuntimeAllowed(true);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
}

void test_clone_in_flight_window()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(nk.cloneInFlight(1060));
    TEST_ASSERT_FALSE(nk.cloneInFlight(1080));
}

// ── Final-review C1: edge-triggered, one clone per warning EDGE ──────────

void test_persistent_warning_does_not_rearm_level_style()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    // 0x399 arrives repeatedly at hands=3 through a sustained warning
    // (~2 Hz). The engine must arm ONCE per edge — the level-style
    // rearm produced 13 clones over a 6 s warning in review simulation.
    warn(nk, 1000, 3); // edge: outside-window → in-window arms
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1040, out));
    nk.recordTxResult(true, 1040);
    for (uint32_t t = 1500; t <= 6000; t += 500)
    {
        warn(nk, t, 3); // same warning, level stays in-window
        uint8_t out2[8];
        TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, t + 40, out2));
    }
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.cloned);
    // Warning clears, then a NEW edge re-arms.
    warn(nk, 6500, 1);
    warn(nk, 7000, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 7040, out));
    TEST_ASSERT_EQUAL_UINT32(2, nk.diag.cloned);
}

// ── Final-review C2: hands >= 6 hard-off clears an armed trigger ─────────

void test_escalation_to_hands6_disarms_armed_trigger()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 4);       // in-window arm
    warn(nk, 1050, 6);       // escalation into the strike-out domain
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1100, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
}

// ── Final-review I1: yield until the NEXT native frame, not 20 ms ────────

void test_yield_gate_blocks_until_next_native_frame_observed()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(true, 1050);
    // No new native frame observed yet: clone still owns the beat, even
    // long after 20 ms.
    TEST_ASSERT_TRUE(nk.siblingOwnsBeat(2000));
    // The DND feeds the same clone echo back (suppressed), still no NATIVE
    // frame: still owned.
    TEST_ASSERT_FALSE(nk.onNative045(out, 8, 2100, out));
    TEST_ASSERT_TRUE(nk.siblingOwnsBeat(2100));
    // A fresh NATIVE frame arrives: the beat is released.
    TEST_ASSERT_FALSE(nk.onNative045(kIdleD, 8, 2200, out)); // idle, no trigger armed
    TEST_ASSERT_FALSE(nk.siblingOwnsBeat(2250));
}

// ── Final-review I2: only IDLE native frames may be ridden ───────────────

void test_action_frames_are_never_ridden()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 3);
    // A wheel-DND tick echo (volume nibble set in b4) or a jitter stalk
    // frame: carrying a functional payload — must NOT be cloned.
    uint8_t tick[8];
    memcpy(tick, kIdleC, 8);
    tick[4] = 0x40; // positive tick
    tick[7] = DashLegacyWheel045Dnd::crc8J1850(tick, 7);
    TEST_ASSERT_FALSE(nk.onNative045(tick, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
    TEST_ASSERT_TRUE(nk.diag.actionFrameSkipped >= 1);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_warning_edge_plus_native_frame_clones_exactly_one);
    RUN_TEST(test_window_excludes_hands2_and_hands6);
    RUN_TEST(test_own_echo_does_not_chain);
    RUN_TEST(test_trigger_expires_without_native_frame);
    RUN_TEST(test_two_tx_failures_disarm_then_heal_800ms);
    RUN_TEST(test_disabled_by_default_no_activity);
    RUN_TEST(test_clone_in_flight_window);
    RUN_TEST(test_persistent_warning_does_not_rearm_level_style);
    RUN_TEST(test_escalation_to_hands6_disarms_armed_trigger);
    RUN_TEST(test_yield_gate_blocks_until_next_native_frame_observed);
    RUN_TEST(test_action_frames_are_never_ridden);
    return UNITY_END();
}
