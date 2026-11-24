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

// ── Press encoding: CENTER button, switch word bit4 (data[4] |= 0x10) ───
// Field verdict (beta.05-09, 23 captures): the CENTER press (0x0010) cleared
// 6/6 warnings on beta.07 — two of them with a SINGLE frame (9-26 ms). The
// side button (0x0020) cleared 0/6 on beta.09. The center bit is the only
// field-verified press signal.

void test_press_encodes_center_button_bit_and_counter_and_crc()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 3); // hands=3 = VISUAL, the actual prompt
    uint8_t out[8];
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    // CENTER press bit set (data[4] bit4 = switch word 0x0010); the side
    // button bit must stay clear.
    TEST_ASSERT_EQUAL_UINT8(kIdleC[4] | 0x10, out[4]);
    TEST_ASSERT_EQUAL_UINT8(0, out[4] & 0x20);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[0], out[0]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[3], out[3]);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[5], out[5]);
    // counter +1 in b6 high nibble
    TEST_ASSERT_EQUAL_UINT8(((kIdleC[6] >> 4) + 1) & 0x0F, (out[6] >> 4) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[6] & 0x0F, out[6] & 0x0F);
    // CRC8-J1850 over the rewritten frame
    TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
}

// ── Click shape: press frame + immediate RELEASE frame ──────────────────
// Every press pops the center-button menu ("phone not connected" card) —
// the head unit treats a lone press frame as a HELD button. A physical
// press is always followed by an idle (released) frame, so the engine now
// emits one too: the shortest click the bus can express.

void test_press_is_followed_by_release_frame()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    uint8_t pressCtr = (out[6] >> 4) & 0x0F;
    // Right after the press frame, the engine owes exactly one release.
    TEST_ASSERT_TRUE(p.releasePending());
    uint8_t rel[8];
    TEST_ASSERT_TRUE(p.popRelease(rel, 1050));
    // release = same idle frame, NO button bits, counter advanced once more
    TEST_ASSERT_EQUAL_UINT8(0x00, rel[4] & 0x3F);
    TEST_ASSERT_EQUAL_UINT8((pressCtr + 1) & 0x0F, (rel[6] >> 4) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(rel, 7), rel[7]);
    // exactly one release per press — never two
    TEST_ASSERT_FALSE(p.popRelease(rel, 1050));
    TEST_ASSERT_FALSE(p.releasePending());
}

// The release is still owed when a second native frame sneaks in before the
// release was popped: a new press must NOT be emitted first.
void test_second_press_waits_for_release_and_fresh_native()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    // native beat arrives while the release is still pending: no new press
    warn(p, 1100, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleD, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
    uint8_t rel[8];
    TEST_ASSERT_TRUE(p.popRelease(rel, 1050));
    // release popped, but this native beat was already consumed by the wait:
    // the NEXT fresh idle beat may press again (still inside the budget).
    warn(p, 1200, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
}

// ── Episode budget: at most TWO clicks per warning episode ──────────────

// beta.07 cleared with a single frame twice; two clicks bound the exposure
// far below the beta.08 blast. The third is refused until the warning
// clears and re-enters.
void test_episode_caps_at_two_clicks()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_TRUE(p.popRelease(out, 1050));
    warn(p, 1100, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1250, out)); // 200 ms after the release
    TEST_ASSERT_TRUE(p.popRelease(out, 1250));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT8(2, p.diag.pressesThisEpisode);
    warn(p, 1500, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1550, out));
    TEST_ASSERT_FALSE(p.releasePending());
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
    // warning clears and returns: budget reopens
    warn(p, 1600, 1);
    warn(p, 1700, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1750, out));
    TEST_ASSERT_EQUAL_UINT32(3, p.diag.pressed);
}

// ── Diagnostics for the status grid (mirrors the left-wheel DND grid) ───

void test_diag_counts_released_frames_and_tracks_native_beat()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    TEST_ASSERT_FALSE(p.diag.native045Seen);
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.released); // release not sent yet
    TEST_ASSERT_TRUE(p.popRelease(out, 1060));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.released);
    TEST_ASSERT_TRUE(p.diag.native045Seen);
    TEST_ASSERT_EQUAL_UINT32(1050, p.diag.native045Ms);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[6] >> 4, p.diag.nativeCounter);
    TEST_ASSERT_EQUAL_UINT32(1000, p.diag.dasStatusMs);
    TEST_ASSERT_TRUE(p.diag.warningActive);
    TEST_ASSERT_EQUAL_UINT8(6, p.diag.apState);
    TEST_ASSERT_TRUE(p.diag.apActive);
}

