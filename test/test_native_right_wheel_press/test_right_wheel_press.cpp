#include <unity.h>

#include "dash_legacy_wheel_045_dnd.h" // crc8J1850 for expected values
#include "dash_legacy_wheel_045_press.h"

// Native 0x045 idle frames (same vector family as the wheel-DND suite).
static const uint8_t kIdleC[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xC0, 0xE0};
static const uint8_t kIdleD[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xD0, 0x2D};

static void arm(DashLegacyWheel045Press &p)
{
    p.configure(true);
    p.setRuntimeAllowed(true);
}

static void warn(DashLegacyWheel045Press &p, uint32_t nowMs, uint8_t handsState)
{
    p.onDasStatus(6, handsState, nowMs);
}

// ── Press encoding: switch word bit4 (data[4] |= 0x10) ───────────────────

void test_warning_active_presses_every_idle_beat()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    // press bit set (data[4] bit4), all other bytes untouched except counter
    // and CRC.
    TEST_ASSERT_EQUAL_UINT8(kIdleC[4] | 0x10, out[4]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[0], out[0]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[3], out[3]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[5], out[5]);
    // counter +1 in b6 high nibble
    TEST_ASSERT_EQUAL_UINT8(((kIdleC[6] >> 4) + 1) & 0x0F, (out[6] >> 4) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[6] & 0x0F, out[6] & 0x0F);
    // CRC8-J1850 over the rewritten frame
    TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);

    // Level persists: the next idle beat ALSO presses.
    uint8_t out2[8];
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1100, out2));
    TEST_ASSERT_EQUAL_UINT8(kIdleD[4] | 0x10, out2[4]);
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
}

void test_warning_clear_stops_pressing_immediately()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    warn(p, 1100, 1); // hands leaves the window
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
    TEST_ASSERT_FALSE(p.diag.warningActive);
}

void test_ap_inactive_blocks_press()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    p.onDasStatus(1, 3, 1000); // AP state 1 (not active): warning not armed
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
    // AP activates with hands in window: presses resume.
    p.onDasStatus(6, 3, 1100);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
}

void test_hands6_and_hands9_10_are_warning_states()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 6);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    warn(p, 1100, 9);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
    // hands 2 and 7 and 11 are NOT warning states.
    warn(p, 1200, 2);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1250, out));
    warn(p, 1300, 7);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1350, out));
    warn(p, 1400, 11);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1450, out));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
}

// ── Safety gates retained ────────────────────────────────────────────────

void test_own_echo_does_not_chain()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(DashLegacyWheel045Press::isPressFrame(out));
    // The echoed press frame comes back: recognised, not re-pressed.
    uint8_t out2[8];
    TEST_ASSERT_FALSE(p.onNative045(out, 8, 1250, out2));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.echoSuppressed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
}

void test_action_frames_are_never_pressed()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 3);
    uint8_t out[8];
    // A left-wheel DND tick echo (volume nibble in b4) carries a functional
    // payload — must NOT be pressed.
    uint8_t tick[8];
    memcpy(tick, kIdleC, 8);
    tick[4] = 0x40; // positive tick
    tick[7] = DashLegacyWheel045Dnd::crc8J1850(tick, 7);
    TEST_ASSERT_FALSE(p.onNative045(tick, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
    TEST_ASSERT_TRUE(p.diag.actionFrameSkipped >= 1);
}

void test_two_tx_failures_disarm_then_heal_800ms()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(false, 1050);
    TEST_ASSERT_FALSE(p.diag.disarmed);
    warn(p, 1100, 3); // level persists
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1150, out));
    p.recordTxResult(false, 1150);
    TEST_ASSERT_TRUE(p.diag.disarmed);
    // While disarmed, in-window beats do not press.
    warn(p, 1200, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
    // 810 ms after disarm: healed; an in-window beat presses again.
    warn(p, 1150 + 810, 3);
    TEST_ASSERT_FALSE(p.diag.disarmed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.disarmHeals);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1150 + 850, out));
    TEST_ASSERT_EQUAL_UINT32(3, p.diag.pressed);
}

void test_disabled_by_default_no_activity()
{
    DashLegacyWheel045Press p; // never configured
    p.setRuntimeAllowed(true);
    warn(p, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
}

// ── Beat-generational yield with the left-wheel DND ──────────────────────

void test_yield_gate_blocks_until_next_native_frame_observed()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(2000));
    // The press echo comes back (suppressed), still no NATIVE frame: owned.
    TEST_ASSERT_FALSE(p.onNative045(out, 8, 2100, out));
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(2100));
    // A fresh NATIVE frame arrives: released.
    warn(p, 2200, 1); // drop out of window so the native frame is a pure release
    TEST_ASSERT_FALSE(p.onNative045(kIdleD, 8, 2200, out));
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(2250));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_warning_active_presses_every_idle_beat);
    RUN_TEST(test_warning_clear_stops_pressing_immediately);
    RUN_TEST(test_ap_inactive_blocks_press);
    RUN_TEST(test_hands6_and_hands9_10_are_warning_states);
    RUN_TEST(test_own_echo_does_not_chain);
    RUN_TEST(test_action_frames_are_never_pressed);
    RUN_TEST(test_two_tx_failures_disarm_then_heal_800ms);
    RUN_TEST(test_disabled_by_default_no_activity);
    RUN_TEST(test_yield_gate_blocks_until_next_native_frame_observed);
    return UNITY_END();
}
