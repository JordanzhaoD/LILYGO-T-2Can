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

// ── Level semantics: 1:1 follow while hands ∈ [3,5] ─────────────────────

void test_in_window_native_idle_clones_every_beat()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT8(kIdleC[3] | 0x08, out[3]);
    TEST_ASSERT_EQUAL_UINT8(((kIdleC[6] >> 4) + 1) & 0x0F, (out[6] >> 4) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[6] & 0x0F, out[6] & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.cloned);

    // Level stays in-window: the NEXT native idle beat ALSO clones. No
    // single-shot consumption — this is the whole point of v3.
    uint8_t out2[8];
    TEST_ASSERT_TRUE(nk.onNative045(kIdleD, 8, 1100, out2));
    TEST_ASSERT_EQUAL_UINT8(kIdleD[3] | 0x08, out2[3]);
    TEST_ASSERT_EQUAL_UINT32(2, nk.diag.cloned);
}

void test_sustained_warning_clones_every_native_beat()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    // 0x399 keeps feeding hands=3 at ~2 Hz; the native 0x045 stream is ~10 Hz.
    // Every idle native beat must clone while the level stays in-window —
    // this is the continuous-overwrite the field verdict demands.
    for (uint32_t t = 1000; t <= 6000; t += 500)
    {
        warn(nk, t, 3);
        uint8_t o[8];
        TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, t + 40, o));
        nk.recordTxResult(true, t + 40);
    }
    TEST_ASSERT_EQUAL_UINT32(11, nk.diag.cloned);
}

void test_out_of_window_stops_immediately()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    // hands leaves the window: the very next native beat must NOT clone.
    warn(nk, 1100, 1);
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.cloned);
    TEST_ASSERT_FALSE(nk.diag.inWindow);
}

void test_hands6_hard_off_stops_cloning()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 4); // in-window
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    warn(nk, 1100, 6); // escalation into strike-out domain
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.cloned);
    TEST_ASSERT_FALSE(nk.diag.inWindow);
}

void test_window_excludes_hands2_and_hands6()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 2);
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1050, out));
    warn(nk, 2000, 6);
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 2050, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
}

void test_reenter_window_resumes_cloning()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    warn(nk, 1100, 1); // leaves
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1150, out));
    warn(nk, 1200, 4); // re-enters
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_UINT32(2, nk.diag.cloned);
}

// ── Safety gates retained ────────────────────────────────────────────────

void test_own_echo_does_not_chain()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(DashLegacyNagKill045::isCloneFrame(out));
    // The echoed clone comes back: recognised, not re-cloned (level still
    // in-window, but the frame carries bit27 → echo suppression wins).
    uint8_t out2[8];
    TEST_ASSERT_FALSE(nk.onNative045(out, 8, 1250, out2));
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.echoSuppressed);
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.cloned);
}

void test_two_tx_failures_disarm_then_heal_800ms()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    // fail 1
    warn(nk, 1000, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(false, 1050);
    TEST_ASSERT_FALSE(nk.diag.disarmed);
    // fail 2 — level semantics: no new edge needed, the level persists;
    // consecutive failure counts across beats.
    warn(nk, 1100, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1150, out));
    nk.recordTxResult(false, 1150);
    TEST_ASSERT_TRUE(nk.diag.disarmed);
    // While disarmed, even in-window beats do not clone.
    warn(nk, 1200, 3);
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_UINT32(2, nk.diag.cloned); // two clones sent, two failed
    // 810 ms after the disarm: healed; an in-window beat clones again.
    warn(nk, 1150 + 810, 3);
    TEST_ASSERT_FALSE(nk.diag.disarmed);
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.disarmHeals);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1150 + 850, out));
    TEST_ASSERT_EQUAL_UINT32(3, nk.diag.cloned);
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

// ── Final-review I1: yield until the NEXT native frame, not 20 ms ────────

void test_yield_gate_blocks_until_next_native_frame_observed()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(nk.siblingOwnsBeat(2000));
    // The DND feeds the same clone echo back (suppressed), still no NATIVE
    // frame: still owned.
    TEST_ASSERT_FALSE(nk.onNative045(out, 8, 2100, out));
    TEST_ASSERT_TRUE(nk.siblingOwnsBeat(2100));
    // A fresh NATIVE frame arrives (still in-window → would clone, but here
    // it releases the yield slot regardless).
    warn(nk, 2200, 1); // drop out of window so the native frame is a pure release
    TEST_ASSERT_FALSE(nk.onNative045(kIdleD, 8, 2200, out));
    TEST_ASSERT_FALSE(nk.siblingOwnsBeat(2250));
}

// ── Final-review I2: only IDLE native frames may be ridden ───────────────

void test_action_frames_are_never_ridden()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 3);
    uint8_t tick[8];
    memcpy(tick, kIdleC, 8);
    tick[4] = 0x40; // positive tick (volume nibble set in b4)
    tick[7] = DashLegacyWheel045Dnd::crc8J1850(tick, 7);
    TEST_ASSERT_FALSE(nk.onNative045(tick, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
    TEST_ASSERT_TRUE(nk.diag.actionFrameSkipped >= 1);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_in_window_native_idle_clones_every_beat);
    RUN_TEST(test_sustained_warning_clones_every_native_beat);
    RUN_TEST(test_out_of_window_stops_immediately);
    RUN_TEST(test_hands6_hard_off_stops_cloning);
    RUN_TEST(test_window_excludes_hands2_and_hands6);
    RUN_TEST(test_reenter_window_resumes_cloning);
    RUN_TEST(test_own_echo_does_not_chain);
    RUN_TEST(test_two_tx_failures_disarm_then_heal_800ms);
    RUN_TEST(test_disabled_by_default_no_activity);
    RUN_TEST(test_clone_in_flight_window);
    RUN_TEST(test_yield_gate_blocks_until_next_native_frame_observed);
    RUN_TEST(test_action_frames_are_never_ridden);
    return UNITY_END();
}
