#pragma once

#include <cstdint>
#include <cstring>
#include "can_frame_types.h"

enum class LegacySmartOffsetMode : uint8_t
{
    Off = 0,
    Manual = 1,
    Auto = 2,
    Custom = 3,
};

enum class LegacySpeedLimitSource : uint8_t
{
    None = 0,
    Gps2F8 = 1,
    Fused399 = 2,
};

enum class LegacySpeedBusMode : uint8_t
{
    Auto = 0,
    Can1 = 1,
    Can2 = 2,
};

inline constexpr uint32_t kLegacyGpsLimitFreshMs = 3000;
inline constexpr uint32_t kLegacyFusedLimitFreshMs = 1000;

struct LegacySmartOffsetConfig
{
    LegacySmartOffsetMode mode = LegacySmartOffsetMode::Off;
    uint8_t manualOffsetKph = 0;
    bool smoothDownEnabled = true;
    uint8_t smoothDownRateKphS = 5;
    uint8_t customPctLow = 50;
    uint8_t customPctMid = 30;
    uint8_t customPctHigh = 20;
    uint8_t customPctVeryHigh = 10;
};

struct LegacySmartOffsetResult
{
    LegacySmartOffsetMode mode = LegacySmartOffsetMode::Off;
    uint8_t speedLimitRaw = 0;
    uint16_t speedLimitKph = 0;
    uint8_t offsetPct = 0;
    uint16_t absoluteCapKph = 0;
    uint16_t rawTargetKph = 0;
    uint16_t smoothedTargetKph = 0;
    uint8_t outputOffsetKph = 0;
    bool fallbackUsed = false;
    bool smoothingActive = false;
    uint32_t lastUpdateMs = 0;
    const char *blockedReason = "none";
};

inline uint8_t dashClampLegacySmartOffsetKph(int v)
{
    if (v < 0)
        v = 0;
    if (v > 33)
        v = 33;
    return static_cast<uint8_t>(v);
}

// The verified Legacy path reads 0x2F8 but writes its request on 0x3EE mux 0.
inline void dashWriteLegacyOffsetTo3eeMux0(uint8_t data[8], uint8_t offsetKph, uint8_t offsetPct)
{
    const uint8_t raw = static_cast<uint8_t>(dashClampLegacySmartOffsetKph(offsetKph) + 30U);
    data[3] = static_cast<uint8_t>((data[3] & 0x01U) | 0x80U | ((raw & 0x3FU) << 1U));
    data[5] = static_cast<uint8_t>(data[5] | 0x03U);
    const uint8_t wirePct = offsetPct > 63U ? 63U : offsetPct;
    data[7] = static_cast<uint8_t>((data[7] & 0xC0U) | (wirePct & 0x3FU));
}

inline uint8_t dashClampLegacySmartPct(int v)
{
    if (v < 0)
        v = 0;
    if (v > 63)
        v = 63;
    return static_cast<uint8_t>(v);
}

inline uint8_t dashClampLegacySmartRate(int v)
{
    if (v < 1 || v > 20)
        return 5;
    return static_cast<uint8_t>(v);
}

inline LegacySmartOffsetMode dashClampLegacySmartMode(int v)
{
    if (v == static_cast<int>(LegacySmartOffsetMode::Manual))
        return LegacySmartOffsetMode::Manual;
    if (v == static_cast<int>(LegacySmartOffsetMode::Auto))
        return LegacySmartOffsetMode::Auto;
    if (v == static_cast<int>(LegacySmartOffsetMode::Custom))
        return LegacySmartOffsetMode::Custom;
    return LegacySmartOffsetMode::Off;
}

inline LegacySpeedBusMode dashClampLegacySpeedBusMode(int v)
{
    if (v == static_cast<int>(LegacySpeedBusMode::Can1))
        return LegacySpeedBusMode::Can1;
    if (v == static_cast<int>(LegacySpeedBusMode::Can2))
        return LegacySpeedBusMode::Can2;
    return LegacySpeedBusMode::Auto;
}

inline bool dashLegacySmartLimitValid(uint8_t raw)
{
    return raw > 0 && raw < 31;
}

inline uint8_t dashLegacySmartAutoPct(uint16_t speedLimitKph)
{
    if (speedLimitKph <= 35)
        return 63;
    if (speedLimitKph <= 60)
        return 50;
    if (speedLimitKph <= 90)
        return 30;
    if (speedLimitKph <= 110)
        return 20;
    return 10;
}

