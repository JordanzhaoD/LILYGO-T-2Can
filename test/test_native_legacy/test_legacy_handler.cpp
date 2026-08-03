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

static bool denyLegacyActivation(uint32_t)
{
    return false;
}

static FsdGateBlockReason denyByApGate()
{
    return FsdGateBlockReason::ApGate;
}

static CanFrame makeLegacyNagGearDrive()
{
    CanFrame frame = {.id = 280};
    frame.data[2] = static_cast<uint8_t>(4 << 5);
    return frame;
}

static CanFrame makeLegacyNagDas(uint8_t apState, uint8_t handsOn)
{
    CanFrame frame = {.id = 921};
    frame.data[0] = static_cast<uint8_t>(apState & 0x0F);
    frame.data[5] = static_cast<uint8_t>((handsOn & 0x0F) << 2);
    return frame;
}

static CanFrame makeLegacyNagEpas(uint8_t counter, uint8_t bus = CAN_BUS_VEH)
{
    CanFrame frame = {.id = 880};
    frame.bus = bus;
    frame.data[0] = 0x11;
    frame.data[1] = 0x22;
    frame.data[2] = 0x08;
    frame.data[3] = 0x12;
    frame.data[4] = 0x1F;
    frame.data[5] = 0x33;
    frame.data[6] = static_cast<uint8_t>(0x40 | (counter & 0x0F));
    frame.data[7] = LegacyHandler::legacyNagChecksum(frame);
    return frame;
}

static CanFrame makeLegacyNagSteering(int16_t angleX10 = -100,
                                      uint8_t bus = CAN_BUS_VEH,
                                      uint8_t controlType = 1)
{
    CanFrame frame = {.id = 1160};
    frame.bus = bus;
    uint16_t raw = static_cast<uint16_t>(static_cast<int32_t>(angleX10) + 16384);
    frame.data[0] = static_cast<uint8_t>((raw >> 8) & 0x7F);
    frame.data[1] = static_cast<uint8_t>(raw & 0xFF);
    frame.data[2] = static_cast<uint8_t>((controlType & 0x03) << 6);
    return frame;
}

static void enableLegacyNag(uint8_t handsOn = 0)
{
    nagKillerRuntime = true;
    handler.bionicSteering = true;
    handler.resetBionic(1234);
    CanFrame gear = makeLegacyNagGearDrive();
    handler.handleMessage(gear, mock);
    CanFrame das = makeLegacyNagDas(4, handsOn);
    handler.handleMessage(das, mock);
    for (uint16_t i = 0; i < 2200; i++)
        (void)dashDiagNowMs();
    handler.handleMessage(das, mock);
    CanFrame steering = makeLegacyNagSteering();
    handler.handleMessage(steering, mock);
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
    nagKillerRuntime = false;
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

void test_legacy_activation_callback_blocks_mux0_send()
{
    handler.legacyFsdActivationAllowed = denyLegacyActivation;

    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);

    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_TRUE(handler.fsdTriggered);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL(FsdSkipReason::GateBlocked, handler.legacyFsdDiag.mux0.lastSkip);
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

void test_legacy_tesla_parity_mux0_writes_speed_profile()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::TeslaParity;
    handler.speedProfile = 2;

    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    f.data[6] = 0x00;

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40);
    TEST_ASSERT_EQUAL_HEX8(0x04, mock.sent[0].data[6] & 0x06);
}

void test_legacy_tesla_parity_mux1_clears_bit19()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::TeslaParity;

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

// Pre-fix this test asserted bit48 was PRESERVED under TeslaParity even with
// visionLimitClearEnable=true (because line 465 required policy==Experimental).
// Spec Task 1 made visionLimitClearEnable policy-independent, so the old
// expectation is inverted. The post-fix behavior is covered by
// test_legacy_vision_clear_works_under_tesla_parity_policy and
// test_legacy_vision_clear_disabled_does_not_clear_bit48 below.

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

void test_legacy_mux0_records_can_a_twai_labels()
{
    CanFrame f = {.id = 1006};
    f.bus = CAN_BUS_VEH;
    f.data[0] = 0x00;
    f.data[4] = 0x40;

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(FsdDiagBus::CanA, handler.legacyFsdDiag.mux0.bus);
    TEST_ASSERT_EQUAL(FsdDiagDriver::Twai, handler.legacyFsdDiag.mux0.driver);
    TEST_ASSERT_NOT_EQUAL(0, handler.legacyFsdDiag.firstMux0RxMs);
    TEST_ASSERT_NOT_EQUAL(0, handler.legacyFsdDiag.firstTxOkMs);
}

void test_legacy_gate_block_records_gate_blocked_health()
{
    handler.checkAD = denyAD;
    CanFrame f = {.id = 1006};
    f.bus = CAN_BUS_VEH;
    f.data[0] = 0x00;
    f.data[4] = 0x40;

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(FsdSkipReason::GateBlocked, handler.legacyFsdDiag.mux0.lastSkip);
    TEST_ASSERT_EQUAL(FsdHealthState::GateBlocked, handler.legacyFsdDiag.health);
    TEST_ASSERT_EQUAL(FsdGateBlockReason::CheckAd, handler.legacyFsdDiag.lastBlockedBy);
}

