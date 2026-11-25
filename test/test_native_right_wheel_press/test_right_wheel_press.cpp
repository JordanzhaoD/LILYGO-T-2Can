#include <unity.h>
#include <initializer_list>

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

static uint8_t ctr(const uint8_t f[8]) { return (f[6] >> 4) & 0x0F; }

static void idleWithCtr(uint8_t out[8], uint8_t counter)
{
    memcpy(out, kIdleC, 8);
    out[6] = static_cast<uint8_t>((out[6] & 0x0F) | ((counter & 0x0F) << 4));
    out[7] = DashLegacyWheel045Dnd::crc8J1850(out, 7);
}

// ═══ 6.1.0-beta.12: BOUNDED LEVEL press (the beta.07 shape, with a cap) ═══
//
// Field matrix for the center button (data[4] |= 0x10), 2026-11-24/25:
//   beta.07  LEVEL: a press frame behind every idle native beat while the
//            warning lasts          -> cleared 6/6 nags (P, P, PPPPP, PPPP...)
//   beta.11  click: press + release, one per native beat
//            -> persistent nags 0/3 (5-8 s = their natural length)
// So what the vehicle accepts is a HELD button, not a tap. beta.07 had no
// bound (that shape, gated on hands=2, became the beta.08 853-frame blast),
// so the level press here is capped at kMaxPressesPerEpisode frames.

void test_press_encodes_center_button_bit_and_counter_and_crc()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT8(kIdleC[4] | 0x10, out[4]); // center button (switch word bit4)
    TEST_ASSERT_EQUAL_UINT8(0, out[4] & 0x20);         // side button never
    TEST_ASSERT_EQUAL_UINT8(kIdleC[0], out[0]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[1], out[1]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[2], out[2]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[3], out[3]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[5], out[5]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[6] & 0x0F, out[6] & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(((kIdleC[6] >> 4) + 1) & 0x0F, ctr(out));
    TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
}

// The shape that cleared nags in the field: one press frame behind EVERY
// idle native beat for as long as the warning lasts.
void test_level_presses_every_idle_beat_while_warning_lasts()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    uint32_t pressed = 0;
    for (uint32_t t = 1050; t < 1650; t += 100) // ~100 ms native cadence
    {
        uint8_t n[8];
        idleWithCtr(n, static_cast<uint8_t>(((t - 1050) / 100) & 0x0F));
        warn(p, t, 3);
        const bool r = p.onNative045(n, 8, t, out);
        if (r)
        {
            pressed++;
            TEST_ASSERT_NOT_EQUAL_UINT8(0, out[4] & 0x10); // a press, never a lone release
            TEST_ASSERT_EQUAL_UINT8((ctr(n) + 1) & 0x0F, ctr(out));
            TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
            p.recordTxResult(true, t);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(DashLegacyWheel045Press::kMaxPressesPerEpisode, pressed);
}

// The warning clears -> the press stops on the very next frame (the car's own
// next native idle frame releases the button, exactly like beta.07).
void test_clear_stops_the_press_at_once()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1150, out));
    p.recordTxResult(true, 1150);
    warn(p, 1200, 1); // hands back on the wheel
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
    TEST_ASSERT_EQUAL_STRING("warning_clear", p.diag.blockedReason);
}

// The cap (review C1 / the beta.08 blast): a warning that never clears gets
// at most kMaxPressesPerEpisode frames, then nothing more until the episode
// really ends — and the reason stays readable.
void test_level_press_is_capped()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    uint32_t pressed = 0;
    for (uint32_t t = 1050; t < 12050; t += 100) // 11 s of relentless warning
    {
        uint8_t n[8];
        idleWithCtr(n, static_cast<uint8_t>(((t - 1050) / 100) & 0x0F));
        warn(p, t, 3);
        if (p.onNative045(n, 8, t, out))
        {
            pressed++;
            p.recordTxResult(true, t);
        }
        else if (pressed == DashLegacyWheel045Press::kMaxPressesPerEpisode)
            TEST_ASSERT_EQUAL_STRING("episode_limit", p.diag.blockedReason);
    }
    TEST_ASSERT_EQUAL_UINT32(DashLegacyWheel045Press::kMaxPressesPerEpisode, pressed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.warningEpisodes);
}

// beta.08: gating on hands=2 (the "hands off wheel" baseline) blasted 853
// frames. hands=2 must never press.
void test_hands2_never_presses()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    for (uint32_t t = 1000; t < 6000; t += 50)
    {
        warn(p, t, 2);
        TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, t + 25, out));
    }
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.warningEpisodes);
}

// 1<->2 detector jitter is not a warning.
void test_hands12_jitter_does_not_press()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    for (uint32_t t = 1000; t < 4000; t += 25)
    {
        warn(p, t, (t / 25) % 2 ? 1 : 2);
        TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, t + 10, out));
    }
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.warningEpisodes);
}

