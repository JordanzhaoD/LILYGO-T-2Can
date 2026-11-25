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

// Native idle frame with an arbitrary counter (CRC recomputed).
static void idleWithCtr(uint8_t out[8], uint8_t counter)
{
    memcpy(out, kIdleC, 8);
    out[6] = static_cast<uint8_t>((out[6] & 0x0F) | ((counter & 0x0F) << 4));
    out[7] = DashLegacyWheel045Dnd::crc8J1850(out, 7);
}

// ═══ Click shape: press frame, then the release rides the NEXT native beat ═══
// Field basis for the bit: the CENTER press (0x0010) cleared 6/6 warnings on
// beta.07; the SIDE button (0x0020) cleared 0/6 on beta.09.

void test_press_encodes_center_button_bit_and_counter_and_crc()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    // CENTER press bit set (data[4] bit4 = switch word 0x0010); the side
    // button bit must stay clear.
    TEST_ASSERT_EQUAL_UINT8(kIdleC[4] | 0x10, out[4]);
    TEST_ASSERT_EQUAL_UINT8(0, out[4] & 0x20);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[0], out[0]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[3], out[3]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[5], out[5]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[6] & 0x0F, out[6] & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(((kIdleC[6] >> 4) + 1) & 0x0F, ctr(out));
    TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
    TEST_ASSERT_TRUE(p.releasePending());
    p.recordTxResult(true, 1050);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.released);
}

// The release is the NEXT native frame untouched except counter+1 and CRC:
// no button bits, every other byte exactly what the car sent. No c+2 frame,
// so the bus counter never regresses (review M6).
void test_release_rides_next_native_frame_without_counter_regression()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    uint8_t n1[8], n2[8];
    idleWithCtr(n1, 0xA);
    idleWithCtr(n2, 0xB); // the car's own next frame already advanced
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(n1, 8, 1050, out)); // press: c+1 = B
    TEST_ASSERT_EQUAL_UINT8(0xB, ctr(out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(n2, 8, 1160, out)); // release rides n2: c+1 = C
    TEST_ASSERT_EQUAL_UINT8(0x00, out[4] & 0x3F);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[0], out[0]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[1], out[1]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[2], out[2]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[3], out[3]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[5], out[5]);
    TEST_ASSERT_EQUAL_UINT8(0xC, ctr(out));
    TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
    TEST_ASSERT_FALSE(p.releasePending());
    p.recordTxResult(true, 1160);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.released);
    // Bus sequence: native A, press B, native B, release C — monotonic,
    // duplicates only (the shape the field-stable left-wheel DND also has).
}

// Counter wrap, both frames (all 16 values).
void test_counter_wraps_on_press_and_release()
{
    for (uint8_t c = 0; c < 16; c++)
    {
        DashLegacyWheel045Press p;
        arm(p);
        uint8_t out[8], n1[8], n2[8];
        idleWithCtr(n1, c);
        idleWithCtr(n2, static_cast<uint8_t>((c + 1) & 0x0F));
        warn(p, 1000, 3);
        TEST_ASSERT_TRUE(p.onNative045(n1, 8, 1050, out));
        TEST_ASSERT_EQUAL_UINT8((c + 1) & 0x0F, ctr(out));
        TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
        p.recordTxResult(true, 1050);
        TEST_ASSERT_TRUE(p.onNative045(n2, 8, 1160, out));
        TEST_ASSERT_EQUAL_UINT8((c + 2) & 0x0F, ctr(out));
        TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
    }
}

// While a release is owed, a fresh native beat carries the release and
// nothing else — never a second press stacked on a held button.
void test_release_goes_out_before_any_second_press()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    warn(p, 1100, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1160, out)); // this is the RELEASE
    TEST_ASSERT_EQUAL_UINT8(0x00, out[4] & 0x3F);
    TEST_ASSERT_EQUAL_UINT8(1, p.diag.pressesThisEpisode);
}

// ═══ Episode budget: at most TWO clicks, sticky until the warning ends ═══

void test_episode_caps_at_two_clicks_and_reason_is_sticky()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out)); // press 1
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1160, out)); // release 1
    p.recordTxResult(true, 1160);
    warn(p, 1250, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1350, out)); // press 2 (190 ms after release 1)
    p.recordTxResult(true, 1350);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1460, out)); // release 2
    p.recordTxResult(true, 1460);
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.released);
    // The third click is refused, and the reason STAYS episode_limit on every
    // following beat while the warning persists (review I2: it used to be
    // overwritten by "idle" one beat later).
    for (uint32_t t = 1600; t <= 2600; t += 100)
    {
        warn(p, t, 3);
        TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, t + 50, out));
        TEST_ASSERT_EQUAL_STRING("episode_limit", p.diag.blockedReason);
    }
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
}

