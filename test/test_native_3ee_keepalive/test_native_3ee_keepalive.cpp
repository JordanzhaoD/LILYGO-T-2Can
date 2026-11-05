#include <unity.h>
#include <cstring>

#include "dash_3ee_keepalive.h"

// 2026-11-05/06 field root cause (see ledger): the car's UI computer
// duty-cycles 0x3EE outside active driving — garage recon sessions sat
// 30-62% of wall-clock in >2s native silence and the Nov-5 drive capture
// flipped from a continuous 2Hz carrier (max gap 964ms) to six 2-5s gaps
// across its parking endgame. The tail-ride TX starves with the carrier and
// the car-side consumer ages out bit46 + the speed offset (FSD UI flicker).
// These tests pin the pure timing policy of the keep-alive state machine:
// silence trigger, cadence, template expiry, instant native yield, per-page
// independence, and gate handling. Payload patching / gate mirroring live in
// LegacyHandler::keepAliveTick and are pinned in test_native_legacy.

static Dash3eeKeepAlive ka;

static const uint8_t kMux0Native[8] = {0x00, 0x00, 0x00, 0xD0, 0x20, 0x00, 0x02, 0x80};
static const uint8_t kMux1Native[8] = {0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x05, 0x80};

void setUp()
{
    ka = Dash3eeKeepAlive{};
}

void tearDown() {}

// ── Silence trigger ────────────────────────────────────────────────────────

void test_silence_below_threshold_no_action()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    // 1799ms of silence: still under kSilenceTriggerMs (1800).
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(2799, true, true));
}

void test_silence_at_threshold_fires_mux0_first()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    // Exactly 1800ms of silence: eligible (>= boundary).
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux0,
                          (int)ka.tick(2800, true, true));
}

void test_mux1_follows_on_next_tick()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux0,
                          (int)ka.tick(2800, true, true));
    // mux0's cadence timer now holds it back; mux1 fires immediately.
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux1,
                          (int)ka.tick(2801, true, true));
}

// ── Cadence ────────────────────────────────────────────────────────────────

void test_cadence_boundary_899ms_blocked_900ms_eligible()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    (void)ka.tick(2800, true, true); // mux0 keep-alive issued at 2800
    (void)ka.tick(2801, true, true); // mux1 issued at 2801
    // 2800+899: cadence not yet due for mux0 (and mux1 issued 1ms ago).
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(3699, true, true));
    // 2800+900: mux0 eligible again.
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux0,
                          (int)ka.tick(3700, true, true));
}

// ── Native yield ───────────────────────────────────────────────────────────

void test_native_return_yields_instantly()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    (void)ka.tick(2800, true, true); // keep-alive mux0 sent
    // Native carrier returns; the tail-ride owns the stream again.
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 2810);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 2811);
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(4000, true, true));
}

void test_native_blip_restarts_the_silence_window()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    // A native 1500ms in resets the silence clock: at 1500+1799 no fire.
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 2500);
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(4299, true, true));
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux0,
                          (int)ka.tick(4300, true, true));
}

// ── Template expiry (fail-closed) ─────────────────────────────────────────

void test_template_expiry_at_10s_fail_closed()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    // 9999ms of silence: the last eligible instant (mux0 fires; mux1 would
    // follow but the next tick crosses the boundary).
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux0,
                          (int)ka.tick(10999, true, true));
    // 10000ms after the last native: template stale, fail-closed for both.
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(11000, true, true));
}

void test_template_expired_blocks_both_pages()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    // 10000ms after the last native: template stale, no keep-alive ever.
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(11000, true, true));
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(20000, true, true));
}

// ── No template ────────────────────────────────────────────────────────────

void test_never_seen_native_never_fires()
{
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(100000, true, true));
}

// ── Gates ─────────────────────────────────────────────────────────────────

void test_gates_closed_blocks_without_consuming_cadence()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(5000, false, false));
    // Gates reopen: fires immediately (cadence was never consumed).
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux0,
                          (int)ka.tick(5001, true, true));
}

void test_page_specific_gate()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    // mux0 gate closed, mux1 open: skip mux0 (no cadence consumption), fire mux1.
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux1,
                          (int)ka.tick(2800, false, true));
    // Now mux0 opens: fires on the next tick (its cadence untouched).
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux0,
                          (int)ka.tick(2801, true, true));
}

// ── Per-page independence ─────────────────────────────────────────────────

