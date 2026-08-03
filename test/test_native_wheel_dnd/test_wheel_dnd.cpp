#include <unity.h>

#include "dash_wheel_dnd.h"

static const uint8_t kNativeMux1Idle[8] = {0xA9, 0x55, 0x00, 0x00, 0x00, 0x00, 0x31, 0x80};

static void seedNative3c2(DashWheelDND &dnd, uint32_t nowMs = 1000)
{
    TEST_ASSERT_TRUE(dnd.recordNative3c2(kNativeMux1Idle, nowMs));
}

static void armReactive(DashWheelDND &dnd)
{
    dnd.setReactiveConfigured(true);
    dnd.setRuntimeAllowed(true, DashWheelDndBlock::None);
}

static void finishReactiveAction(DashWheelDND &dnd, uint32_t startMs)
{
    uint8_t data[8] = {};
    for (uint8_t i = 0; i < DashWheelDND::kStepCount; ++i)
    {
        TEST_ASSERT_TRUE(dnd.tick(startMs + i * DashWheelDND::kStepIntervalMs, data));
        dnd.recordTxResult(true, startMs + i * DashWheelDND::kStepIntervalMs);
    }
}

void test_dnd_constants_match_reactive_contract()
{
    TEST_ASSERT_EQUAL(4, DashWheelDND::kStepCount);
    TEST_ASSERT_EQUAL_UINT32(100, DashWheelDND::kStepIntervalMs);
    TEST_ASSERT_EQUAL_UINT32(1000, DashWheelDND::kNativeMaxAgeMs);
    TEST_ASSERT_EQUAL_UINT32(2000, DashWheelDND::kReactiveCooldownMs);
    TEST_ASSERT_EQUAL(3, DashWheelDND::kMaxActionsPerEpisode);
    TEST_ASSERT_EQUAL_HEX32(0x3C2, DashWheelDND::kCanId);
}

void test_native_cache_accepts_only_idle_mux1()
{
    DashWheelDND dnd;
    uint8_t wrongMux[8] = {};
    uint8_t activeScroll[8] = {0x09, 0, 1, 0, 0, 0, 0, 0};

    TEST_ASSERT_FALSE(dnd.recordNative3c2(wrongMux, 100));
    TEST_ASSERT_FALSE(dnd.recordNative3c2(activeScroll, 200));
    TEST_ASSERT_TRUE(dnd.recordNative3c2(kNativeMux1Idle, 300));
    TEST_ASSERT_EQUAL_UINT32(2, dnd.diag.nativeRejected);
    TEST_ASSERT_TRUE(dnd.diag.nativeMux1IdleSeen);
}

void test_native_cache_expires_after_1000_ms()
{
    DashWheelDND dnd;
    seedNative3c2(dnd, 1000);
    TEST_ASSERT_TRUE(dnd.native3c2Fresh(2000));
    TEST_ASSERT_FALSE(dnd.native3c2Fresh(2001));
}

void test_volume_sequence_changes_only_data2()
{
    DashWheelDND dnd;
    uint8_t data[8] = {};
    seedNative3c2(dnd, 1000);
    dnd.startVolume();

    for (uint8_t i = 0; i < DashWheelDND::kStepCount; ++i)
    {
        TEST_ASSERT_TRUE(dnd.tick(1000 + i * 100, data));
        TEST_ASSERT_EQUAL_HEX8(DashWheelDND::kSteps[i], data[2]);
        TEST_ASSERT_EQUAL_HEX8(kNativeMux1Idle[0], data[0]);
        TEST_ASSERT_EQUAL_HEX8(kNativeMux1Idle[3], data[3]);
        TEST_ASSERT_EQUAL_HEX8(kNativeMux1Idle[7], data[7]);
    }
    TEST_ASSERT_FALSE(dnd.isRunning());
}

void test_tick_uses_100_ms_interval()
{
    DashWheelDND dnd;
    uint8_t data[8] = {};
    seedNative3c2(dnd, 1000);
    dnd.startVolume();
    TEST_ASSERT_TRUE(dnd.tick(1000, data));
    TEST_ASSERT_FALSE(dnd.tick(1099, data));
    TEST_ASSERT_TRUE(dnd.tick(1100, data));
}

