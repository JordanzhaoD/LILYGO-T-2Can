#include <unity.h>

#include "dash_heap_guard.h"

// Heap guard tripwire v2 (2026-11-02 crash forensics round 3): three field
// crashes all decode to mcause=1/mepc=0 (jump to zero) after can_rt derails
// inside an indirect driver/handler vtable dispatch. The canary tripwire
// (HEAP_POISONING_COMPREHENSIVE) never fired — the corruption class escapes
// free-time checks. These tests pin the DECISION CORE of the statics
// sentinels that watch the exact dispatch pointers:
//
//   stable slots — appDriver/appHandler/appDriverSecondary/appDriverTertiary:
//     created once at setup; pointer must equal the boot snapshot forever
//     (late-init adoption allowed); object head word (vtable) must match.
//   closed-set slots — appActiveHandler/dashHandler: dashSwapHandler()
//     legitimately switches these among handlerPool[0..2]; the valid set is
//     CLOSED at three members, so any post-settle unknown pointer trips.
//
// All tests drive the core with EXPLICIT values — no dereference, no
// millis(), no FreeRTOS: the ESP layer feeds real memory through the same
// functions and aborts on anything but Ok/Rearm.

namespace
{
uint32_t somePtr = 0x4ff40010u;
} // namespace

void setUp()
{
    somePtr = 0x4ff40010u;
}

void tearDown() {}

// ── range gate ────────────────────────────────────────────────────────────

void test_range_boundaries()
{
    TEST_ASSERT_FALSE(dashHgRangeOk(0x00000000u));
    TEST_ASSERT_FALSE(dashHgRangeOk(0x4fefffffu));
    TEST_ASSERT_TRUE(dashHgRangeOk(0x4ff00000u));
    TEST_ASSERT_TRUE(dashHgRangeOk(0x4ffbffffu));
    TEST_ASSERT_FALSE(dashHgRangeOk(0x4ffc0000u));
    TEST_ASSERT_FALSE(dashHgRangeOk(0xffffffffu));
}

// ── stable slots ──────────────────────────────────────────────────────────

void test_stable_unarmed_null_stays_ok()
{
    DashHgSlot s = dashHgSlotStable("t");
    TEST_ASSERT_EQUAL(kDashHgOk, dashHgEvalStable(&s, 0, 0));
    TEST_ASSERT_EQUAL(0, s.count);
}

void test_stable_unarmed_adopts_in_range_pointer()
{
    DashHgSlot s = dashHgSlotStable("t");
    TEST_ASSERT_EQUAL(kDashHgRearm, dashHgEvalStable(&s, 0x4ff50020u, 0x40000100u));
    TEST_ASSERT_EQUAL(1, s.count);
    TEST_ASSERT_EQUAL_UINT32(0x4ff50020u, s.mem[0].ptr);
    TEST_ASSERT_EQUAL_UINT32(0x40000100u, s.mem[0].head);
}

void test_stable_unarmed_out_of_range_does_not_adopt()
{
    // A wild pointer seen before arm: stay unarmed, keep watching — do not
    // adopt garbage as the baseline and do not trip before arming.
    DashHgSlot s = dashHgSlotStable("t");
    TEST_ASSERT_EQUAL(kDashHgOk, dashHgEvalStable(&s, 0xdeadbeefu, 0));
    TEST_ASSERT_EQUAL(0, s.count);
}

void test_stable_armed_matching_is_ok()
{
    DashHgSlot s = dashHgSlotStable("t");
    dashHgEvalStable(&s, 0x4ff50020u, 0x40000100u);
    TEST_ASSERT_EQUAL(kDashHgOk, dashHgEvalStable(&s, 0x4ff50020u, 0x40000100u));
}

void test_stable_armed_null_trips()
{
    DashHgSlot s = dashHgSlotStable("t");
    dashHgEvalStable(&s, 0x4ff50020u, 0x40000100u);
    TEST_ASSERT_EQUAL(kDashHgTripNull, dashHgEvalStable(&s, 0, 0));
}

void test_stable_armed_pointer_change_trips()
{
    DashHgSlot s = dashHgSlotStable("t");
    dashHgEvalStable(&s, 0x4ff50020u, 0x40000100u);
    TEST_ASSERT_EQUAL(kDashHgTripPointer, dashHgEvalStable(&s, 0x4ff60030u, 0));
}

void test_stable_armed_head_change_trips()
{
    // Pointer intact, vtable word rewritten: the exact derail shape.
    DashHgSlot s = dashHgSlotStable("t");
    dashHgEvalStable(&s, 0x4ff50020u, 0x40000100u);
    TEST_ASSERT_EQUAL(kDashHgTripHead, dashHgEvalStable(&s, 0x4ff50020u, 0x4fc1a588u));
}

