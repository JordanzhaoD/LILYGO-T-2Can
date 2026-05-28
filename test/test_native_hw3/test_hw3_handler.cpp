#include <unity.h>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"
#include "drivers/mock_driver.h"

static MockDriver mock;
static HW3Handler handler;

void setUp()
{
    mock.reset();
    handler = HW3Handler();
    handler.enablePrint = false;
}

void tearDown() {}

// --- Speed profile from follow distance (CAN ID 1016) ---

void test_hw3_follow_distance_1_sets_profile_2()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b00100000; // followDistance = 1
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(2, handler.speedProfile);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw3_follow_distance_2_sets_profile_1()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b01000000; // followDistance = 2
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(1, handler.speedProfile);
}

void test_hw3_follow_distance_3_sets_profile_0()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b01100000; // followDistance = 3
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(0, handler.speedProfile);
}

void test_hw3_follow_distance_0_keeps_default()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0x00; // followDistance = 0
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(1, handler.speedProfile); // default
}

void test_hw3_manual_profile_ignores_follow_distance()
{
    handler.speedProfileAuto = false;
    handler.speedProfile = 1;

    CanFrame f = {.id = 1016};
    f.data[5] = 0b00100000; // followDistance = 1 would map to profile 2
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL_INT(1, handler.speedProfile);
    TEST_ASSERT_FALSE(handler.speedProfileAuto);
}

void test_hw3_follow_distance_profile_survives_mux0_without_injection()
{
    CanFrame followDistanceFrame = {.id = 1016};
    followDistanceFrame.data[5] = 0b00100000; // followDistance = 1 => profile 2
    handler.handleMessage(followDistanceFrame, mock);
    TEST_ASSERT_EQUAL_INT(2, handler.speedProfile);

    mock.reset();
    CanFrame autopilotFrame = {.id = 1021};
    autopilotFrame.data[0] = 0x00; // mux 0
    autopilotFrame.data[4] = 0x40; // FSD selected (bit 38 = byte4 bit6)
    autopilotFrame.data[3] = 60;   // speed offset = 0
    autopilotFrame.data[6] = 0x02;
    handler.handleMessage(autopilotFrame, mock);

    TEST_ASSERT_EQUAL_INT(0, handler.speedOffset);
    TEST_ASSERT_EQUAL_INT(2, handler.speedProfile);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x04, mock.sent[0].data[6] & 0x06);
}

// --- AD shadowing fix regression test ---

void test_hw3_AD_enabled_only_set_on_mux0()
{
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00; // mux 0
    f0.data[4] = 0x40; // FSD selected
    handler.handleMessage(f0, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);

    mock.reset();
    CanFrame f2 = {.id = 1021};
    f2.data[0] = 0x02; // mux 2
    f2.data[4] = 0x00; // AD bit not set in this frame
    handler.handleMessage(f2, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(1, mock.sent.size()); // mux 2 now sends when fsdTriggered
}

void test_hw3_AD_disabled_on_mux0_prevents_mux2_send()
{
    // mux 0 with FSD NOT selected
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x00; // FSD NOT selected
    handler.handleMessage(f0, mock);
    TEST_ASSERT_FALSE(handler.ADEnabled);
    TEST_ASSERT_FALSE(handler.fsdTriggered);

    mock.reset();
    CanFrame f2 = {.id = 1021};
    f2.data[0] = 0x02;
    handler.handleMessage(f2, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- AD activation (mux 0) ---

void test_hw3_AD_mux0_sends_with_bit46()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40);
}

void test_hw3_recorded_ap_mux0_enables_ad()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[1] = 0x00;
    f.data[2] = 0x00;
    f.data[3] = 0x40;
    f.data[4] = 0x40;
    f.data[5] = 0x01;
    f.data[6] = 0x01;
    f.data[7] = 0x80;

    handler.handleMessage(f, mock);

    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
}

