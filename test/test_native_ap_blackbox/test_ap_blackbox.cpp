#include <unity.h>

#include <stdio.h>
#include <string.h>

#include "dash_ap_blackbox.h"

// 4.5.0-beta10 P1-4: unit tests for the NVS flight recorder that surrounds
// the 8.3.6 coordinator. The coordinator module itself is untouched in
// beta10; these tests pin the blackbox contract the /status JSON and the
// main.cpp tick wiring rely on: stack order (entries()[0] = NEWEST),
// capacity drop of the oldest, per-kind flap guard, seq monotonicity across
// load(), blob round-trip, and reason truncation.
//
// 4.5.0-beta12 (LittleGong port 5): golden-string tests for
// dashApBbEntryToJson. Three releases (beta06/beta07/beta10) shipped
// quote-closure bugs in this exact JSON because it was assembled inline in
// the dashboard header, which native tests never compile. The emission now
// lives here in dash_ap_blackbox.h, and these tests pin every kind — with
// and without a reason — character by character before firmware ever links.

static DashApBlackbox box;

static void wipe()
{
    box.clear();
}

void setUp()
{
    wipe();
}

void tearDown() {}

static void add(DashApBbKind kind, uint32_t ms, uint16_t tick = 0,
                uint8_t bits = 0x1F, const char *reason = "")
{
    box.record(kind, 0, reason, bits, 3, ms, tick);
}

void test_fresh_box_is_empty()
{
    TEST_ASSERT_EQUAL_UINT8(0, box.count());
}

void test_first_record_lands_at_index_zero_as_newest()
{
    TEST_ASSERT_TRUE(box.record(DashApBbKind::Boot, 0, "", 0, 0, 42, 0));
    TEST_ASSERT_EQUAL_UINT8(1, box.count());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DashApBbKind::Boot),
                            box.entries()[0].kind);
    TEST_ASSERT_EQUAL_UINT32(42, box.entries()[0].ms);
    TEST_ASSERT_EQUAL_UINT16(1, box.entries()[0].seq);
}

void test_newer_records_stack_above_older()
{
    add(DashApBbKind::Heartbeat, 1000);
    add(DashApBbKind::RoundStart, 2000);
    add(DashApBbKind::RoundEnd, 3000, 0, 0x1F, "windowExpired");
    TEST_ASSERT_EQUAL_UINT8(3, box.count());
    // entries()[0] = newest, entries()[count-1] = oldest
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DashApBbKind::RoundEnd),
                            box.entries()[0].kind);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DashApBbKind::Heartbeat),
                            box.entries()[2].kind);
    TEST_ASSERT_EQUAL_STRING("windowExpired", box.entries()[0].reason);
}

void test_full_box_drops_oldest_and_keeps_capacity()
{
    for (uint32_t i = 1; i <= 12; ++i)
        add(DashApBbKind::Heartbeat, i * 1000);
    TEST_ASSERT_EQUAL_UINT8(DashApBlackbox::kCapacity, box.count());
    // Newest retained = the 12th; oldest retained = the 5th.
    TEST_ASSERT_EQUAL_UINT32(12000, box.entries()[0].ms);
    TEST_ASSERT_EQUAL_UINT32(5000, box.entries()[box.count() - 1].ms);
}

void test_seq_monotonic_across_wrap()
{
    for (uint32_t i = 1; i <= 12; ++i)
        add(DashApBbKind::Heartbeat, i * 1000);
    // Newest ([0]) carries the largest seq; older entries strictly smaller.
    // Unity LESS_THAN(threshold, actual): assert entries[i].seq < entries[i-1].seq.
    for (uint8_t i = 1; i < box.count(); ++i)
        TEST_ASSERT_LESS_THAN(box.entries()[i - 1].seq, box.entries()[i].seq);
    TEST_ASSERT_EQUAL_UINT16(12, box.entries()[0].seq);
}

