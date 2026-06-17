#include <unity.h>
#include "dash_epas_nag.h"

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
    TEST_ASSERT_EQUAL_UINT16(2050, DashEpasNagEngine::kTorqueCenter);
    TEST_ASSERT_EQUAL_UINT16(1780, DashEpasNagEngine::kTorqueMin);
    TEST_ASSERT_EQUAL_UINT16(2500, DashEpasNagEngine::kTorqueMax);
    TEST_ASSERT_EQUAL_UINT32(1500, DashEpasNagEngine::kBlackoutMs);
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

void test_torque_reverts_toward_center()
{
    DashEpasNagEngine e;
    e.torqueWalk = DashEpasNagEngine::kTorqueMax;
    uint32_t sum = 0;
    const int N = 300;
    for (int n = 0; n < N; n++)
        sum += e.nextTorque();
    uint32_t avg = sum / N;
    TEST_ASSERT_TRUE(avg >= 1950 && avg <= 2150);
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

// ── echo builder ─────────────────────────────────────────────────────
void test_echo_gated_returns_false()
{
    DashEpasNagEngine e;
    e.noteRuntime(false, true, true);
    uint8_t in[8];
    buildEpasFrame(in, 0, 5);
    uint8_t out[8] = {0};
    TEST_ASSERT_FALSE(e.tryBuildEcho(in, 1000, out));
    TEST_ASSERT_EQUAL_UINT32(0, e.diag.echoCount);
}
void test_echo_faithful_preserves_handsonlevel()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 5);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 1000, out));
    TEST_ASSERT_EQUAL_HEX8(in[4], out[4]);
}
void test_echo_counter_increments_and_wraps()
{
    DashEpasNagEngine e = makeArmedEngine();
    for (uint8_t c = 0; c < 16; c++)
    {
        uint8_t in[8];
        buildEpasFrame(in, 0, c);
        uint8_t out[8];
        TEST_ASSERT_TRUE(e.tryBuildEcho(in, 1000 + c, out));
        uint8_t expected = static_cast<uint8_t>((c + 1) & 0x0F);
        TEST_ASSERT_EQUAL_HEX8(expected, out[6] & 0x0F);
    }
}
void test_echo_checksum_correct()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 9);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 1000, out));
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
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 1000, out));
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
    e.tryBuildEcho(in, 1000, out);
    e.tryBuildEcho(in, 1010, out);
    e.tryBuildEcho(in, 1020, out);
    TEST_ASSERT_EQUAL_UINT32(3, e.diag.echoCount);
}
void test_reset_clears_state()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    e.tryBuildEcho(in, 1000, out);
    e.reset();
    TEST_ASSERT_EQUAL_UINT16(DashEpasNagEngine::kTorqueCenter, e.torqueWalk);
    TEST_ASSERT_EQUAL_UINT8(DashEpasNagEngine::kDasUnseen, e.dasHandsOnState);
    TEST_ASSERT_EQUAL_UINT32(0, e.apDisengageMs);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_nag_constants);
    RUN_TEST(test_checksum_additive_formula);
    RUN_TEST(test_checksum_counter_additive);
    RUN_TEST(test_torque_stays_in_bounds);
    RUN_TEST(test_torque_reverts_toward_center);
    RUN_TEST(test_gate_disabled_blocks);
    RUN_TEST(test_gate_canactive_blocks);
    RUN_TEST(test_gate_defense_blocks);
    RUN_TEST(test_gate_real_hand_blocks);
    RUN_TEST(test_gate_das_satisfied_blocks);
    RUN_TEST(test_gate_das_suspended_blocks);
    RUN_TEST(test_gate_das_unseen_injects_d5);
    RUN_TEST(test_gate_das_escalating_injects);
    RUN_TEST(test_gate_blackout_blocks_then_clears);
    RUN_TEST(test_echo_gated_returns_false);
    RUN_TEST(test_echo_faithful_preserves_handsonlevel);
    RUN_TEST(test_echo_counter_increments_and_wraps);
    RUN_TEST(test_echo_checksum_correct);
    RUN_TEST(test_echo_torque_in_valid_range_data23);
    RUN_TEST(test_echo_count_increments);
    RUN_TEST(test_reset_clears_state);
    return UNITY_END();
}
