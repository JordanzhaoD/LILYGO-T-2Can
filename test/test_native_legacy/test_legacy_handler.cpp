#include <unity.h>
#include <cstring>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"
#include "drivers/mock_driver.h"

static MockDriver mock;
static LegacyHandler handler;

static bool denyAD()
{
    return false;
}

void setUp()
{
    mock.reset();
    handler = LegacyHandler();
    handler.enablePrint = false;
    fusedSpeedLimitRaw = 0;
    offsetMode = 1;
    manualOffsetPct = 0;
    customPct[0] = 30;
    customPct[1] = 20;
    customPct[2] = 10;
    customPct[3] = 10;
    smoothedOffset = 0.0f;
    actualOffset = 0.0f;
    forceActivateRuntime = false;
}

void tearDown() {}

// --- Speed profile from stalk position (CAN ID 69) ---

void test_legacy_stalk_pos0_sets_profile_2()
{
    CanFrame f = {.id = 69};
    f.data[1] = 0x00; // pos = 0 >> 5 = 0
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(2, handler.speedProfile);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_legacy_stalk_pos1_sets_profile_2()
{
    CanFrame f = {.id = 69};
    f.data[1] = 0x21; // pos = 0x21 >> 5 = 1
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(2, handler.speedProfile);
}

void test_legacy_stalk_pos2_sets_profile_1()
{
    CanFrame f = {.id = 69};
    f.data[1] = 0x42; // pos = 0x42 >> 5 = 2
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(1, handler.speedProfile);
}

void test_legacy_stalk_pos3_sets_profile_0()
{
    CanFrame f = {.id = 69};
    f.data[1] = 0x64; // pos = 0x64 >> 5 = 3
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(0, handler.speedProfile);
}

void test_legacy_manual_profile_ignores_stalk_position()
{
    handler.speedProfileAuto = false;
    handler.speedProfile = 1;

    CanFrame f = {.id = 69};
    f.data[1] = 0x00; // would map to profile 2
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL_INT(1, handler.speedProfile);
    TEST_ASSERT_FALSE(handler.speedProfileAuto);
}

// --- AD activation (CAN ID 1006) ---

void test_legacy_AD_enabled_on_mux0()
{
    CanFrame f = {.id = 1006};
    f.data[0] = 0x00; // mux 0
    f.data[4] = 0x40; // FSD bit set (bit 38 = bit 6 of byte 4)
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
}

void test_legacy_no_send_when_AD_disabled()
{
    CanFrame f = {.id = 1006};
    f.data[0] = 0x00; // mux 0
    f.data[4] = 0x00; // FSD bit NOT set
    handler.handleMessage(f, mock);
    TEST_ASSERT_FALSE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_legacy_AD_sets_bit46()
{
    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40);
}

void test_legacy_stable_mux0_sets_bit46_only()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::Stable;
    handler.speedProfile = 2;

    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    f.data[6] = 0x02;
    uint8_t before[8];
    memcpy(before, f.data, 8);

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40);
    TEST_ASSERT_EQUAL_HEX8(before[6], mock.sent[0].data[6]);
    for (int i = 0; i < 8; ++i)
    {
        if (i == 5)
            continue;
        TEST_ASSERT_EQUAL_HEX8(before[i], mock.sent[0].data[i]);
    }
    TEST_ASSERT_EQUAL(FsdSkipReason::Sent, handler.legacyFsdDiag.mux0.lastSkip);
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyFsdDiag.mux0.tx);
}

void test_legacy_checkAD_blocks_mux0_send()
{
    handler.checkAD = denyAD;

    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_TRUE(handler.fsdTriggered);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- fsdTriggered state tracking ---

void test_legacy_fsdTriggered_set_on_mux0()
{
    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(handler.fsdTriggered);
}

// --- Nag suppression (mux 1) ---

void test_legacy_stable_mux1_is_disabled()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::Stable;

    CanFrame f0 = {.id = 1006};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);

    CanFrame f1 = {.id = 1006};
    f1.data[0] = 0x01;
    setBit(f1, 19, true);
    handler.handleMessage(f1, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyFsdDiag.mux1.rx);
    TEST_ASSERT_EQUAL_UINT32(0, handler.legacyFsdDiag.mux1.tx);
    TEST_ASSERT_EQUAL(FsdSkipReason::DisabledInStable, handler.legacyFsdDiag.mux1.lastSkip);
}

