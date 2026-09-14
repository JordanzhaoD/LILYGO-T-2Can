#pragma once

#include <cstdint>
#include <cstring>

#include "can_frame_types.h"
#include "dash_tx_evidence.h"

enum class DashWheelDndBlock : uint8_t
{
    None = 0,
    Disabled,
    NotLegacy,
    ApInactive,
    WarningClear,
    EscalatedObserveOnly,
    NoNativeMux1,
    StaleNative,
    Cooldown,
    EpisodeLimit,
    Busy,
    Can2Offline,
    OtaGuard,
    TxFailed,
    AutoDisarmed,
};

enum class DashWheelDndAction : uint8_t
{
    None = 0,
    LeftVolume,
    RightSpeed,
};

static inline const char *dashWheelDndActionName(DashWheelDndAction action)
{
    switch (action)
    {
    case DashWheelDndAction::None:
        return "none";
    case DashWheelDndAction::LeftVolume:
        return "left_volume";
    case DashWheelDndAction::RightSpeed:
        return "right_scroll";
    }
    return "unknown";
}

static inline const char *dashWheelDndBlockName(DashWheelDndBlock reason)
{
    switch (reason)
    {
    case DashWheelDndBlock::None:
        return "none";
    case DashWheelDndBlock::Disabled:
        return "disabled";
    case DashWheelDndBlock::NotLegacy:
        return "not_legacy";
    case DashWheelDndBlock::ApInactive:
        return "ap_inactive";
    case DashWheelDndBlock::WarningClear:
        return "warning_clear";
    case DashWheelDndBlock::EscalatedObserveOnly:
        return "escalated_observe_only";
    case DashWheelDndBlock::NoNativeMux1:
        return "no_native_mux1";
    case DashWheelDndBlock::StaleNative:
        return "stale_native";
    case DashWheelDndBlock::Cooldown:
        return "cooldown";
    case DashWheelDndBlock::EpisodeLimit:
        return "episode_limit";
    case DashWheelDndBlock::Busy:
        return "busy";
    case DashWheelDndBlock::Can2Offline:
        return "can2_offline";
    case DashWheelDndBlock::OtaGuard:
        return "ota_guard";
    case DashWheelDndBlock::TxFailed:
        return "tx_failed";
    case DashWheelDndBlock::AutoDisarmed:
        return "auto_disarmed";
    }
    return "unknown";
}

struct DashWheelDndDiag
{
    bool configured = false;
    bool armed = false;
    bool autoDisarmed = false;
    bool native3c2Seen = false;
    bool nativeMux1IdleSeen = false;
    bool warningActive = false;
    bool escalatedObserved = false;
    bool apActive = false;
    bool dasStatusSeen = false;
    uint8_t apState = 0;
    uint8_t dasStatusBus = CAN_BUS_ANY;
    uint8_t handsState = 0;
    uint8_t sequenceStep = 0;
    uint8_t actionsThisEpisode = 0;
    uint8_t consecutiveTxFailures = 0;
    uint32_t native3c2Ms = 0;
    uint32_t dasStatusMs = 0;
    uint32_t nativeRejected = 0;
    uint32_t warningEpisodes = 0;
    uint32_t escalatedObservations = 0;
    uint32_t triggerCount = 0;
    uint32_t lastTriggerMs = 0;
    uint32_t cooldownUntilMs = 0;
    DashWheelDndAction action = DashWheelDndAction::None;
    const char *baseFrameMode = "waiting_native_mux1";
    const char *sequenceState = "idle";
    DashWheelDndBlock blocked = DashWheelDndBlock::Disabled;
    DashTxEvidence tx{};
};

struct DashWheelDND
{
    static constexpr uint8_t kSteps[]{0x01, 0x00, 0x3F, 0x00};
    static constexpr int kStepCount{4};
    static constexpr uint32_t kStepIntervalMs{100};
    static constexpr uint32_t kNativeMaxAgeMs{1000};
    static constexpr uint32_t kReactiveCooldownMs{2000};
    static constexpr uint8_t kMaxActionsPerEpisode{3};
    static constexpr uint8_t kMaxConsecutiveTxFailures{2};
    static constexpr uint32_t kCanId{0x3C2};
    static constexpr uint8_t kMuxMask{0x03};
    static constexpr uint8_t kMuxScroll{0x01};

