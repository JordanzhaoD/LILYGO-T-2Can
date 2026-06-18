#pragma once

// ──────────────────────────────────────────────────────────────
// dash_epas_nag.h — EPAS-faithful unified steering nag suppression
//
// Echoes CAN 0x370 (EPAS3P_sysStatus, ~100 Hz) with counter+1 and a
// mean-reverting ~0 Nm torque so the car INFERS hands-on instead of
// reading a forced flag. One 0x370 path covers legacy/HW3/HW4; the
// per-generation DAS hands-on gate (0x399 legacy/HW3, 0x39B Highland
// HW3/HW4) only decides WHEN to inject.
//
// Spec: docs/superpowers/specs/2026-06-17-epas-faithful-nag-unified-design.md
// Default OFF (experimental). bit19 nag stays as default-on baseline;
// this engine takes over only when dashEpasNagEnabled is set (D7).
//
// Deterministic: no millis()/random() inside — time is passed in via
// nowMs; PRNG is a seeded xorshift32. Unit-testable as a plain struct.
// ──────────────────────────────────────────────────────────────

#include <cstdint>
#include <cstring>

#include "dash_tx_evidence.h"

struct DashEpasNagDiag
{
    uint32_t echoCount = 0;
    uint32_t suppressCount = 0;
    uint8_t dasHandsOnState = 0xFF;   // 0xFF = unseen
    uint8_t lastHandsOn = 0;
    const char *lastGate = "idle";
    DashTxEvidence tx{};
    // Read-only observability (never affects behaviour). Snapshot of the last
    // real 0x370 frame seen and the torque our echo used — the side-by-side
    // evidence for whether the echo can ever look like "hands on".
    uint16_t lastRealTorque = 0; // 0x370 bytes[2:3] raw ((b2&0x0F)<<8 | b3)
    uint8_t lastRealCnt = 0;     // 0x370 byte6 & 0x0F
    uint16_t lastEchoTorque = 0; // torque the last successful echo used
};

struct DashEpasNagEngine
{
    // ── constants ────────────────────────────────────────────
    static constexpr uint32_t kCanId{880};          // 0x370 EPAS3P_sysStatus
    static constexpr uint32_t kDasIdLegacy{921};    // 0x399 DAS_status (legacy/HW3/Juniper fallback)
    static constexpr uint32_t kDasIdHighland{923};  // 0x39B DAS_status (Highland HW3/HW4)
    static constexpr uint16_t kTorqueCenter{2050};  // raw for 0.00 Nm  (Nm = raw*0.01 - 20.5)
    static constexpr uint16_t kTorqueMin{1780};     // ~-2.70 Nm
    static constexpr uint16_t kTorqueMax{2500};     // ~+4.50 Nm
    static constexpr uint16_t kTorqueHoldNm{2230};  // B1: HOLD +1.80 Nm (raw 0x08B6, matches NagHandler byte3=0xB6). Real-car 2026-06-18 proved a mean-reverting ~0 Nm walk never suppresses nag.
    static constexpr uint16_t kPullDivisor{16};     // legacy mean-reversion pull strength (unused after B1)
    static constexpr uint16_t kStepSpan{81};        // xorshift % 81 -> [0,80], step = -40..+40
    static constexpr uint32_t kBlackoutMs{1500};    // G6 post-disengage silence window
    static constexpr uint8_t kDasUnseen{0xFF};

    // ── runtime state (fed by dashboard glue) ───────────────
    bool enabled{false};        // mirrors dashEpasNagEnabled (G2)
    bool canActive{false};      // mirrors canActive (G3)
    bool defenseEnabled{false}; // mirrors dashDefenseEnabled (G1)
    uint8_t dasHandsOnState{kDasUnseen};
    bool prevApActive{false};
    bool apBlackoutActive{false};
    uint32_t apDisengageMs{0};
    uint16_t torqueWalk{kTorqueCenter};
    uint32_t prng{0xDEADBEEFu};
    DashEpasNagDiag diag{};

    // ── pure helpers ─────────────────────────────────────────

    // EPAS 0x370 additive checksum: byte7 = (sum(data[0..6]) + 0x73) & 0xFF
    // 0x73 = (0x370 & 0xFF) + (0x370 >> 8) = 0x70 + 0x03. Matches handlers.h:778.
    static uint8_t teslaChecksum0x370(const uint8_t d[7])
    {
        uint16_t sum = 0x70u + 0x03u;
        for (int i = 0; i < 7; ++i)
            sum += d[i];
        return static_cast<uint8_t>(sum & 0xFFu);
    }

    static uint16_t clampTorque(int32_t v)
    {
        if (v < static_cast<int32_t>(kTorqueMin))
            return kTorqueMin;
        if (v > static_cast<int32_t>(kTorqueMax))
            return kTorqueMax;
        return static_cast<uint16_t>(v);
    }

