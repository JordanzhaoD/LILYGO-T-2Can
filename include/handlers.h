#pragma once

#include <memory>
#include <algorithm>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "shared_types.h"
#include "log_buffer.h"
#include "dash_hw3_speed.h"
#include "dash_legacy_speed.h"

#ifndef DASH_FSD_252_COMPAT
#define DASH_FSD_252_COMPAT 0
#endif

#ifndef NATIVE_BUILD
#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#else
#include <Arduino.h>
#endif
#endif

inline LogRingBuffer logRing;

static inline bool framePayloadChanged(const CanFrame &original, const CanFrame &modified)
{
    if (original.id != modified.id || original.dlc != modified.dlc)
        return true;

    const uint8_t dlc = (original.dlc <= 8) ? original.dlc : 8;
    for (uint8_t i = 0; i < dlc; ++i)
    {
        if (original.data[i] != modified.data[i])
            return true;
    }
    return false;
}

struct CarManagerBase
{
    Shared<int> speedProfile{1};
    Shared<bool> speedProfileAuto{true};
    Shared<bool> ADEnabled{false};
    Shared<bool> APActive{false};
    // Default Parked=true so the AP Injection Gate opens immediately on
    // module boot when the DI is asleep (e.g. car locked with Sentry on,
    // CAN ID 280 not broadcast). The first DI_systemStatus frame with a
    // driving gear (R/N/D) flips this to false; if 280 never arrives,
    // the car is asleep / parked and the gate stays open by design.
    Shared<bool> Parked{true};
    Shared<bool> Summoning{false};
    Shared<int> gatewayAutopilot{-1};
    Shared<bool> enablePrint{true};
    Shared<uint32_t> frameCount{0};
    Shared<uint32_t> framesSent{0};
    Shared<int> speedOffset{0};
    // --- FSD activation state (from tesla-fsd-controller verified logic) ---
    Shared<bool> fsdTriggered{false};
    Shared<bool> removeVisionSpeedLimit{true};
    Shared<int> legacyOffset{0};
    Shared<bool> overrideSpeedLimit{false};
    Shared<bool> tlsscBypass{false};
    Shared<bool> emergencyVehicleDetection{true};
    Shared<bool> isaChimeSuppress{false};
    Shared<uint8_t> hw4OffsetRaw{0};
    Shared<bool> banShieldEnable{false};
    Shared<uint32_t> banShieldBlocks{0};
    Shared<uint8_t> hwDetected{0};
    Shared<bool> autoModeEnabled{false};
    // Ban Shield per-mux state (CAN task only, non-atomic)
    uint8_t banShieldSnapshot[8][8] = {};
    bool banShieldValid[8] = {};

    unsigned long lastSummonActivityMs = 0;
    // Summon-vs-AP/TACC discrimination state. ACA (DI_autonomyControlActive)
    // alone is set during AP, TACC, and Smart Summon, so it cannot be the
    // sole gate signal. We only treat ACA as "summon active" when we have
    // also observed UI_selfParkRequest go non-zero during the current
    // autonomy episode. ACA falling edge clears sprSeen so the next ACA
    // rising edge (e.g. user engaging TACC after a completed summon) does
    // not falsely keep the gate open.
    bool sprSeen = false;
    bool lastAca = false;

    void (*onFrame)(const CanFrame &) = nullptr;
    void (*onSend)(uint8_t mux, bool ok) = nullptr;
    bool (*checkAD)() = nullptr;
    bool (*checkNag)() = nullptr;
    bool (*checkSummon)() = nullptr;
    bool (*checkIsa)() = nullptr;
    bool (*checkEvd)() = nullptr;

    bool injectionGateOpen() const
    {
        return (bool)APActive || (bool)Parked || (bool)Summoning;
    }

    // Recompute Summoning from current sprSeen + lastAca state. Summoning
    // requires both: ACA bit currently set AND we have seen at least one
    // UI_selfParkRequest non-zero command in the current autonomy episode.
    // This excludes plain TACC (ACA=1, no spr) and post-AP ACA tail
    // (ACA blip with no fresh spr) from latching the gate.
    void recomputeSummoning()
    {
        Summoning = lastAca && sprSeen;
    }

