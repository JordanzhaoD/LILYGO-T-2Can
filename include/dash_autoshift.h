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
    // SBW compose (Task 3)
    uint8_t stalkB0[kAsGearCount];  // per-gear stalk-state byte 0
    uint8_t stalkB1[kAsGearCount];  // per-gear stalk-state byte 1
    uint32_t sbwSrcFreshMs;         // max age of a 0x228 source frame
    // Gate chain (Task 4)
    uint8_t speedLimitKph;          // shift only at or below this speed
    uint32_t cooldownMs;            // between accepted requests
    uint32_t confirmTimeoutMs;      // DI_gear confirm deadline
    uint8_t retryMax;               // in-flight retries before abandon
    uint8_t shiftCountMax;          // per-session shift request limit
    // 0x118 DI_torque2 field map (Task 5)
    uint32_t t2FreshMs;             // 0x118 freshness budget
    uint8_t t2BrakeByte;            // 0xFF = unknown -> brake undecodable
    uint8_t t2BrakeMask;            // applied when (d[b] & mask) != 0
    uint8_t t2SpeedByte;            // 0xFF = unknown -> speed undecodable
    uint8_t t2SpeedScale;           // kph per LSB; 0 = unknown
    // 0x00E STW_ANGLHP_STAT map (Task 5): 16-bit big-endian at bytes 0-1
    int16_t angleScaleT10;          // deci-deg per LSB; 0 = unknown
    int16_t angleOffsetT10;         // deci-deg offset
    uint32_t obstacleFreshMs;       // 0x23E/0x20E freshness budget
    uint8_t oocBlockByte;           // 0x23E oocStatus; 0xFF = unknown
    uint8_t oocBlockMask;           // blocked when (d[b] & mask) != 0
    uint8_t sdiBlockByte;           // 0x20E sdiFront; 0xFF = unknown
    uint8_t sdiBlockMask;
    // 0x045 STW_ACTN_RQ wheel map (Task 5); 0xFF = unknown
    uint8_t w45SideByte;            // byte carrying wheel identity
    uint8_t w45SideL;               // value meaning left wheel
    uint8_t w45SideR;               // value meaning right wheel
    uint8_t w45DirByte;             // byte carrying scroll direction
    uint8_t w45DirPos;              // value meaning positive scroll
    uint8_t w45DirNeg;              // value meaning negative scroll
    // Trigger thresholds (policy defaults, Albert reference)
    uint8_t steerParkDeg;           // P/N steering threshold
    uint8_t steerDriveReverseDeg;   // D/R toggle threshold
    uint8_t steerRearmDeg;          // hysteresis re-arm margin
    uint32_t scrollEpisodeGapMs;    // gap closing a scroll episode
};

// PROFILE UNFROZEN — everything marked [sentinel] awaits the Task-2 field
// freeze; swapping values in is a constants-only change. Sentinel maps
// (0xFF / 0) DISABLE the corresponding decode, which fails CLOSED through
// the gates: undecoded brake/speed never passes, absent obstacle telemetry
// never reads clear. Policy defaults (Albert reference) are not sentinels.
static const DashAutoshiftProfile kAsProfile = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // stalkB0  [sentinel: Task 2]
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF},  // stalkB1  [sentinel: Task 2]
    100,    // sbwSrcFreshMs   [sentinel: Task 2 = max 0x228 gap x 4]
    8,      // speedLimitKph   (Albert default)
    1000,   // cooldownMs      (Albert default)
    1000,   // confirmTimeoutMs [sentinel: Task 2 DI_gear observed latency]
    2,      // retryMax        (default, per plan Task 4)
    20,     // shiftCountMax   (policy default, per-session)
    300,    // t2FreshMs       (0x118 @100Hz; policy default)
    0xFF,   // t2BrakeByte     [sentinel: Task 2]
    0x00,   // t2BrakeMask     [sentinel: Task 2]
    0xFF,   // t2SpeedByte     [sentinel: Task 2]
    0,      // t2SpeedScale    [sentinel: Task 2]
    0,      // angleScaleT10   [sentinel: Task 2]
    0,      // angleOffsetT10  [sentinel: Task 2]
    500,    // obstacleFreshMs (suite ~8-12Hz; policy default)
    0xFF,   // oocBlockByte    [sentinel: Task 2]
    0x00,   // oocBlockMask    [sentinel: Task 2]
    0xFF,   // sdiBlockByte    [sentinel: Task 2]
    0x00,   // sdiBlockMask    [sentinel: Task 2]
    0xFF,   // w45SideByte     [sentinel: Task 2]
    0xFF,   // w45SideL        [sentinel: Task 2]
    0xFF,   // w45SideR        [sentinel: Task 2]
    0xFF,   // w45DirByte      [sentinel: Task 2]
    0xFF,   // w45DirPos       [sentinel: Task 2]
    0xFF,   // w45DirNeg       [sentinel: Task 2]
    30,     // steerParkDeg    (Albert default)
    60,     // steerDriveReverseDeg (Albert default)
    10,     // steerRearmDeg   (policy default)
    150,    // scrollEpisodeGapMs (policy default)
};