void test_legacy_experimental_mux0_can_write_speed_profile()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::Experimental;
    handler.legacyFsdDiag.profileWriteEnable = true;
    handler.speedProfile = 2;

    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    f.data[6] = 0x02;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x04, mock.sent[0].data[6] & 0x06);
}

void test_legacy_experimental_mux1_can_clear_bit19()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::Experimental;
    handler.legacyFsdDiag.mux1Enable = true;

    CanFrame f0 = {.id = 1006};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);

    CanFrame f1 = {.id = 1006};
    f1.data[0] = 0x01;
    setBit(f1, 19, true);
    handler.handleMessage(f1, mock);

    TEST_ASSERT_EQUAL(2, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[1].data[2] >> 3) & 0x01);
    TEST_ASSERT_EQUAL(FsdSkipReason::Sent, handler.legacyFsdDiag.mux1.lastSkip);
}

void test_legacy_trigger_source_force_vs_ui_bit_vs_false()
{
    forceActivateRuntime = true;
    CanFrame forced = {.id = 1006};
    forced.data[0] = 0x00;
    forced.data[4] = 0x00;
    handler.handleMessage(forced, mock);
    TEST_ASSERT_EQUAL(FsdTriggerSource::Force, handler.legacyFsdDiag.triggerSource);

    setUp();
    CanFrame ui = {.id = 1006};
    ui.data[0] = 0x00;
    ui.data[4] = 0x40;
    handler.handleMessage(ui, mock);
    TEST_ASSERT_EQUAL(FsdTriggerSource::UiBit, handler.legacyFsdDiag.triggerSource);

    setUp();
    CanFrame none = {.id = 1006};
    none.data[0] = 0x00;
    none.data[4] = 0x00;
    handler.handleMessage(none, mock);
    TEST_ASSERT_EQUAL(FsdTriggerSource::FalseSource, handler.legacyFsdDiag.triggerSource);
    TEST_ASSERT_EQUAL(FsdSkipReason::NotTriggered, handler.legacyFsdDiag.mux0.lastSkip);
}

// --- CAN 760 offset write ---

void test_legacy_can760_writes_offset()
{
    handler.legacyOffset = 10;
    CanFrame f = {.id = 760};
    f.dlc = 8;
    f.data[5] = 0xC0;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0xC0 | 40, mock.sent[0].data[5]);
}

void test_legacy_can760_skips_when_offset_zero()
{
    handler.legacyOffset = 0;
    CanFrame f = {.id = 760};
    f.dlc = 8;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_legacy_can921_captures_fused_speed_limit()
{
    CanFrame f = {.id = 921};
    f.dlc = 2;
    f.data[1] = 12;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_UINT8(12, fusedSpeedLimitRaw);
}

void test_legacy_can760_fixed_pct_writes_simple_offset()
{
    fusedSpeedLimitRaw = 12; // 60 kph
    offsetMode = 0;
    manualOffsetPct = 20;
    handler.legacyOffset = 0;

    CanFrame f = {.id = 760};
    f.dlc = 8;
    f.data[5] = 0xC0;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0xC0 | 42, mock.sent[0].data[5]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 12.0f, actualOffset);
}

void test_legacy_can760_auto_writes_simple_offset()
{
    fusedSpeedLimitRaw = 12; // 60 kph -> target 90, offset 30
    offsetMode = 1;
    handler.legacyOffset = 0;

    CanFrame f = {.id = 760};
    f.dlc = 8;
    f.data[5] = 0x80;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x80 | 60, mock.sent[0].data[5]);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.0f, actualOffset);
}

void test_legacy_can760_custom_writes_simple_offset()
{
    fusedSpeedLimitRaw = 16; // 80 kph
    offsetMode = 2;
    customPct[2] = 10;
    handler.legacyOffset = 0;

    CanFrame f = {.id = 760};
    f.dlc = 8;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(38, mock.sent[0].data[5] & 0x3F);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 8.0f, actualOffset);
}

void test_legacy_can760_clamps_simple_offset_to_wire_max()
{
    fusedSpeedLimitRaw = 20; // 100 kph
    offsetMode = 0;
    manualOffsetPct = 50;
    handler.legacyOffset = 0;

    CanFrame f = {.id = 760};
    f.dlc = 8;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(63, mock.sent[0].data[5] & 0x3F);
}

