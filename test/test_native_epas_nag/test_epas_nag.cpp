#include <unity.h>
#include "dash_epas_nag.h"
#include "dash_epas_nag_trace.h"

// Build a 0x370 EPAS frame (8 bytes). handsOnLevel -> data[4] bits[7:6]; counter -> data[6] low nibble.
static void buildEpasFrame(uint8_t data[8], uint8_t handsOnLevel, uint8_t counter)
{
    for (int i = 0; i < 8; i++)
        data[i] = 0;
    data[4] = static_cast<uint8_t>((handsOnLevel & 0x03) << 6);
    data[6] = static_cast<uint8_t>(counter & 0x0F);
}

static DashEpasNagEngine makeArmedEngine()
{
    DashEpasNagEngine e;
    e.noteRuntime(true, true, true); // enabled + canActive + defense; dasHandsOnState stays 0xFF (unseen -> D5 inject)
    return e;
}

// Mode-C: engine armed + das==2 (mild) + delay window already burned, so the
// NEXT tryBuildEcho at nowMs>=3001 injects. Used by frame-construction tests
// (checksum/counter/count/diag) that need a guaranteed inject on one call.
static DashEpasNagEngine makeDemandArmedEngine()
{
    DashEpasNagEngine e;
    e.noteRuntime(true, true, true);
    e.noteApState(true, 1000);
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(2 << 2);
    e.onDasGate(das);
    uint8_t in[8]; buildEpasFrame(in, 0, 1); uint8_t out[8];
    e.tryBuildEcho(in, 8, 1100, out); // arm s2EnterMs=1100, returns false (in delay)
    return e;
}

// ── constants ────────────────────────────────────────────────────────
void test_nag_constants()
{
    TEST_ASSERT_EQUAL_HEX32(880, DashEpasNagEngine::kCanId);
    TEST_ASSERT_EQUAL_HEX32(921, DashEpasNagEngine::kDasIdLegacy);
    TEST_ASSERT_EQUAL_HEX32(923, DashEpasNagEngine::kDasIdHighland);
    TEST_ASSERT_EQUAL_UINT16(2050, DashEpasNagEngine::kTorqueCenter);
    TEST_ASSERT_EQUAL_UINT16(1780, DashEpasNagEngine::kTorqueMin);
    TEST_ASSERT_EQUAL_UINT16(2500, DashEpasNagEngine::kTorqueMax);
    TEST_ASSERT_EQUAL_UINT32(1500, DashEpasNagEngine::kBlackoutMs);
}

// ── Mode-C: handsOnLevel derived from torque magnitude (spec §7.1) ───
void test_hands_level_from_raw()
{
    TEST_ASSERT_EQUAL_UINT8(0, DashEpasNagEngine::handsLevelFromRaw(2050)); // 0 Nm
    TEST_ASSERT_EQUAL_UINT8(0, DashEpasNagEngine::handsLevelFromRaw(2149)); // <1.0 Nm
    TEST_ASSERT_EQUAL_UINT8(1, DashEpasNagEngine::handsLevelFromRaw(2150)); // =1.0 Nm
    TEST_ASSERT_EQUAL_UINT8(1, DashEpasNagEngine::handsLevelFromRaw(2249)); // <2.0 Nm
    TEST_ASSERT_EQUAL_UINT8(2, DashEpasNagEngine::handsLevelFromRaw(2250)); // =2.0 Nm
    TEST_ASSERT_EQUAL_UINT8(2, DashEpasNagEngine::handsLevelFromRaw(2260)); // strong peak
    TEST_ASSERT_EQUAL_UINT8(1, DashEpasNagEngine::handsLevelFromRaw(1950)); // -1.0 Nm (dir=-1)
    TEST_ASSERT_EQUAL_UINT8(2, DashEpasNagEngine::handsLevelFromRaw(1850)); // -2.0 Nm (dir=-1)
    TEST_ASSERT_EQUAL_UINT8(2, DashEpasNagEngine::handsLevelFromRaw(1840)); // strong negative peak
}

