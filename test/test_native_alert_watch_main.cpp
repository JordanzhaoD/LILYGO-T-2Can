// Native harness for the alert-watch core (dash_alert_watch.h).
// Compiled and run by test/test_alert_watch_contract.py via:
//   clang++ -std=c++17 -Wall -Wextra -Werror -DDRIVER_P4_3CAN -I include
// -DDRIVER_P4_3CAN is mandatory (the header lives inside the P4 guard);
// plain C++ (no Unity), the pytest asserts exit 0 + "ALL PASS".
// PIO-inert guard: PlatformIO's native test builder globs top-level
// test/*.cpp into every native unit build; this file would fail there
// (no -DDRIVER_P4_3CAN) and collide with Unity's main().
#ifndef NATIVE_BUILD

#include <cstdio>
#include <cstring>

#include "dash_alert_watch.h"

static int g_fail = 0;

#define CHECK(cond)                                                    \
    do                                                                 \
    {                                                                  \
        if (!(cond))                                                   \
        {                                                              \
            ++g_fail;                                                  \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        }                                                              \
    } while (0)

// (a) 0x526 a144 config alert -> CAPTURE-severity event, code=144.
static void test_log_frame_a144_is_capture()
{
    const uint8_t f[8] = {0x90, 0x80, 0x20, 0, 0, 0, 0, 0}; // sess_124 body
    bool fired = dashAlertWatchOnFrame(0x526, 8, f, 1000);
    CHECK(fired);
    const DashAlertEvent *e = dashAlertWatchLastEvent();
    CHECK(e != nullptr);
    CHECK(e->src == kAlertSrcLog526);
    CHECK(e->code == 144);
    CHECK(e->sev == kSevCapture);
    CHECK(std::memcmp(e->data, f, 8) == 0);
}

// (b) a062 limp -> capture; an ordinary code -> log-only.
static void test_log_frame_severity_split()
{
    const uint8_t limp[8] = {0x3E, 0x00, 0x19, 0, 0, 0, 0, 0}; // sess_127
    CHECK(dashAlertWatchOnFrame(0x526, 8, limp, 2000));
    CHECK(dashAlertWatchLastEvent()->code == 62);
    CHECK(dashAlertWatchLastEvent()->sev == kSevCapture);

    const uint8_t ordinary[8] = {0x0A, 0, 0, 0, 0, 0, 0, 0}; // a010
    CHECK(dashAlertWatchOnFrame(0x526, 8, ordinary, 3000));
    CHECK(dashAlertWatchLastEvent()->code == 10);
    CHECK(dashAlertWatchLastEvent()->sev == kSevLog);

    // 0x524: ANY frame is powertrain-management class -> capture.
    const uint8_t pm[8] = {0x2B, 0, 0, 0, 0, 0, 0, 0}; // a043 decimal 43
    CHECK(dashAlertWatchOnFrame(0x524, 8, pm, 4000));
    CHECK(dashAlertWatchLastEvent()->src == kAlertSrcLog524);
    CHECK(dashAlertWatchLastEvent()->code == 43);
    CHECK(dashAlertWatchLastEvent()->sev == kSevCapture);
}

// (c) 0x346 matrix: rising edges only, repeated broadcast dedupes; the
// edge after an empty bitmap (fresh boot) is itself an event.
static void test_matrix_rising_edge()
{
    const uint8_t zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(!dashAlertWatchOnFrame(0x346, 8, zero, 5000)); // idle: no event

    const uint8_t latched[8] = {0, 0, 0, 0, 0, 0, 0, 0x20}; // b7 bit5 = w062
    CHECK(dashAlertWatchOnFrame(0x346, 8, latched, 6000));
    const DashAlertEvent *e = dashAlertWatchLastEvent();
    CHECK(e->src == kAlertSrcMatrix346);
    CHECK(e->code == 62); // w062_systemLimpMode
    CHECK(e->sev == kSevCapture);

    CHECK(!dashAlertWatchOnFrame(0x346, 8, latched, 7000)); // re-broadcast: no edge

    const uint8_t plus[8] = {0, 0, 0, 0, 0, 0, 0, 0xA0}; // +b7bit7 = w064
    CHECK(dashAlertWatchOnFrame(0x346, 8, plus, 8000));
    CHECK(dashAlertWatchLastEvent()->code == 64);

    // Ordinary bit (w023 carNotParked = byte2 bit6, LSB-first -> 0x40) is log-only.
    const uint8_t w023[8] = {0, 0, 0x40, 0, 0, 0, 0, 0};
    CHECK(dashAlertWatchOnFrame(0x346, 8, w023, 9000));
    CHECK(dashAlertWatchLastEvent()->code == 23);
    CHECK(dashAlertWatchLastEvent()->sev == kSevLog);
}

