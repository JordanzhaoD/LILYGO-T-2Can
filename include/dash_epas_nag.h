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
// Spec: docs/superpowers/specs/2026-06-19-upstream-epas-nag-modec-align-design.md
//      (supersedes 2026-06-17-epas-faithful-nag-unified-design.md — Mode-C demand-state)
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
    // Mode-C observability (spec §5 D8).
    uint8_t lastDemandState = 0xFF;        // das snapshot at last inject
    uint16_t lastDemandTorque = 0;         // torque the demand machine returned
    uint8_t lastHandsLevel = 0;            // level written to byte4
    const char *lastDemandGate = "idle";  // s1_hold/s2_mild/strong_ramp/...
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
    // ── Mode-C demand-state machine (spec §6.2) ───────────────
    bool apActive{false};       // current AP state (noteApState refreshes)
    int16_t steerAngleX10{0};   // 0x129 angle x10 (onSteeringAngle); dir = >0?-1:+1
    uint32_t s1EnterMs{0};
    uint32_t s2EnterMs{0};
    uint32_t strongEnterMs{0};
    uint32_t s2HoldUntilMs{0};
    int16_t mildWalk{kTorqueCenter};
    int16_t s2HoldRaw{kTorqueCenter};
    uint8_t s2HoldLevel{0};
    bool s2Level2Active{false};
    uint8_t prevDas{0};         // ★ 0 (not 0xFF) so first 0xFF triggers strongEnterMs (D5)
    int16_t lastRaw{kTorqueCenter};
    uint8_t lastLevel{0};
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

    static uint8_t handsLevelFromRaw(int32_t raw)
    {
        const int32_t center = static_cast<int32_t>(kTorqueCenter);
        int32_t a = (raw >= center) ? (raw - center) : (center - raw);
        if (a >= 200)
            return 2;
        if (a >= 100)
            return 1;
        return 0;
    }

    // int16_t clamp for negative torque bands (clampTorque returns uint16_t).
    static int16_t clampInt16(int32_t v, int32_t lo, int32_t hi)
    {
        if (v < lo)
            return static_cast<int16_t>(lo);
        if (v > hi)
            return static_cast<int16_t>(hi);
        return static_cast<int16_t>(v);
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

    // ── Mode-C demand-state torque machine (spec §6.3) ───────────────
    // Returns true + fills outTorque/outLevel when this 0x370 should be echoed;
    // false (demand_idle) when the demand machine says "don't inject this frame".
    bool nextDemandTorque(uint8_t das, uint32_t nowMs, uint16_t &outTorque, uint8_t &outLevel)
    {
        // edge detection — record entry timestamps per state
        bool isStrong = (das == 3 || das == 4 || das == 5 || das == 0xFF);
        bool prevStrong = (prevDas == 3 || prevDas == 4 || prevDas == 5 || prevDas == 0xFF);
        if (prevDas != 1 && das == 1) s1EnterMs = nowMs;
        if (das != 1) s1EnterMs = 0;
        if (prevDas != 2 && das == 2) s2EnterMs = nowMs;
        if (das != 2) { s2EnterMs = 0; s2HoldUntilMs = 0; s2Level2Active = false; }
        if (!prevStrong && isStrong) strongEnterMs = nowMs;
        if (!isStrong) strongEnterMs = 0;
        prevDas = das;

        diag.lastDemandState = das;

        // AP gate + excluded das states (spec §6.3, upstream das_ap_state<2 analog)
        if (!apActive || das == 0 || das == 8 || das == 15)
        {
            lastRaw = kTorqueCenter; lastLevel = 0;
            diag.lastDemandGate = "demand_idle";
            return false;
        }

        // direction: reverse-assist from 0x129 (spec §6.3 / upstream fsd_handler.c:973)
        int8_t dir = (steerAngleX10 > 0) ? -1 : 1;

        if (das == 1)
        {
            if (s1EnterMs != 0 && (nowMs - s1EnterMs) < 500u)
            {
                outTorque = (uint16_t)lastRaw; outLevel = lastLevel;
                diag.lastDemandGate = "s1_hold";
                return true;
            }
            lastRaw = kTorqueCenter; lastLevel = 0;
            diag.lastDemandGate = "s1_done";
            return false;
        }

        if (das == 2)
        {
            if (s2EnterMs != 0 && (nowMs - s2EnterMs) < 2000u)
            {
                diag.lastDemandGate = "s2_delay";
                return false;
            }
            if (nowMs < s2HoldUntilMs)
            {
                outTorque = (uint16_t)s2HoldRaw; outLevel = s2HoldLevel;
                diag.lastDemandGate = "s2_hold";
                return true;
            }
            int16_t lo = (dir < 0) ? 1850 : 2100;
            int16_t hi = (dir < 0) ? 2000 : 2250;
            if (mildWalk < lo || mildWalk > hi)
                mildWalk = (int16_t)((lo + hi) / 2);
            mildWalk += (int16_t)((int)(xorshift32() % 25u) - 12);
            mildWalk = clampInt16(mildWalk, lo, hi);
            outTorque = (uint16_t)mildWalk;
            outLevel = handsLevelFromRaw(mildWalk);
            if (handsLevelFromRaw(mildWalk) == 2 && !s2Level2Active)
            {
                s2HoldUntilMs = nowMs + 1000u;
                s2HoldRaw = mildWalk; s2HoldLevel = 2;
            }
            s2Level2Active = (handsLevelFromRaw(mildWalk) == 2);
            lastRaw = mildWalk; lastLevel = outLevel;
            diag.lastDemandGate = "s2_mild";
            return true;
        }

        // strong: das==3/4/5 (and 0xFF D5 fallback). 1s pause, then 0->2.1Nm ramp, 1.5s cycle.
        if (strongEnterMs != 0 && (nowMs - strongEnterMs) < 1000u)
        {
            diag.lastDemandGate = "strong_delay";
            return false;
        }
        uint32_t active = strongEnterMs ? (nowMs - strongEnterMs - 1000u) : 0u;
        uint16_t phase = (uint16_t)(active % 1500u);
        int16_t mag = (phase < 500u) ? (int16_t)(((int32_t)phase * 210) / 500) : 210;
        int16_t torq = (int16_t)(2050 + dir * mag);
        outTorque = (uint16_t)clampTorque((int32_t)torq);
        outLevel = handsLevelFromRaw(torq);
        lastRaw = torq; lastLevel = outLevel;
        diag.lastDemandGate = (phase < 500u) ? "strong_ramp" : "strong_hold";
        return true;
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
        apActive = active;                       // ★ Mode-C AP gate input
        if (prevApActive && !active)
        {
            apDisengageMs = nowMs;
            apBlackoutActive = true;
        }
        prevApActive = active;
    }

    // 0x129 steering angle (spec §6.3 dir input). x10 deg, offset-binary already
    // decoded by mcp2515_dashboard.h:689; 0x3FFF-invalid arrives as 0 here.
    void onSteeringAngle(int16_t x10)
    {
        steerAngleX10 = x10;
    }

    // ── gate G1-G6 ───────────────────────────────────────────
    bool shouldInject(uint8_t handsOn, uint32_t nowMs) const
    {
        if (!enabled || !canActive || !defenseEnabled)          // G1 G2 G3
            return false;
        if (handsOn == 1)                                        // G4: real hand detected
            return false;
        const uint8_t das = dasHandsOnState;
        if (das == 0 || das == 8 || das == 15)                   // G5: NOT_REQD / SUSPENDED / (15)
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

        uint16_t torq = 0;
        uint8_t level = 0;
        if (!nextDemandTorque(dasHandsOnState, nowMs, torq, level))
        {
            diag.suppressCount++;
            diag.lastGate = diag.lastDemandGate; // demand_idle / s1_done / *_delay
            return false;
        }

        diag.lastDemandTorque = torq;
        diag.lastHandsLevel = level;
        diag.lastEchoTorque = torq;
        diag.lastGate = diag.lastDemandGate;

        outData[0] = inData[0];
        outData[1] = inData[1];
        outData[2] = static_cast<uint8_t>((inData[2] & 0xF0u) | ((torq >> 8) & 0x0Fu));
        outData[3] = static_cast<uint8_t>(torq & 0xFFu);
        outData[4] = static_cast<uint8_t>((inData[4] & 0x3Fu) | (uint8_t)(level << 6)); // ★ level derived
        outData[5] = inData[5];
        uint8_t cnt = static_cast<uint8_t>(inData[6] & 0x0Fu);
        cnt = static_cast<uint8_t>((cnt + 1) & 0x0Fu);
        outData[6] = static_cast<uint8_t>((inData[6] & 0xF0u) | cnt);
        outData[7] = teslaChecksum0x370(outData);

        diag.echoCount++;
        return true;
    }

    void reset()
    {
        prng = 0xDEADBEEFu;
        dasHandsOnState = kDasUnseen;
        diag.dasHandsOnState = kDasUnseen;
        prevApActive = false;
        apBlackoutActive = false;
        apDisengageMs = 0;
        // Mode-C machine
        apActive = false;
        steerAngleX10 = 0;
        s1EnterMs = s2EnterMs = strongEnterMs = s2HoldUntilMs = 0;
        mildWalk = kTorqueCenter; s2HoldRaw = kTorqueCenter; s2HoldLevel = 0;
        s2Level2Active = false;
        prevDas = 0; lastRaw = kTorqueCenter; lastLevel = 0;
    }
};