void test_clamp_int16()
{
    TEST_ASSERT_EQUAL_INT16(-10, DashEpasNagEngine::clampInt16(-25, -10, 10));
    TEST_ASSERT_EQUAL_INT16(10, DashEpasNagEngine::clampInt16(25, -10, 10));
    TEST_ASSERT_EQUAL_INT16(3, DashEpasNagEngine::clampInt16(3, -10, 10));
}

// ── Mode-C: 0x129 steering direction + diag fields ──────────────────
void test_on_steering_angle_stores_and_defaults()
{
    DashEpasNagEngine e;
    TEST_ASSERT_EQUAL_UINT8(0xFF, e.diag.lastDemandState);
    TEST_ASSERT_EQUAL_UINT16(0, e.diag.lastDemandTorque);
    TEST_ASSERT_EQUAL_UINT8(0, e.diag.lastHandsLevel);
    TEST_ASSERT_EQUAL_STRING("idle", e.diag.lastDemandGate);

    // default 0 -> dir +1 path (verified in Task 4/5); setting positive flips dir
    e.onSteeringAngle(0);
    TEST_ASSERT_EQUAL_INT16(0, e.steerAngleX10);
    e.onSteeringAngle(500); // right turn -> X10 positive
    TEST_ASSERT_EQUAL_INT16(500, e.steerAngleX10);
    e.onSteeringAngle(-300);
    TEST_ASSERT_EQUAL_INT16(-300, e.steerAngleX10);
}

// ── Mode-C: nextDemandTorque demand-state machine (spec §6.3) ───────
// Helper: arm engine with given das state, AP on, no steering (dir +1).
static DashEpasNagEngine makeDemandEngine(uint8_t dasState)
{
    DashEpasNagEngine e;
    e.noteRuntime(true, true, true);
    e.noteApState(true, 1000); // apActive=true
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>((dasState & 0x0F) << 2);
    e.onDasGate(das);
    return e;
}

void test_demand_rejects_excluded_das_states()
{
    const uint8_t excluded[] = {0u, 8u, 15u};
    for (uint8_t d : excluded)
    {
        DashEpasNagEngine e = makeDemandEngine(d);
        uint16_t t = 9999; uint8_t lvl = 9;
        TEST_ASSERT_FALSE(e.nextDemandTorque(d, 5000, t, lvl));
    }
}
// Regression (real-car 2026-06-19): apActive mirrors isDASAutopilotActive(), which
// reads 0x399 byte0 low-nibble and only accepts {3,4,5}. Cars whose byte0 AP-status
// code is 6 (e.g. Jordan's 2026.14.x — 22/22 frames byte0=6) read apActive=false
// FOREVER, even though AP is engaged and the DAS hands-on level (byte5 bits[5:2])
// is actively requesting (das=1/2/3/4/5). Gating the demand machine on apActive made
// nextDemandTorque return demand_idle on the real car -> echo=0 -> nag never
// suppressed -> escalated to red "take over immediately". The demand machine must be
// driven by the das level ALONE (das==0/8/15 still excluded); apActive is NOT a
// reliable AP signal here. was: test_demand_ap_inactive_blocks (locked the bug).
void test_demand_injects_when_apactive_false_but_das_strong()
{
    DashEpasNagEngine e = makeDemandEngine(3); // das=3 strong; helper arms apActive=true
    e.noteApState(false, 1000);                // simulate byte0=6 -> apActive=false (AP still engaged)
    uint16_t t; uint8_t lvl;
    e.nextDemandTorque(3, 1100, t, lvl);        // arm strongEnterMs=1100 (within 1000ms delay)
    // past strong delay -> must inject (strong hold peak 2260), NOT demand_idle
    TEST_ASSERT_TRUE(e.nextDemandTorque(3, 2700, t, lvl)); // active=600 -> phase=600 -> mag=210
    TEST_ASSERT_EQUAL_UINT16(2260, t);
    TEST_ASSERT_EQUAL_UINT8(2, lvl);
}
// Same regression at das==1 — the level Jordan's car actually broadcasts (byte5=0xC4
// for all 22 frames). Proves even the lightest nag request injects without apActive.
void test_demand_injects_s1_when_apactive_false_but_das_light()
{
    DashEpasNagEngine e = makeDemandEngine(1);
    e.noteApState(false, 1000); // byte0=6 car: apActive=false, but das=1 requests hands-on
    uint16_t t; uint8_t lvl;
    TEST_ASSERT_TRUE(e.nextDemandTorque(1, 1100, t, lvl)); // s1 hold window -> inject 2050
    TEST_ASSERT_EQUAL_UINT16(2050, t);
}
void test_demand_state1_holds_then_stops()
{
    DashEpasNagEngine e = makeDemandEngine(1);
    uint16_t t; uint8_t lvl;
    // first call at 1100: arms s1EnterMs, within 500ms -> holds lastRaw (2050 init)
    TEST_ASSERT_TRUE(e.nextDemandTorque(1, 1100, t, lvl));
    TEST_ASSERT_EQUAL_UINT16(2050, t);
    // after 500ms window -> stops
    TEST_ASSERT_FALSE(e.nextDemandTorque(1, 1700, t, lvl));
}
void test_demand_state2_delays_then_mild_walks()
{
    DashEpasNagEngine e = makeDemandEngine(2);
    uint16_t t; uint8_t lvl;
    // within 2000ms delay -> no inject
    TEST_ASSERT_FALSE(e.nextDemandTorque(2, 1100, t, lvl));
    // still in delay at 2000 (3000-1000=1900 < 2000)
    TEST_ASSERT_FALSE(e.nextDemandTorque(2, 3000, t, lvl));
    // past delay -> mild walk in positive band [2100,2250] (dir +1, no steer)
    TEST_ASSERT_TRUE(e.nextDemandTorque(2, 3100, t, lvl));
    TEST_ASSERT_TRUE(t >= 2100 && t <= 2250);
    TEST_ASSERT_TRUE(lvl == 1 || lvl == 2); // 0.5..2.0 Nm
}
void test_demand_state2_negative_band_when_steer_right()
{
    DashEpasNagEngine e = makeDemandEngine(2);
    e.onSteeringAngle(500); // right turn -> dir -1
    uint16_t t; uint8_t lvl;
    TEST_ASSERT_FALSE(e.nextDemandTorque(2, 1100, t, lvl)); // delay
    TEST_ASSERT_TRUE(e.nextDemandTorque(2, 3100, t, lvl));
    TEST_ASSERT_TRUE(t >= 1850 && t <= 2000); // negative band
    TEST_ASSERT_TRUE(lvl == 1 || lvl == 2);
}

