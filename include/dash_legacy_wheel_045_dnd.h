#pragma once

#include <cstdint>
#include <cstring>

#include "can_frame_types.h"
#include "dash_tx_evidence.h"

// Legacy 0x045 scroll DND — single-frame-per-action, direction alternating
// per warning episode (Plan D, 6.0.0-beta07).
//
// beta03..beta05 tried to neutralize the volume side effect by pairing each
// +1 tick with a -1 tick inside the same episode (gaps 4-10ms / 25ms / 600ms).
// All three drifted: the car merges same-direction-aware pulses inside one
// window, so the inverse frame either strands (+1 leaks) or lands without the
// warning pulse (-1 leaks). beta05 also proved the pair design cannot both
// stay under the 300ms stalk-hold ceiling (P0: FSD activation blocked) and
// exceed the 600ms real-reversal floor.
//
// Plan D removes the pairing entirely: every frame in an episode points the
// same way, and the direction flips only after an episode actually delivered
// a frame onto the bus. Net drift is bounded by one episode's budget
// (+-kMaxFramesPerEpisode ticks) and cancels across consecutive delivered
// episodes (+n then -n). All beta02 safety rails stay: echo window, counter
// conflict abort, physical-input abort, release timeout, TX-failure
// auto-disarm, episode frame budget.

enum class DashLegacyWheel045State : uint8_t
{
    Idle = 0,
    WaitingSource,
    TxPending,
    WaitingIdleConfirm,
    RetryWait,
    Aborted,
    AutoDisarmed,
};

enum class DashLegacyWheel045Block : uint8_t
{
    None = 0,
    Disabled,
    NotLegacy,
    ApInactive,
    WarningClear,
    NoNative,
    StaleNative,
    InvalidNative,
    OtaGuard,
    TwaiUnavailable,
    ContextStale,
    PhysicalInput,
    CounterConflict,
    ReleaseTimeout,
    TxFailed,
    AutoDisarmed,
    EpisodeLimit,
    AbortGuard,
    StalkOwnedByJitter,
};

static inline const char *dashLegacyWheel045StateName(DashLegacyWheel045State state)
{
    switch (state)
    {
    case DashLegacyWheel045State::Idle:
        return "idle";
    case DashLegacyWheel045State::WaitingSource:
        return "waiting_source";
    case DashLegacyWheel045State::TxPending:
        return "tx_pending";
    case DashLegacyWheel045State::WaitingIdleConfirm:
        return "wait_idle_confirm";
    case DashLegacyWheel045State::RetryWait:
        return "retry_wait";
    case DashLegacyWheel045State::Aborted:
        return "aborted";
    case DashLegacyWheel045State::AutoDisarmed:
        return "auto_disarmed";
    }
    return "unknown";
}

static inline const char *dashLegacyWheel045BlockName(DashLegacyWheel045Block reason)
{
    switch (reason)
    {
    case DashLegacyWheel045Block::None:
        return "none";
    case DashLegacyWheel045Block::Disabled:
        return "disabled";
    case DashLegacyWheel045Block::NotLegacy:
        return "not_legacy";
    case DashLegacyWheel045Block::ApInactive:
        return "ap_inactive";
    case DashLegacyWheel045Block::WarningClear:
        return "warning_clear";
    case DashLegacyWheel045Block::NoNative:
        return "no_native";
    case DashLegacyWheel045Block::StaleNative:
        return "stale_native";
    case DashLegacyWheel045Block::InvalidNative:
        return "invalid_native";
    case DashLegacyWheel045Block::OtaGuard:
        return "ota_guard";
    case DashLegacyWheel045Block::TwaiUnavailable:
        return "twai_unavailable";
    case DashLegacyWheel045Block::ContextStale:
        return "context_stale";
    case DashLegacyWheel045Block::PhysicalInput:
        return "physical_input";
    case DashLegacyWheel045Block::CounterConflict:
        return "counter_conflict";
    case DashLegacyWheel045Block::ReleaseTimeout:
        return "release_timeout";
    case DashLegacyWheel045Block::TxFailed:
        return "tx_failed";
    case DashLegacyWheel045Block::AutoDisarmed:
        return "auto_disarmed";
    case DashLegacyWheel045Block::EpisodeLimit:
        return "episode_limit";
    case DashLegacyWheel045Block::AbortGuard:
        return "abort_guard";
    case DashLegacyWheel045Block::StalkOwnedByJitter:
        return "stalk_owned_by_jitter";
    }
    return "unknown";
}

