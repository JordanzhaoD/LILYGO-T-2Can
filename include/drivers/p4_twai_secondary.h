#pragma once

// Atlas-P4-3CAN secondary bus (CanB, X197 9/10 — wheel/stalk bus).
// The T-2CAN dual build drives this bus with an MCP2515 over SPI; the P4
// board has three native TWAI controllers, so this is the same shape as
// twai_driver.h (mutex, 500k, 2ms TX timeout, bus-off cooldown) bound to
// TWAI_NUM_1 on GPIO 4/5, plus the MCP-facing accessors the shared dual
// skeleton in main.cpp calls on the secondary driver
// (setReceiveAllMode/recoveryCount/consecutiveTxFailures). The filter is
// accept-all from construction — the recorder-bus default the MCP path
// establishes via setReceiveAllMode() after init; the method stays as a
// no-op so the shared call sites compile unchanged.

#include "../can_frame_types.h"
#include "can_driver.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include <driver/twai.h>
#pragma GCC diagnostic pop
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifndef T2CAN_SECONDARY_TX_PIN
#define T2CAN_SECONDARY_TX_PIN GPIO_NUM_4
#endif
#ifndef T2CAN_SECONDARY_RX_PIN
#define T2CAN_SECONDARY_RX_PIN GPIO_NUM_5
#endif

class P4TwaiSecondaryDriver : public CanDriver
{
public:
    static constexpr bool kSupportsISR = false;

    P4TwaiSecondaryDriver(gpio_num_t txPin = T2CAN_SECONDARY_TX_PIN,
                          gpio_num_t rxPin = T2CAN_SECONDARY_RX_PIN)
        : txPin_(txPin), rxPin_(rxPin) {}

    bool init() override
    {
        if (!mutex_)
            mutex_ = xSemaphoreCreateMutex();
        if (!mutex_)
            return false;

        // controller_id = 1: the primary driver owns controller 0. IDF 6
        // dropped the TWAI_NUM_x enum — controller_id is a plain index —
        // BUT only the _v2 handle API honors it: legacy twai_driver_install()
        // always installs controller 0 regardless (g_twai_objs[0]) and every
        // legacy no-handle call operates on that same controller. Using them
        // here returned ESP_ERR_INVALID_STATE (controller 0 already owned by
        // the primary) — and had it succeeded, reads/writes would have hit
        // the MAIN bus. Every call below is the _v2 handle form.
        g_config_ = TWAI_GENERAL_CONFIG_DEFAULT_V2(
            kSecondaryControllerId, txPin_, rxPin_, TWAI_MODE_NORMAL);
        g_config_.rx_queue_len = TWAI_RX_QUEUE_LEN;
        g_config_.tx_queue_len = TWAI_TX_QUEUE_LEN;

        t_config_ = TWAI_TIMING_CONFIG_500KBITS();
        f_config_ = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        lock();
        driverOK_ = installAndStartLocked();
        unlock();
        return driverOK_;
    }

    // Recorder bus: accept-all from construction and setFilters is a no-op
    // (nothing may narrow the secondary bus behind the back of the
    // discovered-ID table). Kept for the CanDriver interface.
    void setFilters(const uint32_t * /*ids*/, uint8_t /*count*/) override {}

    // Accept-all already active on every (re)install; the MCP path reapplies
    // its mode after controller resets, this driver cannot lose it.
    void setReceiveAllMode() {}

    bool enableInterrupt(void (* /*onReady*/)()) override { return false; }

    bool read(CanFrame &frame) override
    {
        for (uint8_t attempt = 0; attempt < kReadDrainBudget; attempt++)
        {
            lock();
            if (!driverOK_)
            {
                tryRecoverLocked();
                unlock();
                return false;
            }

            twai_message_t msg;
            if (twai_receive_v2(handle_, &msg, 0) != ESP_OK)
            {
                if (isBusOffLocked())
                    recoverWithCooldownLocked();
                unlock();
                return false;
            }
            unlock();

            frame.id = msg.identifier;
            frame.dlc = (msg.data_length_code <= 8) ? msg.data_length_code : 8;
            memset(frame.data, 0, 8);
            memcpy(frame.data, msg.data, frame.dlc);
            return true;
        }
        return false;
    }