void test_prompt_ladder_presses()
{
    for (uint8_t h : {3, 4, 5, 6, 9, 10})
    {
        DashLegacyWheel045Press p;
        arm(p);
        uint8_t out[8];
        warn(p, 1000, h);
        TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    }
    for (uint8_t h : {0, 1, 2, 7, 8, 11})
    {
        DashLegacyWheel045Press p;
        arm(p);
        uint8_t out[8];
        warn(p, 1000, h);
        TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    }
}

// ── C1: a warning that flickers back must NOT earn a new budget ─────────

void test_warning_flicker_does_not_reopen_episode()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out)); // press 1
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1150, out)); // press 2
    p.recordTxResult(true, 1150);
    // 10 s of hands 2<->3 flicker at 50 ms, native beats every 100 ms.
    uint32_t pressed = 2;
    uint32_t t = 1200;
    for (int i = 0; i < 200; i++)
    {
        warn(p, t, (i % 2) ? 3 : 2);
        if (p.onNative045((i % 2) ? kIdleD : kIdleC, 8, t + 25, out))
        {
            pressed++;
            p.recordTxResult(true, t + 25);
        }
        t += 50;
    }
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.warningEpisodes);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(DashLegacyWheel045Press::kMaxPressesPerEpisode, pressed);
}

void test_gate_flap_while_warning_persists_does_not_rearm()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1150, out));
    p.recordTxResult(true, 1150);
    uint32_t pressed = 2;
    uint32_t t = 1200;
    for (int i = 0; i < 30; i++)
    {
        p.setRuntimeAllowed(false);
        p.setRuntimeAllowed(true);
        warn(p, t, 3); // hands never left the window
        if (p.onNative045(kIdleC, 8, t + 50, out))
        {
            pressed++;
            p.recordTxResult(true, t + 50);
        }
        t += 300;
    }
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.warningEpisodes);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(DashLegacyWheel045Press::kMaxPressesPerEpisode, pressed);
}

// After the warning has been gone for the settle time, a returning warning
// is a real new episode with a fresh budget.
void test_settled_clear_then_new_warning_reopens()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    const uint32_t S = DashLegacyWheel045Press::kWarningSettleMs;
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    warn(p, 1100, 1);
    // back inside the settle window: not a new episode
    warn(p, 1100 + S - 200, 3);
    TEST_ASSERT_EQUAL_STRING("warn_flicker", p.diag.blockedReason);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.warningEpisodes);
    warn(p, 1100 + S - 200, 1);
    // gone for >= S: the next warning is a new episode
    warn(p, 1100 + S - 200 + S + 50, 3);
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.warningEpisodes);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1100 + S - 200 + S + 100, out));
}

// ── TX failures ─────────────────────────────────────────────────────────

void test_failed_press_is_counted_and_not_retried_on_the_same_beat()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(false, 1050);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.txFail);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed); // the bus never took it
    // the next native beat presses again (level semantics)
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1150, out));
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
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1150, out));
    p.recordTxResult(false, 1150);
    TEST_ASSERT_TRUE(p.diag.disarmed);
    warn(p, 1300, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1350, out));
    warn(p, 1150 + 810, 3);
    TEST_ASSERT_FALSE(p.diag.disarmed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.disarmHeals);
}

// ── Gates ───────────────────────────────────────────────────────────────

void test_ap_inactive_blocks_press()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    p.onDasStatus(1, 3, 1000);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
}

// While blocked the engine still OBSERVES the bus, but never acts.
void test_blocked_engine_observes_but_does_not_act()
{
    DashLegacyWheel045Press p;
    p.configure(true);
    uint8_t out[8];
    p.onDasStatus(6, 4, 1000);
    TEST_ASSERT_TRUE(p.diag.warningActive);
    TEST_ASSERT_EQUAL_STRING("runtime", p.diag.blockedReason);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.warningEpisodes);
}

void test_disabled_reports_disabled_and_observes()
{
    DashLegacyWheel045Press p; // never enabled
    p.setRuntimeAllowed(true);
    uint8_t out[8];
    p.onDasStatus(6, 3, 1000);
    TEST_ASSERT_TRUE(p.diag.warningActive);
    TEST_ASSERT_EQUAL_STRING("disabled", p.diag.blockedReason);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(1060));
}

// ── Source-frame hygiene ────────────────────────────────────────────────

void test_own_press_echo_does_not_chain()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t press[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, press));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(DashLegacyWheel045Press::isPressFrame(press));
    uint8_t out2[8];
    TEST_ASSERT_FALSE(p.onNative045(press, 8, 1100, out2)); // same beat, our own echo
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.echoSuppressed);
}

void test_action_frames_are_never_used()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    uint8_t tick[8];
    memcpy(tick, kIdleC, 8);
    tick[4] = 0x40; // a left-wheel tick echo
    tick[7] = DashLegacyWheel045Dnd::crc8J1850(tick, 7);
    TEST_ASSERT_FALSE(p.onNative045(tick, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.actionFrameSkipped);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
}