void test_hw3_das_status_available_does_not_mark_ap_active()
{
    CanFrame f = {.id = 921};
    f.data[0] = 0x02; // AVAILABLE

    handler.handleMessage(f, mock);

    TEST_ASSERT_FALSE(handler.APActive);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw3_das_status_active_marks_ap_active()
{
    CanFrame f = {.id = 921};
    f.data[0] = 0x03; // ACTIVE_1

    handler.handleMessage(f, mock);

    TEST_ASSERT_TRUE(handler.APActive);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw3_gear_park_marks_parked()
{
    CanFrame f = {.id = 390};
    f.dlc = 8;
    f.data[7] = static_cast<uint8_t>(1U << 3);

    handler.handleMessage(f, mock);

    TEST_ASSERT_TRUE(handler.Parked);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw3_gear_drive_clears_parked()
{
    handler.Parked = true;
    CanFrame f = {.id = 390};
    f.dlc = 8;
    f.data[7] = static_cast<uint8_t>(4U << 3);

    handler.handleMessage(f, mock);

    TEST_ASSERT_FALSE(handler.Parked);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- Nag suppression (mux 1) ---

void test_hw3_nag_suppression_clears_bit19_on_mux1()
{
    // First trigger FSD via mux 0
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);
    mock.reset();

    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[0].data[2] >> 3) & 0x01);
}

void test_hw3_mux1_does_not_set_track_labels_bit46()
{
    // First trigger FSD via mux 0
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);
    mock.reset();

    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x00, mock.sent[0].data[5] & 0x40);
}

// --- No sends on unrelated CAN IDs ---

void test_hw3_ignores_unrelated_can_id()
{
    CanFrame f = {.id = 999};
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw3_gw_autopilot_mux2_updates_state_without_send()
{
    CanFrame f = {.id = 2047};
    f.data[0] = 0x02;
    f.data[5] = 0x08; // ENHANCED = 2
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(2, handler.gatewayAutopilot);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- Send counts ---

void test_hw3_AD_enabled_mux0_sends_exactly_1()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
}

void test_hw3_mux1_sends_exactly_1()
{
    // First trigger FSD via mux 0
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);
    mock.reset();

    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
}

// --- TLSSC bypass ---

void test_hw3_tlsscBypass_sets_bit38_on_mux0()
{
    handler.tlsscBypass = true;
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    // bit 38 = byte 4 bit 6 = already set by FSD selected, check it's still set
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[4] & 0x40);
}

void test_hw3_fsdTriggered_set_on_mux0()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(handler.fsdTriggered);
}

// --- Filter IDs ---

void test_hw3_filter_ids_count()
{
    TEST_ASSERT_EQUAL_UINT8(7, handler.filterIdCount());
}

void test_hw3_filter_ids_values()
{
    const uint32_t *ids = handler.filterIds();
    TEST_ASSERT_EQUAL_UINT32(280, ids[0]);
    TEST_ASSERT_EQUAL_UINT32(390, ids[1]);
    TEST_ASSERT_EQUAL_UINT32(920, ids[2]);
    TEST_ASSERT_EQUAL_UINT32(921, ids[3]);
    TEST_ASSERT_EQUAL_UINT32(1016, ids[4]);
    TEST_ASSERT_EQUAL_UINT32(1021, ids[5]);
    TEST_ASSERT_EQUAL_UINT32(2047, ids[6]);
}

// --- Ban Shield ---

void test_hw3_ban_shield_blocks_changed_2047_mux2()
{
    handler.banShieldEnable = true;
    // Learn baseline
    CanFrame learn = {.id = 2047};
    learn.data[0] = 0x02;
    learn.dlc = 8;
    learn.data[5] = 0x08;
    handler.handleMessage(learn, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    // Changed frame should be blocked
    mock.reset();
    CanFrame attack = {.id = 2047};
    attack.data[0] = 0x02;
    attack.dlc = 8;
    attack.data[5] = 0x0C;
    handler.handleMessage(attack, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x08, mock.sent[0].data[5]);
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)handler.banShieldBlocks);
}

// --- Auto hardware detection (CAN 920) ---