// Mode-C: das==2 level-2 hold gate. Once a walk arms a level-2 hold
// (s2HoldUntilMs/s2HoldRaw/s2HoldLevel), subsequent frames within the hold
// window emit the held value at gate "s2_hold" (spec §6.3 s2 hold bookkeeping).
void test_demand_state2_level2_hold_emits_held_value()
{
    DashEpasNagEngine e = makeDemandEngine(2);
    uint16_t t; uint8_t lvl;
    e.nextDemandTorque(2, 1100, t, lvl);   // arm s2EnterMs (in delay)
    e.nextDemandTorque(2, 3100, t, lvl);   // enter walk phase (past 2000ms delay)
    // Simulate a level-2 walk having armed the hold (public struct members):
    e.s2HoldUntilMs = 5000;
    e.s2HoldRaw = 2250;     // positive-band level-2 peak
    e.s2HoldLevel = 2;
    // within hold window -> held value, gate s2_hold
    TEST_ASSERT_TRUE(e.nextDemandTorque(2, 4000, t, lvl));
    TEST_ASSERT_EQUAL_UINT16(2250, t);
    TEST_ASSERT_EQUAL_UINT8(2, lvl);
    // past hold window -> back to mild walk (not the held value)
    bool ok = e.nextDemandTorque(2, 5100, t, lvl);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(t >= 2100 && t <= 2250);   // mild band again
}