// Runtime copy: decoders/triggers/gates read this instance so tests can
// simulate the Task-2 freeze by mutating it; kAsProfile stays the frozen
// initializer.
inline DashAutoshiftProfile dashAsProfile = kAsProfile;

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
    if (srcAgeMs > dashAsProfile.sbwSrcFreshMs) {
        return false;
    }
    out[0] = dashAsProfile.stalkB0[gear];
    out[1] = dashAsProfile.stalkB1[gear];
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

// Session runtime + stats (Task 6): what the DIAG status JSON and the Web
// status read. lastShiftMs/shiftCount are the caller-maintained gate inputs
// (cooldown anchor / session count), updated by the confirm path itself.
struct DashAsStats
{
    uint32_t triggered = 0;
    uint32_t submitted = 0;
    uint32_t blocked = 0;
    uint32_t confirmed = 0;
    uint32_t retried = 0;
    uint32_t abandoned = 0;
    uint32_t txOk = 0;
    uint32_t txFail = 0;
};
inline DashAsStats dashAsStats = {};

struct DashAsRuntime
{
    uint32_t lastShiftMs = 0;   // cooldown anchor: last COMPLETE (0 = none)
    uint32_t shiftCount = 0;    // confirmed shifts this session
    DashAsVerdict lastVerdict = AS_SUBMIT;  // latest gate evaluation
    DashAsVerdict lastBlock = AS_SUBMIT;    // latest non-submit verdict
};
inline DashAsRuntime dashAsRuntime = {};

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
    if (in.speedKph > dashAsProfile.speedLimitKph) {
        return AS_BLOCK_SPEED;
    }
    if (!in.brakeApplied) {
        return AS_BLOCK_BRAKE;
    }
    if (in.lastShiftMs != 0
        && (nowMs - in.lastShiftMs) < dashAsProfile.cooldownMs) {
        return AS_BLOCK_COOLDOWN;
    }
    if (in.nowCount >= dashAsProfile.shiftCountMax) {
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
    dashAsGateState.pendingDeadlineMs = nowMs + dashAsProfile.confirmTimeoutMs;
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
        dashAsRuntime.lastShiftMs = nowMs ? nowMs : 1;
        ++dashAsRuntime.shiftCount;
        ++dashAsStats.confirmed;
        return AS_CONFIRM_COMPLETE;
    }
    if (nowMs > s.pendingDeadlineMs) {
        ++s.retries;
        const bool exhausted = s.retries >= dashAsProfile.retryMax;
        if (exhausted) {
            s.lastAbandonReason = kAsAbandonRetryExhausted;
            s.retries = 0;
            ++dashAsStats.abandoned;
        } else {
            ++dashAsStats.retried;
        }
        s.pendingTargetDi = 0;
        s.pendingDeadlineMs = 0;
        return exhausted ? AS_CONFIRM_ABANDONED : AS_CONFIRM_RETRY;
    }
    return AS_CONFIRM_WAIT;
}

// ---------------------------------------------------------------------------
// Decoders, triggers, close-loop wiring (Task 5)
// ---------------------------------------------------------------------------

enum DashAsTriggerType : uint8_t
{
    kAsTrigNone = 0,
    kAsTrigScroll,
    kAsTrigSteer,
};

struct DashAsTrigger
{
    uint8_t type = kAsTrigNone;
    uint8_t side = 0;   // scroll: 0 = left wheel, 1 = right wheel
    int8_t dir = 0;     // scroll: +1/-1; steer: sign of the angle
    uint8_t gear = 0;   // DashAsGear target for the request
};

