// dash_legacy_wheel_045_press.h — right-wheel CENTER-press DND (v3.3).
//
// v3.3 — FIELD VERDICT ACROSS beta.05-09 (23 captures, 2026-11-23/24):
//
//   press signal        version   warning episodes   result
//   ------------------  --------  -----------------  ----------------------------
//   CENTER  0x0010      beta.07   6                  6/6 CLEARED (2 with a
//                                                     SINGLE frame, 9-26 ms)
//   SIDE    0x0020      beta.09   8                  6 not cleared (warning
//                                                     lasted 6-15 s), 2 were
//                                                     self-clearing blips
//   SIDE, hands=2 gate  beta.08   -                  96.9 s / 853-frame blast
//                                                     -> strike-out
//
// So the CENTER bit is the only field-verified press signal; the SIDE bit
// is retired. Both engines share one frame skeleton, so the difference is
// the button bit alone.
//
// The cost of a center press is a HEAD-UNIT side effect: the button is
// bound to a menu ("phone not connected" card), and it popped on EVERY
// press in the field. The head unit reads a lone press frame as a HELD
// button (the engine used to wait ~100 ms for the car's own next native
// frame to release it). A physical press is always followed by a released
// (idle) frame. v3.3 emits one too: each click = press frame + immediate
// RELEASE frame — the shortest click the bus can express. Whether that is
// short enough for the head unit not to open the menu is the open field
// question this build exists to answer.
//
// Trigger semantics are unchanged from v3.2 (field-stable, same as the
// left-wheel DND): the PROMPT ladder hands in [3,6] ∪ [9,10] only, EDGE
// entry opens an episode, clear stops at once. hands==2 is the "hands off
// wheel" baseline and never triggers.
//
// Cadence: one click per fresh idle native beat, the press and its release
// back to back, then the engine waits for the NEXT fresh idle native frame
// before it may click again. At most kMaxPressesPerEpisode clicks per
// warning episode (two: beta.07 cleared with a single frame twice, so the
// second is the retry, and two clicks stay far below the beta.08 blast).
//
// Unlike the left-wheel tick (which shifts volume and needs a compensating
// -1), a click has no cumulative side effect, so there is no balancing
// pair — only a bounded number of clicks per episode.
//
// Safety gates retained (same discipline as the left-wheel DND):
//   - idle-only riding (a frame carrying a wheel/stalk action payload is
//     never used as a press source);
//   - own-echo suppression (a press frame echoing back is not chained);
//   - sibling recognition + beat-generational yield with the left-wheel DND
//     (the yield now holds until the first idle native frame AFTER the
//     release);
//   - TX-failure disarm self-heal (800 ms, 3-per-60 s); a press whose TX
//     failed never reached the bus, so its release is dropped.
// Default OFF, NVS key rwp.

#pragma once

#include <stdint.h>
#include <string.h>

#include "can_frame_types.h"

struct DashLegacyWheel045PressDiag
{
    bool enabled = false;
    bool runtimeAllowed = false;
    bool disarmed = false;
    bool warningActive = false;
    bool apActive = false;
    uint8_t apState = 0;
    uint8_t handsState = 0;
    uint8_t pressesThisEpisode = 0;
    uint32_t warningEpisodes = 0;
    uint32_t pressed = 0;      // press frames the bus accepted
    uint32_t released = 0;     // release frames handed to the wiring
    uint32_t txFail = 0;
    uint32_t echoSuppressed = 0;
    uint32_t actionFrameSkipped = 0;
    uint32_t disarmHeals = 0;
    uint8_t consecutiveTxFailures = 0;
    // Status-grid observability (mirrors DashLegacyWheel045Diag): the last
    // native beat and the last 0x399 the engine acted on.
    bool native045Seen = false;
    uint32_t native045Ms = 0;
    uint8_t nativeCounter = 0;
    uint32_t dasStatusMs = 0;
    const char *sequenceState = "idle";
    const char *blockedReason = "disabled";
};