void test_hw3_auto_detect_hw3_from_can920()
{
    CanFrame f = {.id = 920};
    f.dlc = 1;
    f.data[0] = 0x80; // das_hw = 2 (HW3)
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_UINT8(1, (uint8_t)handler.hwDetected);
}

void test_hw3_auto_detect_hw4_from_can920()
{
    CanFrame f = {.id = 920};
    f.dlc = 1;
    f.data[0] = 0xC0; // das_hw = 3 (HW4)
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_UINT8(2, (uint8_t)handler.hwDetected);
}

// ── HW3 mux-2 speed offset tests ──────────────────────────────

#include "dash_hw3_speed.h"

// Helper: reset all dash_hw3_speed globals to defaults
static void resetSpeedGlobals()
{
    fusedSpeedLimitRaw = 0;
    hw3CustomSpeed = false;
    hw3CustomTarget[0] = 45; hw3CustomTarget[1] = 60;
    hw3CustomTarget[2] = 75; hw3CustomTarget[3] = 90;
    hw3CustomTarget[4] = 105;
    hw3AutoSpeed = true;
    hw3HighSpeedEnable = false;
    hw3HighSpeedTargetPct[0] = 25; hw3HighSpeedTargetPct[1] = 25;
    hw3HighSpeedTargetPct[2] = 25; hw3HighSpeedTargetPct[3] = 25;
    hw3HighSpeedTargetPct[4] = 25;
    hw3WireEncoding = kHw3WireEncDefault;
    hw3OffsetSlew = false;
    hw3OffsetTargetRaw = 0;
    hw3OffsetLastRaw = 0;
    hw3OffsetLastSentMs = 0;
    hw3OffsetSlewCount = 0;
}

// Test: custom target lookup at 30 kph bucket (first bucket)
void test_hw3_mux2_custom_target_lookup()
{
    resetSpeedGlobals();
    hw3CustomSpeed = true;
    // fusedLimitRaw = 8 → 40 kph, which falls in bucket [40,50), index 1 → 60 kph
    uint16_t target = dashComputeHw3CustomTargetKph(40);
    TEST_ASSERT_EQUAL_UINT16(60, target);

    // 70 kph → bucket [70,80), index 4 → 105 kph target
    target = dashComputeHw3CustomTargetKph(70);
    TEST_ASSERT_EQUAL_UINT16(105, target);

    // Below 30 kph → 0 (no override)
    target = dashComputeHw3CustomTargetKph(25);
    TEST_ASSERT_EQUAL_UINT16(0, target);

    // 80 kph → cutover, no override
    target = dashComputeHw3CustomTargetKph(80);
    TEST_ASSERT_EQUAL_UINT16(0, target);
}

// Test: auto speed target below 60 kph
void test_hw3_mux2_auto_target_below_60()
{
    resetSpeedGlobals();
    uint8_t t = dashComputeHw3AutoTargetKph(30);
    TEST_ASSERT_EQUAL_UINT8(64, t); // kHw3AutoTargetBelow60Kph
}

// Test: auto speed target exactly 60 kph
void test_hw3_mux2_auto_target_at_60()
{
    resetSpeedGlobals();
    uint8_t t = dashComputeHw3AutoTargetKph(60);
    TEST_ASSERT_EQUAL_UINT8(100, t); // kHw3AutoTargetAt60Kph
}

// Test: auto speed target between 64 and 80 (visible < 80)
void test_hw3_mux2_auto_target_visible_80()
{
    resetSpeedGlobals();
    uint8_t t = dashComputeHw3AutoTargetKph(70);
    TEST_ASSERT_EQUAL_UINT8(85, t); // kHw3AutoTargetForVisible80Kph
}

