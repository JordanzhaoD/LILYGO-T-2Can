// dash_legacy_nag_kill_045.h — bit27 "nagKill" parallel DND path (TX split v2).
//
// Reference implementation (Jordan-provided reverse-engineered pseudocode,
// 2026-11-23): on a 0x399 warning edge with hands in [3,5], arm a trigger
// flag; when the NEXT native 0x045 (STW_ACTN_RQ) arrives, clone that frame
// with StW_Cond_Flt bit27 set (byte3 bit3, LSB-first), counter+1 (b6 high
// nibble) and a recomputed CRC8-J1850, then re-send it. ONE frame per
// warning edge — no interval re-fire (the retired 11-18 line's 1500 ms
// cadence is deliberately NOT ported).
//
// Differences from the retired beta17-21 clone (field incident, cause
// never isolated): window starts at hands==3 (not 2), single-shot per
// edge (not periodic), strictly RX-driven (rides the native cadence,
// never free-runs), default OFF, fresh NVS key.
//
// Coordination with the wheel DND engine (beta02): the DND engine treats
// a bit27-tagged frame as a sibling — it must not refresh nativeFresh nor
// start a pair — and yields its nextFrame slot while cloneInFlight().

#pragma once

#include <stdint.h>
#include <string.h>

#include "can_frame_types.h"

struct DashLegacyNagKillDiag
{
    bool enabled = false;
    bool runtimeAllowed = false;
    bool disarmed = false;
    bool triggerArmed = false;
    uint8_t handsState = 0;
    uint32_t cloned = 0;
    uint32_t txFail = 0;
    uint32_t echoSuppressed = 0;
    uint32_t actionFrameSkipped = 0;
    uint32_t disarmHeals = 0;
    uint8_t consecutiveTxFailures = 0;
    const char *blockedReason = "disabled";
};

struct DashLegacyNagKill045
{
    static constexpr uint32_t kCanId = 0x045;
    // Warning window: hands [3,5]. hands>=6 (escalating/strike-out domain)
    // is a hard off — the engine never contests that domain.
    static constexpr uint8_t kHandsMin = 3;
    static constexpr uint8_t kHandsMax = 5;
    // The trigger flag expires if no native frame shows up to ride.
    static constexpr uint32_t kTriggerExpireMs = 500;
    // Clone-in-flight window: the DND engine yields its slot this long
    // after a clone so the two TX writers never contend the counter.
    static constexpr uint32_t kCloneInFlightMs = 20;
    // Self-heal (same shape as the wheel-DND engine, TX split v2).
    static constexpr uint8_t kMaxConsecutiveTxFailures = 2;
    static constexpr uint32_t kHealDelayMs = 800;
    static constexpr uint8_t kMaxHealsPerWindow = 3;
    static constexpr uint32_t kHealWindowMs = 60000;

    DashLegacyNagKillDiag diag;

    void configure(bool enabled)
    {
        enabled_ = enabled;
        diag.enabled = enabled;
        if (!enabled)
        {
            trigger_ = false;
            diag.triggerArmed = false;
            diag.blockedReason = "disabled";
        }
    }

    void setRuntimeAllowed(bool allowed)
    {
        runtimeAllowed_ = allowed;
        diag.runtimeAllowed = allowed;
    }

    // bit27 (LSB-first frame bit 27) = byte3 bit3.
    static bool isCloneFrame(const uint8_t data[8])
    {
        return (data[3] & 0x08U) != 0;
    }

    // True when the frame carries no wheel/stalk action payload — the
    // same switch-word shape (b4 | b5<<8, wheel mask 0x3FFF) the DND
    // engine requires before it will use a frame as source.
    static bool isIdleActionFrame(const uint8_t data[8])
    {
        const uint16_t wheel = static_cast<uint16_t>(
            data[4] | (static_cast<uint16_t>(data[5]) << 8));
        return (wheel & 0x3FFFU) == 0;
    }

