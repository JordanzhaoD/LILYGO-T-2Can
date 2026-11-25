// ─────────────────────────────────────────────────────────────────────────────
// FROZEN — Jordan, 2026-11-26. Do not change this engine's behaviour.
//
// Field-verified on 6.1.0-beta.12 (75-minute drive, FSD injection OFF: 21
// warning rounds, 126 press frames, 0 TX failures): this right-wheel held
// press and the left-wheel DND (dash_legacy_wheel_045_dnd.h) both ran
// stable. The same freeze covers their runtime gates in src/main.cpp
// (t2canRightWheelPressTick / t2canWheelDndTick) and the TX split that lets
// them run with the FSD master switch off. Jordan's verdict: "目前基本完美".
//
// Known and accepted: the head-unit center-button menu pops on a press and
// goes away by itself after 3-4 s. Every round uses the full 6-frame cap;
// if a nag ever fails to clear, the cap (not the trigger logic) is the first
// thing to revisit — with Jordan's OK.
//
// Changing anything here (behaviour, timing constants, gates) needs Jordan
// to lift the freeze explicitly. A bug fix is still a change: report it,
// do not apply it.
// ─────────────────────────────────────────────────────────────────────────────

// dash_legacy_wheel_045_press.h — right-wheel CENTER-press DND (v3.4).
//
// v3.4 (6.1.0-beta.12, 2026-11-26) — BOUNDED LEVEL PRESS.
//
//   The field matrix for the center button (switch word bit4 = data[4] 0x10):
//
//     shape                      version   persistent nags   result
//     -------------------------  --------  ----------------  ----------------------
//     LEVEL, a press frame per   beta.07   3                 3/3 cleared
//       idle native beat while
//       the warning lasts
//     click: press + release,     beta.11   3                 0/3 (5-8 s, the
//       one per beat                                         natural length)
//     LEVEL gated on hands=2      beta.08   -                 853-frame blast
//                                                                 -> strike-out
//
//   What the vehicle accepts is a HELD button, not a tap: beta.07 cleared
//   every nag by pressing behind every idle native beat until hands dropped
//   (4-8 frames, 0.4-0.7 s). beta.11 shortened that to a ~100 ms tap to stop
//   the head-unit menu from popping, and the vehicle stopped reacting. v3.4
//   restores the held-button shape and bounds it: at most
//   kMaxPressesPerEpisode frames per warning episode (beta.07 had no bound,
//   and an unbounded level press is the beta.08 blast).
//
//   No release frame is emitted. The car's own next native idle 0x045
//   reports the button released, exactly as in beta.07, so the bus counter
//   only ever repeats (native c, press c+1, native c+1) and never regresses.
//
//   Known cost, unchanged since beta.07: every press pops the head-unit
//   center-button menu ("phone not connected" card). Not solved here.
//
// Trigger semantics (unchanged, field-stable, same as the left-wheel DND):
//   - the PROMPT ladder only: hands in [3,6] U [9,10]. hands==2
//     (NOT_DETECTED) is the persistent "hands off wheel" baseline and never
//     triggers (the beta.08 regression);
//   - a new episode needs the warning to have been ABSENT for
//     kWarningSettleMs, so a hands 2<->3 flicker, or any gate dropping and
//     re-raising while hands stayed in the window, never earns a new budget;
//   - clear stops on the next frame.
//   The engine OBSERVES the bus unconditionally and gates only the acting, so
//   the status grid shows the real warning even while blocked.
//
// Coordination with the left-wheel DND (same CAN id, same counter):
//   the press engine owns a beat ONLY while a press frame it emitted is the
//   newest frame on it; the next idle native frame frees it. Losing the
//   permission ends the action state, so a gate dropping right after a press
//   cannot wedge the left-wheel DND.
//
// Safety gates (same discipline as the left-wheel DND):
//   - idle-only riding; own-echo suppression; TX-failure disarm with
//     self-heal (800 ms, 3-per-60 s); runtime permission owned solely by the
//     CAN-task tick.
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
    // Episode budget: at most this many PRESS frames per warning episode.
    // The press is LEVEL-shaped (one frame behind every idle native beat
    // while the warning lasts — the beta.07 shape that cleared 6/6 nags in
    // the field). beta.07 had no bound, and that shape gated on hands=2 is
    // the beta.08 853-frame blast, so it is capped here. beta.07's slowest
    // clear took 8 frames (0.7 s); 6 frames (~0.6 s at the ~100 ms native
    // cadence) clears what cleared before while staying an order of
    // magnitude below the blast.
    static constexpr uint8_t kMaxPressesPerEpisode = 6;
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
    // engine see whether a press could go out.
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

    // Beat ownership for the left-wheel DND yield gate. The press engine
    // owns a beat ONLY while a press frame it emitted is the newest thing on
    // that beat: the next idle native frame clears it (that frame reports
    // the button released, so the two engines never share a beat and never
    // leave the button held by the wrong one). While the engine is not
    // permitted to act it owns nothing — otherwise a gate dropping right
    // after a press would leave the left DND yielding forever (review I5:
    // it starved until reboot).
    bool siblingOwnsBeat(uint32_t /*nowMs*/) const
    {
        if (!canAct())
            return false;
        return pressInFlight_;
    }

    // True while a press frame of ours is the newest frame on the bus.
    bool pressInFlight() const { return pressInFlight_; }

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
        //   warning, flicker  -> warn_flicker
        //   warning, ready   -> waiting_native
        // The 0x045 path only refines it INSIDE an open episode (spacing,
        // episode_limit, armed) and never touches it
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
                diag.blockedReason = "warn_flicker";
        }
    }

    // Observe a native beat without ever emitting on it (the CH-bus view,
    // or any path that must not transmit). Keeps "have I seen a native frame
    // after my press" true to the bus on both buses.
    void observeNative045(const uint8_t data[8], uint8_t dlc, uint32_t nowMs)
    {
        if (dlc < 8 || isPressFrame(data) || !isIdleActionFrame(data))
            return;
        lastIdleNativeMs_ = nowMs;
        diag.native045Seen = true;
        diag.native045Ms = nowMs;
        diag.nativeCounter = static_cast<uint8_t>((data[6] >> 4) & 0x0F);
        pressInFlight_ = false;
    }

    // ── 0x045: the press itself ──────────────────────────────────────────
    // PRESS: returns true and fills `out` when a press frame should go out
    // for this native frame (level press: every idle beat while the
    // warning lasts). No release frame is sent (review M6 — a c+2 release
    // put a counter regression on the bus: c, c+1, c+2, c+1).
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
            // A new idle native beat: the car has reported the button
            // released, so the previous press no longer owns the beat.
            pressInFlight_ = false;
        }

        if (!canAct() || dlc < 8)
            return false; // reason already set by the 0x399 path

        if (!episodeActive_ || !warningActive_ || !apActive_)
        {
            // No open episode: the 0x399 path owns the reason (see there).
            // Writing "idle" here used to erase warning_clear / warn_flicker one
            // beat after they were recorded.
            return false;
        }
        if (episodeExhausted_)
        {
            diag.blockedReason = "episode_limit"; // sticky until the episode ends
            return false;
        }

        // PRESS frame behind this idle native beat (beta.07 shape): the
        // native frame with the center button bit set, counter +1, CRC
        // recomputed. No release frame and no click spacing — the vehicle
        // clears a nag for a HELD button, not for a tap (field: beta.07
        // level 6/6, beta.11 click 0/3 on persistent nags). The car's own
        // next native idle frame is what reports the button released.
        memcpy(out, data, 8);
        out[4] = static_cast<uint8_t>(out[4] | 0x10U); // center button (switch word bit4)
        out[6] = static_cast<uint8_t>((out[6] & 0x0FU) |
                                      ((((out[6] >> 4) + 1U) & 0x0FU) << 4));
        out[7] = crc8J1850(out, 7);
        pressInFlight_ = true;
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

    void recordTxResult(bool ok, uint32_t nowMs)
    {
        if (ok)
        {
            consecutiveTxFailures_ = 0;
            diag.consecutiveTxFailures = 0;
            diag.pressed++; // only once the bus accepted it (review I3)
            return;
        }
        diag.txFail++;
        // A failed press never reached the bus: nothing is held down.
        pressesThisEpisode_--; // the frame did not happen: give it back
        diag.pressesThisEpisode = pressesThisEpisode_;
        if (episodeExhausted_ && pressesThisEpisode_ < kMaxPressesPerEpisode)
            episodeExhausted_ = false;
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
        pressInFlight_ = false;
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
    // A press frame of ours is the newest frame on the bus.
    bool pressInFlight_ = false;
    uint32_t lastIdleNativeMs_ = 0;
    // Last 0x399 that carried a warning; 0 = none seen since boot.
    uint32_t lastWarningMs_ = 0;
    uint32_t disarmedAtMs_ = 0;
    uint8_t consecutiveTxFailures_ = 0;
    uint8_t healsThisWindow_ = 0;
    uint32_t healWindowStartMs_ = 0;
};
