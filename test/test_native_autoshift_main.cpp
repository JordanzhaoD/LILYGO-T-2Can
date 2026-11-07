// Native harness for the autoshift engine core (dash_autoshift.h).
// Compiled and run by test/test_autoshift_contract.py via:
//   clang++ -std=c++17 -Wall -Wextra -Werror -DDRIVER_P4_3CAN -I include
// The -DDRIVER_P4_3CAN is mandatory: the header lives inside the P4
// guard and preprocesses to nothing without it.
//
// v2 (2026-11-07, ka750 evidence upgrade): sensors move to 0x106 speed /
// 0x116 brake / 0x514 wheel-press pairs; the composer becomes a frozen
// two-phase sequence player (P->D = C4 x4 + C8 x25, D->P = D0 x12) riding
// the native MsgTxmtId; the engine owns the in-flight lifecycle
// (burst -> confirm -> auto-retry -> abandon). See
// docs/superpowers/specs/2026-11-07-autoshift-v2-evidence-upgrade-design.md
//
// Plain C++ (no Unity): the pytest contract test asserts exit code 0
// and the "ALL PASS" banner.
//
// PIO-inert guard: PlatformIO's native test builder globs top-level
// test/*.cpp into EVERY native unit build, where this file would fail
// (no -DDRIVER_P4_3CAN there) and collide with Unity's main() even if
// it compiled. The pytest clang++ invocation never defines NATIVE_BUILD,
// so the guard is transparent to the real consumer of this file.
#ifndef NATIVE_BUILD

#include <cstdio>

#include "dash_autoshift.h"

static int g_fail = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            ++g_fail;                                                   \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);  \
        }                                                               \
    } while (0)

// The 16 desk-verified static vectors for stalk state 40 C0:
// dashAsCrc8J1850({40, C0, c0}) == crc for c = 0..F.
static const struct { uint8_t counterByte; uint8_t crc; } kPinned[16] = {
    {0x00, 0xB2}, {0x10, 0x7F}, {0x20, 0x35}, {0x30, 0xF8},
    {0x40, 0xA1}, {0x50, 0x6C}, {0x60, 0x26}, {0x70, 0xEB},
    {0x80, 0x94}, {0x90, 0x59}, {0xA0, 0x13}, {0xB0, 0xDE},
    {0xC0, 0x87}, {0xD0, 0x4A}, {0xE0, 0x00}, {0xF0, 0xCD},
};

// RAII: restore profile + sensor + gate + engine state on scope exit so
// tests can mutate the runtime copies freely.
struct ProfileGuard {
    DashAutoshiftProfile saved;
    ProfileGuard() : saved(dashAsProfile) {}
    ~ProfileGuard()
    {
        dashAsProfile = saved;
        dashAsSensor = DashAsSensor{};
        dashAsGateReset();
        dashAsEngineReset();
        dashAsSbwSource = DashAsSbwSource{};
        dashAsLastSbwTxMs = 0;
    }
};