    // Update summon state from UI_driverAssistControl (CAN ID 1016).
    // Tesla DBC: UI_selfParkRequest at byte 3 bits 4-7 (4=PRIME, 5=PAUSE,
    // 7/8=AUTO_SUMMON_FWD/REV, 11=SMART_SUMMON, 0=NONE). Records that a
    // summon command has been issued during the current autonomy episode.
    void updateSummonFrom1016(const CanFrame &frame)
    {
        if (frame.dlc < 4)
            return;
        uint8_t spr = static_cast<uint8_t>((frame.data[3] >> 4) & 0x0F);
        if (spr != 0)
            sprSeen = true;
        recomputeSummoning();
    }

    // Update summon state from DI_systemStatus (CAN ID 280).
    // Tesla DBC: DI_autonomyControlActive at bit 50 (byte 6 bit 2). Held
    // high while the DI is being driven by AP, TACC, Smart Summon, etc.
    // ACA falling edge ends the autonomy episode and clears sprSeen so a
    // subsequent TACC engagement (ACA=1 again) does not re-latch the gate.
    void updateSummonFromDISystemStatus(const CanFrame &frame)
    {
        if (frame.dlc < 7)
            return;
        bool aca = (frame.data[6] & 0x04) != 0;
        if (lastAca && !aca)
            sprSeen = false;
        lastAca = aca;
        recomputeSummoning();
    }

    // Force Summoning off and reset sprSeen when the vehicle is observed
    // in Park with no active autonomy episode, so a manual P->D shift
    // afterwards correctly waits for AP. During Smart Summon startup the
    // DI can report ACA=1 while gear is still P; keep sprSeen latched so
    // it survives the pending shift out of Park.
    void clearSummonOnPark()
    {
        Summoning = false;
        sprSeen = false;
#ifndef NATIVE_BUILD
        lastSummonActivityMs = 0;
#endif
    }

    void clearSummonOnParkIfAcaInactive(uint8_t gear)
    {
        if (gear == 1 && !lastAca)
            clearSummonOnPark();
    }

    bool shouldInjectSpeedProfile() const
    {
#if defined(ESP32_DASHBOARD)
        return !speedProfileAuto;
#else
        return true;
#endif
    }

    virtual void handleMessage(CanFrame &frame, CanDriver &driver) = 0;
    virtual const uint32_t *filterIds() const = 0;
    virtual uint8_t filterIdCount() const = 0;
    virtual ~CarManagerBase() = default;
};

