#pragma once

#include "../can_frame_types.h"
#include "can_driver.h"
#include "can_bench_inject.h"
#include "../dash_twai_diag.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include <driver/twai.h>
#pragma GCC diagnostic pop
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)
// Albert-P4 mechanism port (2026-11-04): on the P4 vehicle build the private
// layer delegates to the new-API node core (IRAM/cache-safe ISR, internal-RAM
// ring); the legacy queue driver below stays the T2CAN dual / native path.
#include "p4_twai_node.h"
#define T2CAN_TWAI_NODE_CORE 1
#endif

#ifndef TWAI_RX_QUEUE_LEN
#define TWAI_RX_QUEUE_LEN 128
#endif
#ifndef TWAI_TX_QUEUE_LEN
#define TWAI_TX_QUEUE_LEN 16
#endif

class TWAIDriver : public CanDriver
{
public:
    static constexpr bool kSupportsISR = false;

    TWAIDriver(gpio_num_t txPin, gpio_num_t rxPin)
        : txPin_(txPin), rxPin_(rxPin)
#ifdef T2CAN_TWAI_NODE_CORE
          ,
          core_(txPin, rxPin, /*listenOnly=*/false)
#endif
    {
    }

    bool init() override
    {
        if (!mutex_)
            mutex_ = xSemaphoreCreateMutex();
        if (!mutex_)
            return false;

#ifdef T2CAN_TWAI_NODE_CORE
        // Consumer (can_rt) is created after setup; injected via
        // setRxConsumer() from app_start_can_task.
        lock();
        driverOK_ = core_.init(nullptr) == ESP_OK;
        unlock();
        return driverOK_;
#else
        g_config_ = TWAI_GENERAL_CONFIG_DEFAULT(txPin_, rxPin_, TWAI_MODE_NORMAL);
        g_config_.rx_queue_len = TWAI_RX_QUEUE_LEN;
        g_config_.tx_queue_len = TWAI_TX_QUEUE_LEN;

        t_config_ = TWAI_TIMING_CONFIG_500KBITS();
        f_config_ = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        lock();
        driverOK_ = installAndStartLocked();
        unlock();
        return driverOK_;
#endif
    }

    void setFilters(const uint32_t *ids, uint8_t count) override
    {
        if (count == 0)
            return;

        uint32_t differ = 0;
        for (uint8_t i = 1; i < count; i++)
        {
            differ |= ids[0] ^ ids[i];
        }

        uint32_t base = ids[0] & ~differ;
#ifdef T2CAN_TWAI_NODE_CORE
        lock();
        // TWAI only has a mask filter; sparse ID sets can pass false positives.
        exactFilterCount_ = (count < kMaxExactFilters) ? count : kMaxExactFilters;
        for (uint8_t i = 0; i < exactFilterCount_; i++)
            exactFilterIds_[i] = ids[i];
        core_.applyMaskFilter(base, differ);
        unlock();
#else
        twai_filter_config_t nextFilter = f_config_;
        nextFilter.acceptance_code = base << 21;
        nextFilter.acceptance_mask = (differ << 21) | 0x001FFFFF;
        nextFilter.single_filter = true;

        lock();
        // TWAI only has a mask filter; sparse ID sets can pass false positives.
        exactFilterCount_ = (count < kMaxExactFilters) ? count : kMaxExactFilters;
        for (uint8_t i = 0; i < exactFilterCount_; i++)
            exactFilterIds_[i] = ids[i];
        f_config_ = nextFilter;
        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
        unlock();
#endif
    }

    bool enableInterrupt(void (* /*onReady*/)()) override { return false; }

#ifdef T2CAN_TWAI_NODE_CORE
    void setRxConsumer(TaskHandle_t consumer)
    {
        lock();
        core_.setConsumer(consumer);
        unlock();
    }

    esp_err_t lastError() const { return core_.lastError(); }
#endif

    DashTwaiDiag snapshotDiag()
    {
        lock();
        updateDiagLocked();
        DashTwaiDiag copy = diag_;
        unlock();
        return copy;
    }