// The status grid shows why the engine is NOT pressing; each gate must
// leave a distinct, stable reason string.
void test_blocked_reasons_are_distinct()
{
    DashLegacyWheel045Press p;
    uint8_t out[8];
    p.setRuntimeAllowed(true);
    warn(p, 1000, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_STRING("disabled", p.diag.blockedReason);
    arm(p);
    warn(p, 1100, 1);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_EQUAL_STRING("idle", p.diag.blockedReason);
    warn(p, 1200, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_STRING("armed", p.diag.blockedReason);
    // second native beat while the release is still owed
    warn(p, 1260, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleD, 8, 1270, out));
    TEST_ASSERT_EQUAL_STRING("release_pending", p.diag.blockedReason);
    TEST_ASSERT_TRUE(p.popRelease(out, 1280));
    // A native beat arrives far too soon after the release (35 ms; the real
    // bus spaces native 0x045 frames ~100 ms apart): the minimum click
    // spacing still holds the engine back.
    TEST_ASSERT_FALSE(p.onNative045(kIdleD, 8, 1285, out));
    TEST_ASSERT_EQUAL_STRING("spacing", p.diag.blockedReason);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
    // A properly spaced fresh native beat lets the second click through.
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1280 + 160, out));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
}

// ── Warning window [3,6] ∪ [9,10] — the PROMPT ladder ────────────────────

// hands=2 (NOT_DETECTED) is the persistent "hands off wheel" baseline, NOT
// a prompt. beta.08 gated on it and blasted presses for the whole engagement
// (96.9 s uninterrupted), which escalated to strike-out. It MUST NOT fire.
void test_hands2_does_not_trigger()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 2);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
}

// hands=3 (VISUAL) is the real prompt: it triggers.
void test_hands3_prompt_triggers()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_TRUE(p.popRelease(out, 1050));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
}

// The full prompt ladder [3,6] ∪ [9,10] triggers; 0/1/2/7/8 do not.
void test_prompt_ladder_triggers()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    // 3,4,5,6 and 9,10 trigger
    const uint8_t triggerStates[] = {3, 4, 5, 6, 9, 10};
    for (uint8_t i = 0; i < sizeof(triggerStates); i++)
    {
        DashLegacyWheel045Press q;
        arm(q);
        warn(q, 1000, triggerStates[i]);
        TEST_ASSERT_TRUE(q.onNative045(kIdleC, 8, 1050, out));
    }
    // 0,1,2,7,8 do not
    const uint8_t quietStates[] = {0, 1, 2, 7, 8};
    for (uint8_t i = 0; i < sizeof(quietStates); i++)
    {
        DashLegacyWheel045Press q;
        arm(q);
        warn(q, 1000, quietStates[i]);
        TEST_ASSERT_FALSE(q.onNative045(kIdleC, 8, 1050, out));
    }
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
}

// ── EDGE semantics (the beta.08 level-frenzy reversal) ───────────────────

// A SUSTAINED warning is bounded: one press per fresh idle beat, up to the
// episode budget — NOT a per-beat blast for the whole engagement. This is
// the key contrast with beta.08, which fired every idle beat indefinitely
// while hands sat in 2.
void test_sustained_warning_is_episode_bounded()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3); // warning edge: arms the episode
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);

    // Warning persists: the episode allows further presses (bounded by the
    // budget), NOT an unbounded per-beat blast.
    warn(p, 1100, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
}

// ── Episode-limited: at most kMaxPressesPerEpisode ───────────────────────

// Each warning episode allows up to 3 presses (one per fresh idle beat),
// then stops intervening until the warning clears and re-enters.
void test_episode_caps_presses()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3); // edge opens the episode
    // Three presses, each riding the next idle beat.
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    warn(p, 1100, 3); // still warning (re-entry after a press re-arms one more)
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1150, out));
    warn(p, 1200, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_UINT32(3, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT8(3, p.diag.pressesThisEpisode);
    // The fourth idle beat while the same episode is open: capped.
    warn(p, 1300, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleD, 8, 1350, out));
    TEST_ASSERT_EQUAL_UINT32(3, p.diag.pressed);
}

// ── Clear stops immediately (no debounce) ────────────────────────────────

// Leaving the window (hands back to 0/1/2) closes the episode immediately —
// no 500 ms debounce (that existed only to paper over the wrong [2,7]
// window).
void test_clear_stops_immediately()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_TRUE(p.popRelease(out, 1050));
    // hands drops to 1: episode closes at once.
    warn(p, 1100, 1);
    TEST_ASSERT_FALSE(p.diag.warningActive);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
}

// The 1↔2 jitter (detector noise) must NOT re-open the episode — only a
// real prompt (hands >= 3) does.
void test_hands12_jitter_does_not_reopen_episode()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_TRUE(p.popRelease(out, 1050));
    // Warning clears, then jitters 1↔2 — no prompt, no re-fire.
    warn(p, 1100, 1);
    warn(p, 1150, 2);
    warn(p, 1200, 1);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1250, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
}

// A fresh prompt after a clear re-opens a NEW episode (budget reset).
void test_reentry_after_clear_reopens_episode()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_TRUE(p.popRelease(out, 1050));
    warn(p, 1100, 1); // clear
    warn(p, 2000, 3); // new prompt edge: new episode
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 2050, out));
    TEST_ASSERT_TRUE(p.popRelease(out, 2050));
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.pressed);
    TEST_ASSERT_EQUAL_UINT32(2, p.diag.warningEpisodes);
}

// ── AP active gate ───────────────────────────────────────────────────────