inline uint16_t dashLegacySmartAbsoluteCapKph(uint16_t speedLimitKph)
{
    if (speedLimitKph <= 35)
        return 60;
    switch (speedLimitKph)
    {
    case 40: return 60;
    case 45: return 67;
    case 50: return 75;
    case 55: return 82;
    case 60: return 90;
    case 70: return 91;
    case 80: return 104;
    case 90: return 117;
    case 100: return 120;
    case 110: return 132;
    case 120: return 132;
    default: return static_cast<uint16_t>(speedLimitKph + 15);
    }
}

inline uint8_t dashLegacySmartCustomPct(const LegacySmartOffsetConfig &cfg, uint16_t speedLimitKph)
{
    if (speedLimitKph <= 50)
        return dashClampLegacySmartPct(cfg.customPctLow);
    if (speedLimitKph <= 70)
        return dashClampLegacySmartPct(cfg.customPctMid);
    if (speedLimitKph <= 100)
        return dashClampLegacySmartPct(cfg.customPctHigh);
    return dashClampLegacySmartPct(cfg.customPctVeryHigh);
}

inline uint16_t dashLegacySmartRoundedTargetKph(uint16_t speedLimitKph, uint8_t offsetPct, uint16_t absoluteCapKph)
{
    uint32_t target = (static_cast<uint32_t>(speedLimitKph) * static_cast<uint32_t>(100 + offsetPct) + 50) / 100;
    if (target > absoluteCapKph)
        target = absoluteCapKph;
    return static_cast<uint16_t>(target);
}

class LegacySmartOffsetEngine
{
public:
    LegacySmartOffsetResult compute(const LegacySmartOffsetConfig &cfg,
                                    uint8_t speedLimitRaw,
                                    uint32_t nowMs,
                                    bool apOrFsdEngaged)
    {
        (void)apOrFsdEngaged;
        LegacySmartOffsetResult result;
        result.mode = dashClampLegacySmartMode(static_cast<int>(cfg.mode));
        result.speedLimitRaw = speedLimitRaw;
        result.lastUpdateMs = nowMs;

        if (result.mode == LegacySmartOffsetMode::Off)
        {
            result.blockedReason = "off";
            syncSmoothing(0, nowMs);
            return result;
        }
        if (dashLegacySmartLimitValid(speedLimitRaw))
            result.speedLimitKph = static_cast<uint16_t>(speedLimitRaw) * 5U;

        if (result.mode == LegacySmartOffsetMode::Manual)
        {
            result.outputOffsetKph = dashClampLegacySmartOffsetKph(cfg.manualOffsetKph);
            result.rawTargetKph = static_cast<uint16_t>(result.speedLimitKph + result.outputOffsetKph);
            result.smoothedTargetKph = result.rawTargetKph;
            syncSmoothing(result.smoothedTargetKph, nowMs);
            return result;
        }
        if (!dashLegacySmartLimitValid(speedLimitRaw))
        {
            result.fallbackUsed = true;
            result.blockedReason = "speedLimitUnknown";
            syncSmoothing(0, nowMs);
            return result;
        }

        result.offsetPct = result.mode == LegacySmartOffsetMode::Auto
                               ? dashLegacySmartAutoPct(result.speedLimitKph)
                               : dashLegacySmartCustomPct(cfg, result.speedLimitKph);
        result.absoluteCapKph = dashLegacySmartAbsoluteCapKph(result.speedLimitKph);
        result.rawTargetKph = dashLegacySmartRoundedTargetKph(result.speedLimitKph,
                                                              result.offsetPct,
                                                              result.absoluteCapKph);
        result.smoothedTargetKph = applySmoothing(result.rawTargetKph,
                                                  nowMs,
                                                  cfg.smoothDownEnabled,
                                                  dashClampLegacySmartRate(cfg.smoothDownRateKphS),
                                                  result.smoothingActive);
        int offsetKph = static_cast<int>(result.smoothedTargetKph) - static_cast<int>(result.speedLimitKph);
        result.outputOffsetKph = dashClampLegacySmartOffsetKph(offsetKph);
        return result;
    }