struct DashLegacyWheel045Press
{
    static constexpr uint32_t kCanId = 0x045;
    // The CENTER button bit in the switch word (b4 | b5<<8): bit 4 ->
    // data[4] 0x10. The only field-verified press signal (beta.07, 6/6).
    static constexpr uint16_t kPressMask = 0x0010;
    // Any wheel/stalk action bits that make a frame non-idle (same mask the
    // left-wheel DND uses to reject action frames).
    static constexpr uint16_t kAnyWheelMask = 0x3FFF;
    // Episode budget: at most this many CLICKS (press + release) per
    // warning episode. beta.07 cleared twice with a single frame; the
    // second click is the retry. A bounded intervention, never a blast.
    static constexpr uint8_t kMaxPressesPerEpisode = 2;
    // Minimum spacing between the end of one click (its release) and the
    // next press. Native 0x045 idles arrive ~100 ms apart on this vehicle,
    // so 150 ms guarantees the bus carried at least one released native
    // frame between two clicks — the head unit never sees a held button
    // across them. Time-based as well as beat-based: a burst of native
    // frames closer than that must not let two clicks stack.
    static constexpr uint32_t kMinClickSpacingMs = 150;
    // A new episode needs the warning to have been ABSENT for this long.
    // Without it every hands 2<->3 flicker (and every gate drop/re-raise
    // while hands stayed in the window) refilled the per-episode budget, and
    // the engine clicked at the spacing rate for as long as the flicker
    // lasted (review C1; density above the beta.08 blast).
    static constexpr uint32_t kWarningSettleMs = 1500;
    // TX-failure disarm self-heal (same shape as the left-wheel DND).
    static constexpr uint8_t kMaxConsecutiveTxFailures = 2;
    static constexpr uint32_t kHealDelayMs = 800;
    static constexpr uint8_t kMaxHealsPerWindow = 3;
    static constexpr uint32_t kHealWindowMs = 60000;

    DashLegacyWheel045PressDiag diag;

