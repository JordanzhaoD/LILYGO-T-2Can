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
#include "dash_abort_guard.h"
#include "dash_bionic_steer.h"
#include "dash_reactive_nag.h"
#include "dash_fsd_diag.h"

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

struct LegacySpeedRuntimeDiag
{
    LegacySmartOffsetResult result{};
    LegacySpeedLimitSource limitSource = LegacySpeedLimitSource::None;
    bool gpsSpeedSeen = false;
    bool gpsSpeedFresh = false;
    uint32_t gpsSpeedPeriodMs = 0;
    uint32_t gpsSpeedLastMs = 0;
    uint32_t gpsSpeedCount = 0;
    uint8_t gpsUserOffsetRaw = 0;
    int gpsUserOffsetKph = -30;
    uint8_t gpsMppLimitRaw = 0;
    uint16_t gpsMppLimitKph = 0;
    uint32_t fusedLimitLastMs = 0;
    uint8_t lastSentOffsetRaw = 0;
    uint8_t lastSentOffsetKph = 0;
    uint8_t outputOffsetPct = 0;
    uint32_t mux0Count = 0;
    uint8_t mux0RxByte3 = 0;
    uint8_t mux0TxByte3 = 0;
    uint8_t mux0RxByte5 = 0;
    uint8_t mux0TxByte5 = 0;
    uint8_t mux0RxByte7 = 0;
    uint8_t mux0TxByte7 = 0;
    uint32_t txOk = 0;
    uint32_t txFail = 0;
    uint32_t offsetOnlyTxOk = 0;
    uint32_t offsetOnlyTxFail = 0;
    bool runtimeActive = false;
    const char *blockedReason = "off";
};

struct CarManagerBase
{
    Shared<int> speedProfile{1};
    Shared<bool> speedProfileAuto{true};
    Shared<bool> ADEnabled{false};
    Shared<bool> APActive{false};
    Shared<uint8_t> currentGear{0};
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
    LegacyFsdDiag legacyFsdDiag{};
    Shared<int> legacyOffset{0};
    Shared<bool> overrideSpeedLimit{false};
    LegacySmartOffsetConfig legacySmartOffsetConfig{};
    LegacySmartOffsetEngine legacySmartOffsetEngine{};
    LegacySpeedRuntimeDiag legacySpeedDiag{};
    DashLegacySpeedBusLock legacySpeedBus{};
    DashAbortGuard abortGuard{};
    Shared<bool> tlsscBypass{false};
    Shared<bool> emergencyVehicleDetection{true};
    Shared<bool> isaChimeSuppress{false};
    Shared<bool> isaOverride{false};      // spec Task 3: HW4 重写限速 5b (0x39B raw=31 NONE)
    Shared<bool> bionicSteering{false};   // Phase 3: bionic steering mode
    Shared<uint8_t> hw4OffsetRaw{0};
    Shared<bool> banShieldEnable{false};
    Shared<uint32_t> banShieldBlocks{0};
    Shared<uint8_t> hwDetected{0};
    Shared<bool> autoModeEnabled{false};
    // Ban Shield per-mux state (CAN task only, non-atomic)
    uint8_t banShieldSnapshot[8][8] = {};
    bool banShieldValid[8] = {};

    bool handleBanShield(CanFrame &frame, CanDriver &driver)
    {
        if (!(bool)banShieldEnable) return false;
        if (frame.id != 2047 || frame.dlc < 8) return false;

        uint8_t mux = readMuxID(frame);
        if (mux >= 8) return false;

        if (!banShieldValid[mux]) {
            for (int i = 0; i < 8; i++) banShieldSnapshot[mux][i] = frame.data[i];
            banShieldValid[mux] = true;
            return false;
        }

        bool changed = false;
        for (int i = 0; i < 8; i++) {
            if (frame.data[i] != banShieldSnapshot[mux][i]) {
                changed = true;
                break;
            }
        }
        if (!changed) return false;

        if (checkAD && !checkAD()) return true;
        CanFrame out = frame;
        for (int i = 0; i < 8; i++) out.data[i] = banShieldSnapshot[mux][i];
        banShieldBlocks++;
        driver.send(out);
        return true;
    }

    // Auto hardware detection from GTW_carConfig (CAN 920)
    void updateHwDetectedFrom920(const CanFrame &frame)
    {
        if (frame.id != 920 || frame.dlc < 1) return;
        uint8_t das_hw = (frame.data[0] >> 6) & 0x03;
        if (das_hw == 2) hwDetected = 1;   // HW3
        else if (das_hw == 3) hwDetected = 2; // HW4
    }

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
    FsdGateBlockReason (*gateBlockReason)() = nullptr;
    bool (*legacyFsdActivationAllowed)(uint32_t nowMs) = nullptr;
    // v1.19 JITTER hooks (see dash_jitter_procedure.h). The observe wrapper
    // is fed from the Legacy mux0 branch with the full native frame BEFORE
    // any gate decision — the procedure caches its own one-shot template
    // (byte5 |= 0x43 clone) from it; the inhibit wrapper pauses every device
    // 0x3EE write while the procedure runs.
    void (*jitterObserve3ee)(const uint8_t data[8], uint8_t bus, uint32_t nowMs) = nullptr;
    bool (*jitterInhibit3ee)() = nullptr;
    // When set and returns true, an installed plugin owns the FSD activation
    // frames (1006/1021/2047) and the built-in handler must suppress its own
    // injection to avoid duplicate frames. Defaults to nullptr so the built-in
    // path is always allowed (no behavior change for dual CAN / native tests).
    bool (*pluginOwnsFsdActivation)() = nullptr;

    // Returns false only when a plugin currently owns FSD activation, so the
    // built-in send paths should skip. With no callback (default) the built-in
    // path is always allowed.
    bool builtInFsdInjectionAllowed() const
    {
        if (pluginOwnsFsdActivation && pluginOwnsFsdActivation())
            return false;
        return true;
    }

