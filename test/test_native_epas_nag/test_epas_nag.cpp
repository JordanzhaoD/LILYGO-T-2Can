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
void test_demand_ap_inactive_blocks()
{
    DashEpasNagEngine e = makeDemandEngine(3);
    e.noteApState(false, 1000);
    uint16_t t; uint8_t lvl;
    TEST_ASSERT_FALSE(e.nextDemandTorque(3, 5000, t, lvl));
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

// ── torque walk ──────────────────────────────────────────────────────
void test_torque_stays_in_bounds()
{
    DashEpasNagEngine e;
    for (int n = 0; n < 500; n++)
    {
        uint16_t t = e.nextTorque();
        TEST_ASSERT_TRUE(t >= DashEpasNagEngine::kTorqueMin);
        TEST_ASSERT_TRUE(t <= DashEpasNagEngine::kTorqueMax);
    }
}

// B1 fix (real-car evidence 2026-06-18: echoTorque=1908 = -1.42 Nm, yet nag
// still fired). A mean-reverting torque that drifts back toward 0 Nm never
// looks like "hands on" — the car ignores it. Echo must HOLD a sustained
// positive torque (+1.80 Nm = raw 2230), matching the proven NagHandler
// byte3=0xB6 strategy. Fixed, not a random walk.
void test_torque_returns_fixed_positive()
{
    DashEpasNagEngine e;
    for (int n = 0; n < 50; n++)
        TEST_ASSERT_EQUAL_UINT16(2230, e.nextTorque());
}

void test_torque_sequence_is_deterministic()
{
    DashEpasNagEngine a;
    DashEpasNagEngine b;
    for (int n = 0; n < 64; n++)
        TEST_ASSERT_EQUAL_UINT16(a.nextTorque(), b.nextTorque());
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
// B2 (real-car 2026-06-18): echoTorque=2230 already matched NagHandler yet nag
// still fired — torque alone is insufficient. Force handsOnLevel active
// (byte4 bit6=1), matching NagHandler byte4|=0x40. CSV showed handsOn=0 (hands
// off) on every echoed frame; the car disbelieves "hands off + 1.80Nm torque".
void test_echo_forces_handson_active()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 5); // handsOnLevel=0 (hands off) — the nag-trigger case
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 1000, out));
    TEST_ASSERT_EQUAL_HEX8(0x40, out[4] & 0x40); // bit6 forced set -> handsOn active
}
void test_echo_counter_increments_and_wraps()
{
    DashEpasNagEngine e = makeArmedEngine();
    for (uint8_t c = 0; c < 16; c++)
    {
        uint8_t in[8];
        buildEpasFrame(in, 0, c);
        in[6] = static_cast<uint8_t>(0xA0 | c);
        uint8_t out[8];
        TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 1000 + c, out));
        uint8_t expected = static_cast<uint8_t>(0xA0 | ((c + 1) & 0x0F));
        TEST_ASSERT_EQUAL_HEX8(expected, out[6]);
    }
}
void test_echo_checksum_correct()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 9);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 1000, out));
    uint16_t expected = 0x70u + 0x03u;
    for (int i = 0; i < 7; i++)
        expected += out[i];
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(expected & 0xFF), out[7]);
}
void test_echo_torque_in_valid_range_data23()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 1000, out));
    uint16_t raw = static_cast<uint16_t>(((out[2] & 0x0F) << 8) | out[3]);
    TEST_ASSERT_TRUE(raw >= DashEpasNagEngine::kTorqueMin);
    TEST_ASSERT_TRUE(raw <= DashEpasNagEngine::kTorqueMax);
}
void test_echo_count_increments()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    e.tryBuildEcho(in, 8, 1000, out);
    e.tryBuildEcho(in, 8, 1010, out);
    e.tryBuildEcho(in, 8, 1020, out);
    TEST_ASSERT_EQUAL_UINT32(3, e.diag.echoCount);
}
void test_reset_clears_state()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    e.tryBuildEcho(in, 8, 1000, out);
    e.reset();
    TEST_ASSERT_EQUAL_UINT16(DashEpasNagEngine::kTorqueCenter, e.torqueWalk);
    TEST_ASSERT_EQUAL_UINT8(DashEpasNagEngine::kDasUnseen, e.dasHandsOnState);
    TEST_ASSERT_EQUAL_UINT32(0, e.apDisengageMs);
}

// ── diag snapshots (observability) ───────────────────────────────────
void test_diag_records_real_torque_and_counter()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8] = {0};
    in[2] = 0x0A; // torque high nibble
    in[3] = 0x34; // torque low byte -> raw 0xA34
    in[4] = 0x00; // handsOnLevel 0
    in[6] = 0x07; // counter 7
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 1000, out));
    TEST_ASSERT_EQUAL_UINT16(0x0A34, e.diag.lastRealTorque);
    TEST_ASSERT_EQUAL_UINT8(7, e.diag.lastRealCnt);
}
void test_diag_records_echo_torque()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 1000, out));
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
    RUN_TEST(test_demand_ap_inactive_blocks);
    RUN_TEST(test_demand_state1_holds_then_stops);
    RUN_TEST(test_demand_state2_delays_then_mild_walks);
    RUN_TEST(test_demand_state2_negative_band_when_steer_right);
    RUN_TEST(test_demand_state2_level2_hold_emits_held_value);
    RUN_TEST(test_checksum_additive_formula);
    RUN_TEST(test_checksum_counter_additive);
    RUN_TEST(test_torque_stays_in_bounds);
    RUN_TEST(test_torque_returns_fixed_positive);
    RUN_TEST(test_torque_sequence_is_deterministic);
    RUN_TEST(test_gate_disabled_blocks);
    RUN_TEST(test_gate_canactive_blocks);
    RUN_TEST(test_gate_defense_blocks);
    RUN_TEST(test_gate_real_hand_blocks);
    RUN_TEST(test_gate_das_satisfied_blocks);
    RUN_TEST(test_gate_das_suspended_blocks);
    RUN_TEST(test_gate_das_unseen_injects_d5);
    RUN_TEST(test_gate_das_escalating_injects);
    RUN_TEST(test_gate_blackout_blocks_then_clears);
    RUN_TEST(test_gate_blackout_handles_zero_timestamp_edge);
    RUN_TEST(test_echo_rejects_invalid_dlc);
    RUN_TEST(test_echo_gated_returns_false);
    RUN_TEST(test_echo_forces_handson_active);
    RUN_TEST(test_echo_counter_increments_and_wraps);
    RUN_TEST(test_echo_checksum_correct);
    RUN_TEST(test_echo_torque_in_valid_range_data23);
    RUN_TEST(test_echo_count_increments);
    RUN_TEST(test_reset_clears_state);
    RUN_TEST(test_diag_records_real_torque_and_counter);
    RUN_TEST(test_diag_records_echo_torque);
    RUN_TEST(test_trace_ring_push_and_read);
    RUN_TEST(test_trace_ring_wrap_overwrites_oldest);
    RUN_TEST(test_trace_ring_read_since_clamps_to_oldest);
    return UNITY_END();
}
