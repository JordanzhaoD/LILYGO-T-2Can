// Native harness for the Ban Shield X forge core (dash_ban_forge.h).
// Compiled and run by test/test_ban_forge_contract.py via:
//   clang++ -std=c++17 -Wall -Wextra -Werror -DDRIVER_P4_3CAN -I include
// The -DDRIVER_P4_3CAN is mandatory: the header lives inside the P4
// guard and preprocesses to nothing without it.
//
// Plain C++ (no Unity): the pytest contract test asserts exit code 0
// and the "ALL PASS" banner.
//
// PIO-inert guard: PlatformIO's native test builder globs top-level
// test/*.cpp into EVERY native unit build, where this file would fail
// (no -DDRIVER_P4_3CAN there) and collide with Unity's main() even if
// it compiled. The pytest clang++ invocation never defines NATIVE_BUILD,
// so the guard is transparent to the real consumer of this file.
#ifndef NATIVE_BUILD

#include <cstdio>
#include <cstring>

#include "dash_ban_forge.h"

static int g_fail = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            ++g_fail;                                                   \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);  \
        }                                                               \
    } while (0)

// Field template (hex): 51 31 59 0F 88 49 67 00 — six weeks + three
// states byte-identical (sess_120/121/122, 20261028).
static const uint8_t kTemplate[8] = {0x51, 0x31, 0x59, 0x0F, 0x88, 0x49, 0x67, 0x00};

// Expected injection payloads per arm on the field template.
// Bit windows all hold 3 (SELF_DRIVING) in the template; each arm
// rewrites its window to 2 (ENHANCED) — the harmless direction.
static const uint8_t kExpected[8][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0}, // 0: disarm, no transform
    {0x51, 0x31, 0x59, 0x0F, 0x88, 0x49, 0x47, 0x00}, // 1 B1 b6[7:5]
    {0x51, 0x31, 0x59, 0x0F, 0x88, 0x49, 0x65, 0x00}, // 2 B2 b6[3:1]
    {0x51, 0x21, 0x59, 0x0F, 0x88, 0x49, 0x67, 0x00}, // 3 B3 b1[6:4]
    {0x51, 0x31, 0x51, 0x0F, 0x88, 0x49, 0x67, 0x00}, // 4 B4 b2[5:3]
    {0x51, 0x31, 0x59, 0x0B, 0x88, 0x49, 0x67, 0x00}, // 5 B5 b3[4:2]
    {0x51, 0x31, 0x59, 0x0F, 0x88, 0x49, 0x47, 0x02}, // 6 B6 b6[7:5] + additive b7
    {0x51, 0x31, 0x59, 0x0F, 0x88, 0x49, 0x00, 0x00}, // 7 B7 whole b6 = 0
};