struct LegacyHandler : public CarManagerBase
{
    const uint32_t *filterIds() const override
    {
        // 1080 added for UI_driverAssistAnonDebugParams visionSpeedSlider override.
        static constexpr uint32_t ids[] = {69, 280, 390, 760, 921, 1006, 1080};
        return ids;
    }
    uint8_t filterIdCount() const override { return 7; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        // STW_ACTN_RQ (0x045 = 69): Follow-Distance-Stalk as Source for Profile Mapping
        // byte[1]: 0x00=Pos1, 0x21=Pos2, 0x42=Pos3, 0x64=Pos4, 0x85=Pos5, 0xA6=Pos6, 0xC8=Pos7
        if (frame.id == 69)
        {
            if (frame.dlc < 2)
                return;
            if (!speedProfileAuto)
                return;
            uint8_t pos = frame.data[1] >> 5;
            if (pos <= 1)
                speedProfile = 2;
            else if (pos == 2)
                speedProfile = 1;
            else
                speedProfile = 0;
            return;
        }
        // UI_gpsVehicleSpeed (0x2F8 = 760): write UI_userSpeedOffset (bit40|6,
        // raw = kph+30). Byte 5 layout: bits 0-5 = offset (0-63), bit 6 reserved,
        // bit 7 = UI_userSpeedOffsetUnits (0=MPH, 1=KPH). We preserve bits 6-7
        // so the offset unit follows the car's setting.
        if (frame.id == 760)
        {
            if ((int)legacyOffset == 0) return;
            if (frame.dlc < 6) return;
            uint8_t raw = (uint8_t)((int)legacyOffset + 30);
            frame.data[5] = (frame.data[5] & 0xC0) | (raw & 0x3F);
            framesSent++;
            driver.send(frame);
            if (onSend) onSend(0, true);
            return;
        }
        // 0x438 (1080) — UI_driverAssistAnonDebugParams: visionSpeedSlider = 100
        if (frame.id == 1080)
        {
            if (frame.dlc < 8) return;
            if (!overrideSpeedLimit) return;
            frame.data[7] = (frame.data[7] & 0x80) | 100;
            framesSent++;
            driver.send(frame);
            if (onSend) onSend(0, true);
            return;
        }
        if (frame.id == 280)
        {
            if (frame.dlc < 3)
                return;
            {
                uint8_t diGear = readDIGear(frame);
                Parked = isVehicleParked(diGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions
                // (e.g. during a Summon shift to Reverse) and would
                // otherwise drop the gate mid-summon.
                updateSummonFromDISystemStatus(frame);
                clearSummonOnParkIfAcaInactive(diGear);
            }
            return;
        }
        if (frame.id == 390)
        {
            if (frame.dlc < 8)
                return;
            {
                uint8_t difGear = readVehicleGear(frame);
                Parked = isVehicleParked(difGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions.
                clearSummonOnParkIfAcaInactive(difGear);
            }
            return;
        }
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            APActive = isDASAutopilotActive(readDASAutopilotStatus(frame));
            return;
        }
        // 0x3EE (1006) — FSD activation frame (mux 0/1)
        if (frame.id == 1006)
        {
            if (frame.dlc < 8)
                return;
            auto index = readMuxID(frame);
            // Mux 0: detect FSD selection and activate
            if (index == 0)
            {
                fsdTriggered = (bool)forceActivateRuntime || isFSDSelectedInUI(frame);
            }
            if (index == 0 && (bool)fsdTriggered && (!checkAD || checkAD()))
            {
                ADEnabled = true;
                setBit(frame, 46, true);
                setSpeedProfileV12V13(frame, speedProfile);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(0, true);
            }
            // Mux 1: nag suppression + optional vision speed limit removal
            if (index == 1 && (bool)fsdTriggered && (!checkAD || checkAD()))
            {
                setBit(frame, 19, false);
                if ((bool)removeVisionSpeedLimit) setBit(frame, 48, false);
                driver.send(frame);
                if (onSend) onSend(1, true);
            }
            if (index == 0 && enablePrint)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "LegacyHandler: AD: %d, Profile: %d",
                         (bool)ADEnabled, (int)speedProfile);
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
        }
    }
};

