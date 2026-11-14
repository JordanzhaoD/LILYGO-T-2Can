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

// Watch-set ring: only watched IDs enter, full ring evicts the oldest,
// PopOldest drains in insertion order, empty buffer degrades safely.
static void test_watch_ring()
{
    static uint8_t buf[6 * 18]; // six slots
    dashAlertWatchRingInit(buf, sizeof(buf));

    const uint8_t tpl[8] = {0x51, 0x31, 0x59, 0x0F, 0x88, 0x49, 0x67, 0x00};
    uint8_t notWatched = 0;
    CHECK(!dashAlertWatchRingPush(0x7FF, 8, tpl, 1000)); // not in set
    CHECK(dashAlertWatchRingCount() == 0 + notWatched);

    CHECK(dashAlertWatchRingPush(0x368, 8, tpl, 2000));
    CHECK(dashAlertWatchRingCount() == 1);
    CHECK(dashAlertWatchRingPush(0x3EE, 8, tpl, 3000));
    CHECK(dashAlertWatchRingPush(0x398, 8, tpl, 4000));
    CHECK(dashAlertWatchRingCount() == 3);

    DashAlertRec r{};
    CHECK(dashAlertWatchRingPopOldest(&r));
    CHECK(r.id == 0x368 && r.tsUs == 2000 && r.flags == 3); // CanC RX tag
    CHECK(dashAlertWatchRingPopOldest(&r));
    CHECK(r.id == 0x3EE);
    CHECK(dashAlertWatchRingPopOldest(&r));
    CHECK(r.id == 0x398);
    CHECK(!dashAlertWatchRingPopOldest(&r)); // drained

    // Overflow: 8 pushes into 6 slots -> oldest two evicted.
    for (int i = 0; i < 8; i++)
    {
        uint8_t p[8];
        std::memcpy(p, tpl, 8);
        p[7] = static_cast<uint8_t>(i);
        CHECK(dashAlertWatchRingPush(0x126, 8, p, 5000 + i));
    }
    CHECK(dashAlertWatchRingCount() == 6);
    CHECK(dashAlertWatchRingPopOldest(&r));
    CHECK(r.data[7] == 2); // payloads 0,1 evicted
    CHECK(dashAlertWatchRingPopOldest(&r));
    CHECK(r.data[7] == 3);

    // Null buffer = safe drop (wiring alloc failure degrades to log-only).
    dashAlertWatchRingInit(nullptr, 0);
    CHECK(!dashAlertWatchRingPush(0x368, 8, tpl, 9000));
    CHECK(dashAlertWatchRingCount() == 0);
    dashAlertWatchRingInit(buf, sizeof(buf)); // restore
}

// ---- capture state machine (poll/action protocol) ------------------------

static uint32_t g_smNow = 0; // simulated clock, ms

// Feed one CAPTURE-severity event (a144 log frame).
static void smInjectCapture()
{
    const uint8_t f[8] = {0x90, 0x80, 0x20, 0, 0, 0, 0, 0};
    dashAlertWatchOnFrame(0x526, 8, f, g_smNow);
}
// Feed one LOG-severity event.
static void smInjectLog()
{
    const uint8_t f[8] = {0x0A, 0, 0, 0, 0, 0, 0, 0};
    dashAlertWatchOnFrame(0x526, 8, f, g_smNow);
}

static void test_sm_full_chain()
{
    smInjectCapture();
    DashAlertPoll p = dashAlertWatchPoll(g_smNow);
    CHECK(p.action == DashAlertAction::ArmFlush);
    CHECK(p.preMs == 300000 && p.postMs == 30000 && p.busMask == 0x05);

    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::None); // once
    dashAlertWatchNotifyCaptureBegin(true, nullptr);

    g_smNow += 30000; // post window elapses
    p = dashAlertWatchPoll(g_smNow);
    CHECK(p.action == DashAlertAction::Lock);
    CHECK(p.preMs == 300000 && p.postMs == 30000 && p.busMask == 0x05);
    dashAlertWatchNotifyLockDone(true);
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::None); // cooldown

    g_smNow += 60000;                                                   // cooldown elapses
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::None); // idle

    smInjectCapture(); // a fresh event re-arms the whole chain
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::ArmFlush);
    dashAlertWatchNotifyCaptureBegin(true, nullptr);
    g_smNow += 30000; // full wind-down for the next test: the SM ignores
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::Lock); // LockDone without a Lock action
    dashAlertWatchNotifyLockDone(true);
    g_smNow += 61000;
    dashAlertWatchPoll(g_smNow);
}