void test_flap_guard_drops_same_kind_within_window()
{
    TEST_ASSERT_TRUE(box.record(DashApBbKind::PermitChange, 1, "", 0x1E, 3,
                                10'000, 5));
    // 3s later, same kind -> dropped by the 5s flap guard.
    TEST_ASSERT_FALSE(box.record(DashApBbKind::PermitChange, 1, "", 0x1C, 3,
                                 13'000, 6));
    TEST_ASSERT_EQUAL_UINT8(1, box.count());
    // 6s after the first (outside the window) -> recorded.
    TEST_ASSERT_TRUE(box.record(DashApBbKind::PermitChange, 1, "", 0x1C, 3,
                                16'000, 7));
    TEST_ASSERT_EQUAL_UINT8(2, box.count());
}

void test_flap_guard_tracks_permit_and_intent_independently()
{
    TEST_ASSERT_TRUE(box.record(DashApBbKind::PermitChange, 1, "", 0x1E, 3,
                                10'000, 5));
    // Different flappy kind, same instant -> NOT suppressed.
    TEST_ASSERT_TRUE(box.record(DashApBbKind::IntentChange, 1, "", 0x1E, 3,
                                10'000, 5));
    TEST_ASSERT_EQUAL_UINT8(2, box.count());
}

void test_non_flappy_kinds_are_never_suppressed()
{
    for (uint32_t i = 0; i < 4; ++i)
        add(DashApBbKind::RoundEnd, 1000 + i, 0, 0x1F, "windowExpired");
    TEST_ASSERT_EQUAL_UINT8(4, box.count());
}

void test_dump_load_round_trip_preserves_entries_and_seq()
{
    add(DashApBbKind::Heartbeat, 1000);
    add(DashApBbKind::RoundStart, 2000);
    add(DashApBbKind::Lock, 3000, 0, 0x1F, "counterConflict");
    uint8_t blob[sizeof(DashApBbEntry) * DashApBlackbox::kCapacity];
    const size_t len = box.dump(blob, sizeof(blob));
    TEST_ASSERT_EQUAL_size_t(box.storageBytes(), len);

    DashApBlackbox restored;
    restored.load(blob, len);
    TEST_ASSERT_EQUAL_UINT8(box.count(), restored.count());
    for (uint8_t i = 0; i < box.count(); ++i)
    {
        TEST_ASSERT_EQUAL_UINT16(box.entries()[i].seq, restored.entries()[i].seq);
        TEST_ASSERT_EQUAL_UINT8(box.entries()[i].kind, restored.entries()[i].kind);
        TEST_ASSERT_EQUAL_UINT32(box.entries()[i].ms, restored.entries()[i].ms);
    }
    // seq numbering resumes AFTER the restored newest entry.
    TEST_ASSERT_TRUE(restored.record(DashApBbKind::Boot, 0, "", 0, 0, 4000, 0));
    TEST_ASSERT_EQUAL_UINT16(box.entries()[0].seq + 1,
                             restored.entries()[0].seq);
}

void test_load_rejects_wrong_length_blob()
{
    uint8_t junk[7] = {1, 2, 3, 4, 5, 6, 7};
    DashApBlackbox restored;
    restored.load(junk, sizeof(junk));
    TEST_ASSERT_EQUAL_UINT8(0, restored.count());
}

void test_load_null_is_ignored()
{
    DashApBlackbox restored;
    restored.load(nullptr, 0);
    TEST_ASSERT_EQUAL_UINT8(0, restored.count());
}

void test_reason_truncation_length()
{
    // 20 chars: must truncate to 15 chars + NUL, no overflow, stable prefix.
    TEST_ASSERT_TRUE(box.record(DashApBbKind::RoundEnd, 7,
                                "apLeftInjectionSet22",
                                0x1F, 3, 1000, 0));
    const DashApBbEntry &e = box.entries()[0];
    TEST_ASSERT_EQUAL_UINT8(15, strlen(e.reason));
    TEST_ASSERT_EQUAL_CHAR('\0', e.reason[15]);
    TEST_ASSERT_EQUAL_STRING("apLeftInjection", e.reason);
}

void test_invalid_kind_rejected()
{
    TEST_ASSERT_FALSE(box.record(DashApBbKind::None, 0, "", 0x1F, 3, 1000, 0));
    TEST_ASSERT_FALSE(box.record(static_cast<DashApBbKind>(9), 0, "", 0x1F,
                                 3, 1000, 0));
    TEST_ASSERT_EQUAL_UINT8(0, box.count());
}

void test_clear_resets_everything()
{
    add(DashApBbKind::Heartbeat, 1000);
    add(DashApBbKind::RoundStart, 2000);
    box.clear();
    TEST_ASSERT_EQUAL_UINT8(0, box.count());
    // After clear, seq restarts at 1 (fresh boot semantics).
    TEST_ASSERT_TRUE(box.record(DashApBbKind::Boot, 0, "", 0, 0, 3000, 0));
    TEST_ASSERT_EQUAL_UINT16(1, box.entries()[0].seq);
}

void test_permit_bits_snapshot_round_trip()
{
    TEST_ASSERT_TRUE(box.record(DashApBbKind::PermitChange, 1, "",
                                0b00000, 3, 1000, 42));
    TEST_ASSERT_EQUAL_UINT8(0, box.entries()[0].permitBits);
    TEST_ASSERT_TRUE(box.record(DashApBbKind::PermitChange, 1, "",
                                0b11111, 3, 20'000, 43));
    TEST_ASSERT_EQUAL_UINT8(0x1F, box.entries()[0].permitBits);
    TEST_ASSERT_EQUAL_UINT32(43, box.entries()[0].tick);
}

// ── 4.5.0-beta12: golden-string tests for dashApBbEntryToJson ──────────────
// These strings are the CONTRACT: the dashboard emission loop concatenates
// them verbatim (comma-separated) inside the blackbox JSON array. A single
// stray/unbalanced quote here is exactly the beta06/beta07/beta10 bug class.

static DashApBbEntry mkEntry(uint8_t kind, uint16_t seq, uint32_t ms,
                             uint32_t tick, uint8_t phase, uint8_t bits,
                             uint8_t apState, const char *reason)
{
    DashApBbEntry e = {};
    e.seq = seq;
    e.kind = kind;
    e.ms = ms;
    e.tick = tick;
    e.phase = phase;
    e.permitBits = bits;
    e.apState = apState;
    strncpy(e.reason, reason, sizeof(e.reason) - 1);
    return e;
}

static void assertEntryJson(const char *golden, const DashApBbEntry &e)
{
    char buf[kDashApBbEntryJsonCap];
    const size_t n = dashApBbEntryToJson(buf, sizeof(buf), e);
    TEST_ASSERT_GREATER_THAN_size_t(0, n);
    TEST_ASSERT_EQUAL_STRING(golden, buf);
    TEST_ASSERT_EQUAL_size_t(strlen(golden), n); // no trailing garbage/NUL inside
}

void test_entry_json_golden_all_kinds_with_reason()
{
    // Same envelope, only the kind name varies — pins the kind table AND the
    // key order AND every separator character in one sweep.
    const char *kinds[] = {"boot", "heartbeat", "roundStart", "roundEnd",
                           "lock", "permitChange", "intentChange"};
    for (uint8_t k = 1; k <= 7; ++k)
    {
        DashApBbEntry e = mkEntry(k, 12, 60000, 77, 1, 23, 6, "windowExpired");
        char golden[160];
        snprintf(golden, sizeof(golden),
                 "{\"seq\":12,\"kind\":\"%s\",\"ms\":60000,\"tick\":77,"
                 "\"phase\":1,\"permitBits\":23,\"apState\":6,"
                 "\"reason\":\"windowExpired\"}",
                 kinds[k - 1]);
        assertEntryJson(golden, e);
    }
}

void test_entry_json_golden_without_reason()
{
    // Empty reason must still emit the key with an empty string — omitting
    // the key would change the UI-side shape.
    assertEntryJson(
        "{\"seq\":3,\"kind\":\"lock\",\"ms\":4294967295,\"tick\":0,"
        "\"phase\":2,\"permitBits\":0,\"apState\":0,\"reason\":\"\"}",
        mkEntry(5, 3, 4294967295u, 0, 2, 0, 0, ""));
}

void test_entry_json_golden_boot_and_heartbeat_distinct()
{
    // Adjacent kinds must not collapse (a broken switch returning one name
    // for two kinds would silently mislabel every entry).
    assertEntryJson(
        "{\"seq\":1,\"kind\":\"boot\",\"ms\":0,\"tick\":0,\"phase\":0,"
        "\"permitBits\":31,\"apState\":0,\"reason\":\"\"}",
        mkEntry(1, 1, 0, 0, 0, 31, 0, ""));
    assertEntryJson(
        "{\"seq\":2,\"kind\":\"heartbeat\",\"ms\":5000,\"tick\":99,"
        "\"phase\":0,\"permitBits\":31,\"apState\":0,\"reason\":\"\"}",
        mkEntry(2, 2, 5000, 99, 0, 31, 0, ""));
}

void test_entry_json_unknown_kind_emits_placeholder()
{
    // Out-of-range kinds (e.g. a future blob loaded by an older firmware)
    // must serialize as "?" — never as an empty/malformed fragment.
    assertEntryJson(
        "{\"seq\":4,\"kind\":\"?\",\"ms\":1,\"tick\":1,\"phase\":0,"
        "\"permitBits\":0,\"apState\":0,\"reason\":\"x\"}",
        mkEntry(0, 4, 1, 1, 0, 0, 0, "x"));
    assertEntryJson(
        "{\"seq\":4,\"kind\":\"?\",\"ms\":1,\"tick\":1,\"phase\":0,"
        "\"permitBits\":0,\"apState\":0,\"reason\":\"x\"}",
        mkEntry(9, 4, 1, 1, 0, 0, 0, "x"));
}

void test_entry_json_buffer_too_small_returns_zero_and_empty()
{
    DashApBbEntry e = mkEntry(6, 12, 60000, 77, 1, 23, 6, "windowExpired");
    char tiny[8];
    memset(tiny, 'Z', sizeof(tiny));
    TEST_ASSERT_EQUAL_size_t(0, dashApBbEntryToJson(tiny, sizeof(tiny), e));
    TEST_ASSERT_EQUAL_CHAR('\0', tiny[0]); // empty string, never a fragment
    // One byte BELOW the cap refuses (no truncation)…
    char small[kDashApBbEntryJsonCap - 1];
    TEST_ASSERT_EQUAL_size_t(0, dashApBbEntryToJson(small, sizeof(small), e));
    // …exactly the cap succeeds.
    char exact[kDashApBbEntryJsonCap];
    TEST_ASSERT_GREATER_THAN_size_t(0,
                                    dashApBbEntryToJson(exact, sizeof(exact), e));
    // Zero-cap must not write anything.
    TEST_ASSERT_EQUAL_size_t(0, dashApBbEntryToJson(exact, 0, e));
}

void test_entry_json_matches_real_recorded_entry()
{
    // End-to-end shape check: what record() produces is exactly what
    // toJson emits — the dashboard loop relies on this pairing.
    box.clear();
    TEST_ASSERT_TRUE(box.record(DashApBbKind::RoundEnd, 7,
                                "apLeftInjectionSet22", 0x1F, 3, 1000, 0));
    const DashApBbEntry &e = box.entries()[0];
    // reason truncates to 15 chars ("apLeftInjection"), seq starts at 1.
    assertEntryJson(
        "{\"seq\":1,\"kind\":\"roundEnd\",\"ms\":1000,\"tick\":0,"
        "\"phase\":7,\"permitBits\":31,\"apState\":3,"
        "\"reason\":\"apLeftInjection\"}",
        e);
}


int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_fresh_box_is_empty);
    RUN_TEST(test_first_record_lands_at_index_zero_as_newest);
    RUN_TEST(test_newer_records_stack_above_older);
    RUN_TEST(test_full_box_drops_oldest_and_keeps_capacity);
    RUN_TEST(test_seq_monotonic_across_wrap);
    RUN_TEST(test_flap_guard_drops_same_kind_within_window);
    RUN_TEST(test_flap_guard_tracks_permit_and_intent_independently);
    RUN_TEST(test_non_flappy_kinds_are_never_suppressed);
    RUN_TEST(test_dump_load_round_trip_preserves_entries_and_seq);
    RUN_TEST(test_load_rejects_wrong_length_blob);
    RUN_TEST(test_load_null_is_ignored);
    RUN_TEST(test_reason_truncation_length);
    RUN_TEST(test_invalid_kind_rejected);
    RUN_TEST(test_clear_resets_everything);
    RUN_TEST(test_permit_bits_snapshot_round_trip);
    RUN_TEST(test_entry_json_golden_all_kinds_with_reason);
    RUN_TEST(test_entry_json_golden_without_reason);
    RUN_TEST(test_entry_json_golden_boot_and_heartbeat_distinct);
    RUN_TEST(test_entry_json_unknown_kind_emits_placeholder);
    RUN_TEST(test_entry_json_buffer_too_small_returns_zero_and_empty);
    RUN_TEST(test_entry_json_matches_real_recorded_entry);
    return UNITY_END();
}