struct HW3Handler : public CarManagerBase
{
    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {280, 390, 921, 1016, 1021, 2047};
        return ids;
    }
    uint8_t filterIdCount() const override { return 6; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        if (frame.id == 280)
        {
            if (frame.dlc < 3)
                return;
            {
                uint8_t diGear = readDIGear(frame);
                Parked = isVehicleParked(diGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions
                // (e.g. during a Summon shift to Reverse) and would
                // otherwise drop the gate mid-summon.
                updateSummonFromDISystemStatus(frame);
                clearSummonOnParkIfAcaInactive(diGear);
            }
            return;
        }
        if (frame.id == 390)
        {
            if (frame.dlc < 8)
                return;
            {
                uint8_t difGear = readVehicleGear(frame);
                Parked = isVehicleParked(difGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions.
                clearSummonOnParkIfAcaInactive(difGear);
            }
            return;
        }
        if (frame.id == 1016)
        {
            if (frame.dlc < 6)
                return;
            updateSummonFrom1016(frame);
            if (!speedProfileAuto)
                return;
            uint8_t followDistance = (frame.data[5] & 0b11100000) >> 5;
            switch (followDistance)
            {
            case 1:
                speedProfile = 2;
                break;
            case 2:
                speedProfile = 1;
                break;
            case 3:
                speedProfile = 0;
                break;
            default:
                break;
            }
            return;
        }
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            APActive = isDASAutopilotActive(readDASAutopilotStatus(frame));
            // Capture ISA fused speed limit from byte1[4:0]. raw*5 = kph;
            // 0 = SNA, 31 = NONE-broadcast — both treated as "unknown" by the
            // HW3 mux-2 override path. Used by dashComputeHw3OffsetRaw().
            if (frame.dlc >= 2)
                fusedSpeedLimitRaw = static_cast<uint8_t>(frame.data[1] & 0x1F);
            return;
        }
        if (frame.id == 2047)
        {
            if (frame.dlc < 6)
                return;
            if (readMuxID(frame) != 2)
                return;

            uint8_t next = readGTWAutopilot(frame);
            int prev = gatewayAutopilot;
            gatewayAutopilot = next;

            if (enablePrint && prev != next)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "HW3Handler: GTW_autopilot: %d -> %u (%s)",
                         prev, (unsigned int)next, describeGTWAutopilot(next));
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
            return;
        }
        if (frame.id == 1021)
        {
            if (frame.dlc < 8)
                return;
            auto index = readMuxID(frame);

            // ── Mux 0: FSD activation ──────────────────────────────────────
            if (index == 0)
            {
                bool fsdRequested = (bool)forceActivateRuntime || isFSDSelectedInUI(frame);
                fsdTriggered = fsdRequested && (!checkAD || checkAD());
                ADEnabled = (bool)fsdTriggered;
            }
            if (index == 0 && (bool)fsdTriggered)
            {
                speedOffset = std::max(std::min(((int)((frame.data[3] >> 1) & 0x3F) - 30) * 5, 100), 0);
                hw3StockOffsetKph = (int)speedOffset;
                setBit(frame, 46, true);
                if ((bool)tlsscBypass) setBit(frame, 38, true);
                setSpeedProfileV12V13(frame, speedProfile);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(0, true);
            }

            // ── Mux 1: Nag suppression ─────────────────────────────────────
            if (index == 1 && (bool)fsdTriggered)
            {
                setBit(frame, 19, false);
                driver.send(frame);
                if (onSend) onSend(1, true);
            }

            // ── Mux 2: Speed offset (three-layer + slew limiter) ──────────
            if (index == 2 && (bool)fsdTriggered)
            {
                uint8_t activeRaw = (uint8_t)std::max(std::min((int)speedOffset, 255), 0);

                uint8_t fl = fusedSpeedLimitRaw;
                if (fl > 0 && fl < 31) {
                    int fusedLimitKph = (int)fl * 5;
                    if (fusedLimitKph < kHw3StockOffsetCutoverKph) {
                        if (hw3CustomSpeed || hw3AutoSpeed) {
                            uint16_t targetSpeedKph = hw3CustomSpeed
                                ? dashComputeHw3CustomTargetKph(static_cast<uint8_t>(fusedLimitKph))
                                : dashComputeHw3AutoTargetKph(static_cast<uint8_t>(fusedLimitKph));
                            if (targetSpeedKph > 0) {
                                int desiredOffsetKph = std::max((int)targetSpeedKph - fusedLimitKph, 0);
                                activeRaw = dashEncodeHw3Offset(desiredOffsetKph, static_cast<uint8_t>(fusedLimitKph));
                            }
                        }
                    } else {
                        if (hw3HighSpeedEnable) {
                            int idx = (fusedLimitKph - kHw3HighSpeedBucketBaseKph_verified)
                                      / kHw3HighSpeedBucketStepKph_verified;
                            if (idx < 0) idx = 0;
                            if (idx >= kHw3HighSpeedBucketCount_verified) idx = kHw3HighSpeedBucketCount_verified - 1;
                            uint8_t pct = hw3HighSpeedTargetPct[idx];
                            if (pct > 0) {
                                activeRaw = dashEncodeHw3OffsetFromPct((int)pct, static_cast<uint8_t>(fusedLimitKph));
                            }
                        }
                    }
                }

                hw3OffsetTargetRaw = activeRaw;

                // Slew limiter: damps downward drops only
                if (hw3OffsetSlew && fl > 0 && (int)fl * 5 < kHw3StockOffsetCutoverKph) {
                    uint32_t now =
#ifndef NATIVE_BUILD
                        millis();
#else
                        0;
#endif
                    uint8_t last = hw3OffsetLastRaw;
                    uint8_t ratePctPerSec = dashLoadHw3SlewRate(hw3SlewRate);
                    uint32_t rateRawPerSec = (uint32_t)ratePctPerSec * 4;
                    if (activeRaw < last && hw3OffsetLastSentMs != 0) {
                        uint32_t dt = now - hw3OffsetLastSentMs;
                        uint32_t maxDrop = (rateRawPerSec * dt + 500) / 1000;
                        uint8_t floorRaw = last > maxDrop ? (uint8_t)(last - maxDrop) : 0;
                        if (activeRaw < floorRaw) {
                            activeRaw = floorRaw;
                            hw3OffsetSlewCount++;
                        }
                    }
                    hw3OffsetLastRaw = activeRaw;
                    hw3OffsetLastSentMs = now;
                } else {
                    hw3OffsetLastRaw = activeRaw;
#ifndef NATIVE_BUILD
                    hw3OffsetLastSentMs = millis();
#endif
                }

                // Write to wire format
                frame.data[0] &= ~(0b11000000);
                frame.data[1] &= ~(0b00111111);
                frame.data[0] |= (activeRaw & 0x03) << 6;
                frame.data[1] |= (activeRaw >> 2);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(2, true);
            }

            if (index == 0 && enablePrint)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "HW3Handler: AD: %d, Profile: %d, Offset: %d",
                         (bool)ADEnabled, (int)speedProfile, (int)speedOffset);
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
        }
    }
};