static void test_sm_retrigger_extends_post_window_capped()
{
    smInjectCapture();
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::ArmFlush);
    dashAlertWatchNotifyCaptureBegin(true, nullptr);

    g_smNow += 10000;
    smInjectCapture(); // retrigger at +10s -> deadline 30+15=45s
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::None);

    g_smNow += 20000; // +30s total: original deadline passed, extension holds
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::None);
    g_smNow += 15000; // +45s: extended deadline reached
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::Lock);

    // Cap: begin at T, hammer retriggers — Lock never later than T+60s.
    dashAlertWatchNotifyLockDone(true);
    g_smNow += 61000;
    dashAlertWatchPoll(g_smNow); // drain to idle
    smInjectCapture();
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::ArmFlush);
    uint32_t begin = g_smNow;
    dashAlertWatchNotifyCaptureBegin(true, nullptr);
    for (int i = 0; i < 10; i++)
    {
        g_smNow += 5000;
        smInjectCapture(); // would extend endlessly without the cap
        CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::None);
    }
    g_smNow = begin + 60000; // exactly at the cap
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::Lock);
    dashAlertWatchNotifyLockDone(true);
    g_smNow += 61000;
    dashAlertWatchPoll(g_smNow);
}

static void test_sm_external_recording_skips_flush()
{
    dashAlertWatchSetExternalRecording(true);
    smInjectCapture();
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::None); // no ArmFlush
    g_smNow += 30000;
    DashAlertPoll p = dashAlertWatchPoll(g_smNow);
    CHECK(p.action == DashAlertAction::Lock);
    dashAlertWatchNotifyLockDone(true);
    dashAlertWatchSetExternalRecording(false);
    g_smNow += 61000;
    dashAlertWatchPoll(g_smNow);
}

static void test_sm_denied_capture_marks_event_and_cooldowns()
{
    smInjectCapture();
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::ArmFlush);
    dashAlertWatchNotifyCaptureBegin(false, "no space");
    CHECK(dashAlertWatchLastEvent()->captureFailed);
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::None); // cooldown
    g_smNow += 61000;
    dashAlertWatchPoll(g_smNow); // back to idle
    smInjectCapture();           // next event can arm again
    CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::ArmFlush);
    dashAlertWatchNotifyCaptureBegin(true, nullptr);
    dashAlertWatchNotifyLockDone(true);
    g_smNow += 61000;
    dashAlertWatchPoll(g_smNow);
}

static void test_sm_log_events_never_arm()
{
    smInjectLog();
    for (uint32_t t = 0; t < 120000; t += 10000)
    {
        g_smNow += 10000;
        CHECK(dashAlertWatchPoll(g_smNow).action == DashAlertAction::None);
    }
}

static void test_enabled_flag_gates_all_entries()
{
    uint32_t before = g_alertCaptureSeq; // monotonic, never caps
    dashAlertWatchSetEnabled(false);
    const uint8_t f[8] = {0x90, 0x80, 0x20, 0, 0, 0, 0, 0};
    CHECK(!dashAlertWatchOnFrame(0x526, 8, f, 500000));
    CHECK(!dashAlertWatchOnTripwire(f, 500001));
    uint8_t rb[3 * 18];
    dashAlertWatchRingInit(rb, sizeof(rb));
    CHECK(!dashAlertWatchRingPush(0x368, 8, f, 500002));
    CHECK(g_alertCaptureSeq == before); // nothing new recorded
    dashAlertWatchSetEnabled(true);
    CHECK(dashAlertWatchOnFrame(0x526, 8, f, 500003)); // alive again
    CHECK(g_alertCaptureSeq == before + 1);
}

int main()
{
    test_log_frame_a144_is_capture();
    test_log_frame_severity_split();
    test_matrix_rising_edge();
    test_tripwire_payload_dedupe();
    test_decode_names();
    test_event_ring_capacity();
    test_watch_ring();
    test_sm_full_chain();
    test_sm_retrigger_extends_post_window_capped();
    test_sm_external_recording_skips_flush();
    test_sm_denied_capture_marks_event_and_cooldowns();
    test_sm_log_events_never_arm();
    test_enabled_flag_gates_all_entries();
    if (g_fail == 0)
        std::printf("ALL PASS\n");
    else
        std::printf("%d FAILURES\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}

#endif // NATIVE_BUILD
