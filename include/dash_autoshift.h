#pragma once

// Auto gear shift (Legacy Intel Model X) — engine core.
//
// Platform-free C++: no Arduino/ESP includes, so the native harness
// (test/test_native_autoshift_main.cpp) compiles this header directly
// with -DDRIVER_P4_3CAN. All vehicle-facing wiring (CAN drains, NVS,
// DIAG, TX queue) lives in later tasks and stays outside this core.
//
// Protocol facts (2026-11-04 dictionary + sess_64 verification, see
// docs/superpowers/specs/2026-11-04-autoshift-recon-and-v1-design.md):
//   SBW_RQ_SCCM = CanC 0x228, DLC4, ~98 Hz.
//   b0/b1 = stalk state pair (static P idle observed as 40 C0),
//   b2 high nibble = MsgTxmtId (16-count), b3 = CRC8 SAE J1850
//   (poly 0x1D, init 0xFF, xorout 0xFF) over b0..b2 — 16/16 vectors.
//   The live SCCM stream keeps its own counter; a composed frame always
//   takes source counter + 1, never a locally rolling count.

#include <cstddef>
#include <cstdint>

#if defined(DRIVER_P4_3CAN)

// Stalk-table index space. NOTE: deliberately NOT the DI_gear encoding
// (DI: 1=P, 2=R, 3=N, 4=D, 0=INVALID, 7=SNA) — use dashAsGearFromDi()
// to cross between the two; close-loop confirmation works in DI codes.
enum DashAsGear : uint8_t
{
    kAsGearP = 0,   // stalk in P position
    kAsGearR,       // stalk in R position
    kAsGearN,       // stalk in N position
    kAsGearD,       // stalk in D position
    kAsGearPBtn,    // stalk-end P button
    kAsGearCount,
};

// 0xFF = "no mapping" (DI INVALID/SNA and any unknown code).
inline uint8_t dashAsGearFromDi(uint8_t diGear)
{
    switch (diGear) {
    case 1: return kAsGearP;
    case 2: return kAsGearR;
    case 3: return kAsGearN;
    case 4: return kAsGearD;
    default: return 0xFF;
    }
}

struct DashAutoshiftProfile
{
    uint8_t stalkB0[kAsGearCount];  // per-gear stalk-state byte 0
    uint8_t stalkB1[kAsGearCount];  // per-gear stalk-state byte 1
    uint32_t sbwSrcFreshMs;         // max age of a 0x228 source frame
    uint8_t speedLimitKph;          // shift only at or below this speed
    uint32_t cooldownMs;            // between accepted requests
    uint32_t confirmTimeoutMs;      // DI_gear confirm deadline
    uint8_t retryMax;               // in-flight retries before abandon
    uint8_t shiftCountMax;          // per-session shift request limit
};

// PROFILE UNFROZEN — stalk pairs (0xFF sentinels) and the budgets marked
// [sentinel] below await the Task-2 field freeze; swapping them in is a
// constants-only change. Defaults adopted from the Albert reference are
// NOT sentinels (speed limit, cooldown, retries, count).
static const DashAutoshiftProfile kAsProfile = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // stalkB0  [sentinel: Task 2]
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // stalkB1  [sentinel: Task 2]
    100,    // sbwSrcFreshMs   [sentinel: Task 2 = max 0x228 gap x 4]
    8,      // speedLimitKph   (Albert default)
    1000,   // cooldownMs      (Albert default)
    1000,   // confirmTimeoutMs [sentinel: Task 2 DI_gear observed latency]
    2,      // retryMax        (default, per plan Task 4)
    20,     // shiftCountMax   (policy default, per-session)
};

// CRC8 SAE J1850: poly 0x1D, init 0xFF, xorout 0xFF, MSB-first.
inline uint8_t dashAsCrc8J1850(const uint8_t *d, size_t n)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < n; ++i) {
        crc ^= d[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80u)
                      ? static_cast<uint8_t>((crc << 1) ^ 0x1Du)
                      : static_cast<uint8_t>(crc << 1);
        }
    }
    return static_cast<uint8_t>(crc ^ 0xFFu);
}

// Compose a gear-request 0x228 frame from the freshest source frame.
//
// Writes the frozen stalk-state pair for `gear` (DashAsGear index),
// advances the MsgTxmtId nibble from the SOURCE frame (+1 mod 16, the
// live SCCM stream owns the sequence), preserves the b2 low nibble,
// and recomputes the J1850 CRC over b0..b2.
//
// Returns false (out untouched) when `gear` is out of range or the
// source frame is older than the freshness budget — never inject a
// modified copy of a stale stream.
inline bool dashAsSbwCompose(const uint8_t src[4], uint32_t srcAgeMs,
                             uint8_t gear, uint8_t out[4])
{
    if (gear >= kAsGearCount) {
        return false;
    }
    if (srcAgeMs > kAsProfile.sbwSrcFreshMs) {
        return false;
    }
    out[0] = kAsProfile.stalkB0[gear];
    out[1] = kAsProfile.stalkB1[gear];
    const uint8_t counter =
        static_cast<uint8_t>(((src[2] >> 4) + 1u) & 0x0Fu);
    out[2] = static_cast<uint8_t>((counter << 4) | (src[2] & 0x0F));
    out[3] = dashAsCrc8J1850(out, 3);
    return true;
}

// ---------------------------------------------------------------------------
// Gate chain (fail-closed, single in-flight, exact-code confirm)
// ---------------------------------------------------------------------------