void test_legacy_gate_block_uses_resolver_reason()
{
    handler.checkAD = denyAD;
    handler.gateBlockReason = denyByApGate;
    CanFrame f = {.id = 1006};
    f.bus = CAN_BUS_VEH;
    f.data[0] = 0x00;
    f.data[4] = 0x40;

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(FsdSkipReason::GateBlocked, handler.legacyFsdDiag.mux0.lastSkip);
    TEST_ASSERT_EQUAL(FsdHealthState::GateBlocked, handler.legacyFsdDiag.health);
    TEST_ASSERT_EQUAL(FsdGateBlockReason::ApGate, handler.legacyFsdDiag.lastBlockedBy);
}

// --- Experimental Legacy safety controls ---

void test_abort_guard_latches_on_abort_and_only_clears_after_clean_disengage()
{
    handler.abortGuard.setEnabled(true);
    handler.abortGuard.onApState(8, 1000);
    TEST_ASSERT_TRUE(handler.abortGuard.diag().latched);
    TEST_ASSERT_FALSE(handler.abortGuard.allowsInjection());

    handler.abortGuard.onApState(6, 1100);
    TEST_ASSERT_TRUE(handler.abortGuard.diag().latched);
    handler.abortGuard.onApState(1, 1200);
    TEST_ASSERT_FALSE(handler.abortGuard.diag().latched);
    TEST_ASSERT_TRUE(handler.abortGuard.allowsInjection());
    TEST_ASSERT_EQUAL_STRING("clean_disengage", handler.abortGuard.diag().lastClearReason);

    // beta.15 parity: state 2 (AVAILABLE) is below DAS_APSTATE_ENGAGED (3), so
    // it clears the latch too — the abort guard release threshold now matches
    // AP-First's engagement threshold (single-CAN / upstream alignment).
    handler.abortGuard.onApState(9, 1300);
    TEST_ASSERT_TRUE(handler.abortGuard.diag().latched);
    handler.abortGuard.onApState(2, 1400);
    TEST_ASSERT_FALSE(handler.abortGuard.diag().latched);
    TEST_ASSERT_TRUE(handler.abortGuard.allowsInjection());
}

void test_abort_guard_blocks_legacy_mux0_before_transmit()
{
    handler.abortGuard.setEnabled(true);
    handler.abortGuard.onApState(9, 1000);
    CanFrame f = {.id = 1006};
    f.data[0] = 0;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL_UINT32(0, mock.sent.size());
    TEST_ASSERT_EQUAL(FsdGateBlockReason::AbortGuard, handler.legacyFsdDiag.lastBlockedBy);
    TEST_ASSERT_EQUAL_UINT32(1, handler.abortGuard.diag().blocks);
    TEST_ASSERT_EQUAL_STRING("legacy_fsd_mux0", handler.abortGuard.diag().lastBlockedPath);
}

// --- Minimal Inject (Legacy mux0 quota) ---
// The old level-triggered DashMinimalInject quota on the Legacy mux0 path has
// been superseded by the edge-gated DashLegacySteerDefense Minimal burst (#108,
// v2.16-beta.17/.19 parity). The handler no longer consults minimalInject on
// mux0, so the prior "exactly five mux0 activations" and "quota reset on
// engagement" assertions describe retired behavior. Equivalent coverage of the
// 5-frame edge burst (armed at engagement edge, hard stop, reset on disengage)
// lives in test/test_native_legacy_steer_defense/.
void test_minimal_inject_superseded_by_steer_defense_placeholder()
{
    // Sentinel: the base DashMinimalInject module still compiles and its diag
    // reflects an idle state when not consulted on the Legacy mux0 path.
    handler.minimalInject.setEnabled(true);
    TEST_ASSERT_TRUE(handler.minimalInject.diag().enabled);
    TEST_ASSERT_EQUAL_UINT8(0, handler.minimalInject.diag().successfulMux0);
}

// --- 视觉限速 clear 独立于 policy (spec Task 1) ---
// NOTE: Stable policy short-circuits mux-1 at handlers.h:431 (DisabledInStable),
// so the effective demonstration of policy-independent visionLimitClearEnable
// uses TeslaParity (which already permits mux-1 injection). The old behavior
// (Experimental-only gate) was recorded by test_legacy_tesla_parity_does_not_clear_bit48_...

void test_legacy_vision_clear_works_under_tesla_parity_policy()
{
    // Pre-fix: line 465 required policy==Experimental, so TeslaParity+vision=true
    // did NOT clear bit48. Post-fix: visionLimitClearEnable gates independently.
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::TeslaParity;
    handler.legacyFsdDiag.visionLimitClearEnable = true;

    // mux-0 first to arm fsdTriggered (matches existing TeslaParity test pattern)
    CanFrame f0 = {.id = 1006};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);

    CanFrame f1 = {.id = 1006};
    f1.data[0] = 0x01; // mux 1
    setBit(f1, 19, true);
    setBit(f1, 48, true); // bit48 set, expect cleared post-fix
    handler.handleMessage(f1, mock);

    TEST_ASSERT_EQUAL(2, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[1].data[2] >> 3) & 0x01); // bit19 cleared (existing behavior)
    TEST_ASSERT_FALSE(mock.sent[1].data[6] & 0x01);        // bit48 cleared (NEW: post-fix)
    TEST_ASSERT_EQUAL(FsdSkipReason::Sent, handler.legacyFsdDiag.mux1.lastSkip);
}