    int volumeStep{0};
    int speedStep{0};
    bool volumeActive{false};
    bool speedActive{false};
    bool reactiveAction{false};
    bool reactiveConfigured{false};
    DashWheelDndAction reactiveActionType{DashWheelDndAction::None};
    bool runtimeAllowed{false};
    DashWheelDndBlock runtimeBlock{DashWheelDndBlock::Disabled};
    bool primaryWarningActive{false};
    bool escalatedWarningActive{false};
    bool apActive{false};
    bool autoDisarmed{false};
    uint8_t actionsThisEpisode{0};
    uint8_t consecutiveTxFailures{0};
    uint32_t lastStepMs{0};
    uint32_t cooldownUntilMs{0};
    uint8_t nativeData[8] = {};
    uint32_t nativeMs{0};
    bool nativeSeen{false};
    DashWheelDndDiag diag{};

    void startVolume()
    {
        startVolumeInternal(false, 0);
    }

    void startSpeed()
    {
        if (reactiveAction)
            return;
        speedActive = true;
        speedStep = 0;
        lastStepMs = 0;
        diag.sequenceState = "manual_speed";
    }

    bool recordNative3c2(const uint8_t data[8], uint32_t nowMs)
    {
        diag.native3c2Seen = true;
        if ((data[0] & kMuxMask) != kMuxScroll || data[2] != 0 || data[3] != 0)
        {
            diag.nativeRejected++;
            return false;
        }

        memcpy(nativeData, data, sizeof(nativeData));
        nativeMs = nowMs;
        nativeSeen = true;
        diag.nativeMux1IdleSeen = true;
        diag.native3c2Ms = nowMs;
        diag.baseFrameMode = "native_mux1_idle";
        return true;
    }

    bool native3c2Fresh(uint32_t nowMs) const
    {
        return nativeSeen && (nowMs - nativeMs) <= kNativeMaxAgeMs;
    }

    void setReactiveConfigured(bool enabled)
    {
        setReactiveActions(enabled, false);
    }

    void setReactiveActions(bool volumeEnabled, bool speedEnabled)
    {
        const DashWheelDndAction requested = speedEnabled
                                                 ? DashWheelDndAction::RightSpeed
                                                 : (volumeEnabled ? DashWheelDndAction::LeftVolume
                                                                  : DashWheelDndAction::None);
        if (reactiveAction && requested != reactiveActionType)
            cancelReactiveAction();

        reactiveActionType = requested;
        reactiveConfigured = requested != DashWheelDndAction::None;
        diag.action = requested;
        diag.configured = reactiveConfigured;
        diag.armed = reactiveConfigured && runtimeAllowed && !autoDisarmed;
        if (!reactiveConfigured)
        {
            cancelReactiveAction();
            actionsThisEpisode = 0;
            diag.actionsThisEpisode = 0;
            setBlocked(DashWheelDndBlock::Disabled);
        }
        else if (autoDisarmed)
            setBlocked(DashWheelDndBlock::AutoDisarmed);
    }

    void setRuntimeAllowed(bool allowed, DashWheelDndBlock blockedWhenDenied)
    {
        runtimeAllowed = allowed;
        runtimeBlock = allowed ? DashWheelDndBlock::None : blockedWhenDenied;
        diag.armed = reactiveConfigured && runtimeAllowed && !autoDisarmed;
        if (!allowed)
        {
            cancelReactiveAction();
            volumeActive = false;
            speedActive = false;
            volumeStep = 0;
            speedStep = 0;
            setBlocked(blockedWhenDenied);
        }
    }

    void clearAutoDisarm()
    {
        autoDisarmed = false;
        consecutiveTxFailures = 0;
        diag.autoDisarmed = false;
        diag.consecutiveTxFailures = 0;
        diag.armed = reactiveConfigured && runtimeAllowed;
        if (reactiveConfigured)
            setBlocked(DashWheelDndBlock::WarningClear);
    }

