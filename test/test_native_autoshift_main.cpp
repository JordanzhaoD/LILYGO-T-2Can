// Native harness for the autoshift engine core (dash_autoshift.h).
// Compiled and run by test/test_autoshift_contract.py via:
//   clang++ -std=c++17 -Wall -Wextra -Werror -DDRIVER_P4_3CAN -I include
// The -DDRIVER_P4_3CAN is mandatory: the header lives inside the P4
// guard and preprocesses to nothing without it.
//
// Plain C++ (no Unity): the pytest contract test asserts exit code 0
// and the "ALL PASS" banner.

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

// RAII: simulate the Task-2 freeze by mutating the runtime profile copy,
// then restore profile + sensor + gate state on scope exit.
struct ProfileGuard {
    DashAutoshiftProfile saved;
    ProfileGuard() : saved(dashAsProfile) {}
    ~ProfileGuard()
    {
        dashAsProfile = saved;
        dashAsSensor = DashAsSensor{};
        dashAsGateReset();
    }
};

int main()
{
    // --- CRC8 J1850 against all 16 pinned vehicle vectors ---
    for (const auto &v : kPinned) {
        const uint8_t frame[3] = {0x40, 0xC0, v.counterByte};
        CHECK(dashAsCrc8J1850(frame, 3) == v.crc);
    }

    // --- compose: pair write, counter +1, low nibble kept, crc recompute ---
    // (simulate a frozen stalk pair — the sentinel 0xFF table refuses)
    {
        ProfileGuard g;
        for (int i = 0; i < kAsGearCount; ++i) {
            dashAsProfile.stalkB0[i] = 0x40 + static_cast<uint8_t>(i) * 0x10;
            dashAsProfile.stalkB1[i] = 0xC0;
        }
        uint8_t out[4];
        const uint8_t src[4] = {0x40, 0xC0, 0x35, 0x00};  // counter 3, lo 5
        CHECK(dashAsSbwCompose(src, 0, kAsGearD, out));
        CHECK(out[0] == 0x70);  // frozen pair for gear D: 0x40 + 3*0x10
        CHECK(out[1] == 0xC0);
        CHECK(((out[2] >> 4) & 0x0F) == 0x04);            // 3 + 1
        CHECK((out[2] & 0x0F) == 0x05);                   // preserved
        CHECK(out[3] == dashAsCrc8J1850(out, 3));

        // --- compose: counter wraps 0xF -> 0x0 ---
        const uint8_t wrapSrc[4] = {0x40, 0xC0, 0xF5, 0x00};
        CHECK(dashAsSbwCompose(wrapSrc, 0, kAsGearP, out));
        CHECK(((out[2] >> 4) & 0x0F) == 0x00);
        CHECK((out[2] & 0x0F) == 0x05);
        CHECK(out[3] == dashAsCrc8J1850(out, 3));

        // --- freshness budget: age == budget passes, age > budget refuses ---
        CHECK(dashAsSbwCompose(src, kAsProfile.sbwSrcFreshMs, kAsGearD, out));
        CHECK(!dashAsSbwCompose(src, kAsProfile.sbwSrcFreshMs + 1,
                                kAsGearD, out));
        CHECK(!dashAsSbwCompose(src, 0, kAsGearCount, out));  // bad index
    }
    // sentinel stalk table (unfrozen profile) must refuse even fresh input
    {
        uint8_t out4[4] = {0, 0, 0, 0};
        const uint8_t src4[4] = {0x40, 0xC0, 0x35, 0x00};
        CHECK(!dashAsSbwCompose(src4, 0, kAsGearD, out4));
    }

    // --- sentinel: profile is UNFROZEN until the Task-2 field session ---
    for (int i = 0; i < kAsGearCount; ++i) {
        CHECK(kAsProfile.stalkB0[i] == 0xFF);
        CHECK(kAsProfile.stalkB1[i] == 0xFF);
    }

    // --- DI_gear code -> stalk-table index mapping ---
    CHECK(dashAsGearFromDi(1) == kAsGearP);
    CHECK(dashAsGearFromDi(2) == kAsGearR);
    CHECK(dashAsGearFromDi(3) == kAsGearN);
    CHECK(dashAsGearFromDi(4) == kAsGearD);
    CHECK(dashAsGearFromDi(0) == 0xFF);  // DI INVALID
    CHECK(dashAsGearFromDi(5) == 0xFF);
    CHECK(dashAsGearFromDi(7) == 0xFF);  // DI SNA

    // ================= Task 4: gate chain matrix =================
    const uint32_t kNow = 10000;
    auto okInputs = []() {
        DashAsInputs in = {};
        in.master = true;
        in.torque2Fresh = true;
        in.speedKph = 8;
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
        in.brakeApplied = false;    // would also block; master must win
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
    // speed limit: 8 passes (full happy path below), 9 blocks
    {
        DashAsInputs in = okInputs();
        in.speedKph = 9;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_SPEED);
    }
    // brake released
    {
        DashAsInputs in = okInputs();
        in.brakeApplied = false;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_BRAKE);
    }
    // cooldown window (last shift 500 ms ago, cooldown 1000 ms)
    {
        DashAsInputs in = okInputs();
        in.lastShiftMs = kNow - 500;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_COOLDOWN);
    }
    // per-session count at limit
    {
        DashAsInputs in = okInputs();
        in.nowCount = kAsProfile.shiftCountMax;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_COUNT);
    }
    // obstacle stale or blocked — absent telemetry is NOT clear
    {
        DashAsInputs in = okInputs();
        in.obstacleFreshAndClear = false;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_BLOCK_OBSTACLE);
    }
    // single in-flight: a pending request blocks new submissions
    {
        dashAsGateReset();
        CHECK(dashAsSubmit(4, kNow));            // target DI D
        CHECK(dashAsGateEval(kNow, okInputs()) == AS_BLOCK_PENDING);
    }
    // already at target
    {
        DashAsInputs in = okInputs();
        in.gearNow = 4;                          // DI D
        in.gearTarget = kAsGearD;
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, in) == AS_ALREADY_TARGET);
    }
    // happy path
    {
        dashAsGateReset();
        CHECK(dashAsGateEval(kNow, okInputs()) == AS_SUBMIT);
    }

    // --- close loop ---
    // exact-code confirm: DI 4 matches pending DI 4
    {
        dashAsGateReset();
        CHECK(dashAsSubmit(4, kNow));
        CHECK(dashAsConfirmGear(4, kNow + 100) == AS_CONFIRM_COMPLETE);
        // pending cleared -> gate open again
        CHECK(dashAsGateEval(kNow + 100, okInputs()) == AS_SUBMIT);
    }
    // codes 0 (INVALID) and 7 (SNA) never confirm
    {
        dashAsGateReset();
        CHECK(dashAsSubmit(4, kNow));
        CHECK(dashAsConfirmGear(0, kNow + 100) == AS_CONFIRM_WAIT);
        CHECK(dashAsConfirmGear(7, kNow + 200) == AS_CONFIRM_WAIT);
        CHECK(dashAsConfirmGear(4, kNow + 300) == AS_CONFIRM_COMPLETE);
    }
    // N (3) matches its own pending target but is outside {1,2,4}: waits
    {
        dashAsGateReset();
        CHECK(dashAsSubmit(3, kNow));
        CHECK(dashAsConfirmGear(3, kNow + 100) == AS_CONFIRM_WAIT);
    }
    // no pending -> idle
    {
        dashAsGateReset();
        CHECK(dashAsConfirmGear(4, kNow) == AS_CONFIRM_IDLE);
    }
    // deadline expiry: retry first, abandon at retryMax with a reason
    {
        dashAsGateReset();
        CHECK(dashAsSubmit(4, kNow));                       // deadline kNow+1000
        CHECK(dashAsConfirmGear(1, kNow + 1001) == AS_CONFIRM_RETRY);
        CHECK(dashAsSubmit(4, kNow + 2000));                // the one retry
        CHECK(dashAsConfirmGear(1, kNow + 3001) == AS_CONFIRM_ABANDONED);
        CHECK(dashAsLastAbandonReason() != 0);
        // abandoned -> pending cleared -> gate open (caller re-arms policy)
        CHECK(dashAsGateEval(kNow + 3001, okInputs()) == AS_SUBMIT);
    }

    // ================= Task 5: decoders + triggers =================
    // --- 0x118 decode with the simulated freeze (gear hardcoded b3>>4) ---
    {
        ProfileGuard g;
        dashAsProfile.t2BrakeByte = 6;
        dashAsProfile.t2BrakeMask = 0x20;
        dashAsProfile.t2SpeedByte = 0;
        dashAsProfile.t2SpeedScaleH = 100;  // 1.00 kph/bit
        const uint8_t d[8] = {3, 0, 0, 0x40, 0, 0, 0x20, 0};  // D, brake, 3kph
        dashAsOnTorque2(d, 1000);
        CHECK(dashAsSensor.gearNow == 4);
        CHECK(dashAsSensor.brake == 1);
        CHECK(dashAsSensor.speedKph == 3);
        CHECK(dashAsTorque2Fresh(1000));
        CHECK(!dashAsTorque2Fresh(1000 + dashAsProfile.t2FreshMs + 1));
        const uint8_t nobrake[8] = {0, 0, 0, 0x10, 0, 0, 0x00, 0};
        dashAsOnTorque2(nobrake, 2000);
        CHECK(dashAsSensor.brake == 0);
        CHECK(dashAsSensor.speedKph == 0);
    }
    // --- 0x118 sentinel map: freshness + gear still live, fields unknown ---
    {
        ProfileGuard g;
        const uint8_t d[8] = {5, 0, 0, 0x10, 0, 0, 0xFF, 0};
        dashAsOnTorque2(d, 1000);
        CHECK(dashAsSensor.gearNow == 1);
        CHECK(dashAsSensor.brake == 0xFF);  // unknown, NOT "released"
        CHECK(dashAsSensor.speedKph == 0xFF);
        CHECK(dashAsTorque2Fresh(1000));
    }
    // --- close loop rides the decoder: confirm on the next 0x118 ---
    {
        ProfileGuard g;
        CHECK(dashAsSubmit(4, 1000));
        const uint8_t d[8] = {0, 0, 0, 0x40, 0, 0, 0, 0};
        dashAsOnTorque2(d, 1200);
        CHECK(dashAsSensor.lastConfirm == AS_CONFIRM_COMPLETE);
        CHECK(dashAsGateState.pendingTargetDi == 0);
    }

    // --- 0x00E angle decode + steering trigger (simulated freeze) ---
    {
        ProfileGuard g;
        dashAsProfile.angleScaleT10 = 10;   // 1.0 deg/LSB
        dashAsProfile.angleOffsetT10 = 0;
        const uint8_t level[8] = {0x00, 0x00, 0, 0, 0, 0, 0, 0};
        const uint8_t p25[8] = {0x00, 0x19, 0, 0, 0, 0, 0, 0};   // +25.0
        const uint8_t p35[8] = {0x00, 0x23, 0, 0, 0, 0, 0, 0};   // +35.0
        const uint8_t n35[8] = {0xFF, 0xDD, 0, 0, 0, 0, 0, 0};   // -35.0
        const uint8_t back[8] = {0x00, 0x0A, 0, 0, 0, 0, 0, 0};  // +10.0
        // park (gear 1), steering crossing 30 deg
        const uint8_t park[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(park, 1000);
        dashAsOnAngle00E(level, 1100);
        DashAsTrigger t = dashAsPollTrigger(1200);
        CHECK(t.type == kAsTrigNone);
        dashAsOnAngle00E(p25, 1300);         // below threshold
        t = dashAsPollTrigger(1400);
        CHECK(t.type == kAsTrigNone);
        dashAsOnAngle00E(p35, 1500);         // crosses +30
        t = dashAsPollTrigger(1600);
        CHECK(t.type == kAsTrigSteer);
        CHECK(t.dir > 0);
        CHECK(t.gear == kAsGearD);           // P + steer+ -> D
        t = dashAsPollTrigger(1700);         // one-shot
        CHECK(t.type == kAsTrigNone);
        dashAsOnAngle00E(back, 1800);        // re-arm zone
        (void)dashAsPollTrigger(1900);
        dashAsOnAngle00E(n35, 2000);         // crosses -30
        t = dashAsPollTrigger(2100);
        CHECK(t.type == kAsTrigSteer);
        CHECK(t.dir < 0);
        CHECK(t.gear == kAsGearR);           // P + steer- -> R
    }
    // --- steering in D/R toggles at the higher threshold ---
    {
        ProfileGuard g;
        dashAsProfile.angleScaleT10 = 10;
        const uint8_t drive[8] = {0, 0, 0, 0x40, 0, 0, 0, 0};   // gear D
        const uint8_t p35[8] = {0x00, 0x23, 0, 0, 0, 0, 0, 0};
        const uint8_t p65[8] = {0x00, 0x41, 0, 0, 0, 0, 0, 0};
        const uint8_t back[8] = {0x00, 0x0A, 0, 0, 0, 0, 0, 0};
        dashAsOnTorque2(drive, 1000);
        dashAsOnAngle00E(p35, 1100);         // 35 < 60 in D: nothing
        DashAsTrigger t = dashAsPollTrigger(1200);
        CHECK(t.type == kAsTrigNone);
        dashAsOnAngle00E(p65, 1300);         // 65 >= 60
        t = dashAsPollTrigger(1400);
        CHECK(t.type == kAsTrigSteer);
        CHECK(t.gear == kAsGearR);           // D -> R toggle
        dashAsOnAngle00E(back, 1500);
        (void)dashAsPollTrigger(1600);
        // now in R (feed gear 2), +65 again -> back to D
        const uint8_t rev[8] = {0, 0, 0, 0x20, 0, 0, 0, 0};
        dashAsOnTorque2(rev, 1700);
        dashAsOnAngle00E(p65, 1800);
        t = dashAsPollTrigger(1900);
        CHECK(t.type == kAsTrigSteer);
        CHECK(t.gear == kAsGearD);
    }
    // --- angle map unfrozen: no steering trigger ever ---
    {
        ProfileGuard g;  // angleScaleT10 stays 0
        const uint8_t park[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        const uint8_t big[8] = {0x01, 0x00, 0, 0, 0, 0, 0, 0};
        dashAsOnTorque2(park, 1000);
        dashAsOnAngle00E(big, 1100);
        CHECK(dashAsSensor.angleDegT10 == 0x7FFF);  // invalid
        CHECK(dashAsPollTrigger(1200).type == kAsTrigNone);
    }

    // --- scroll episode: 3 motion frames = exactly ONE trigger ---
    {
        ProfileGuard g;
        dashAsProfile.w45SideByte = 3;
        dashAsProfile.w45SideL = 0x10;   // left
        dashAsProfile.w45SideR = 0x20;   // right
        dashAsProfile.w45DirByte = 4;
        dashAsProfile.w45DirPos = 0x40;
        dashAsProfile.w45DirNeg = 0xC0;
        const uint8_t park[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(park, 1000);
        const uint8_t lpos[8] = {0, 0, 0, 0x10, 0x40, 0, 0, 0};
        dashAsOnWheel045(lpos, 2000);    // episode start
        dashAsOnWheel045(lpos, 2010);
        dashAsOnWheel045(lpos, 2020);
        DashAsTrigger t = dashAsPollTrigger(2030);
        CHECK(t.type == kAsTrigScroll);
        CHECK(t.side == 0);              // left
        CHECK(t.dir > 0);
        CHECK(t.gear == kAsGearD);       // left+ = D
        CHECK(dashAsPollTrigger(2040).type == kAsTrigNone);  // consumed
        // still inside the episode window: no new trigger
        dashAsOnWheel045(lpos, 2030);
        CHECK(dashAsPollTrigger(2040).type == kAsTrigNone);
        // after the episode gap: a new gesture fires again
        dashAsOnWheel045(lpos, 2000 + dashAsProfile.scrollEpisodeGapMs + 100);
        t = dashAsPollTrigger(2300);
        CHECK(t.type == kAsTrigScroll);
        // left- = R
        const uint8_t lneg[8] = {0, 0, 0, 0x10, 0xC0, 0, 0, 0};
        dashAsOnWheel045(lneg, 3000);
        t = dashAsPollTrigger(3010);
        CHECK(t.type == kAsTrigScroll);
        CHECK(t.dir < 0);
        CHECK(t.gear == kAsGearR);
        // right+ = P button, right- = D/R toggle (in P -> D)
        const uint8_t rpos[8] = {0, 0, 0, 0x20, 0x40, 0, 0, 0};
        dashAsOnWheel045(rpos, 4000);
        t = dashAsPollTrigger(4010);
        CHECK(t.gear == kAsGearPBtn);
        const uint8_t rneg[8] = {0, 0, 0, 0x20, 0xC0, 0, 0, 0};
        dashAsOnWheel045(rneg, 5000);
        t = dashAsPollTrigger(5010);
        CHECK(t.gear == kAsGearD);       // toggle from P -> D
    }
    // --- wheel map unfrozen: scrolls never trigger ---
    {
        ProfileGuard g;  // w45* stay 0xFF sentinel
        const uint8_t lpos[8] = {0, 0, 0, 0x10, 0x40, 0, 0, 0};
        dashAsOnWheel045(lpos, 1000);
        CHECK(dashAsPollTrigger(1100).type == kAsTrigNone);
    }

    // --- obstacle: fresh AND clear, fail-closed when absent/blocked ---
    {
        ProfileGuard g;
        dashAsProfile.oocBlockByte = 0;
        dashAsProfile.oocBlockMask = 0x01;
        dashAsProfile.sdiBlockByte = 0;
        dashAsProfile.sdiBlockMask = 0x01;
        const uint8_t clear8[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        const uint8_t blocked[8] = {1, 0, 0, 0, 0, 0, 0, 0};
        // nothing seen yet: NOT clear (absent telemetry is not clear)
        CHECK(!dashAsObstacleFreshAndClear(1000));
        dashAsOnObstacle(0x23E, clear8, 1000);   // ooc clear
        CHECK(!dashAsObstacleFreshAndClear(1000));  // sdi still absent
        dashAsOnObstacle(0x20E, clear8, 1010);   // sdi clear
        CHECK(dashAsObstacleFreshAndClear(1020));
        // blocked bit set on either frame -> blocked (keep ooc fresh so
        // the block, not staleness, is what fails the check)
        dashAsOnObstacle(0x23E, clear8, 1995);
        dashAsOnObstacle(0x20E, blocked, 2000);
        CHECK(!dashAsObstacleFreshAndClear(2010));
        dashAsOnObstacle(0x23E, clear8, 2995);
        dashAsOnObstacle(0x20E, clear8, 3000);
        CHECK(dashAsObstacleFreshAndClear(3010));
        dashAsOnObstacle(0x20E, clear8, 3095);
        dashAsOnObstacle(0x23E, blocked, 3100);  // ooc blocked
        CHECK(!dashAsObstacleFreshAndClear(3110));
        // both fresh stamps go stale
        CHECK(!dashAsObstacleFreshAndClear(
            3100 + dashAsProfile.obstacleFreshMs + 1));
        // unrelated id ignored: 0x22E (CanB-only sdiRear) fed CLEAR data
        // must not clear the blocked state recorded at 3100
        dashAsOnObstacle(0x22E, clear8, 3200);
        CHECK(!dashAsObstacleFreshAndClear(3210));
    }

    // ================= Task 6: permit + 0x228 snapshot =================
    {
        ProfileGuard g;
        dashAsPermit = DashAsPermit{};
        // master off: arm denied outright
        CHECK(dashAsArmRequest(1000) == kAsArmDeniedMaster);
        dashAsSetMaster(true);
        CHECK(dashAsPermit.master);
        // two-step arm: open the window, then confirm inside it
        CHECK(dashAsArmRequest(2000) == kAsArmOpened);
        CHECK(!dashAsPermit.armed);
        CHECK(dashAsArmRequest(2000 + kAsArmWindowMs) == kAsArmEnabled);
        CHECK(dashAsPermit.armed);
        CHECK(dashAsArmRequest(3000) == kAsArmAlreadyArmed);
        // disarm clears armed AND any in-flight request
        CHECK(dashAsSubmit(4, 3000));
        dashAsDisarm();
        CHECK(!dashAsPermit.armed);
        CHECK(dashAsGateState.pendingTargetDi == 0);
        // window expiry clears the pending arm; next call re-opens
        dashAsPermit = DashAsPermit{};
        dashAsSetMaster(true);
        CHECK(dashAsArmRequest(1000) == kAsArmOpened);
        CHECK(dashAsArmRequest(1000 + kAsArmWindowMs + 1) == kAsArmExpired);
        CHECK(dashAsPermit.armPendingSinceMs == 0);
        CHECK(dashAsArmRequest(2000) == kAsArmOpened);
        CHECK(dashAsArmRequest(2100) == kAsArmEnabled);
        // master-off cascade disarms
        dashAsSetMaster(false);
        CHECK(!dashAsPermit.master);
        CHECK(!dashAsPermit.armed);
    }
    // --- 0x228 source snapshot feeds the composer ---
    {
        ProfileGuard g;
        // freeze the D pair so the sentinel guard admits composition
        dashAsProfile.stalkB0[kAsGearD] = 0x40;
        dashAsProfile.stalkB1[kAsGearD] = 0xC0;
        const uint8_t idle[4] = {0x40, 0xC0, 0x30, 0x00};
        CHECK(!dashAsSbwSource.seen);
        dashAsOnSbw0228(idle, 1000);
        CHECK(dashAsSbwSource.seen);
        CHECK(dashAsSbwSource.data[0] == 0x40);
        uint8_t out[4];
        // age is the parameter, not "now": fed at t=1000, ask at t=1100
        CHECK(dashAsSbwCompose(dashAsSbwSource.data, 100, kAsGearD, out));
        CHECK(((out[2] >> 4) & 0x0F) == 0x04);
        CHECK(out[3] == dashAsCrc8J1850(out, 3));
        CHECK(!dashAsSbwCompose(dashAsSbwSource.data,
                                1100 + dashAsProfile.sbwSrcFreshMs + 1,
                                kAsGearD, out));
        CHECK(!dashAsSbwCompose(dashAsSbwSource.data, 1100, 0xFE, out));
        // echo suppression: our own TX echo never refreshes the snapshot
        const uint8_t other[4] = {0x50, 0xC0, 0x30, 0x00};
        dashAsLastSbwTxMs = 1000;
        dashAsOnSbw0228(other, 1010);  // 10ms after our TX — suppressed
        CHECK(dashAsSbwSource.data[0] == 0x40);
        dashAsOnSbw0228(other, 1030);  // exactly the window edge — suppressed
        CHECK(dashAsSbwSource.data[0] == 0x40);
        dashAsOnSbw0228(other, 1031);  // past the window — accepted
        CHECK(dashAsSbwSource.data[0] == 0x50);
        CHECK(dashAsSbwSource.ms == 1031);
        dashAsLastSbwTxMs = 0;
    }

    // --- runtime bookkeeping rides the confirm path ---
    {
        ProfileGuard g;
        dashAsGateReset();
        dashAsStats = DashAsStats{};
        dashAsRuntime = DashAsRuntime{};
        CHECK(dashAsSubmit(2, 5000));                               // request R
        CHECK(dashAsConfirmGear(2, 5100) == AS_CONFIRM_COMPLETE);
        CHECK(dashAsStats.confirmed == 1);
        CHECK(dashAsRuntime.lastShiftMs == 5100);
        CHECK(dashAsRuntime.shiftCount == 1);
        CHECK(dashAsSubmit(4, 6000));                               // request D
        CHECK(dashAsConfirmGear(1, 7001) == AS_CONFIRM_RETRY);      // expiry 1
        CHECK(dashAsStats.retried == 1);
        CHECK(dashAsSubmit(4, 8000));
        CHECK(dashAsConfirmGear(1, 9001) == AS_CONFIRM_ABANDONED);  // expiry 2
        CHECK(dashAsStats.abandoned == 1);
        CHECK(dashAsLastAbandonReason() == kAsAbandonRetryExhausted);
        // gear -> DI mapping for the submit path (P button requests P)
        CHECK(dashAsGearToDi(kAsGearP) == 1 && dashAsGearToDi(kAsGearPBtn) == 1);
        CHECK(dashAsGearToDi(kAsGearR) == 2 && dashAsGearToDi(kAsGearN) == 3);
        CHECK(dashAsGearToDi(kAsGearD) == 4);
        CHECK(dashAsGearToDi(kAsGearCount) == 0xFF);
    }

    // --- final review: arm/disarm invalidates latched triggers ---
    {
        ProfileGuard g;
        dashAsPermit = DashAsPermit{};
        dashAsSetMaster(true);
        dashAsProfile.w45SideByte = 3;
        dashAsProfile.w45SideL = 0x10;   // left
        dashAsProfile.w45SideR = 0x20;   // right
        dashAsProfile.w45DirByte = 4;
        dashAsProfile.w45DirPos = 0x40;
        dashAsProfile.w45DirNeg = 0xC0;
        // gear P so the steering path gets a park threshold (gearNow==0
        // maps to threshold 0 and steering never fires)
        const uint8_t t2p[8] = {0, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnTorque2(t2p, 500);
        const uint8_t lpos[8] = {0, 0, 0, 0x10, 0x40, 0, 0, 0};
        dashAsOnWheel045(lpos, 1000);    // gesture while DISARMED
        CHECK(dashAsSensor.pending.type == kAsTrigScroll);
        CHECK(dashAsArmRequest(2000) == kAsArmOpened);
        CHECK(dashAsArmRequest(2100) == kAsArmEnabled);
        // stale gesture must NOT fire on the first armed poll
        CHECK(dashAsPollTrigger(2200).type == kAsTrigNone);
        // disarm also invalidates a fresh gesture
        dashAsOnWheel045(lpos, 2000 + dashAsProfile.scrollEpisodeGapMs + 100);
        CHECK(dashAsSensor.pending.type == kAsTrigScroll);
        dashAsDisarm();
        CHECK(dashAsPollTrigger(2300).type == kAsTrigNone);
        // steering: past-threshold angle at arm time must not fire; the
        // one-shot re-arms only after returning inside threshold-margin
        dashAsProfile.angleScaleT10 = 10;   // 1.0 deg/LSB
        dashAsProfile.angleOffsetT10 = 0;
        const uint8_t ang40[8] = {0x00, 40, 0, 0x10, 0, 0, 0, 0};
        dashAsOnAngle00E(ang40, 3000);      // +40 deg, past the 30 deg gate
        CHECK(dashAsArmRequest(3100) == kAsArmOpened);
        CHECK(dashAsArmRequest(3200) == kAsArmEnabled);
        CHECK(dashAsPollTrigger(3300).type == kAsTrigNone);
        const uint8_t ang0[8] = {0x00, 0, 0, 0x10, 0, 0, 0, 0};
        dashAsOnAngle00E(ang0, 3400);       // return inside margin
        CHECK(dashAsPollTrigger(3500).type == kAsTrigNone);
        dashAsOnAngle00E(ang40, 3600);      // fresh crossing
        CHECK(dashAsPollTrigger(3700).type == kAsTrigSteer);
    }

    // --- final review: composer refuses sentinel stalk bytes ---
    {
        ProfileGuard g;
        dashAsProfile.stalkB0[kAsGearD] = 0xFF;  // partial freeze: no table
        dashAsProfile.stalkB1[kAsGearD] = 0xFF;
        const uint8_t src[4] = {0x40, 0xC0, 0x30, 0x00};
        uint8_t out[4] = {0, 0, 0, 0};
        CHECK(dashAsSbwCompose(src, 0, kAsGearD, out) == false);
        dashAsProfile.stalkB0[kAsGearD] = 0x50;  // frozen pair composes
        dashAsProfile.stalkB1[kAsGearD] = 0xC0;
        CHECK(dashAsSbwCompose(src, 0, kAsGearD, out) == true);
        CHECK(out[0] == 0x50);
    }

    // --- final review: speed scale carries hundredths of kph per LSB ---
    {
        ProfileGuard g;
        dashAsProfile.t2SpeedByte = 6;
        dashAsProfile.t2SpeedScaleH = 7;         // 0.07 kph/bit
        const uint8_t d41[8] = {0, 0, 0, 0x40, 0, 0, 41, 0};
        dashAsOnTorque2(d41, 1000);
        CHECK(dashAsSensor.speedKph == 3);       // 2.87 rounds to 3
        const uint8_t d255[8] = {0, 0, 0, 0x40, 0, 0, 255, 0};
        dashAsOnTorque2(d255, 1100);
        CHECK(dashAsSensor.speedKph == 18);      // 17.85 rounds to 18
        dashAsProfile.t2SpeedScaleH = 150;       // 1.5 kph/bit saturates
        dashAsOnTorque2(d255, 1200);
        CHECK(dashAsSensor.speedKph == 255);
    }

    if (g_fail == 0) {
        std::printf("native autoshift: ALL PASS\n");
    }
    return g_fail == 0 ? 0 : 1;
}
