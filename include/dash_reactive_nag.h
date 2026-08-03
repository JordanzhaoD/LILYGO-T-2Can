#pragma once

// Legacy Reactive NAG v2. Pure timing/state logic; the caller owns CAN routing.

#include <cmath>
#include <cstdint>
#include "can_frame_types.h"
#include "dash_legacy_nag_v4.h"

struct DashReactivePRNG
{
    uint32_t state{0xDEADBEEFu};

    void seed(uint32_t value) { state = value ? value : 0xDEADBEEFu; }

    uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    uint32_t range(uint32_t low, uint32_t high)
    {
        return high < low ? low : low + (next() % (high - low + 1));
    }
};

enum class NagMode : uint8_t
{
    Idle = 0,
    Proactive = 1,
    Reactive = 2,
};

struct DashReactiveDiag
{
    bool enabled{false};
    NagMode mode{NagMode::Idle};
    bool injecting{false};
    uint8_t lastHandsOnState{0};
    int currentAmplitude{0};
    uint32_t nagSamples{0};
    uint32_t reactiveBursts{0};
    uint32_t proactiveWiggles{0};
    uint32_t echoSent{0};
    uint32_t echoFailed{0};
    unsigned long nextProactiveInMs{0};
};

enum class LegacyNagBlockReason : uint8_t
{
    None = 0,
    Disabled = 1,
    NotLegacy = 2,
    NotInDrive = 3,
    ApInactive = 4,
    ContextStale = 5,
    BusLearning = 6,
    BusMismatch = 7,
    TargetHandsOn = 8,
    CheckAd = 9,
    DlcShort = 10,
    TxFailed = 11,
    Detected = 12,
    Degraded = 13,
    OwnEcho = 14,
    CounterCollision = 15,
    DasSatisfied = 16,
    SteeringMissing = 17,
    SteeringStale = 18,
    SteeringInvalid = 19,
    CenterNoDirection = 20,
    StateDelay = 21,
    StateStopped = 22,
    AbortGuard = 23,
};

static inline const char *legacyNagBlockReasonName(LegacyNagBlockReason reason)
{
    switch (reason)
    {
    case LegacyNagBlockReason::None: return "none";
    case LegacyNagBlockReason::Disabled: return "disabled";
    case LegacyNagBlockReason::NotLegacy: return "not_legacy";
    case LegacyNagBlockReason::NotInDrive: return "not_in_drive";
    case LegacyNagBlockReason::ApInactive: return "ap_inactive";
    case LegacyNagBlockReason::ContextStale: return "context_stale";
    case LegacyNagBlockReason::BusLearning: return "bus_learning";
    case LegacyNagBlockReason::BusMismatch: return "bus_mismatch";
    case LegacyNagBlockReason::TargetHandsOn: return "target_hands_on";
    case LegacyNagBlockReason::CheckAd: return "check_ad";
    case LegacyNagBlockReason::DlcShort: return "dlc_short";
    case LegacyNagBlockReason::TxFailed: return "tx_failed";
    case LegacyNagBlockReason::Detected: return "detected";
    case LegacyNagBlockReason::Degraded: return "degraded";
    case LegacyNagBlockReason::OwnEcho: return "own_echo";
    case LegacyNagBlockReason::CounterCollision: return "counter_collision";
    case LegacyNagBlockReason::DasSatisfied: return "das_satisfied";
    case LegacyNagBlockReason::SteeringMissing: return "steering_missing";
    case LegacyNagBlockReason::SteeringStale: return "steering_stale";
    case LegacyNagBlockReason::SteeringInvalid: return "steering_invalid";
    case LegacyNagBlockReason::CenterNoDirection: return "center_no_direction";
    case LegacyNagBlockReason::StateDelay: return "state_delay";
    case LegacyNagBlockReason::StateStopped: return "state_stopped";
    case LegacyNagBlockReason::AbortGuard: return "abort_guard";
    }
    return "unknown";
}

