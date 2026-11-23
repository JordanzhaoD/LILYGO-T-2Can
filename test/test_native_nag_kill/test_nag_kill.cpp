#include <unity.h>

#include "dash_legacy_nag_kill_045.h"
#include "dash_legacy_wheel_045_dnd.h" // crc8J1850 for expected values

// Native 0x045 idle frame (same vector family as the wheel-DND suite).
static const uint8_t kIdleC[8] = {0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xC0, 0xE0};

static void arm(DashLegacyNagKill045 &nk)
{
    nk.configure(true);
    nk.setRuntimeAllowed(true);
}

static void warn(DashLegacyNagKill045 &nk, uint32_t nowMs, uint8_t handsState)
{
    nk.onDasStatus(6, handsState, nowMs);
}

void test_warning_edge_plus_native_frame_clones_exactly_one()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    // bit27 (byte3 bit3) set
    TEST_ASSERT_EQUAL_UINT8(kIdleC[3] | 0x08, out[3]);
    // counter +1 in b6 high nibble
    TEST_ASSERT_EQUAL_UINT8(((kIdleC[6] >> 4) + 1) & 0x0F, (out[6] >> 4) & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(kIdleC[6] & 0x0F, out[6] & 0x0F);
    // CRC8-J1850 over the rewritten frame
    TEST_ASSERT_EQUAL_UINT8(DashLegacyWheel045Dnd::crc8J1850(out, 7), out[7]);
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.cloned);
    // single-shot: the next native frame within the same edge does not clone
    uint8_t out2[8];
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1100, out2));
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.cloned);
}

void test_window_excludes_hands2_and_hands6()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    warn(nk, 1000, 2);
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1050, out));
    warn(nk, 2000, 6); // strike-out domain: hard-off
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 2050, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
}

void test_own_echo_does_not_chain()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(true, 1050);
    // the echoed clone comes back: recognised, no new trigger, no clone
    TEST_ASSERT_TRUE(DashLegacyNagKill045::isCloneFrame(out));
    warn(nk, 1200, 3); // fresh edge
    uint8_t out2[8];
    TEST_ASSERT_FALSE(nk.onNative045(out, 8, 1250, out2));
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.echoSuppressed);
}

void test_trigger_expires_without_native_frame()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    // no native frame for 600 ms: the trigger flag expires
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1601, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
}

void test_two_tx_failures_disarm_then_heal_800ms()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    uint8_t out[8];
    // fail 1
    warn(nk, 1000, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(false, 1050);
    TEST_ASSERT_FALSE(nk.diag.disarmed);
    // fail 2 → disarmed at 1150
    warn(nk, 1100, 3);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1150, out));
    nk.recordTxResult(false, 1150);
    TEST_ASSERT_TRUE(nk.diag.disarmed);
    // 700 ms after the disarm: still down
    warn(nk, 1150 + 700, 3);
    TEST_ASSERT_TRUE(nk.diag.disarmed);
    // 810 ms after the disarm: healed, fresh trigger + native frame clones
    warn(nk, 1150 + 810, 3);
    TEST_ASSERT_FALSE(nk.diag.disarmed);
    TEST_ASSERT_EQUAL_UINT32(1, nk.diag.disarmHeals);
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1150 + 850, out));
}

void test_disabled_by_default_no_activity()
{
    DashLegacyNagKill045 nk; // never configured
    nk.setRuntimeAllowed(true);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_FALSE(nk.onNative045(kIdleC, 8, 1050, out));
    TEST_ASSERT_EQUAL_UINT32(0, nk.diag.cloned);
}

void test_clone_in_flight_window()
{
    DashLegacyNagKill045 nk;
    arm(nk);
    warn(nk, 1000, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(nk.onNative045(kIdleC, 8, 1050, out));
    nk.recordTxResult(true, 1050);
    TEST_ASSERT_TRUE(nk.cloneInFlight(1060));
    TEST_ASSERT_FALSE(nk.cloneInFlight(1080));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_warning_edge_plus_native_frame_clones_exactly_one);
    RUN_TEST(test_window_excludes_hands2_and_hands6);
    RUN_TEST(test_own_echo_does_not_chain);
    RUN_TEST(test_trigger_expires_without_native_frame);
    RUN_TEST(test_two_tx_failures_disarm_then_heal_800ms);
    RUN_TEST(test_disabled_by_default_no_activity);
    RUN_TEST(test_clone_in_flight_window);
    return UNITY_END();
}
