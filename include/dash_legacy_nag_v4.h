#pragma once

#include <cstdint>

enum class LegacyNagV4State : uint8_t
{
    Disabled = 0,
    Armed = 1,
    StartupHold = 2,
    Support = 3,
    DetectedHold = 4,
    Detected = 5,
    Escalated = 6,
    Degraded = 7,
};

inline const char *dashLegacyNagV4StateName(LegacyNagV4State state)
{
    switch (state)
    {
    case LegacyNagV4State::Armed:
        return "armed";
    case LegacyNagV4State::StartupHold:
        return "state_delay";
    case LegacyNagV4State::Support:
        return "support_488";
    case LegacyNagV4State::DetectedHold:
        return "state1_hold";
    case LegacyNagV4State::Detected:
        return "state1_stopped";
    case LegacyNagV4State::Escalated:
        return "strong_488";
    case LegacyNagV4State::Degraded:
        return "degraded";
    default:
        return "disabled";
    }
}

enum class LegacyNagV4Block : uint8_t
{
    None = 0,
    Satisfied,
    StateDelay,
    StateStopped,
    SteeringMissing,
    SteeringStale,
    SteeringInvalid,
    CenterNoDirection,
};

struct DashLegacyNagV4Decision
{
    bool send{false};
    int16_t torqueCentiNm{0};
    uint8_t handsOnLevel{0};
    LegacyNagV4State state{LegacyNagV4State::Disabled};
    LegacyNagV4Block blocked{LegacyNagV4Block::None};
};

struct DashLegacyNagV4
{
    static constexpr int kTorqueRawBase{2050};
    static constexpr int kEncoderMinCentiNm{-270};
    static constexpr int kEncoderMaxCentiNm{450};
    static constexpr bool kPreserveNativeHandsOnLevel{true};

    static constexpr uint32_t kFaithfulSeed{0xDEADBEEFu};
    static constexpr uint32_t kSteeringFreshMs{250};
    static constexpr int16_t kSteeringDeadbandX10{5};
    static constexpr uint8_t kRequiredSteeringControlType{1};
    static constexpr uint32_t kState2DelayMs{2000};
    static constexpr uint32_t kStrongDelayMs{1000};
    static constexpr uint32_t kStrongRampMs{500};
    static constexpr uint32_t kState1HoldMs{500};
    static constexpr int kMildMinCentiNm{80};
    static constexpr int kMildMaxCentiNm{180};
    static constexpr int kStrongMaxCentiNm{210};
    static constexpr int kMildStepCentiNm{12};
    static constexpr int kOutputSlewCentiNm{20};

    bool enabled{false};
    bool apActive{false};
    uint8_t handsOnState{0};
    LegacyNagV4State state{LegacyNagV4State::Disabled};
    uint32_t stateEnterMs{0};
    uint32_t state1EnterMs{0};
    int16_t lastOutputCentiNm{0};
    int16_t targetTorqueCentiNm{0};

    bool steeringSeen{false};
    uint8_t steeringControlType{0};
    int16_t steeringAngleX10{0};
    uint8_t steeringBus{0};
    uint32_t steeringMs{0};
    int8_t episodeDirection{0};
    int8_t lastSteeringDirection{0};
    uint32_t directionChangeCount{0};

    int16_t mildMagnitudeCentiNm{120};
    uint32_t faithfulPrngState{kFaithfulSeed};
    uint32_t faithfulWalkSteps{0};
    uint32_t state1EnterCount{0};
    uint32_t state1HoldCount{0};
    uint32_t state1FallbackCount{0};

    void configure(bool nextEnabled)
    {
        if (enabled == nextEnabled)
            return;
        enabled = nextEnabled;
        resetRuntime();
    }

    void resetRuntime()
    {
        state = enabled ? LegacyNagV4State::Armed : LegacyNagV4State::Disabled;
        apActive = false;
        handsOnState = 0;
        stateEnterMs = 0;
        state1EnterMs = 0;
        lastOutputCentiNm = 0;
        targetTorqueCentiNm = 0;
        steeringSeen = false;
        steeringControlType = 0;
        steeringAngleX10 = 0;
        steeringBus = 0;
        steeringMs = 0;
        episodeDirection = 0;
        lastSteeringDirection = 0;
        directionChangeCount = 0;
        mildMagnitudeCentiNm = 120;
        faithfulPrngState = kFaithfulSeed;
        faithfulWalkSteps = 0;
    }

    void resetCounters()
    {
        state1EnterCount = 0;
        state1HoldCount = 0;
        state1FallbackCount = 0;
        directionChangeCount = 0;
        faithfulWalkSteps = 0;
    }

