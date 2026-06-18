#pragma once
// Bus2 discovered-ID table with LRU eviction for DRIVER_T2CAN_DUAL builds.
// Replaces the inline array in main.cpp with a bounded table that evicts
// the least-recently-seen entry when full.
//
// Thread safety: record() runs on the CAN task, get()/count() on the web
// task. A portMUX_TYPE spinlock serialises access so readers never see a
// partially-written entry (torn id/data during eviction).

#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#else
#include <Arduino.h>
#endif

#ifdef DRIVER_T2CAN_DUAL

#include "can_frame_types.h"

struct T2CanBus2Entry {
    uint16_t id;
    uint8_t  dlc;
    uint8_t  data[8];
    uint32_t count;
    uint32_t lastSeenMs;
};

class T2CanBus2Table {
public:
    static constexpr uint16_t kMaxIds = 160;

    T2CanBus2Table() : mux_(portMUX_INITIALIZER_UNLOCKED) {}

    bool record(const CanFrame &frame) {
        if (frame.id & 0x80000000UL) return false;
        uint16_t sid = (uint16_t)(frame.id & 0x7FF);
        uint32_t now = millis();

        portENTER_CRITICAL(&mux_);
        uint16_t n = count_;
        for (uint16_t i = 0; i < n; i++) {
            if (entries_[i].id == sid) {
                entries_[i].dlc = frame.dlc;
                memcpy(entries_[i].data, frame.data, 8);
                entries_[i].count++;
                entries_[i].lastSeenMs = now;
                portEXIT_CRITICAL(&mux_);
                return false;
            }
        }

        uint16_t slot;
        if (n < kMaxIds) {
            slot = n;
            count_ = n + 1;
        } else {
            slot = findLruLocked();
            Serial.printf("bus2 LRU evict: 0x%03X (count=%u) for new 0x%03X\n",
                          entries_[slot].id, entries_[slot].count, sid);
        }

        entries_[slot].id = sid;
        entries_[slot].dlc = frame.dlc;
        memcpy(entries_[slot].data, frame.data, 8);
        entries_[slot].count = 1;
        entries_[slot].lastSeenMs = now;
        portEXIT_CRITICAL(&mux_);
        return true;
    }

    uint16_t count() const {
        portENTER_CRITICAL(&mux_);
        uint16_t c = count_;
        portEXIT_CRITICAL(&mux_);
        return c;
    }

    bool get(uint16_t index, uint16_t *id, uint8_t *dlc,
             uint8_t data[8], uint32_t *count) const {
        portENTER_CRITICAL(&mux_);
        if (index >= count_) {
            portEXIT_CRITICAL(&mux_);
            return false;
        }
        *id = entries_[index].id;
        *dlc = entries_[index].dlc;
        memcpy(data, entries_[index].data, 8);
        *count = entries_[index].count;
        portEXIT_CRITICAL(&mux_);
        return true;
    }

private:
    // Must be called with mux_ held.
    uint16_t findLruLocked() const {
        uint16_t lruIdx = 0;
        uint32_t oldest = entries_[0].lastSeenMs;
        for (uint16_t i = 1; i < count_; i++) {
            // Signed difference handles millis() 49.7-day wraparound.
            if ((int32_t)(entries_[i].lastSeenMs - oldest) < 0) {
                oldest = entries_[i].lastSeenMs;
                lruIdx = i;
            }
        }
        return lruIdx;
    }

    T2CanBus2Entry entries_[kMaxIds];
    uint16_t count_ = 0;
    mutable portMUX_TYPE mux_;
};

extern T2CanBus2Table g_bus2Table;

#endif // DRIVER_T2CAN_DUAL