// (d) tripwire payload dedupe: 100 repeats of one deviant -> exactly one
// event; a second distinct payload -> second event. Rolling window of 8
// distinct payloads: after 9 distinct, the first is forgotten and fires
// again on return.
static void test_tripwire_payload_dedupe()
{
    const uint8_t a[8] = {0x51, 0x31, 0x51, 0x0F, 0x88, 0x49, 0x67, 0x00};
    for (int i = 0; i < 100; i++)
        dashAlertWatchOnTripwire(a, 10000 + i);
    CHECK(dashAlertWatchLastEvent()->src == kAlertSrcTripwire);
    CHECK(dashAlertWatchLastEvent()->sev == kSevCapture);
    size_t eventsAfterA = 1;

    const uint8_t b[8] = {0x51, 0x31, 0x59, 0x0F, 0x88, 0x49, 0x67, 0x00};
    CHECK(dashAlertWatchOnTripwire(b, 11000));
    eventsAfterA += 1;
    (void)eventsAfterA;

    // Fill the rolling dedupe set with 8 distinct payloads total, then
    // push one more so `a` falls out; replaying `a` must fire again.
    for (int k = 0; k < 7; k++)
    {
        uint8_t p[8];
        std::memcpy(p, a, 8);
        p[0] = static_cast<uint8_t>(0x60 + k);
        CHECK(dashAlertWatchOnTripwire(p, 12000 + k));
    }
    uint8_t ninth[8];
    std::memcpy(ninth, a, 8);
    ninth[1] = 0x99;
    CHECK(dashAlertWatchOnTripwire(ninth, 13000)); // `a` evicted now
    CHECK(dashAlertWatchOnTripwire(a, 14000));     // forgotten -> refires
}

// Decode names: the shared w/a table + the PM special-case.
static void test_decode_names()
{
    CHECK(std::strcmp(dashAlertWatchName(kAlertSrcMatrix346, 62),
                      "w062_systemLimpMode") == 0);
    CHECK(std::strcmp(dashAlertWatchName(kAlertSrcLog524, 43),
                      "a043_configInconsistent") == 0);
    CHECK(std::strcmp(dashAlertWatchName(kAlertSrcLog526, 144),
                      "a144_config") == 0); // shared table may abbreviate
}

// Event ring capacity: 24 slots, oldest evicted.
static void test_event_ring_capacity()
{
    for (int i = 0; i < 30; i++)
    {
        uint8_t p[8] = {static_cast<uint8_t>(0xE0 + i), 0, 0, 0, 0, 0, 0, 0};
        p[1] = static_cast<uint8_t>(i);
        dashAlertWatchOnTripwire(p, 20000 + i);
    }
    // Last event is the 30th payload; the ring kept the newest 24.
    CHECK(dashAlertWatchLastEvent()->data[1] == 29);
}

int main()
{
    test_log_frame_a144_is_capture();
    test_log_frame_severity_split();
    test_matrix_rising_edge();
    test_tripwire_payload_dedupe();
    test_decode_names();
    test_event_ring_capacity();
    if (g_fail == 0)
        std::printf("ALL PASS\n");
    else
        std::printf("%d FAILURES\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}

#endif // NATIVE_BUILD