int main()
{
    // --- CRC8 J1850 against all 16 pinned vehicle vectors ---
    for (const auto &v : kPinned) {
        const uint8_t frame[3] = {0x40, 0xC0, v.counterByte};
        CHECK(dashAsCrc8J1850(frame, 3) == v.crc);
    }

    // --- DI_gear code -> stalk-table index mapping (unchanged v1 fact) ---
    CHECK(dashAsGearFromDi(1) == kAsGearP);
    CHECK(dashAsGearFromDi(2) == kAsGearR);
    CHECK(dashAsGearFromDi(3) == kAsGearN);
    CHECK(dashAsGearFromDi(4) == kAsGearD);
    CHECK(dashAsGearFromDi(0) == 0xFF);  // DI INVALID
    CHECK(dashAsGearFromDi(5) == 0xFF);
    CHECK(dashAsGearFromDi(7) == 0xFF);  // DI SNA

    // ================= v2: frozen profile pins (ka750 2026-11-07) =================
    // P->D = C4 x4 then C8 x25 (s4 sweep @209.789); D->P = D0 x12; 10 ms
    // cadence; speed/brake freshness from the 100 Hz CanC streams.
    CHECK(kAsProfile.pdLeadCode == 0xC4);
    CHECK(kAsProfile.pdLeadFrames == 4);
    CHECK(kAsProfile.pdMainCode == 0xC8);
    CHECK(kAsProfile.pdMainFrames == 25);
    CHECK(kAsProfile.dpCode == 0xD0);
    CHECK(kAsProfile.dpFrames == 12);
    CHECK(kAsProfile.frameCadenceMs == 10);
    CHECK(kAsProfile.speedFreshMs == 300);
    CHECK(kAsProfile.brakeFreshMs == 300);
    CHECK(kAsProfile.wheelPressTimeoutMs == 2000);
    // still-frozen-out surfaces: angle scale and obstacle maps stay
    // sentinel until their field evidence lands.
    CHECK(kAsProfile.angleScaleT10 == 0);
    CHECK(kAsProfile.oocBlockByte == 0xFF);
    CHECK(kAsProfile.sdiBlockByte == 0xFF);

    // ================= v2: 0x106 speed decoder =================
    // b5:b6 big-endian, 4 mm/s per tick; the three ka750 calibration
    // points are 797/1354/2104 tick = 11.5/19.5/30.3 km/h.
    {
        ProfileGuard g;
        const uint8_t s797[8] =  {0, 0, 0, 0, 0, 0x03, 0x1D, 0};  // 797
        const uint8_t s1354[8] = {0, 0, 0, 0, 0, 0x05, 0x4A, 0};  // 1354
        const uint8_t s2104[8] = {0, 0, 0, 0, 0, 0x08, 0x38, 0};  // 2104
        CHECK(dashAsSensor.speedTick == 0xFFFF);  // undecoded sentinel
        dashAsOnSpeed106(s797, 1000);
        CHECK(dashAsSensor.speedTick == 797);
        CHECK(dashAsSensor.speedKph == 11);       // 11.48 rounds to 11
        CHECK(dashAsSpeedFresh(1000));
        CHECK(!dashAsSpeedFresh(1000 + kAsProfile.speedFreshMs + 1));
        dashAsOnSpeed106(s1354, 2000);
        CHECK(dashAsSensor.speedKph == 19);       // 19.5
        dashAsOnSpeed106(s2104, 3000);
        CHECK(dashAsSensor.speedKph == 30);       // 30.3
        // standstill decodes as 0 km/h, still fresh and decodable
        const uint8_t zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        dashAsOnSpeed106(zero, 4000);
        CHECK(dashAsSensor.speedTick == 0);
        CHECK(dashAsSensor.speedKph == 0);
    }

    // ================= v2: 0x116 mirror brake decoder =================
    // b1 bit7 = brake applied (41.8 s continuous hold, ka750 s4).
    {
        ProfileGuard g;
        CHECK(dashAsSensor.brake == 0xFF);        // undecoded sentinel
        const uint8_t hold[8] =  {0, 0x9B, 0, 0, 0, 0, 0, 0};  // bit7 + N mirror
        const uint8_t free[8] =  {0, 0x1B, 0, 0, 0, 0, 0, 0};
        dashAsOnMirror116(hold, 1000);
        CHECK(dashAsSensor.brake == 1);
        CHECK(dashAsBrakeFresh(1000));
        CHECK(!dashAsBrakeFresh(1000 + kAsProfile.brakeFreshMs + 1));
        dashAsOnMirror116(free, 2000);
        CHECK(dashAsSensor.brake == 0);
        CHECK(dashAsBrakeFresh(2000));
    }

    // ================= v2: 0x514/0x51B wheel press pairs =================
    // Left page 0x514; b0=0x10 press / 0x11 release; a completed PAIR
    // arms one trigger whose target maps P->D / D->P from the CURRENT
    // 0x118 gear. Right page 0x51B never triggers in v2.
    {
        ProfileGuard g;
        const uint8_t t2p[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};   // gear P
        const uint8_t t2d[8] = {0, 0, 0, 0x40, 0, 0, 0, 0};   // gear D
        const uint8_t t2n[8] = {0, 0, 0, 0x30, 0, 0, 0, 0};   // gear N
        const uint8_t pressL[8] =  {0x10, 0x01, 0x05, 0, 0, 0, 0, 0};
        const uint8_t releaseL[8] = {0x11, 0x00, 0x04, 0, 0, 0, 0, 0};
        // press alone never fires
        dashAsOnTorque2(t2p, 1000);
        dashAsOnWheelEvent(0x514, pressL, 2000);
        CHECK(dashAsPollTrigger(2100).type == kAsTrigNone);
        // completed pair in P requests D
        dashAsOnWheelEvent(0x514, releaseL, 2200);
        DashAsTrigger t = dashAsPollTrigger(2300);
        CHECK(t.type == kAsTrigScroll);
        CHECK(t.side == 0);                       // left wheel
        CHECK(t.gear == kAsGearD);
        CHECK(dashAsPollTrigger(2310).type == kAsTrigNone);  // consumed
        // in D the same gesture requests P
        dashAsOnTorque2(t2d, 3000);
        dashAsOnWheelEvent(0x514, pressL, 3100);
        dashAsOnWheelEvent(0x514, releaseL, 3200);
        t = dashAsPollTrigger(3300);
        CHECK(t.type == kAsTrigScroll);
        CHECK(t.gear == kAsGearP);
        // release arriving too late is dropped, press state cleared
        dashAsOnWheelEvent(0x514, pressL, 4000);
        dashAsOnWheelEvent(0x514, releaseL,
                           4000 + kAsProfile.wheelPressTimeoutMs + 1);
        CHECK(dashAsPollTrigger(7000).type == kAsTrigNone);
        // right page ignored entirely
        dashAsOnTorque2(t2p, 8000);
        dashAsOnWheelEvent(0x51B, pressL, 8100);
        dashAsOnWheelEvent(0x51B, releaseL, 8200);
        CHECK(dashAsPollTrigger(8300).type == kAsTrigNone);
        // gear N (or R / invalid): the gesture completes but maps to no
        // target — v2 supports P<->D only
        dashAsOnTorque2(t2n, 9000);
        dashAsOnWheelEvent(0x514, pressL, 9100);
        dashAsOnWheelEvent(0x514, releaseL, 9200);
        CHECK(dashAsPollTrigger(9300).type == kAsTrigNone);
        // arm invalidation still clears a latched press (disarm mid-gesture)
        dashAsOnTorque2(t2p, 10000);
        dashAsOnWheelEvent(0x514, pressL, 10100);
        dashAsInvalidateTriggers();
        dashAsOnWheelEvent(0x514, releaseL, 10200);
        CHECK(dashAsPollTrigger(10300).type == kAsTrigNone);
    }

    // ================= v2: gate chain (order re-pinned) =================
    const uint32_t kNow = 10000;
    auto okInputs = []() {
        DashAsInputs in = {};
        in.master = true;
        in.torque2Fresh = true;
        in.speedFresh = true;
        in.speedKph = 8;
        in.brakeFresh = true;
        in.brakeApplied = true;
        in.gearNow = 1;             // DI P
        in.gearTarget = kAsGearD;
        in.obstacleFreshAndClear = true;
        in.lastShiftMs = 0;         // no previous shift
        in.nowCount = 0;
        return in;
    };
    // master first — even with everything else failing too
    {
        DashAsInputs in = okInputs();
        in.master = false;
        in.brakeApplied = false;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_MASTER);
    }
    // stale 0x118
    {
        DashAsInputs in = okInputs();
        in.torque2Fresh = false;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_STALE_118);
    }
    // stale speed outranks over-limit (undecoded is never "over")
    {
        DashAsInputs in = okInputs();
        in.speedFresh = false;
        in.speedKph = 40;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_STALE_SPEED);
    }
    // over-limit speed with a fresh decode
    {
        DashAsInputs in = okInputs();
        in.speedKph = 9;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_SPEED);
    }
    // stale brake outranks released brake
    {
        DashAsInputs in = okInputs();
        in.brakeFresh = false;
        in.brakeApplied = false;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_STALE_BRAKE);
    }
    // brake released
    {
        DashAsInputs in = okInputs();
        in.brakeApplied = false;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_BRAKE);
    }
    // cooldown / count / obstacle / pending / already-target / happy
    {
        DashAsInputs in = okInputs();
        in.lastShiftMs = kNow - 500;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_COOLDOWN);
        in = okInputs();
        in.nowCount = kAsProfile.shiftCountMax;
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_COUNT);
        in = okInputs();
        in.obstacleFreshAndClear = false;
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_OBSTACLE);
        in = okInputs();
        dashAsGateState.pendingTargetDi = 4;
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_PENDING);
        dashAsGateReset();
        in = okInputs();
        in.gearNow = 4;
        in.gearTarget = kAsGearD;
        CHECK(dashAsGateEval(kNow, in) == AS_ALREADY_TARGET);
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, okInputs()) == AS_SUBMIT);
    }

    // ================= v2: engine — sequence request + validation =================
    {
        ProfileGuard g;
        // a fresh 0x118 gear + a fresh 0x228 source frame
        const uint8_t t2p[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(t2p, 1000);
        const uint8_t idle[4] = {0x40, 0xC0, 0x35, 0x00};  // counter 3, lo 5
        dashAsOnSbw0228(idle, 1000);
        // unsupported transitions never start (v2 = P<->D only)
        CHECK(!dashAsRequestShift(kAsGearR, 1100));   // P -> R unobserved
        CHECK(!dashAsRequestShift(kAsGearN, 1100));   // P -> N not a target
        CHECK(!dashAsRequestShift(kAsGearPBtn, 1100));
        CHECK(dashAsEngineState() == kAsEngIdle);
        // age == budget passes (source fed at 1000, budget 100)
        CHECK(dashAsRequestShift(kAsGearD, 1100));
        CHECK(dashAsEngineState() == kAsEngBurst);
        dashAsEngineReset();
        // stale source -> refuse
        dashAsSbwSource.ms = 1100 - kAsProfile.sbwSrcFreshMs - 1;
        CHECK(!dashAsRequestShift(kAsGearD, 1100));
    }

    // ================= v2: engine — P->D burst emission =================
    {
        ProfileGuard g;
        dashAsStats = DashAsStats{};
        const uint8_t t2p[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(t2p, 1000);
        const uint8_t idle[4] = {0x40, 0xC0, 0x35, 0x00};  // counter 3
        dashAsOnSbw0228(idle, 1000);
        CHECK(dashAsRequestShift(kAsGearD, 1100));
        CHECK(dashAsStats.submitted == 1);
        CHECK(dashAsGateState.pendingTargetDi == 4);
        CHECK(dashAsEngineState() == kAsEngBurst);

        // 29 frames total: C4 x4 then C8 x25, one per 10 ms tick, the
        // first immediate; counter walks +1 per emitted frame from the
        // source counter + 1 (3 -> 4), wrapping mod 16 at frame 13.
        uint8_t out[4];
        uint8_t expCounter = 4;
        uint32_t t = 1100;
        for (int i = 0; i < 29; ++i) {
            // pacing: nothing before the cadence elapses
            if (i > 0) {
                CHECK(!dashAsEngineTick(t - 1, out));
            }
            CHECK(dashAsEngineTick(t, out));
            const uint8_t expCode = (i < 4) ? 0xC4 : 0xC8;
            CHECK(out[0] == 0x40);
            CHECK(out[1] == expCode);
            CHECK(((out[2] >> 4) & 0x0F) == (expCounter & 0x0F));
            CHECK((out[2] & 0x0F) == 0x05);       // low nibble preserved
            CHECK(out[3] == dashAsCrc8J1850(out, 3));
            ++expCounter;
            t += 10;
        }
        CHECK(dashAsStats.txOk == 29);
        CHECK(dashAsEngineState() == kAsEngConfirm);
        CHECK(dashAsGateState.pendingTargetDi == 4);   // still in flight
        // wrong gear inside the window: WAIT, never confirms
        const uint8_t t2n[8] = {0, 0, 0, 0x30, 0, 0, 0, 0};  // N
        dashAsOnTorque2(t2n, t + 10);
        CHECK(dashAsSensor.lastConfirm == AS_CONFIRM_WAIT);
        CHECK(dashAsEngineState() == kAsEngConfirm);
        // confirm window: exact gear on the next 0x118 completes
        const uint8_t t2d[8] = {0, 0, 0, 0x40, 0, 0, 0, 0};
        dashAsOnTorque2(t2d, t + 100);
        CHECK(dashAsSensor.lastConfirm == AS_CONFIRM_COMPLETE);
        CHECK(dashAsEngineState() == kAsEngIdle);
        CHECK(dashAsGateState.pendingTargetDi == 0);
        CHECK(dashAsStats.confirmed == 1);
        CHECK(dashAsRuntime.shiftCount == 1);
        CHECK(dashAsRuntime.lastShiftMs == t + 100);
    }

    // ================= v2: engine — D->P burst emission =================
    {
        ProfileGuard g;
        const uint8_t t2d[8] = {0, 0, 0, 0x40, 0, 0, 0, 0};
        dashAsOnTorque2(t2d, 1000);
        const uint8_t idle[4] = {0x40, 0xC0, 0xF1, 0x00};  // counter F
        dashAsOnSbw0228(idle, 1000);
        CHECK(dashAsRequestShift(kAsGearP, 1100));
        uint8_t out[4];
        uint8_t expCounter = 0;                   // F + 1 wraps to 0
        uint32_t t = 1100;
        for (int i = 0; i < 12; ++i) {
            CHECK(dashAsEngineTick(t, out));
            CHECK(out[1] == 0xD0);
            CHECK(((out[2] >> 4) & 0x0F) == (expCounter & 0x0F));
            CHECK(out[3] == dashAsCrc8J1850(out, 3));
            ++expCounter;
            t += 10;
        }
        CHECK(dashAsEngineState() == kAsEngConfirm);
    }

    // ================= v2: engine — timeout -> auto-retry -> abandon =================
    {
        ProfileGuard g;
        dashAsStats = DashAsStats{};
        const uint8_t t2p[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(t2p, 1000);
        const uint8_t idle[4] = {0x40, 0xC0, 0x35, 0x00};
        dashAsOnSbw0228(idle, 1000);
        CHECK(dashAsRequestShift(kAsGearD, 1100));
        uint8_t out[4];
        uint32_t t = 1100;
        for (int i = 0; i < 29; ++i) {
            CHECK(dashAsEngineTick(t, out));
            t += 10;
        }
        CHECK(dashAsEngineState() == kAsEngConfirm);
        // the engine anchors the deadline at the LAST FRAME time; the
        // loop left t one cadence past it
        const uint32_t deadline = t - 10 + kAsProfile.confirmTimeoutMs;
        // inside the window: no confirm, still waiting
        CHECK(!dashAsEngineTick(deadline - 1, out));
        CHECK(dashAsEngineState() == kAsEngConfirm);
        // the native stream moved on: a fresher source frame (counter 9)
        // must be what the retry rides, NOT our stale burst counter
        const uint8_t idle2[4] = {0x40, 0xC0, 0x95, 0x00};
        dashAsOnSbw0228(idle2, deadline);
        // deadline expiry fires the auto-retry: burst restarts
        CHECK(dashAsEngineTick(deadline + 1, out));
        CHECK(out[1] == 0xC4);
        CHECK(((out[2] >> 4) & 0x0F) == 0x0A);    // 9 + 1
        CHECK(dashAsStats.retried == 1);
        CHECK(dashAsEngineState() == kAsEngBurst);
        // run the retry burst out, miss again -> abandoned (retryMax = 2)
        t = deadline + 11;
        for (int i = 1; i < 29; ++i) {
            CHECK(dashAsEngineTick(t, out));
            t += 10;
        }
        CHECK(dashAsEngineState() == kAsEngConfirm);
        CHECK(!dashAsEngineTick(t + kAsProfile.confirmTimeoutMs + 1, out));
        CHECK(dashAsEngineState() == kAsEngIdle);
        CHECK(dashAsStats.abandoned == 1);
        CHECK(dashAsLastAbandonReason() == kAsAbandonRetryExhausted);
        CHECK(dashAsGateState.pendingTargetDi == 0);
    }

    // ================= v2: engine — stale source at retry abandons =================
    {
        ProfileGuard g;
        const uint8_t t2p[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(t2p, 1000);
        const uint8_t idle[4] = {0x40, 0xC0, 0x35, 0x00};
        dashAsOnSbw0228(idle, 1000);
        CHECK(dashAsRequestShift(kAsGearD, 1100));
        uint8_t out[4];
        uint32_t t = 1100;
        for (int i = 0; i < 29; ++i) {
            CHECK(dashAsEngineTick(t, out));
            t += 10;
        }
        // no fresh source arrives; the own-echo window is long over and
        // the snapshot ages past the budget -> retry impossible: abandon
        // with the stale-source reason
        const uint32_t tStale = t + kAsProfile.confirmTimeoutMs + 1;
        CHECK(!dashAsEngineTick(tStale, out));
        CHECK(dashAsEngineState() == kAsEngIdle);
        CHECK(dashAsLastAbandonReason() == kAsAbandonStaleSource);
        CHECK(dashAsGateState.pendingTargetDi == 0);
    }

    // ================= v2: engine — mid-burst confirm stops the burst =================
    {
        ProfileGuard g;
        const uint8_t t2p[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(t2p, 1000);
        const uint8_t idle[4] = {0x40, 0xC0, 0x35, 0x00};
        dashAsOnSbw0228(idle, 1000);
        CHECK(dashAsRequestShift(kAsGearD, 1100));
        uint8_t out[4];
        CHECK(dashAsEngineTick(1100, out));       // frame 1
        // the car shifts after ONE command frame (observed in s4):
        const uint8_t t2d[8] = {0, 0, 0, 0x40, 0, 0, 0, 0};
        dashAsOnTorque2(t2d, 1200);
        CHECK(dashAsEngineState() == kAsEngIdle);
        CHECK(dashAsStats.confirmed == 1);
        CHECK(!dashAsEngineTick(1210, out));      // burst cancelled
    }

    // ================= v2: engine — busy refusal + disarm cancel =================
    {
        ProfileGuard g;
        const uint8_t t2p[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(t2p, 1000);
        const uint8_t idle[4] = {0x40, 0xC0, 0x35, 0x00};
        dashAsOnSbw0228(idle, 1000);
        CHECK(dashAsRequestShift(kAsGearD, 1100));
        CHECK(!dashAsRequestShift(kAsGearP, 1200));   // busy (and D-gear
        // context is not even established anymore — single in-flight)
        dashAsDisarm();
        CHECK(dashAsEngineState() == kAsEngIdle);
        CHECK(dashAsGateState.pendingTargetDi == 0);
    }

    // ================= v2: engine — TX failure cancels cleanly =================
    {
        ProfileGuard g;
        const uint8_t t2p[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(t2p, 1000);
        const uint8_t idle[4] = {0x40, 0xC0, 0x35, 0x00};
        dashAsOnSbw0228(idle, 1000);
        CHECK(dashAsRequestShift(kAsGearD, 1100));
        uint8_t out[4];
        CHECK(dashAsEngineTick(1100, out));       // emitted — caller send FAILS
        dashAsEngineTxFail();
        CHECK(dashAsEngineState() == kAsEngIdle);
        CHECK(dashAsStats.txFail == 1);
        CHECK(dashAsGateState.pendingTargetDi == 0);
    }

    // ================= 0x118 decode: gear + freshness + confirm feed =================
    {
        ProfileGuard g;
        const uint8_t d[8] = {3, 0, 0, 0x40, 0, 0, 0x20, 0};  // D
        dashAsOnTorque2(d, 1000);
        CHECK(dashAsSensor.gearNow == 4);
        CHECK(dashAsTorque2Fresh(1000));
        CHECK(!dashAsTorque2Fresh(1000 + kAsProfile.t2FreshMs + 1));
        // v2 removed the 0x118 brake/speed maps: those fields stay
        // undecoded regardless of payload (0x106/0x116 own them now)
        CHECK(dashAsSensor.brake == 0xFF);
        CHECK(dashAsSensor.speedTick == 0xFFFF);
        CHECK(dashAsSensor.speedKph == 0xFF);
    }

    // ================= 0x00E angle: still sentinel-frozen =================
    {
        ProfileGuard g;  // angleScaleT10 stays 0
        const uint8_t big[8] = {0x01, 0x00, 0, 0, 0, 0, 0, 0};
        dashAsOnAngle00E(big, 1100);
        CHECK(dashAsSensor.angleDegT10 == 0x7FFF);  // invalid
        CHECK(dashAsPollTrigger(1200).type == kAsTrigNone);
    }
    // (frozen-simulation steering behavior stays covered by the poll
    // contract: with the sentinel map no steering trigger ever fires)

    // ================= obstacle: unchanged sentinel fail-closed =================
    {
        ProfileGuard g;
        CHECK(!dashAsObstacleFreshAndClear(1000));  // absent != clear
    }

    // ================= permit chain (unchanged v1 semantics) =================
    {
        ProfileGuard g;
        dashAsPermit = DashAsPermit{};
        CHECK(dashAsArmRequest(1000) == kAsArmDeniedMaster);
        dashAsSetMaster(true);
        CHECK(dashAsPermit.master);
        CHECK(dashAsArmRequest(2000) == kAsArmOpened);
        CHECK(!dashAsPermit.armed);
        CHECK(dashAsArmRequest(2000 + kAsArmWindowMs) == kAsArmEnabled);
        CHECK(dashAsPermit.armed);
        CHECK(dashAsArmRequest(3000) == kAsArmAlreadyArmed);
        dashAsDisarm();
        CHECK(!dashAsPermit.armed);
        dashAsPermit = DashAsPermit{};
        dashAsSetMaster(true);
        CHECK(dashAsArmRequest(1000) == kAsArmOpened);
        CHECK(dashAsArmRequest(1000 + kAsArmWindowMs + 1) == kAsArmExpired);
        CHECK(dashAsPermit.armPendingSinceMs == 0);
        CHECK(dashAsArmRequest(2000) == kAsArmOpened);
        CHECK(dashAsArmRequest(2100) == kAsArmEnabled);
        dashAsSetMaster(false);
        CHECK(!dashAsPermit.master);
        CHECK(!dashAsPermit.armed);
    }

    // ================= 0x228 source snapshot + own-echo suppression =================
    {
        ProfileGuard g;
        const uint8_t idle[4] = {0x40, 0xC0, 0x30, 0x00};
        CHECK(!dashAsSbwSource.seen);
        dashAsOnSbw0228(idle, 1000);
        CHECK(dashAsSbwSource.seen);
        CHECK(dashAsSbwSource.data[0] == 0x40);
        const uint8_t other[4] = {0x50, 0xC0, 0x30, 0x00};
        dashAsLastSbwTxMs = 1000;
        dashAsOnSbw0228(other, 1010);  // 10ms after our TX — suppressed
        CHECK(dashAsSbwSource.data[0] == 0x40);
        dashAsOnSbw0228(other, 1030);  // exactly the window edge — suppressed
        CHECK(dashAsSbwSource.data[0] == 0x40);
        dashAsOnSbw0228(other, 1031);  // past the window — accepted
        CHECK(dashAsSbwSource.data[0] == 0x50);
        CHECK(dashAsSbwSource.ms == 1031);
    }

    // gear -> DI mapping (P button requests P; N keeps its code)
    CHECK(dashAsGearToDi(kAsGearP) == 1 && dashAsGearToDi(kAsGearPBtn) == 1);
    CHECK(dashAsGearToDi(kAsGearR) == 2 && dashAsGearToDi(kAsGearN) == 3);
    CHECK(dashAsGearToDi(kAsGearD) == 4);
    CHECK(dashAsGearToDi(kAsGearCount) == 0xFF);

    if (g_fail == 0) {
        std::printf("native autoshift: ALL PASS\n");
    }
    return g_fail == 0 ? 0 : 1;
}

#endif // NATIVE_BUILD
