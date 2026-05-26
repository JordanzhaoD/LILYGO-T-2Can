#include <unity.h>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"
#include "drivers/mock_driver.h"

// ============================================================
// Cross-handler unit tests for HW3, Legacy, and Nag handlers.
// Validates shared state machines: gear/park detection (CAN 280,
// 390), DAS autopilot status (CAN 921), stalk speed profile
// (CAN 69), and NagHandler hands-on echo (CAN 880).
// ============================================================

// --- HW3 / Legacy gear, park, AP, injection-gate tests ---

static MockDriver mock;

static HW3Handler hw3;
static LegacyHandler legacy;
static NagHandler nag;

// Helper: reset all handlers and the mock driver to a clean state.
static void resetAll()
{
    mock.reset();
    hw3 = HW3Handler();
    hw3.enablePrint = false;
    legacy = LegacyHandler();
    legacy.enablePrint = false;
    nag = NagHandler();
    nag.enablePrint = false;
}

void setUp()
{
    resetAll();
    // NagHandler requires nagKillerRuntime = true (set by NAG_KILLER build flag
    // in production, but native env doesn't define it).
    nagKillerRuntime = true;
}
void tearDown() {}

// ----------------------------------------------------------------
// Test 1: CAN 280 (DI_systemStatus) gear=1 (Park) -> Parked=true
// readDIGear reads byte 2 bits 5-7.  gear 1 = Park.
// ----------------------------------------------------------------
void test_hw3_park_from_280()
{
    CanFrame f = {.id = 280, .dlc = 8};
    f.data[2] = static_cast<uint8_t>(1U << 5); // gear = 1 (Park)

    hw3.handleMessage(f, mock);

    TEST_ASSERT_TRUE(hw3.Parked);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// ----------------------------------------------------------------
// Test 2: CAN 280 gear=4 (Drive) -> Parked=false
// ----------------------------------------------------------------
void test_hw3_drive_from_280()
{
    // Start as parked (default)
    TEST_ASSERT_TRUE(hw3.Parked);

    CanFrame f = {.id = 280, .dlc = 8};
    f.data[2] = static_cast<uint8_t>(4U << 5); // gear = 4 (Drive)

    hw3.handleMessage(f, mock);

    TEST_ASSERT_FALSE(hw3.Parked);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// ----------------------------------------------------------------
// Test 3: CAN 921 byte[0]=0x03 -> APActive=true
// DAS status 3..5 = active. readDASAutopilotStatus reads byte 0
// low nibble.  isDASAutopilotActive returns true for 3-5.
// ----------------------------------------------------------------
void test_hw3_ap_active_from_921()
{
    CanFrame f = {.id = 921, .dlc = 8};
    f.data[0] = 0x03; // DAS status = 3 = ACTIVE

    hw3.handleMessage(f, mock);

    TEST_ASSERT_TRUE(hw3.APActive);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// ----------------------------------------------------------------
// Test 4: CAN 921 byte[0]=0x00 -> APActive=false
// Status 0 = unavailable, not active.
// ----------------------------------------------------------------
void test_hw3_ap_inactive_from_921()
{
    // Force AP active first, then deactivate
    hw3.APActive = true;

    CanFrame f = {.id = 921, .dlc = 8};
    f.data[0] = 0x00; // DAS status = 0 = unavailable

    hw3.handleMessage(f, mock);

    TEST_ASSERT_FALSE(hw3.APActive);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// ----------------------------------------------------------------
// Test 5: injection gate open when Parked=true (default).
// By default Parked=true, so gate should be open even without
// setting APActive or Summoning.
// ----------------------------------------------------------------
void test_hw3_injection_gate_open_parked()
{
    // Default state: Parked=true, APActive=false, Summoning=false
    TEST_ASSERT_TRUE(hw3.Parked);
    TEST_ASSERT_FALSE(hw3.APActive);
    TEST_ASSERT_FALSE(hw3.Summoning);
    TEST_ASSERT_TRUE(hw3.injectionGateOpen());
}

// ----------------------------------------------------------------
// Test 6: injection gate open when APActive=true (not parked).
// ----------------------------------------------------------------
void test_hw3_injection_gate_open_ap()
{
    hw3.APActive = true;
    hw3.Parked = false;
    hw3.Summoning = false;

    TEST_ASSERT_TRUE(hw3.injectionGateOpen());
}

// ----------------------------------------------------------------
// Test 7: injection gate closed when nothing is active.
// Parked=false, APActive=false, Summoning=false -> gate closed.
// ----------------------------------------------------------------
void test_hw3_injection_gate_closed()
{
    hw3.Parked = false;
    hw3.APActive = false;
    hw3.Summoning = false;

    TEST_ASSERT_FALSE(hw3.injectionGateOpen());
}

// ----------------------------------------------------------------
// Test 8: Legacy stalk speed profile (CAN 69).
// byte[1] bits 5-7 encode stalk position.
// pos <= 1 -> profile 2, pos == 2 -> profile 1, else -> profile 0.
// ----------------------------------------------------------------
void test_legacy_speed_profile_stalk()
{
    // Test pos 0 -> profile 2
    {
        CanFrame f = {.id = 69, .dlc = 8};
        f.data[1] = 0x00; // pos = 0 >> 5 = 0
        legacy.handleMessage(f, mock);
        TEST_ASSERT_EQUAL_INT(2, legacy.speedProfile);
    }

    mock.reset();
    legacy = LegacyHandler();
    legacy.enablePrint = false;

    // Test pos 2 -> profile 1
    {
        CanFrame f = {.id = 69, .dlc = 8};
        f.data[1] = 0x42; // pos = 0x42 >> 5 = 2
        legacy.handleMessage(f, mock);
        TEST_ASSERT_EQUAL_INT(1, legacy.speedProfile);
    }

    mock.reset();
    legacy = LegacyHandler();
    legacy.enablePrint = false;

    // Test pos 3 -> profile 0
    {
        CanFrame f = {.id = 69, .dlc = 8};
        f.data[1] = 0x64; // pos = 0x64 >> 5 = 3
        legacy.handleMessage(f, mock);
        TEST_ASSERT_EQUAL_INT(0, legacy.speedProfile);
    }
}

// ----------------------------------------------------------------
// Test 9: NagHandler echoes CAN 880 when handsOnLevel=0.
// handsOnLevel is (frame.data[4] >> 6) & 0x03.
// When 0 (hands off) and nagKillerActive + nagKillerRuntime are
// true, the handler sends an echo with data[4] |= 0x40
// (handsOnLevel set to 1).
// ----------------------------------------------------------------
void test_nag_echo_hands_off()
{
    CanFrame f = {.id = 880, .dlc = 8};
    f.data[4] = 0x00; // handsOnLevel = 0 (hands off)
    // Fill other bytes with non-zero values to verify echo structure
    f.data[0] = 0x12;
    f.data[1] = 0x34;
    f.data[2] = 0x56;
    f.data[3] = 0x78;
    f.data[5] = 0x9A;
    f.data[6] = 0xBC;

    nag.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    // The echo must have handsOnLevel = 1: data[4] bit 6 set
    TEST_ASSERT_TRUE((mock.sent[0].data[4] & 0x40) != 0);
}

// ----------------------------------------------------------------
// Test 10: NagHandler does NOT echo when handsOnLevel != 0.
// handsOnLevel=1 (hands on) -> no echo sent.
// ----------------------------------------------------------------
void test_nag_no_echo_hands_on()
{
    CanFrame f = {.id = 880, .dlc = 8};
    f.data[4] = 0x40; // handsOnLevel = 1 (hands on)

    nag.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

int main()
{
    UNITY_BEGIN();

    // HW3 gear/park from CAN 280
    RUN_TEST(test_hw3_park_from_280);
    RUN_TEST(test_hw3_drive_from_280);

    // HW3 DAS autopilot status from CAN 921
    RUN_TEST(test_hw3_ap_active_from_921);
    RUN_TEST(test_hw3_ap_inactive_from_921);

    // Injection gate logic
    RUN_TEST(test_hw3_injection_gate_open_parked);
    RUN_TEST(test_hw3_injection_gate_open_ap);
    RUN_TEST(test_hw3_injection_gate_closed);

    // Legacy stalk speed profile from CAN 69
    RUN_TEST(test_legacy_speed_profile_stalk);

    // NagHandler CAN 880 hands-on echo
    RUN_TEST(test_nag_echo_hands_off);
    RUN_TEST(test_nag_no_echo_hands_on);

    return UNITY_END();
}