void test_legacy_vision_clear_disabled_does_not_clear_bit48()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::TeslaParity;
    handler.legacyFsdDiag.visionLimitClearEnable = false;

    CanFrame f0 = {.id = 1006};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);

    CanFrame f1 = {.id = 1006};
    f1.data[0] = 0x01;
    setBit(f1, 19, true);
    setBit(f1, 48, true);
    handler.handleMessage(f1, mock);

    TEST_ASSERT_EQUAL(2, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[1].data[2] >> 3) & 0x01); // bit19 still cleared
    TEST_ASSERT_TRUE(mock.sent[1].data[6] & 0x01);         // bit48 PRESERVED (flag off)
}

// --- Legacy Smart Offset: 0x2F8 read-only, output on 0x3EE mux0 ---

static void observeLegacyGpsLimit(uint8_t rawLimit, uint8_t userOffsetRaw = 30)
{
    CanFrame frame = {.id = 760};
    frame.bus = CAN_BUS_VEH;
    frame.data[5] = static_cast<uint8_t>(0xC0 | (userOffsetRaw & 0x3F));
    frame.data[6] = rawLimit & 0x1F;
    handler.handleMessage(frame, mock);
}

static void sendLegacySpeedMux0(uint8_t bus, uint8_t sequence)
{
    CanFrame frame = {.id = 1006};
    frame.bus = bus;
    frame.data[0] = 0;
    frame.data[1] = sequence;
    handler.handleMessage(frame, mock);
}

static void configureLegacySmartSpeed(LegacySmartOffsetMode mode)
{
    handler.legacySmartOffsetConfig.mode = mode;
    handler.legacySmartOffsetConfig.manualOffsetKph = 0;
    handler.legacySmartOffsetConfig.smoothDownEnabled = true;
    handler.legacySmartOffsetConfig.smoothDownRateKphS = 5;
    handler.legacySmartOffsetConfig.customPctLow = 30;
    handler.legacySmartOffsetConfig.customPctMid = 20;
    handler.legacySmartOffsetConfig.customPctHigh = 10;
    handler.legacySmartOffsetConfig.customPctVeryHigh = 10;
}

void test_legacy_can760_is_read_only()
{
    configureLegacySmartSpeed(LegacySmartOffsetMode::Auto);
    observeLegacyGpsLimit(12, 40);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_TRUE(handler.legacySpeedDiag.gpsSpeedSeen);
    TEST_ASSERT_EQUAL_UINT8(12, handler.legacySpeedDiag.gpsMppLimitRaw);
    TEST_ASSERT_EQUAL_UINT16(60, handler.legacySpeedDiag.gpsMppLimitKph);
    TEST_ASSERT_EQUAL_INT(10, handler.legacySpeedDiag.gpsUserOffsetKph);
}

void test_legacy_can921_captures_fused_speed_limit()
{
    CanFrame f = {.id = 921};
    f.dlc = 2;
    f.data[1] = 12;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_UINT8(12, fusedSpeedLimitRaw);
}

void test_legacy_3ee_manual_writes_offset_after_bus_lock()
{
    configureLegacySmartSpeed(LegacySmartOffsetMode::Manual);
    handler.legacySmartOffsetConfig.manualOffsetKph = 12;
    observeLegacyGpsLimit(12);
    sendLegacySpeedMux0(CAN_BUS_PARTY, 1);
    sendLegacySpeedMux0(CAN_BUS_PARTY, 2);
    sendLegacySpeedMux0(CAN_BUS_PARTY, 3);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_PARTY, mock.sent[0].bus);
    TEST_ASSERT_EQUAL_HEX8(42, (mock.sent[0].data[3] >> 1) & 0x3F);
    TEST_ASSERT_EQUAL_HEX8(0x03, mock.sent[0].data[5] & 0x03);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_PARTY, handler.legacySpeedBus.selectedBus);
}

void test_legacy_3ee_auto_uses_2f8_limit()
{
    configureLegacySmartSpeed(LegacySmartOffsetMode::Auto);
    observeLegacyGpsLimit(12);
    sendLegacySpeedMux0(CAN_BUS_VEH, 1);
    sendLegacySpeedMux0(CAN_BUS_VEH, 2);
    sendLegacySpeedMux0(CAN_BUS_VEH, 3);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT8(30, handler.legacySpeedDiag.lastSentOffsetKph);
    TEST_ASSERT_EQUAL_UINT8(50, mock.sent[0].data[7] & 0x3F);
    TEST_ASSERT_EQUAL(LegacySpeedLimitSource::Gps2F8, handler.legacySpeedDiag.limitSource);
}

void test_legacy_3ee_custom_uses_band_percentage()
{
    configureLegacySmartSpeed(LegacySmartOffsetMode::Custom);
    handler.legacySmartOffsetConfig.customPctHigh = 10;
    observeLegacyGpsLimit(16);
    handler.legacySpeedBus.setMode(LegacySpeedBusMode::Can1);
    sendLegacySpeedMux0(CAN_BUS_VEH, 1);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT8(8, handler.legacySpeedDiag.lastSentOffsetKph);
    TEST_ASSERT_EQUAL_UINT8(10, mock.sent[0].data[7] & 0x3F);
}