// ═══ C1: a warning that flickers back must NOT re-open an episode ═══
// The per-episode budget used to be refilled on every window ENTRY, so hands
// jittering 2<->3 (or any gate dropping and re-raising) clicked at the
// spacing rate for as long as the jitter lasted.

void test_warning_flicker_does_not_reopen_episode()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out)); // click 1 (press)
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1160, out)); // release
    p.recordTxResult(true, 1160);
    // 10 s of hands 2<->3 flicker at 50 ms with native beats every 100 ms.
    uint32_t clicked = 1;
    uint32_t t = 1200;
    for (int i = 0; i < 200; i++)
    {
        warn(p, t, (i % 2) ? 3 : 2);
        if (p.onNative045((i % 2) ? kIdleD : kIdleC, 8, t + 25, out))
        {
            clicked++;
            p.recordTxResult(true, t + 25);
        }
        t += 50;
    }
    TEST_ASSERT_EQUAL_UINT32(1, clicked);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.warningEpisodes);
}

// A gate dropping and re-raising while hands STAYS in the window must not
// re-arm either (second C1 scenario).
void test_gate_flap_while_warning_persists_does_not_rearm()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1160, out));
    p.recordTxResult(true, 1160);
    uint32_t clicked = 1;
    uint32_t t = 1200;
    for (int i = 0; i < 30; i++)
    {
        p.setRuntimeAllowed(false);
        p.setRuntimeAllowed(true);
        warn(p, t, 3); // hands never left the window
        if (p.onNative045(kIdleC, 8, t + 50, out))
        {
            clicked++;
            p.recordTxResult(true, t + 50);
        }
        t += 300;
    }
    TEST_ASSERT_EQUAL_UINT32(1, clicked);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.warningEpisodes);
}

// Once the warning has been absent for the settle time, a returning warning
// IS a new episode with a fresh budget.
void test_settled_clear_then_new_warning_reopens()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    const uint32_t S = DashLegacyWheel045Press::kWarningSettleMs;
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    warn(p, 1100, 1); // warning gone
    // Back inside the settle window: no new episode, reason "settling"
    warn(p, 1100 + S - 200, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleD, 8, 1100 + S - 150, out));
    TEST_ASSERT_EQUAL_STRING("settling", p.diag.blockedReason);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.warningEpisodes);
    // A warning that keeps flickering back never settles: every return
    // refreshes "last seen", so the absence never reaches S.
    for (int i = 0; i < 20; i++)
    {
        warn(p, 1100 + S - 200 + 1000 * i + 100, 1);
        warn(p, 1100 + S - 200 + 1000 * (i + 1), 3); // absent 900 ms < S
    }
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.warningEpisodes);
    // One uninterrupted absence of >= S: the next warning is a real new
    // episode. (The settle point is S after the LAST warning frame.)
    const uint32_t lastWarn = 1100 + S - 200 + 1000 * 20;
    warn(p, lastWarn + S - 100, 1); // still 100 ms short
    warn(p, lastWarn + S - 50, 3);  // flickered back in time: still episode 1
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.warningEpisodes);
    const uint32_t lastWarn2 = lastWarn + S - 50;
    warn(p, lastWarn2 + S, 1);      // absent for exactly S
    warn(p, lastWarn2 + S + 50, 3); // a real new episode
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.warningEpisodes);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, lastWarn2 + S + 100, out));
    // pressed/released count frames the BUS accepted (review I3): report
    // the TX result before counting.
    p.recordTxResult(true, lastWarn2 + S + 100);
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
}

// ═══ TX failures (review I3) ═══

// A press that failed never reached the bus: no release is owed, and the
// click is given back to the episode budget.
void test_failed_press_drops_release_and_refunds_the_click()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(false, 1050);
    TEST_ASSERT_FALSE(p.releasePending());
    TEST_ASSERT_EQUAL_UINT8(0, p.diag.pressesThisEpisode);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.txFail);
    // the next fresh beat may try again
    warn(p, 1200, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1250, out));
    TEST_ASSERT_NOT_EQUAL_UINT8(0, out[4] & 0x10); // a press, not a release
}