    static uint8_t crc8J1850(const uint8_t *data, uint8_t length)
    {
        uint8_t crc = 0xFF;
        for (uint8_t i = 0; i < length; ++i)
        {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; ++bit)
                crc = static_cast<uint8_t>((crc & 0x80) ? ((crc << 1) ^ 0x1D) : (crc << 1));
        }
        return static_cast<uint8_t>(crc ^ 0xFF);
    }

    // Switches are PLAIN FLAGS (review I4): these two are called from the
    // web task (config saves), while the engine itself runs in the CAN task.
    // They never touch episode state here — the next onDasStatus() /
    // onNative045() in the CAN task applies them.
    void configure(bool enabled)
    {
        enabled_ = enabled;
        diag.enabled = enabled;
        if (!enabled)
            endEpisode("disabled");
    }

    void setRuntimeAllowed(bool allowed)
    {
        runtimeAllowed_ = allowed;
        diag.runtimeAllowed = allowed;
        // Losing the permission ends the action state (review I4/I5): a
        // release still owed is dropped — the car's own next native frame
        // releases the button — so nothing stays pending that would let the
        // engine re-own the beat (and starve the left-wheel DND) once the
        // permission comes back.
        if (!allowed)
            endEpisode("runtime");
    }

    // True when the engine is permitted to emit right now. The CAN-task tick
    // owns the full gate chain; this lets a caller that only holds the
    // engine see whether a click could go out.
    bool acting() const { return canAct(); }

    // A press frame is identified by the center-button press bit (switch
    // word bit 4).
    static bool isPressFrame(const uint8_t data[8])
    {
        const uint16_t wheel = static_cast<uint16_t>(data[4]) |
                               static_cast<uint16_t>(static_cast<uint16_t>(data[5]) << 8);
        return (wheel & kPressMask) != 0;
    }

    // True when the frame carries no wheel/stalk action payload — the same
    // switch-word shape the left-wheel DND requires of a source frame.
    static bool isIdleActionFrame(const uint8_t data[8])
    {
        const uint16_t wheel = static_cast<uint16_t>(data[4]) |
                               static_cast<uint16_t>(static_cast<uint16_t>(data[5]) << 8);
        return (wheel & kAnyWheelMask) == 0;
    }

    // Beat ownership for the left-wheel DND yield gate. The click owns the
    // beat from its press frame until the first idle native frame AFTER its
    // release. While the engine is not permitted to act it owns nothing —
    // otherwise a gate dropping right after a click would leave the left
    // DND yielding forever (review I5: it starved until reboot).
    bool siblingOwnsBeat(uint32_t /*nowMs*/) const
    {
        if (!canAct())
            return false;
        if (releasePending_)
            return true;
        return lastClickEndMs_ != 0 && lastIdleNativeMs_ <= lastClickEndMs_;
    }

    // True between a press frame and the native beat that carries its
    // release.
    bool releasePending() const { return releasePending_; }

    // ── 0x399: observe unconditionally, act only when allowed ────────────
    void onDasStatus(uint8_t apState, uint8_t handsState, uint32_t nowMs)
    {
        healTick(nowMs);

        // Observation (always): the status grid and the settle timer must
        // reflect the bus even while the engine is off or blocked (review I2).
        apActive_ = apState >= 3 && apState <= 6;
        warningActive_ = isWarningState(handsState);
        diag.apState = apState;
        diag.handsState = handsState;
        diag.apActive = apActive_;
        diag.warningActive = warningActive_;
        diag.dasStatusMs = nowMs;
        // Sample the PREVIOUS warning time before overwriting it: "how long
        // had the warning been absent before this frame" is what the settle
        // rule needs. (Updating first and then subtracting would always give
        // 0 — the engine could then never open an episode.)
        const uint32_t warningAbsentForMs =
            warningActive_ ? (lastWarningMs_ == 0 ? 0xFFFFFFFFu : nowMs - lastWarningMs_)
                           : 0;
        if (warningActive_)
            lastWarningMs_ = nowMs;

        // Episode rule (one rule, reviewed C1): an episode opens when the
        // warning is present, no episode is open, and no warning has been
        // seen for kWarningSettleMs. A flicker — hands dropping out and
        // back inside the settle time — never earns a new budget, whichever
        // side moved: hands, a gate, or the user switch.
        // Rule for the readable reason (review I2): the 0x399 path owns it
        // whenever the WARNING state is not "present with an open episode":
        //   no AP            -> ap_inactive
        //   no warning       -> warning_clear if an episode just ended, else idle
        //   warning, blocked -> disabled / disarmed / runtime
        //   warning, settling-> settling
        //   warning, ready   -> waiting_native
        // The 0x045 path only refines it INSIDE an open episode (spacing,
        // release_pending, episode_limit, armed) and never touches it
        // otherwise, so none of the values above is overwritten one beat later.
        if (!apActive_)
        {
            endEpisode("ap_inactive");
            diag.blockedReason = "ap_inactive";
        }
        else if (!warningActive_)
        {
            const bool wasOpen = episodeActive_;
            endEpisode(wasOpen ? "warning_clear" : "idle");
            diag.blockedReason = wasOpen ? "warning_clear" : "idle";
        }
        else if (!episodeActive_)
        {
            const bool settled = lastWarningMs_ == 0 ||
                                 warningAbsentForMs >= kWarningSettleMs;
            if (!canAct())
                diag.blockedReason = !enabled_ ? "disabled" : (disarmed_ ? "disarmed" : "runtime");
            else if (settled)
                openEpisode();
            else
                diag.blockedReason = "settling";
        }
    }

    // Observe a native beat without ever emitting on it (the CH-bus view,
    // or any path that must not transmit). Keeps "have I seen a native frame
    // after my release" true to the bus on both buses.
    void observeNative045(const uint8_t data[8], uint8_t dlc, uint32_t nowMs)
    {
        if (dlc < 8 || isPressFrame(data) || !isIdleActionFrame(data))
            return;
        lastIdleNativeMs_ = nowMs;
        diag.native045Seen = true;
        diag.native045Ms = nowMs;
        diag.nativeCounter = static_cast<uint8_t>((data[6] >> 4) & 0x0F);
    }

    // ── 0x045: the click itself ──────────────────────────────────────────
    // PRESS: returns true and fills `out` when a press frame should go out
    // for this native frame. The release does NOT go out with it any more:
    // it rides the NEXT idle native frame (review M6 — an immediate c+2
    // release put a counter regression on the bus: c, c+1, c+2, c+1).
    bool onNative045(const uint8_t data[8], uint8_t dlc, uint32_t nowMs, uint8_t out[8])
    {
        if (isPressFrame(data))
        {
            // A press frame (ours echoing back, or the driver really pressing
            // the center button): never chain, never a fresh native beat.
            diag.echoSuppressed++;
            return false;
        }
        if (!isIdleActionFrame(data))
        {
            // A frame carrying a wheel/stalk action payload is never a source.
            diag.actionFrameSkipped++;
            return false;
        }
        const bool fresh = lastIdleNativeMs_ != nowMs;
        if (fresh)
        {
            lastIdleNativeMs_ = nowMs;
            diag.native045Seen = true;
            diag.native045Ms = nowMs;
            diag.nativeCounter = static_cast<uint8_t>((data[6] >> 4) & 0x0F);
        }

        if (!canAct() || dlc < 8)
            return false; // reason already set by the 0x399 path

        // RELEASE first: a click is press frame, then the next native beat
        // carries the released frame. The button is held for exactly one
        // native interval (~100 ms on this vehicle).
        if (releasePending_ && fresh)
        {
            memcpy(out, data, 8);
            out[6] = static_cast<uint8_t>((out[6] & 0x0FU) |
                                          ((((out[6] >> 4) + 1U) & 0x0FU) << 4));
            out[7] = crc8J1850(out, 7);
            releasePending_ = false;
            releaseConfirmed_ = true;      // handed to the wiring
            lastClickEndMs_ = nowMs;
            return true;
        }

        if (!episodeActive_ || !warningActive_ || !apActive_)
        {
            // No open episode: the 0x399 path owns the reason (see there).
            // Writing "idle" here used to erase warning_clear / settling one
            // beat after they were recorded.
            return false;
        }
        if (episodeExhausted_)
        {
            diag.blockedReason = "episode_limit"; // sticky until the episode ends
            return false;
        }
        if (releasePending_)
        {
            diag.blockedReason = "release_pending";
            return false;
        }
        if (lastClickEndMs_ != 0 && nowMs - lastClickEndMs_ < kMinClickSpacingMs)
        {
            diag.blockedReason = "spacing";
            return false;
        }

        // PRESS frame: the native idle frame with the center button bit set,
        // counter +1, CRC recomputed.
        memcpy(out, data, 8);
        out[4] = static_cast<uint8_t>(out[4] | 0x10U); // center button (switch word bit4)
        out[6] = static_cast<uint8_t>((out[6] & 0x0FU) |
                                      ((((out[6] >> 4) + 1U) & 0x0FU) << 4));
        out[7] = crc8J1850(out, 7);
        releasePending_ = true;
        pressesThisEpisode_++;
        diag.pressesThisEpisode = pressesThisEpisode_;
        diag.blockedReason = "armed";
        if (pressesThisEpisode_ >= kMaxPressesPerEpisode)
        {
            // Budget spent: the episode stays open but exhausted, so a
            // flicker cannot reopen it; it ends when the warning clears.
            episodeExhausted_ = true;
            diag.sequenceState = "pressed_last";
        }
        else
        {
            diag.sequenceState = "pressed";
        }
        return true;
    }

    // Which frame a true return from onNative045 delivered. The wiring calls
    // recordTxResult() for either; it matters for the counters.
    bool lastWasRelease() const { return releaseConfirmed_; }

    void recordTxResult(bool ok, uint32_t nowMs)
    {
        const bool wasRelease = releaseConfirmed_;
        releaseConfirmed_ = false;
        if (ok)
        {
            consecutiveTxFailures_ = 0;
            diag.consecutiveTxFailures = 0;
            if (wasRelease)
                diag.released++; // only once the bus accepted it (review I3)
            else
                diag.pressed++;
            return;
        }
        diag.txFail++;
        if (wasRelease)
        {
            // The button is left HELD on the bus. Re-arm the release so the
            // next idle native beat retries it, and no new press may stack.
            releasePending_ = true;
        }
        else
        {
            // The press never reached the bus: nothing is held, no release.
            releasePending_ = false;
            pressesThisEpisode_--; // the click did not happen: give it back
            diag.pressesThisEpisode = pressesThisEpisode_;
        }
        if (consecutiveTxFailures_ < 0xFF)
            consecutiveTxFailures_++;
        diag.consecutiveTxFailures = consecutiveTxFailures_;
        if (consecutiveTxFailures_ >= kMaxConsecutiveTxFailures)
        {
            disarmed_ = true;
            diag.disarmed = true;
            disarmedAtMs_ = nowMs;
            endEpisode("disarmed");
        }
    }

    void healTick(uint32_t nowMs)
    {
        if (!disarmed_)
            return;
        if (healsThisWindow_ > 0 && nowMs - healWindowStartMs_ >= kHealWindowMs)
            healsThisWindow_ = 0;
        if (healsThisWindow_ >= kMaxHealsPerWindow)
            return;
        if (disarmedAtMs_ == 0 || nowMs - disarmedAtMs_ < kHealDelayMs)
            return;
        disarmed_ = false;
        diag.disarmed = false;
        consecutiveTxFailures_ = 0;
        diag.consecutiveTxFailures = 0;
        if (healsThisWindow_ == 0)
            healWindowStartMs_ = nowMs;
        healsThisWindow_++;
        diag.disarmHeals++;
    }