void test_0x399_primary_warning_starts_reactive_volume_only()
{
    DashWheelDND dnd;
    uint8_t data[8] = {};
    seedNative3c2(dnd, 1000);
    armReactive(dnd);
    dnd.onDasStatus(6, 2, 1000);
    TEST_ASSERT_FALSE(dnd.isRunning());

    dnd.onDasStatus(6, 3, 1100, CAN_BUS_PARTY);
    TEST_ASSERT_TRUE(dnd.reactiveAction);
    TEST_ASSERT_TRUE(dnd.volumeActive);
    TEST_ASSERT_FALSE(dnd.speedActive);
    TEST_ASSERT_TRUE(dnd.tick(1100, data));
    TEST_ASSERT_EQUAL_HEX8(0x01, data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x00, data[3]);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.warningEpisodes);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.triggerCount);
    TEST_ASSERT_TRUE(dnd.diag.dasStatusSeen);
    TEST_ASSERT_EQUAL_UINT8(6, dnd.diag.apState);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_PARTY, dnd.diag.dasStatusBus);
    TEST_ASSERT_EQUAL_UINT32(1100, dnd.diag.dasStatusMs);
}

void test_0x399_primary_warning_starts_reactive_right_scroll()
{
    DashWheelDND dnd;
    uint8_t data[8] = {};
    seedNative3c2(dnd, 1000);
    dnd.setReactiveActions(false, true);
    dnd.setRuntimeAllowed(true, DashWheelDndBlock::None);
    dnd.onDasStatus(6, 3, 1100, CAN_BUS_PARTY);

    TEST_ASSERT_TRUE(dnd.reactiveAction);
    TEST_ASSERT_TRUE(dnd.speedActive);
    TEST_ASSERT_FALSE(dnd.volumeActive);
    TEST_ASSERT_EQUAL(DashWheelDndAction::RightSpeed, dnd.diag.action);
    for (uint8_t i = 0; i < DashWheelDND::kStepCount; ++i)
    {
        TEST_ASSERT_TRUE(dnd.tick(1100 + i * DashWheelDND::kStepIntervalMs, data));
        TEST_ASSERT_EQUAL_HEX8(DashWheelDND::kSteps[i], data[3]);
        TEST_ASSERT_EQUAL_HEX8(0x00, data[2]);
    }
    TEST_ASSERT_FALSE(dnd.isRunning());
    TEST_ASSERT_EQUAL_STRING("right_scroll", dashWheelDndActionName(dnd.diag.action));
}

void test_right_scroll_wins_when_both_reactive_actions_enabled()
{
    DashWheelDND dnd;
    seedNative3c2(dnd, 1000);
    dnd.setReactiveActions(true, true);
    dnd.setRuntimeAllowed(true, DashWheelDndBlock::None);
    dnd.onDasStatus(6, 3, 1100);
    TEST_ASSERT_TRUE(dnd.speedActive);
    TEST_ASSERT_FALSE(dnd.volumeActive);
    TEST_ASSERT_EQUAL(DashWheelDndAction::RightSpeed, dnd.diag.action);
}

void test_escalated_states_are_observe_only()
{
    DashWheelDND dnd;
    seedNative3c2(dnd, 1000);
    armReactive(dnd);
    dnd.onDasStatus(6, 9, 1100);

    TEST_ASSERT_FALSE(dnd.isRunning());
    TEST_ASSERT_EQUAL(DashWheelDndBlock::EscalatedObserveOnly, dnd.diag.blocked);
    TEST_ASSERT_EQUAL_UINT32(1, dnd.diag.escalatedObservations);
    TEST_ASSERT_EQUAL_UINT32(0, dnd.diag.triggerCount);
}

void test_warning_does_not_send_when_ap_inactive()
{
    DashWheelDND dnd;
    seedNative3c2(dnd, 1000);
    armReactive(dnd);
    dnd.onDasStatus(2, 3, 1100);

    TEST_ASSERT_FALSE(dnd.isRunning());
    TEST_ASSERT_EQUAL(DashWheelDndBlock::ApInactive, dnd.diag.blocked);
}

void test_warning_clear_aborts_reactive_action()
{
    DashWheelDND dnd;
    seedNative3c2(dnd, 1000);
    armReactive(dnd);
    dnd.onDasStatus(6, 3, 1100);
    TEST_ASSERT_TRUE(dnd.isRunning());
    dnd.onDasStatus(6, 2, 1150);

    TEST_ASSERT_FALSE(dnd.isRunning());
    TEST_ASSERT_EQUAL(DashWheelDndBlock::WarningClear, dnd.diag.blocked);
}