void test_demand_state3_delays_then_ramps_positive()
{
    DashEpasNagEngine e = makeDemandEngine(3);
    uint16_t t; uint8_t lvl;
    TEST_ASSERT_FALSE(e.nextDemandTorque(3, 1100, t, lvl));   // within 1000ms delay
    TEST_ASSERT_FALSE(e.nextDemandTorque(3, 2000, t, lvl));   // 2000-1100=900 <1000, still delay
    // just past delay: mag should be small (ramp start)
    bool ok = e.nextDemandTorque(3, 2100, t, lvl);            // active=2100-1100-1000=0 -> mag=0
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(t >= 2050 && t <= 2092);                  // 2050+0..42
}
void test_demand_state3_ramp_peaks_at_2260()
{
    DashEpasNagEngine e = makeDemandEngine(3);
    uint16_t t; uint8_t lvl;
    e.nextDemandTorque(3, 1100, t, lvl);                       // arm strongEnterMs=1100 (delay)
    // active = now-1100-1000; phase in [500,1500) -> mag=210 hold
    TEST_ASSERT_TRUE(e.nextDemandTorque(3, 2700, t, lvl));     // active=600, phase=600 -> mag=210
    TEST_ASSERT_EQUAL_UINT16(2260, t);                         // 2050+210
    TEST_ASSERT_EQUAL_UINT8(2, lvl);
}
// Mode-C: strong ramp is LINEAR between delay-end (mag 0) and hold (mag 210).
// Pins the (phase*210/500) formula — boundary tests at phase=0 and phase>=500
// alone cannot detect a malformed divisor/multiplier.
void test_demand_state3_ramp_midpoint_linear()
{
    DashEpasNagEngine e = makeDemandEngine(3);
    uint16_t t; uint8_t lvl;
    e.nextDemandTorque(3, 1100, t, lvl);    // arm strongEnterMs=1100 (delay)
    // active=2350-1100-1000=250, phase=250 -> mag=105 -> torq=2155, level 1
    TEST_ASSERT_TRUE(e.nextDemandTorque(3, 2350, t, lvl));
    TEST_ASSERT_EQUAL_UINT16(2155, t);
    TEST_ASSERT_EQUAL_UINT8(1, lvl);
}
void test_demand_state3_negative_when_steer_right()
{
    DashEpasNagEngine e = makeDemandEngine(3);
    e.onSteeringAngle(500);
    uint16_t t; uint8_t lvl;
    e.nextDemandTorque(3, 1100, t, lvl);
    TEST_ASSERT_TRUE(e.nextDemandTorque(3, 2700, t, lvl));
    TEST_ASSERT_EQUAL_UINT16(1840, t);                         // 2050-210
    TEST_ASSERT_EQUAL_UINT8(2, lvl);
}
void test_demand_unseen_0xff_falls_into_strong()
{
    // makeArmedEngine leaves dasHandsOnState=0xFF (unseen). D5: treat as strong.
    DashEpasNagEngine e = makeArmedEngine();
    e.noteApState(true, 1000);
    uint16_t t; uint8_t lvl;
    e.nextDemandTorque(0xFF, 1100, t, lvl);                    // prevDas 0 -> 0xFF, arms strongEnterMs=1100
    TEST_ASSERT_TRUE(e.nextDemandTorque(0xFF, 2700, t, lvl));  // peak
    TEST_ASSERT_EQUAL_UINT16(2260, t);
}

// ── checksum (additive, NOT crc) ─────────────────────────────────────
void test_checksum_additive_formula()
{
    uint8_t d[7];
    for (uint16_t v = 0; v < 256; v += 37)
    {
        for (int i = 0; i < 7; i++)
            d[i] = static_cast<uint8_t>(v + i * 7);
        uint16_t expected = 0x70u + 0x03u;
        for (int i = 0; i < 7; i++)
            expected += d[i];
        TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(expected & 0xFF), DashEpasNagEngine::teslaChecksum0x370(d));
    }
}

void test_checksum_counter_additive()
{
    uint8_t d[7] = {0x10, 0x20, 0x08, 0x02, 0x40, 0x00, 0x05};
    uint8_t cs0 = DashEpasNagEngine::teslaChecksum0x370(d);
    d[6] = 0x06;
    uint8_t cs1 = DashEpasNagEngine::teslaChecksum0x370(d);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((cs0 + 1) & 0xFF), cs1);
}