    void onSteeringRequest(uint8_t controlType, int16_t angleX10, uint8_t bus, uint32_t nowMs)
    {
        steeringSeen = true;
        steeringControlType = controlType;
        steeringAngleX10 = angleX10;
        steeringBus = bus;
        steeringMs = nowMs;

        if (controlType != kRequiredSteeringControlType)
            return;
        int8_t nextDirection = angleX10 > kSteeringDeadbandX10 ? -1 : angleX10 < -kSteeringDeadbandX10 ? 1
                                                                                                       : 0;
        if (nextDirection == 0)
            return;
        if (lastSteeringDirection != 0 && nextDirection != lastSteeringDirection)
            directionChangeCount++;
        lastSteeringDirection = nextDirection;
    }

    void onContext(bool nextApActive, uint8_t nextHandsOnState, uint32_t nowMs)
    {
        if (!enabled || !nextApActive)
        {
            apActive = false;
            handsOnState = nextHandsOnState;
            stateEnterMs = 0;
            state1EnterMs = 0;
            clearEpisode(true);
            state = enabled ? LegacyNagV4State::Armed : LegacyNagV4State::Disabled;
            return;
        }

        if (!apActive)
            stateEnterMs = nowMs;
        apActive = true;

        if (nextHandsOnState != handsOnState)
        {
            stateEnterMs = nowMs;
            if (nextHandsOnState == 1)
            {
                state1EnterMs = nowMs;
                state1EnterCount++;
            }
            else if (handsOnState == 1)
            {
                if (state1EnterMs && nowMs - state1EnterMs >= kState1HoldMs)
                    state1FallbackCount++;
                state1EnterMs = 0;
            }
            if (nextHandsOnState == 0 || nextHandsOnState == 8 || nextHandsOnState == 15)
                clearEpisode(false);
        }
        handsOnState = nextHandsOnState;
    }

    uint32_t steeringAgeMs(uint32_t nowMs) const
    {
        return steeringSeen && nowMs >= steeringMs ? nowMs - steeringMs : 0;
    }

    bool steeringFresh(uint32_t nowMs) const
    {
        return steeringSeen && steeringControlType == kRequiredSteeringControlType &&
               nowMs >= steeringMs && nowMs - steeringMs <= kSteeringFreshMs;
    }

    DashLegacyNagV4Decision decide(uint32_t nowMs, bool hardGatesAllowed)
    {
        DashLegacyNagV4Decision out{};
        if (!enabled)
        {
            state = LegacyNagV4State::Disabled;
            out.state = state;
            return out;
        }
        if (!hardGatesAllowed || !apActive)
        {
            state = LegacyNagV4State::Armed;
            out.state = state;
            return out;
        }

        if (handsOnState == 0 || handsOnState == 8 || handsOnState == 15)
        {
            state = LegacyNagV4State::Armed;
            out.state = state;
            out.blocked = LegacyNagV4Block::Satisfied;
            return out;
        }

        if (handsOnState == 1)
        {
            if (state1EnterMs && nowMs - state1EnterMs < kState1HoldMs && lastOutputCentiNm != 0)
            {
                state = LegacyNagV4State::DetectedHold;
                state1HoldCount++;
                return buildDecision(lastOutputCentiNm, state);
            }
            state = LegacyNagV4State::Detected;
            lastOutputCentiNm = 0;
            targetTorqueCentiNm = 0;
            out.state = state;
            out.blocked = LegacyNagV4Block::StateStopped;
            return out;
        }

        bool mild = handsOnState == 2;
        bool strong = handsOnState >= 3 && handsOnState <= 5;
        if (!mild && !strong)
        {
            state = LegacyNagV4State::Armed;
            out.state = state;
            out.blocked = LegacyNagV4Block::StateStopped;
            return out;
        }

        LegacyNagV4Block steeringBlock = refreshEpisodeDirection(nowMs);
        if (steeringBlock != LegacyNagV4Block::None)
        {
            state = LegacyNagV4State::Armed;
            out.state = state;
            out.blocked = steeringBlock;
            return out;
        }

        uint32_t stateAge = nowMs >= stateEnterMs ? nowMs - stateEnterMs : 0;
        if (mild)
        {
            if (stateAge < kState2DelayMs)
            {
                state = LegacyNagV4State::StartupHold;
                out.state = state;
                out.blocked = LegacyNagV4Block::StateDelay;
                return out;
            }
            state = LegacyNagV4State::Support;
            mildMagnitudeCentiNm = static_cast<int16_t>(nextMildMagnitude());
            targetTorqueCentiNm = static_cast<int16_t>(episodeDirection * mildMagnitudeCentiNm);
        }
        else
        {
            if (stateAge < kStrongDelayMs)
            {
                state = LegacyNagV4State::StartupHold;
                out.state = state;
                out.blocked = LegacyNagV4Block::StateDelay;
                return out;
            }
            state = LegacyNagV4State::Escalated;
            uint32_t activeMs = stateAge - kStrongDelayMs;
            int magnitude = activeMs >= kStrongRampMs ? kStrongMaxCentiNm : static_cast<int>((activeMs * kStrongMaxCentiNm) / kStrongRampMs);
            targetTorqueCentiNm = static_cast<int16_t>(episodeDirection * magnitude);
        }

        lastOutputCentiNm = static_cast<int16_t>(slewToward(
            lastOutputCentiNm, targetTorqueCentiNm, kOutputSlewCentiNm));
        faithfulWalkSteps++;
        return buildDecision(lastOutputCentiNm, state);
    }

