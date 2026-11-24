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

// ── Press encoding: switch word bit5 (data[4] |= 0x20) ───────────────────

void test_warning_active_presses_every_idle_beat()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 2); // hands=2 = NOT_DETECTED, the initial NAG state
    uint8_t out[8];
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    // side-button press bit set (data[4] bit5), other bytes untouched except
    // counter and CRC.
    TEST_ASSERT_EQUAL_UINT8(kIdleC[4] | 0x20, out[4]);
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
    TEST_ASSERT_EQUAL_UINT8(kIdleD[4] | 0x20, out2[4]);
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
}

// ── Warning window [2,7] (the full NAG ladder) ───────────────────────────

void test_hands2_is_the_primary_nag_state()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    // hands=2 (NOT_DETECTED) is the most common NAG state — it MUST trigger.
    warn(p, 1000, 2);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
}

void test_full_ladder_2_to_7_all_trigger()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    for (uint8_t hs = 2; hs <= 7; hs++)
    {
        warn(p, 1000 + hs * 100, hs);
        TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050 + hs * 100, out));
    }
    TEST_ASSERT_EQUAL_UINT32(6, p.diag.pressed);
}

void test_hands0_1_and_8_do_not_trigger()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 0);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    warn(p, 1100, 1);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1150, out));
    warn(p, 1200, 8); // suspended
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
}

// ── Debounced release (500 ms) ───────────────────────────────────────────

void test_release_is_debounced_500ms()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 2);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));

    // hands leaves the window at 1100: pressing continues (debounce running).
    warn(p, 1100, 1);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_TRUE(p.diag.warningActive);

    // Still out at 1400 (300ms): still pressing.
    warn(p, 1400, 1);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1450, out));

    // 500ms elapsed at 1600: now released.
    warn(p, 1600, 1);
    TEST_ASSERT_FALSE(p.diag.warningActive);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1650, out));
}

void test_jitter_does_not_interrupt_press()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 2);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));

    // hands jitters 1↔2 every ~25ms (the field-observed pattern): the
    // release debounce never completes, so pressing is continuous.
    for (uint32_t t = 1100; t <= 2000; t += 25)
    {
        const uint8_t hs = ((t / 25) % 2 == 0) ? 1 : 2;
        warn(p, t, hs);
        TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, t + 5, out));
    }
    TEST_ASSERT_TRUE(p.diag.warningActive);
}

void test_ap_inactive_blocks_press()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    p.onDasStatus(1, 2, 1000); // AP state 1 (not active): warning not armed
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
    // AP activates with hands in window: presses resume.
    p.onDasStatus(6, 2, 1100);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
}

// ── Safety gates retained ────────────────────────────────────────────────

void test_own_echo_does_not_chain()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 2);
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
    warn(p, 1000, 2);
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
    warn(p, 1000, 2);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(false, 1050);
    TEST_ASSERT_FALSE(p.diag.disarmed);
    warn(p, 1100, 2); // level persists
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1150, out));
    p.recordTxResult(false, 1150);
    TEST_ASSERT_TRUE(p.diag.disarmed);
    // While disarmed, in-window beats do not press.
    warn(p, 1200, 2);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
    // 810 ms after disarm: healed; an in-window beat presses again.
    warn(p, 1150 + 810, 2);
    TEST_ASSERT_FALSE(p.diag.disarmed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.disarmHeals);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1150 + 850, out));
    TEST_ASSERT_EQUAL_UINT32(3, p.diag.pressed);
}

void test_disabled_by_default_no_activity()
{
    DashLegacyWheel045Press p; // never configured
    p.setRuntimeAllowed(true);
    warn(p, 1000, 2);
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
    warn(p, 1000, 2);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(2000));
    // The press echo comes back (suppressed), still no NATIVE frame: owned.
    TEST_ASSERT_FALSE(p.onNative045(out, 8, 2100, out));
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(2100));
    // Drop out of window and let the debounce complete: a fresh NATIVE idle
    // frame then releases the beat.
    warn(p, 2200, 1);
    warn(p, 2800, 1); // 600ms out: debounce done, warning released
    TEST_ASSERT_FALSE(p.onNative045(kIdleD, 8, 2900, out));
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(2950));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_warning_active_presses_every_idle_beat);
    RUN_TEST(test_hands2_is_the_primary_nag_state);
    RUN_TEST(test_full_ladder_2_to_7_all_trigger);
    RUN_TEST(test_hands0_1_and_8_do_not_trigger);
    RUN_TEST(test_release_is_debounced_500ms);
    RUN_TEST(test_jitter_does_not_interrupt_press);
    RUN_TEST(test_ap_inactive_blocks_press);
    RUN_TEST(test_own_echo_does_not_chain);
    RUN_TEST(test_action_frames_are_never_pressed);
    RUN_TEST(test_two_tx_failures_disarm_then_heal_800ms);
    RUN_TEST(test_disabled_by_default_no_activity);
    RUN_TEST(test_yield_gate_blocks_until_next_native_frame_observed);
    return UNITY_END();
}