void test_legacy_3ee_auto_falls_back_to_fresh_fused_limit()
{
    configureLegacySmartSpeed(LegacySmartOffsetMode::Auto);
    CanFrame das = makeLegacyNagDas(4, 0);
    das.data[1] = 16;
    handler.handleMessage(das, mock);
    handler.legacySpeedBus.setMode(LegacySpeedBusMode::Can2);
    sendLegacySpeedMux0(CAN_BUS_PARTY, 1);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL(LegacySpeedLimitSource::Fused399, handler.legacySpeedDiag.limitSource);
    TEST_ASSERT_EQUAL_UINT8(24, handler.legacySpeedDiag.lastSentOffsetKph);
}

void test_legacy_3ee_checkAD_blocks_offset()
{
    configureLegacySmartSpeed(LegacySmartOffsetMode::Auto);
    observeLegacyGpsLimit(12);
    handler.legacySpeedBus.setMode(LegacySpeedBusMode::Can1);
    handler.checkAD = denyAD;
    sendLegacySpeedMux0(CAN_BUS_VEH, 1);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL_STRING("gateBlocked", handler.legacySpeedDiag.blockedReason);
}

void test_legacy_3ee_records_tx_fail()
{
    configureLegacySmartSpeed(LegacySmartOffsetMode::Manual);
    handler.legacySmartOffsetConfig.manualOffsetKph = 10;
    handler.legacySpeedBus.setMode(LegacySpeedBusMode::Can2);
    mock.sendOk = false;
    sendLegacySpeedMux0(CAN_BUS_PARTY, 1);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacySpeedDiag.txFail);
    TEST_ASSERT_EQUAL_STRING("txFail", handler.legacySpeedDiag.blockedReason);
}

void test_legacy_3ee_mirrored_buses_fail_closed()
{
    configureLegacySmartSpeed(LegacySmartOffsetMode::Auto);
    observeLegacyGpsLimit(12);
    CanFrame can1 = {.id = 1006};
    can1.bus = CAN_BUS_VEH;
    can1.data[0] = 0;
    can1.data[1] = 9;
    CanFrame can2 = can1;
    can2.bus = CAN_BUS_PARTY;
    handler.handleMessage(can1, mock);
    handler.handleMessage(can2, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_TRUE(handler.legacySpeedBus.ambiguous);
    TEST_ASSERT_EQUAL_STRING("busAmbiguous", handler.legacySpeedDiag.blockedReason);
}

// --- CAN 1080 visionSpeedSlider override ---

void test_legacy_can1080_sets_vision_slider()
{
    handler.overrideSpeedLimit = true;
    CanFrame f = {.id = 1080};
    f.dlc = 8;
    f.data[7] = 0x80;
    const uint8_t originalByte7 = f.data[7];

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x80 | 100, mock.sent[0].data[7]);
    TEST_ASSERT_NOT_EQUAL(originalByte7, mock.sent[0].data[7]);
    TEST_ASSERT_EQUAL_HEX8(originalByte7, handler.legacyFsdDiag.aux1080.before[7]);
    TEST_ASSERT_EQUAL_HEX8(mock.sent[0].data[7], handler.legacyFsdDiag.aux1080.after[7]);
}

void test_legacy_can1080_skips_when_disabled()
{
    handler.overrideSpeedLimit = false;
    CanFrame f = {.id = 1080};
    f.dlc = 8;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_legacy_can1080_records_tx_fail()
{
    handler.overrideSpeedLimit = true;
    mock.sendOk = false;

    CanFrame f = {.id = 1080};
    f.dlc = 8;
    f.data[7] = 0x80;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyFsdDiag.aux1080.err);
    TEST_ASSERT_EQUAL(FsdSkipReason::TxFail, handler.legacyFsdDiag.aux1080.lastSkip);
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
    TEST_ASSERT_EQUAL_UINT8(12, handler.filterIdCount());
}

void test_legacy_filter_ids_values()
{
    const uint32_t *ids = handler.filterIds();
    TEST_ASSERT_EQUAL_UINT32(69, ids[0]);
    TEST_ASSERT_EQUAL_UINT32(280, ids[1]);
    TEST_ASSERT_EQUAL_UINT32(390, ids[2]);
    TEST_ASSERT_EQUAL_UINT32(760, ids[3]);
    TEST_ASSERT_EQUAL_UINT32(880, ids[4]);   // 0x370 EPAS3P_sysStatus (EPAS-faithful nag)
    TEST_ASSERT_EQUAL_UINT32(920, ids[5]);
    TEST_ASSERT_EQUAL_UINT32(921, ids[6]);
    TEST_ASSERT_EQUAL_UINT32(962, ids[7]);   // 0x3C2 dual-input recorder evidence
    TEST_ASSERT_EQUAL_UINT32(1006, ids[8]);
    TEST_ASSERT_EQUAL_UINT32(1080, ids[9]);
    TEST_ASSERT_EQUAL_UINT32(1160, ids[10]); // 0x488 steering direction source
    TEST_ASSERT_EQUAL_UINT32(CAN_ID_OTA_STATUS, ids[11]);
}

void test_legacy_488_decodes_type_angle_and_bus_without_sending()
{
    CanFrame steering = makeLegacyNagSteering(-123, CAN_BUS_PARTY, 1);
    handler.handleMessage(steering, mock);

    TEST_ASSERT_EQUAL_UINT32(0, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyNagState.rx488);
    TEST_ASSERT_TRUE(handler.legacyNagV4.steeringSeen);
    TEST_ASSERT_EQUAL_UINT8(1, handler.legacyNagV4.steeringControlType);
    TEST_ASSERT_EQUAL_INT16(-123, handler.legacyNagV4.steeringAngleX10);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_PARTY, handler.legacyNagV4.steeringBus);
    TEST_ASSERT_EQUAL_INT8(1, handler.legacyNagV4.lastSteeringDirection);
}