// ── gate G1-G6 truth table ───────────────────────────────────────────
void test_gate_disabled_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    e.noteRuntime(false, true, true);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_canactive_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    e.noteRuntime(true, false, true);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_defense_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    e.noteRuntime(true, true, false);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_real_hand_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    TEST_ASSERT_FALSE(e.shouldInject(1, 1000));
}
void test_gate_das_satisfied_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(0 << 2);
    e.onDasGate(das);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_das_suspended_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(8 << 2);
    e.onDasGate(das);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_das_15_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(15 << 2);
    e.onDasGate(das);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_das_unseen_injects_d5()
{
    DashEpasNagEngine e = makeArmedEngine();
    TEST_ASSERT_TRUE(e.shouldInject(0, 1000));
}
void test_gate_das_escalating_injects()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(3 << 2);
    e.onDasGate(das);
    TEST_ASSERT_TRUE(e.shouldInject(0, 1000));
}
void test_gate_blackout_blocks_then_clears()
{
    DashEpasNagEngine e = makeArmedEngine();
    e.noteApState(true, 1000);
    e.noteApState(false, 2000);
    TEST_ASSERT_FALSE(e.shouldInject(0, 2500));
    TEST_ASSERT_TRUE(e.shouldInject(0, 2000 + 1500 + 1));
}

void test_gate_blackout_handles_zero_timestamp_edge()
{
    DashEpasNagEngine e = makeArmedEngine();
    e.noteApState(true, 0);
    e.noteApState(false, 0);
    TEST_ASSERT_FALSE(e.shouldInject(0, 100));
    TEST_ASSERT_TRUE(e.shouldInject(0, DashEpasNagEngine::kBlackoutMs + 1));
}

// ── echo builder ─────────────────────────────────────────────────────
void test_echo_rejects_invalid_dlc()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 5);
    uint8_t out[8] = {0};
    TEST_ASSERT_FALSE(e.tryBuildEcho(in, 7, 1000, out));
    TEST_ASSERT_EQUAL_UINT32(0, e.diag.echoCount);
}

void test_echo_gated_returns_false()
{
    DashEpasNagEngine e;
    e.noteRuntime(false, true, true);
    uint8_t in[8];
    buildEpasFrame(in, 0, 5);
    uint8_t out[8] = {0};
    TEST_ASSERT_FALSE(e.tryBuildEcho(in, 8, 1000, out));
    TEST_ASSERT_EQUAL_UINT32(0, e.diag.echoCount);
}
// Mode-C (replaces B2): handsOnLevel is DERIVED from torque magnitude, not
// forced to 1. Strong peak (2.1Nm) -> level 2; mild midband (>=1Nm) -> level 1.
void test_echo_level_derived_from_torque()
{
    // strong peak path: das==0xFF, AP on, advance past delay -> 2260 -> level 2
    DashEpasNagEngine e;
    e.noteRuntime(true, true, true);
    e.noteApState(true, 1000);
    uint8_t in[8]; buildEpasFrame(in, 0, 5);
    uint8_t out[8];
    e.tryBuildEcho(in, 8, 1100, out);            // arm strongEnterMs (das 0xFF unseen)
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 2700, out));   // peak hold
    TEST_ASSERT_EQUAL_UINT8(2, (out[4] >> 6) & 0x03);     // level 2 written to bits[7:6]
    TEST_ASSERT_EQUAL_UINT8(2, e.diag.lastHandsLevel);
}
// Mode-C: tryBuildEcho's reject path. When the demand machine says "don't
// inject" (das==2 still in its 2000ms delay), tryBuildEcho must return false,
// NOT increment echoCount, increment suppressCount, and propagate the demand
// gate string. (Task 6 integration — the production path to the vehicle.)
void test_echo_demand_reject_propagates_gate()
{
    DashEpasNagEngine e = makeDemandEngine(2);
    uint8_t in[8]; buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    // das==2 within its 2000ms delay -> nextDemandTorque returns false
    TEST_ASSERT_FALSE(e.tryBuildEcho(in, 8, 1100, out));
    TEST_ASSERT_EQUAL_UINT32(0, e.diag.echoCount);
    TEST_ASSERT_TRUE(e.diag.suppressCount >= 1);
    TEST_ASSERT_EQUAL_STRING("s2_delay", e.diag.lastGate);
}
void test_echo_counter_increments_and_wraps()
{
    for (uint8_t c = 0; c < 16; c++)
    {
        DashEpasNagEngine e = makeDemandArmedEngine();
        uint8_t in[8];
        buildEpasFrame(in, 0, c);
        in[6] = static_cast<uint8_t>(0xA0 | c);
        uint8_t out[8];
        TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 3100, out));
        uint8_t expected = static_cast<uint8_t>(0xA0 | ((c + 1) & 0x0F));
        TEST_ASSERT_EQUAL_HEX8(expected, out[6]);
    }
}
void test_echo_checksum_correct()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 9);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 3100, out));
    uint16_t expected = 0x70u + 0x03u;
    for (int i = 0; i < 7; i++)
        expected += out[i];
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(expected & 0xFF), out[7]);
}
void test_echo_torque_in_valid_range_data23()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 3100, out));
    uint16_t raw = static_cast<uint16_t>(((out[2] & 0x0F) << 8) | out[3]);
    TEST_ASSERT_TRUE(raw >= DashEpasNagEngine::kTorqueMin);
    TEST_ASSERT_TRUE(raw <= DashEpasNagEngine::kTorqueMax);
}
void test_echo_count_increments()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    e.tryBuildEcho(in, 8, 3100, out);
    e.tryBuildEcho(in, 8, 3110, out);
    e.tryBuildEcho(in, 8, 3120, out);
    TEST_ASSERT_EQUAL_UINT32(3, e.diag.echoCount);
}
void test_reset_clears_state()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    e.tryBuildEcho(in, 8, 3100, out);
    e.reset();
    TEST_ASSERT_EQUAL_UINT8(DashEpasNagEngine::kDasUnseen, e.dasHandsOnState);
    TEST_ASSERT_EQUAL_UINT32(0, e.apDisengageMs);
    TEST_ASSERT_EQUAL_UINT16(2050, (uint16_t)e.mildWalk);   // mildWalk reset to center
    TEST_ASSERT_EQUAL_UINT8(0, e.prevDas);                  // prevDas reset to 0 (not 0xFF)
    TEST_ASSERT_EQUAL_INT16(0, e.steerAngleX10);
}

