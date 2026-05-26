#pragma once
// Bus2 discovered-ID table with LRU eviction for DRIVER_T2CAN_DUAL builds.
// Replaces the inline array in main.cpp with a bounded table that evicts
// the least-recently-seen entry when full.

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

    bool record(const CanFrame &frame) {
        if (frame.id & 0x80000000UL) return false;
        uint16_t sid = (uint16_t)(frame.id & 0x7FF);
        uint32_t now = millis();

        uint16_t n = count_;
        for (uint16_t i = 0; i < n; i++) {
            if (entries_[i].id == sid) {
                entries_[i].dlc = frame.dlc;
                memcpy(entries_[i].data, frame.data, 8);
                entries_[i].count++;
                entries_[i].lastSeenMs = now;
                return false;
            }
        }

        uint16_t slot;
        if (n < kMaxIds) {
            slot = n;
            count_ = n + 1;
        } else {
            slot = findLru();
            Serial.printf("bus2 LRU evict: 0x%03X (count=%u) for new 0x%03X\n",
                          entries_[slot].id, entries_[slot].count, sid);
        }

        entries_[slot].id = sid;
        entries_[slot].dlc = frame.dlc;
        memcpy(entries_[slot].data, frame.data, 8);
        entries_[slot].count = 1;
        entries_[slot].lastSeenMs = now;
        return true;
    }

    uint16_t count() const { return count_; }

    bool get(uint16_t index, uint16_t *id, uint8_t *dlc,
             uint8_t data[8], uint32_t *count) const {
        if (index >= count_) return false;
        *id = entries_[index].id;
        *dlc = entries_[index].dlc;
        memcpy(data, entries_[index].data, 8);
        *count = entries_[index].count;
        return true;
    }

private:
    uint16_t findLru() const {
        uint16_t lruIdx = 0;
        uint32_t oldest = entries_[0].lastSeenMs;
        for (uint16_t i = 1; i < count_; i++) {
            if (entries_[i].lastSeenMs < oldest) {
                oldest = entries_[i].lastSeenMs;
                lruIdx = i;
            }
        }
        return lruIdx;
    }

    T2CanBus2Entry entries_[kMaxIds];
    volatile uint16_t count_ = 0;
};

extern T2CanBus2Table g_bus2Table;

#endif // DRIVER_T2CAN_DUAL