struct DashLegacyWheel045Diag
{
    bool configured = false;
    bool runtimeArmed = false;
    bool autoDisarmed = false;
    bool apActive = false;
    bool warningActive = false;
    bool nativeSeen = false;
    bool nativeFresh = false;
    bool directionPositive = true;
    bool nextDirectionPositive = true;
    uint8_t apState = 0;
    uint8_t handsState = 0;
    uint8_t dasStatusBus = CAN_BUS_ANY;
    uint8_t nativeCounter = 0;
    uint8_t generatedCounter = 0;
    uint8_t generatedCrc = 0;
    uint8_t framesThisEpisode = 0;
    uint8_t consecutiveTxFailures = 0;
    uint32_t dasStatusMs = 0;
    uint32_t nativeMs = 0;
    uint32_t nativeRx = 0;
    uint32_t nativeIdleRx = 0;
    uint32_t nativeInvalid = 0;
    uint32_t physicalInputRx = 0;
    uint32_t warningEpisodes = 0;
    uint32_t positiveTx = 0;
    uint32_t negativeTx = 0;
    uint32_t txFailed = 0;
    uint32_t directionFlips = 0;
    uint32_t retryCount = 0;
    uint32_t expectedCounterDuplicates = 0;
    uint32_t counterConflicts = 0;
    uint32_t ownEchoRx = 0;
    uint32_t gatewayMirrorRx = 0;
    uint32_t abortCount = 0;
    uint32_t firstTxMs = 0;
    uint32_t warningClearLatencyMs = 0;
    uint32_t retryAtMs = 0;
    DashLegacyWheel045State state = DashLegacyWheel045State::Idle;
    DashLegacyWheel045Block blocked = DashLegacyWheel045Block::Disabled;
    DashTxEvidence tx{};
};

struct DashLegacyWheel045Dnd
{
    static constexpr uint32_t kCanId = 0x045;
    static constexpr uint32_t kGatewayMirrorId = 0x292;
    static constexpr uint32_t kNativeMaxAgeMs = 150;
    static constexpr uint32_t kContextMaxAgeMs = 1000;
    // beta05 P0 regression: raising the release timeout to 1200ms overlapped
    // the FSD activation landing window and blocked activation. 300ms is the
    // ceiling; native test pins it.
    static constexpr uint32_t kReleaseTimeoutMs = 300;
    static constexpr uint32_t kRetryDelayMs = 800;
    static constexpr uint32_t kOwnEchoWindowMs = 30;
    static constexpr uint8_t kMaxFramesPerEpisode = 3;
    static constexpr uint8_t kMaxConsecutiveTxFailures = 2;
    static constexpr uint16_t kLeftTickMask = 0x03C0;
    static constexpr uint16_t kAnyWheelMask = 0x3FFF;
    static constexpr uint8_t kPositiveNibble = 0x01;
    static constexpr uint8_t kNegativeNibble = 0x0F;

    DashLegacyWheel045Diag diag{};