void test_ap_inactive_blocks_press()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    p.onDasStatus(1, 3, 1000); // AP state 1 (not active): no episode
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, p.diag.pressed);
    // AP activates with hands in the prompt window: episode opens.
    p.onDasStatus(6, 3, 1100);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1150, out));
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.pressed);
}

// ── Safety gates retained ────────────────────────────────────────────────

void test_own_echo_does_not_chain()
{
    DashLegacyWheel045Press p;
    arm(p);
    warn(p, 1000, 3);
    uint8_t press[8];
    uint8_t rel[8];
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, press));
    TEST_ASSERT_TRUE(p.popRelease(rel, 1050));
    p.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(DashLegacyWheel045Press::isPressFrame(press));
    TEST_ASSERT_FALSE(DashLegacyWheel045Press::isPressFrame(rel));
    // The echoed press frame comes back: recognised, not re-pressed.
    uint8_t out2[8];
    TEST_ASSERT_FALSE(p.onNative045(press, 8, 1250, out2));
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
    // a failed press never reached the bus: no release is owed
    TEST_ASSERT_FALSE(p.releasePending());
    // A fresh episode entry (clear + re-prompt) so a second press is armed.
    warn(p, 1100, 1); // clear
    warn(p, 1150, 3); // new prompt
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1200, out));
    p.recordTxResult(false, 1200);
    TEST_ASSERT_TRUE(p.diag.disarmed);
    // While disarmed, in-window beats do not press.
    warn(p, 1250, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1300, out));
    // 810 ms after disarm: healed; a fresh prompt presses again.
    warn(p, 1200 + 810, 3);
    TEST_ASSERT_FALSE(p.diag.disarmed);
    TEST_ASSERT_EQUAL_UINT32(1, p.diag.disarmHeals);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1200 + 850, out));
}

// A press whose TX failed never reached the bus, so the matching release
// must not be emitted on its own (an isolated idle frame is harmless, but a
// dangling release would desynchronise the click bookkeeping).
void test_failed_press_drops_pending_release()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_TRUE(p.releasePending());
    p.recordTxResult(false, 1050);
    TEST_ASSERT_FALSE(p.releasePending());
    uint8_t rel[8];
    TEST_ASSERT_FALSE(p.popRelease(rel, 1050));
}

// ── Yield: the click (press + release) owns the beat until the next fresh
// idle native frame after the RELEASE, not after the press ───────────────
void test_yield_holds_until_native_after_release()
{
    DashLegacyWheel045Press p;
    arm(p);
    uint8_t out[8];
    warn(p, 1000, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1060));
    TEST_ASSERT_TRUE(p.popRelease(out, 1050));
    // release just sent: still owning (the left DND must not pair now)
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1075));
    // a fresh idle native frame arrives: the engine may click again...
    warn(p, 1200, 3);
    TEST_ASSERT_TRUE(p.onNative045(kIdleD, 8, 1250, out)); // 2nd click: press (200 ms after the release)
    // ...but the button is held again, so the slot is still owned
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1260));
    TEST_ASSERT_TRUE(p.popRelease(out, 1250));
    TEST_ASSERT_TRUE(p.siblingOwnsBeat(1270));
    // first idle native frame AFTER this release frees the slot
    warn(p, 1450, 3);
    TEST_ASSERT_FALSE(p.onNative045(kIdleC, 8, 1500, out)); // budget spent (2/2)
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(1510));
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
    // A fresh NATIVE idle frame (after clear) releases the beat.
    warn(p, 2200, 1); // clear
    TEST_ASSERT_FALSE(p.onNative045(kIdleD, 8, 2300, out));
    TEST_ASSERT_FALSE(p.siblingOwnsBeat(2350));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_press_encodes_center_button_bit_and_counter_and_crc);
    RUN_TEST(test_press_is_followed_by_release_frame);
    RUN_TEST(test_second_press_waits_for_release_and_fresh_native);
    RUN_TEST(test_episode_caps_at_two_clicks);
    RUN_TEST(test_failed_press_drops_pending_release);
    RUN_TEST(test_yield_holds_until_native_after_release);
    RUN_TEST(test_diag_counts_released_frames_and_tracks_native_beat);
    RUN_TEST(test_blocked_reasons_are_distinct);
    RUN_TEST(test_hands2_does_not_trigger);
    RUN_TEST(test_hands3_prompt_triggers);
    RUN_TEST(test_prompt_ladder_triggers);
    RUN_TEST(test_clear_stops_immediately);
    RUN_TEST(test_hands12_jitter_does_not_reopen_episode);
    RUN_TEST(test_reentry_after_clear_reopens_episode);
    RUN_TEST(test_ap_inactive_blocks_press);
    RUN_TEST(test_own_echo_does_not_chain);
    RUN_TEST(test_action_frames_are_never_pressed);
    RUN_TEST(test_two_tx_failures_disarm_then_heal_800ms);
    RUN_TEST(test_disabled_by_default_no_activity);
    RUN_TEST(test_yield_gate_blocks_until_next_native_frame_observed);
    return UNITY_END();
}
