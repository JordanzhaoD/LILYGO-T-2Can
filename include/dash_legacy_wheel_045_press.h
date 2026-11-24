// dash_legacy_wheel_045_press.h — right-wheel PRESS DND (TX split v3).
//
// Field basis (2026-11-24, Jordan's on-road captures): physically pressing
// the RIGHT scroll wheel clears a hands-on NAG. On the wire this is the
// switch-word (b4 | b5<<8) bit 4 = 0x0010, i.e. data[4] |= 0x10. The left
// wheel DND already rides this same native 0x045 stream with a balanced
// +/- tick pair (bits 6..9); the press path is a PARALLEL sibling engine
// that rides the press bit instead.
//
// Semantics (LEVEL, not edge): while a warning is active (hands ∈ [3,6] or
// [9,10], the same window as the left-wheel DND), every native idle 0x045
// that arrives is cloned 1:1 with the press bit set, counter+1, recomputed
// CRC8-J1850, and re-sent in-line one beat behind the native frame. When the
// warning clears (hands leaves the window, or AP drops out), the engine
// stops immediately — no trigger to consume, no expiry, no free-run.
//
// Unlike the left-wheel tick (which shifts volume and therefore needs a
// compensating -1), a press has no cumulative side effect: a single
// sustained press is volume-neutral by nature, so there is no balancing
// pair. The "hold until clear" shape is RX-synchronized — identical to the
// left-wheel DND's ride-the-native-stream architecture and the 0x3EE
// offsetOnly speed writer, both field-verified.
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
    // The press bit in the switch word (b4 | b5<<8): bit 4 -> data[4] 0x10.
    static constexpr uint16_t kPressMask = 0x0010;
    // Any wheel/stalk action bits that make a frame non-idle (same mask the
    // left-wheel DND uses to reject action frames).
    static constexpr uint16_t kAnyWheelMask = 0x3FFF;
    // TX-failure disarm self-heal (same shape as the left-wheel DND).
    static constexpr uint8_t kMaxConsecutiveTxFailures = 2;
    static constexpr uint32_t kHealDelayMs = 800;
    static constexpr uint8_t kMaxHealsPerWindow = 3;
    static constexpr uint32_t kHealWindowMs = 60000;

    DashLegacyWheel045PressDiag diag;

    void configure(bool enabled)
    {
        enabled_ = enabled;
        diag.enabled = enabled;
        if (!enabled)
        {
            warningActive_ = false;
            apActive_ = false;
            diag.warningActive = false;
            diag.apActive = false;
            diag.blockedReason = "disabled";
        }
    }

    void setRuntimeAllowed(bool allowed)
    {
        runtimeAllowed_ = allowed;
        diag.runtimeAllowed = allowed;
        if (!allowed)
        {
            warningActive_ = false;
            diag.warningActive = false;
        }
    }

    // A press frame is identified by the press bit (switch word bit 4).
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

    void onDasStatus(uint8_t apState, uint8_t handsState, uint32_t nowMs)
    {
        healTick(nowMs);
        diag.apState = apState;
        diag.handsState = handsState;
        apActive_ = apState >= 3 && apState <= 6;
        diag.apActive = apActive_;
        const bool wasWarning = warningActive_;
        warningActive_ = isWarningState(handsState) && apActive_;
        diag.warningActive = warningActive_;
        if (!enabled_ || !runtimeAllowed_ || disarmed_)
        {
            warningActive_ = false;
            diag.warningActive = false;
            return;
        }
        if (!apActive_)
        {
            warningActive_ = false;
            diag.warningActive = false;
            return;
        }
        // LEVEL semantics: nothing to arm — while warningActive_ holds, every
        // idle native beat presses. Clear drops the level immediately.
        (void)wasWarning;
    }

    // Returns true and fills `out` when a press frame should be transmitted
    // for this native frame. LEVEL semantics: presses every idle native beat
    // while the warning is active — no single-shot consumption.
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
        if (!warningActive_)
        {
            diag.blockedReason = "idle";
            return false;
        }
        if (dlc < 8)
            return false;

        memcpy(out, data, 8);
        out[4] = static_cast<uint8_t>(out[4] | 0x10U); // press bit (switch word bit4)
        out[6] = static_cast<uint8_t>(((out[6] & 0x0FU)) |
                                      ((((out[6] >> 4) + 1U) & 0x0FU) << 4));
        out[7] = crc8J1850(out, 7);
        lastPressMs_ = nowMs;
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

    // CRC8 SAE J1850 (same polynomial/init/final-xor as the 0x045 wheel
    // vectors).
    static uint8_t crc8J1850(const uint8_t *data, uint8_t length)
    {
        uint8_t crc = 0xFF;
        for (uint8_t i = 0; i < length; i++)
        {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; bit++)
                crc = static_cast<uint8_t>((crc & 0x80) ? ((crc << 1) ^ 0x1D) : (crc << 1));
        }
        return static_cast<uint8_t>(crc ^ 0xFF);
    }

private:
    static constexpr uint32_t kCloneInFlightMs = 20;

    static bool isWarningState(uint8_t handsState)
    {
        return (handsState >= 3 && handsState <= 6) ||
               (handsState >= 9 && handsState <= 10);
    }

    bool enabled_ = false;
    bool runtimeAllowed_ = false;
    bool disarmed_ = false;
    bool warningActive_ = false;
    bool apActive_ = false;
    uint32_t lastPressMs_ = 0;
    uint32_t lastIdleNativeMs_ = 0;
    uint32_t disarmedAtMs_ = 0;
    uint8_t consecutiveTxFailures_ = 0;
    uint8_t healsThisWindow_ = 0;
    uint32_t healWindowStartMs_ = 0;
};
