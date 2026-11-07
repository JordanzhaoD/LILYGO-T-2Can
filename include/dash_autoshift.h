#pragma once

// Auto gear shift (Legacy Intel Model X) — engine core, v2.
//
// Platform-free C++: no Arduino/ESP includes, so the native harness
// (test/test_native_autoshift_main.cpp) compiles this header directly
// with -DDRIVER_P4_3CAN. All vehicle-facing wiring (CAN drains, NVS,
// DIAG, TX queue) lives outside this core.
//
// Protocol facts (2026-11-04 dictionary + sess_64, upgraded 2026-11-07 by
// the ka750 field round — see
// docs/superpowers/specs/2026-11-07-autoshift-v2-evidence-upgrade-design.md):
//   SBW_RQ_SCCM = CanC 0x228, DLC4, ~100 Hz idle (b1=0xC0) stream.
//   b0/b1 = stalk state pair (idle 40 C0), b2 high nibble = MsgTxmtId
//   (16-count) walking +1 per frame ACROSS idle and command frames,
//   b3 = CRC8 SAE J1850 (poly 0x1D, init 0xFF, xorout 0xFF) over b0..b2
//   — 16/16 static vectors. Shift commands are multi-frame PHASES at a
//   10 ms cadence inside the gateway-owned stream (ka750 sess_s4_gears):
//   P->N = C4, N->D = C8, D->N = C2, N->R = C2 then C1 (two-phase),
//   ->P = D0; a continuous P->D sweep is C4 x4 then C8 x26 with no gap.
//   Gear feedback = CanA 0x118 b3>>4 (1P/2R/3N/4D), flipping 0-30 ms
//   after burst start (worst observed 520 ms).
//   Speed = CanC 0x106 b5:b6 big-endian, 4 mm/s per tick (3-point
//   calibrated). Brake = CanC 0x116 b1 bit7. Wheel gestures = CanB
//   0x514 (left) / 0x51B (right) event pages, b0 = 0x10 press / 0x11
//   release, arriving as pairs.
//
// v2 posture (Jordan 2026-11-07): trigger = left-wheel press pair,
// P<->D only — both TX sequences field-proven end to end. Angle scale
// and obstacle maps stay sentinel (their evidence is still pending),
// which fails those gates CLOSED. A composed frame always rides the
// live SCCM counter (+1), never a locally rolling count.

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

// DI codes for the submit/confirm path. The stalk-end P button requests
// P (DI 1); N keeps its code for completeness. 0xFF = invalid index.
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

struct DashAutoshiftProfile
{
    // SBW sequence (FROZEN 2026-11-07 from ka750 sess_s4_gears).
    uint8_t pdLeadCode;         // P->D phase 1 code (exit P)
    uint8_t pdLeadFrames;       // P->D phase 1 frame count
    uint8_t pdMainCode;         // P->D phase 2 code (enter D)
    uint8_t pdMainFrames;       // P->D phase 2 frame count
    uint8_t dpCode;             // D->P code
    uint8_t dpFrames;           // D->P frame count
    uint32_t frameCadenceMs;    // native burst cadence (10 ms)
    uint32_t sbwSrcFreshMs;     // max age of a 0x228 source frame
    // Gate chain
    uint8_t speedLimitKph;      // shift only at or below this speed
    uint32_t cooldownMs;        // between accepted requests
    uint32_t confirmTimeoutMs;  // gear-confirm deadline (from burst end)
    uint8_t retryMax;           // in-flight retries before abandon
    uint8_t shiftCountMax;      // per-session shift request limit
    // Sensor freshness budgets (all streams ~100 Hz)
    uint32_t t2FreshMs;         // 0x118 gear/vehicle anchor
    uint32_t speedFreshMs;      // 0x106 speed
    uint32_t brakeFreshMs;      // 0x116 brake
    // 0x00E STW_ANGLHP_STAT map: 16-bit big-endian at bytes 0-1.
    int16_t angleScaleT10;      // deci-deg per LSB; 0 = unknown [sentinel]
    int16_t angleOffsetT10;     // deci-deg offset
    uint32_t obstacleFreshMs;   // 0x23E/0x20E freshness budget
    uint8_t oocBlockByte;       // 0x23E oocStatus; 0xFF = unknown [sentinel]
    uint8_t oocBlockMask;       // blocked when (d[b] & mask) != 0
    uint8_t sdiBlockByte;       // 0x20E sdiFront; 0xFF = unknown [sentinel]
    uint8_t sdiBlockMask;
    // Trigger thresholds (policy defaults, Albert reference)
    uint8_t steerParkDeg;       // P/N steering threshold
    uint8_t steerDriveReverseDeg; // D/R toggle threshold
    uint8_t steerRearmDeg;      // hysteresis re-arm margin
    uint32_t wheelPressTimeoutMs; // release must complete within this
};

