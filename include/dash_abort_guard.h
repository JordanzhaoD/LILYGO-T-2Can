#pragma once

#include <cstdint>

// Legacy Abort Guard is intentionally a local safety latch. It observes the
// native 0x399 AP state but never creates or changes a vehicle CAN frame.

enum class DashAbortGuardBlockPath : uint8_t
{
    None = 0,
    LegacyFsdMux0,
    LegacyFsdMux1,
    LegacyVisionSlider0x438,
    LegacyNag370,
    LegacyWheel045Dnd,
};

inline const char *dashAbortGuardBlockPathName(DashAbortGuardBlockPath path)
{
    switch (path)
    {
    case DashAbortGuardBlockPath::LegacyFsdMux0: return "legacy_fsd_mux0";
    case DashAbortGuardBlockPath::LegacyFsdMux1: return "legacy_fsd_mux1";
    case DashAbortGuardBlockPath::LegacyVisionSlider0x438: return "legacy_vision_slider_0x438";
    case DashAbortGuardBlockPath::LegacyNag370: return "legacy_nag_0x370";
    case DashAbortGuardBlockPath::LegacyWheel045Dnd: return "legacy_wheel_0x045";
    case DashAbortGuardBlockPath::None: default: return "none";
    }
}

struct DashAbortGuardDiag
{
    bool enabled = false;
    bool latched = false;
    uint8_t lastApState = 0;
    uint8_t lastAbortState = 0;
    uint32_t latchedAtMs = 0;
    const char *lastClearReason = "none";
    uint32_t blocks = 0;
    const char *lastBlockedPath = "none";
};

class DashAbortGuard
{
public:
    void setEnabled(bool enabled)
    {
        if (enabled_ == enabled)
            return;
        enabled_ = enabled;
        if (!enabled_)
        {
            latched_ = false;
            lastClearReason_ = "disabled";
        }
    }

    void onApState(uint8_t apState, uint32_t nowMs)
    {
        lastApState_ = apState;
        if (!enabled_)
            return;

        if (apState == 8 || apState == 9)
        {
            if (!latched_)
                latchedAtMs_ = nowMs;
            latched_ = true;
            lastAbortState_ = apState;
            lastClearReason_ = "none";
            return;
        }

        // An active AP state must not clear an abort latch. Release only when AP
        // drops below the engaged threshold (DAS_APSTATE_ENGAGED == 3, beta.15
        // parity): states 0/1 (disengaged) and 2 (available) re-arm the latch.
        if (latched_ && apState < 3)
        {
            latched_ = false;
            lastClearReason_ = "clean_disengage";
        }
    }

    bool allowsInjection() const { return !enabled_ || !latched_; }

    void recordBlock(DashAbortGuardBlockPath path)
    {
        if (allowsInjection())
            return;
        ++blocks_;
        lastBlockedPath_ = dashAbortGuardBlockPathName(path);
    }

    DashAbortGuardDiag diag() const
    {
        DashAbortGuardDiag diag;
        diag.enabled = enabled_;
        diag.latched = latched_;
        diag.lastApState = lastApState_;
        diag.lastAbortState = lastAbortState_;
        diag.latchedAtMs = latchedAtMs_;
        diag.lastClearReason = lastClearReason_;
        diag.blocks = blocks_;
        diag.lastBlockedPath = lastBlockedPath_;
        return diag;
    }

private:
    bool enabled_ = false;
    bool latched_ = false;
    uint8_t lastApState_ = 0;
    uint8_t lastAbortState_ = 0;
    uint32_t latchedAtMs_ = 0;
    const char *lastClearReason_ = "none";
    uint32_t blocks_ = 0;
    const char *lastBlockedPath_ = "none";
};
