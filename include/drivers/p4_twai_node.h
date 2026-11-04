#pragma once

// P4 CAN hot path on the new TWAI node API — the Albert-P4 mechanism port
// (2026-11-04, docs/superpowers/specs/2026-11-04-p4-twai-node-hotpath-design.md).
// The in-car-stable reference firmware runs the whole CAN hot path
// IRAM/cache-safe with RX serviced in the ISR callback and hot state in
// internal RAM; our legacy queue driver kept all of it in flash cache and is
// the prime suspect for the 12-39 s field crash family. This core is shared
// by TWAIDriver (CanA) and P4TwaiSecondaryDriver (CanB/CanC) under the
// DRIVER_P4_3CAN gate; the T2CAN dual build never compiles it.
//
// Mechanism notes that keep this correct:
// - The driver's own ISR drains the hardware RX FIFO and calls on_rx_done
//   once per frame (esp_twai_onchip.c _node_isr_main), so the callback does
//   exactly one twai_node_receive_from_isr -> ring push -> task-notify and
//   never blocks. With CONFIG_TWAI_ISR_CACHE_SAFE=y the callback must live
//   in IRAM (IRAM_ATTR).
// - _node_queue_tx mounts immediately only when the hardware is idle; when
//   busy it queues the CALLER's frame pointer, so transmit() must run from
//   slots this core owns (kTxSlots, internal RAM) freed by on_tx_done.
// - Ring and pool are heap_caps-allocated with MALLOC_CAP_INTERNAL at init:
//   internal DRAM stays reachable with the flash cache disabled, and the
//   bus_enable boot gating keeps allocating them only for constructed lanes.

#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)

#include "../can_frame_types.h"

#include <esp_attr.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_twai.h>
#include <esp_twai_onchip.h>
#include <esp_twai_types.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// twai_status_info_t / twai_state_t are used ONLY as the accessor currency
// the main.cpp consumers already speak; no legacy driver function is called.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include <driver/twai.h>
#pragma GCC diagnostic pop

struct P4NodeRingSlot
{
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
};

struct P4NodeTxSlot
{
    twai_frame_t frame;
    uint8_t data[8];
};

class P4TwaiNodeCore
{
public:
    static constexpr uint16_t kRingFrames = 128; // >= legacy rx queue depth
    static constexpr uint8_t kTxSlots = 18;      // 16 queued + 1 mounted + margin
    static constexpr uint32_t BUSOFF_COOLDOWN_MS = 1000;

    P4TwaiNodeCore(gpio_num_t txPin, gpio_num_t rxPin, bool listenOnly)
        : txPin_(txPin), rxPin_(rxPin), listenOnly_(listenOnly) {}

    ~P4TwaiNodeCore() { deinit(); }