    void onDasStatus(uint8_t apState, uint8_t handsState, uint32_t nowMs,
                     uint8_t sourceBus = CAN_BUS_ANY)
    {
        const bool wasPrimary = primaryWarningActive;
        apActive = apState >= 3 && apState <= 6;
        primaryWarningActive = handsState >= 3 && handsState <= 6;
        escalatedWarningActive = handsState >= 9 && handsState <= 10;

        diag.apActive = apActive;
        diag.dasStatusSeen = true;
        diag.apState = apState;
        diag.dasStatusBus = sourceBus;
        diag.dasStatusMs = nowMs;
        diag.handsState = handsState;
        diag.warningActive = primaryWarningActive;

        if (escalatedWarningActive)
        {
            if (!diag.escalatedObserved)
                diag.escalatedObservations++;
            diag.escalatedObserved = true;
            cancelReactiveAction();
            setBlocked(DashWheelDndBlock::EscalatedObserveOnly);
            return;
        }
        diag.escalatedObserved = false;

        if (!primaryWarningActive)
        {
            cancelReactiveAction();
            actionsThisEpisode = 0;
            diag.actionsThisEpisode = 0;
            setBlocked(DashWheelDndBlock::WarningClear);
            return;
        }

        if (!wasPrimary)
        {
            actionsThisEpisode = 0;
            cooldownUntilMs = nowMs;
            diag.actionsThisEpisode = 0;
            diag.warningEpisodes++;
        }

        serviceReactive(nowMs);
    }

    void serviceReactive(uint32_t nowMs)
    {
        diag.armed = reactiveConfigured && runtimeAllowed && !autoDisarmed;
        if (!reactiveConfigured)
        {
            setBlocked(DashWheelDndBlock::Disabled);
            return;
        }
        if (autoDisarmed)
        {
            setBlocked(DashWheelDndBlock::AutoDisarmed);
            return;
        }
        if (!runtimeAllowed)
        {
            setBlocked(runtimeBlock);
            return;
        }
        if (!apActive)
        {
            setBlocked(DashWheelDndBlock::ApInactive);
            return;
        }
        if (!primaryWarningActive)
        {
            setBlocked(DashWheelDndBlock::WarningClear);
            return;
        }
        if (reactiveAction || isRunning())
        {
            if (!reactiveAction)
                setBlocked(DashWheelDndBlock::Busy);
            return;
        }
        if (actionsThisEpisode >= kMaxActionsPerEpisode)
        {
            setBlocked(DashWheelDndBlock::EpisodeLimit);
            return;
        }
        if (nowMs < cooldownUntilMs)
        {
            diag.cooldownUntilMs = cooldownUntilMs;
            setBlocked(DashWheelDndBlock::Cooldown);
            return;
        }
        if (!nativeSeen)
        {
            setBlocked(DashWheelDndBlock::NoNativeMux1);
            return;
        }
        if (!native3c2Fresh(nowMs))
        {
            setBlocked(DashWheelDndBlock::StaleNative);
            return;
        }

        actionsThisEpisode++;
        diag.actionsThisEpisode = actionsThisEpisode;
        diag.triggerCount++;
        diag.lastTriggerMs = nowMs;
        startReactiveInternal(nowMs);
        setBlocked(DashWheelDndBlock::None);
    }