// A release that failed leaves the button HELD on the bus: the release is
// re-armed for the next native beat, counted as a failure, and no new press
// may go out in between.
void test_failed_release_is_retried_on_next_beat()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1160, out)); // release attempt
    p.recordTxResult(false, 1160);
    TEST_ASSERT_TRUE(p.releasePending());
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.released); // not counted: the bus did not take it
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.txFail);
    // next beat: the release again — not a second press
    warn(p, 1250, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1360, out));
    TEST_ASSERT_EQUAL_UINT8(0x00, out[4] & 0x3F);
    p.recordTxResult(true, 1360);
    TEST_ASSERT_FALSE(p.releasePending());
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.released);
    TEST_ASSERT_EQUAL_UINT8(1, p.diag.pressesThisEpisode);
}

// Two failed releases in a row disarm (the button would stay held).
void test_repeated_release_failures_disarm()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1160, out));
    p.recordTxResult(false, 1160);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1270, out));
    p.recordTxResult(false, 1270);
    TEST_ASSERT_TRUE(p.diag.disarmed);
}

// ═══ Warning window: the PROMPT ladder only ═══

// hands=2 (NOT_DETECTED) is the persistent "hands off wheel" baseline, NOT a
// prompt. beta.08 gated on it and blasted 853 frames -> strike-out.
void test_hands2_does_not_trigger()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    for (uint32_t t = 1000; t < 5000; t += 50)
    {
        warn(p, t, 2);
        TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, t + 25, out));
    }
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.warningEpisodes);
}

void test_hands3_prompt_triggers()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
}

void test_prompt_ladder_triggers()
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

// Leaving the window closes the episode at once: a release still owed is
// dropped (the car's own next native frame releases the button).
void test_clear_stops_immediately()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    warn(p, 1100, 1);
    TEST_ASSERT_FALSE(p.releasePending());
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1160, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
    TEST_ASSERT_EQUAL_STRING("warning_clear", p.diag.blockedReason);
}

// The 1<->2 jitter (detector noise) must not open an episode.
void test_hands12_jitter_does_not_open_episode()
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

// ═══ Runtime gates ═══

void test_ap_inactive_blocks_press()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    p.onDasStatus(1, 3, 1000); // AP off, hands reads "warning"
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
}

// While blocked by a gate the engine still OBSERVES the bus (the status
// grid shows the real warning) but never acts (review I2).
void test_blocked_engine_observes_but_does_not_act()
{
    DashLegacyWheel045Press p;
    p.configure(true);
    uint8_t out[8];
    p.onDasStatus(6, 4, 1000);
    TEST_ASSERT_TRUE(p.diag.warningActive); // observed
    TEST_ASSERT_TRUE(p.diag.apActive);
    TEST_ASSERT_EQUAL_UINT8(4, p.diag.handsState);
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

// ═══ Source-frame hygiene ═══

void test_own_press_echo_does_not_chain()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t press[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, press));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(DashLegacyWheel045Press::isPressFrame(press));
    // The echoed press frame comes back: recognised, never a release source.
    uint8_t out2[8];
    TEST_ASSERT_FALSE(p.onNative045(press, 8, 1160, out2));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.echoSuppressed);
    TEST_ASSERT_TRUE(p.releasePending()); // still owed to a real idle frame
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

// ═══ Disarm / heal ═══

void test_two_tx_failures_disarm_then_heal_800ms()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(false, 1050);
    TEST_ASSERT_FALSE(p.diag.disarmed);
    warn(p, 1200, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1250, out));
    p.recordTxResult(false, 1250); // second consecutive failure
    TEST_ASSERT_TRUE(p.diag.disarmed);
    TEST_ASSERT_EQUAL_STRING("disarmed", p.diag.blockedReason);
    // While disarmed, in-window beats do not press
    warn(p, 1300, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1350, out));
    // 810 ms after the disarm: healed
    warn(p, 1250 + 810, 3);
    TEST_ASSERT_FALSE(p.diag.disarmed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.disarmHeals);
}

// ═══ Yield to the left-wheel DND (review I5) ═══

// The click owns the beat from the press until the first idle native frame
// AFTER its release.
void test_yield_holds_until_native_after_release()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out)); // press
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1060));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1090)); // release not sent yet
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1160, out)); // release rides this beat
    p.recordTxResult(true, 1160);
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1170)); // owned until a native frame AFTER it
    // A native beat 30 ms after the release: too soon for another click
    // (spacing), and it is the first native frame after the release, so the
    // beat is released.
    warn(p, 1180, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1190, out));
    TEST_ASSERT_EQUAL_STRING("spacing", p.diag.blockedReason);
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(1191));
}