// ── closed-set slots (appActiveHandler / dashHandler) ─────────────────────

void test_set_null_is_ok()
{
    DashHgSlot s = dashHgSlotSet("t");
    TEST_ASSERT_EQUAL(kDashHgOk, dashHgEvalSet(&s, 0, 0));
    TEST_ASSERT_EQUAL(0, s.count);
}

void test_set_adopts_three_members_then_closes()
{
    DashHgSlot s = dashHgSlotSet("t");
    TEST_ASSERT_EQUAL(kDashHgRearm, dashHgEvalSet(&s, 0x4ff60000u, 0x40000100u));
    TEST_ASSERT_EQUAL(kDashHgRearm, dashHgEvalSet(&s, 0x4ff60040u, 0x40000140u));
    TEST_ASSERT_EQUAL(kDashHgRearm, dashHgEvalSet(&s, 0x4ff60080u, 0x40000180u));
    TEST_ASSERT_EQUAL(3, s.count);
    TEST_ASSERT_EQUAL(kDashHgOk, dashHgEvalSet(&s, 0x4ff60000u, 0x40000100u));
    TEST_ASSERT_EQUAL(kDashHgOk, dashHgEvalSet(&s, 0x4ff60080u, 0x40000180u));
}

void test_set_full_unknown_in_range_trips_pointer()
{
    // handlerPool has exactly three members; once all are seen any new
    // in-range pointer is corruption, not a fourth handler.
    DashHgSlot s = dashHgSlotSet("t");
    dashHgEvalSet(&s, 0x4ff60000u, 0x40000100u);
    dashHgEvalSet(&s, 0x4ff60040u, 0x40000140u);
    dashHgEvalSet(&s, 0x4ff60080u, 0x40000180u);
    TEST_ASSERT_EQUAL(kDashHgTripPointer, dashHgEvalSet(&s, 0x4ff700f0u, 0));
}

void test_set_unknown_out_of_range_trips_pointer()
{
    DashHgSlot s = dashHgSlotSet("t");
    dashHgEvalSet(&s, 0x4ff60000u, 0x40000100u);
    TEST_ASSERT_EQUAL(kDashHgTripPointer, dashHgEvalSet(&s, 0x20000000u, 0));
}

void test_set_known_member_wrong_head_trips()
{
    // A swapped-in pool object whose vtable word was rewritten.
    DashHgSlot s = dashHgSlotSet("t");
    dashHgEvalSet(&s, 0x4ff60000u, 0x40000100u);
    TEST_ASSERT_EQUAL(kDashHgTripHead, dashHgEvalSet(&s, 0x4ff60000u, 0x00000001u));
}

void test_set_null_after_members_is_ok()
{
    // appActiveHandler may legally drop back to null? It cannot today, but
    // null is never corruption evidence on its own — the dispatch guards
    // against null already; the sentinel's job is wild pointers.
    DashHgSlot s = dashHgSlotSet("t");
    dashHgEvalSet(&s, 0x4ff60000u, 0x40000100u);
    TEST_ASSERT_EQUAL(kDashHgOk, dashHgEvalSet(&s, 0, 0));
}

// ── zero-head adoption (bench false-trip #1, 2026-11-02) ──────────────────
// Real-path sequence: the first check cannot deref yet (discipline) so the
// slot adopts with curHead=0; the second check reads the real vtable. That
// must RE-BASELINE, not trip — a live object's vtable is never 0.

void test_stable_zero_head_adoption_rebaselines()
{
    DashHgSlot s = dashHgSlotStable("t");
    TEST_ASSERT_EQUAL(kDashHgRearm, dashHgEvalStable(&s, 0x4ff50020u, 0));
    TEST_ASSERT_EQUAL_UINT32(0, s.mem[0].head);
    // Second pass sees the real head: baseline fill, not a trip.
    TEST_ASSERT_EQUAL(kDashHgOk, dashHgEvalStable(&s, 0x4ff50020u, 0x4010c498u));
    TEST_ASSERT_EQUAL_UINT32(0x4010c498u, s.mem[0].head);
    // Third pass with a DIFFERENT head: now it is a real trip.
    TEST_ASSERT_EQUAL(kDashHgTripHead, dashHgEvalStable(&s, 0x4ff50020u, 0x4fc1a588u));
}

