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
    // A press frame is owned by the engine for one beat after it is sent;
    // the left-wheel DND yields its slot this long (beat-generational, the
    // actual release is the next fresh idle native frame — see
    // siblingOwnsBeat).
    static constexpr uint32_t kCloneInFlightMs = 20;
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

    void configure(bool enabled)
    {
        enabled_ = enabled;
        diag.enabled = enabled;
        if (!enabled)
        {
            resetEpisode();
            diag.blockedReason = "disabled";
        }
    }

    void setRuntimeAllowed(bool allowed)
    {
        runtimeAllowed_ = allowed;
        diag.runtimeAllowed = allowed;
        if (!allowed)
            resetEpisode();
    }

    // A press frame is identified by the side-button press bit (switch word
    // bit 5).
    static bool isPressFrame(const uint8_t data[8])
    {
        const uint16_t wheel = static_cast<uint16_t>(data[4]) |
                               static_cast<uint16_t>(static_cast<uint16_t>(data[5]) << 8);
        return (wheel & kPressMask) != 0;
    }

    // True when the frame carries no wheel/stalk action payload other than
    // possibly our own press bit — the same switch-word shape the left-wheel
    // DND requires before it will use a frame as source.
    static bool isIdleActionFrame(const uint8_t data[8])
    {
        const uint16_t wheel = static_cast<uint16_t>(data[4]) |
                               static_cast<uint16_t>(static_cast<uint16_t>(data[5]) << 8);
        return (wheel & kAnyWheelMask) == 0;
    }

    bool cloneInFlight(uint32_t nowMs) const
    {
        return lastPressMs_ != 0 && nowMs - lastPressMs_ < kCloneInFlightMs;
    }

    // Beat ownership for the left-wheel DND yield gate: the click owns the
    // beat until a FRESH idle native frame has been observed after its last
    // frame. While a release is still owed the click is not finished, so
    // the slot is owned regardless of native traffic.
    bool siblingOwnsBeat(uint32_t /*nowMs*/) const
    {
        if (releasePending_)
            return true;
        return lastClickEndMs_ != 0 && lastIdleNativeMs_ <= lastClickEndMs_;
    }

    // True between a press frame and the moment its release is popped.
    bool releasePending() const { return releasePending_; }

    // Hands the owed release frame to the caller (the wiring sends it right
    // after the press frame). Returns false when nothing is owed.
    bool popRelease(uint8_t out[8], uint32_t nowMs)
    {
        if (!releasePending_)
            return false;
        memcpy(out, releaseFrame_, 8);
        releasePending_ = false;
        lastClickEndMs_ = nowMs;
        diag.released++;
        return true;
    }

    // EDGE trigger, identical to the left-wheel DND's onDasStatus episode
    // entry: a warning ENTRY (was not warning -> now warning) opens an
    // episode; staying inside the warning ladder does NOT re-open it. Leaving
    // the window (or AP out, or engine down) closes the episode immediately.
    void onDasStatus(uint8_t apState, uint8_t handsState, uint32_t nowMs)
    {
        healTick(nowMs);
        const bool wasWarning = warningActive_;
        apActive_ = apState >= 3 && apState <= 6;
        warningActive_ = isWarningState(handsState);
        diag.apState = apState;
        diag.handsState = handsState;
        diag.apActive = apActive_;
        diag.warningActive = warningActive_;
        diag.dasStatusMs = nowMs;

        if (!enabled_ || !runtimeAllowed_ || disarmed_ || !apActive_)
        {
            resetEpisode();
            return;
        }

        if (warningActive_ && !wasWarning)
        {
            // Fresh warning edge: open the episode, reset the press budget,
            // and arm for the next idle native beat.
            episodeActive_ = true;
            pressesThisEpisode_ = 0;
            diag.pressesThisEpisode = 0;
            diag.warningEpisodes++;
            diag.blockedReason = "waiting_native";
            diag.sequenceState = "armed";
        }
        else if (!warningActive_ && wasWarning)
        {
            // Warning cleared (hands back to 0/1/2, or AP out): close it.
            resetEpisode();
            diag.blockedReason = "warning_clear";
        }
    }

    // Returns true and fills `out` when a press frame should be transmitted
    // for this native frame. EDGE + episode-limited: at most
    // kMaxPressesPerEpisode presses, each riding a fresh idle native beat.
    bool onNative045(const uint8_t data[8], uint8_t dlc, uint32_t nowMs, uint8_t out[8])
    {
        if (!enabled_)
        {
            diag.blockedReason = "disabled";
            return false;
        }
        if (!runtimeAllowed_)
        {
            diag.blockedReason = "runtime";
            return false;
        }
        if (disarmed_)
        {
            diag.blockedReason = "disarmed";
            return false;
        }
        if (isPressFrame(data))
        {
            // Our own echo (or a sibling's press frame): never chain and
            // never count as a fresh native beat.
            diag.echoSuppressed++;
            return false;
        }
        if (!isIdleActionFrame(data))
        {
            // Left-wheel DND tick echoes / jitter stalk frames carry a
            // functional payload — pressing those would re-send another
            // engine's action with the press bit stitched on.
            diag.actionFrameSkipped++;
            return false;
        }
        // A fresh idle native beat: this is what releases the yield slot.
        lastIdleNativeMs_ = nowMs;
        diag.native045Seen = true;
        diag.native045Ms = nowMs;
        diag.nativeCounter = static_cast<uint8_t>((data[6] >> 4) & 0x0F);
        if (!episodeActive_ || !warningActive_ || !apActive_)
        {
            diag.blockedReason = "idle";
            return false;
        }
        if (dlc < 8)
            return false;
        if (releasePending_)
        {
            // The previous press has not been released yet: never stack a
            // second press on a held button.
            diag.blockedReason = "release_pending";
            return false;
        }
        if (pressesThisEpisode_ >= kMaxPressesPerEpisode)
        {
            // Episode budget exhausted: stop intervening for this warning.
            episodeActive_ = false;
            diag.blockedReason = "episode_limit";
            diag.sequenceState = "episode_limit";
            return false;
        }
        if (lastClickEndMs_ != 0 &&
            (lastIdleNativeMs_ <= lastClickEndMs_ || nowMs - lastClickEndMs_ < kMinClickSpacingMs))
        {
            // Spacing: the click before this one has not yet been followed by
            // a fresh idle native frame, or not enough time has passed.
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

        // RELEASE frame, owed immediately after: the same idle frame, no
        // button bits, counter +1 again, CRC recomputed. This is what a
        // physical click looks like on the wire.
        memcpy(releaseFrame_, data, 8);
        releaseFrame_[6] = static_cast<uint8_t>((releaseFrame_[6] & 0x0FU) |
                                                ((((releaseFrame_[6] >> 4) + 2U) & 0x0FU) << 4));
        releaseFrame_[7] = crc8J1850(releaseFrame_, 7);
        releasePending_ = true;

        lastPressMs_ = nowMs;
        pressesThisEpisode_++;
        diag.pressesThisEpisode = pressesThisEpisode_;
        diag.pressed++;
        diag.blockedReason = "armed";
        diag.sequenceState = pressesThisEpisode_ >= kMaxPressesPerEpisode
                                 ? "pressed_last"
                                 : "pressed";
        return true;
    }

    void recordTxResult(bool ok, uint32_t nowMs)
    {
        if (!ok)
        {
            diag.txFail++;
            // A press that failed never reached the bus: nothing is held
            // down, so its release must not go out on its own.
            releasePending_ = false;
            if (consecutiveTxFailures_ < 0xFF)
                consecutiveTxFailures_++;
            diag.consecutiveTxFailures = consecutiveTxFailures_;
            if (consecutiveTxFailures_ >= kMaxConsecutiveTxFailures)
            {
                disarmed_ = true;
                diag.disarmed = true;
                disarmedAtMs_ = nowMs;
                resetEpisode();
                diag.blockedReason = "disarmed";
            }
            return;
        }
        consecutiveTxFailures_ = 0;
        diag.consecutiveTxFailures = 0;
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
        // baseline, NOT a prompt: it must never trigger an intervention
        // (the beta.08 [2,7] regression). The prompt starts at hands==3
        // (VISUAL) and escalates 4/5 (CHIME) to 7 (STRUCK_OUT). 9/10 are
        // the escalated warning domain the left-wheel DND also covers.
        return (handsState >= 3 && handsState <= 6) ||
               (handsState >= 9 && handsState <= 10);
    }

    void resetEpisode()
    {
        episodeActive_ = false;
        warningActive_ = false;
        apActive_ = false;
        pressesThisEpisode_ = 0;
        // The episode is over (or the engine went down): a release that was
        // still owed is dropped — the press it belonged to may not have
        // been sent either, and the caller must never emit a lone release.
        releasePending_ = false;
        diag.sequenceState = "idle";
        diag.warningActive = false;
        diag.apActive = false;
        diag.pressesThisEpisode = 0;
    }

    bool enabled_ = false;
    bool runtimeAllowed_ = false;
    bool disarmed_ = false;
    bool warningActive_ = false;
    bool apActive_ = false;
    bool episodeActive_ = false;
    uint8_t pressesThisEpisode_ = 0;
    uint32_t lastPressMs_ = 0;
    // Timestamp of the click's last frame (press or release, whichever was
    // sent last); the next click waits for an idle native frame after it.
    uint32_t lastClickEndMs_ = 0;
    uint32_t lastIdleNativeMs_ = 0;
    bool releasePending_ = false;
    uint8_t releaseFrame_[8] = {};
    uint32_t disarmedAtMs_ = 0;
    uint8_t consecutiveTxFailures_ = 0;
    uint8_t healsThisWindow_ = 0;
    uint32_t healWindowStartMs_ = 0;
};
