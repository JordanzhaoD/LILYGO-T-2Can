// dash_legacy_wheel_045_press.h — right-wheel PRESS DND (TX split v3.2).
//
// Field basis (2026-11-24, Jordan's on-road captures): physically pressing
// the RIGHT scroll wheel's LEFT side (side button B) clears a hands-on NAG.
// On the wire this is the switch-word (b4 | b5<<8) bit 5 = 0x0020, i.e.
// data[4] |= 0x20 (99 hits in the stationary 60 s capture). The left-wheel
// DND rides the same native 0x045 stream with a balanced +/- tick pair
// (bits 6..9); the press path is a PARALLEL sibling engine that rides the
// side-button press bit instead.
//
// v3.2 — REVERT the [2,7] level-frenzy to the field-stable EDGE + episode
// semantics. Field verdict on beta.08: gating on hands==2 (NOT_DETECTED)
// is WRONG. hands==2 is the persistent "hands not on wheel" baseline while
// AP is engaged — it is NOT a prompt. It sits in 2 for the entire drive and
// jitters 1<->2 at 9-50 ms (detector noise), not NAG transitions. Gating on
// it made the engine blast a press frame every idle beat for the WHOLE
// engagement (96.9 s uninterrupted in capture 221001, 853 frames), which
// the vehicle read as repeated-abuse of the side button and escalated to a
// strike-out (ap 6->8->9->1->14, steering-assist disabled in 221005).
//
// The correct semantics — the ones the left-wheel DND has held stable for
// six weeks — are: wait for the actual PROMPT. The prompt is the visual /
// chime ladder, hands==3 (VISUAL) upward. So:
//   - warning window is [3,6] ∪ [9,10] (identical to the left-wheel DND);
//   - EDGE trigger: only a warning ENTRY (was not warning, now warning)
//     starts an episode — a sustained hands==3 does NOT re-fire;
//   - episode-limited: at most kMaxPressesPerEpisode press frames per
//     episode, each riding the next fresh idle native beat, one beat apart;
//   - clear stops immediately (no debounce — the debounce existed only to
//     paper over the wrong [2,7] level window).
//
// Unlike the left-wheel tick (which shifts volume and needs a compensating
// -1), a press has no cumulative side effect, so there is no balancing
// pair — only a bounded number of presses per episode.
//
// Safety gates retained (same discipline as the left-wheel DND):
//   - idle-only riding (a frame carrying a wheel/stalk action payload is
//     never used as a press source);
//   - own-echo suppression (a press frame echoing back is not chained);
//   - sibling recognition + beat-generational yield with the left-wheel DND;
//   - TX-failure disarm self-heal (800 ms, 3-per-60 s).
// Default OFF, fresh NVS key rwp.

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
    uint32_t pressed = 0;
    uint32_t txFail = 0;
    uint32_t echoSuppressed = 0;
    uint32_t actionFrameSkipped = 0;
    uint32_t disarmHeals = 0;
    uint8_t consecutiveTxFailures = 0;
    const char *blockedReason = "disabled";
};

struct DashLegacyWheel045Press
{
    static constexpr uint32_t kCanId = 0x045;
    // The side-button (left-press) bit in the switch word (b4 | b5<<8):
    // bit 5 -> data[4] 0x20.
    static constexpr uint16_t kPressMask = 0x0020;
    // Any wheel/stalk action bits that make a frame non-idle (same mask the
    // left-wheel DND uses to reject action frames).
    static constexpr uint16_t kAnyWheelMask = 0x3FFF;
    // Episode budget: at most this many press frames per warning episode.
    // Mirrors the left-wheel DND's kMaxPairsPerEpisode shape — a bounded
    // intervention, never a sustained blast.
    static constexpr uint8_t kMaxPressesPerEpisode = 3;
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

    // Beat ownership for the left-wheel DND yield gate: the press frame owns
    // the beat until a FRESH idle native frame has been observed after it.
    bool siblingOwnsBeat(uint32_t /*nowMs*/) const
    {
        return lastPressMs_ != 0 && lastIdleNativeMs_ <= lastPressMs_;
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
        if (!episodeActive_ || !warningActive_ || !apActive_)
        {
            diag.blockedReason = "idle";
            return false;
        }
        if (pressesThisEpisode_ >= kMaxPressesPerEpisode)
        {
            // Episode budget exhausted: stop intervening for this warning.
            episodeActive_ = false;
            diag.blockedReason = "episode_limit";
            return false;
        }
        if (dlc < 8)
            return false;

        memcpy(out, data, 8);
        out[4] = static_cast<uint8_t>(out[4] | 0x20U); // side-button press bit (switch word bit5)
        out[6] = static_cast<uint8_t>(((out[6] & 0x0FU)) |
                                      ((((out[6] >> 4) + 1U) & 0x0FU) << 4));
        out[7] = crc8J1850(out, 7);
        lastPressMs_ = nowMs;
        pressesThisEpisode_++;
        diag.pressesThisEpisode = pressesThisEpisode_;
        diag.pressed++;
        diag.blockedReason = "armed";
        return true;
    }

    void recordTxResult(bool ok, uint32_t nowMs)
    {
        if (!ok)
        {
            diag.txFail++;
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
    uint32_t lastIdleNativeMs_ = 0;
    uint32_t disarmedAtMs_ = 0;
    uint8_t consecutiveTxFailures_ = 0;
    uint8_t healsThisWindow_ = 0;
    uint32_t healWindowStartMs_ = 0;
};