void test_set_zero_head_adoption_rebaselines()
{
    DashHgSlot s = dashHgSlotSet("t");
    TEST_ASSERT_EQUAL(kDashHgRearm, dashHgEvalSet(&s, 0x4ff60000u, 0));
    TEST_ASSERT_EQUAL(kDashHgOk, dashHgEvalSet(&s, 0x4ff60000u, 0x40000100u));
    TEST_ASSERT_EQUAL_UINT32(0x40000100u, s.mem[0].head);
    TEST_ASSERT_EQUAL(kDashHgTripHead, dashHgEvalSet(&s, 0x4ff60000u, 0x00000001u));
}

// ── deref policy ──────────────────────────────────────────────────────────

void test_deref_only_when_armed_and_matching()
{
    DashHgSlot s = dashHgSlotStable("t");
    TEST_ASSERT_FALSE(dashHgDerefAllowed(&s, 0));            // unarmed
    TEST_ASSERT_FALSE(dashHgDerefAllowed(&s, 0x4ff50020u));   // unarmed, not yet adopted
    dashHgEvalStable(&s, 0x4ff50020u, 0x40000100u);
    TEST_ASSERT_TRUE(dashHgDerefAllowed(&s, 0x4ff50020u));    // armed + match
    TEST_ASSERT_FALSE(dashHgDerefAllowed(&s, 0x4ff60030u));   // armed, mismatch (trip before deref)
    TEST_ASSERT_FALSE(dashHgDerefAllowed(&s, 0));             // armed, null
}

// ── TCB sentinels v3 (2026-11-03, round 4/5 forensics) ────────────────────
// The can_rt TCB was found 0xA5-filled inside web's stack block: the
// corruption lands on task control blocks themselves. The v3 core pins:
//   name-head packing (NUL-padded so adjacent TCB bytes never enter the
//   baseline), recycled-handle adoption via xTaskNumber, and the three
//   trip shapes (name / stack base / top-of-stack out of bounds).

void test_tcb_pack_name_pads_past_nul()
{
    // "web" and "web" + garbage-after-NUL must pack identically: bytes
    // past the terminator are neighboring TCB fields, never baseline'd.
    uint32_t a1 = 0, b1 = 0, a2 = 0, b2 = 0;
    char padded[8] = {'w', 'e', 'b', 'X', 'Y', 'Z', 'Q', 'R'};
    dashHgPackName8("web", &a1, &b1);
    padded[3] = '\0';
    dashHgPackName8(padded, &a2, &b2);
    TEST_ASSERT_EQUAL_UINT32(a1, a2);
    TEST_ASSERT_EQUAL_UINT32(b1, b2);
    TEST_ASSERT_EQUAL_UINT32(0, b1); // bytes 4..7 all zero
    TEST_ASSERT_EQUAL_UINT32(('w') | ('e' << 8) | ('b' << 16), a1);
}

void test_tcb_pack_name_distinguishes_idle_cores()
{
    // IDLE0 vs IDLE1 differ at byte 4: 8-byte packing catches what a
    // 4-byte head cannot.
    uint32_t a0 = 0, b0 = 0, a1 = 0, b1 = 0;
    dashHgPackName8("IDLE0", &a0, &b0);
    dashHgPackName8("IDLE1", &a1, &b1);
    TEST_ASSERT_EQUAL_UINT32(a0, a1);
    TEST_ASSERT_TRUE(b0 != b1);
    TEST_ASSERT_EQUAL_UINT32('0', b0);
    TEST_ASSERT_EQUAL_UINT32('1', b1);
}

namespace
{
DashHgTcbBase tcbBaseline(const char *name, uint32_t number, uint32_t stackBase)
{
    DashHgTcbBase b = {};
    b.handle = 0x4ff7a09cu;
    b.taskNumber = number;
    b.stackBase = stackBase;
    dashHgPackName8(name, &b.nameA, &b.nameB);
    return b;
}
const uint32_t kDramEnd = 0x4ffc0000u;
} // namespace

void test_tcb_matching_observation_is_ok()
{
    DashHgTcbBase b = tcbBaseline("can_rt", 7, 0x4ff79000u);
    uint32_t nameA = 0, nameB = 0;
    dashHgPackName8("can_rt", &nameA, &nameB);
    // pxTopOfStack somewhere inside [base, dramEnd): stacks grow down
    TEST_ASSERT_EQUAL(kDashHgTcbOk, dashHgTcbEval(&b, 7, nameA, nameB,
                                                  0x4ff79000u, 0x4ff7a080u, kDramEnd));
    // top equal to base is legal (deepest possible growth)
    TEST_ASSERT_EQUAL(kDashHgTcbOk, dashHgTcbEval(&b, 7, nameA, nameB,
                                                  0x4ff79000u, 0x4ff79000u, kDramEnd));
}