private:
    static bool isWarningState(uint8_t handsState)
    {
        // The PROMPT ladder — identical to the field-stable left-wheel DND.
        // hands==2 (NOT_DETECTED) is the persistent "hands off wheel"
        // baseline, NOT a prompt (the beta.08 [2,7] regression). The prompt
        // starts at hands==3 (VISUAL); 9/10 are the escalated domain.
        return (handsState >= 3 && handsState <= 6) ||
               (handsState >= 9 && handsState <= 10);
    }

    bool canAct() const { return enabled_ && runtimeAllowed_ && !disarmed_; }

    void openEpisode()
    {
        episodeActive_ = true;
        episodeExhausted_ = false;
        pressesThisEpisode_ = 0;
        diag.pressesThisEpisode = 0;
        diag.warningEpisodes++;
        diag.sequenceState = "armed";
        diag.blockedReason = "waiting_native";
    }

    // Closes the episode and forgets the action state. The OBSERVATION
    // (apActive_, warningActive_, lastWarningMs_) is deliberately kept: it
    // belongs to the bus, not to the engine's permission to act.
    void endEpisode(const char *why)
    {
        const bool wasOpen = episodeActive_;
        episodeActive_ = false;
        episodeExhausted_ = false;
        pressesThisEpisode_ = 0;
        diag.pressesThisEpisode = 0;
        releasePending_ = false;
        releaseConfirmed_ = false;
        if (wasOpen)
        {
            diag.sequenceState = "idle";
            diag.blockedReason = why;
        }
    }

    bool enabled_ = false;
    bool runtimeAllowed_ = false;
    bool disarmed_ = false;
    bool warningActive_ = false;
    bool apActive_ = false;
    bool episodeActive_ = false;
    bool episodeExhausted_ = false;
    uint8_t pressesThisEpisode_ = 0;
    // A press frame went out; the next idle native beat owes the release.
    bool releasePending_ = false;
    // Last frame the engine handed to the wiring was a release.
    bool releaseConfirmed_ = false;
    uint32_t lastClickEndMs_ = 0;
    uint32_t lastIdleNativeMs_ = 0;
    // Last 0x399 that carried a warning; 0 = none seen since boot.
    uint32_t lastWarningMs_ = 0;
    uint32_t disarmedAtMs_ = 0;
    uint8_t consecutiveTxFailures_ = 0;
    uint8_t healsThisWindow_ = 0;
    uint32_t healWindowStartMs_ = 0;
};