    uint32_t xorshift32()
    {
        uint32_t x = prng;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        prng = x;
        return x;
    }

    // B1 fix (2026-06-18 real-car evidence: echoTorque=1908=-1.42Nm, txOk=74078
    // 100% success, yet nag still fired). The mean-reverting walk toward 0 Nm is
    // the root cause — the car ignores torque that drifts back to 0. HOLD a
    // sustained +1.80 Nm so the car reads continuous driver input, matching the
    // proven NagHandler byte3=0xB6 strategy. The legacy xorshift walk below is
    // kept for reference but no longer drives the echo.
    uint16_t nextTorque()
    {
        return kTorqueHoldNm;
    }

    // ── observation (called by dashboard glue) ───────────────
    void noteRuntime(bool enabled_, bool canActive_, bool defenseEnabled_)
    {
        enabled = enabled_;
        canActive = canActive_;
        defenseEnabled = defenseEnabled_;
    }

    // 0x399 / 0x39B DAS_status: hands-on state = byte5 bits[5:2]
    void onDasGate(const uint8_t data[8])
    {
        dasHandsOnState = static_cast<uint8_t>((data[5] >> 2) & 0x0Fu);
        diag.dasHandsOnState = dasHandsOnState;
    }

    // Track AP falling edge for the G6 blackout window.
    void noteApState(bool active, uint32_t nowMs)
    {
        if (prevApActive && !active)
        {
            apDisengageMs = nowMs;
            apBlackoutActive = true;
        }
        prevApActive = active;
    }

    // ── gate G1-G6 ───────────────────────────────────────────
    bool shouldInject(uint8_t handsOn, uint32_t nowMs) const
    {
        if (!enabled || !canActive || !defenseEnabled)          // G1 G2 G3
            return false;
        if (handsOn == 1)                                        // G4: real hand detected
            return false;
        const uint8_t das = dasHandsOnState;
        if (das == 0 || das == 8)                                // G5: NOT_REQD / SUSPENDED
            return false;                                        //     (0xFF unseen -> passes -> D5 inject)
        if (apBlackoutActive && (nowMs - apDisengageMs) < kBlackoutMs) // G6
            return false;
        return true;
    }

    // ── echo builder ─────────────────────────────────────────
    // On a valid 8-byte 0x370 frame: if gate open, write faithful echo to outData[8], return true.
    bool tryBuildEcho(const uint8_t inData[8], uint8_t dlc, uint32_t nowMs, uint8_t outData[8])
    {
        if (dlc != 8)
        {
            diag.suppressCount++;
            diag.lastGate = "invalid_dlc";
            return false;
        }

        // Snapshot the real 0x370 we just saw (observability only).
        diag.lastRealTorque = static_cast<uint16_t>(((inData[2] & 0x0Fu) << 8) | inData[3]);
        diag.lastRealCnt = static_cast<uint8_t>(inData[6] & 0x0Fu);

        const uint8_t handsOn = static_cast<uint8_t>((inData[4] >> 6) & 0x03u);
        diag.lastHandsOn = handsOn;

        if (!shouldInject(handsOn, nowMs))
        {
            diag.suppressCount++;
            diag.lastGate = "gated";
            return false;
        }

        const uint16_t torq = nextTorque();
        diag.lastEchoTorque = torq;
        outData[0] = inData[0];
        outData[1] = inData[1];
        outData[2] = static_cast<uint8_t>((inData[2] & 0xF0u) | ((torq >> 8) & 0x0Fu));
        outData[3] = static_cast<uint8_t>(torq & 0xFFu);
        outData[4] = inData[4] | 0x40u;               // B2: force handsOnLevel active (bit6=1). Real-car 2026-06-18: echoTorque=2230 matched NagHandler yet nag fired — torque alone is insufficient, the car disbelieves "hands off + torque". Matches NagHandler byte4|=0x40.
        outData[5] = inData[5];
        uint8_t cnt = static_cast<uint8_t>(inData[6] & 0x0Fu);
        cnt = static_cast<uint8_t>((cnt + 1) & 0x0Fu);
        outData[6] = static_cast<uint8_t>((inData[6] & 0xF0u) | cnt);
        outData[7] = teslaChecksum0x370(outData);     // sum(outData[0..6]) + 0x73

        diag.echoCount++;
        diag.lastGate = "inject";
        return true;
    }

    void reset()
    {
        torqueWalk = kTorqueCenter;
        prng = 0xDEADBEEFu;
        dasHandsOnState = kDasUnseen;
        diag.dasHandsOnState = kDasUnseen;
        prevApActive = false;
        apBlackoutActive = false;
        apDisengageMs = 0;
    }
};