    bool send(const CanFrame &frame) override
    {
        lock();
        if (!driverOK_)
        {
            unlock();
            if (onSendFrame)
                onSendFrame(frame, false);
            return false;
        }

        twai_message_t msg = {};
        uint8_t dlc = (frame.dlc <= 8) ? frame.dlc : 8;
        msg.identifier = frame.id;
        msg.data_length_code = dlc;
        memcpy(msg.data, frame.data, dlc);

        // Short timeout (2ms): modified frames should not be dropped, but
        // long blocks (10ms) risk overflowing the RX queue (same reasoning
        // as the primary driver).
        bool ok = twai_transmit_v2(handle_, &msg, pdMS_TO_TICKS(2)) == ESP_OK;
        if (ok)
            consecutiveTxFailures_ = 0;
        else
        {
            if (consecutiveTxFailures_ < 0xFF)
                consecutiveTxFailures_++;
            if (isBusOffLocked())
                recoverWithCooldownLocked();
            if (consecutiveTxFailures_ >= kMaxConsecutiveTxFailures)
                tryRecoverLocked();
        }
        unlock();
        if (onSendFrame)
            onSendFrame(frame, ok);
        return ok;
    }

    uint32_t recoveryCount() const { return recoveryCount_; }
    uint8_t consecutiveTxFailures() const { return consecutiveTxFailures_; }

private:
    // Mirror of the MCP driver's injection-dies guard: after this many
    // consecutive failed sends, force a recover attempt.
    static constexpr uint8_t kMaxConsecutiveTxFailures = 5;
    static constexpr uint8_t kReadDrainBudget = 8;
    static constexpr uint32_t BUSOFF_COOLDOWN_MS = 1000;
    static constexpr int kSecondaryControllerId = 1;

    bool isBusOffLocked()
    {
        if (!driverInstalled_)
            return false;
        twai_status_info_t status;
        if (twai_get_status_info_v2(handle_, &status) != ESP_OK)
            return false;
        return status.state == TWAI_STATE_BUS_OFF;
    }

    void recoverWithCooldownLocked()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS)
            return;
        lastRecovery_ = now;
        recoveryCount_++;

        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
    }

    void tryRecoverLocked()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS * 10)
            return;
        lastRecovery_ = now;
        recoveryCount_++;

        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
    }

    void lock()
    {
        if (mutex_)
            xSemaphoreTake(mutex_, portMAX_DELAY);
    }

    void unlock()
    {
        if (mutex_)
            xSemaphoreGive(mutex_);
    }

    bool installAndStartLocked()
    {
        esp_err_t err = twai_driver_install_v2(&g_config_, &t_config_, &f_config_, &handle_);
        lastErr_ = err;
        if (err != ESP_OK)
        {
            handle_ = nullptr;
            driverInstalled_ = false;
            return false;
        }
        driverInstalled_ = true;
        err = twai_start_v2(handle_);
        lastErr_ = err;
        if (err != ESP_OK)
        {
            twai_driver_uninstall_v2(handle_);
            handle_ = nullptr;
            driverInstalled_ = false;
            return false;
        }
        return true;
    }

    void stopAndUninstallLocked()
    {
        if (!driverInstalled_)
            return;
        twai_stop_v2(handle_);
        twai_driver_uninstall_v2(handle_);
        handle_ = nullptr;
        driverInstalled_ = false;
        driverOK_ = false;
    }

    gpio_num_t txPin_;
    gpio_num_t rxPin_;

public:
    // Last esp_err from install/start — printed by the caller on init failure
    // so field logs carry the reason, not a bare "init failed".
    esp_err_t lastError() const { return lastErr_; }

private:
    esp_err_t lastErr_ = ESP_OK;
    twai_handle_t handle_ = nullptr;
    twai_general_config_t g_config_;
    twai_timing_config_t t_config_;
    twai_filter_config_t f_config_;
    SemaphoreHandle_t mutex_ = nullptr;
    bool driverInstalled_ = false;
    bool driverOK_ = false;
    uint32_t lastRecovery_ = 0;
    uint32_t recoveryCount_ = 0;
    uint8_t consecutiveTxFailures_ = 0;
};