void test_legacy_health_no_source_when_mux0_stale()
{
    LegacyFsdDiag d;
    d.mux0.lastRxMs = 1000;
    FsdHealthState s = classifyLegacyFsdHealth(d, 7000, true);
    TEST_ASSERT_EQUAL(FsdHealthState::NoSourceFrame, s);
}

void test_legacy_health_tx_degraded_recovers_after_success()
{
    LegacyFsdDiag d;
    d.mux0.lastRxMs = 6500;
    d.mux0.err = 1;
    d.mux0.lastSkip = FsdSkipReason::TxFail;
    d.mux0.lastTxMs = 6600;

    FsdHealthState degraded = classifyLegacyFsdHealth(d, 7000, true);
    TEST_ASSERT_EQUAL(FsdHealthState::TxDegraded, degraded);

    d.mux0.tx = 1;
    d.mux0.lastSkip = FsdSkipReason::Sent;
    d.mux0.lastTxMs = 6800;
    d.health = FsdHealthState::Healthy;

    FsdHealthState recovered = classifyLegacyFsdHealth(d, 7000, true);
    TEST_ASSERT_EQUAL(FsdHealthState::Healthy, recovered);
}

void test_legacy_health_recovers_when_mux0_success_after_aux_fail()
{
    LegacyFsdDiag d;
    d.mux0.lastRxMs = 8000;
    d.aux760.lastSkip = FsdSkipReason::TxFail;
    d.aux760.lastTxMs = 8500;
    d.aux760.err = 1;

    FsdHealthState degraded = classifyLegacyFsdHealth(d, 9000, true);
    TEST_ASSERT_EQUAL(FsdHealthState::TxDegraded, degraded);

    d.mux0.lastSkip = FsdSkipReason::Sent;
    d.mux0.lastTxMs = 8900;
    d.mux0.tx = 1;
    d.health = FsdHealthState::Healthy;

    FsdHealthState recovered = classifyLegacyFsdHealth(d, 9000, true);
    TEST_ASSERT_EQUAL(FsdHealthState::Healthy, recovered);
}

void test_legacy_health_same_ms_success_then_fail_is_degraded()
{
    LegacyFsdDiag d;
    d.mux0.lastRxMs = 9000;
    d.mux0.tx = 1;
    d.mux0.lastSkip = FsdSkipReason::Sent;
    d.mux0.lastTxMs = 9000;
    d.mux0.lastTxSeq = 1;
    d.mux1.err = 1;
    d.mux1.lastSkip = FsdSkipReason::TxFail;
    d.mux1.lastTxMs = 9000;
    d.mux1.lastTxSeq = 2;
    d.health = FsdHealthState::TxDegraded;

    TEST_ASSERT_EQUAL(FsdHealthState::TxDegraded, classifyLegacyFsdHealth(d, 9001, true));
}

void test_legacy_health_same_ms_fail_then_success_is_healthy()
{
    LegacyFsdDiag d;
    d.mux0.lastRxMs = 9000;
    d.mux0.tx = 1;
    d.mux0.lastSkip = FsdSkipReason::Sent;
    d.mux0.lastTxMs = 9000;
    d.mux0.lastTxSeq = 2;
    d.mux1.err = 1;
    d.mux1.lastSkip = FsdSkipReason::TxFail;
    d.mux1.lastTxMs = 9000;
    d.mux1.lastTxSeq = 1;
    d.health = FsdHealthState::Healthy;

    TEST_ASSERT_EQUAL(FsdHealthState::Healthy, classifyLegacyFsdHealth(d, 9001, true));
}

void test_legacy_health_gate_blocked_when_last_blocked()
{
    LegacyFsdDiag d;
    d.mux0.lastRxMs = 6500;
    d.health = FsdHealthState::GateBlocked;
    FsdHealthState s = classifyLegacyFsdHealth(d, 7000, true);
    TEST_ASSERT_EQUAL(FsdHealthState::GateBlocked, s);
}

void test_legacy_nag_disabled_never_sends()
{
    CanFrame gear = makeLegacyNagGearDrive();
    handler.handleMessage(gear, mock);
    CanFrame das = makeLegacyNagDas(4, 5);
    handler.handleMessage(das, mock);
    for (uint8_t counter = 0; counter < 4; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(0, mock.sent.size());
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::Disabled, handler.legacyNagState.blocked);
}

void test_legacy_nag_learns_bus_and_sends_same_bus()
{
    enableLegacyNag(5);
    for (uint8_t counter = 0; counter < 3; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter, CAN_BUS_VEH);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_VEH, mock.sent[0].bus);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_VEH, handler.legacyNagBus.selectedBus);
}