// FROZEN 2026-11-07 (ka750 evidence round). Angle scale and obstacle
// maps remain sentinel — those DISABLE the corresponding decode, which
// fails CLOSED through the gates: unfrozen steering never triggers and
// absent obstacle telemetry never reads clear. Policy defaults (Albert
// reference) are not sentinels.
static const DashAutoshiftProfile kAsProfile = {
    0xC4,   // pdLeadCode     (P->N / sweep lead-in, s4 @181.000/@209.789)
    4,      // pdLeadFrames   (one-sweep observation; standalone x8-12)
    0xC8,   // pdMainCode     (enter D, s4 @181.080/@209.829)
    25,     // pdMainFrames   (observed 13/25/26)
    0xD0,   // dpCode         (enter P, stalk + P-button, s4 + recon2)
    12,     // dpFrames       (observed 12/12)
    10,     // frameCadenceMs (native burst cadence, 100 Hz stream)
    100,    // sbwSrcFreshMs  (native 10 ms cadence -> 10 frames)
    8,      // speedLimitKph  (Albert default)
    1000,   // cooldownMs     (Albert default)
    1000,   // confirmTimeoutMs (300 ms burst + 520 ms worst observed lag)
    2,      // retryMax       (default, per plan Task 4)
    20,     // shiftCountMax  (policy default, per-session)
    300,    // t2FreshMs      (0x118 @100Hz; policy default)
    300,    // speedFreshMs   (0x106 @100Hz)
    300,    // brakeFreshMs   (0x116 @100Hz)
    0,      // angleScaleT10  [sentinel: video calibration pending]
    0,      // angleOffsetT10 [sentinel]
    500,    // obstacleFreshMs (suite ~8-12Hz; policy default)
    0xFF,   // oocBlockByte   [sentinel: box-push adjudication pending]
    0x00,   // oocBlockMask   [sentinel]
    0xFF,   // sdiBlockByte   [sentinel]
    0x00,   // sdiBlockMask   [sentinel]
    30,     // steerParkDeg   (Albert default)
    60,     // steerDriveReverseDeg (Albert default)
    10,     // steerRearmDeg  (policy default)
    2000,   // wheelPressTimeoutMs (press+release pairs are sub-second)
};

// Runtime copy: decoders/triggers/gates read this instance so tests can
// simulate later freezes by mutating it; kAsProfile stays the frozen
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

// ---------------------------------------------------------------------------
// Gate chain (fail-closed, single in-flight, exact-code confirm)
// ---------------------------------------------------------------------------

