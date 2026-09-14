#include <unity.h>

#include "dash_ota_guard.h"

// 4.5.0-beta12 (LittleGong port 1): unit tests for the three-path OTA guard
// rework. The permit expression that consumes dashOtaGuardAllowInjection() is
// pinned VERBATIM elsewhere (contract) — every behavioral change lives in
// this state machine's set/clear conditions:
//   A. 6 consecutive non-OTA 0x318 frames clear the flag (original path,
//      kept byte-for-byte in spirit);
//   B. stream-stall timeout: flag set for >= OTA_STALE_TIMEOUT_MS with zero
//      0x318 frames in between (any dlc counts as carrier-alive) →
//      dashOtaGuardTick degrades, staleClears++;
//   C. user override: on = clear immediately + suspend setting; off = clean
//      restart (a real OTA is re-captured by the 3-frame confirmation).
// All tests drive the guard with an EXPLICIT discrete clock (nowMs) — no
// millis() dependency, so timeout boundaries are exact.

void setUp()
{
    vehicleOtaActive = false;
    otaConfirmCount = 0;
    otaClearCount = 0;
    otaGuardOverride = false;
    otaLastSeenMs = 0;
    otaActiveSinceMs = 0;
    otaStaleClears = 0;
}

void tearDown() {}

static CanFrame mk318(uint8_t dlc = 8, uint8_t b6 = 0x02)
{
    CanFrame f;
    f.id = CAN_ID_OTA_STATUS; // 0x318
    f.dlc = dlc;
    for (uint8_t i = 0; i < 8; ++i)
        f.data[i] = 0;
    if (dlc >= 7)
        f.data[6] = b6;
    f.bus = CAN_BUS_ANY;
    return f;
}

static CanFrame mkOther(uint32_t id = 0x123)
{
    CanFrame f;
    f.id = id;
    f.dlc = 8;
    f.bus = CAN_BUS_ANY;
    return f;
}

// Feed `n` OTA-flagged (0x02) frames at 100ms spacing starting at t0.
static void feedOta(uint32_t t0, uint8_t n)
{
    for (uint8_t i = 0; i < n; ++i)
        dashOtaGuardProcessFrame(mk318(8, 0x02), t0 + i * 100);
}

// Feed `n` non-OTA (0x00) 0x318 frames at 100ms spacing starting at t0.
static void feedClear(uint32_t t0, uint8_t n)
{
    for (uint8_t i = 0; i < n; ++i)
        dashOtaGuardProcessFrame(mk318(8, 0x00), t0 + i * 100);
}

// ── Path A: confirmation and 6-frame clear (original behavior) ─────────────

void test_fresh_guard_allows_injection()
{
    TEST_ASSERT_TRUE(dashOtaGuardAllowInjection());
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardStaleClears());
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardLastSeenMs());
}

void test_two_ota_frames_do_not_set_flag()
{
    feedOta(1000, 2);
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
    TEST_ASSERT_TRUE(dashOtaGuardAllowInjection());
}

void test_three_ota_frames_set_flag_with_since_timestamp()
{
    feedOta(1000, 3);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    TEST_ASSERT_FALSE(dashOtaGuardAllowInjection());
    // ActiveSinceMs = the moment the 3rd confirming frame arrived.
    TEST_ASSERT_EQUAL_UINT32(1200, dashOtaGuardActiveSinceMs());
}

void test_interleaved_clear_frame_resets_confirmation()
{
    feedOta(1000, 2);
    dashOtaGuardProcessFrame(mk318(8, 0x00), 1200); // breaks the streak
    feedOta(1300, 2);
    TEST_ASSERT_FALSE(dashOtaGuardStatus()); // 2 fresh frames, not 3
    feedOta(1500, 1);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
}

void test_five_clear_frames_do_not_clear_flag()
{
    feedOta(1000, 3);
    feedClear(2000, 5);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
}

void test_six_clear_frames_clear_flag_path_a()
{
    feedOta(1000, 3);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    feedClear(2000, 6);
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
    TEST_ASSERT_TRUE(dashOtaGuardAllowInjection());
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardActiveSinceMs());
    // Path A never touches staleClears.
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardStaleClears());
}

void test_ota_frame_resets_clear_streak()
{
    feedOta(1000, 3);
    feedClear(2000, 5);
    dashOtaGuardProcessFrame(mk318(8, 0x02), 3000); // keeps it alive
    feedClear(3100, 5);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
}