    static uint8_t crc8J1850(const uint8_t *data, uint8_t length)
    {
        uint8_t crc = 0xFF;
        for (uint8_t i = 0; i < length; ++i)
        {
            crc ^= data[i];
            for (uint8_t bit = 0; bit < 8; ++bit)
                crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x1D)
                                   : static_cast<uint8_t>(crc << 1);
        }
        return static_cast<uint8_t>(crc ^ 0xFF);
    }

    static bool validFrame(const uint8_t data[8])
    {
        return crc8J1850(data, 7) == data[7];
    }

    static uint16_t switchWord(const uint8_t data[8])
    {
        return static_cast<uint16_t>(data[4]) |
               static_cast<uint16_t>(static_cast<uint16_t>(data[5]) << 8);
    }

    static uint8_t counter(const uint8_t data[8])
    {
        return static_cast<uint8_t>((data[6] >> 4) & 0x0F);
    }

    static bool isIdleWheelFrame(const uint8_t data[8])
    {
        return (switchWord(data) & kAnyWheelMask) == 0;
    }

    static void encodeTick(uint8_t data[8], uint8_t signedNibble)
    {
        uint16_t value = switchWord(data);
        value = static_cast<uint16_t>(value & static_cast<uint16_t>(~kLeftTickMask));
        value = static_cast<uint16_t>(value | ((signedNibble & 0x0F) << 6));
        data[4] = static_cast<uint8_t>(value & 0xFF);
        data[5] = static_cast<uint8_t>((value >> 8) & 0xFF);
    }

    void configure(bool enabled)
    {
        if (diag.configured == enabled)
        {
            updateArmed();
            return;
        }
        diag.configured = enabled;
        if (!enabled)
        {
            abort(DashLegacyWheel045Block::Disabled, false);
            autoDisarmed_ = false;
            diag.autoDisarmed = false;
            diag.consecutiveTxFailures = 0;
        }
        updateArmed();
    }

    void setRuntimeAllowed(bool allowed, DashLegacyWheel045Block deniedReason)
    {
        const DashLegacyWheel045Block nextBlock = allowed ? DashLegacyWheel045Block::None : deniedReason;
        if (runtimeAllowed_ == allowed && runtimeBlock_ == nextBlock)
        {
            updateArmed();
            return;
        }
        runtimeAllowed_ = allowed;
        runtimeBlock_ = nextBlock;
        if (!allowed)
            abort(deniedReason, isActionActive());
        updateArmed();
    }

    void onDasStatus(uint8_t apState, uint8_t handsState, uint8_t sourceBus, uint32_t nowMs)
    {
        const bool wasWarning = warningActive_;
        apActive_ = apState >= 3 && apState <= 6;
        warningActive_ = isWarningState(handsState);
        contextMs_ = nowMs;
        diag.apState = apState;
        diag.handsState = handsState;
        diag.dasStatusBus = sourceBus;
        diag.dasStatusMs = nowMs;
        diag.apActive = apActive_;
        diag.warningActive = warningActive_;

        if (!apActive_)
        {
            abort(DashLegacyWheel045Block::ApInactive, isActionActive());
            episodeActive_ = false;
            resetEpisodeCounters();
            return;
        }

        if (warningActive_ && !wasWarning)
        {
            episodeActive_ = true;
            // Latch this episode's direction from the alternation cursor.
            // The cursor only moves when a frame actually reaches the bus, so
            // episodes that send nothing keep their direction for next time.
            episodeDirectionPositive_ = nextDirectionPositive_;
            episodeTxDelivered_ = false;
            resetEpisodeCounters();
            pendingTx_ = false;
            retryAtMs_ = nowMs;
            diag.warningEpisodes++;
            diag.firstTxMs = 0;
            diag.warningClearLatencyMs = 0;
            setState(DashLegacyWheel045State::WaitingSource, DashLegacyWheel045Block::NoNative);
            syncEpisodeDiag();
        }
        else if (!warningActive_ && wasWarning)
        {
            if (diag.firstTxMs != 0)
                diag.warningClearLatencyMs = nowMs - diag.firstTxMs;
            episodeActive_ = false;
            pendingTx_ = false;
            syncEpisodeDiag();
            setState(DashLegacyWheel045State::Idle, DashLegacyWheel045Block::WarningClear);
        }
    }

    void onCan1Frame(const uint8_t data[8], uint8_t dlc, uint32_t nowMs)
    {
        diag.nativeRx++;
        if (dlc != 8 || !validFrame(data))
        {
            diag.nativeInvalid++;
            if (isActionActive())
                abort(DashLegacyWheel045Block::InvalidNative, true);
            return;
        }
        if (hasTxFingerprint_ && nowMs - txFingerprintMs_ <= kOwnEchoWindowMs &&
            memcmp(data, txFingerprint_, sizeof(txFingerprint_)) == 0)
        {
            diag.ownEchoRx++;
            return;
        }
        if (!isIdleWheelFrame(data))
        {
            diag.physicalInputRx++;
            if (isActionActive())
                abort(DashLegacyWheel045Block::PhysicalInput, true);
            return;
        }

        const uint8_t nativeCounter = counter(data);
        memcpy(nativeData_, data, sizeof(nativeData_));
        nativeMs_ = nowMs;
        nativeSeen_ = true;
        diag.nativeSeen = true;
        diag.nativeFresh = true;
        diag.nativeMs = nowMs;
        diag.nativeCounter = nativeCounter;
        diag.nativeIdleRx++;

        if (diag.state == DashLegacyWheel045State::WaitingIdleConfirm)
        {
            if (!expectedCounter(nativeCounter))
            {
                diag.counterConflicts++;
                abort(DashLegacyWheel045Block::CounterConflict, true);
                return;
            }
            if (nativeCounter == lastTxCounter_)
                diag.expectedCounterDuplicates++;
            if (framesThisEpisode_ < 0xFF)
                framesThisEpisode_++;
            syncEpisodeDiag();
            if (warningActive_ && episodeActive_ && framesThisEpisode_ < kMaxFramesPerEpisode)
            {
                retryAtMs_ = nowMs + kRetryDelayMs;
                diag.retryAtMs = retryAtMs_;
                setState(DashLegacyWheel045State::RetryWait, DashLegacyWheel045Block::None);
            }
            else if (warningActive_ && framesThisEpisode_ >= kMaxFramesPerEpisode)
                setState(DashLegacyWheel045State::Idle, DashLegacyWheel045Block::EpisodeLimit);
            else
                setState(DashLegacyWheel045State::Idle, DashLegacyWheel045Block::WarningClear);
        }
    }

    void observeGatewayMirror(const uint8_t data[8], uint8_t dlc, uint32_t nowMs)
    {
        if (dlc == 8 && hasTxFingerprint_ && nowMs - txFingerprintMs_ <= kNativeMaxAgeMs &&
            memcmp(data, txFingerprint_, sizeof(txFingerprint_)) == 0)
            diag.gatewayMirrorRx++;
    }

    bool nextFrame(uint32_t nowMs, uint8_t outData[8])
    {
        serviceTimeouts(nowMs);
        updateFresh(nowMs);
        updateArmed();
        if (!diag.runtimeArmed || !apActive_ || !contextFresh(nowMs))
        {
            if (diag.configured && runtimeAllowed_ && !autoDisarmed_ && !contextFresh(nowMs))
                setState(DashLegacyWheel045State::WaitingSource, DashLegacyWheel045Block::ContextStale);
            return false;
        }

        if (diag.state == DashLegacyWheel045State::RetryWait)
        {
            if (!warningActive_ || !episodeActive_)
                return false;
            if (nowMs < retryAtMs_)
                return false;
            diag.retryCount++;
            setState(DashLegacyWheel045State::WaitingSource, DashLegacyWheel045Block::NoNative);
        }

        const bool send = warningActive_ && episodeActive_ &&
                          framesThisEpisode_ < kMaxFramesPerEpisode &&
                          (diag.state == DashLegacyWheel045State::WaitingSource ||
                           diag.state == DashLegacyWheel045State::Idle);
        if (!send)
        {
            if (warningActive_ && episodeActive_ && framesThisEpisode_ >= kMaxFramesPerEpisode &&
                diag.state == DashLegacyWheel045State::Idle)
                setState(DashLegacyWheel045State::Idle, DashLegacyWheel045Block::EpisodeLimit);
            return false;
        }
        if (!nativeSeen_)
        {
            setState(DashLegacyWheel045State::WaitingSource, DashLegacyWheel045Block::NoNative);
            return false;
        }
        if (!nativeFresh(nowMs))
        {
            setState(DashLegacyWheel045State::WaitingSource, DashLegacyWheel045Block::StaleNative);
            return false;
        }

        memcpy(outData, nativeData_, sizeof(nativeData_));
        encodeTick(outData, episodeDirectionPositive_ ? kPositiveNibble : kNegativeNibble);
        const uint8_t generatedCounter = static_cast<uint8_t>((counter(nativeData_) + 1) & 0x0F);
        outData[6] = static_cast<uint8_t>((outData[6] & 0x0F) | (generatedCounter << 4));
        outData[7] = crc8J1850(outData, 7);
        memcpy(pendingData_, outData, sizeof(pendingData_));
        pendingTx_ = true;
        diag.generatedCounter = generatedCounter;
        diag.generatedCrc = outData[7];
        setState(DashLegacyWheel045State::TxPending, DashLegacyWheel045Block::None);
        return true;
    }

    void recordTxResult(bool ok, uint32_t nowMs)
    {
        if (!pendingTx_)
            return;
        pendingTx_ = false;
        if (!ok)
        {
            diag.txFailed++;
            if (consecutiveTxFailures_ < 0xFF)
                consecutiveTxFailures_++;
            diag.consecutiveTxFailures = consecutiveTxFailures_;
            abort(DashLegacyWheel045Block::TxFailed, true);
            if (consecutiveTxFailures_ >= kMaxConsecutiveTxFailures)
            {
                autoDisarmed_ = true;
                diag.autoDisarmed = true;
                setState(DashLegacyWheel045State::AutoDisarmed,
                         DashLegacyWheel045Block::AutoDisarmed);
            }
            updateArmed();
            return;
        }

        consecutiveTxFailures_ = 0;
        diag.consecutiveTxFailures = 0;
        memcpy(txFingerprint_, pendingData_, sizeof(txFingerprint_));
        txFingerprintMs_ = nowMs;
        hasTxFingerprint_ = true;
        lastTxCounter_ = counter(pendingData_);
        lastTxMs_ = nowMs;
        if (episodeDirectionPositive_)
            diag.positiveTx++;
        else
            diag.negativeTx++;
        if (!episodeTxDelivered_)
        {
            // First frame of this episode reached the bus: advance the
            // alternation cursor so the next episode pushes the other way.
            episodeTxDelivered_ = true;
            nextDirectionPositive_ = !episodeDirectionPositive_;
            diag.directionFlips++;
        }
        if (diag.firstTxMs == 0)
            diag.firstTxMs = nowMs;
        syncEpisodeDiag();
        setState(DashLegacyWheel045State::WaitingIdleConfirm,
                 DashLegacyWheel045Block::None);
    }

    void resetRuntime()
    {
        pendingTx_ = false;
        episodeActive_ = false;
        warningActive_ = false;
        apActive_ = false;
        framesThisEpisode_ = 0;
        episodeDirectionPositive_ = true;
        nextDirectionPositive_ = true;
        episodeTxDelivered_ = false;
        retryAtMs_ = 0;
        autoDisarmed_ = false;
        consecutiveTxFailures_ = 0;
        diag.autoDisarmed = false;
        diag.consecutiveTxFailures = 0;
        diag.warningActive = false;
        diag.apActive = false;
        syncEpisodeDiag();
        setState(DashLegacyWheel045State::Idle,
                 diag.configured ? DashLegacyWheel045Block::WarningClear
                                 : DashLegacyWheel045Block::Disabled);
        updateArmed();
    }