struct DashAsSensor
{
    uint32_t lastTorque2Ms = 0;    // 0 = never seen
    uint8_t gearNow = 0;           // DI code from 0x118 b3>>4 (proven)
    uint8_t brake = 0xFF;          // 0/1; 0xFF = undecoded (fail-closed)
    uint8_t speedKph = 0xFF;       // decoded kph; 0xFF = undecoded
    uint32_t lastAngleMs = 0;
    int16_t angleDegT10 = 0x7FFF;  // deci-degrees; 0x7FFF = invalid
    uint32_t lastOocMs = 0;        // 0x23E oocStatus
    uint32_t lastSdiMs = 0;        // 0x20E sdiFront
    bool oocBlocked = true;        // conservative until a decoded clear
    bool sdiBlocked = true;
    uint32_t lastWheelMs = 0;      // 0x045 episode tracking
    DashAsTrigger pending = {};    // one-shot trigger slot (scroll)
    DashAsConfirm lastConfirm = AS_CONFIRM_IDLE;
    bool steerArmed = true;        // steering one-shot latch
};

inline DashAsSensor dashAsSensor = {};

inline bool dashAsTorque2Fresh(uint32_t nowMs)
{
    return dashAsSensor.lastTorque2Ms != 0
        && (nowMs - dashAsSensor.lastTorque2Ms) <= dashAsProfile.t2FreshMs;
}

// DI_torque2 (CanA 0x118, DLC8, ~100 Hz). Gear position is field-proven
// b3>>4; brake/speed ride the frozen map (sentinel = undecoded, and the
// gates read undecoded as blocked). The close loop runs on every fresh
// 0x118: pending requests confirm or age out here.
inline void dashAsOnTorque2(const uint8_t d[8], uint32_t nowMs)
{
    DashAsSensor &s = dashAsSensor;
    s.lastTorque2Ms = nowMs ? nowMs : 1;
    s.gearNow = static_cast<uint8_t>((d[3] >> 4) & 0x0F);
    if (dashAsProfile.t2BrakeByte < 8 && dashAsProfile.t2BrakeMask != 0) {
        s.brake = (d[dashAsProfile.t2BrakeByte]
                   & dashAsProfile.t2BrakeMask) != 0 ? 1 : 0;
    }
    if (dashAsProfile.t2SpeedByte < 8 && dashAsProfile.t2SpeedScale != 0) {
        const uint32_t kph = static_cast<uint32_t>(
            d[dashAsProfile.t2SpeedByte]) * dashAsProfile.t2SpeedScale;
        s.speedKph = kph < 255 ? static_cast<uint8_t>(kph) : 255;
    }
    if (dashAsGateState.pendingTargetDi != 0) {
        s.lastConfirm = dashAsConfirmGear(s.gearNow, nowMs);
    }
}

// STW_ANGLHP_STAT (CanC 0x00E, ~100 Hz). Raw = 16-bit big-endian at
// bytes 0-1; scale/offset frozen. Map unfrozen (scale 0) -> angle stays
// invalid and steering triggers never fire.
inline void dashAsOnAngle00E(const uint8_t d[8], uint32_t nowMs)
{
    if (dashAsProfile.angleScaleT10 == 0) {
        return;
    }
    const int16_t raw = static_cast<int16_t>(
        (static_cast<uint16_t>(d[0]) << 8) | d[1]);
    int32_t a = static_cast<int32_t>(raw) * dashAsProfile.angleScaleT10
              + dashAsProfile.angleOffsetT10;
    if (a > 32767) {
        a = 32767;
    } else if (a < -32767) {
        a = -32767;
    }
    dashAsSensor.angleDegT10 = static_cast<int16_t>(a);
    dashAsSensor.lastAngleMs = nowMs ? nowMs : 1;
}

// Obstacle suite (busmask-5 posture: blocker-only, Jordan 2026-11-05):
// 0x23E oocStatus + 0x20E sdiFront on CanC. CanB-only frames (0x22E
// sdiRear) are ignored by design — rear coverage is accepted weaker.
inline void dashAsOnObstacle(uint32_t id, const uint8_t d[8], uint32_t nowMs)
{
    DashAsSensor &s = dashAsSensor;
    if (id == 0x23E && dashAsProfile.oocBlockByte < 8
        && dashAsProfile.oocBlockMask != 0) {
        s.oocBlocked = (d[dashAsProfile.oocBlockByte]
                        & dashAsProfile.oocBlockMask) != 0;
        s.lastOocMs = nowMs ? nowMs : 1;
    } else if (id == 0x20E && dashAsProfile.sdiBlockByte < 8
               && dashAsProfile.sdiBlockMask != 0) {
        s.sdiBlocked = (d[dashAsProfile.sdiBlockByte]
                        & dashAsProfile.sdiBlockMask) != 0;
        s.lastSdiMs = nowMs ? nowMs : 1;
    }
}