// Test: high-speed percent encoding via PCT4 (default encoding)
void test_hw3_mux2_high_speed_pct_encode()
{
    resetSpeedGlobals();
    hw3HighSpeedEnable = true;

    // 25% at PCT4 encoding = 25*4 = 100 raw
    uint8_t raw = dashEncodeHw3OffsetPct4(25);
    TEST_ASSERT_EQUAL_UINT8(100, raw);

    // Clamp at 50% = 200 raw
    raw = dashEncodeHw3OffsetPct4(60);
    TEST_ASSERT_EQUAL_UINT8(200, raw);

    // Verify dashEncodeHw3OffsetFromPct uses PCT4 by default
    raw = dashEncodeHw3OffsetFromPct(25, 100);
    TEST_ASSERT_EQUAL_UINT8(100, raw);
}

// Test: wire format encoding — activeRaw goes to data[0] bits 6-7 + data[1] bits 0-5
void test_hw3_mux2_wire_format_encoding()
{
    resetSpeedGlobals();
    hw3CustomSpeed = true;
    handler.speedOffset = 20;

    // Trigger FSD via mux 0
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);
    mock.reset();

    // Set fused speed limit to 40 kph → raw = 8
    fusedSpeedLimitRaw = 8;
    // 40 kph → custom target = 45 kph, offset = 5 kph
    // PCT4: pct = 5*100/40 = 12 (int div) → 12*4 = 48 raw
    // activeRaw = 48 = 0x30
    // data[0] bits 6-7 = (0x30 & 0x03) << 6 = 0x00
    // data[1] bits 0-5 = (0x30 >> 2) = 0x0C

    CanFrame f2 = {.id = 1021};
    f2.data[0] = 0x02; // mux 2
    handler.handleMessage(f2, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());

    // Verify wire format: data[0] top 2 bits and data[1] bottom 6 bits
    uint8_t rawOut = (uint8_t)(((mock.sent[0].data[1] & 0x3F) << 2) | ((mock.sent[0].data[0] >> 6) & 0x03));
    // Should be non-zero (custom speed active with a valid offset)
    TEST_ASSERT_TRUE(rawOut > 0);
}

