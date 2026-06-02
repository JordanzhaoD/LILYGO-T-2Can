#pragma once

#include <cstdint>
#include <cstring>

#ifndef NATIVE_BUILD
#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#else
#include <Arduino.h>
#endif
#endif

enum class LegacyFsdPolicy : uint8_t
{
    Stable = 0,
    Experimental = 1,
};

enum class FsdTriggerSource : uint8_t
{
    Unknown = 0,
    Force = 1,
    UiBit = 2,
    FalseSource = 3,
};

enum class FsdSkipReason : uint8_t
{
    None = 0,
    Sent = 1,
    DisabledInStable = 2,
    DisabledByPolicy = 3,
    NotTriggered = 4,
    CheckAdFalse = 5,
    TxFail = 6,
    DlcShort = 7,
};

static inline const char *legacyFsdPolicyName(LegacyFsdPolicy policy)
{
    switch (policy)
    {
    case LegacyFsdPolicy::Stable:
        return "legacy_stable";
    case LegacyFsdPolicy::Experimental:
        return "legacy_experimental";
    }
    return "legacy_stable";
}

static inline const char *fsdTriggerSourceName(FsdTriggerSource source)
{
    switch (source)
    {
    case FsdTriggerSource::Unknown:
        return "unknown";
    case FsdTriggerSource::Force:
        return "force";
    case FsdTriggerSource::UiBit:
        return "ui_bit";
    case FsdTriggerSource::FalseSource:
        return "false";
    }
    return "unknown";
}

static inline const char *fsdSkipReasonName(FsdSkipReason reason)
{
    switch (reason)
    {
    case FsdSkipReason::None:
        return "none";
    case FsdSkipReason::Sent:
        return "sent";
    case FsdSkipReason::DisabledInStable:
        return "disabled_in_stable";
    case FsdSkipReason::DisabledByPolicy:
        return "disabled_by_policy";
    case FsdSkipReason::NotTriggered:
        return "not_triggered";
    case FsdSkipReason::CheckAdFalse:
        return "check_ad_false";
    case FsdSkipReason::TxFail:
        return "tx_fail";
    case FsdSkipReason::DlcShort:
        return "dlc_short";
    }
    return "none";
}

struct FsdMuxDiag
{
    uint32_t rx = 0;
    uint32_t tx = 0;
    uint32_t err = 0;
    uint32_t lastRxMs = 0;
    uint32_t lastTxMs = 0;
    FsdSkipReason lastSkip = FsdSkipReason::None;
    uint8_t before[8] = {};
    uint8_t after[8] = {};

    void recordRx(uint32_t nowMs)
    {
        rx++;
        lastRxMs = nowMs;
    }

    void recordBefore(const uint8_t data[8])
    {
        memcpy(before, data, 8);
    }

    void recordAfter(const uint8_t data[8])
    {
        memcpy(after, data, 8);
    }

    void recordTx(bool ok, uint32_t nowMs)
    {
        if (ok)
        {
            tx++;
            lastSkip = FsdSkipReason::Sent;
        }
        else
        {
            err++;
            lastSkip = FsdSkipReason::TxFail;
        }
        lastTxMs = nowMs;
    }
};

struct LegacyFsdDiag
{
    LegacyFsdPolicy policy = LegacyFsdPolicy::Stable;
    bool mux1Enable = false;
    bool profileWriteEnable = false;
    bool visionLimitClearEnable = false;
    bool triggered = false;
    bool forceRuntime = false;
    FsdTriggerSource triggerSource = FsdTriggerSource::Unknown;
    FsdMuxDiag mux0;
    FsdMuxDiag mux1;
};

static inline uint32_t dashDiagNowMs()
{
#ifndef NATIVE_BUILD
    return millis();
#else
    return 0;
#endif
}
