#pragma once
// Bus2 MCP2515 error recovery for DRIVER_T2CAN_DUAL builds.
// Detects BUS-OFF and TX-error states via EFLG register and performs
// a soft reset + reconfiguration sequence to restore operation.

#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#else
#include <Arduino.h>
#endif

#ifdef DRIVER_T2CAN_DUAL

#include "drivers/esp32_mcp2515_driver.h"

class T2CanBus2Recovery {
public:
    void begin(ESP32_MCP2515Driver *driver) {
        driver_ = driver;
        lastErrorMs_ = 0;
        consecutiveErrors_ = 0;
        totalRecoveries_ = 0;
    }

    bool tick() {
        if (!driver_) return false;

        uint8_t eflg = driver_->mcp().getErrorFlags();

        // EFLG_TXBO (bit 5): TX buffer overflow / bus-off
        if (eflg & 0x20) {
            uint32_t now = millis();
            if (now - lastErrorMs_ < 1000) return false;

            lastErrorMs_ = now;
            consecutiveErrors_++;
            totalRecoveries_++;

            Serial.printf("bus2 recovery: EFLG=0x%02X, attempt #%u (total %u)\n",
                          eflg, consecutiveErrors_, totalRecoveries_);

            if (driver_->init()) {
                driver_->mcp().setReceiveAllMode();
                Serial.println("bus2 recovery: MCP2515 re-initialized OK");
                consecutiveErrors_ = 0;
            } else {
                Serial.println("bus2 recovery: MCP2515 re-init FAILED");
                lastErrorMs_ = now + 1000;
            }
            return true;
        }

        if (consecutiveErrors_ > 0 && eflg == 0) {
            consecutiveErrors_ = 0;
        }
        return false;
    }

    uint32_t totalRecoveries() const { return totalRecoveries_; }
    uint16_t consecutiveErrors() const { return consecutiveErrors_; }

private:
    ESP32_MCP2515Driver *driver_ = nullptr;
    uint32_t lastErrorMs_ = 0;
    uint16_t consecutiveErrors_ = 0;
    uint32_t totalRecoveries_ = 0;
};

#endif // DRIVER_T2CAN_DUAL
