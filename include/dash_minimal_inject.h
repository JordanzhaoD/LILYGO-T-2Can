#pragma once

#include <cstdint>

// Minimal Inject bounds only Legacy 0x3EE mux0 *activation* writes. It does
// not suppress source observation, speed-only writes, diagnostics, or a later
// AP engagement. The quota is consumed only after a successful CAN TX.
struct DashMinimalInjectDiag
{
    bool enabled = false;
    bool apEngaged = false;
    uint8_t lastApState = 0;
    uint8_t successfulMux0 = 0;
    uint8_t quota = 5;
    uint32_t engagementCount = 0;
    uint32_t blocked = 0;
    const char *lastBlockReason = "none";
};

class DashMinimalInject
{
public:
    static constexpr uint8_t kQuota = 5;

    void setEnabled(bool enabled)
    {
        if (enabled_ == enabled)
            return;
        enabled_ = enabled;
        if (!enabled_)
        {
            apEngaged_ = false;
            successfulMux0_ = 0;
            lastBlockReason_ = "disabled";
        }
    }

    void onApState(uint8_t apState)
    {
        lastApState_ = apState;
        if (!enabled_)
            return;

        const bool engaged = apState >= 3 && apState <= 6;
        if (engaged && !apEngaged_)
        {
            apEngaged_ = true;
            successfulMux0_ = 0;
            ++engagementCount_;
            lastBlockReason_ = "none";
        }
        else if (!engaged)
        {
            apEngaged_ = false;
            successfulMux0_ = 0;
        }
    }

    bool allowsActivation()
    {
        if (!enabled_ || !apEngaged_ || successfulMux0_ < kQuota)
            return true;
        ++blocked_;
        lastBlockReason_ = "quota_reached";
        return false;
    }

    void recordSuccessfulActivation()
    {
        if (!enabled_ || !apEngaged_)
            return;
        if (successfulMux0_ < kQuota)
            ++successfulMux0_;
        if (successfulMux0_ >= kQuota)
            lastBlockReason_ = "quota_reached";
    }

    DashMinimalInjectDiag diag() const
    {
        DashMinimalInjectDiag diag;
        diag.enabled = enabled_;
        diag.apEngaged = apEngaged_;
        diag.lastApState = lastApState_;
        diag.successfulMux0 = successfulMux0_;
        diag.quota = kQuota;
        diag.engagementCount = engagementCount_;
        diag.blocked = blocked_;
        diag.lastBlockReason = lastBlockReason_;
        return diag;
    }

private:
    bool enabled_ = false;
    bool apEngaged_ = false;
    uint8_t lastApState_ = 0;
    uint8_t successfulMux0_ = 0;
    uint32_t engagementCount_ = 0;
    uint32_t blocked_ = 0;
    const char *lastBlockReason_ = "none";
};
