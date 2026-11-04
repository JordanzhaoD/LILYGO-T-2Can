#pragma once

// Atlas-P4-3CAN secondary bus (CanB, X197 9/10 — wheel/stalk bus).
// The T-2CAN dual build drives this bus with an MCP2515 over SPI; the P4
// board has three native TWAI controllers, so this is the same shape as
// twai_driver.h (mutex, 500k, 2ms TX timeout, bus-off cooldown) bound to
// controller 1 on GPIO 4/5, plus the MCP-facing accessors the shared dual
// skeleton in main.cpp calls on the secondary driver
// (setReceiveAllMode/recoveryCount/consecutiveTxFailures). The filter is
// accept-all from construction — the recorder-bus default the MCP path
// establishes via setReceiveAllMode() after init; the method stays as a
// no-op-shaped reapply so the shared call sites compile unchanged.
// beta08: the ctor also takes (controllerId, mode) so CAN C (CanC, X179
// 18/19 PT powertrain bus) reuses this class as a LISTEN-ONLY third
// instance on controller 2 / GPIO 26-27 — observation only, never TX.
//
// Albert-P4 mechanism port (2026-11-04): the private layer delegates to
// P4TwaiNodeCore (new TWAI node API, IRAM/cache-safe ISR, internal-RAM RX
// ring + TX staging pool). controllerId_ no longer selects hardware (the
// node API binds any free controller at creation, and construction order
// keeps CanA→0/CanB→1/CanC→2); it stays as the bench-inject lane selector.

#include "../can_frame_types.h"
#include "can_driver.h"
#include "can_bench_inject.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include <driver/twai.h>
#pragma GCC diagnostic pop
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "p4_twai_node.h"

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
                          gpio_num_t rxPin = T2CAN_SECONDARY_RX_PIN,
                          int controllerId = kSecondaryControllerId,
                          twai_mode_t mode = TWAI_MODE_NORMAL)
        : txPin_(txPin), rxPin_(rxPin), controllerId_(controllerId),
          core_(txPin, rxPin, /*listenOnly=*/mode == TWAI_MODE_LISTEN_ONLY) {}

    bool init() override
    {
        if (!mutex_)
            mutex_ = xSemaphoreCreateMutex();
        if (!mutex_)
            return false;

        // Consumer (can_rt) is created after setup; injected via
        // setRxConsumer() from app_start_can_task.
        lock();
        esp_err_t err = core_.init(nullptr);
        driverOK_ = err == ESP_OK;
        unlock();
        return driverOK_;
    }

    // Recorder bus: accept-all from construction and setFilters is a no-op
    // (nothing may narrow the secondary bus behind the back of the
    // discovered-ID table). Kept for the CanDriver interface.
    void setFilters(const uint32_t * /*ids*/, uint8_t /*count*/) override {}

    // Accept-all reapply after controller resets, mirroring the MCP path.
    void setReceiveAllMode()
    {
        lock();
        if (core_.healthy())
            core_.applyMaskFilter(0, 0x7FF);
        unlock();
    }

    bool enableInterrupt(void (* /*onReady*/)()) override { return false; }

    void setRxConsumer(TaskHandle_t consumer)
    {
        lock();
        core_.setConsumer(consumer);
        unlock();
    }

    bool read(CanFrame &frame) override
    {
#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)
        // Field-crash replay (2026-11-02): same hook as the primary driver,
        // but on this class's lane — controller 1 = secondary (bus 2),
        // controller 2 = tertiary/CanC (bus 3). Both real drain paths
        // (t2canDrainSecondary / t2canDrainCan3) run on can_rt.
        if (canBenchInjectPop(controllerId_ == kSecondaryControllerId ? 1 : 2, frame))
            return true;
#endif
        for (uint8_t attempt = 0; attempt < kReadDrainBudget; attempt++)
        {
            lock();
            if (!driverOK_)
            {
                tryRecoverLocked();
                unlock();
                return false;
            }

            if (!core_.read(frame))
            {
                if (isBusOffLocked())
                    recoverWithCooldownLocked();
                unlock();
                return false;
            }
            unlock();

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

        // Short timeout lives inside the core (2 ms): modified frames should
        // not be dropped, but long blocks risk starving the RX ring drain
        // (same reasoning as the primary driver).
        bool ok = core_.send(frame);
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

    uint32_t recoveryCount() const { return core_.recoveryCount(); }
    uint8_t consecutiveTxFailures() const { return consecutiveTxFailures_; }

private:
    // Mirror of the MCP driver's injection-dies guard: after this many
    // consecutive failed sends, force a recover attempt.
    static constexpr uint8_t kMaxConsecutiveTxFailures = 5;
    static constexpr uint8_t kReadDrainBudget = 8;
    static constexpr int kSecondaryControllerId = 1;

    bool isBusOffLocked()
    {
        return core_.isBusOff();
    }

    void recoverWithCooldownLocked()
    {
        core_.recoverWithCooldown();
    }

    void tryRecoverLocked()
    {
        core_.tryRecover();
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

public:
#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)
    // Experiment B (crash forensics round 7, 2026-11-03): NO_ACK
    // self-reception flood — same purpose as TWAIDriver::benchSetMode, on
    // this lane. The new API maps NO_ACK onto the node self-test flag;
    // loopback (self-reception) is a node-level capability wired via
    // core_.benchReinit for the round-9 jumper plan. exp_b only; the caller
    // restores the original mode afterwards.
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
    gpio_num_t txPin_;
    gpio_num_t rxPin_;
    // beta08: CAN C reuses this class as a listen-only third instance
    // (controller 2, GPIO 26/27) — same mutex/core shape, no TX path.
    // controllerId_ is now only the bench-inject lane selector (see header
    // comment): the node API binds free controllers at creation and boot
    // construction order preserves the CanA/CanB/CanC mapping.
    int controllerId_;

public:
    // Last esp_err from install/start — printed by the caller on init failure
    // so field logs carry the reason, not a bare "init failed".
    esp_err_t lastError() const { return core_.lastError(); }

    // beta08 listen-only third instance: receiver health for /status can3.
    // Called from the same task as read(), so no lock is taken here.
    // EFLG is an MCP2515 concept; TWAI's closest equivalents are the error
    // counters (tx stays 0 in listen-only, rx counts receive-side errors).
    bool statusInfo(twai_status_info_t &out)
    {
        if (!driverOK_)
            return false;
        return core_.statusInfo(out);
    }

private:
    P4TwaiNodeCore core_;
    SemaphoreHandle_t mutex_ = nullptr;
    bool driverOK_ = false;
    uint8_t consecutiveTxFailures_ = 0;
};
