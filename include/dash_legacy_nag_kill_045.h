// dash_legacy_nag_kill_045.h — bit27 "nagKill" parallel DND path (TX split v3).
//
// Field verdict (2026-11-24): the v2 single-shot-per-edge form is dead. A
// lone bit27 frame buried in the 10 Hz native 0x045 stream does nothing —
// the "steering-wheel state valid" claim only holds if it is asserted
// CONTINUOUSLY, one clone riding every native beat while hands ∈ [3,5].
//
// v3 semantics (level, not edge): while handsState ∈ [3,5] persists, every
// native idle 0x045 that arrives is cloned 1:1 (bit27 set, counter+1,
// recomputed CRC8-J1850) and re-sent in-line, one beat behind the native
// frame. When hands leaves the window (or reaches ≥6, the strike-out
// domain) the engine stops immediately — no trigger to consume, no expiry,
// no free-run. This is the same RX-synchronized "ride the native stream"
// shape as the wheel-DND pair engine and the 0x3EE offsetOnly speed writer,
// both field-verified.
//
// Unchanged from v2: window starts at hands==3 (not 2); strictly RX-driven
// (never free-runs); default OFF; fresh NVS key nk27.
//
// Safety gates retained: idle-only riding, own-echo suppression, DND
// sibling recognition + beat yield, TX-failure disarm self-heal.
//
// Coordination with the wheel DND engine (beta02): the DND engine treats
// a bit27-tagged frame as a sibling — it must not refresh nativeFresh nor
// start a pair — and yields its nextFrame slot while the clone owns the
// beat (beat-generational yield, final-review I1).

#pragma once

#include <stdint.h>
#include <string.h>

#include "can_frame_types.h"

struct DashLegacyNagKillDiag
{
    bool enabled = false;
    bool runtimeAllowed = false;
    bool disarmed = false;
    bool inWindow = false;
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
            inWindow_ = false;
            diag.inWindow = false;
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
        lastHandsState_ = handsState;
        if (!enabled_ || !runtimeAllowed_ || disarmed_)
        {
            // Engine is down: the level drops to out-of-window so no clone
            // can leak through while blocked.
            inWindow_ = false;
            diag.inWindow = false;
            return;
        }
        if (handsState >= 6)
        {
            // Strike-out escalation domain: hard off (final-review C2).
            // Level drops immediately — a prior in-window level is voided.
            inWindow_ = false;
            diag.inWindow = false;
            return;
        }
        // LEVEL, not edge: inWindow_ tracks the current hands level. While
        // hands stays in [3,5], every native idle beat clones (1:1 follow).
        inWindow_ = handsState >= kHandsMin && handsState <= kHandsMax;
        diag.inWindow = inWindow_;
    }

    // Returns true and fills `out` when a clone should be transmitted for
    // this native frame. LEVEL semantics: clones every idle native beat
    // while hands ∈ [3,5] — no single-shot consumption.
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
        if (!inWindow_)
        {
            diag.blockedReason = "idle";
            return false;
        }
        if (dlc < 8)
            return false;

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
    bool inWindow_ = false;
    uint8_t lastHandsState_ = 0;
    uint32_t lastCloneMs_ = 0;
    uint32_t lastIdleNativeMs_ = 0;
    uint32_t disarmedAtMs_ = 0;
    uint8_t consecutiveTxFailures_ = 0;
    uint8_t healsThisWindow_ = 0;
    uint32_t healWindowStartMs_ = 0;
};