void test_legacy_nag_echo_fields_counter_and_checksum()
{
    enableLegacyNag(5);
    for (uint8_t counter = 6; counter <= 8; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    const CanFrame &echo = mock.sent[0];
    TEST_ASSERT_EQUAL_UINT8(0, (echo.data[4] >> 6) & 0x03);
    TEST_ASSERT_EQUAL_UINT8(9, echo.data[6] & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(LegacyHandler::legacyNagChecksum(echo), echo.data[7]);
    TEST_ASSERT_GREATER_THAN_UINT16(0x0812, static_cast<uint16_t>(((echo.data[2] & 0x0F) << 8) | echo.data[3]));
}

void test_legacy_nag_does_not_mix_candidate_buses()
{
    enableLegacyNag(5);
    CanFrame veh0 = makeLegacyNagEpas(0, CAN_BUS_VEH);
    CanFrame veh1 = makeLegacyNagEpas(1, CAN_BUS_VEH);
    handler.handleMessage(veh0, mock);
    handler.handleMessage(veh1, mock);
    CanFrame party0 = makeLegacyNagEpas(0, CAN_BUS_PARTY);
    CanFrame party1 = makeLegacyNagEpas(1, CAN_BUS_PARTY);
    CanFrame party2 = makeLegacyNagEpas(2, CAN_BUS_PARTY);
    handler.handleMessage(party0, mock);
    handler.handleMessage(party1, mock);
    handler.handleMessage(party2, mock);
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_PARTY, mock.sent[0].bus);
}

void test_legacy_nag_blocks_when_ap_not_engaged()
{
    nagKillerRuntime = true;
    handler.bionicSteering = true;
    handler.resetBionic(1234);
    CanFrame gear = makeLegacyNagGearDrive();
    handler.handleMessage(gear, mock);
    CanFrame das = makeLegacyNagDas(2, 5);
    handler.handleMessage(das, mock);
    for (uint8_t counter = 0; counter < 3; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(0, mock.sent.size());
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::ApInactive, handler.legacyNagState.blocked);
}

void test_legacy_nag_status6_sends_without_drive_gear_gate()
{
    nagKillerRuntime = true;
    handler.bionicSteering = true;
    handler.resetBionic(1234);

    // Real captures 68/69: 0x399 AP status remains 6 while DAS hands demand
    // is active. No gear frame is supplied because this Legacy route no longer
    // uses the incompatible 0x118 DI_gear field as a NAG safety gate.
    CanFrame das = makeLegacyNagDas(6, 2);
    handler.handleMessage(das, mock);
    TEST_ASSERT_TRUE((bool)handler.APActive);
    TEST_ASSERT_EQUAL_UINT8(0, (uint8_t)handler.currentGear);

    for (uint16_t i = 0; i < 2200; i++)
        (void)dashDiagNowMs();
    handler.handleMessage(das, mock);
    CanFrame steering = makeLegacyNagSteering(-100, CAN_BUS_PARTY);
    handler.handleMessage(steering, mock);

    for (uint8_t counter = 1; counter <= 3; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter, CAN_BUS_PARTY);
        handler.handleMessage(epas, mock);
    }

    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyNagState.tx370);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_PARTY, mock.sent[0].bus);
    TEST_ASSERT_EQUAL_UINT8(0, (mock.sent[0].data[4] >> 6) & 0x03);
    TEST_ASSERT_EQUAL_UINT8(4, mock.sent[0].data[6] & 0x0F);
    TEST_ASSERT_EQUAL_UINT8(LegacyHandler::legacyNagChecksum(mock.sent[0]), mock.sent[0].data[7]);
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::None, handler.legacyNagState.blocked);
}

void test_legacy_nag_skips_native_hands_level_1_like_beta7()
{
    enableLegacyNag(5);
    for (uint8_t counter = 0; counter < 2; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    CanFrame echoed = makeLegacyNagEpas(2);
    echoed.data[4] = static_cast<uint8_t>((echoed.data[4] & 0x3F) | 0x40);
    echoed.data[7] = LegacyHandler::legacyNagChecksum(echoed);
    handler.handleMessage(echoed, mock);
    TEST_ASSERT_EQUAL_UINT32(0, mock.sent.size());
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::TargetHandsOn, handler.legacyNagState.blocked);
}

void test_legacy_nag_preserves_native_hands_level_3()
{
    enableLegacyNag(5);
    for (uint8_t counter = 0; counter < 2; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    CanFrame escalated = makeLegacyNagEpas(2);
    escalated.data[4] = static_cast<uint8_t>((escalated.data[4] & 0x3F) | 0xC0);
    escalated.data[7] = LegacyHandler::legacyNagChecksum(escalated);
    handler.handleMessage(escalated, mock);
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT8(3, (mock.sent[0].data[4] >> 6) & 0x03);
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::None, handler.legacyNagState.blocked);
}

void test_legacy_nag_retries_once_and_auto_disarms_after_three_failures()
{
    enableLegacyNag(5);
    mock.sendOk = false;
    for (uint8_t counter = 0; counter < 5; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(6, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(3, handler.legacyNagState.txRetryAttempts);
    TEST_ASSERT_TRUE(handler.legacyNagAutoDisarmed);
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::Degraded, handler.legacyNagState.blocked);
}

void test_legacy_nag_blocks_its_own_recent_echo()
{
    enableLegacyNag(5);
    for (uint8_t counter = 0; counter < 3; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    CanFrame echoed = mock.sent[0];
    handler.handleMessage(echoed, mock);
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyNagState.duplicateCounterObserved);
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::OwnEcho, handler.legacyNagState.blocked);
}

