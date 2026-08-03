#include <unity.h>
#include "dash_legacy_smart_offset.h"

static LegacySmartOffsetConfig cfg;
static LegacySmartOffsetEngine engine;

void setUp()
{
    cfg = LegacySmartOffsetConfig{};
    engine = LegacySmartOffsetEngine{};
}

void tearDown() {}

static LegacySmartOffsetResult compute(uint8_t rawLimit, uint32_t nowMs, bool engaged = true)
{
    return engine.compute(cfg, rawLimit, nowMs, engaged);
}

void test_off_mode_outputs_zero()
{
    cfg.mode = LegacySmartOffsetMode::Off;
    cfg.manualOffsetKph = 12;
    LegacySmartOffsetResult r = compute(12, 1000);
    TEST_ASSERT_EQUAL_UINT8(0, r.outputOffsetKph);
    TEST_ASSERT_EQUAL_STRING("off", r.blockedReason);
}

void test_manual_mode_clamps_to_33()
{
    cfg.mode = LegacySmartOffsetMode::Manual;
    cfg.manualOffsetKph = 99;
    TEST_ASSERT_EQUAL_UINT8(33, compute(12, 1000).outputOffsetKph);
}

void test_unknown_limit_auto_fails_closed()
{
    cfg.mode = LegacySmartOffsetMode::Auto;
    cfg.manualOffsetKph = 8;
    LegacySmartOffsetResult r = compute(0, 1000);
    TEST_ASSERT_EQUAL_UINT8(0, r.outputOffsetKph);
    TEST_ASSERT_TRUE(r.fallbackUsed);
    TEST_ASSERT_EQUAL_STRING("speedLimitUnknown", r.blockedReason);
}

void test_reference_3ee_wire_write()
{
    uint8_t data[8] = {0};
    data[3] = 0x01;
    data[5] = 0xA4;
    data[7] = 0xC5;
    dashWriteLegacyOffsetTo3eeMux0(data, 30, 50);
    TEST_ASSERT_EQUAL_HEX8(0xF9, data[3]);
    TEST_ASSERT_EQUAL_HEX8(0xA7, data[5]);
    TEST_ASSERT_EQUAL_HEX8(0xF2, data[7]);
}

void test_auto_reference_points()
{
    cfg.mode = LegacySmartOffsetMode::Auto;
    TEST_ASSERT_EQUAL_UINT8(22, compute(7, 1000).outputOffsetKph);
    TEST_ASSERT_EQUAL_UINT8(30, compute(12, 1100).outputOffsetKph);
    TEST_ASSERT_EQUAL_UINT8(24, compute(16, 1200).outputOffsetKph);
    TEST_ASSERT_EQUAL_UINT8(12, compute(24, 1300).outputOffsetKph);
}

void test_custom_bands_and_pct_clamp()
{
    cfg.mode = LegacySmartOffsetMode::Custom;
    cfg.customPctLow = 70;
    cfg.customPctMid = 31;
    cfg.customPctHigh = 21;
    cfg.customPctVeryHigh = 11;
    TEST_ASSERT_EQUAL_UINT8(63, compute(10, 1000).offsetPct);
    TEST_ASSERT_EQUAL_UINT8(31, compute(14, 1040).offsetPct);
    TEST_ASSERT_EQUAL_UINT8(21, compute(20, 1080).offsetPct);
    TEST_ASSERT_EQUAL_UINT8(11, compute(24, 1120).offsetPct);
}

void test_speed_down_smooths_at_five_kph_per_second()
{
    cfg.mode = LegacySmartOffsetMode::Auto;
    cfg.smoothDownRateKphS = 5;
    LegacySmartOffsetResult high = compute(20, 1000);
    LegacySmartOffsetResult low = compute(8, 2000);
    TEST_ASSERT_EQUAL_UINT16(120, high.smoothedTargetKph);
    TEST_ASSERT_EQUAL_UINT16(115, low.smoothedTargetKph);
    TEST_ASSERT_TRUE(low.smoothingActive);
}

static CanFrame routeFrame(uint8_t bus, uint8_t value)
{
    CanFrame frame = {.id = 1006};
    frame.bus = bus;
    frame.data[0] = 0;
    frame.data[1] = value;
    return frame;
}

void test_bus_lock_requires_three_consistent_samples()
{
    DashLegacySpeedBusLock lock;
    CanFrame a = routeFrame(CAN_BUS_PARTY, 1);
    CanFrame b = routeFrame(CAN_BUS_PARTY, 2);
    CanFrame c = routeFrame(CAN_BUS_PARTY, 3);
    TEST_ASSERT_FALSE(lock.accept(a, 100));
    TEST_ASSERT_FALSE(lock.accept(b, 120));
    TEST_ASSERT_TRUE(lock.accept(c, 140));
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_PARTY, lock.selectedBus);
}

void test_bus_lock_blocks_mirrored_frames()
{
    DashLegacySpeedBusLock lock;
    CanFrame can1 = routeFrame(CAN_BUS_VEH, 9);
    CanFrame can2 = can1;
    can2.bus = CAN_BUS_PARTY;
    TEST_ASSERT_FALSE(lock.accept(can1, 100));
    TEST_ASSERT_FALSE(lock.accept(can2, 105));
    TEST_ASSERT_TRUE(lock.ambiguous);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_ANY, lock.selectedBus);
}

void test_manual_bus_override_is_immediate()
{
    DashLegacySpeedBusLock lock;
    lock.setMode(LegacySpeedBusMode::Can2);
    CanFrame can1 = routeFrame(CAN_BUS_VEH, 1);
    CanFrame can2 = routeFrame(CAN_BUS_PARTY, 1);
    TEST_ASSERT_FALSE(lock.accept(can1, 100));
    TEST_ASSERT_TRUE(lock.accept(can2, 101));
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_PARTY, lock.selectedBus);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_off_mode_outputs_zero);
    RUN_TEST(test_manual_mode_clamps_to_33);
    RUN_TEST(test_unknown_limit_auto_fails_closed);
    RUN_TEST(test_reference_3ee_wire_write);
    RUN_TEST(test_auto_reference_points);
    RUN_TEST(test_custom_bands_and_pct_clamp);
    RUN_TEST(test_speed_down_smooths_at_five_kph_per_second);
    RUN_TEST(test_bus_lock_requires_three_consistent_samples);
    RUN_TEST(test_bus_lock_blocks_mirrored_frames);
    RUN_TEST(test_manual_bus_override_is_immediate);
    return UNITY_END();
}