enum DashAsVerdict : uint8_t
{
    AS_SUBMIT = 0,
    AS_BLOCK_MASTER,
    AS_BLOCK_STALE_118,
    AS_BLOCK_STALE_SPEED,   // v2: 0x106 absent/stale/undecoded
    AS_BLOCK_SPEED,
    AS_BLOCK_STALE_BRAKE,   // v2: 0x116 absent/stale/undecoded
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
    bool torque2Fresh;          // 0x118 inside freshness budget
    bool speedFresh;            // v2: 0x106 inside freshness budget
    uint8_t speedKph;           // v2: decoded from 0x106 (4 mm/s ticks)
    bool brakeFresh;            // v2: 0x116 inside freshness budget
    bool brakeApplied;          // v2: 0x116 b1 bit7
    uint8_t gearNow;            // DI_gear code (1=P 2=R 3=N 4=D)
    uint8_t gearTarget;         // DashAsGear index of the request
    bool obstacleFreshAndClear; // fresh 0x23E/0x20E AND no obstacle
    uint32_t lastShiftMs;       // caller-maintained; 0 = none this session
    uint32_t nowCount;          // caller-maintained session shift count
};

enum DashAsConfirm : uint8_t
{
    AS_CONFIRM_IDLE = 0,      // no pending request
    AS_CONFIRM_WAIT,          // pending, not confirmed
    AS_CONFIRM_COMPLETE,      // DI_gear reached the exact target code
};

enum DashAsAbandonReason : uint8_t
{
    kAsAbandonNone = 0,
    kAsAbandonRetryExhausted = 1,
    kAsAbandonStaleSource = 2,  // v2: no fresh 0x228 to ride at retry
};

struct DashAsGateState
{
    uint8_t pendingTargetDi;    // 0 = none; else DI code 1..4
    uint32_t pendingDeadlineMs;
    uint8_t retries;            // expiries so far this request cycle
    uint8_t lastAbandonReason;  // DashAsAbandonReason, 0 = none
};

inline DashAsGateState dashAsGateState = {};

// Session runtime + stats: what the DIAG status JSON and the Web status
// read. lastShiftMs/shiftCount are the caller-maintained gate inputs
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