void test_legacy_nag_same_counter_different_payload_is_collision()
{
    enableLegacyNag(5);
    for (uint8_t counter = 0; counter < 3; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());

    CanFrame nativeCollision = mock.sent[0];
    nativeCollision.data[3] ^= 0x01;
    nativeCollision.data[7] = LegacyHandler::legacyNagChecksum(nativeCollision);
    handler.handleMessage(nativeCollision, mock);

    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(0, handler.legacyNagState.duplicateCounterObserved);
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyNagState.counterCollisionObserved);
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::CounterCollision, handler.legacyNagState.blocked);
}

void test_legacy_nag_valid_delayed_native_follower_drives_next_tx()
{
    enableLegacyNag(5);
    for (uint8_t counter = 0; counter < 3; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());

    // Vehicle captures 82-84 place the native same-counter follower 39-40 ms
    // after our TX. Advance the native test clock beyond the 20 ms boundary.
    for (uint8_t i = 0; i < 25; i++)
        (void)dashDiagNowMs();
    CanFrame nativeFollower = mock.sent[0];
    nativeFollower.data[3] ^= 0x01;
    nativeFollower.data[7] = LegacyHandler::legacyNagChecksum(nativeFollower);
    handler.handleMessage(nativeFollower, mock);

    TEST_ASSERT_EQUAL_UINT32(2, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyNagState.expectedNativeFollowerObserved);
    TEST_ASSERT_EQUAL_UINT32(0, handler.legacyNagState.counterCollisionObserved);
    TEST_ASSERT_EQUAL_UINT8(0, handler.legacyNagState.consecutiveCounterCollisions);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(LegacyHandler::kNagNativeFollowerMinMs,
                                        handler.legacyNagState.lastNativeFollowerDelayMs);
    TEST_ASSERT_EQUAL_UINT8((mock.sent[0].data[6] + 1) & 0x0F,
                            mock.sent[1].data[6] & 0x0F);
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::None, handler.legacyNagState.blocked);
}

void test_legacy_nag_native_follower_chain_remains_continuous()
{
    enableLegacyNag(5);
    for (uint8_t counter = 0; counter < 3; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());

    for (uint8_t follower = 0; follower < 10; follower++)
    {
        for (uint8_t i = 0; i < 25; i++)
            (void)dashDiagNowMs();
        CanFrame steering = makeLegacyNagSteering();
        handler.handleMessage(steering, mock);
        CanFrame nativeFollower = mock.sent.back();
        nativeFollower.data[5] ^= 0x01;
        nativeFollower.data[7] = LegacyHandler::legacyNagChecksum(nativeFollower);
        handler.handleMessage(nativeFollower, mock);
    }

    TEST_ASSERT_EQUAL_UINT32(11, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(10, handler.legacyNagState.expectedNativeFollowerObserved);
    TEST_ASSERT_EQUAL_UINT32(0, handler.legacyNagState.counterCollisionObserved);
    TEST_ASSERT_FALSE(handler.legacyNagAutoDisarmed);
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::None, handler.legacyNagState.blocked);
}

