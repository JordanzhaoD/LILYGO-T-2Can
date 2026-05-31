#include <unity.h>
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

void test_legacy_AD_applies_selected_speed_profile_bits()
{
    handler.speedProfile = 2;
    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    f.data[6] = 0x02;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_HEX8(0x04, mock.sent[0].data[6] & 0x06);
}

void test_legacy_checkAD_blocks_mux0_send()
{
    handler.checkAD = denyAD;

    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_FALSE(handler.ADEnabled);
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

void test_legacy_nag_suppression_clears_bit19_on_mux1()
{
    // First activate FSD via mux 0
    CanFrame f0 = {.id = 1006};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);

    // Then send mux 1
    CanFrame f = {.id = 1006};
    f.data[0] = 0x01;    // mux 1
    setBit(f, 19, true); // pre-set nag bit
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(2, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[1].data[2] >> 3) & 0x01);
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
    RUN_TEST(test_legacy_AD_applies_selected_speed_profile_bits);
    RUN_TEST(test_legacy_checkAD_blocks_mux0_send);
    RUN_TEST(test_legacy_fsdTriggered_set_on_mux0);

    RUN_TEST(test_legacy_nag_suppression_clears_bit19_on_mux1);

    RUN_TEST(test_legacy_can760_writes_offset);
    RUN_TEST(test_legacy_can760_skips_when_offset_zero);
    RUN_TEST(test_legacy_can1080_sets_vision_slider);
    RUN_TEST(test_legacy_can1080_skips_when_disabled);

    RUN_TEST(test_legacy_ignores_unrelated_can_id);

    return UNITY_END();
}