    void resetSmoothing()
    {
        hasSmoothedTarget_ = false;
        lastSmoothedTargetKph_ = 0;
        lastUpdateMs_ = 0;
    }

private:
    uint16_t applySmoothing(uint16_t rawTargetKph,
                            uint32_t nowMs,
                            bool smoothDownEnabled,
                            uint8_t smoothDownRateKphS,
                            bool &smoothingActive)
    {
        smoothingActive = false;
        if (!hasSmoothedTarget_ || !smoothDownEnabled || rawTargetKph >= lastSmoothedTargetKph_)
        {
            syncSmoothing(rawTargetKph, nowMs);
            return rawTargetKph;
        }
        uint32_t dtMs = nowMs - lastUpdateMs_;
        if (dtMs == 0 || dtMs > 10000)
        {
            syncSmoothing(rawTargetKph, nowMs);
            return rawTargetKph;
        }
        uint32_t maxDrop = (static_cast<uint32_t>(smoothDownRateKphS) * dtMs) / 1000;
        if (maxDrop == 0)
        {
            smoothingActive = true;
            return lastSmoothedTargetKph_;
        }
        uint16_t smoothed = rawTargetKph;
        uint32_t candidate = static_cast<uint32_t>(lastSmoothedTargetKph_) - maxDrop;
        if (candidate > rawTargetKph)
        {
            smoothed = static_cast<uint16_t>(candidate);
            smoothingActive = true;
        }
        syncSmoothing(smoothed, nowMs);
        return smoothed;
    }

    void syncSmoothing(uint16_t targetKph, uint32_t nowMs)
    {
        hasSmoothedTarget_ = true;
        lastSmoothedTargetKph_ = targetKph;
        lastUpdateMs_ = nowMs;
    }

    bool hasSmoothedTarget_ = false;
    uint16_t lastSmoothedTargetKph_ = 0;
    uint32_t lastUpdateMs_ = 0;
};

struct DashLegacySpeedBusLock
{
    static constexpr uint8_t kSamplesToLock = 3;
    static constexpr uint32_t kMirrorWindowMs = 10;
    LegacySpeedBusMode mode = LegacySpeedBusMode::Auto;
    uint8_t selectedBus = CAN_BUS_ANY;
    uint8_t candidateBus = CAN_BUS_ANY;
    uint8_t candidateCount = 0;
    bool ambiguous = false;
    uint32_t ambiguityCount = 0;
    uint32_t lastSeenMs = 0;
    uint32_t lastSignature = 0;
    uint8_t lastBus = CAN_BUS_ANY;

    void reset()
    {
        selectedBus = CAN_BUS_ANY;
        candidateBus = CAN_BUS_ANY;
        candidateCount = 0;
        ambiguous = false;
        lastSeenMs = 0;
        lastSignature = 0;
        lastBus = CAN_BUS_ANY;
    }

    void setMode(LegacySpeedBusMode next)
    {
        if (mode != next)
        {
            mode = next;
            reset();
        }
    }

    static uint32_t signature(const CanFrame &frame)
    {
        uint32_t hash = 2166136261U;
        for (uint8_t i = 0; i < frame.dlc && i < 8; ++i)
        {
            hash ^= frame.data[i];
            hash *= 16777619U;
        }
        return hash;
    }

    bool accept(const CanFrame &frame, uint32_t nowMs)
    {
        uint8_t expected = mode == LegacySpeedBusMode::Can1
                               ? CAN_BUS_VEH
                               : (mode == LegacySpeedBusMode::Can2 ? CAN_BUS_PARTY : CAN_BUS_ANY);
        if (expected != CAN_BUS_ANY)
        {
            selectedBus = expected;
            ambiguous = false;
            return frame.bus == expected;
        }
        if (frame.bus == CAN_BUS_ANY)
            return false;

        uint32_t sig = signature(frame);
        if (lastBus != CAN_BUS_ANY && frame.bus != lastBus && sig == lastSignature &&
            static_cast<uint32_t>(nowMs - lastSeenMs) <= kMirrorWindowMs)
        {
            selectedBus = CAN_BUS_ANY;
            candidateBus = CAN_BUS_ANY;
            candidateCount = 0;
            ambiguous = true;
            ambiguityCount++;
            lastBus = frame.bus;
            lastSignature = sig;
            lastSeenMs = nowMs;
            return false;
        }
        lastBus = frame.bus;
        lastSignature = sig;
        lastSeenMs = nowMs;
        if (ambiguous)
            return false;
        if (selectedBus != CAN_BUS_ANY)
            return frame.bus == selectedBus;
        if (candidateBus != frame.bus)
        {
            candidateBus = frame.bus;
            candidateCount = 1;
            return false;
        }
        if (candidateCount < kSamplesToLock)
            candidateCount++;
        if (candidateCount < kSamplesToLock)
            return false;
        selectedBus = candidateBus;
        return true;
    }
};