    static int clampEncoderTorque(int torqueCentiNm)
    {
        if (torqueCentiNm > kEncoderMaxCentiNm)
            return kEncoderMaxCentiNm;
        if (torqueCentiNm < kEncoderMinCentiNm)
            return kEncoderMinCentiNm;
        return torqueCentiNm;
    }

    static uint16_t torqueRaw(int torqueCentiNm)
    {
        return static_cast<uint16_t>(kTorqueRawBase + clampEncoderTorque(torqueCentiNm));
    }

    static int torqueCentiNm(uint8_t data2, uint8_t data3)
    {
        uint16_t raw = static_cast<uint16_t>(((data2 & 0x0F) << 8) | data3);
        return static_cast<int>(raw) - kTorqueRawBase;
    }

    static void applyTorque(uint8_t &data2, uint8_t &data3, int torqueCentiNm)
    {
        uint16_t raw = torqueRaw(torqueCentiNm);
        data2 = static_cast<uint8_t>((data2 & 0xF0) | ((raw >> 8) & 0x0F));
        data3 = static_cast<uint8_t>(raw & 0xFF);
    }

private:
    void clearEpisode(bool clearDirection)
    {
        episodeDirection = 0;
        lastOutputCentiNm = 0;
        targetTorqueCentiNm = 0;
        mildMagnitudeCentiNm = 120;
        if (clearDirection)
            lastSteeringDirection = 0;
    }

    LegacyNagV4Block refreshEpisodeDirection(uint32_t nowMs)
    {
        if (!steeringSeen)
            return LegacyNagV4Block::SteeringMissing;
        if (steeringControlType != kRequiredSteeringControlType)
            return LegacyNagV4Block::SteeringInvalid;
        if (nowMs < steeringMs || nowMs - steeringMs > kSteeringFreshMs)
            return LegacyNagV4Block::SteeringStale;

        int8_t nextDirection = steeringAngleX10 > kSteeringDeadbandX10 ? -1 : steeringAngleX10 < -kSteeringDeadbandX10 ? 1
                                                                                                                       : lastSteeringDirection;
        if (nextDirection == 0)
            return LegacyNagV4Block::CenterNoDirection;
        episodeDirection = nextDirection;
        return LegacyNagV4Block::None;
    }

    DashLegacyNagV4Decision buildDecision(int torque, LegacyNagV4State nextState) const
    {
        DashLegacyNagV4Decision out{};
        out.send = torque != 0;
        out.torqueCentiNm = static_cast<int16_t>(torque);
        out.handsOnLevel = 0;
        out.state = nextState;
        return out;
    }

    uint32_t nextFaithfulRandom()
    {
        uint32_t x = faithfulPrngState ? faithfulPrngState : kFaithfulSeed;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        faithfulPrngState = x;
        return x;
    }

    int nextMildMagnitude()
    {
        int step = static_cast<int>(nextFaithfulRandom() %
                                    static_cast<uint32_t>(2 * kMildStepCentiNm + 1)) -
                   kMildStepCentiNm;
        int magnitude = static_cast<int>(mildMagnitudeCentiNm) + step;
        if (magnitude < kMildMinCentiNm)
            magnitude = kMildMinCentiNm;
        if (magnitude > kMildMaxCentiNm)
            magnitude = kMildMaxCentiNm;
        return magnitude;
    }

    static int slewToward(int current, int target, int maxDelta)
    {
        if (target > current + maxDelta)
            return current + maxDelta;
        if (target < current - maxDelta)
            return current - maxDelta;
        return target;
    }
};