void test_legacy_can760_checkAD_blocks_computed_offset()
{
    fusedSpeedLimitRaw = 12;
    offsetMode = 0;
    manualOffsetPct = 20;
    handler.checkAD = denyAD;

    CanFrame f = {.id = 760};
    f.dlc = 8;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- CAN 1080 visionSpeedSlider override ---

void test_legacy_can1080_sets_vision_slider()
{
    handler.overrideSpeedLimit = true;
    CanFrame f = {.id = 1080};
    f.dlc = 8;
    f.data[7] = 0x80;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x80 | 100, mock.sent[0].data[7]);
}

void test_legacy_can1080_skips_when_disabled()
{
    handler.overrideSpeedLimit = false;
    CanFrame f = {.id = 1080};
    f.dlc = 8;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- No sends on unrelated CAN IDs ---

void test_legacy_ignores_unrelated_can_id()
{
    CanFrame f = {.id = 999};
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- Filter IDs ---

void test_legacy_filter_ids_count()
{
    TEST_ASSERT_EQUAL_UINT8(9, handler.filterIdCount());
}

void test_legacy_filter_ids_values()
{
    const uint32_t *ids = handler.filterIds();
    TEST_ASSERT_EQUAL_UINT32(69, ids[0]);
    TEST_ASSERT_EQUAL_UINT32(280, ids[1]);
    TEST_ASSERT_EQUAL_UINT32(390, ids[2]);
    TEST_ASSERT_EQUAL_UINT32(760, ids[3]);
    TEST_ASSERT_EQUAL_UINT32(920, ids[4]);
    TEST_ASSERT_EQUAL_UINT32(921, ids[5]);
    TEST_ASSERT_EQUAL_UINT32(1006, ids[6]);
    TEST_ASSERT_EQUAL_UINT32(1080, ids[7]);
    TEST_ASSERT_EQUAL_UINT32(CAN_ID_OTA_STATUS, ids[8]);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_legacy_filter_ids_count);
    RUN_TEST(test_legacy_filter_ids_values);

    RUN_TEST(test_legacy_stalk_pos0_sets_profile_2);
    RUN_TEST(test_legacy_stalk_pos1_sets_profile_2);
    RUN_TEST(test_legacy_stalk_pos2_sets_profile_1);
    RUN_TEST(test_legacy_stalk_pos3_sets_profile_0);
    RUN_TEST(test_legacy_manual_profile_ignores_stalk_position);

    RUN_TEST(test_legacy_AD_enabled_on_mux0);
    RUN_TEST(test_legacy_no_send_when_AD_disabled);
    RUN_TEST(test_legacy_AD_sets_bit46);
    RUN_TEST(test_legacy_stable_mux0_sets_bit46_only);
    RUN_TEST(test_legacy_checkAD_blocks_mux0_send);
    RUN_TEST(test_legacy_fsdTriggered_set_on_mux0);

    RUN_TEST(test_legacy_stable_mux1_is_disabled);
    RUN_TEST(test_legacy_experimental_mux0_can_write_speed_profile);
    RUN_TEST(test_legacy_experimental_mux1_can_clear_bit19);
    RUN_TEST(test_legacy_trigger_source_force_vs_ui_bit_vs_false);

    RUN_TEST(test_legacy_can760_writes_offset);
    RUN_TEST(test_legacy_can760_skips_when_offset_zero);
    RUN_TEST(test_legacy_can921_captures_fused_speed_limit);
    RUN_TEST(test_legacy_can760_fixed_pct_writes_simple_offset);
    RUN_TEST(test_legacy_can760_auto_writes_simple_offset);
    RUN_TEST(test_legacy_can760_custom_writes_simple_offset);
    RUN_TEST(test_legacy_can760_clamps_simple_offset_to_wire_max);
    RUN_TEST(test_legacy_can760_checkAD_blocks_computed_offset);
    RUN_TEST(test_legacy_can1080_sets_vision_slider);
    RUN_TEST(test_legacy_can1080_skips_when_disabled);

    RUN_TEST(test_legacy_ignores_unrelated_can_id);

    return UNITY_END();
}