// Evaluation order is contract-pinned (master -> stale 0x118 -> stale
// speed -> speed -> stale brake -> brake -> cooldown -> count ->
// obstacle -> pending -> target -> submit); every freshness input fails
// CLOSED — an undecoded sensor is never fresh, an absent stream is
// never clear.
inline DashAsVerdict dashAsGateEval(uint32_t nowMs, const DashAsInputs &in)
{
    if (!in.master) {
        return AS_BLOCK_MASTER;
    }
    if (!in.torque2Fresh) {
        return AS_BLOCK_STALE_118;
    }
    if (!in.speedFresh) {
        return AS_BLOCK_STALE_SPEED;
    }
    if (in.speedKph > dashAsProfile.speedLimitKph) {
        return AS_BLOCK_SPEED;
    }
    if (!in.brakeFresh) {
        return AS_BLOCK_STALE_BRAKE;
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

// ---------------------------------------------------------------------------
// Engine state (the single owner of an in-flight shift request)
// ---------------------------------------------------------------------------

enum DashAsEngineSt : uint8_t
{
    kAsEngIdle = 0,
    kAsEngBurst,
    kAsEngConfirm,
};

struct DashAsEngine
{
    uint8_t st = kAsEngIdle;
    uint8_t targetGear = 0xFF;  // DashAsGear index
    uint16_t seqIdx = 0;        // frames emitted this burst
    uint16_t framesTotal = 0;   // burst length from the frozen profile
    uint8_t lastCounter = 0;    // MsgTxmtId nibble of the last TX frame
    uint32_t lastFrameMs = 0;   // cadence pacer
    uint8_t retries = 0;        // confirm expiries this request
    uint32_t deadlineMs = 0;    // confirm deadline (absolute)
};
inline DashAsEngine dashAsEngine = {};

inline uint8_t dashAsEngineState()
{
    return dashAsEngine.st;
}

inline void dashAsEngineReset()
{
    dashAsEngine = DashAsEngine{};
}

// Close the loop from each fresh 0x118. COMPLETE only on the EXACT DI
// target code and only for codes in {1,2,4} (0 INVALID / 7 SNA never
// confirm). Deadline/retry/abandon are owned by the engine tick, not
// here — one owner per lifecycle transition.
inline DashAsConfirm dashAsConfirmGear(uint8_t gearNow, uint32_t nowMs)
{
    (void)nowMs;
    DashAsGateState &s = dashAsGateState;
    if (s.pendingTargetDi == 0) {
        return AS_CONFIRM_IDLE;
    }
    const bool exact = gearNow == s.pendingTargetDi
                       && (gearNow == 1 || gearNow == 2 || gearNow == 4);
    if (!exact) {
        return AS_CONFIRM_WAIT;
    }
    s.pendingTargetDi = 0;
    s.pendingDeadlineMs = 0;
    s.retries = 0;
    s.lastAbandonReason = kAsAbandonNone;
    dashAsRuntime.lastShiftMs = nowMs ? nowMs : 1;
    ++dashAsRuntime.shiftCount;
    ++dashAsStats.confirmed;
    dashAsEngineReset();  // a mid-burst confirm cancels the burst
    return AS_CONFIRM_COMPLETE;
}

// ---------------------------------------------------------------------------
// Decoders, triggers
// ---------------------------------------------------------------------------

enum DashAsTriggerType : uint8_t
{
    kAsTrigNone = 0,
    kAsTrigScroll,   // v2: left-wheel press+release pair
    kAsTrigSteer,    // frozen-out until the angle scale lands
};

struct DashAsTrigger
{
    uint8_t type = kAsTrigNone;
    uint8_t side = 0;   // scroll: 0 = left wheel, 1 = right wheel
    int8_t dir = 0;     // steer: sign of the angle
    uint8_t gear = 0;   // DashAsGear target for the request
};

struct DashAsSensor
{
    uint32_t lastTorque2Ms = 0;    // 0 = never seen
    uint8_t gearNow = 0;           // DI code from 0x118 b3>>4 (proven)
    uint32_t lastSpeedMs = 0;      // 0x106 (v2)
    uint16_t speedTick = 0xFFFF;   // raw 4 mm/s ticks; 0xFFFF = undecoded
    uint8_t speedKph = 0xFF;       // rounded kph; 0xFF = undecoded
    uint32_t lastMirrorMs = 0;     // 0x116 (v2)
    uint8_t brake = 0xFF;          // 0/1; 0xFF = undecoded (fail-closed)
    uint32_t lastAngleMs = 0;
    int16_t angleDegT10 = 0x7FFF;  // deci-degrees; 0x7FFF = invalid
    uint32_t lastOocMs = 0;        // 0x23E oocStatus
    uint32_t lastSdiMs = 0;        // 0x20E sdiFront
    bool oocBlocked = true;        // conservative until a decoded clear
    bool sdiBlocked = true;
    uint32_t wheelPressMs = 0;     // 0x514 press latch (0 = none) (v2)
    DashAsTrigger pending = {};    // one-shot trigger slot
    DashAsConfirm lastConfirm = AS_CONFIRM_IDLE;
    bool steerArmed = true;        // steering one-shot latch
};

inline DashAsSensor dashAsSensor = {};

inline bool dashAsTorque2Fresh(uint32_t nowMs)
{
    return dashAsSensor.lastTorque2Ms != 0
        && (nowMs - dashAsSensor.lastTorque2Ms) <= dashAsProfile.t2FreshMs;
}

inline bool dashAsSpeedFresh(uint32_t nowMs)
{
    return dashAsSensor.lastSpeedMs != 0
        && (nowMs - dashAsSensor.lastSpeedMs) <= dashAsProfile.speedFreshMs;
}

inline bool dashAsBrakeFresh(uint32_t nowMs)
{
    return dashAsSensor.lastMirrorMs != 0
        && (nowMs - dashAsSensor.lastMirrorMs) <= dashAsProfile.brakeFreshMs;
}

// DI_torque2 (CanA 0x118, DLC8, ~100 Hz). Gear position is field-proven
// b3>>4. v2 removed the 0x118 brake/speed maps (disproven by the ka750
// round): brake/speed live on 0x116/0x106 and stay undecoded here. The
// close loop runs on every fresh 0x118: a pending request confirms on
// the exact target code.
inline void dashAsOnTorque2(const uint8_t d[8], uint32_t nowMs)
{
    DashAsSensor &s = dashAsSensor;
    s.lastTorque2Ms = nowMs ? nowMs : 1;
    s.gearNow = static_cast<uint8_t>((d[3] >> 4) & 0x0F);
    if (dashAsGateState.pendingTargetDi != 0) {
        s.lastConfirm = dashAsConfirmGear(s.gearNow, nowMs);
    }
}

// PT speed (CanC 0x106, ~100 Hz): b5:b6 big-endian, 4 mm/s per tick.
// 3-point calibrated 2026-11-07: 797/1354/2104 tick = 11.5/19.5/30.3
// km/h. kph = tick * 144 / 10000, rounded to nearest.
inline void dashAsOnSpeed106(const uint8_t d[8], uint32_t nowMs)
{
    DashAsSensor &s = dashAsSensor;
    s.speedTick = static_cast<uint16_t>((d[5] << 8) | d[6]);
    const uint32_t kph = (static_cast<uint32_t>(s.speedTick) * 144u + 5000u)
                       / 10000u;
    s.speedKph = kph < 255 ? static_cast<uint8_t>(kph) : 255;
    s.lastSpeedMs = nowMs ? nowMs : 1;
}

// DI mirror (CanC 0x116, ~100 Hz): b1 bit7 = brake applied (ka750 s4:
// one continuous 41.8 s hold across the whole shifting test).
inline void dashAsOnMirror116(const uint8_t d[8], uint32_t nowMs)
{
    DashAsSensor &s = dashAsSensor;
    s.brake = (d[1] & 0x80u) != 0 ? 1 : 0;
    s.lastMirrorMs = nowMs ? nowMs : 1;
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

// Obstacle suite (blocker-only posture, Jordan 2026-11-05): 0x23E
// oocStatus + 0x20E sdiFront on CanC. CanB-only frames (0x22E sdiRear)
// are ignored by design — rear coverage is accepted weaker.
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

// Wheel event pages (CanB 0x514 left / 0x51B right, ka750 s3): b0=0x10
// press / 0x11 release arriving as pairs. v2 consumes the LEFT page
// only; a completed press+release pair arms ONE pending trigger whose
// target maps from the CURRENT 0x118 gear (P->D, D->P; anything else
// drops — v2 never composes an unobserved sequence). A pending slot is
// never overwritten; a release without a fresh press latch is ignored.
inline void dashAsOnWheelEvent(uint16_t id, const uint8_t d[8], uint32_t nowMs)
{
    if (id != 0x514) {
        return;  // right page (0x51B) has no v2 mapping
    }
    DashAsSensor &s = dashAsSensor;
    if (d[0] == 0x10) {
        s.wheelPressMs = nowMs ? nowMs : 1;  // latch the press
        return;
    }
    if (d[0] != 0x11 || s.wheelPressMs == 0) {
        return;
    }
    const uint32_t pressMs = s.wheelPressMs;
    s.wheelPressMs = 0;  // consumed either way
    if (nowMs < pressMs
        || (nowMs - pressMs) > dashAsProfile.wheelPressTimeoutMs) {
        return;  // stale pair — dropped
    }
    if (s.pending.type != kAsTrigNone) {
        return;  // never overwrite a pending gesture
    }
    uint8_t gear = 0xFF;
    if (s.gearNow == 1) {
        gear = kAsGearD;      // P -> D
    } else if (s.gearNow == 4) {
        gear = kAsGearP;      // D -> P
    } else {
        return;               // N/R/invalid: no v2 target
    }
    s.pending.type = kAsTrigScroll;
    s.pending.side = 0;
    s.pending.dir = 0;
    s.pending.gear = gear;
}

// Consume the pending trigger. Scroll wins over steering (a deliberate
// wheel gesture outranks a possibly-transient angle). Steering stays
// edge-triggered with hysteresis re-arm but is frozen out (sentinel
// scale) until its calibration evidence lands.
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
        t.gear = (t.dir > 0) ? kAsGearD : kAsGearR;
    } else {
        t.gear = (s.gearNow == 4) ? kAsGearR : kAsGearD;
    }
    return t;
}

// ---------------------------------------------------------------------------
// Permit gate, 0x228 source snapshot
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
    bool master = false;             // NVS "as_master"
    bool armed = false;              // NVS "as_armed" intent
    uint32_t armPendingSinceMs = 0;  // 0 = no confirm window open
};