// Fresh AND clear, fail-closed: absent/stale/unfrozen telemetry is NOT
// clear (gate blocks), a blocked bit on either frame blocks.
inline bool dashAsObstacleFreshAndClear(uint32_t nowMs)
{
    const DashAsSensor &s = dashAsSensor;
    if (s.lastOocMs == 0 || s.lastSdiMs == 0) {
        return false;
    }
    if (nowMs - s.lastOocMs > dashAsProfile.obstacleFreshMs) {
        return false;
    }
    if (nowMs - s.lastSdiMs > dashAsProfile.obstacleFreshMs) {
        return false;
    }
    return !s.oocBlocked && !s.sdiBlocked;
}

// STW_ACTN_RQ (CanA 0x045): scroll gesture episodes. A physical roll
// emits a burst of motion frames; the FIRST frame of an episode (after
// the episode gap) arms ONE pending trigger — later frames of the same
// burst never re-fire, and a pending slot is never overwritten.
// Default mappings (Albert reference): left+ = D, left- = R,
// right+ = P button, right- = D/R toggle.
inline void dashAsOnWheel045(const uint8_t d[8], uint32_t nowMs)
{
    const DashAutoshiftProfile &p = dashAsProfile;
    if (p.w45SideByte >= 8 || p.w45DirByte >= 8) {
        return;  // map unfrozen
    }
    if (p.w45SideL == p.w45SideR || p.w45DirPos == p.w45DirNeg) {
        return;  // degenerate map (sentinel equalities)
    }
    uint8_t side;
    const uint8_t sideVal = d[p.w45SideByte];
    if (sideVal == p.w45SideL) {
        side = 0;
    } else if (sideVal == p.w45SideR) {
        side = 1;
    } else {
        return;  // not a motion frame we decode
    }
    int8_t dir;
    const uint8_t dirVal = d[p.w45DirByte];
    if (dirVal == p.w45DirPos) {
        dir = 1;
    } else if (dirVal == p.w45DirNeg) {
        dir = -1;
    } else {
        return;
    }
    DashAsSensor &s = dashAsSensor;
    const bool episodeStart = s.lastWheelMs == 0
        || (nowMs - s.lastWheelMs) > p.scrollEpisodeGapMs;
    s.lastWheelMs = nowMs ? nowMs : 1;
    if (!episodeStart || s.pending.type != kAsTrigNone) {
        return;
    }
    uint8_t gear;
    if (side == 0) {
        gear = (dir > 0) ? kAsGearD : kAsGearR;
    } else if (dir > 0) {
        gear = kAsGearPBtn;
    } else {
        gear = (s.gearNow == 4) ? kAsGearR : kAsGearD;
    }
    s.pending.type = kAsTrigScroll;
    s.pending.side = side;
    s.pending.dir = dir;
    s.pending.gear = gear;
}

// Consume the pending trigger. Scroll wins over steering (a hand on the
// wheel is the deliberate gesture). Steering is edge-triggered with a
// hysteresis re-arm: fires once when |angle| crosses the gear-dependent
// threshold, re-arms only after returning inside threshold minus margin.
inline DashAsTrigger dashAsPollTrigger(uint32_t nowMs)
{
    (void)nowMs;
    DashAsSensor &s = dashAsSensor;
    if (s.pending.type == kAsTrigScroll) {
        const DashAsTrigger t = s.pending;
        s.pending = DashAsTrigger{};
        return t;
    }
    if (s.angleDegT10 == 0x7FFF || s.lastAngleMs == 0) {
        return DashAsTrigger{};
    }
    int32_t thresholdT10 = 0;
    if (s.gearNow == 1 || s.gearNow == 3) {
        thresholdT10 = static_cast<int32_t>(dashAsProfile.steerParkDeg) * 10;
    } else if (s.gearNow == 2 || s.gearNow == 4) {
        thresholdT10 =
            static_cast<int32_t>(dashAsProfile.steerDriveReverseDeg) * 10;
    }
    if (thresholdT10 == 0) {
        return DashAsTrigger{};
    }
    const int32_t mag = s.angleDegT10 < 0 ? -s.angleDegT10 : s.angleDegT10;
    const int32_t rearmT10 =
        thresholdT10 - static_cast<int32_t>(dashAsProfile.steerRearmDeg) * 10;
    if (!s.steerArmed) {
        if (mag <= rearmT10) {
            s.steerArmed = true;
        }
        return DashAsTrigger{};
    }
    if (mag < thresholdT10) {
        return DashAsTrigger{};
    }
    s.steerArmed = false;
    DashAsTrigger t = {};
    t.type = kAsTrigSteer;
    t.dir = s.angleDegT10 >= 0 ? 1 : -1;
    if (s.gearNow == 1 || s.gearNow == 3) {
        t.gear = (t.dir > 0) ? kAsGearD : kAsGearR;  // infer from sign
    } else {
        t.gear = (s.gearNow == 4) ? kAsGearR : kAsGearD;  // D/R toggle
    }
    return t;
}