// I5: if a gate drops right after a click, the engine must stop owning the
// beat — otherwise the left-wheel DND yields forever (until reboot).
void test_blocked_engine_releases_the_beat()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out)); // press
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1060));
    // the gate drops before any further native frame is seen
    p.setRuntimeAllowed(false);
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(1070));
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(5000));
    // and comes back: still not owning
    p.setRuntimeAllowed(true);
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(5010));
}

// ═══ Diagnostics for the status grid ═══

void test_diag_tracks_native_beat_and_counters()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    TEST_ASSERT_FALSE(p.diag.native045Seen);
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1160, out));
    p.recordTxResult(true, 1160);
    TEST_ASSERT_TRUE(p.diag.native045Seen);
    TEST_ASSERT_EQUAL_UINT32(1160, p.diag.native045Ms);
    TEST_ASSERT_EQUAL_UINT8(kIdleD[6] >> 4, p.diag.nativeCounter);
    TEST_ASSERT_EQUAL_UINT32(1000, p.diag.dasStatusMs);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.released);
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.txFail);
    TEST_ASSERT_EQUAL_UINT8(1, p.diag.pressesThisEpisode); // ONE click (press + release)
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
    p.onDasStatus(6, 1, 1200); // gates open, bus quiet (AP engaged, no warning)
    TEST_ASSERT_EQUAL_STRING("idle", p.diag.blockedReason);
    p.onNative045(kIdleC, 8, 1250, out); // a native beat does not overwrite it
    TEST_ASSERT_EQUAL_STRING("idle", p.diag.blockedReason);
    // A warning this soon after the one at 1100 is still inside the settle
    // time (counted from the LAST warning frame): "settling", not a new episode.
    p.onDasStatus(6, 3, 1300);
    TEST_ASSERT_EQUAL_STRING("settling", p.diag.blockedReason);
    // After the full settle time — counted from the LAST warning frame,
    // which is now 1300 — the next warning opens an episode.
    const uint32_t tReady = 1300 + DashLegacyWheel045Press::kWarningSettleMs + 100;
    p.onDasStatus(6, 1, tReady - 50);
    p.onDasStatus(6, 3, tReady);
    TEST_ASSERT_EQUAL_STRING("waiting_native", p.diag.blockedReason);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, tReady + 50, out));
    TEST_ASSERT_EQUAL_STRING("armed", p.diag.blockedReason);
    p.recordTxResult(true, tReady + 50);
    // The same native beat processed again (same timestamp, as when a frame
    // is delivered twice): not a fresh beat, so the owed release cannot go
    // out and no second press may either.
    p.onDasStatus(6, 3, tReady + 60);
    TEST_ASSERT_FALSE(p.onNative045(kIdleD, 8, tReady + 50, out));
    TEST_ASSERT_EQUAL_STRING("release_pending", p.diag.blockedReason);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_press_encodes_center_button_bit_and_counter_and_crc);
    RUN_TEST(test_release_rides_next_native_frame_without_counter_regression);
    RUN_TEST(test_counter_wraps_on_press_and_release);
    RUN_TEST(test_release_goes_out_before_any_second_press);
    RUN_TEST(test_episode_caps_at_two_clicks_and_reason_is_sticky);
    RUN_TEST(test_warning_flicker_does_not_reopen_episode);
    RUN_TEST(test_gate_flap_while_warning_persists_does_not_rearm);
    RUN_TEST(test_settled_clear_then_new_warning_reopens);
    RUN_TEST(test_failed_press_drops_release_and_refunds_the_click);
    RUN_TEST(test_failed_release_is_retried_on_next_beat);
    RUN_TEST(test_repeated_release_failures_disarm);
    RUN_TEST(test_hands2_does_not_trigger);
    RUN_TEST(test_hands3_prompt_triggers);
    RUN_TEST(test_prompt_ladder_triggers);
    RUN_TEST(test_clear_stops_immediately);
    RUN_TEST(test_hands12_jitter_does_not_open_episode);
    RUN_TEST(test_ap_inactive_blocks_press);
    RUN_TEST(test_blocked_engine_observes_but_does_not_act);
    RUN_TEST(test_disabled_reports_disabled_and_observes);
    RUN_TEST(test_own_press_echo_does_not_chain);
    RUN_TEST(test_action_frames_are_never_used);
    RUN_TEST(test_two_tx_failures_disarm_then_heal_800ms);
    RUN_TEST(test_yield_holds_until_native_after_release);
    RUN_TEST(test_blocked_engine_releases_the_beat);
    RUN_TEST(test_diag_tracks_native_beat_and_counters);
    RUN_TEST(test_blocked_reasons_are_distinct);
    return UNITY_END();
}
