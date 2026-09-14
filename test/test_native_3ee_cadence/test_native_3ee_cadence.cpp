#include <unity.h>

#include "dash_3ee_cadence.h"

// 4.5.0-beta12 (LittleGong port 2): unit tests for the 0x3EE mux0 carrier
// cadence observer. Pure observation, zero behavior change — these tests pin
// the FILTER contract (the coordinator's qualification injection watches
// exactly this stream: id 0x3EE, mux0, CAN_BUS_VEH) and the gap statistics
// that /status's carrier3ee block emits, including the 2000ms longGap
// boundary. Symptom 1 ("only EAP appears" = carrier missing inside the
// qualification window) is judged in the field from these numbers.

static Dash3eeCadence cad;

void setUp()
{
    cad = Dash3eeCadence{};
}

void tearDown() {}

// mux0 carrier frame on the coordinator source bus.
static CanFrame mk3ee(uint8_t mux = 0, uint8_t bus = CAN_BUS_VEH,
                      uint8_t dlc = 8, uint32_t id = 0x3EE)
{
    CanFrame f;
    f.id = id;
    f.dlc = dlc;
    for (uint8_t i = 0; i < 8; ++i)
        f.data[i] = 0;
    f.data[0] = mux;
    f.bus = bus;
    return f;
}

// ── Filter contract ────────────────────────────────────────────────────────

void test_first_mux0_frame_counts_but_has_no_gap()
{
    cad.observe(mk3ee(), 1000);
    TEST_ASSERT_EQUAL_UINT32(1, cad.total);
    TEST_ASSERT_EQUAL_UINT32(1000, cad.lastMs);
    TEST_ASSERT_EQUAL_UINT32(0, cad.lastGapMs);
    TEST_ASSERT_EQUAL_UINT32(0, cad.maxGapMs);
    TEST_ASSERT_EQUAL_UINT32(0, cad.longGaps);
}

void test_non_3ee_id_ignored()
{
    cad.observe(mk3ee(0, CAN_BUS_VEH, 8, 0x3ED), 1000);
    cad.observe(mk3ee(0, CAN_BUS_VEH, 8, 0x118), 2000);
    TEST_ASSERT_EQUAL_UINT32(0, cad.total);
    TEST_ASSERT_EQUAL_UINT32(0, cad.lastMs);
}

void test_non_mux0_ignored()
{
    cad.observe(mk3ee(1), 1000);    // mux1 (scroll wheel activity)
    cad.observe(mk3ee(7), 2000);    // worst-case low bits
    cad.observe(mk3ee(0x08), 3000); // low 3 bits zero: IS mux0 — counts
    TEST_ASSERT_EQUAL_UINT32(1, cad.total);
    TEST_ASSERT_EQUAL_UINT32(3000, cad.lastMs);
}

void test_wrong_bus_ignored()
{
    cad.observe(mk3ee(0, CAN_BUS_CH), 1000); // chassis bus: not the source
    cad.observe(mk3ee(0, CAN_BUS_PARTY), 2000);
    cad.observe(mk3ee(0, CAN_BUS_ANY), 3000); // unrouted: not the source
    TEST_ASSERT_EQUAL_UINT32(0, cad.total);
}

void test_zero_dlc_ignored()
{
    cad.observe(mk3ee(0, CAN_BUS_VEH, 0), 1000);
    TEST_ASSERT_EQUAL_UINT32(0, cad.total);
    TEST_ASSERT_EQUAL_UINT32(0, cad.lastMs);
}

// ── Gap statistics ─────────────────────────────────────────────────────────

void test_gap_accumulation_and_max_tracking()
{
    cad.observe(mk3ee(), 1000);
    cad.observe(mk3ee(), 2000);
    TEST_ASSERT_EQUAL_UINT32(2, cad.total);
    TEST_ASSERT_EQUAL_UINT32(1000, cad.lastGapMs);
    TEST_ASSERT_EQUAL_UINT32(1000, cad.maxGapMs);
    cad.observe(mk3ee(), 2600);
    TEST_ASSERT_EQUAL_UINT32(600, cad.lastGapMs);
    TEST_ASSERT_EQUAL_UINT32(1000, cad.maxGapMs); // max keeps the bigger one
    cad.observe(mk3ee(), 5600);
    TEST_ASSERT_EQUAL_UINT32(3000, cad.lastGapMs);
    TEST_ASSERT_EQUAL_UINT32(3000, cad.maxGapMs);
}