struct DashLegacyNagDiag
{
    bool runtimeActive{false};
    bool autoDisarmed{false};
    bool degraded{false};
    uint8_t selectedBus{CAN_BUS_ANY};
    uint8_t apState{0};
    uint8_t gear{0};
    uint8_t dasHandsState{0};
    uint8_t nativeHandsOnLevel{0};
    uint8_t consecutiveTxFailures{0};
    uint8_t consecutiveCounterCollisions{0};
    LegacyNagV4State state{LegacyNagV4State::Disabled};
    LegacyNagBlockReason blocked{LegacyNagBlockReason::Disabled};
    uint32_t rx370{0};
    uint32_t eligible370{0};
    uint32_t tx370{0};
    uint32_t txFail{0};
    uint32_t txAttempts{0};
    uint32_t txRetryAttempts{0};
    uint32_t txRetryOk{0};
    uint32_t targetHandsBlocked{0};
    uint32_t duplicateCounterObserved{0};
    uint32_t expectedNativeFollowerObserved{0};
    uint32_t counterCollisionObserved{0};
    uint32_t lastNativeFollowerDelayMs{0};
    uint32_t state1EnterCount{0};
    uint32_t state1HoldCount{0};
    uint32_t state1FallbackCount{0};
    uint32_t faithfulWalkSteps{0};
    uint32_t rx488{0};
    uint32_t directionChangeCount{0};
    uint32_t currentSilentMs{0};
    uint32_t maxSilentMs{0};
    uint32_t lastContextMs{0};
    uint32_t lastRxMs{0};
    uint32_t lastTxMs{0};
    int16_t requestedTorqueCentiNm{0};
    int16_t targetTorqueCentiNm{0};
    int16_t steeringAngleX10{0};
    uint16_t requestedTorqueRaw{2050};
    uint32_t steeringAgeMs{0};
    uint8_t steeringControlType{0};
    uint8_t steeringBus{CAN_BUS_ANY};
    int8_t episodeDirection{0};
    bool steeringSeen{false};
    uint8_t txCounter{0};
    uint8_t txChecksum{0};
};

struct DashNagBusLock
{
    static constexpr uint8_t kFramesToLock{3};
    static constexpr uint32_t kSelectedBusStaleMs{1000};

    uint8_t selectedBus{CAN_BUS_ANY};
    uint8_t candidateBus{CAN_BUS_ANY};
    uint8_t candidateFrames{0};
    uint8_t lastCounter{0};
    bool hasCounter{false};
    uint32_t lastSelectedRxMs{0};

    void reset()
    {
        selectedBus = CAN_BUS_ANY;
        candidateBus = CAN_BUS_ANY;
        candidateFrames = 0;
        lastCounter = 0;
        hasCounter = false;
        lastSelectedRxMs = 0;
    }

    bool accept(uint8_t bus, uint8_t counter, uint32_t nowMs)
    {
        if (bus == CAN_BUS_ANY)
            return true;

        if (selectedBus != CAN_BUS_ANY)
        {
            if (bus == selectedBus)
            {
                lastSelectedRxMs = nowMs;
                return true;
            }
            if (nowMs - lastSelectedRxMs <= kSelectedBusStaleMs)
                return false;
            reset();
        }

        uint8_t forwardDelta = static_cast<uint8_t>((counter - lastCounter) & 0x0F);
        bool sequential = hasCounter && candidateBus == bus &&
            forwardDelta >= 1 && forwardDelta <= 3;
        if (sequential)
        {
            if (candidateFrames < 0xFF)
                candidateFrames++;
        }
        else
        {
            candidateBus = bus;
            candidateFrames = 1;
        }
        hasCounter = true;
        lastCounter = counter;

        if (candidateFrames < kFramesToLock)
            return false;
        selectedBus = candidateBus;
        lastSelectedRxMs = nowMs;
        return true;
    }
};

struct DashReactiveNagBurst
{
    static constexpr int kHumanWeight{8};
    static constexpr int kNagThreshold{3};
    static constexpr int kReactiveAmplitude{70};
    static constexpr int kReactiveJitter{15};
    static constexpr int kProactiveAmplitude{35};
    static constexpr int kProactiveJitter{12};
    static constexpr int kAmplitudeCap{95};
    static constexpr int kStrokesMin{2};
    static constexpr int kStrokesMax{3};
    static constexpr int kStrokeDurationMinMs{300};
    static constexpr int kStrokeDurationMaxMs{400};
    static constexpr unsigned long kReactiveGapMs{800};
    static constexpr unsigned long kProactiveIntervalMinMs{2000};
    static constexpr unsigned long kProactiveIntervalMaxMs{5000};

    DashReactivePRNG random;
    NagMode currentMode{NagMode::Idle};
    bool injecting{false};
    bool proactiveBurst{false};
    unsigned long waveStartMs{0};
    int strokeCount{0};
    int totalStrokes{2};
    int strokeDurationMs{350};
    int amplitude{0};
    int jitter{0};
    unsigned long lastReactiveEndMs{0};
    unsigned long nextProactiveMs{0};
    uint8_t lastHandsOnState{0};
    bool nagActive{false};
    uint32_t nagSamples{0};
    uint32_t reactiveBursts{0};
    uint32_t proactiveWiggles{0};
    uint32_t echoSent{0};
    uint32_t echoFailed{0};

    void init(uint32_t seedValue)
    {
        random.seed(seedValue);
        nextProactiveMs = 0;
    }

    void reset()
    {
        currentMode = NagMode::Idle;
        injecting = false;
        proactiveBurst = false;
        strokeCount = 0;
        lastReactiveEndMs = 0;
        nextProactiveMs = 0;
        lastHandsOnState = 0;
        nagActive = false;
    }