    esp_err_t init(TaskHandle_t consumer)
    {
        consumer_ = consumer;
        if (!ring_)
        {
            // Internal RAM on purpose: the ISR writes here, including inside
            // cache-disabled windows (flash erase/write, PSRAM maintenance).
            ring_ = (P4NodeRingSlot *)heap_caps_calloc(kRingFrames, sizeof(P4NodeRingSlot),
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            txPool_ = (P4NodeTxSlot *)heap_caps_calloc(kTxSlots, sizeof(P4NodeTxSlot),
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            if (!ring_ || !txPool_)
            {
                deinit();
                lastErr_ = ESP_ERR_NO_MEM;
                return lastErr_;
            }
        }
        return createNode();
    }

    void deinit()
    {
        if (node_)
        {
            twai_node_disable(node_);
            twai_node_delete(node_);
            node_ = nullptr;
        }
        if (ring_)
        {
            heap_caps_free(ring_);
            ring_ = nullptr;
        }
        if (txPool_)
        {
            heap_caps_free(txPool_);
            txPool_ = nullptr;
        }
        txSlotMask_ = 0;
        head_ = 0;
        tail_ = 0;
        enabled_ = false;
    }

    void setConsumer(TaskHandle_t consumer) { consumer_ = consumer; }

    bool healthy() const { return node_ != nullptr && enabled_; }
    esp_err_t lastError() const { return lastErr_; }
    uint32_t rxRingDrops() const { return rxRingDrops_; }
    uint32_t recoveryCount() const { return recoveries_; }
    uint16_t ringCount() const
    {
        uint16_t head = head_;
        uint16_t tail = tail_;
        return (uint16_t)((head - tail) & (kRingFrames - 1));
    }

    // Task side (can_rt): pop one frame. Non-blocking, same contract as the
    // legacy twai_receive(…, 0) path.
    bool read(CanFrame &frame)
    {
        uint16_t tail = tail_;
        if (tail == head_)
            return false;
        const P4NodeRingSlot &slot = ring_[tail];
        frame.id = slot.id;
        frame.dlc = (slot.dlc <= 8) ? slot.dlc : 8;
        memset(frame.data, 0, 8);
        memcpy(frame.data, slot.data, frame.dlc);
        tail_ = (uint16_t)((tail + 1) & (kRingFrames - 1));
        return true;
    }

    // Task side: transmit from an owned slot (see file header: the driver
    // queues the caller's pointer when the hardware is busy). 2 ms timeout,
    // same policy as the legacy drivers.
    bool send(const CanFrame &frame)
    {
        if (!healthy())
            return false;
        uint32_t mask = txSlotMask_;
        for (uint8_t i = 0; i < kTxSlots; i++)
        {
            uint32_t bit = 1u << i;
            if (mask & bit)
                continue;
            if (!__atomic_compare_exchange_n(&txSlotMask_, &mask, mask | bit,
                                             false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
            {
                i--; // mask reloaded by CAS failure; re-test this index
                continue;
            }
            P4NodeTxSlot &slot = txPool_[i];
            uint8_t dlc = (frame.dlc <= 8) ? frame.dlc : 8;
            slot.frame.header.id = frame.id;
            slot.frame.header.dlc = dlc;
            slot.frame.buffer = slot.data;
            slot.frame.buffer_len = sizeof(slot.data);
            memcpy(slot.data, frame.data, dlc);
            esp_err_t err = twai_node_transmit(node_, &slot.frame, 2);
            if (err == ESP_OK)
                return true;
            __atomic_fetch_and(&txSlotMask_, ~bit, __ATOMIC_ACQ_REL);
            lastErr_ = err;
            txFailures_++;
            return false;
        }
        // Pool exhausted: hardware queue is full of our live frames — behave
        // exactly like a TX timeout so the caller's failure guards engage.
        txFailures_++;
        return false;
    }

    // Hardware mask filter via the (base, differ) pair the shared setFilters
    // computes. New-API mask polarity is INVERTED vs the legacy acceptance
    // mask: 1 = must match (legacy: 1 = don't care), so the care-mask is
    // ~differ restricted to the 11 std-ID bits.
    void applyMaskFilter(uint32_t base, uint32_t differ)
    {
        if (!node_)
            return;
        twai_mask_filter_config_t cfg = {};
        cfg.id = base & 0x7FF;
        cfg.mask = ~differ & 0x7FF; // polarity: 1 = must match
        cfg.no_fd = true;           // classic CAN buses only
        twai_node_disable(node_);
        twai_node_config_mask_filter(node_, 0, &cfg);
        twai_node_enable(node_);
        enabled_ = true;
    }

    // Legacy-shaped accessor so the main.cpp consumers (can3 /status,
    // recorder-meta missed counter) stay byte-identical.
    bool statusInfo(twai_status_info_t &out)
    {
        memset(&out, 0, sizeof(out));
        if (!node_)
        {
            out.state = TWAI_STATE_STOPPED;
            return false;
        }
        twai_node_status_t st = {};
        twai_node_record_t rec = {};
        if (twai_node_get_info(node_, &st, &rec) != ESP_OK)
        {
            out.state = TWAI_STATE_STOPPED;
            return false;
        }
        switch (st.state)
        {
        case TWAI_ERROR_ACTIVE:
        case TWAI_ERROR_WARNING:
        case TWAI_ERROR_PASSIVE:
            out.state = recovering_ ? TWAI_STATE_RECOVERING : TWAI_STATE_RUNNING;
            if (st.state == TWAI_ERROR_ACTIVE)
                recovering_ = false;
            break;
        case TWAI_ERROR_BUS_OFF:
            out.state = recovering_ ? TWAI_STATE_RECOVERING : TWAI_STATE_BUS_OFF;
            break;
        default:
            out.state = TWAI_STATE_STOPPED;
            break;
        }
        out.tx_error_counter = st.tx_error_count;
        out.rx_error_counter = st.rx_error_count;
        out.msgs_to_tx = st.tx_queue_remaining;
        out.msgs_to_rx = ringCount();
        out.rx_missed_count = rxRingDrops_; // frames lost to consumer keep-up
        out.tx_failed_count = txFailures_;
        out.arb_lost_count = 0; // not exposed by the new driver
        out.bus_error_count = rec.bus_err_num;
        out.rx_overrun_count = 0; // hw fifo overrun handled driver-side
        return true;
    }

    bool isBusOff()
    {
        if (!node_)
            return false;
        twai_node_status_t st = {};
        if (twai_node_get_info(node_, &st, nullptr) != ESP_OK)
            return false;
        return st.state == TWAI_ERROR_BUS_OFF;
    }

    void recoverWithCooldown()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS)
            return;
        lastRecovery_ = now;
        recoveries_++;
        recovering_ = true;
        lastErr_ = twai_node_recover(node_);
    }

    void tryRecover()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS * 10)
            return;
        lastRecovery_ = now;
        recoveries_++;
        recovering_ = true;
        // Recover-through-reinit: a node wedged past twai_node_recover gets a
        // fresh controller, mirroring the legacy stop/uninstall/install path.
        deinitRingPreserved();
        lastErr_ = createNode();
    }