void test_long_gap_boundary_exactly_2000_not_counted()
{
    cad.observe(mk3ee(), 1000);
    cad.observe(mk3ee(), 3000); // gap == kDash3eeLongGapMs exactly
    TEST_ASSERT_EQUAL_UINT32(2000, cad.lastGapMs);
    TEST_ASSERT_EQUAL_UINT32(0, cad.longGaps);
}

void test_long_gap_boundary_2001_counted()
{
    cad.observe(mk3ee(), 1000);
    cad.observe(mk3ee(), 3001); // gap == 2001 > 2000
    TEST_ASSERT_EQUAL_UINT32(2001, cad.lastGapMs);
    TEST_ASSERT_EQUAL_UINT32(1, cad.longGaps);
}

void test_long_gaps_accumulate()
{
    cad.observe(mk3ee(), 1000);
    cad.observe(mk3ee(), 4000); // 3000 -> longGap 1
    cad.observe(mk3ee(), 4100); // 100 -> normal
    cad.observe(mk3ee(), 9000); // 4900 -> longGap 2
    TEST_ASSERT_EQUAL_UINT32(4, cad.total);
    TEST_ASSERT_EQUAL_UINT32(2, cad.longGaps);
    TEST_ASSERT_EQUAL_UINT32(4900, cad.maxGapMs);
}

void test_equal_timestamp_records_no_gap()
{
    // Same-millis double observation must not fabricate a zero gap that
    // would corrupt lastGapMs reporting.
    cad.observe(mk3ee(), 1000);
    cad.observe(mk3ee(), 2000);
    cad.observe(mk3ee(), 2000); // replay/same tick
    TEST_ASSERT_EQUAL_UINT32(3, cad.total);
    TEST_ASSERT_EQUAL_UINT32(1000, cad.lastGapMs);
}

void test_backward_timestamp_records_no_gap()
{
    cad.observe(mk3ee(), 5000);
    cad.observe(mk3ee(), 4000); // defensive: never a negative/wrapped gap
    TEST_ASSERT_EQUAL_UINT32(2, cad.total);
    TEST_ASSERT_EQUAL_UINT32(0, cad.lastGapMs);
    TEST_ASSERT_EQUAL_UINT32(0, cad.maxGapMs);
    TEST_ASSERT_EQUAL_UINT32(0, cad.longGaps);
}

void test_filtered_frames_do_not_break_the_timeline()
{
    // mux1/other-bus frames BETWEEN mux0 frames must not shrink or distort
    // the measured mux0-to-mux0 gap — the whole point of the observer is
    // the true carrier rhythm on the qualification stream.
    cad.observe(mk3ee(), 1000);
    cad.observe(mk3ee(1), 1500);                        // filtered
    cad.observe(mk3ee(0, CAN_BUS_CH), 1800);            // filtered
    cad.observe(mk3ee(0, CAN_BUS_VEH, 8, 0x118), 1900); // filtered
    cad.observe(mk3ee(), 4000);                         // real gap = 3000
    TEST_ASSERT_EQUAL_UINT32(2, cad.total);
    TEST_ASSERT_EQUAL_UINT32(3000, cad.lastGapMs);
    TEST_ASSERT_EQUAL_UINT32(1, cad.longGaps);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_first_mux0_frame_counts_but_has_no_gap);
    RUN_TEST(test_non_3ee_id_ignored);
    RUN_TEST(test_non_mux0_ignored);
    RUN_TEST(test_wrong_bus_ignored);
    RUN_TEST(test_zero_dlc_ignored);
    RUN_TEST(test_gap_accumulation_and_max_tracking);
    RUN_TEST(test_long_gap_boundary_exactly_2000_not_counted);
    RUN_TEST(test_long_gap_boundary_2001_counted);
    RUN_TEST(test_long_gaps_accumulate);
    RUN_TEST(test_equal_timestamp_records_no_gap);
    RUN_TEST(test_backward_timestamp_records_no_gap);
    RUN_TEST(test_filtered_frames_do_not_break_the_timeline);
    return UNITY_END();
}