    void resetCounters()
    {
        nagSamples = 0;
        reactiveBursts = 0;
        proactiveWiggles = 0;
        echoSent = 0;
        echoFailed = 0;
    }

    void recordEcho(bool ok)
    {
        if (ok)
            echoSent++;
        else
            echoFailed++;
    }

    NagMode mode() const { return currentMode; }
    bool shouldEcho() const { return injecting; }
    int currentAmplitude() const { return injecting ? amplitude : 0; }

    unsigned long nextProactiveIn(unsigned long nowMs) const
    {
        return currentMode == NagMode::Proactive && nextProactiveMs > nowMs
            ? nextProactiveMs - nowMs
            : 0;
    }

    void startBurst(bool proactive, unsigned long nowMs)
    {
        injecting = true;
        proactiveBurst = proactive;
        strokeCount = 0;
        waveStartMs = nowMs;
        totalStrokes = static_cast<int>(random.range(kStrokesMin, kStrokesMax));
        strokeDurationMs = static_cast<int>(random.range(kStrokeDurationMinMs, kStrokeDurationMaxMs));
        if (proactive)
        {
            amplitude = kProactiveAmplitude;
            jitter = kProactiveJitter;
            proactiveWiggles++;
        }
        else
        {
            amplitude = kReactiveAmplitude;
            jitter = kReactiveJitter;
            reactiveBursts++;
        }
    }

    void onNagSample(uint8_t handsOnState, unsigned long nowMs, bool active)
    {
        if (!active)
        {
            reset();
            lastHandsOnState = handsOnState;
            return;
        }

        if (handsOnState != lastHandsOnState)
        {
            lastHandsOnState = handsOnState;
            if (handsOnState <= 2)
            {
                nagActive = false;
                if (currentMode == NagMode::Reactive)
                {
                    injecting = false;
                    lastReactiveEndMs = 0;
                    currentMode = NagMode::Proactive;
                    nextProactiveMs = nowMs;
                }
            }
        }

        if (handsOnState >= kNagThreshold)
        {
            nagActive = true;
            nagSamples++;
            currentMode = NagMode::Reactive;
            if (injecting && proactiveBurst)
                injecting = false;
            if (injecting)
                return;
            bool gapElapsed = lastReactiveEndMs == 0 || nowMs - lastReactiveEndMs > kReactiveGapMs;
            if (gapElapsed)
                startBurst(false, nowMs);
            return;
        }

        nagActive = false;
        if (currentMode == NagMode::Reactive)
            return;
        currentMode = NagMode::Proactive;
        if (nextProactiveMs == 0)
            nextProactiveMs = nowMs;
        if (!injecting && nowMs >= nextProactiveMs)
            startBurst(true, nowMs);
    }

    int computeHold(unsigned long nowMs)
    {
        if (!injecting)
            return 0;
        unsigned long elapsed = nowMs - waveStartMs;
        float phase = (static_cast<float>(elapsed) / static_cast<float>(strokeDurationMs)) * static_cast<float>(M_PI);
        if (phase > static_cast<float>(M_PI))
            phase = static_cast<float>(M_PI);
        int perturbation = amplitude + static_cast<int>(sinf(phase) * static_cast<float>(jitter));
        if (perturbation > kAmplitudeCap)
            perturbation = kAmplitudeCap;
        if (perturbation < 0)
            perturbation = 0;
        if (elapsed > static_cast<unsigned long>(strokeDurationMs))
        {
            strokeCount++;
            if (strokeCount < totalStrokes)
            {
                waveStartMs = nowMs;
            }
            else
            {
                injecting = false;
                if (proactiveBurst)
                    nextProactiveMs = nowMs + random.range(kProactiveIntervalMinMs, kProactiveIntervalMaxMs);
                else
                    lastReactiveEndMs = nowMs;
            }
        }
        return perturbation;
    }

    void applyToFrame(uint8_t &data2Low, uint8_t &data3, int perturbation) const
    {
        int torque = (static_cast<int>(data2Low) << 8) | data3;
        torque += kHumanWeight + perturbation;
        if (torque < 0)
            torque = 0;
        if (torque > 0x0FFF)
            torque = 0x0FFF;
        data2Low = static_cast<uint8_t>((torque >> 8) & 0x0F);
        data3 = static_cast<uint8_t>(torque & 0xFF);
    }

    DashReactiveDiag diag(unsigned long nowMs, bool enabled) const
    {
        return {enabled, currentMode, injecting, lastHandsOnState, currentAmplitude(),
                nagSamples, reactiveBursts, proactiveWiggles, echoSent, echoFailed,
                nextProactiveIn(nowMs)};
    }
};