/**
 * NagHandler — Autosteer nag suppression (counter+1 echo method)
 *
 * Replicates the Chinese TSL6P module behavior:
 * - Listens for CAN 880 (0x370) = EPAS3P_sysStatus
 * - When handsOnLevel = 0 (nag would trigger):
 *   1. Copies the real frame
 *   2. Sets byte 3 = 0xB6 (fixed torsionBarTorque = 1.80 Nm)
 *   3. Sets byte 4 |= 0x40 (handsOnLevel = 1)
 *   4. Increments counter (byte 6 lower nibble + 1)
 *   5. Recalculates checksum (byte 7)
 * - The real EPAS frame with the same counter arrives AFTER -> rejected as duplicate
 *
 * Tested: Model Y Performance 2022 HW3, Basic Autopilot
 * Bus: X179 pin 2/3 (CAN bus 4)
 *
 * Enable with build flag: -D NAG_KILLER
 */
struct NagHandler : public CarManagerBase
{
    Shared<bool> nagKillerActive{true};
    Shared<uint32_t> nagEchoCount{0};

    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {880};
        return ids;
    }
    uint8_t filterIdCount() const override { return 1; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (frame.id != 880 || frame.dlc < 8)
            return;

        uint8_t handsOn = (frame.data[4] >> 6) & 0x03;

        if (!nagKillerActive || !nagKillerRuntime || handsOn != 0)
            return;

        CanFrame echo;
        echo.id = 880;
        echo.dlc = 8;

        echo.data[0] = frame.data[0];
        echo.data[1] = frame.data[1];
        echo.data[2] = (frame.data[2] & 0xF0) | 0x08;
        echo.data[5] = frame.data[5];

        // Fixed torque = 1.80 Nm (tRaw = 0x08B6)
        echo.data[3] = 0xB6;

        // handsOnLevel = 1
        echo.data[4] = frame.data[4] | 0x40;

        // Counter + 1
        uint8_t cnt = (frame.data[6] & 0x0F);
        cnt = (cnt + 1) & 0x0F;
        echo.data[6] = (frame.data[6] & 0xF0) | cnt;

        // Checksum: sum(byte0..byte6) + 0x73
        uint16_t sum = echo.data[0] + echo.data[1] + echo.data[2] + echo.data[3] + echo.data[4] + echo.data[5] + echo.data[6];
        echo.data[7] = static_cast<uint8_t>((sum + 0x73) & 0xFF);

        framesSent++;
        nagEchoCount++;
        driver.send(echo);

        if (enablePrint && (nagEchoCount % 500 == 1))
        {
            char buf[LogRingBuffer::kMaxMsgLen];
            snprintf(buf, sizeof(buf), "NagHandler: echo=%u",
                     (unsigned int)(uint32_t)nagEchoCount);
            logRing.push(buf,
#ifndef NATIVE_BUILD
                         millis()
#else
                         0
#endif
            );
#ifndef NATIVE_BUILD
            Serial.println(buf);
#endif
        }
    }
};