inline DashAsPermit dashAsPermit = {};

// Invalidate latched trigger state on every permit transition: a wheel
// gesture recorded while disarmed (or a steering angle already past the
// threshold at arm time) must never fire on the first armed poll — the
// driver re-earns the trigger by making a fresh gesture/edge.
inline void dashAsInvalidateTriggers()
{
    dashAsSensor.pending = DashAsTrigger{};
    dashAsSensor.steerArmed = false;  // require a fresh edge after (re)arm
    dashAsSensor.wheelPressMs = 0;    // a half-gesture never survives
}

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
            dashAsInvalidateTriggers();
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
    dashAsGateReset();           // no in-flight request survives a disarm
    dashAsEngineReset();         // nor a running burst
    dashAsInvalidateTriggers();  // no latched gesture survives it either
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

// ---------------------------------------------------------------------------
// Engine implementation — burst sequence player riding the live counter
// ---------------------------------------------------------------------------

// Burst code for frame index i of the frozen sequence for `target`.
inline uint8_t dashAsBurstCode(uint8_t target, uint16_t i)
{
    if (target == kAsGearD) {
        return i < dashAsProfile.pdLeadFrames ? dashAsProfile.pdLeadCode
                                              : dashAsProfile.pdMainCode;
    }
    return dashAsProfile.dpCode;
}