// ---------------------------------------------------------------------------
// Permit gate, 0x228 source snapshot (Task 6)
// ---------------------------------------------------------------------------

// Two-step arm house pattern: the first arm request only OPENS a confirm
// window; a second request inside the window enables. No single command
// ever enables TX, and a same-tick double request cannot confirm (the
// elapsed check demands >= 1 ms between the two).
static const uint32_t kAsArmWindowMs = 5000;
// Own-echo suppression: after we transmit a composed 0x228, an echoed copy
// arriving within this window is ours and must not refresh the source
// snapshot (which would desynchronize the MsgTxmtId counter).
static const uint32_t kAsOwnEchoWindowMs = 30;

enum DashAsArmResult : uint8_t
{
    kAsArmDeniedMaster = 0,  // master off — arm refused outright
    kAsArmOpened,            // confirm window opened, NOT armed yet
    kAsArmEnabled,           // second request inside the window — armed
    kAsArmAlreadyArmed,
    kAsArmExpired,           // window lapsed; pending request cleared
};

struct DashAsPermit
{
    bool master = false;             // NVS "autoshift_master"
    bool armed = false;              // NVS "autoshift_armed" intent
    uint32_t armPendingSinceMs = 0;  // 0 = no confirm window open
};

inline DashAsPermit dashAsPermit = {};

inline DashAsArmResult dashAsArmRequest(uint32_t nowMs)
{
    if (!dashAsPermit.master) {
        return kAsArmDeniedMaster;
    }
    if (dashAsPermit.armed) {
        return kAsArmAlreadyArmed;
    }
    if (dashAsPermit.armPendingSinceMs != 0) {
        const uint32_t elapsed = nowMs - dashAsPermit.armPendingSinceMs;
        if (elapsed >= 1 && elapsed <= kAsArmWindowMs) {
            dashAsPermit.armed = true;
            dashAsPermit.armPendingSinceMs = 0;
            return kAsArmEnabled;
        }
        if (elapsed > kAsArmWindowMs) {
            dashAsPermit.armPendingSinceMs = 0;
            return kAsArmExpired;
        }
        // elapsed == 0: same tick as the open — keep the window, re-open
    }
    dashAsPermit.armPendingSinceMs = nowMs ? nowMs : 1;
    return kAsArmOpened;
}

inline void dashAsDisarm()
{
    dashAsPermit.armed = false;
    dashAsPermit.armPendingSinceMs = 0;
    dashAsGateReset();  // no in-flight request survives a disarm
}

inline void dashAsSetMaster(bool on)
{
    dashAsPermit.master = on;
    if (!on) {
        dashAsDisarm();  // cascade: master-off always disarms
    }
}

// Freshest observed SBW_RQ_SCCM (CanC 0x228) — the compose source. The
// snapshot skips our own armed-mode TX echoes (kAsOwnEchoWindowMs).
struct DashAsSbwSource
{
    uint8_t data[4] = {0, 0, 0, 0};
    uint32_t ms = 0;  // 0 = never seen
    bool seen = false;
};
inline DashAsSbwSource dashAsSbwSource = {};
inline uint32_t dashAsLastSbwTxMs = 0;  // 0 = we never transmitted

inline void dashAsOnSbw0228(const uint8_t d[4], uint32_t nowMs)
{
    if (dashAsLastSbwTxMs != 0
        && (nowMs - dashAsLastSbwTxMs) <= kAsOwnEchoWindowMs) {
        return;  // our own echo — not a source frame
    }
    DashAsSbwSource &src = dashAsSbwSource;
    src.data[0] = d[0];
    src.data[1] = d[1];
    src.data[2] = d[2];
    src.data[3] = d[3];
    src.ms = nowMs ? nowMs : 1;
    src.seen = true;
}

// DI codes for the submit/confirm path (close loop works in DI codes).
// The stalk-end P button requests P (DI 1); N is not a confirmable v1
// target but keeps its code for completeness. 0xFF = invalid index.
inline uint8_t dashAsGearToDi(uint8_t gear)
{
    switch (gear) {
    case kAsGearP:
    case kAsGearPBtn: return 1;
    case kAsGearR: return 2;
    case kAsGearN: return 3;
    case kAsGearD: return 4;
    default: return 0xFF;
    }
}

#endif  // defined(DRIVER_P4_3CAN)