void test_tcb_recycled_handle_adopts()
{
    // Same address, new xTaskNumber: a fresh task, not corruption.
    DashHgTcbBase b = tcbBaseline("gw_dns", 12, 0x4ff50000u);
    uint32_t nameA = 0, nameB = 0;
    dashHgPackName8("recycle", &nameA, &nameB);
    TEST_ASSERT_EQUAL(kDashHgTcbAdopt, dashHgTcbEval(&b, 13, nameA, nameB,
                                                     0x4ff50000u, 0x4ff50100u, kDramEnd));
}

void test_tcb_name_rewrite_trips()
{
    // The round-5 corpse shape: TCB name bytes overwritten (0xA5 fill).
    DashHgTcbBase b = tcbBaseline("can_rt", 7, 0x4ff79000u);
    uint32_t nameA = 0, nameB = 0;
    dashHgPackName8("\xa5\xa5\xa5\xa5\xa5_rt", &nameA, &nameB);
    TEST_ASSERT_EQUAL(kDashHgTcbTripName, dashHgTcbEval(&b, 7, nameA, nameB,
                                                        0x4ff79000u, 0x4ff7a080u, kDramEnd));
}

void test_tcb_stack_base_rewrite_trips()
{
    DashHgTcbBase b = tcbBaseline("can_rt", 7, 0x4ff79000u);
    uint32_t nameA = 0, nameB = 0;
    dashHgPackName8("can_rt", &nameA, &nameB);
    TEST_ASSERT_EQUAL(kDashHgTcbTripStackBase, dashHgTcbEval(&b, 7, nameA, nameB,
                                                             0x4ff88000u, 0x4ff7a080u, kDramEnd));
}

void test_tcb_top_of_stack_out_of_bounds_trips()
{
    DashHgTcbBase b = tcbBaseline("can_rt", 7, 0x4ff79000u);
    uint32_t nameA = 0, nameB = 0;
    dashHgPackName8("can_rt", &nameA, &nameB);
    // 0xA5-filled TCB: pxTopOfStack reads 0xa5a5a5a5 — below base and outside DRAM
    TEST_ASSERT_EQUAL(kDashHgTcbTripTop, dashHgTcbEval(&b, 7, nameA, nameB,
                                                       0x4ff79000u, 0xa5a5a5a5u, kDramEnd));
    // above the DRAM top is equally poisoned
    TEST_ASSERT_EQUAL(kDashHgTcbTripTop, dashHgTcbEval(&b, 7, nameA, nameB,
                                                       0x4ff79000u, 0x50000000u, kDramEnd));
}

int runUnityTests()
{
    UNITY_BEGIN();
    RUN_TEST(test_range_boundaries);
    RUN_TEST(test_stable_unarmed_null_stays_ok);
    RUN_TEST(test_stable_unarmed_adopts_in_range_pointer);
    RUN_TEST(test_stable_unarmed_out_of_range_does_not_adopt);
    RUN_TEST(test_stable_armed_matching_is_ok);
    RUN_TEST(test_stable_armed_null_trips);
    RUN_TEST(test_stable_armed_pointer_change_trips);
    RUN_TEST(test_stable_armed_head_change_trips);
    RUN_TEST(test_set_null_is_ok);
    RUN_TEST(test_set_adopts_three_members_then_closes);
    RUN_TEST(test_set_full_unknown_in_range_trips_pointer);
    RUN_TEST(test_set_unknown_out_of_range_trips_pointer);
    RUN_TEST(test_set_known_member_wrong_head_trips);
    RUN_TEST(test_set_null_after_members_is_ok);
    RUN_TEST(test_stable_zero_head_adoption_rebaselines);
    RUN_TEST(test_set_zero_head_adoption_rebaselines);
    RUN_TEST(test_deref_only_when_armed_and_matching);
    RUN_TEST(test_tcb_pack_name_pads_past_nul);
    RUN_TEST(test_tcb_pack_name_distinguishes_idle_cores);
    RUN_TEST(test_tcb_matching_observation_is_ok);
    RUN_TEST(test_tcb_recycled_handle_adopts);
    RUN_TEST(test_tcb_name_rewrite_trips);
    RUN_TEST(test_tcb_stack_base_rewrite_trips);
    RUN_TEST(test_tcb_top_of_stack_out_of_bounds_trips);
    return UNITY_END();
}

#if defined(ARDUINO)
#include <Arduino.h>
void setup() {}
void loop() { runUnityTests(); }
#else
int main() { return runUnityTests(); }
#endif