    bool tick(uint32_t nowMs, uint8_t outData[8])
    {
        if (!isRunning())
            return false;
        if (nowMs - lastStepMs < kStepIntervalMs)
            return false;
        if (!native3c2Fresh(nowMs))
        {
            diag.baseFrameMode = nativeSeen ? "stale_native_mux1" : "waiting_native_mux1";
            diag.sequenceState = "blocked";
            setBlocked(nativeSeen ? DashWheelDndBlock::StaleNative : DashWheelDndBlock::NoNativeMux1);
            if (reactiveAction)
                cancelReactiveAction();
            return false;
        }

        lastStepMs = nowMs;
        memcpy(outData, nativeData, sizeof(nativeData));
        diag.baseFrameMode = "native_mux1_idle";
        diag.sequenceState = reactiveAction ? "reactive_running" : "manual_running";

        if (volumeActive)
        {
            outData[2] = kSteps[volumeStep++];
            diag.sequenceStep = static_cast<uint8_t>(volumeStep);
            if (volumeStep >= kStepCount)
            {
                volumeActive = false;
                volumeStep = 0;
            }
        }

        if (speedActive)
        {
            outData[3] = kSteps[speedStep++];
            diag.sequenceStep = static_cast<uint8_t>(speedStep);
            if (speedStep >= kStepCount)
            {
                speedActive = false;
                speedStep = 0;
            }
        }

        if (reactiveAction && !volumeActive && !speedActive)
        {
            reactiveAction = false;
            cooldownUntilMs = nowMs + kReactiveCooldownMs;
            diag.cooldownUntilMs = cooldownUntilMs;
            diag.sequenceState = "cooldown";
            diag.sequenceStep = 0;
        }
        else if (!isRunning())
        {
            diag.sequenceState = "sent";
            diag.sequenceStep = 0;
        }
        return true;
    }

    void recordTxResult(bool ok, uint32_t nowMs)
    {
        if (ok)
        {
            consecutiveTxFailures = 0;
            diag.consecutiveTxFailures = 0;
            return;
        }

        consecutiveTxFailures++;
        diag.consecutiveTxFailures = consecutiveTxFailures;
        cancelReactiveAction();
        cooldownUntilMs = nowMs + kReactiveCooldownMs;
        diag.cooldownUntilMs = cooldownUntilMs;
        setBlocked(DashWheelDndBlock::TxFailed);
        if (consecutiveTxFailures >= kMaxConsecutiveTxFailures)
        {
            autoDisarmed = true;
            diag.autoDisarmed = true;
            diag.armed = false;
            setBlocked(DashWheelDndBlock::AutoDisarmed);
        }
    }

    bool isRunning() const { return volumeActive || speedActive; }

    void reset()
    {
        volumeActive = false;
        speedActive = false;
        reactiveAction = false;
        reactiveActionType = DashWheelDndAction::None;
        volumeStep = 0;
        speedStep = 0;
        lastStepMs = 0;
        primaryWarningActive = false;
        escalatedWarningActive = false;
        apActive = false;
        actionsThisEpisode = 0;
        cooldownUntilMs = 0;
        diag.warningActive = false;
        diag.apActive = false;
        diag.sequenceStep = 0;
        diag.actionsThisEpisode = 0;
        diag.sequenceState = "idle";
    }

private:
    void startVolumeInternal(bool reactive, uint32_t nowMs)
    {
        if (reactive && isRunning())
            return;
        volumeActive = true;
        volumeStep = 0;
        reactiveAction = reactive;
        lastStepMs = nowMs >= kStepIntervalMs ? nowMs - kStepIntervalMs : 0;
        diag.sequenceStep = 0;
        diag.sequenceState = reactive ? "reactive_pending" : "manual_volume";
    }

    void startReactiveInternal(uint32_t nowMs)
    {
        if (isRunning())
            return;

        reactiveAction = true;
        lastStepMs = nowMs >= kStepIntervalMs ? nowMs - kStepIntervalMs : 0;
        diag.sequenceStep = 0;
        diag.sequenceState = "reactive_pending";
        diag.action = reactiveActionType;
        if (reactiveActionType == DashWheelDndAction::RightSpeed)
        {
            speedActive = true;
            speedStep = 0;
        }
        else
        {
            volumeActive = true;
            volumeStep = 0;
        }
    }

    void cancelReactiveAction()
    {
        if (!reactiveAction)
            return;
        volumeActive = false;
        speedActive = false;
        volumeStep = 0;
        speedStep = 0;
        reactiveAction = false;
        diag.sequenceStep = 0;
        diag.sequenceState = "aborted";
    }

    void setBlocked(DashWheelDndBlock reason)
    {
        diag.blocked = reason;
    }
};