void test_non_318_frames_fully_ignored()
{
    dashOtaGuardProcessFrame(mkOther(), 1000);
    dashOtaGuardProcessFrame(mkOther(0x317), 1100);
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardLastSeenMs());
    feedOta(1200, 2);
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
}

void test_short_dlc_stamps_last_seen_but_never_judges_flag()
{
    // dlc < 7 cannot carry the OTA flag in data[6]: the guard must neither
    // confirm nor clear on it — but it still proves the 0x318 carrier is
    // alive (lastSeen stamp, the path-B input).
    feedOta(1000, 3);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    for (uint8_t i = 0; i < 10; ++i)
        dashOtaGuardProcessFrame(mk318(3, 0x00), 2000 + i * 100);
    // Not treated as clear frames:
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    // But the carrier is proven alive at the last short frame:
    TEST_ASSERT_EQUAL_UINT32(2900, dashOtaGuardLastSeenMs());
    // And short frames alone never activate:
    setUp();
    for (uint8_t i = 0; i < 10; ++i)
        dashOtaGuardProcessFrame(mk318(3, 0x02), 1000 + i * 100);
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
}

// ── Path B: stream-stall timeout degradation ───────────────────────────────

void test_tick_is_noop_while_inactive()
{
    TEST_ASSERT_FALSE(dashOtaGuardTick(10 * OTA_STALE_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardStaleClears());
}

void test_tick_never_fires_without_any_318_seen()
{
    // Defensive edge: active without ever having seen 0x318 (not reachable
    // via ProcessFrame alone, but the guard must not guess a stall with no
    // lastSeen baseline).
    vehicleOtaActive = true;
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardLastSeenMs());
    TEST_ASSERT_FALSE(dashOtaGuardTick(10 * OTA_STALE_TIMEOUT_MS));
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
}

void test_stale_timeout_boundary_599999_does_not_clear()
{
    feedOta(1000, 3); // lastSeen = 1200
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    TEST_ASSERT_FALSE(dashOtaGuardTick(1200 + 599999));
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardStaleClears());
}

void test_stale_timeout_boundary_600000_clears_and_counts_once()
{
    feedOta(1000, 3); // lastSeen = 1200
    TEST_ASSERT_TRUE(dashOtaGuardTick(1200 + 600000));
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
    TEST_ASSERT_TRUE(dashOtaGuardAllowInjection());
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardActiveSinceMs());
    TEST_ASSERT_EQUAL_UINT32(1, dashOtaGuardStaleClears());
    // Idempotent: subsequent ticks do not double-count.
    TEST_ASSERT_FALSE(dashOtaGuardTick(1200 + 700000));
    TEST_ASSERT_EQUAL_UINT32(1, dashOtaGuardStaleClears());
    // Counters were reset: a fresh OTA needs a full 3-frame confirmation.
    TEST_ASSERT_EQUAL_UINT8(0, otaConfirmCount);
    feedOta(1'000'000, 2);
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
    feedOta(1'000'200, 1);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
}

void test_live_318_stream_prevents_stale_clear()
{
    // Symptom-2/3 discriminator: if 0x318 keeps flowing AND reports OTA, the
    // guard must keep blocking — only a true carrier stall degrades.
    feedOta(1000, 3);
    for (uint32_t t = 2000; t <= 700000; t += 60000)
        dashOtaGuardProcessFrame(mk318(8, 0x02), t); // OTA still streaming
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    TEST_ASSERT_FALSE(dashOtaGuardTick(700000));
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardStaleClears());
}

void test_live_non_ota_318_also_prevents_stale_clear()
{
    // Carrier alive but never reaching 6 consecutive non-OTA frames (5
    // spread across the window): path A cannot fire, and path B must NOT
    // either — only a true carrier stall degrades. Path A owns the
    // streaming-non-OTA transition.
    feedOta(1000, 3);
    dashOtaGuardProcessFrame(mk318(8, 0x00), 2000);
    dashOtaGuardProcessFrame(mk318(8, 0x00), 140000);
    dashOtaGuardProcessFrame(mk318(8, 0x00), 280000);
    dashOtaGuardProcessFrame(mk318(8, 0x00), 420000);
    dashOtaGuardProcessFrame(mk318(8, 0x00), 560000);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    TEST_ASSERT_EQUAL_UINT32(560000, dashOtaGuardLastSeenMs());
    TEST_ASSERT_FALSE(dashOtaGuardTick(570000));
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardStaleClears());
}