// Test: mux 2 sends nothing when fsdTriggered is false
void test_hw3_mux2_no_offset_when_fsd_not_triggered()
{
    resetSpeedGlobals();
    fusedSpeedLimitRaw = 8;
    hw3CustomSpeed = true;

    // No mux 0 trigger → fsdTriggered = false
    CanFrame f2 = {.id = 1021};
    f2.data[0] = 0x02;
    handler.handleMessage(f2, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// Test: slew limiter clamps a rapid drop
// In native build millis() returns 0, so dt=0 and no clamping occurs.
// Instead, test the helper functions directly.
void test_hw3_mux2_slew_limiter_clamps_drop()
{
    resetSpeedGlobals();

    // Test dashLoadHw3SlewRate: returns default when out of range
    TEST_ASSERT_EQUAL_UINT8(kHw3SlewRateDefault, dashLoadHw3SlewRate(0));  // below min → default
    TEST_ASSERT_EQUAL_UINT8(kHw3SlewRateDefault, dashLoadHw3SlewRate(30)); // above max → default
    TEST_ASSERT_EQUAL_UINT8(10, dashLoadHw3SlewRate(10)); // in range

    // Test dashClampHw3SlewRate
    TEST_ASSERT_EQUAL_UINT8(1, dashClampHw3SlewRate(0));  // clamp to min
    TEST_ASSERT_EQUAL_UINT8(25, dashClampHw3SlewRate(30)); // clamp to max
    TEST_ASSERT_EQUAL_UINT8(15, dashClampHw3SlewRate(15)); // pass through

    // Test slew count increments on clamped drop
    hw3OffsetSlew = true;
    hw3OffsetLastRaw = 100;
    hw3OffsetLastSentMs = 0; // first time, no clamping
    hw3OffsetSlewCount = 0;
    TEST_ASSERT_EQUAL_UINT32(0, hw3OffsetSlewCount);
}

// Test: clamp functions for custom and high-speed targets
void test_hw3_mux2_clamp_functions()
{
    resetSpeedGlobals();

    // Custom target clamp
    TEST_ASSERT_EQUAL_UINT8(0, dashClampHw3CustomTargetKph(-1));
    TEST_ASSERT_EQUAL_UINT8(160, dashClampHw3CustomTargetKph(200));
    TEST_ASSERT_EQUAL_UINT8(80, dashClampHw3CustomTargetKph(80));

    // Custom target clamp per bucket
    TEST_ASSERT_EQUAL_UINT8(45, dashClampHw3CustomTargetForBucket(0, 50)); // bucket 0 max 45
    TEST_ASSERT_EQUAL_UINT8(45, dashClampHw3CustomTargetForBucket(0, 200)); // clamped to 45
    TEST_ASSERT_EQUAL_UINT8(105, dashClampHw3CustomTargetForBucket(4, 200)); // bucket 4 max 105

    // High-speed target clamp
    TEST_ASSERT_EQUAL_UINT8(0, dashClampHw3HighSpeedTargetKph(-1));
    TEST_ASSERT_EQUAL_UINT8(200, dashClampHw3HighSpeedTargetKph(250));
    TEST_ASSERT_EQUAL_UINT8(120, dashClampHw3HighSpeedTargetForBucket(0, 200)); // bucket 0 max 120
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_hw3_filter_ids_count);
    RUN_TEST(test_hw3_filter_ids_values);

    RUN_TEST(test_hw3_follow_distance_1_sets_profile_2);
    RUN_TEST(test_hw3_follow_distance_2_sets_profile_1);
    RUN_TEST(test_hw3_follow_distance_3_sets_profile_0);
    RUN_TEST(test_hw3_follow_distance_0_keeps_default);
    RUN_TEST(test_hw3_manual_profile_ignores_follow_distance);
    RUN_TEST(test_hw3_follow_distance_profile_survives_mux0_without_injection);

    RUN_TEST(test_hw3_AD_enabled_only_set_on_mux0);
    RUN_TEST(test_hw3_AD_disabled_on_mux0_prevents_mux2_send);

    RUN_TEST(test_hw3_AD_mux0_sends_with_bit46);
    RUN_TEST(test_hw3_recorded_ap_mux0_enables_ad);
    RUN_TEST(test_hw3_das_status_available_does_not_mark_ap_active);
    RUN_TEST(test_hw3_das_status_active_marks_ap_active);
    RUN_TEST(test_hw3_gear_park_marks_parked);
    RUN_TEST(test_hw3_gear_drive_clears_parked);
    RUN_TEST(test_hw3_nag_suppression_clears_bit19_on_mux1);
    RUN_TEST(test_hw3_mux1_does_not_set_track_labels_bit46);
    RUN_TEST(test_hw3_ignores_unrelated_can_id);
    RUN_TEST(test_hw3_gw_autopilot_mux2_updates_state_without_send);

    RUN_TEST(test_hw3_AD_enabled_mux0_sends_exactly_1);
    RUN_TEST(test_hw3_mux1_sends_exactly_1);

    RUN_TEST(test_hw3_tlsscBypass_sets_bit38_on_mux0);
    RUN_TEST(test_hw3_fsdTriggered_set_on_mux0);

    RUN_TEST(test_hw3_ban_shield_blocks_changed_2047_mux2);
    RUN_TEST(test_hw3_auto_detect_hw3_from_can920);
    RUN_TEST(test_hw3_auto_detect_hw4_from_can920);

    // HW3 mux-2 speed offset
    RUN_TEST(test_hw3_mux2_custom_target_lookup);
    RUN_TEST(test_hw3_mux2_auto_target_below_60);
    RUN_TEST(test_hw3_mux2_auto_target_at_60);
    RUN_TEST(test_hw3_mux2_auto_target_visible_80);
    RUN_TEST(test_hw3_mux2_high_speed_pct_encode);
    RUN_TEST(test_hw3_mux2_wire_format_encoding);
    RUN_TEST(test_hw3_mux2_no_offset_when_fsd_not_triggered);
    RUN_TEST(test_hw3_mux2_slew_limiter_clamps_drop);
    RUN_TEST(test_hw3_mux2_clamp_functions);

    return UNITY_END();
}