// Mode-C: reset() must clear the demand-state machine fields so a stale
// session can't bleed torque/timer state into the next one.
void test_reset_clears_demand_machine()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8]; buildEpasFrame(in, 0, 1); uint8_t out[8];
    e.tryBuildEcho(in, 8, 3100, out);    // populate s2EnterMs/mildWalk/prevDas
    e.reset();
    TEST_ASSERT_EQUAL_UINT32(0, e.s1EnterMs);
    TEST_ASSERT_EQUAL_UINT32(0, e.s2EnterMs);
    TEST_ASSERT_EQUAL_UINT32(0, e.strongEnterMs);
    TEST_ASSERT_EQUAL_UINT8(0, e.prevDas);
    TEST_ASSERT_FALSE(e.apActive);
}

// ── diag snapshots (observability) ───────────────────────────────────
void test_diag_records_real_torque_and_counter()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8] = {0};
    in[2] = 0x0A; // torque high nibble
    in[3] = 0x34; // torque low byte -> raw 0xA34
    in[4] = 0x00; // handsOnLevel 0
    in[6] = 0x07; // counter 7
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 3100, out));
    TEST_ASSERT_EQUAL_UINT16(0x0A34, e.diag.lastRealTorque);
    TEST_ASSERT_EQUAL_UINT8(7, e.diag.lastRealCnt);
}
void test_diag_records_echo_torque()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 3100, out));
    uint16_t used = e.diag.lastEchoTorque;
    TEST_ASSERT_TRUE(used >= DashEpasNagEngine::kTorqueMin);
    TEST_ASSERT_TRUE(used <= DashEpasNagEngine::kTorqueMax);
}