    bool cloneInFlight(uint32_t nowMs) const
    {
        return lastCloneMs_ != 0 && nowMs - lastCloneMs_ < kCloneInFlightMs;
    }

    // Beat ownership for the DND yield gate (final-review I1): the clone
    // owns the beat until a FRESH idle native frame has been observed
    // after it. Time alone cannot release the slot — the DND's cached
    // native frame is the one the clone was built on, and pairing from
    // it would repeat the clone's counter.
    bool siblingOwnsBeat(uint32_t /*nowMs*/) const
    {
        return lastCloneMs_ != 0 && lastIdleNativeMs_ <= lastCloneMs_;
    }

    void onDasStatus(uint8_t /*apState*/, uint8_t handsState, uint32_t nowMs)
    {
        healTick(nowMs);
        diag.handsState = handsState;
        const bool inWindow = handsState >= kHandsMin && handsState <= kHandsMax;
        const bool wasInWindow = lastHandsState_ >= kHandsMin && lastHandsState_ <= kHandsMax;
        lastHandsState_ = handsState;
        if (!enabled_ || !runtimeAllowed_ || disarmed_)
            return;
        if (handsState >= 6)
        {
            // Strike-out escalation domain: hard off. An armed trigger is
            // cleared immediately (final-review C2) and no new edge can
            // arm while the domain persists.
            trigger_ = false;
            diag.triggerArmed = false;
            return;
        }
        if (inWindow && !wasInWindow)
        {
            // EDGE only (final-review C1): entering the window arms;
            // staying inside it does NOT re-arm a consumed trigger —
            // a sustained warning yields ONE clone, not ~2 Hz of them.
            trigger_ = true;
            triggerAtMs_ = nowMs;
            diag.triggerArmed = true;
        }
    }

    // Returns true and fills `out` when a clone should be transmitted for
    // this native frame. Single-shot: the trigger is consumed.
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
        if (isCloneFrame(data))
        {
            // Our own echo (or a sibling's tagged frame): never chain and
            // never count as a fresh native beat.
            diag.echoSuppressed++;
            return false;
        }
        if (!isIdleActionFrame(data))
        {
            // Wheel-DND tick echoes / jitter stalk frames carry a
            // functional payload — cloning those would re-send another
            // engine's action with bit27 stitched on (final-review I2).
            diag.actionFrameSkipped++;
            return false;
        }
        // A fresh idle native beat: this is what releases the yield slot.
        lastIdleNativeMs_ = nowMs;
        if (!trigger_)
        {
            diag.blockedReason = "idle";
            return false;
        }
        if (nowMs - triggerAtMs_ > kTriggerExpireMs)
        {
            trigger_ = false;
            diag.triggerArmed = false;
            diag.blockedReason = "triggerExpired";
            return false;
        }
        if (dlc < 8)
            return false;

        trigger_ = false;
        diag.triggerArmed = false;
        memcpy(out, data, 8);
        out[3] = static_cast<uint8_t>(out[3] | 0x08U); // bit27
        out[6] = static_cast<uint8_t>(((out[6] & 0x0FU)) |
                                      ((((out[6] >> 4) + 1U) & 0x0FU) << 4));
        out[7] = crc8J1850(out, 7);
        lastCloneMs_ = nowMs;
        diag.cloned++;
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
    bool enabled_ = false;
    bool runtimeAllowed_ = false;
    bool disarmed_ = false;
    bool trigger_ = false;
    uint8_t lastHandsState_ = 0;
    uint32_t triggerAtMs_ = 0;
    uint32_t lastCloneMs_ = 0;
    uint32_t lastIdleNativeMs_ = 0;
    uint32_t disarmedAtMs_ = 0;
    uint8_t consecutiveTxFailures_ = 0;
    uint8_t healsThisWindow_ = 0;
    uint32_t healWindowStartMs_ = 0;
};