inline uint16_t dashAsBurstFrames(uint8_t target)
{
    if (target == kAsGearD) {
        return static_cast<uint16_t>(dashAsProfile.pdLeadFrames)
             + dashAsProfile.pdMainFrames;
    }
    return dashAsProfile.dpFrames;
}

// Compose one burst frame: {0x40, code, counter<<4 | srcLowNibble, crc}.
inline void dashAsComposeFrame(uint8_t code, uint8_t counterNibble,
                               uint8_t out[4])
{
    out[0] = 0x40;
    out[1] = code;
    out[2] = static_cast<uint8_t>((counterNibble << 4)
                                  | (dashAsSbwSource.data[2] & 0x0F));
    out[3] = dashAsCrc8J1850(out, 3);
}

inline void dashAsEngineAbandon(uint8_t reason)
{
    dashAsGateState.lastAbandonReason = reason;
    dashAsGateState.retries = 0;
    dashAsGateState.pendingTargetDi = 0;
    dashAsGateState.pendingDeadlineMs = 0;
    ++dashAsStats.abandoned;
    dashAsEngineReset();
}

// Request a shift. Valid only for the two field-proven transitions
// (P->D, D->P) from a decoded 0x118 gear, with a fresh 0x228 source to
// ride, and only when idle. Never composes an unobserved sequence.
inline bool dashAsRequestShift(uint8_t gear /* DashAsGear */, uint32_t nowMs)
{
    if (dashAsEngine.st != kAsEngIdle) {
        return false;  // single in-flight
    }
    if (gear != kAsGearD && gear != kAsGearP) {
        return false;  // v2 targets only
    }
    const uint8_t cur = dashAsGearFromDi(dashAsSensor.gearNow);
    const bool pd = cur == kAsGearP && gear == kAsGearD;
    const bool dp = cur == kAsGearD && gear == kAsGearP;
    if (!pd && !dp) {
        return false;  // unobserved / same-gear transition
    }
    if (!dashAsSbwSource.seen
        || (nowMs - dashAsSbwSource.ms) > dashAsProfile.sbwSrcFreshMs) {
        return false;  // nothing fresh to ride — fail closed
    }
    const uint8_t targetDi = dashAsGearToDi(gear);
    if (targetDi != 1 && targetDi != 4) {
        return false;
    }
    dashAsEngine.targetGear = gear;
    dashAsEngine.seqIdx = 0;
    dashAsEngine.framesTotal = dashAsBurstFrames(gear);
    dashAsEngine.lastCounter = 0;
    dashAsEngine.lastFrameMs = 0;
    dashAsEngine.retries = 0;
    dashAsEngine.deadlineMs = 0;
    dashAsEngine.st = kAsEngBurst;
    dashAsGateState.pendingTargetDi = targetDi;
    dashAsGateState.pendingDeadlineMs = 0;
    ++dashAsStats.submitted;
    return true;
}

