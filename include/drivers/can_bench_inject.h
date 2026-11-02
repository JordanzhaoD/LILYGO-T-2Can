#pragma once
// can_bench_inject.h — bench-only CAN frame injection rings (2026-11-02).
//
// Field-crash forensics: the in-car can_rt PANIC (7x, pc=0/ra=0, fingerprint
// 0x249 + dlc + ROM-libc) never reproduces on the bench because rv2_flood
// feeds dashRecV2Submit directly — the REAL per-frame paths on can_rt are
// never exercised with real frame content. These rings close that gap: the
// serial DIAG command rv2_frame pushes frames (replayed from a captured
// session CSV by the host) into the lane matching their captured bus, and
// each driver's read() pops its lane on can_rt, so replayed frames flow
// through the exact field paths:
//   lane 0 -> TWAIDriver::read          -> appLoop dispatch (handler,
//                                          plugins, postProcess) = bus 1
//   lane 1 -> P4TwaiSecondaryDriver(1)  -> t2canDrainSecondary
//                                          (handleMessage)      = bus 2
//   lane 2 -> P4TwaiSecondaryDriver(2)  -> t2canDrainCan3
//                                          (mcpDashSniffCan3)   = bus 3
//
// P4-only. Each lane is single producer (serial DIAG task) / single
// consumer (can_rt). Guarded by a critical section; no heap ops inside.
#include "../can_frame_types.h"
#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)
#include <freertos/FreeRTOS.h>

struct CanBenchInjectRing
{
    static constexpr int kCap = 64;
    CanFrame frames[kCap];
    volatile int head = 0; // producer index
    volatile int tail = 0; // consumer index
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    bool push(const CanFrame &f)
    {
        portENTER_CRITICAL(&mux);
        int next = (head + 1) % kCap;
        bool ok = next != tail;
        if (ok)
        {
            frames[head] = f;
            head = next;
        }
        portEXIT_CRITICAL(&mux);
        return ok;
    }

    bool pop(CanFrame &out)
    {
        portENTER_CRITICAL(&mux);
        bool ok = head != tail;
        if (ok)
        {
            out = frames[tail];
            tail = (tail + 1) % kCap;
        }
        portEXIT_CRITICAL(&mux);
        return ok;
    }

    int depth()
    {
        portENTER_CRITICAL(&mux);
        int d = (head - tail + kCap) % kCap;
        portEXIT_CRITICAL(&mux);
        return d;
    }
};

struct CanBenchInject
{
    static constexpr int kLanes = 3;
    CanBenchInjectRing lanes[kLanes];

    static CanBenchInject &instance() { static CanBenchInject inj; return inj; }
};

// lane: 0 = primary (bus 1), 1 = secondary (bus 2), 2 = tertiary (bus 3).
inline bool canBenchInjectPush(int lane, const CanFrame &f)
{
    if (lane < 0 || lane >= CanBenchInject::kLanes)
        return false;
    return CanBenchInject::instance().lanes[lane].push(f);
}
inline bool canBenchInjectPop(int lane, CanFrame &out)
{
    if (lane < 0 || lane >= CanBenchInject::kLanes)
        return false;
    return CanBenchInject::instance().lanes[lane].pop(out);
}
inline int canBenchInjectDepth(int lane)
{
    if (lane < 0 || lane >= CanBenchInject::kLanes)
        return 0;
    return CanBenchInject::instance().lanes[lane].depth();
}
#else
inline bool canBenchInjectPush(int, const CanFrame &) { return false; }
inline bool canBenchInjectPop(int, CanFrame &) { return false; }
inline int canBenchInjectDepth(int) { return 0; }
#endif