    bool read(CanFrame &frame) override
    {
#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)
        // Field-crash replay (2026-11-02): frames pushed by the serial DIAG
        // rv2_frame command flow through the real dispatch chain on can_rt.
        // Empty-ring fast path is one call that returns false immediately.
        if (canBenchInjectPop(0, frame))
            return true;
#endif
        for (uint8_t attempt = 0; attempt < kReadDrainBudget; attempt++)
        {
            lock();
            if (!driverOK_)
            {
                tryRecover();
                unlock();
                return false;
            }

#ifdef T2CAN_TWAI_NODE_CORE
            CanFrame msg;
            if (!core_.read(msg))
            {
                if (isBusOff())
                    recoverWithCooldown();
                unlock();
                return false;
            }
#else
            twai_message_t msg;
            if (twai_receive(&msg, 0) != ESP_OK)
            {
                if (isBusOff())
                    recoverWithCooldown();
                unlock();
                return false;
            }
#endif
            diag_.framesRead++;
            bool accepted = exactFilterMatchesLocked(
#ifdef T2CAN_TWAI_NODE_CORE
                msg.id
#else
                msg.identifier
#endif
            );
            if (accepted)
                diag_.framesAccepted++;
            else
                diag_.framesDropped++;
            unlock();

            if (!accepted)
                continue;

#ifdef T2CAN_TWAI_NODE_CORE
            frame = msg;
#else
            frame.id = msg.identifier;
            frame.dlc = (msg.data_length_code <= 8) ? msg.data_length_code : 8;
            memset(frame.data, 0, 8);
            memcpy(frame.data, msg.data, frame.dlc);
#endif
            return true;
        }

        lock();
        diag_.readDrainBudgetHits++;
        unlock();
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

#ifdef T2CAN_TWAI_NODE_CORE
        // Short timeout lives inside the core (2 ms): modified frames should
        // not be dropped, but long blocks risk starving the RX ring drain.
        bool ok = core_.send(frame);
#else
        twai_message_t msg = {};
        uint8_t dlc = (frame.dlc <= 8) ? frame.dlc : 8;
        msg.identifier = frame.id;
        msg.data_length_code = dlc;
        memcpy(msg.data, frame.data, dlc);

        // Short timeout (2ms): modified frames should not be dropped, but
        // long blocks (10ms) risk overflowing the 32-deep RX queue.
        // At 500kbps, ~8 frames arrive in 2ms — queue handles this fine.
        bool ok = twai_transmit(&msg, pdMS_TO_TICKS(2)) == ESP_OK;
#endif
        if (!ok)
        {
            if (isBusOff())
                recoverWithCooldown();
        }
        unlock();
        if (onSendFrame)
            onSendFrame(frame, ok);
        return ok;
    }

private:
    static constexpr uint8_t kMaxExactFilters = 32;
    static constexpr uint8_t kReadDrainBudget = 8;
    static constexpr uint32_t BUSOFF_COOLDOWN_MS = 1000;

    void updateDiagLocked()
    {
#ifdef T2CAN_TWAI_NODE_CORE
        if (!core_.healthy())
        {
            diag_.state = DashTwaiState::Stopped;
            return;
        }
        twai_status_info_t status;
        if (!core_.statusInfo(status))
        {
            diag_.state = DashTwaiState::Unknown;
            return;
        }
        switch (status.state)
        {
        case TWAI_STATE_RUNNING:
            diag_.state = DashTwaiState::Running;
            break;
        case TWAI_STATE_BUS_OFF:
            diag_.state = DashTwaiState::BusOff;
            break;
        case TWAI_STATE_RECOVERING:
            diag_.state = DashTwaiState::Recovering;
            break;
        case TWAI_STATE_STOPPED:
            diag_.state = DashTwaiState::Stopped;
            break;
        default:
            diag_.state = DashTwaiState::Unknown;
            break;
        }
        diag_.txErrorCounter = status.tx_error_counter;
        diag_.rxErrorCounter = status.rx_error_counter;
        diag_.msgsToTx = status.msgs_to_tx;
        diag_.msgsToRx = status.msgs_to_rx;
        diag_.txFailed = status.tx_failed_count;
        diag_.rxMissed = status.rx_missed_count;
        // The new driver does not expose the hw fifo overrun counter; the
        // consumer-keep-up metric is the honest overrun signal on this path.
        diag_.rxOverrun = core_.rxRingDrops();
        diag_.arbLost = status.arb_lost_count;
        diag_.busError = status.bus_error_count;
#else
        if (!driverInstalled_)
        {
            diag_.state = DashTwaiState::Stopped;
            return;
        }
#ifndef NATIVE_BUILD
        twai_status_info_t status;
        if (twai_get_status_info(&status) != ESP_OK)
        {
            diag_.state = DashTwaiState::Unknown;
            return;
        }
        switch (status.state)
        {
        case TWAI_STATE_RUNNING:
            diag_.state = DashTwaiState::Running;
            break;
        case TWAI_STATE_BUS_OFF:
            diag_.state = DashTwaiState::BusOff;
            break;
        case TWAI_STATE_RECOVERING:
            diag_.state = DashTwaiState::Recovering;
            break;
        case TWAI_STATE_STOPPED:
            diag_.state = DashTwaiState::Stopped;
            break;
        default:
            diag_.state = DashTwaiState::Unknown;
            break;
        }
        diag_.txErrorCounter = status.tx_error_counter;
        diag_.rxErrorCounter = status.rx_error_counter;
        diag_.msgsToTx = status.msgs_to_tx;
        diag_.msgsToRx = status.msgs_to_rx;
        diag_.txFailed = status.tx_failed_count;
        diag_.rxMissed = status.rx_missed_count;
        diag_.rxOverrun = status.rx_overrun_count;
        diag_.arbLost = status.arb_lost_count;
        diag_.busError = status.bus_error_count;
#endif
#endif
    }

