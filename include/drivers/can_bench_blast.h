#pragma once
// can_bench_blast.h — experiment B: NO_ACK self-reception flood replay
// (crash forensics round 7, 2026-11-03).
//
// Round-6/7 verdict: the in-car 12-22s crash family is register-file group
// zeroing under TWAI interrupt pressure (docs/verification/
// field-trip-20261103/ANALYSIS.md section 4). The bench never reproduces
// because its buses are physically silent — rv2_flood/rv2_frame inject
// ABOVE the driver and never exercise the ISR. This runner closes that gap
// the honest way: the selected controllers are reinstalled in
// TWAI_MODE_NO_ACK and frames are transmitted with TWAI_MSG_FLAG_SELF, so
// every frame loops back through the real hardware path: TWAI ISR ->
// driver rx queue -> can_rt drain (dispatch, discovered-ID table,
// handlers, recorder) — the exact chain the car runs and the bench has
// never covered.
//
// Lane map (matches can_bench_inject.h):
//   lane 0 = TWAIDriver (controller 0, bus 1)         -> appLoop dispatch
//   lane 1 = P4TwaiSecondaryDriver (controller 1, b2) -> t2canDrainSecondary
//   lane 2 = P4TwaiSecondaryDriver (controller 2, b3) -> t2canDrainCan3
//
// Frame content: a 160-distinct-id sweep per lane (13 is coprime with
// 2048, so (i*13 + 0x21) & 0x7FF is injective for i < 160) — exactly the
// field trigger, where the bus-2 discovered-ID table fills to its
// 160-entry capacity inside the 12-22s window — plus one hot law id
// (0x249 DND wheel, 0x118 gear, 0x399, 0x045 stalk) every 32nd frame as
// EXTRA frames (never replacing sweep entries, so the table still sees
// all 160 sweep ids and caps at full — the exact table-full dispatch
// branch the field crashes reached).
//
// Pacing mirrors dashRecV2BenchFlood: 10ms ticks with fractional carry,
// fps is PER LANE (3 lanes x 800 fps = 2400 fps total ~= the sess_64
// field rate of 2373). Blocks the caller for the whole duration —
// bench only, never on a live vehicle: this transmits onto the buses.
#include "../can_frame_types.h"
#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)
// p4_3can is a pure ESP-IDF build — millis()/Serial come from the platform
// shim, not Arduino.h (which does not exist in this environment).
#include "../platform/espidf_runtime.h"
#include <driver/twai.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct CanBenchBlastStats
{
    uint32_t tx[3] = {0, 0, 0};
    uint32_t fail[3] = {0, 0, 0};
    uint32_t elapsedMs = 0;
    bool modeSwitchOk = false;
};

// Build the seq-th sweep frame (160 distinct ids; hot law id every 32nd
// frame as an extra — see header comment).
inline void canBenchBlastFrame(CanFrame &f, uint32_t sweepIdx, uint32_t seq)
{
    static const uint32_t kHot[4] = {0x249, 0x118, 0x399, 0x045};
    if ((seq & 31u) == 0)
        f.id = kHot[(seq >> 5) & 3u];
    else
        f.id = ((sweepIdx % 160u) * 13u + 0x21u) & 0x7FFu;
    f.dlc = 8;
    f.bus = 0; // drain loops stamp the real bus
    uint32_t x = seq * 2654435761u; // Knuth multiplicative hash payload
    for (uint8_t i = 0; i < 8; i++)
    {
        f.data[i] = (uint8_t)(x >> ((i & 3u) * 8u));
        x = x * 1664525u + 1013904223u;
    }
}

// One lane per bus: concrete driver pointer + the TWAI mode to restore
// when the blast ends (bus1/bus2 NORMAL, bus3 LISTEN_ONLY). Null drivers
// and unset mask bits are skipped. Always restores modes, even on
// switch failure (best-effort reinstall of the original mode).
template <typename D0, typename D1, typename D2>
CanBenchBlastStats canBenchBlastRun(D0 *lane0, twai_mode_t restore0,
                                    D1 *lane1, twai_mode_t restore1,
                                    D2 *lane2, twai_mode_t restore2,
                                    uint8_t mask, uint32_t fps, uint32_t secs)
{
    CanBenchBlastStats st;
    const uint32_t kMaxSecs = 1800;
    if (secs > kMaxSecs)
        secs = kMaxSecs;
    if (fps == 0)
        fps = 1;

    D0 *d0 = (mask & 1) ? lane0 : nullptr;
    D1 *d1 = (mask & 2) ? lane1 : nullptr;
    D2 *d2 = (mask & 4) ? lane2 : nullptr;

    uint32_t startMs = millis();
    bool ok0 = !d0 || d0->benchSetMode(TWAI_MODE_NO_ACK);
    bool ok1 = !d1 || d1->benchSetMode(TWAI_MODE_NO_ACK);
    bool ok2 = !d2 || d2->benchSetMode(TWAI_MODE_NO_ACK);
    st.modeSwitchOk = ok0 && ok1 && ok2;

    if (st.modeSwitchOk)
    {
        const uint32_t kTicksPerSec = 100; // 10ms ticks, like rv2_flood
        const uint32_t perTick = fps / kTicksPerSec;
        const uint32_t frac = fps % kTicksPerSec;
        uint32_t carry = 0;
        uint32_t sweep[3] = {0, 0, 0};
        uint32_t seq[3] = {0, 0, 0};
        uint32_t nextReport = 10;
        for (uint32_t s = 0; s < secs; s++)
        {
            for (uint32_t t = 0; t < kTicksPerSec; t++)
            {
                uint32_t burst = perTick;
                carry += frac;
                if (carry >= kTicksPerSec)
                {
                    carry -= kTicksPerSec;
                    burst++;
                }
                for (uint8_t lane = 0; lane < 3; lane++)
                {
                    if ((lane == 0 && !d0) || (lane == 1 && !d1) ||
                        (lane == 2 && !d2))
                        continue;
                    for (uint32_t i = 0; i < burst; i++)
                    {
                        CanFrame f;
                        canBenchBlastFrame(f, sweep[lane], seq[lane]);
                        if ((seq[lane] & 31u) != 0)
                            sweep[lane]++; // sweep ids only advance on sweep frames
                        seq[lane]++;
                        bool ok;
                        if (lane == 0)
                            ok = d0->benchSendSelf(f);
                        else if (lane == 1)
                            ok = d1->benchSendSelf(f);
                        else
                            ok = d2->benchSendSelf(f);
                        if (ok)
                            st.tx[lane]++;
                        else
                            st.fail[lane]++;
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            if (s + 1 >= nextReport)
            {
                Serial.printf("[exp_b] t=%us tx=%u/%u/%u fail=%u/%u/%u\n",
                              (unsigned)(s + 1), (unsigned)st.tx[0],
                              (unsigned)st.tx[1], (unsigned)st.tx[2],
                              (unsigned)st.fail[0], (unsigned)st.fail[1],
                              (unsigned)st.fail[2]);
                nextReport += 10;
            }
        }
    }

    // Always restore the pre-experiment modes.
    if (d0)
        d0->benchSetMode(restore0);
    if (d1)
        d1->benchSetMode(restore1);
    if (d2)
        d2->benchSetMode(restore2);
    st.elapsedMs = millis() - startMs;
    return st;
}
#else
struct CanBenchBlastStats
{
    uint32_t tx[3] = {0, 0, 0};
    uint32_t fail[3] = {0, 0, 0};
    uint32_t elapsedMs = 0;
    bool modeSwitchOk = false;
};
#endif