void test_legacy_nag_identical_payload_on_other_bus_is_not_own_echo()
{
    enableLegacyNag(5);
    for (uint8_t counter = 0; counter < 3; counter++)
    {
        CanFrame epas = makeLegacyNagEpas(counter, CAN_BUS_VEH);
        handler.handleMessage(epas, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());

    CanFrame otherBus = mock.sent[0];
    otherBus.bus = CAN_BUS_PARTY;
    handler.handleMessage(otherBus, mock);

    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(0, handler.legacyNagState.duplicateCounterObserved);
    TEST_ASSERT_NOT_EQUAL(LegacyNagBlockReason::OwnEcho, handler.legacyNagState.blocked);
}

void test_legacy_nag_auto_disarms_after_three_counter_collisions()
{
    enableLegacyNag(5);
    uint8_t nativeCounter = 0;
    for (uint8_t collision = 0; collision < 3; collision++)
    {
        for (uint8_t attempts = 0; mock.sent.size() <= collision && attempts < 8; attempts++)
        {
            CanFrame native = makeLegacyNagEpas(nativeCounter++);
            handler.handleMessage(native, mock);
        }
        TEST_ASSERT_GREATER_THAN_UINT32(collision, mock.sent.size());
        CanFrame conflictingNative = mock.sent[collision];
        conflictingNative.data[3] ^= 0x01;
        conflictingNative.data[7] = LegacyHandler::legacyNagChecksum(conflictingNative);
        handler.handleMessage(conflictingNative, mock);
        nativeCounter = static_cast<uint8_t>((handler.legacyNagState.txCounter + 1) & 0x0F);
    }

    TEST_ASSERT_EQUAL_UINT32(3, handler.legacyNagState.counterCollisionObserved);
    TEST_ASSERT_EQUAL_UINT8(3, handler.legacyNagState.consecutiveCounterCollisions);
    TEST_ASSERT_TRUE(handler.legacyNagAutoDisarmed);
    TEST_ASSERT_TRUE(handler.legacyNagState.degraded);
    TEST_ASSERT_EQUAL(LegacyNagBlockReason::Degraded, handler.legacyNagState.blocked);
}

void test_legacy_nag_replays_real_capture_with_counter_gaps()
{
    enableLegacyNag(5);
    static const uint8_t capture[3][8] = {
        {0x52, 0x08, 0x08, 0x0F, 0x20, 0x2C, 0x25, 0x55},
        {0x52, 0x04, 0x08, 0x0A, 0x20, 0x2C, 0x27, 0x4E},
        {0x52, 0x04, 0x08, 0x0A, 0x20, 0x2D, 0x29, 0x51},
    };
    for (const auto &bytes : capture)
    {
        CanFrame frame = {.id = 880};
        frame.bus = CAN_BUS_VEH;
        memcpy(frame.data, bytes, 8);
        TEST_ASSERT_EQUAL_UINT8(LegacyHandler::legacyNagChecksum(frame), frame.data[7]);
        handler.handleMessage(frame, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(1, mock.sent.size());
    const CanFrame &echo = mock.sent[0];
    TEST_ASSERT_EQUAL_UINT8(0x2A, echo.data[6]);
    TEST_ASSERT_EQUAL_UINT8(LegacyHandler::legacyNagChecksum(echo), echo.data[7]);
    TEST_ASSERT_EQUAL_UINT8(0, (echo.data[4] >> 6) & 0x03);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_legacy_filter_ids_count);
    RUN_TEST(test_legacy_filter_ids_values);
    RUN_TEST(test_legacy_488_decodes_type_angle_and_bus_without_sending);
    RUN_TEST(test_legacy_health_no_source_when_mux0_stale);
    RUN_TEST(test_legacy_health_tx_degraded_recovers_after_success);
    RUN_TEST(test_legacy_health_recovers_when_mux0_success_after_aux_fail);
    RUN_TEST(test_legacy_health_same_ms_success_then_fail_is_degraded);
    RUN_TEST(test_legacy_health_same_ms_fail_then_success_is_healthy);
    RUN_TEST(test_legacy_health_gate_blocked_when_last_blocked);
    RUN_TEST(test_legacy_nag_disabled_never_sends);
    RUN_TEST(test_legacy_nag_learns_bus_and_sends_same_bus);
    RUN_TEST(test_legacy_nag_echo_fields_counter_and_checksum);
    RUN_TEST(test_legacy_nag_does_not_mix_candidate_buses);
    RUN_TEST(test_legacy_nag_blocks_when_ap_not_engaged);
    RUN_TEST(test_legacy_nag_status6_sends_without_drive_gear_gate);
    RUN_TEST(test_legacy_nag_skips_native_hands_level_1_like_beta7);
    RUN_TEST(test_legacy_nag_preserves_native_hands_level_3);
    RUN_TEST(test_legacy_nag_retries_once_and_auto_disarms_after_three_failures);
    RUN_TEST(test_legacy_nag_blocks_its_own_recent_echo);
    RUN_TEST(test_legacy_nag_same_counter_different_payload_is_collision);
    RUN_TEST(test_legacy_nag_valid_delayed_native_follower_drives_next_tx);
    RUN_TEST(test_legacy_nag_native_follower_chain_remains_continuous);
    RUN_TEST(test_legacy_nag_identical_payload_on_other_bus_is_not_own_echo);
    RUN_TEST(test_legacy_nag_auto_disarms_after_three_counter_collisions);
    RUN_TEST(test_legacy_nag_replays_real_capture_with_counter_gaps);

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
    RUN_TEST(test_legacy_activation_callback_blocks_mux0_send);
    RUN_TEST(test_legacy_fsdTriggered_set_on_mux0);

    RUN_TEST(test_legacy_stable_mux1_is_disabled);
    RUN_TEST(test_legacy_experimental_mux0_can_write_speed_profile);
    RUN_TEST(test_legacy_experimental_mux1_can_clear_bit19);
    RUN_TEST(test_legacy_tesla_parity_mux0_writes_speed_profile);
    RUN_TEST(test_legacy_tesla_parity_mux1_clears_bit19);
    RUN_TEST(test_legacy_vision_clear_works_under_tesla_parity_policy);
    RUN_TEST(test_legacy_vision_clear_disabled_does_not_clear_bit48);
    RUN_TEST(test_legacy_trigger_source_force_vs_ui_bit_vs_false);
    RUN_TEST(test_legacy_mux0_records_can_a_twai_labels);
    RUN_TEST(test_legacy_gate_block_records_gate_blocked_health);
    RUN_TEST(test_legacy_gate_block_uses_resolver_reason);
    RUN_TEST(test_abort_guard_latches_on_abort_and_only_clears_after_clean_disengage);
    RUN_TEST(test_abort_guard_blocks_legacy_mux0_before_transmit);
    RUN_TEST(test_minimal_inject_superseded_by_steer_defense_placeholder);

    RUN_TEST(test_legacy_can760_is_read_only);
    RUN_TEST(test_legacy_can921_captures_fused_speed_limit);
    RUN_TEST(test_legacy_3ee_manual_writes_offset_after_bus_lock);
    RUN_TEST(test_legacy_3ee_auto_uses_2f8_limit);
    RUN_TEST(test_legacy_3ee_custom_uses_band_percentage);
    RUN_TEST(test_legacy_3ee_auto_falls_back_to_fresh_fused_limit);
    RUN_TEST(test_legacy_3ee_checkAD_blocks_offset);
    RUN_TEST(test_legacy_3ee_records_tx_fail);
    RUN_TEST(test_legacy_3ee_mirrored_buses_fail_closed);
    RUN_TEST(test_legacy_can1080_sets_vision_slider);
    RUN_TEST(test_legacy_can1080_skips_when_disabled);
    RUN_TEST(test_legacy_can1080_records_tx_fail);

    RUN_TEST(test_legacy_ignores_unrelated_can_id);

    return UNITY_END();
}