    bool exactFilterMatchesLocked(uint32_t id) const
    {
        if (exactFilterCount_ == 0)
            return true;
        for (uint8_t i = 0; i < exactFilterCount_; i++)
        {
            if (exactFilterIds_[i] == id)
                return true;
        }
        return false;
    }

    bool isBusOff()
    {
#ifdef T2CAN_TWAI_NODE_CORE
        return core_.isBusOff();
#else
        if (!driverInstalled_)
            return false;
        twai_status_info_t status;
        if (twai_get_status_info(&status) != ESP_OK)
            return false;
        return status.state == TWAI_STATE_BUS_OFF;
#endif
    }

    void recoverWithCooldown()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS)
            return;
        lastRecovery_ = now;
#ifdef T2CAN_TWAI_NODE_CORE
        core_.recoverWithCooldown();
#else
        diag_.recoveries++;
#endif
        diag_.lastRecoveryMs = now;
        diag_.state = DashTwaiState::Recovering;

#ifndef T2CAN_TWAI_NODE_CORE
        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
#endif
    }

    void tryRecover()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS * 10)
            return;
        lastRecovery_ = now;
#ifdef T2CAN_TWAI_NODE_CORE
        core_.tryRecover();
#else
        diag_.recoveries++;
#endif
        diag_.lastRecoveryMs = now;
        diag_.state = DashTwaiState::Recovering;

#ifndef T2CAN_TWAI_NODE_CORE
        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
#endif
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

#ifndef T2CAN_TWAI_NODE_CORE
    bool installAndStartLocked()
    {
        if (twai_driver_install(&g_config_, &t_config_, &f_config_) != ESP_OK)
        {
            driverInstalled_ = false;
            return false;
        }
        driverInstalled_ = true;
        if (twai_start() != ESP_OK)
        {
            twai_driver_uninstall();
            driverInstalled_ = false;
            return false;
        }
        return true;
    }

    void stopAndUninstallLocked()
    {
        if (!driverInstalled_)
            return;
        twai_stop();
        twai_driver_uninstall();
        driverInstalled_ = false;
        driverOK_ = false;
    }
#endif

public:
#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)
    // Experiment B (crash forensics round 7, 2026-11-03): the field crash
    // family is register-file zeroing under TWAI interrupt pressure; the
    // bench never reproduces because its buses are silent (rv2_* inject
    // above the driver, ISR never fires). benchSetMode reinstalls the
    // controller in self-test (NO_ACK) and benchSendSelf transmits from a
    // loopback-enabled node, so each frame loops back through the REAL
    // ISR -> ring -> can_rt path. exp_b only; the caller restores the
    // original mode afterwards. Round-9 jumper plan: wiring CanA↔CanB makes
    // even NORMAL-mode self-floods drive a real differential bus.
    bool benchSetMode(twai_mode_t m)
    {
        // Round-9 finding: this silicon has no internal loopback MODE; the
        // new API splits B1's NO_ACK+self-reception into two node flags.
        // NO_ACK here = self-test (no ACK needed) + loopback (own frames
        // self-received) so the flood really drives the ISR -> ring ->
        // can_rt chain instead of transmitting into a void (rx=0).
        lock();
        bool ok = core_.benchReinit(/*selfTest=*/m == TWAI_MODE_NO_ACK,
                                    /*loopback=*/m == TWAI_MODE_NO_ACK);
        if (!ok)
            core_.benchReinit(false, false); // best-effort restore
        driverOK_ = core_.healthy();
        unlock();
        return ok;
    }

    bool benchSendSelf(const CanFrame &f)
    {
        lock();
        bool ok = driverOK_ && core_.send(f);
        unlock();
        return ok;
    }
#endif

private:
    DashTwaiDiag diag_;
    gpio_num_t txPin_;
    gpio_num_t rxPin_;
#ifdef T2CAN_TWAI_NODE_CORE
    P4TwaiNodeCore core_;
#else
    twai_general_config_t g_config_;
    twai_timing_config_t t_config_;
    twai_filter_config_t f_config_;
#endif
    SemaphoreHandle_t mutex_ = nullptr;
#ifndef T2CAN_TWAI_NODE_CORE
    bool driverInstalled_ = false;
#endif
    bool driverOK_ = false;
    uint32_t lastRecovery_ = 0;
    uint32_t exactFilterIds_[kMaxExactFilters] = {};
    uint8_t exactFilterCount_ = 0;
};
