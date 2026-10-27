#pragma once

#ifdef ESP_PLATFORM
#include "drivers/espidf_mcp2515.h"
#else
#include <SPI.h>
#include <mcp2515.h>
#endif
#include <memory>
#include "../can_frame_types.h"
#include "can_driver.h"

class ESP32_MCP2515Driver : public CanDriver
{
public:
    static constexpr bool kSupportsISR = false;

    explicit ESP32_MCP2515Driver(uint8_t csPin) : mcp_(csPin) {}

    bool init() override
    {
        mcp_.reset();
#ifndef MCP_CRYSTAL_FREQ
#define MCP_CRYSTAL_FREQ MCP_8MHZ
#endif
        MCP2515::ERROR e = mcp_.setBitrate(CAN_500KBPS, MCP_CRYSTAL_FREQ);
        if (e != MCP2515::ERROR_OK)
            return false;
        mcp_.setNormalMode();
        receiveAllMode_ = false;
        consecutiveTxFailures_ = 0;
        return true;
    }

    void setFilters(const uint32_t *ids, uint8_t count) override
    {
        filterCount_ = count > kMaxFilters ? kMaxFilters : count;
        for (uint8_t i = 0; i < filterCount_; i++)
            filterIds_[i] = ids[i];
        receiveAllMode_ = false;
        applyFilters();
    }

    void applyFilters()
    {
        if (filterCount_ == 0)
        {
            mcp_.setNormalMode();
            return;
        }
        mcp_.setConfigMode();
        mcp_.setFilterMask(MCP2515::MASK0, false, 0x7FF);
        mcp_.setFilter(MCP2515::RXF0, false, filterIds_[0]);
        mcp_.setFilter(MCP2515::RXF1, false, filterCount_ > 1 ? filterIds_[1] : filterIds_[0]);
        mcp_.setFilterMask(MCP2515::MASK1, false, 0x7FF);
        mcp_.setFilter(MCP2515::RXF2, false, filterCount_ > 2 ? filterIds_[2] : filterIds_[0]);
        mcp_.setFilter(MCP2515::RXF3, false, filterCount_ > 3 ? filterIds_[3] : filterIds_[0]);
        mcp_.setFilter(MCP2515::RXF4, false, filterCount_ > 4 ? filterIds_[4] : filterIds_[0]);
        mcp_.setFilter(MCP2515::RXF5, false, filterCount_ > 5 ? filterIds_[5] : filterIds_[0]);
        mcp_.setNormalMode();
    }

    bool enableInterrupt(void (*)(void)) override
    {
        return false;
    }

    bool read(CanFrame &frame) override
    {
        checkBusOff();
        can_frame raw;
        if (mcp_.readMessage(&raw) != MCP2515::ERROR_OK)
            return false;
        frame.id = raw.can_id;
        frame.dlc = raw.can_dlc;
        memcpy(frame.data, raw.data, 8);
        return true;
    }

    bool send(const CanFrame &frame) override
    {
        checkBusOff(); // TX path must also detect bus-off (not only read()), else injection silently dies
        can_frame raw;
        raw.can_id = frame.id;
        raw.can_dlc = frame.dlc;
        memcpy(raw.data, frame.data, 8);
        bool ok = mcp_.sendMessage(&raw) == MCP2515::ERROR_OK;
        if (ok)
            consecutiveTxFailures_ = 0;
        else
        {
            if (consecutiveTxFailures_ < 0xFF)
                consecutiveTxFailures_++;
            if (consecutiveTxFailures_ >= kMaxConsecutiveTxFailures)
                tryRecover();
        }
        if (onSendFrame)
            onSendFrame(frame, ok);
        return ok;
    }

    // The secondary bus is a recorder/diagnostic bus by default. Reapply this
    // mode after a controller reset so recovery does not silently stop seeing
    // native frames.
    void setReceiveAllMode()
    {
        mcp_.setReceiveAllMode();
        receiveAllMode_ = true;
    }

    uint32_t recoveryCount() const { return recoveryCount_; }
    uint8_t consecutiveTxFailures() const { return consecutiveTxFailures_; }

    MCP2515 &mcp() { return mcp_; }

private:
    static constexpr uint8_t kMaxConsecutiveTxFailures = 5;
    static constexpr uint32_t kRecoveryCooldownMs = 1000; // was 3000: bus-off must self-heal fast or the feature reads as "dead"

    void checkBusOff()
    {
        uint32_t now = millis();
        if (now - lastErrorCheckMs_ < 1000)
            return;
        lastErrorCheckMs_ = now;
        if (mcp_.getErrorFlags() & MCP2515::EFLG_TXBO)
            tryRecover();
    }

    void tryRecover()
    {
        uint32_t now = millis();
        if (now - lastRecoveryMs_ < kRecoveryCooldownMs)
            return;
        lastRecoveryMs_ = now;

        mcp_.reset();
        delay(10);
        if (mcp_.setBitrate(CAN_500KBPS, MCP_CRYSTAL_FREQ) != MCP2515::ERROR_OK)
            return;
        mcp_.setNormalMode();
        if (receiveAllMode_)
            mcp_.setReceiveAllMode();
        else if (filterCount_ > 0)
            applyFilters();
        consecutiveTxFailures_ = 0;
        recoveryCount_++;
    }

    MCP2515 mcp_;
    static constexpr uint8_t kMaxFilters = 6;
    uint32_t filterIds_[kMaxFilters] = {};
    uint8_t filterCount_ = 0;
    bool receiveAllMode_ = false;
    uint8_t consecutiveTxFailures_ = 0;
    uint32_t recoveryCount_ = 0;
    uint32_t lastErrorCheckMs_ = 0;
    uint32_t lastRecoveryMs_ = 0 - kRecoveryCooldownMs;
};