enum DashAsVerdict : uint8_t
{
    AS_SUBMIT = 0,
    AS_BLOCK_MASTER,
    AS_BLOCK_STALE_118,
    AS_BLOCK_SPEED,
    AS_BLOCK_BRAKE,
    AS_BLOCK_OBSTACLE,
    AS_BLOCK_COOLDOWN,
    AS_BLOCK_COUNT,
    AS_BLOCK_PENDING,
    AS_ALREADY_TARGET,
};

struct DashAsInputs
{
    bool master;                // NVS master switch
    bool torque2Fresh;          // 0x118 DI_torque2 inside freshness budget
    uint8_t speedKph;           // DI_vehicleSpeed
    bool brakeApplied;          // DI_brakePedal
    uint8_t gearNow;            // DI_gear code (1=P 2=R 3=N 4=D)
    uint8_t gearTarget;         // DashAsGear index of the request
    bool obstacleFreshAndClear; // fresh 0x23E/0x20E AND no obstacle
    uint32_t lastShiftMs;       // caller-maintained; 0 = none this session
    uint32_t nowCount;          // caller-maintained session shift count
};

enum DashAsConfirm : uint8_t
{
    AS_CONFIRM_IDLE = 0,      // no pending request
    AS_CONFIRM_WAIT,          // pending, not confirmed, inside deadline
    AS_CONFIRM_COMPLETE,      // DI_gear reached the exact target code
    AS_CONFIRM_RETRY,         // deadline expired, retry budget remains
    AS_CONFIRM_ABANDONED,     // deadline expired, retry budget exhausted
};

enum DashAsAbandonReason : uint8_t
{
    kAsAbandonNone = 0,
    kAsAbandonRetryExhausted = 1,
};

struct DashAsGateState
{
    uint8_t pendingTargetDi;    // 0 = none; else DI code 1..4
    uint32_t pendingDeadlineMs;
    uint8_t retries;            // expiries so far this request cycle
    uint8_t lastAbandonReason;  // DashAsAbandonReason, 0 = none
};

inline DashAsGateState dashAsGateState = {};

inline void dashAsGateReset()
{
    dashAsGateState = DashAsGateState{};
}

inline uint8_t dashAsLastAbandonReason()
{
    return dashAsGateState.lastAbandonReason;
}

// Evaluation order is contract-pinned (master -> stale -> speed -> brake
// -> cooldown -> count -> obstacle -> pending -> target -> submit); every
// freshness input fails CLOSED.
inline DashAsVerdict dashAsGateEval(uint32_t nowMs, const DashAsInputs &in)
{
    if (!in.master) {
        return AS_BLOCK_MASTER;
    }
    if (!in.torque2Fresh) {
        return AS_BLOCK_STALE_118;
    }
    if (in.speedKph > kAsProfile.speedLimitKph) {
        return AS_BLOCK_SPEED;
    }
    if (!in.brakeApplied) {
        return AS_BLOCK_BRAKE;
    }
    if (in.lastShiftMs != 0
        && (nowMs - in.lastShiftMs) < kAsProfile.cooldownMs) {
        return AS_BLOCK_COOLDOWN;
    }
    if (in.nowCount >= kAsProfile.shiftCountMax) {
        return AS_BLOCK_COUNT;
    }
    if (!in.obstacleFreshAndClear) {
        return AS_BLOCK_OBSTACLE;
    }
    if (dashAsGateState.pendingTargetDi != 0) {
        return AS_BLOCK_PENDING;
    }
    if (dashAsGearFromDi(in.gearNow) == in.gearTarget) {
        return AS_ALREADY_TARGET;
    }
    return AS_SUBMIT;
}

// Record a submission (caller invokes ONLY on AS_SUBMIT). Single
// in-flight: refuses while a request is pending. targetDi is a DI code.
inline bool dashAsSubmit(uint8_t targetDi, uint32_t nowMs)
{
    if (targetDi < 1 || targetDi > 4) {
        return false;
    }
    if (dashAsGateState.pendingTargetDi != 0) {
        return false;
    }
    dashAsGateState.pendingTargetDi = targetDi;
    dashAsGateState.pendingDeadlineMs = nowMs + kAsProfile.confirmTimeoutMs;
    return true;
}

// Close the loop from each fresh 0x118. COMPLETE only on the EXACT DI
// target code and only for codes in {1,2,4} (0 INVALID / 7 SNA never
// confirm; N is not a confirmable v1 target). Deadline expiry past the
// retry cap abandons the request with a recorded reason.
inline DashAsConfirm dashAsConfirmGear(uint8_t gearNow, uint32_t nowMs)
{
    DashAsGateState &s = dashAsGateState;
    if (s.pendingTargetDi == 0) {
        return AS_CONFIRM_IDLE;
    }
    const bool exact = gearNow == s.pendingTargetDi
                       && (gearNow == 1 || gearNow == 2 || gearNow == 4);
    if (exact) {
        s.pendingTargetDi = 0;
        s.pendingDeadlineMs = 0;
        s.retries = 0;
        s.lastAbandonReason = kAsAbandonNone;
        return AS_CONFIRM_COMPLETE;
    }
    if (nowMs > s.pendingDeadlineMs) {
        ++s.retries;
        const bool exhausted = s.retries >= kAsProfile.retryMax;
        if (exhausted) {
            s.lastAbandonReason = kAsAbandonRetryExhausted;
            s.retries = 0;
        }
        s.pendingTargetDi = 0;
        s.pendingDeadlineMs = 0;
        return exhausted ? AS_CONFIRM_ABANDONED : AS_CONFIRM_RETRY;
    }
    return AS_CONFIRM_WAIT;
}

#endif  // defined(DRIVER_P4_3CAN)