private:
    uint8_t nativeData_[8] = {};
    uint8_t pendingData_[8] = {};
    uint8_t txFingerprint_[8] = {};
    uint32_t nativeMs_ = 0;
    uint32_t contextMs_ = 0;
    uint32_t lastTxMs_ = 0;
    uint32_t txFingerprintMs_ = 0;
    uint32_t retryAtMs_ = 0;
    uint8_t lastTxCounter_ = 0;
    uint8_t framesThisEpisode_ = 0;
    uint8_t consecutiveTxFailures_ = 0;
    bool nativeSeen_ = false;
    bool runtimeAllowed_ = false;
    bool apActive_ = false;
    bool warningActive_ = false;
    bool episodeActive_ = false;
    bool autoDisarmed_ = false;
    bool pendingTx_ = false;
    bool episodeDirectionPositive_ = true;
    bool nextDirectionPositive_ = true;
    bool episodeTxDelivered_ = false;
    bool hasTxFingerprint_ = false;
    DashLegacyWheel045Block runtimeBlock_ = DashLegacyWheel045Block::Disabled;

    static bool isWarningState(uint8_t handsState)
    {
        return (handsState >= 3 && handsState <= 6) ||
               (handsState >= 9 && handsState <= 10);
    }

    bool nativeFresh(uint32_t nowMs) const
    {
        return nativeSeen_ && nowMs - nativeMs_ <= kNativeMaxAgeMs;
    }

    bool contextFresh(uint32_t nowMs) const
    {
        return contextMs_ != 0 && nowMs - contextMs_ <= kContextMaxAgeMs;
    }

    bool expectedCounter(uint8_t nativeCounter) const
    {
        return nativeCounter == lastTxCounter_ ||
               nativeCounter == static_cast<uint8_t>((lastTxCounter_ + 1) & 0x0F);
    }

    bool isActionActive() const
    {
        return diag.state == DashLegacyWheel045State::TxPending ||
               diag.state == DashLegacyWheel045State::WaitingIdleConfirm;
    }

    void serviceTimeouts(uint32_t nowMs)
    {
        if (diag.state == DashLegacyWheel045State::WaitingIdleConfirm &&
            lastTxMs_ != 0 && nowMs - lastTxMs_ > kReleaseTimeoutMs)
            abort(DashLegacyWheel045Block::ReleaseTimeout, true);
    }

    void updateFresh(uint32_t nowMs)
    {
        diag.nativeFresh = nativeFresh(nowMs);
    }

    void updateArmed()
    {
        diag.runtimeArmed = diag.configured && runtimeAllowed_ && !autoDisarmed_;
        if (!diag.configured)
            diag.blocked = DashLegacyWheel045Block::Disabled;
        else if (!runtimeAllowed_)
            diag.blocked = runtimeBlock_;
        else if (autoDisarmed_)
            diag.blocked = DashLegacyWheel045Block::AutoDisarmed;
    }

    void resetEpisodeCounters()
    {
        framesThisEpisode_ = 0;
        syncEpisodeDiag();
    }

    void syncEpisodeDiag()
    {
        diag.framesThisEpisode = framesThisEpisode_;
        diag.directionPositive = episodeDirectionPositive_;
        diag.nextDirectionPositive = nextDirectionPositive_;
        diag.retryAtMs = retryAtMs_;
    }

    void setState(DashLegacyWheel045State state, DashLegacyWheel045Block reason)
    {
        diag.state = state;
        diag.blocked = reason;
    }

    void abort(DashLegacyWheel045Block reason, bool countAbort)
    {
        pendingTx_ = false;
        if (countAbort)
            diag.abortCount++;
        setState(DashLegacyWheel045State::Aborted, reason);
    }
};