    bool injectionGateOpen() const
    {
        return (bool)APActive || (bool)Parked || (bool)Summoning;
    }

    bool injectionAllowed() const
    {
        if (checkAD && !checkAD())
            return false;
        return enhancedAutopilotInjectionAllowed(injectionGateOpen());
    }

    FsdGateBlockReason currentGateBlockReason() const
    {
        if (gateBlockReason)
            return gateBlockReason();
        if (checkAD && !checkAD())
            return FsdGateBlockReason::CheckAd;
        if (!enhancedAutopilotInjectionAllowed(injectionGateOpen()))
            return FsdGateBlockReason::CompileGate;
        return FsdGateBlockReason::CheckAd;
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

    // Process DI_systemStatus (CAN 280) — gear + summon update.
    // Returns true if the frame was handled.
    bool handleDISystemStatus(const CanFrame &frame)
    {
        if (frame.id != 280 || frame.dlc < 3)
            return false;
        uint8_t diGear = readDIGear(frame);
        currentGear = diGear;
        Parked = isVehicleParked(diGear);
        updateSummonFromDISystemStatus(frame);
        clearSummonOnParkIfAcaInactive(diGear);
        return true;
    }

    // Process DI_vehicleStatus (CAN 390) — gear update.
    // Returns true if the frame was handled.
    bool handleDIVehicleStatus(const CanFrame &frame)
    {
        if (frame.id != 390 || frame.dlc < 8)
            return false;
        uint8_t difGear = readVehicleGear(frame);
        currentGear = difGear;
        Parked = isVehicleParked(difGear);
        clearSummonOnParkIfAcaInactive(difGear);
        return true;
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
    virtual bool bionicDisabled() const { return false; }
    virtual void resetBionic(uint32_t seed) { (void)seed; }
    virtual DashLegacyNagDiag legacyNagDiag(uint32_t /*nowMs*/) const { return {}; }
    virtual ~CarManagerBase() = default;
};

struct LegacyHandler : public CarManagerBase
{
    static constexpr uint32_t kNagContextStaleMs{1000};
    static constexpr uint8_t kNagMaxConsecutiveTxFailures{3};
    static constexpr uint8_t kNagMaxCounterCollisions{3};
    static constexpr uint32_t kNagOwnEchoWindowMs{100};
    static constexpr uint32_t kNagNativeFollowerMinMs{20};
    DashLegacyNagV4 legacyNagV4;
    DashNagBusLock legacyNagBus;
    DashLegacyNagDiag legacyNagState{};
    bool legacyNagAutoDisarmed{false};
    uint8_t legacyNagLastTxData[8]{};
    uint8_t legacyNagLastTxDlc{0};
    uint8_t legacyNagLastTxBus{CAN_BUS_ANY};
    bool legacyNagHasTxFingerprint{false};

    static FsdDiagBus legacyDiagBus(const CanFrame &frame)
    {
        return frame.bus == CAN_BUS_PARTY ? FsdDiagBus::CanB : FsdDiagBus::CanA;
    }

    static FsdDiagDriver legacyDiagDriver(const CanFrame &frame)
    {
        return frame.bus == CAN_BUS_PARTY ? FsdDiagDriver::Mcp2515 : FsdDiagDriver::Twai;
    }

    bool legacySpeedOffsetRequested() const
    {
        return dashClampLegacySmartMode(static_cast<int>(legacySmartOffsetConfig.mode)) !=
               LegacySmartOffsetMode::Off;
    }

    uint8_t computeLegacySpeedOffset(uint32_t nowMs)
    {
        legacySpeedDiag.gpsSpeedFresh = legacySpeedDiag.gpsSpeedSeen &&
                                        static_cast<uint32_t>(nowMs - legacySpeedDiag.gpsSpeedLastMs) <=
                                            kLegacyGpsLimitFreshMs;
        bool fusedFresh = legacySpeedDiag.fusedLimitLastMs != 0 &&
                          static_cast<uint32_t>(nowMs - legacySpeedDiag.fusedLimitLastMs) <=
                              kLegacyFusedLimitFreshMs;
        uint8_t limitRaw = 0;
        legacySpeedDiag.limitSource = LegacySpeedLimitSource::None;
        if (legacySpeedDiag.gpsSpeedFresh && dashLegacySmartLimitValid(legacySpeedDiag.gpsMppLimitRaw))
        {
            limitRaw = legacySpeedDiag.gpsMppLimitRaw;
            legacySpeedDiag.limitSource = LegacySpeedLimitSource::Gps2F8;
        }
        else if (fusedFresh && dashLegacySmartLimitValid(fusedSpeedLimitRaw))
        {
            limitRaw = fusedSpeedLimitRaw;
            legacySpeedDiag.limitSource = LegacySpeedLimitSource::Fused399;
        }

        legacySpeedDiag.result = legacySmartOffsetEngine.compute(
            legacySmartOffsetConfig,
            limitRaw,
            nowMs,
            (bool)APActive || (bool)ADEnabled);
        uint8_t offset = legacySpeedDiag.result.outputOffsetKph;
        legacySpeedDiag.outputOffsetPct = legacySpeedDiag.result.offsetPct;
        legacySpeedDiag.blockedReason = offset > 0 ? "ready" : legacySpeedDiag.result.blockedReason;
        speedOffset = offset;
        return offset;
    }

    void resetBionic(uint32_t seed) override
    {
        (void)seed;
        legacyNagV4.configure((bool)bionicSteering && (bool)nagKillerRuntime);
        legacyNagV4.resetRuntime();
        legacyNagBus.reset();
        legacyNagAutoDisarmed = false;
        legacyNagState.runtimeActive = false;
        legacyNagState.autoDisarmed = false;
        legacyNagState.degraded = false;
        legacyNagState.consecutiveTxFailures = 0;
        legacyNagState.consecutiveCounterCollisions = 0;
        legacyNagState.state = legacyNagV4.state;
        legacyNagState.selectedBus = CAN_BUS_ANY;
        legacyNagState.blocked = LegacyNagBlockReason::Disabled;
        memset(legacyNagLastTxData, 0, sizeof(legacyNagLastTxData));
        legacyNagLastTxDlc = 0;
        legacyNagLastTxBus = CAN_BUS_ANY;
        legacyNagHasTxFingerprint = false;
    }

    bool bionicDisabled() const override { return legacyNagAutoDisarmed; }

    DashLegacyNagDiag legacyNagDiag(uint32_t nowMs) const override
    {
        DashLegacyNagDiag result = legacyNagState;
        result.selectedBus = legacyNagBus.selectedBus;
        result.gear = (uint8_t)currentGear;
        result.state = legacyNagAutoDisarmed ? LegacyNagV4State::Degraded : legacyNagV4.state;
        result.autoDisarmed = legacyNagAutoDisarmed;
        result.degraded = legacyNagAutoDisarmed;
        result.dasHandsState = legacyNagV4.handsOnState;
        result.state1EnterCount = legacyNagV4.state1EnterCount;
        result.state1HoldCount = legacyNagV4.state1HoldCount;
        result.state1FallbackCount = legacyNagV4.state1FallbackCount;
        result.faithfulWalkSteps = legacyNagV4.faithfulWalkSteps;
        result.directionChangeCount = legacyNagV4.directionChangeCount;
        result.targetTorqueCentiNm = legacyNagV4.targetTorqueCentiNm;
        result.steeringSeen = legacyNagV4.steeringSeen;
        result.steeringControlType = legacyNagV4.steeringControlType;
        result.steeringAngleX10 = legacyNagV4.steeringAngleX10;
        result.steeringBus = legacyNagV4.steeringBus;
        result.steeringAgeMs = legacyNagV4.steeringAgeMs(nowMs);
        result.episodeDirection = legacyNagV4.episodeDirection;
        result.currentSilentMs = result.lastTxMs ? nowMs - result.lastTxMs : 0;
        return result;
    }

    bool legacyNagConfigured() const
    {
        return (bool)bionicSteering && (bool)nagKillerRuntime && !legacyNagAutoDisarmed;
    }

    static bool legacyNagApContextActive(uint8_t apState)
    {
        // Real Legacy captures report 0x399 status 6 throughout active FSD.
        // Keep this rule local so HW3/HW4 AP semantics remain unchanged.
        return apState >= 3 && apState <= 6;
    }

    bool legacyNagRuntimeAllowed(uint32_t nowMs)
    {
        if (legacyNagAutoDisarmed)
        {
            legacyNagState.blocked = LegacyNagBlockReason::Degraded;
            return false;
        }
        if (!legacyNagConfigured())
        {
            legacyNagState.blocked = LegacyNagBlockReason::Disabled;
            return false;
        }
        if (!legacyNagApContextActive(legacyNagState.apState))
        {
            legacyNagState.blocked = LegacyNagBlockReason::ApInactive;
            return false;
        }
        if (legacyNagState.lastContextMs == 0 || nowMs - legacyNagState.lastContextMs > kNagContextStaleMs)
        {
            legacyNagState.blocked = LegacyNagBlockReason::ContextStale;
            return false;
        }
        return true;
    }

    static uint8_t legacyNagChecksum(const CanFrame &frame)
    {
        uint16_t sum = 0;
        for (uint8_t i = 0; i < 7; i++)
            sum += frame.data[i];
        return static_cast<uint8_t>((sum + 0x73) & 0xFF);
    }

    static LegacyNagBlockReason legacyNagV4BlockReason(LegacyNagV4Block reason)
    {
        switch (reason)
        {
        case LegacyNagV4Block::Satisfied: return LegacyNagBlockReason::DasSatisfied;
        case LegacyNagV4Block::StateDelay: return LegacyNagBlockReason::StateDelay;
        case LegacyNagV4Block::StateStopped: return LegacyNagBlockReason::StateStopped;
        case LegacyNagV4Block::SteeringMissing: return LegacyNagBlockReason::SteeringMissing;
        case LegacyNagV4Block::SteeringStale: return LegacyNagBlockReason::SteeringStale;
        case LegacyNagV4Block::SteeringInvalid: return LegacyNagBlockReason::SteeringInvalid;
        case LegacyNagV4Block::CenterNoDirection: return LegacyNagBlockReason::CenterNoDirection;
        default: return LegacyNagBlockReason::None;
        }
    }

    void handleLegacyNag370(const CanFrame &frame, CanDriver &driver)
    {
        uint32_t nowMs = dashDiagNowMs();
        legacyNagV4.configure((bool)bionicSteering && (bool)nagKillerRuntime && !legacyNagAutoDisarmed);
        legacyNagState.rx370++;
        legacyNagState.lastRxMs = nowMs;
        if (frame.dlc < 8)
        {
            legacyNagState.blocked = LegacyNagBlockReason::DlcShort;
            return;
        }

        uint8_t counter = static_cast<uint8_t>(frame.data[6] & 0x0F);
        if (!legacyNagBus.accept(frame.bus, counter, nowMs))
        {
            legacyNagState.blocked = legacyNagBus.selectedBus == CAN_BUS_ANY
                ? LegacyNagBlockReason::BusLearning
                : LegacyNagBlockReason::BusMismatch;
            return;
        }
        legacyNagState.selectedBus = legacyNagBus.selectedBus;

        uint8_t targetHandsOn = static_cast<uint8_t>((frame.data[4] >> 6) & 0x03);
        legacyNagState.nativeHandsOnLevel = targetHandsOn;
        uint32_t txAgeMs = legacyNagState.lastTxMs ? nowMs - legacyNagState.lastTxMs : 0;
        bool recentMatchingCounter = legacyNagHasTxFingerprint && legacyNagState.lastTxMs &&
                                     counter == legacyNagState.txCounter &&
                                     txAgeMs <= kNagOwnEchoWindowMs;
        if (recentMatchingCounter && frame.bus == legacyNagLastTxBus &&
            frame.dlc == legacyNagLastTxDlc &&
            memcmp(frame.data, legacyNagLastTxData, frame.dlc) == 0)
        {
            legacyNagState.duplicateCounterObserved++;
            legacyNagState.runtimeActive = false;
            legacyNagState.blocked = LegacyNagBlockReason::OwnEcho;
            return;
        }
        if (recentMatchingCounter && frame.bus == legacyNagLastTxBus &&
            txAgeMs >= kNagNativeFollowerMinMs &&
            legacyNagChecksum(frame) == frame.data[7])
        {
            // In the observed Legacy stream the module sends counter n+1
            // immediately after native n. The vehicle then emits its own n+1
            // frame about 39-40 ms later. It is a native follower, not loopback;
            // let it drive the next overlay so the path remains continuous.
            legacyNagState.expectedNativeFollowerObserved++;
            legacyNagState.lastNativeFollowerDelayMs = txAgeMs;
            legacyNagState.consecutiveCounterCollisions = 0;
        }
        else if (recentMatchingCounter && frame.bus == legacyNagLastTxBus)
        {
            legacyNagState.counterCollisionObserved++;
            if (nowMs - legacyNagState.lastTxMs > 1000)
                legacyNagState.consecutiveCounterCollisions = 0;
            if (legacyNagState.consecutiveCounterCollisions < 0xFF)
                legacyNagState.consecutiveCounterCollisions++;
            legacyNagState.runtimeActive = false;
            if (legacyNagState.consecutiveCounterCollisions >= kNagMaxCounterCollisions)
            {
                legacyNagAutoDisarmed = true;
                legacyNagV4.configure(false);
                legacyNagState.autoDisarmed = true;
                legacyNagState.degraded = true;
                legacyNagState.state = LegacyNagV4State::Degraded;
                legacyNagState.blocked = LegacyNagBlockReason::Degraded;
            }
            else
                legacyNagState.blocked = LegacyNagBlockReason::CounterCollision;
            return;
        }
        if (targetHandsOn == 1)
        {
            legacyNagState.targetHandsBlocked++;
            legacyNagState.blocked = LegacyNagBlockReason::TargetHandsOn;
            return;
        }
        if (!abortGuard.allowsInjection())
        {
            abortGuard.recordBlock(DashAbortGuardBlockPath::LegacyNag370);
            legacyNagState.runtimeActive = false;
            legacyNagState.blocked = LegacyNagBlockReason::AbortGuard;
            return;
        }

        bool gatesAllowed = legacyNagRuntimeAllowed(nowMs);
        DashLegacyNagV4Decision decision = legacyNagV4.decide(nowMs, gatesAllowed);
        legacyNagState.state = decision.state;
        legacyNagState.dasHandsState = legacyNagV4.handsOnState;
        if (!decision.send)
        {
            legacyNagState.runtimeActive = false;
            LegacyNagBlockReason decisionBlock = legacyNagV4BlockReason(decision.blocked);
            if (decisionBlock != LegacyNagBlockReason::None)
                legacyNagState.blocked = decisionBlock;
            else if (decision.state == LegacyNagV4State::Detected)
                legacyNagState.blocked = LegacyNagBlockReason::Detected;
            return;
        }

        CanFrame echo = frame;
        DashLegacyNagV4::applyTorque(echo.data[2], echo.data[3], decision.torqueCentiNm);
        // EPAS-faithful: preserve the complete native byte 4. Ground-truth
        // captures keep handsOnLevel at zero even when hands are detected.
        echo.data[4] = frame.data[4];
        echo.data[6] = static_cast<uint8_t>((frame.data[6] & 0xF0) | ((counter + 1) & 0x0F));
        echo.data[7] = legacyNagChecksum(echo);

        legacyNagState.eligible370++;
        legacyNagState.txAttempts++;
        legacyNagState.requestedTorqueCentiNm = decision.torqueCentiNm;
        legacyNagState.requestedTorqueRaw = DashLegacyNagV4::torqueRaw(decision.torqueCentiNm);
        legacyNagState.txCounter = static_cast<uint8_t>(echo.data[6] & 0x0F);
        legacyNagState.txChecksum = echo.data[7];
        if (legacyNagState.lastTxMs)
        {
            uint32_t silentMs = nowMs - legacyNagState.lastTxMs;
            if (silentMs > legacyNagState.maxSilentMs)
                legacyNagState.maxSilentMs = silentMs;
        }
        framesSent++;
        bool ok = driver.send(echo);
        if (!ok)
        {
            legacyNagState.txRetryAttempts++;
            framesSent++;
            ok = driver.send(echo);
            if (ok)
                legacyNagState.txRetryOk++;
        }
        legacyNagState.runtimeActive = ok;
        legacyNagState.lastTxMs = ok ? nowMs : legacyNagState.lastTxMs;
        if (ok)
        {
            legacyNagState.tx370++;
            legacyNagState.consecutiveTxFailures = 0;
            legacyNagState.blocked = LegacyNagBlockReason::None;
            memcpy(legacyNagLastTxData, echo.data, echo.dlc);
            legacyNagLastTxDlc = echo.dlc;
            legacyNagLastTxBus = echo.bus;
            legacyNagHasTxFingerprint = true;
        }
        else
        {
            legacyNagState.txFail++;
            if (legacyNagState.consecutiveTxFailures < 0xFF)
                legacyNagState.consecutiveTxFailures++;
            if (legacyNagState.consecutiveTxFailures >= kNagMaxConsecutiveTxFailures)
            {
                legacyNagAutoDisarmed = true;
                legacyNagV4.configure(false);
                legacyNagState.autoDisarmed = true;
                legacyNagState.degraded = true;
                legacyNagState.state = LegacyNagV4State::Degraded;
                legacyNagState.blocked = LegacyNagBlockReason::Degraded;
            }
            else
                legacyNagState.blocked = LegacyNagBlockReason::TxFailed;
        }
    }

    const uint32_t *filterIds() const override
    {
        // 1080 added for UI_driverAssistAnonDebugParams visionSpeedSlider override.
        // 920 added for auto hardware detection (GTW_carConfig).
        // 880 (0x370 EPAS3P_sysStatus) added for EPAS-faithful nag engine.
        // 962 (0x3C2) is admitted on CAN1 as receive-only evidence so the
        // recorder can locate the native wheel stream on either T-2CAN input.
        // 1160 (0x488 DAS_steeringControl) is the receive-only direction source
        // for the Legacy one-sided Mode-C torque engine.
        static constexpr uint32_t ids[] = {69, 280, 390, 760, 880, 920, 921, 962, 1006, 1080, 1160, CAN_ID_OTA_STATUS};
        return ids;
    }
    uint8_t filterIdCount() const override { return 12; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        updateHwDetectedFrom920(frame);
        if (frame.id == 1160)
        {
            if (frame.dlc < 3)
                return;
            uint8_t controlType = static_cast<uint8_t>((frame.data[2] >> 6) & 0x03);
            uint16_t rawAngle = static_cast<uint16_t>(((frame.data[0] & 0x7F) << 8) |
                                                       frame.data[1]);
            int16_t angleX10 = static_cast<int16_t>(static_cast<int32_t>(rawAngle) - 16384);
            uint32_t nowMs = dashDiagNowMs();
            legacyNagV4.onSteeringRequest(controlType, angleX10, frame.bus, nowMs);
            legacyNagState.rx488++;
            return;
        }
        if (frame.id == 880)
        {
            handleLegacyNag370(frame, driver);
            return;
        }
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
        // UI_gpsVehicleSpeed (0x2F8 = 760) is a read-only map speed-limit
        // source. The actual Legacy offset request is applied to 0x3EE mux 0.
        if (frame.id == 760)
        {
            if (frame.dlc < 7)
                return;
            uint32_t nowMs = dashDiagNowMs();
            legacySpeedDiag.gpsSpeedSeen = true;
            legacySpeedDiag.gpsSpeedFresh = true;
            legacySpeedDiag.gpsUserOffsetRaw = frame.data[5] & 0x3F;
            legacySpeedDiag.gpsUserOffsetKph = static_cast<int>(legacySpeedDiag.gpsUserOffsetRaw) - 30;
            legacySpeedDiag.gpsMppLimitRaw = frame.data[6] & 0x1F;
            legacySpeedDiag.gpsMppLimitKph = static_cast<uint16_t>(legacySpeedDiag.gpsMppLimitRaw) * 5U;
            if (legacySpeedDiag.gpsSpeedLastMs != 0)
                legacySpeedDiag.gpsSpeedPeriodMs = nowMs - legacySpeedDiag.gpsSpeedLastMs;
            legacySpeedDiag.gpsSpeedLastMs = nowMs;
            legacySpeedDiag.gpsSpeedCount++;
            legacySpeedDiag.blockedReason = "waiting3ee";
            return;
        }
        // 0x438 (1080) — UI_driverAssistAnonDebugParams: visionSpeedSlider = 100
        if (frame.id == 1080)
        {
            if (frame.dlc < 8) return;
            if (!overrideSpeedLimit) return;
            if (checkAD && !checkAD()) return;
            if (!abortGuard.allowsInjection())
            {
                abortGuard.recordBlock(DashAbortGuardBlockPath::LegacyVisionSlider0x438);
                return;
            }
            legacyFsdDiag.aux1080.recordBefore(frame.data);
            frame.data[7] = (frame.data[7] & 0x80) | 100;
            legacyFsdDiag.aux1080.recordAfter(frame.data);
            legacyFsdDiag.aux1080.recordPath(legacyDiagBus(frame), legacyDiagDriver(frame));
            framesSent++;
            bool ok = driver.send(frame);
            legacyFsdDiag.recordMuxTx(legacyFsdDiag.aux1080, ok, dashDiagNowMs());
            if (onSend) onSend(0, ok);
            return;
        }
        if (handleDISystemStatus(frame)) return;
        if (handleDIVehicleStatus(frame)) return;
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            uint8_t apState = readDASAutopilotStatus(frame);
            const uint32_t nowMs = dashDiagNowMs();
            abortGuard.onApState(apState, nowMs);
            APActive = legacyNagApContextActive(apState);
            legacyNagState.apState = apState;
            legacyNagState.lastContextMs = nowMs;
            if (frame.dlc >= 2)
            {
                fusedSpeedLimitRaw = static_cast<uint8_t>(frame.data[1] & 0x1F);
                legacySpeedDiag.fusedLimitLastMs = legacyNagState.lastContextMs;
            }
            if (frame.dlc >= 6)
            {
                uint8_t handsOn = static_cast<uint8_t>((frame.data[5] >> 2) & 0x0F);
                legacyNagV4.configure((bool)bionicSteering && (bool)nagKillerRuntime && !legacyNagAutoDisarmed);
                legacyNagV4.onContext(legacyNagApContextActive(apState), handsOn, legacyNagState.lastContextMs);
                legacyNagState.dasHandsState = handsOn;
                legacyNagState.state = legacyNagV4.state;
                legacyNagState.runtimeActive = false;
            }
            return;
        }
        // 0x3EE (1006) — FSD activation frame (mux 0/1)
        if (frame.id == 1006)
        {
            uint32_t nowMs = dashDiagNowMs();
            if (frame.dlc < 8)
            {
                legacyFsdDiag.mux0.lastSkip = FsdSkipReason::DlcShort;
                return;
            }

            auto index = readMuxID(frame);
            if (index == 0)
            {
                legacySpeedDiag.mux0Count++;
                legacySpeedDiag.runtimeActive = false;
                legacyFsdDiag.mux0.recordRx(nowMs);
                legacyFsdDiag.markMux0Rx(nowMs);
                legacyFsdDiag.mux0.recordPath(legacyDiagBus(frame), legacyDiagDriver(frame));
                bool forced = (bool)forceActivateRuntime;
                bool uiSelected = isFSDSelectedInUI(frame);
                fsdTriggered = forced || uiSelected;
                ADEnabled = (bool)fsdTriggered;
                legacyFsdDiag.triggered = (bool)fsdTriggered;
                legacyFsdDiag.forceRuntime = forced;
                legacyFsdDiag.triggerSource = forced ? FsdTriggerSource::Force : (uiSelected ? FsdTriggerSource::UiBit : FsdTriggerSource::FalseSource);

                // v1.19 JITTER: cache every native mux0 frame BEFORE any gate
                // below can return — the one-shot template must stay fresh
                // even when the gates swallow the frame.
                if (jitterObserve3ee)
                    jitterObserve3ee(frame.data, frame.bus, nowMs);

                const bool speedRequested = legacySpeedOffsetRequested();
                const bool speedBusAllowed = speedRequested && legacySpeedBus.accept(frame, nowMs);
                if (speedRequested && legacySpeedBus.ambiguous)
                    legacySpeedDiag.blockedReason = "busAmbiguous";
                else if (speedRequested && !speedBusAllowed)
                    legacySpeedDiag.blockedReason = legacySpeedBus.selectedBus == CAN_BUS_ANY
                                                        ? "busLearning"
                                                        : "busMismatch";

                if (!(bool)fsdTriggered && !speedRequested)
                {
                    legacyFsdDiag.mux0.lastSkip = FsdSkipReason::NotTriggered;
                    legacyFsdDiag.health = FsdHealthState::NotTriggered;
                    return;
                }
                if (!abortGuard.allowsInjection())
                {
                    abortGuard.recordBlock(DashAbortGuardBlockPath::LegacyFsdMux0);
                    if ((bool)fsdTriggered)
                    {
                        legacyFsdDiag.mux0.lastSkip = FsdSkipReason::GateBlocked;
                        legacyFsdDiag.health = FsdHealthState::GateBlocked;
                        legacyFsdDiag.lastBlockedBy = FsdGateBlockReason::AbortGuard;
                    }
                    if (speedRequested)
                        legacySpeedDiag.blockedReason = "abortGuard";
                    return;
                }
                // v1.19 JITTER pause (replaces the v1.18 barrier A): while the
                // procedure runs, ALL device 0x3EE mux0 writes stop — the
                // one-shot owns bit46. The vehicle's own frame passes untouched.
                if (jitterInhibit3ee && jitterInhibit3ee())
                {
                    legacyFsdDiag.mux0.lastSkip = FsdSkipReason::GateBlocked;
                    legacyFsdDiag.health = FsdHealthState::GateBlocked;
                    legacyFsdDiag.lastBlockedBy = FsdGateBlockReason::LegacyFsdSettle;
                    if (speedRequested)
                        legacySpeedDiag.blockedReason = "jitterProcedure";
                    return;
                }
                bool activationAllowed = false;
                if ((bool)fsdTriggered)
                {
                    activationAllowed = legacyFsdActivationAllowed
                                            ? legacyFsdActivationAllowed(nowMs)
                                            : injectionAllowed();
                    if (!activationAllowed && !speedRequested)
                    {
                        legacyFsdDiag.mux0.lastSkip = FsdSkipReason::GateBlocked;
                        legacyFsdDiag.health = FsdHealthState::GateBlocked;
                        // Dashboard installs a resolver so an optional strict
                        // Legacy AP gate is reported as ApGate. Keep the
                        // historical settle reason for standalone callbacks
                        // that do not supply a reason resolver.
                        legacyFsdDiag.lastBlockedBy = gateBlockReason
                                                          ? gateBlockReason()
                                                          : (legacyFsdActivationAllowed
                                                                 ? FsdGateBlockReason::LegacyFsdSettle
                                                                 : currentGateBlockReason());
                        return;
                    }
                }

                bool speedAllowed = speedRequested && speedBusAllowed && injectionAllowed();
                if (speedRequested && speedBusAllowed && !speedAllowed)
                    legacySpeedDiag.blockedReason = "gateBlocked";

                // Plugin owns FSD activation frames: suppress built-in injection.
                if (!builtInFsdInjectionAllowed())
                {
                    legacyFsdDiag.mux0.lastSkip = FsdSkipReason::GateBlocked;
                    legacyFsdDiag.health = FsdHealthState::GateBlocked;
                    legacySpeedDiag.blockedReason = "pluginOwner";
                    return;
                }

                legacyFsdDiag.mux0.recordBefore(frame.data);
                if (activationAllowed)
                    setBit(frame, 46, true);
                uint8_t effectiveOffset = speedAllowed ? computeLegacySpeedOffset(nowMs) : 0;
                if (effectiveOffset > 0)
                    dashWriteLegacyOffsetTo3eeMux0(frame.data,
                                                   effectiveOffset,
                                                   legacySpeedDiag.outputOffsetPct);
                if (activationAllowed &&
                    (legacyFsdDiag.policy == LegacyFsdPolicy::TeslaParity ||
                    (legacyFsdDiag.policy == LegacyFsdPolicy::Experimental && legacyFsdDiag.profileWriteEnable))
                   )
                    setSpeedProfileV12V13(frame, speedProfile);

                if (!activationAllowed && effectiveOffset == 0)
                    return;
                legacySpeedDiag.mux0RxByte3 = legacyFsdDiag.mux0.before[3];
                legacySpeedDiag.mux0TxByte3 = frame.data[3];
                legacySpeedDiag.mux0RxByte5 = legacyFsdDiag.mux0.before[5];
                legacySpeedDiag.mux0TxByte5 = frame.data[5];
                legacySpeedDiag.mux0RxByte7 = legacyFsdDiag.mux0.before[7];
                legacySpeedDiag.mux0TxByte7 = frame.data[7];
                legacyFsdDiag.mux0.recordAfter(frame.data);
                framesSent++;
                bool ok = driver.send(frame);
                if (effectiveOffset > 0)
                {
                    legacySpeedDiag.lastSentOffsetRaw = static_cast<uint8_t>(effectiveOffset + 30U);
                    legacySpeedDiag.lastSentOffsetKph = effectiveOffset;
                    legacySpeedDiag.runtimeActive = ok;
                    if (ok)
                    {
                        legacySpeedDiag.txOk++;
                        legacySpeedDiag.blockedReason = "none";
                    }
                    else
                    {
                        legacySpeedDiag.txFail++;
                        legacySpeedDiag.blockedReason = "txFail";
                    }
                }
                if (activationAllowed)
                {
                    legacyFsdDiag.recordMuxTx(legacyFsdDiag.mux0, ok, nowMs);
                    legacyFsdDiag.markTxResult(ok, nowMs);
                }
                else if (ok)
                    legacySpeedDiag.offsetOnlyTxOk++;
                else
                    legacySpeedDiag.offsetOnlyTxFail++;
                if (onSend) onSend(0, ok);
            }
            else if (index == 1)
            {
                legacyFsdDiag.mux1.recordRx(nowMs);
                legacyFsdDiag.markMux1Rx(nowMs);
                legacyFsdDiag.mux1.recordPath(legacyDiagBus(frame), legacyDiagDriver(frame));
                if (legacyFsdDiag.policy == LegacyFsdPolicy::Stable)
                {
                    legacyFsdDiag.mux1.lastSkip = FsdSkipReason::DisabledInStable;
                    return;
                }
                if (legacyFsdDiag.policy != LegacyFsdPolicy::TeslaParity && !legacyFsdDiag.mux1Enable)
                {
                    legacyFsdDiag.mux1.lastSkip = FsdSkipReason::DisabledByPolicy;
                    return;
                }
                if (!(bool)fsdTriggered)
                {
                    legacyFsdDiag.mux1.lastSkip = FsdSkipReason::NotTriggered;
                    legacyFsdDiag.health = FsdHealthState::NotTriggered;
                    return;
                }
                if (!abortGuard.allowsInjection())
                {
                    abortGuard.recordBlock(DashAbortGuardBlockPath::LegacyFsdMux1);
                    legacyFsdDiag.mux1.lastSkip = FsdSkipReason::GateBlocked;
                    legacyFsdDiag.health = FsdHealthState::GateBlocked;
                    legacyFsdDiag.lastBlockedBy = FsdGateBlockReason::AbortGuard;
                    return;
                }
                // The JITTER pause covers mux1 device writes too (see the mux0 branch).
                if (jitterInhibit3ee && jitterInhibit3ee())
                {
                    legacyFsdDiag.mux1.lastSkip = FsdSkipReason::GateBlocked;
                    legacyFsdDiag.health = FsdHealthState::GateBlocked;
                    legacyFsdDiag.lastBlockedBy = FsdGateBlockReason::LegacyFsdSettle;
                    return;
                }
                if (!injectionAllowed())
                {
                    legacyFsdDiag.mux1.lastSkip = FsdSkipReason::GateBlocked;
                    legacyFsdDiag.health = FsdHealthState::GateBlocked;
                    legacyFsdDiag.lastBlockedBy = currentGateBlockReason();
                    return;
                }

                // Plugin owns FSD activation frames: suppress built-in injection.
                if (!builtInFsdInjectionAllowed())
                {
                    legacyFsdDiag.mux1.lastSkip = FsdSkipReason::GateBlocked;
                    legacyFsdDiag.health = FsdHealthState::GateBlocked;
                    return;
                }

                legacyFsdDiag.mux1.recordBefore(frame.data);
                setBit(frame, 19, false);
                if (legacyFsdDiag.visionLimitClearEnable)
                    setBit(frame, 48, false);
                legacyFsdDiag.mux1.recordAfter(frame.data);
                framesSent++;
                bool ok = driver.send(frame);
                legacyFsdDiag.recordMuxTx(legacyFsdDiag.mux1, ok, nowMs);
                legacyFsdDiag.markTxResult(ok, nowMs);
                if (onSend) onSend(1, ok);
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
        // 880 (0x370 EPAS3P_sysStatus) + 923 (0x39B DAS_status Highland/HW4) added for EPAS-faithful nag engine.
        static constexpr uint32_t ids[] = {280, 390, 880, 920, 921, 923, 962, 1016, 1021, 2047, CAN_ID_OTA_STATUS};
        return ids;
    }
    uint8_t filterIdCount() const override { return 11; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        updateHwDetectedFrom920(frame);
        if (handleDISystemStatus(frame)) return;
        if (handleDIVehicleStatus(frame)) return;
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
            // ISA chime suppress — runtime gate (spec Task 2, 对齐 HW4Handler:856-864)
            if ((bool)isaChimeSuppress && frame.dlc >= 8 && injectionAllowed())
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
            handleBanShield(frame, driver);
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
                fsdTriggered = fsdRequested;
                ADEnabled = (bool)fsdTriggered;
            }
            if (index == 0 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
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

            // ── Mux 1: Nag suppression (bit-19 clear, legacy baseline) ──
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
            {
                setBit(frame, 19, false);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(1, true);
            }

            // ── Mux 2: Speed offset (three-layer + slew limiter) ──────────
            if (index == 2 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
            {
                uint8_t stockRaw = static_cast<uint8_t>(((frame.data[0] >> 6) & 0x03) |
                                                        ((frame.data[1] & 0x3F) << 2));
                uint8_t fl = fusedSpeedLimitRaw;
                int computeInput = (fl == 0 || fl == 31) ? (int)stockRaw : (int)speedOffset;
                uint8_t activeRaw = dashComputeHw3OffsetRaw(computeInput);

                if (fl == 0 || fl == 31)
                {
                    hw3OffsetTargetRaw = 0;
                    return;
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
                            hw3OffsetSlewCount = hw3OffsetSlewCount + 1;
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
    DashBionicSteer bionic;  // bionic steering state

    bool bionicDisabled() const override { return bionic.isDisabled(); }
    void resetBionic(uint32_t seed) override
    {
        bionic.reset();
        bionic.init(seed ? seed : 0xDEADBEEF);
    }

    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {880, 920, 962, CAN_ID_OTA_STATUS};
        return ids;
    }
    uint8_t filterIdCount() const override { return 4; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        updateHwDetectedFrom920(frame);
        if (frame.id != 880 || frame.dlc < 8)
            return;

        uint8_t handsOn = (frame.data[4] >> 6) & 0x03;

        if (!nagKillerActive || !nagKillerRuntime || handsOn != 0)
            return;
        if (checkAD && !checkAD())
            return;

        // Read bionic steering mode from handler member (set by dashboard)
        bool useBionic = (bool)bionicSteering && !bionic.isDisabled();

        CanFrame echo;
        echo.id = 880;
        echo.dlc = 8;

        echo.data[0] = frame.data[0];
        echo.data[1] = frame.data[1];
        echo.data[2] = (frame.data[2] & 0xF0) | 0x08;
        echo.data[5] = frame.data[5];

        if (useBionic)
        {
            // ── Bionic sine-wave torque path ──────────────────
            if (bionic.needsNewPhase())
                bionic.beginPhase();

            int pert = bionic.computePerturbation();
            uint8_t d2lo = 0x08;
            uint8_t d3   = 0xB6;
            bionic.applyToFrame(d2lo, d3, pert);
            echo.data[2] = (frame.data[2] & 0xF0) | d2lo;
            echo.data[3] = d3;
        }
        else
        {
            // ── Legacy fixed torque = 1.80 Nm (tRaw = 0x08B6) ─
            echo.data[3] = 0xB6;
        }

        // handsOnLevel = 1
        echo.data[4] = frame.data[4] | 0x40;

        // Counter + 1
        uint8_t cnt = (frame.data[6] & 0x0F);
        cnt = (cnt + 1) & 0x0F;
        echo.data[6] = (frame.data[6] & 0xF0) | cnt;

        // Checksum: sum(byte0..byte6) + 0x73
        uint16_t sum = echo.data[0] + echo.data[1] + echo.data[2] + echo.data[3] + echo.data[4] + echo.data[5] + echo.data[6];
        echo.data[7] = static_cast<uint8_t>((sum + 0x73) & 0xFF);

        // Verify checksum (sanity check for bionic mode)
        if (useBionic)
        {
            uint16_t verify = echo.data[0] + echo.data[1] + echo.data[2] +
                              echo.data[3] + echo.data[4] + echo.data[5] +
                              echo.data[6] + 0x73;
            uint8_t expectedCs = static_cast<uint8_t>(verify & 0xFF);
            if (echo.data[7] != expectedCs)
            {
                bionic.reportFailure();
                // Fall back to legacy echo on checksum mismatch
                echo.data[2] = (frame.data[2] & 0xF0) | 0x08;
                echo.data[3] = 0xB6;
                sum = echo.data[0] + echo.data[1] + echo.data[2] +
                      echo.data[3] + echo.data[4] + echo.data[5] + echo.data[6];
                echo.data[7] = static_cast<uint8_t>((sum + 0x73) & 0xFF);
            }
            else
            {
                bionic.reportSuccess();
            }
        }

        framesSent++;
        nagEchoCount++;
        driver.send(echo);

        if (enablePrint && (nagEchoCount % 500 == 1))
        {
            char buf[LogRingBuffer::kMaxMsgLen];
            snprintf(buf, sizeof(buf),
                     "NagHandler: echo=%u bionic=%s",
                     (unsigned int)(uint32_t)nagEchoCount,
                     useBionic ? "ON" : "off");
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
        // 880 (0x370 EPAS3P_sysStatus) + 923 (0x39B DAS_status Highland/HW4) added for EPAS-faithful nag engine.
        static constexpr uint32_t ids[] = {280, 390, 880, 920, 921, 923, 962, 1016, 1021, 2047, CAN_ID_OTA_STATUS};
        return ids;
    }
    uint8_t filterIdCount() const override { return 11; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        updateHwDetectedFrom920(frame);
        if (handleDISystemStatus(frame)) return;
        if (handleDIVehicleStatus(frame)) return;
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            APActive = isDASAutopilotActive(readDASAutopilotStatus(frame));
            if (frame.dlc >= 2)
                fusedSpeedLimitRaw = static_cast<uint8_t>(frame.data[1] & 0x1F);
            // ISA chime suppress — runtime gate (all build modes)
            if ((bool)isaChimeSuppress && frame.dlc >= 8 && injectionAllowed())
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
        // ISA override — rewrite fused/vision speed limit to NONE (spec Task 3, 对齐 tesla handleDASStatusISAOverride)
        if (frame.id == 923 && frame.dlc >= 8 &&
            (bool)isaOverride && injectionAllowed())
        {
            frame.data[1] |= 0x1F;   // DAS_fusedSpeedLimit = 31 (NONE)
            frame.data[2] |= 0x1F;   // DAS_visionOnlySpeedLimit = 31 (NONE)
            frame.data[7] = computeVehicleChecksum(frame);
            framesSent++;
            driver.send(frame);
            if (onSend) onSend(0, true);
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
            handleBanShield(frame, driver);
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
                fsdTriggered = fsdRequested;
                ADEnabled = (bool)fsdTriggered;
            }
            if (index == 0 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
            {
                setBit(frame, 46, true);
                setBit(frame, 60, true);
                if ((bool)emergencyVehicleDetection) setBit(frame, 59, true);
                if ((bool)tlsscBypass) setBit(frame, 38, true);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(0, true);
            }

            // Mux 1: FSD-ready signal (bit 47) + nag suppression (bit 19).
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
            {
                setBit(frame, 47, true);
                setBit(frame, 19, false);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(1, true);
            }

            // Mux 2: Speed profile + offset
            if (index == 2 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
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