void test_mux0_silent_mux1_alive_keeps_mux0_quiet()
{
    // Partial duty-cycle: mux0 went silent, mux1 still streaming.
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    for (uint32_t t = 1500; t <= 5000; t += 500)
        ka.observeMux1(kMux1Native, CAN_BUS_VEH, t);
    // mux0 is 4000ms silent (>= trigger), mux1 is fresh.
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux0,
                          (int)ka.tick(5000, true, true));
    // mux1 just observed at 5000: silence 0 -> no keep-alive.
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(5001, true, true));
}

void test_mux1_only_template_fires_mux1()
{
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::SendMux1,
                          (int)ka.tick(2800, true, true));
}

// ── Template bytes + bus ──────────────────────────────────────────────────

void test_template_bytes_and_bus_round_trip()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_CH, 1000);
    uint8_t out[8] = {0};
    TEST_ASSERT_TRUE(ka.copyMux0Template(out));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kMux0Native, out, 8);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_VEH, ka.mux0NativeBus);
    TEST_ASSERT_TRUE(ka.copyMux1Template(out));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(kMux1Native, out, 8);
    TEST_ASSERT_EQUAL_UINT8(CAN_BUS_CH, ka.mux1NativeBus);
}

void test_no_template_copy_returns_false()
{
    uint8_t out[8] = {0};
    TEST_ASSERT_FALSE(ka.copyMux0Template(out));
    TEST_ASSERT_FALSE(ka.copyMux1Template(out));
}

void test_template_refreshes_with_latest_native()
{
    uint8_t newer[8];
    memcpy(newer, kMux0Native, 8);
    newer[3] = 0xE0; // car changed a field
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux0(newer, CAN_BUS_VEH, 2000);
    uint8_t out[8] = {0};
    TEST_ASSERT_TRUE(ka.copyMux0Template(out));
    TEST_ASSERT_EQUAL_UINT8(0xE0, out[3]);
}

// ── Time-domain safety ────────────────────────────────────────────────────

void test_backward_time_never_fires()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 5000);
    // Defensive: a backward clock must not produce a fireable silence.
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(4000, true, true));
}

void test_equal_time_no_fire()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 5000);
    TEST_ASSERT_EQUAL_INT((int)Dash3eeKeepAlive::Action::None,
                          (int)ka.tick(5000, true, true));
}

// ── Counters ──────────────────────────────────────────────────────────────

void test_keep_alive_counters_track_issuance()
{
    ka.observeMux0(kMux0Native, CAN_BUS_VEH, 1000);
    ka.observeMux1(kMux1Native, CAN_BUS_VEH, 1000);
    (void)ka.tick(2800, true, true); // mux0
    (void)ka.tick(2801, true, true); // mux1
    (void)ka.tick(3700, true, true); // mux0 again
    TEST_ASSERT_EQUAL_UINT32(2, ka.mux0.keepAlives);
    TEST_ASSERT_EQUAL_UINT32(1, ka.mux1.keepAlives);
    TEST_ASSERT_EQUAL_UINT32(3700, ka.mux0.lastKeepAliveMs);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_silence_below_threshold_no_action);
    RUN_TEST(test_silence_at_threshold_fires_mux0_first);
    RUN_TEST(test_mux1_follows_on_next_tick);
    RUN_TEST(test_cadence_boundary_899ms_blocked_900ms_eligible);
    RUN_TEST(test_native_return_yields_instantly);
    RUN_TEST(test_native_blip_restarts_the_silence_window);
    RUN_TEST(test_template_expiry_at_10s_fail_closed);
    RUN_TEST(test_template_expired_blocks_both_pages);
    RUN_TEST(test_never_seen_native_never_fires);
    RUN_TEST(test_gates_closed_blocks_without_consuming_cadence);
    RUN_TEST(test_page_specific_gate);
    RUN_TEST(test_mux0_silent_mux1_alive_keeps_mux0_quiet);
    RUN_TEST(test_mux1_only_template_fires_mux1);
    RUN_TEST(test_template_bytes_and_bus_round_trip);
    RUN_TEST(test_no_template_copy_returns_false);
    RUN_TEST(test_template_refreshes_with_latest_native);
    RUN_TEST(test_backward_time_never_fires);
    RUN_TEST(test_equal_time_no_fire);
    RUN_TEST(test_keep_alive_counters_track_issuance);
    return UNITY_END();
}