int main()
{
    uint8_t out[8];

    // -- constants pin ------------------------------------------------
    CHECK(kBanForgeId == 872);
    CHECK(kBanForgeMaxAttempts == 3);
    CHECK(kBanForgeWindowMs == 600);
    CHECK(kBanForgeCooldownMs == 30000);
    CHECK(kBanForgeEchoMuteMs == 2000);

    // -- pref off: fail closed ----------------------------------------
    // Boot default is pref off; nothing may arm or inject.
    CHECK(!dashBanForgePref());
    CHECK(!dashBanForgeRequestArm(1));
    CHECK(!dashBanForgeActive());
    CHECK(!dashBanForgeOnFrame(872, 8, kTemplate, 1000, out));

    // -- template match + B1 + budget/cap/cooldown ---------------------
    dashBanForgeSetPref(true);
    CHECK(dashBanForgePref());
    CHECK(dashBanForgeRequestArm(1));
    CHECK(dashBanForgeActive());
    CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 1000, out));
    CHECK(std::memcmp(out, kExpected[1], 8) == 0);
    // wrong id / wrong dlc / deviant payload never inject
    CHECK(!dashBanForgeOnFrame(873, 8, kTemplate, 1010, out));
    CHECK(!dashBanForgeOnFrame(872, 7, kTemplate, 1020, out));
    CHECK(!dashBanForgeOnFrame(872, 9, kTemplate, 1030, out));
    {
        uint8_t deviant[8];
        std::memcpy(deviant, kTemplate, 8);
        deviant[0] = 0x52;
        CHECK(!dashBanForgeOnFrame(872, 8, deviant, 1040, out));
    }
    // deviant while armed is tripwire-muted (echo protection)
    CHECK(dashBanForgeDiag().tripwireHits == 0);
    // three attempts, then budget exhaustion auto-disarms + cooldown
    dashBanForgeOnInjectResult(true, 1000); // attempt 1, window starts
    CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 1050, out));
    dashBanForgeOnInjectResult(true, 1050); // attempt 2
    CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 1100, out));
    dashBanForgeOnInjectResult(true, 1100); // attempt 3 -> disarm
    CHECK(!dashBanForgeActive());
    CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 1200, out) == false);
    CHECK(dashBanForgeDiag().injected == 3);
    CHECK(dashBanForgeDiag().armsRun == 1);
    CHECK(dashBanForgeDiag().lastArm == 1);
    // re-arm inside the 30 s cooldown is rejected
    CHECK(!dashBanForgeRequestArm(1));

    // -- transforms B2..B7 ---------------------------------------------
    // Pure transform checks: arm, one template frame, disarm without any
    // inject result (zero attempts -> no cooldown), next arm immediately.
    // First advance simulated time past the cooldown with an unrelated
    // id (refreshes the core's last-observed clock; never touches the
    // tripwire) — the cooldown check in requestArm has no wall clock of
    // its own and evaluates against that last-observed time.
    CHECK(!dashBanForgeOnFrame(0x106, 8, kTemplate, 40000, out));
    for (uint8_t arm = 2; arm <= 7; ++arm)
    {
        CHECK(dashBanForgeRequestArm(arm));
        CHECK(dashBanForgeActive());
        CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 40100 + arm * 100, out));
        CHECK(std::memcmp(out, kExpected[arm], 8) == 0);
        CHECK(dashBanForgeRequestArm(0));
        CHECK(!dashBanForgeActive());
    }
    CHECK(dashBanForgeDiag().armsRun == 7);
    // invalid arm codes rejected
    CHECK(!dashBanForgeRequestArm(8));

    // -- wallclock cap measured from FIRST injection --------------------
    CHECK(dashBanForgeRequestArm(1));
    CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 50000, out));
    dashBanForgeOnInjectResult(true, 50000); // window starts here
    CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 50050, out));
    dashBanForgeOnInjectResult(true, 50050); // attempt 2 of 3
    // 700 ms after the first injection: window (600 ms) expired even
    // though only 2 of 3 attempts are spent -> no injection, auto-disarm.
    CHECK(!dashBanForgeOnFrame(872, 8, kTemplate, 50700, out));
    CHECK(!dashBanForgeActive());

    // -- tripwire: at-rest hit, armed mute, echo-tail mute ---------------
    // Cooldown from the wallclock disarm runs to 50700+30000 = 80700.
    // At rest, outside any arm/echo window, a deviant 872 payload hits.
    {
        // B1-echo bytes (what our own injection looks like on the bus):
        // must hit at rest — that is the whole point of the tripwire.
        CHECK(!dashBanForgeOnFrame(872, 8, kExpected[1], 80000, out));
        CHECK(dashBanForgeDiag().tripwireHits == 1);
        CHECK(std::memcmp(dashBanForgeDiag().lastDeviant, kExpected[1], 8) == 0);
        CHECK(dashBanForgeDiag().lastDeviantMs == 80000);
    }
    // Arm again (past cooldown at 81000); deviants are muted while armed.
    // Advance the core's last-observed clock past the cooldown first.
    CHECK(!dashBanForgeOnFrame(0x106, 8, kTemplate, 81000, out));
    CHECK(dashBanForgeRequestArm(1));
    CHECK(!dashBanForgeOnFrame(872, 8, kExpected[2], 81050, out));
    CHECK(dashBanForgeDiag().tripwireHits == 1);
    CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 81100, out));
    dashBanForgeOnInjectResult(true, 81100);
    CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 81200, out));
    dashBanForgeOnInjectResult(true, 81200);
    CHECK(dashBanForgeOnFrame(872, 8, kTemplate, 81300, out));
    dashBanForgeOnInjectResult(true, 81300); // budget -> disarm
    CHECK(!dashBanForgeActive());
    // Inside the 2 s echo tail after the last injection: muted.
    CHECK(!dashBanForgeOnFrame(872, 8, kExpected[1], 81400, out));
    CHECK(dashBanForgeDiag().tripwireHits == 1);
    // Past the echo tail: hits again.
    CHECK(!dashBanForgeOnFrame(872, 8, kExpected[1], 83400, out));
    CHECK(dashBanForgeDiag().tripwireHits == 2);
    CHECK(dashBanForgeDiag().lastDeviantMs == 83400);

    // -- pref off mid-arm: immediate kill (Stage A abort semantics) ------
    // Cooldown from 81300 runs to 111300; refresh lastKnown time first
    // with an unrelated frame (non-872 ids never touch the tripwire).
    CHECK(!dashBanForgeOnFrame(0x106, 8, kTemplate, 112000, out));
    dashBanForgeSetPref(false);
    CHECK(!dashBanForgeRequestArm(5));
    CHECK(!dashBanForgeOnFrame(872, 8, kTemplate, 112100, out));
    CHECK(!dashBanForgeActive());
    // tripwire is passive observation: it runs even with the forge pref
    // off — that is its job (catch server pushes any time).
    CHECK(!dashBanForgeOnFrame(872, 8, kExpected[1], 112200, out));
    CHECK(dashBanForgeDiag().tripwireHits == 3);
    // pref back on: arms work again; killing the pref mid-arm disarms
    // immediately (no frame needs to arrive first).
    dashBanForgeSetPref(true);
    CHECK(dashBanForgeRequestArm(5));
    CHECK(dashBanForgeActive());
    dashBanForgeSetPref(false);
    CHECK(!dashBanForgeActive());
    CHECK(!dashBanForgeOnFrame(872, 8, kTemplate, 112300, out));

    if (g_fail == 0)
        std::printf("native banforge: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

#endif // NATIVE_BUILD