struct HW4Handler : public CarManagerBase
{
    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {280, 390, 921, 1016, 1021, 2047};
        return ids;
    }
    uint8_t filterIdCount() const override { return 6; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        if (frame.id == 280)
        {
            if (frame.dlc < 3)
                return;
            {
                uint8_t diGear = readDIGear(frame);
                Parked = isVehicleParked(diGear);
                updateSummonFromDISystemStatus(frame);
                clearSummonOnParkIfAcaInactive(diGear);
            }
            return;
        }
        if (frame.id == 390)
        {
            if (frame.dlc < 8)
                return;
            {
                uint8_t difGear = readVehicleGear(frame);
                Parked = isVehicleParked(difGear);
                clearSummonOnParkIfAcaInactive(difGear);
            }
            return;
        }
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            APActive = isDASAutopilotActive(readDASAutopilotStatus(frame));
            if (frame.dlc >= 2)
                fusedSpeedLimitRaw = static_cast<uint8_t>(frame.data[1] & 0x1F);
            // ISA chime suppress — runtime gate (all build modes)
            if ((bool)isaChimeSuppress && frame.dlc >= 8)
            {
                frame.data[1] |= 0x20;
                frame.data[7] = computeVehicleChecksum(frame);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(0, true);
                return;
            }
            return;
        }
        if (frame.id == 1016)
        {
            if (frame.dlc < 6)
                return;
            updateSummonFrom1016(frame);
            if (!speedProfileAuto)
                return;
            auto fd = (frame.data[5] & 0b11100000) >> 5;
            switch (fd)
            {
            case 1:
                speedProfile = 3;
                break;
            case 2:
                speedProfile = 2;
                break;
            case 3:
                speedProfile = 1;
                break;
            case 4:
                speedProfile = 0;
                break;
            case 5:
                speedProfile = 4;
                break;
            }
            return;
        }
        if (frame.id == 2047)
        {
            if (frame.dlc < 6)
                return;
            if (readMuxID(frame) != 2)
                return;

            uint8_t next = readGTWAutopilot(frame);
            int prev = gatewayAutopilot;
            gatewayAutopilot = next;

            if (enablePrint && prev != next)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "HW4Handler: GTW_autopilot: %d -> %u (%s)",
                         prev, (unsigned int)next, describeGTWAutopilot(next));
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
            return;
        }
        if (frame.id == 1021)
        {
            if (frame.dlc < 8)
                return;
            auto index = readMuxID(frame);

            // Mux 0: FSD activation
            if (index == 0)
            {
                bool fsdRequested = (bool)forceActivateRuntime || isFSDSelectedInUI(frame);
                fsdTriggered = fsdRequested && (!checkAD || checkAD());
                ADEnabled = (bool)fsdTriggered;
            }
            if (index == 0 && (bool)fsdTriggered)
            {
                setBit(frame, 46, true);
                setBit(frame, 60, true);
                if ((bool)emergencyVehicleDetection) setBit(frame, 59, true);
                if ((bool)tlsscBypass) setBit(frame, 38, true);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(0, true);
            }

            // Mux 1: Nag suppression + FSD ready signal
            if (index == 1 && (bool)fsdTriggered)
            {
                setBit(frame, 19, false);
                setBit(frame, 47, true);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(1, true);
            }

            // Mux 2: Speed profile + offset
            if (index == 2 && (bool)fsdTriggered)
            {
                // Speed profile
                frame.data[7] &= static_cast<uint8_t>(~(0x07 << 4));
                frame.data[7] |= static_cast<uint8_t>((int)speedProfile & 0x07) << 4;
                // Offset (HW4 offset support)
                if ((int)hw4OffsetRaw > 0)
                    frame.data[1] = (frame.data[1] & 0xC0) | ((int)hw4OffsetRaw & 0x3F);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(2, true);
            }

            if (index == 0 && enablePrint)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "HW4Handler: AD: %d, Profile: %d",
                         (bool)ADEnabled, (int)speedProfile);
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
        }
    }
};