void test_short_dlc_carrier_keeps_flag_alive_through_timeout()
{
    // Any dlc counts as carrier-alive: short 0x318 frames arriving forever
    // must keep the (already active) guard from stale-clearing.
    feedOta(1000, 3);
    for (uint32_t t = 2000; t <= 700000; t += 60000)
        dashOtaGuardProcessFrame(mk318(2, 0x00), t);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    TEST_ASSERT_FALSE(dashOtaGuardTick(700000));
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardStaleClears());
}

// ── Path C: user override ──────────────────────────────────────────────────

void test_override_on_clears_flag_immediately()
{
    feedOta(1000, 3);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    dashOtaGuardSetOverride(true);
    TEST_ASSERT_TRUE(dashOtaGuardOverrideEnabled());
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
    TEST_ASSERT_TRUE(dashOtaGuardAllowInjection());
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardActiveSinceMs());
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardStaleClears()); // not a stale clear
}

void test_override_suspends_setting_while_on()
{
    dashOtaGuardSetOverride(true);
    feedOta(1000, 10); // a wall of OTA frames
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
    TEST_ASSERT_EQUAL_UINT8(0, otaConfirmCount); // counters stay clean
    // Tick under override is a no-op even past the timeout.
    TEST_ASSERT_FALSE(dashOtaGuardTick(1000 + OTA_STALE_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_UINT32(0, dashOtaGuardStaleClears());
}

void test_override_off_restarts_clean_and_recaptures_real_ota()
{
    dashOtaGuardSetOverride(true);
    feedOta(1000, 10);
    dashOtaGuardSetOverride(false);
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
    TEST_ASSERT_EQUAL_UINT8(0, otaConfirmCount);
    // A REAL OTA after override-off is still captured — the guard is a
    // safety net, override does not blind it permanently.
    feedOta(5000, 3);
    TEST_ASSERT_TRUE(dashOtaGuardStatus());
    TEST_ASSERT_FALSE(dashOtaGuardAllowInjection());
    // And that fresh activation still stale-degrades (paths compose).
    TEST_ASSERT_TRUE(dashOtaGuardTick(5200 + OTA_STALE_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_UINT32(1, dashOtaGuardStaleClears());
}

void test_override_on_then_off_does_not_resurrect_old_flag()
{
    feedOta(1000, 3);
    dashOtaGuardSetOverride(true);
    dashOtaGuardSetOverride(false);
    // Nothing re-activates without fresh confirmation frames.
    TEST_ASSERT_FALSE(dashOtaGuardStatus());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_fresh_guard_allows_injection);
    RUN_TEST(test_two_ota_frames_do_not_set_flag);
    RUN_TEST(test_three_ota_frames_set_flag_with_since_timestamp);
    RUN_TEST(test_interleaved_clear_frame_resets_confirmation);
    RUN_TEST(test_five_clear_frames_do_not_clear_flag);
    RUN_TEST(test_six_clear_frames_clear_flag_path_a);
    RUN_TEST(test_ota_frame_resets_clear_streak);
    RUN_TEST(test_non_318_frames_fully_ignored);
    RUN_TEST(test_short_dlc_stamps_last_seen_but_never_judges_flag);
    RUN_TEST(test_tick_is_noop_while_inactive);
    RUN_TEST(test_tick_never_fires_without_any_318_seen);
    RUN_TEST(test_stale_timeout_boundary_599999_does_not_clear);
    RUN_TEST(test_stale_timeout_boundary_600000_clears_and_counts_once);
    RUN_TEST(test_live_318_stream_prevents_stale_clear);
    RUN_TEST(test_live_non_ota_318_also_prevents_stale_clear);
    RUN_TEST(test_short_dlc_carrier_keeps_flag_alive_through_timeout);
    RUN_TEST(test_override_on_clears_flag_immediately);
    RUN_TEST(test_override_suspends_setting_while_on);
    RUN_TEST(test_override_off_restarts_clean_and_recaptures_real_ota);
    RUN_TEST(test_override_on_then_off_does_not_resurrect_old_flag);
    return UNITY_END();
}