void test_reactive_repeat_is_bounded_to_three_actions()
{
    DashWheelDND dnd;
    seedNative3c2(dnd, 1000);
    armReactive(dnd);
    dnd.onDasStatus(6, 3, 1000);
    finishReactiveAction(dnd, 1000);

    seedNative3c2(dnd, 3300);
    dnd.serviceReactive(3300);
    TEST_ASSERT_TRUE(dnd.isRunning());
    finishReactiveAction(dnd, 3300);

    seedNative3c2(dnd, 5600);
    dnd.serviceReactive(5600);
    TEST_ASSERT_TRUE(dnd.isRunning());
    finishReactiveAction(dnd, 5600);

    seedNative3c2(dnd, 7900);
    dnd.serviceReactive(7900);
    TEST_ASSERT_FALSE(dnd.isRunning());
    TEST_ASSERT_EQUAL(3, dnd.diag.actionsThisEpisode);
    TEST_ASSERT_EQUAL(DashWheelDndBlock::EpisodeLimit, dnd.diag.blocked);
}

void test_two_consecutive_tx_failures_auto_disarm()
{
    DashWheelDND dnd;
    uint8_t data[8] = {};
    seedNative3c2(dnd, 1000);
    armReactive(dnd);
    dnd.onDasStatus(6, 3, 1000);
    TEST_ASSERT_TRUE(dnd.tick(1000, data));
    dnd.recordTxResult(false, 1000);
    TEST_ASSERT_FALSE(dnd.autoDisarmed);

    seedNative3c2(dnd, 3000);
    dnd.serviceReactive(3000);
    TEST_ASSERT_TRUE(dnd.tick(3000, data));
    dnd.recordTxResult(false, 3000);

    TEST_ASSERT_TRUE(dnd.autoDisarmed);
    TEST_ASSERT_FALSE(dnd.diag.armed);
    TEST_ASSERT_EQUAL(DashWheelDndBlock::AutoDisarmed, dnd.diag.blocked);
}

void test_manual_speed_action_remains_separate()
{
    DashWheelDND dnd;
    uint8_t data[8] = {};
    seedNative3c2(dnd, 1000);
    dnd.startSpeed();
    TEST_ASSERT_TRUE(dnd.tick(1000, data));
    TEST_ASSERT_EQUAL_HEX8(0x00, data[2]);
    TEST_ASSERT_EQUAL_HEX8(0x01, data[3]);
    TEST_ASSERT_FALSE(dnd.reactiveAction);
}

void test_runtime_gate_blocks_non_legacy()
{
    DashWheelDND dnd;
    seedNative3c2(dnd, 1000);
    dnd.setReactiveConfigured(true);
    dnd.setRuntimeAllowed(false, DashWheelDndBlock::NotLegacy);
    dnd.onDasStatus(6, 3, 1100);
    TEST_ASSERT_FALSE(dnd.isRunning());
    TEST_ASSERT_EQUAL(DashWheelDndBlock::NotLegacy, dnd.diag.blocked);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_dnd_constants_match_reactive_contract);
    RUN_TEST(test_native_cache_accepts_only_idle_mux1);
    RUN_TEST(test_native_cache_expires_after_1000_ms);
    RUN_TEST(test_volume_sequence_changes_only_data2);
    RUN_TEST(test_tick_uses_100_ms_interval);
    RUN_TEST(test_0x399_primary_warning_starts_reactive_volume_only);
    RUN_TEST(test_0x399_primary_warning_starts_reactive_right_scroll);
    RUN_TEST(test_right_scroll_wins_when_both_reactive_actions_enabled);
    RUN_TEST(test_escalated_states_are_observe_only);
    RUN_TEST(test_warning_does_not_send_when_ap_inactive);
    RUN_TEST(test_warning_clear_aborts_reactive_action);
    RUN_TEST(test_reactive_repeat_is_bounded_to_three_actions);
    RUN_TEST(test_two_consecutive_tx_failures_auto_disarm);
    RUN_TEST(test_manual_speed_action_remains_separate);
    RUN_TEST(test_runtime_gate_blocks_non_legacy);
    return UNITY_END();
}