// Engine pump. Returns true and fills out[4] when a frame should be
// transmitted NOW; the caller sends it and, if the driver refuses,
// reports dashAsEngineTxFail() to cancel cleanly (nothing pending).
inline bool dashAsEngineTick(uint32_t nowMs, uint8_t out[4])
{
    DashAsEngine &e = dashAsEngine;
    if (e.st == kAsEngIdle) {
        return false;
    }
    if (dashAsGateState.pendingTargetDi == 0) {
        // confirmed via the 0x118 path (dashAsConfirmGear resets the
        // engine too — this is the belt-and-braces reconciliation)
        dashAsEngineReset();
        return false;
    }
    if (e.st == kAsEngConfirm) {
        if (nowMs <= e.deadlineMs) {
            return false;  // still waiting for the exact gear
        }
        // Confirm deadline expired: budget first (out of budget ->
        // exhausted regardless of source), then a FRESH source to ride
        // or abandon (fail-closed).
        ++e.retries;
        if (e.retries >= dashAsProfile.retryMax) {
            dashAsEngineAbandon(kAsAbandonRetryExhausted);
            return false;
        }
        if (!dashAsSbwSource.seen
            || (nowMs - dashAsSbwSource.ms) > dashAsProfile.sbwSrcFreshMs) {
            dashAsEngineAbandon(kAsAbandonStaleSource);
            return false;
        }
        ++dashAsStats.retried;
        e.seqIdx = 0;
        e.lastFrameMs = 0;
        e.st = kAsEngBurst;
        // fall through: the retry's first frame goes out immediately
    }
    // kAsEngBurst
    if (e.seqIdx > 0
        && (nowMs - e.lastFrameMs) < dashAsProfile.frameCadenceMs) {
        return false;  // pacing
    }
    uint8_t counter;
    if (e.seqIdx == 0) {
        counter = static_cast<uint8_t>(
            ((dashAsSbwSource.data[2] >> 4) + 1u) & 0x0Fu);
    } else {
        counter = static_cast<uint8_t>((e.lastCounter + 1u) & 0x0Fu);
    }
    const uint8_t code = dashAsBurstCode(e.targetGear, e.seqIdx);
    dashAsComposeFrame(code, counter, out);
    e.lastCounter = counter;
    e.lastFrameMs = nowMs;
    dashAsLastSbwTxMs = nowMs;  // open the own-echo suppression window
    ++e.seqIdx;
    ++dashAsStats.txOk;
    if (e.seqIdx >= e.framesTotal) {
        e.st = kAsEngConfirm;
        e.deadlineMs = nowMs + dashAsProfile.confirmTimeoutMs;
        dashAsGateState.pendingDeadlineMs = e.deadlineMs;
    }
    return true;
}

// The emitted frame never hit the bus: cancel the request with no
// pending state (a failed send is not a retry — v1 semantics).
inline void dashAsEngineTxFail()
{
    if (dashAsEngine.st == kAsEngIdle) {
        return;
    }
    ++dashAsStats.txFail;
    dashAsGateReset();
    dashAsEngineReset();
}

#endif  // defined(DRIVER_P4_3CAN)