// ── trace ring ───────────────────────────────────────────────────────
void test_trace_ring_push_and_read()
{
    EpasNagTraceRing r;
    EpasNagTraceEntry e{};
    e.ts_ms = 100;
    e.decision = 1;
    e.realTorque = 2050;
    r.push(e);
    e.ts_ms = 200;
    e.realTorque = 2060;
    r.push(e);
    TEST_ASSERT_EQUAL_UINT32(2, r.currentHead());

    EpasNagTraceEntry out[4] = {};
    int n = r.readSince(0, out, 4);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_UINT32(100, out[0].ts_ms);
    TEST_ASSERT_EQUAL_UINT16(2050, out[0].realTorque);
    TEST_ASSERT_EQUAL_UINT32(200, out[1].ts_ms);
}
void test_trace_ring_wrap_overwrites_oldest()
{
    EpasNagTraceRing r;
    EpasNagTraceEntry e{};
    for (uint32_t i = 0; i < EpasNagTraceRing::kCap + 5; i++)
    {
        e.ts_ms = i;
        r.push(e);
    }
    // Head advanced past capacity; oldest retained == head - kCap.
    uint32_t h = r.currentHead();
    TEST_ASSERT_EQUAL_UINT32(EpasNagTraceRing::kCap + 5, h);
    EpasNagTraceEntry out[2] = {};
    int n = r.readSince(h - 2, out, 2);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_UINT32(h - 2, out[0].ts_ms);
    TEST_ASSERT_EQUAL_UINT32(h - 1, out[1].ts_ms);
}
void test_trace_ring_read_since_clamps_to_oldest()
{
    EpasNagTraceRing r;
    EpasNagTraceEntry e{};
    for (uint32_t i = 0; i < 10; i++)
    {
        e.ts_ms = i;
        r.push(e);
    }
    // Request from a `since` older than what's still held -> clamped to oldest.
    EpasNagTraceEntry out[16] = {};
    int n = r.readSince(0, out, 16);
    TEST_ASSERT_EQUAL_INT(10, n);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_nag_constants);
    RUN_TEST(test_hands_level_from_raw);
    RUN_TEST(test_clamp_int16);
    RUN_TEST(test_on_steering_angle_stores_and_defaults);
    RUN_TEST(test_demand_rejects_excluded_das_states);
    RUN_TEST(test_demand_injects_when_apactive_false_but_das_strong);
    RUN_TEST(test_demand_injects_s1_when_apactive_false_but_das_light);
    RUN_TEST(test_demand_state1_holds_then_stops);
    RUN_TEST(test_demand_state2_delays_then_mild_walks);
    RUN_TEST(test_demand_state2_negative_band_when_steer_right);
    RUN_TEST(test_demand_state2_level2_hold_emits_held_value);
    RUN_TEST(test_demand_state3_delays_then_ramps_positive);
    RUN_TEST(test_demand_state3_ramp_peaks_at_2260);
    RUN_TEST(test_demand_state3_ramp_midpoint_linear);
    RUN_TEST(test_demand_state3_negative_when_steer_right);
    RUN_TEST(test_demand_unseen_0xff_falls_into_strong);
    RUN_TEST(test_checksum_additive_formula);
    RUN_TEST(test_checksum_counter_additive);
    RUN_TEST(test_gate_disabled_blocks);
    RUN_TEST(test_gate_canactive_blocks);
    RUN_TEST(test_gate_defense_blocks);
    RUN_TEST(test_gate_real_hand_blocks);
    RUN_TEST(test_gate_das_satisfied_blocks);
    RUN_TEST(test_gate_das_suspended_blocks);
    RUN_TEST(test_gate_das_15_blocks);
    RUN_TEST(test_gate_das_unseen_injects_d5);
    RUN_TEST(test_gate_das_escalating_injects);
    RUN_TEST(test_gate_blackout_blocks_then_clears);
    RUN_TEST(test_gate_blackout_handles_zero_timestamp_edge);
    RUN_TEST(test_echo_rejects_invalid_dlc);
    RUN_TEST(test_echo_gated_returns_false);
    RUN_TEST(test_echo_level_derived_from_torque);
    RUN_TEST(test_echo_demand_reject_propagates_gate);
    RUN_TEST(test_echo_counter_increments_and_wraps);
    RUN_TEST(test_echo_checksum_correct);
    RUN_TEST(test_echo_torque_in_valid_range_data23);
    RUN_TEST(test_echo_count_increments);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_reset_clears_demand_machine);
    RUN_TEST(test_diag_records_real_torque_and_counter);
    RUN_TEST(test_diag_records_echo_torque);
    RUN_TEST(test_trace_ring_push_and_read);
    RUN_TEST(test_trace_ring_wrap_overwrites_oldest);
    RUN_TEST(test_trace_ring_read_since_clamps_to_oldest);
    return UNITY_END();
}