    // Bench hooks (exp_b / round-9 jumper plan): reinit with self-test /
    // loopback flags so a bench flood drives the REAL ISR path.
    bool benchReinit(bool selfTest, bool loopback)
    {
        deinitRingPreserved();
        benchSelfTest_ = selfTest;
        benchLoopback_ = loopback;
        return createNode() == ESP_OK;
    }

    uint8_t consecutiveTxFailures = 0; // owned by the enclosing driver class

private:
    esp_err_t createNode()
    {
        twai_onchip_node_config_t cfg = {};
        cfg.io_cfg.tx = txPin_;
        cfg.io_cfg.rx = rxPin_;
        cfg.io_cfg.quanta_clk_out = GPIO_NUM_NC;
        cfg.io_cfg.bus_off_indicator = GPIO_NUM_NC;
        cfg.bit_timing.bitrate = 500000;
        cfg.fail_retry_cnt = -1; // legacy default: retry forever
        cfg.tx_queue_depth = 16;
        cfg.intr_priority = 0;
        cfg.flags.enable_listen_only = listenOnly_ ? 1 : 0;
        cfg.flags.enable_self_test = benchSelfTest_ ? 1 : 0;
        cfg.flags.enable_loopback = benchLoopback_ ? 1 : 0;

        esp_err_t err = twai_new_node_onchip(&cfg, &node_);
        if (err != ESP_OK)
        {
            node_ = nullptr;
            lastErr_ = err;
            return err;
        }
        twai_event_callbacks_t cbs = {};
        cbs.on_rx_done = onRxDone;
        cbs.on_tx_done = onTxDone;
        err = twai_node_register_event_callbacks(node_, &cbs, this);
        if (err != ESP_OK)
        {
            lastErr_ = err;
            return err;
        }
        // Accept-all mask while still disabled (config_mask_filter requires
        // the disabled state): differ = all 11 bits -> care-mask 0.
        twai_mask_filter_config_t acceptAll = {};
        acceptAll.mask = 0;
        acceptAll.no_fd = true;
        twai_node_config_mask_filter(node_, 0, &acceptAll);
        err = twai_node_enable(node_);
        if (err != ESP_OK)
        {
            lastErr_ = err;
            return err;
        }
        enabled_ = true;
        lastErr_ = ESP_OK;
        return ESP_OK;
    }