// ── Yield to the left-wheel DND ─────────────────────────────────────────

// With BOTH engines on they must not both transmit on one native beat. The
// left-wheel DND owns every beat the press engine does not take.
void test_yield_only_on_beats_where_it_presses()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out)); // press on this beat
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1051));             // -> owns it
    p.recordTxResult(true, 1050);
    p.setRuntimeAllowed(false);                            // a gate drops
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(1060));            // -> owns nothing
    p.setRuntimeAllowed(true);
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(1070));
}

// A gate dropping right after a press must not wedge the left-wheel DND.
void test_blocked_engine_releases_the_beat()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1060));
    p.setRuntimeAllowed(false);
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(5000));
}

// The CH-bus view is observe-only.
void test_ch_view_observes_without_emitting()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 3);
    p.observeNative045(kIdleD, 8, 1160);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
    TEST_ASSERT_TRUE(p.diag.native045Seen);
    TEST_ASSERT_EQUAL_UINT32(1160, p.diag.native045Ms);
}

// ── Diagnostics for the status grid ─────────────────────────────────────

void test_diag_tracks_native_beat_and_counters()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    TEST_ASSERT_FALSE(p.diag.native045Seen);
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1150, out));
    p.recordTxResult(true, 1150);
    TEST_ASSERT_TRUE(p.diag.native045Seen);
    TEST_ASSERT_EQUAL_UINT32(1150, p.diag.native045Ms);
    TEST_ASSERT_EQUAL_UINT8(kIdleD[6] >> 4, p.diag.nativeCounter);
    TEST_ASSERT_EQUAL_UINT32(1000, p.diag.dasStatusMs);
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.txFail);
    TEST_ASSERT_EQUAL_UINT8(2, p.diag.pressesThisEpisode);
    TEST_ASSERT_EQUAL_UINT8(6, p.diag.apState);
    TEST_ASSERT_TRUE(p.diag.apActive);
}

// Every "why not" the owner can read on the grid must be a distinct string.
void test_blocked_reasons_are_distinct()
{
    DashLegacyWheel045Press p;
    uint8_t out[8];
    p.setRuntimeAllowed(true);
    p.onDasStatus(6, 3, 1000);
    TEST_ASSERT_EQUAL_STRING("disabled", p.diag.blockedReason);
    p.configure(true);
    p.setRuntimeAllowed(false);
    p.onDasStatus(6, 3, 1100);
    TEST_ASSERT_EQUAL_STRING("runtime", p.diag.blockedReason);
    arm(p);
    p.onDasStatus(6, 1, 1150); // gates open, bus quiet (AP engaged, no warning)
    TEST_ASSERT_EQUAL_STRING("idle", p.diag.blockedReason);
    p.onNative045(kIdleC, 8, 1200, out); // a native beat does not overwrite it
    TEST_ASSERT_EQUAL_STRING("idle", p.diag.blockedReason);
    const uint32_t tReady = 1100 + DashLegacyWheel045Press::kWarningSettleMs + 100;
    p.onDasStatus(6, 3, tReady);
    TEST_ASSERT_EQUAL_STRING("waiting_native", p.diag.blockedReason);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, tReady + 50, out));
    TEST_ASSERT_EQUAL_STRING("armed", p.diag.blockedReason);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_press_encodes_center_button_bit_and_counter_and_crc);
    RUN_TEST(test_level_presses_every_idle_beat_while_warning_lasts);
    RUN_TEST(test_clear_stops_the_press_at_once);
    RUN_TEST(test_level_press_is_capped);
    RUN_TEST(test_hands2_never_presses);
    RUN_TEST(test_hands12_jitter_does_not_press);
    RUN_TEST(test_prompt_ladder_presses);
    RUN_TEST(test_warning_flicker_does_not_reopen_episode);
    RUN_TEST(test_gate_flap_while_warning_persists_does_not_rearm);
    RUN_TEST(test_settled_clear_then_new_warning_reopens);
    RUN_TEST(test_failed_press_is_counted_and_not_retried_on_the_same_beat);
    RUN_TEST(test_two_tx_failures_disarm_then_heal_800ms);
    RUN_TEST(test_ap_inactive_blocks_press);
    RUN_TEST(test_blocked_engine_observes_but_does_not_act);
    RUN_TEST(test_disabled_reports_disabled_and_observes);
    RUN_TEST(test_own_press_echo_does_not_chain);
    RUN_TEST(test_action_frames_are_never_used);
    RUN_TEST(test_yield_only_on_beats_where_it_presses);
    RUN_TEST(test_blocked_engine_releases_the_beat);
    RUN_TEST(test_ch_view_observes_without_emitting);
    RUN_TEST(test_diag_tracks_native_beat_and_counters);
    RUN_TEST(test_blocked_reasons_are_distinct);
    return UNITY_END();
}