    // Full reinit keeps the ring/pool allocations (SPSC state resets are
    // safe: can_rt is the only reader and reinit runs under the class mutex).
    void deinitRingPreserved()
    {
        if (node_)
        {
            twai_node_disable(node_);
            twai_node_delete(node_);
            node_ = nullptr;
        }
        txSlotMask_ = 0;
        head_ = 0;
        tail_ = 0;
        enabled_ = false;
    }

    static IRAM_ATTR bool onRxDone(twai_node_handle_t node,
                                   const twai_rx_done_event_data_t *edata,
                                   void *ctx)
    {
        (void)node;
        (void)edata;
        P4TwaiNodeCore *self = (P4TwaiNodeCore *)ctx;
        twai_frame_t fr = {};
        uint8_t buf[8];
        fr.buffer = buf;
        fr.buffer_len = sizeof(buf);
        BaseType_t woken = pdFALSE;
        if (twai_node_receive_from_isr(node, &fr) == ESP_OK)
        {
            uint16_t next = (uint16_t)((self->head_ + 1) & (kRingFrames - 1));
            if (next != self->tail_)
            {
                P4NodeRingSlot &slot = self->ring_[self->head_];
                slot.id = fr.header.id;
                slot.dlc = (fr.header.dlc <= 8) ? fr.header.dlc : 8;
                memcpy(slot.data, buf, slot.dlc);
                self->head_ = next;
            }
            else
            {
                self->rxRingDrops_ = self->rxRingDrops_ + 1;
            }
            if (self->consumer_)
                vTaskNotifyGiveFromISR(self->consumer_, &woken);
        }
        return woken == pdTRUE;
    }

    static IRAM_ATTR bool onTxDone(twai_node_handle_t node,
                                   const twai_tx_done_event_data_t *edata,
                                   void *ctx)
    {
        (void)node;
        P4TwaiNodeCore *self = (P4TwaiNodeCore *)ctx;
        if (edata && edata->done_tx_frame && self->txPool_)
        {
            uintptr_t offset = (uintptr_t)edata->done_tx_frame - (uintptr_t)&self->txPool_[0].frame;
            if (offset < kTxSlots * sizeof(P4NodeTxSlot) &&
                offset % sizeof(P4NodeTxSlot) == 0)
            {
                uint8_t idx = (uint8_t)(offset / sizeof(P4NodeTxSlot));
                __atomic_fetch_and(&self->txSlotMask_, ~(1u << idx), __ATOMIC_ACQ_REL);
            }
        }
        return false;
    }

    gpio_num_t txPin_;
    gpio_num_t rxPin_;
    bool listenOnly_;
    bool benchSelfTest_ = false;
    bool benchLoopback_ = false;
    twai_node_handle_t node_ = nullptr;
    P4NodeRingSlot *ring_ = nullptr;
    P4NodeTxSlot *txPool_ = nullptr;
    volatile uint16_t head_ = 0; // ISR-side write only
    volatile uint16_t tail_ = 0; // can_rt-side write only
    volatile uint32_t rxRingDrops_ = 0;
    volatile uint32_t txSlotMask_ = 0; // __atomic ops: task claims, ISR frees
    TaskHandle_t consumer_ = nullptr;
    bool enabled_ = false;
    bool recovering_ = false;
    esp_err_t lastErr_ = ESP_OK;
    uint32_t lastRecovery_ = 0;
    uint32_t recoveries_ = 0;
    uint32_t txFailures_ = 0;
};

#endif // defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)
