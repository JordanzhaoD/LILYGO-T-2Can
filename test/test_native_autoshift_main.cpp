// Native harness for the autoshift engine core (dash_autoshift.h).
// Compiled and run by test/test_autoshift_contract.py via:
//   clang++ -std=c++17 -Wall -Wextra -Werror -DDRIVER_P4_3CAN -I include
// The -DDRIVER_P4_3CAN is mandatory: the header lives inside the P4
// guard and preprocesses to nothing without it.
//
// Plain C++ (no Unity): the pytest contract test asserts exit code 0
// and the "ALL PASS" banner.

#include <cstdio>

#include "dash_autoshift.h"

static int g_fail = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            ++g_fail;                                                   \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);  \
        }                                                               \
    } while (0)

// The 16 desk-verified static vectors for stalk state 40 C0:
// dashAsCrc8J1850({40, C0, c0}) == crc for c = 0..F.
static const struct { uint8_t counterByte; uint8_t crc; } kPinned[16] = {
    {0x00, 0xB2}, {0x10, 0x7F}, {0x20, 0x35}, {0x30, 0xF8},
    {0x40, 0xA1}, {0x50, 0x6C}, {0x60, 0x26}, {0x70, 0xEB},
    {0x80, 0x94}, {0x90, 0x59}, {0xA0, 0x13}, {0xB0, 0xDE},
    {0xC0, 0x87}, {0xD0, 0x4A}, {0xE0, 0x00}, {0xF0, 0xCD},
};

int main()
{
    // --- CRC8 J1850 against all 16 pinned vehicle vectors ---
    for (const auto &v : kPinned) {
        const uint8_t frame[3] = {0x40, 0xC0, v.counterByte};
        CHECK(dashAsCrc8J1850(frame, 3) == v.crc);
    }

    // --- compose: pair write, counter +1, low nibble kept, crc recompute ---
    uint8_t out[4];
    const uint8_t src[4] = {0x40, 0xC0, 0x35, 0x00};  // counter 3, lo nibble 5
    CHECK(dashAsSbwCompose(src, 0, kAsGearD, out));
    CHECK(out[0] == kAsProfile.stalkB0[kAsGearD]);
    CHECK(out[1] == kAsProfile.stalkB1[kAsGearD]);
    CHECK(((out[2] >> 4) & 0x0F) == 0x04);            // 3 + 1
    CHECK((out[2] & 0x0F) == 0x05);                   // preserved
    CHECK(out[3] == dashAsCrc8J1850(out, 3));

    // --- compose: counter wraps 0xF -> 0x0 ---
    const uint8_t wrapSrc[4] = {0x40, 0xC0, 0xF5, 0x00};
    CHECK(dashAsSbwCompose(wrapSrc, 0, kAsGearP, out));
    CHECK(((out[2] >> 4) & 0x0F) == 0x00);
    CHECK((out[2] & 0x0F) == 0x05);
    CHECK(out[3] == dashAsCrc8J1850(out, 3));

    // --- freshness budget: age == budget passes, age > budget refuses ---
    CHECK(dashAsSbwCompose(src, kAsProfile.sbwSrcFreshMs, kAsGearD, out));
    CHECK(!dashAsSbwCompose(src, kAsProfile.sbwSrcFreshMs + 1, kAsGearD, out));
    CHECK(!dashAsSbwCompose(src, 0xFFFFFFFFu, kAsGearD, out));

    // --- invalid gear index refuses ---
    CHECK(!dashAsSbwCompose(src, 0, 0xFF, out));
    CHECK(!dashAsSbwCompose(src, 0, kAsGearCount, out));

    // --- sentinel: profile is UNFROZEN until the Task-2 field session ---
    for (int i = 0; i < kAsGearCount; ++i) {
        CHECK(kAsProfile.stalkB0[i] == 0xFF);
        CHECK(kAsProfile.stalkB1[i] == 0xFF);
    }

    // --- DI_gear code -> stalk-table index mapping ---
    CHECK(dashAsGearFromDi(1) == kAsGearP);
    CHECK(dashAsGearFromDi(2) == kAsGearR);
    CHECK(dashAsGearFromDi(3) == kAsGearN);
    CHECK(dashAsGearFromDi(4) == kAsGearD);
    CHECK(dashAsGearFromDi(0) == 0xFF);  // DI INVALID
    CHECK(dashAsGearFromDi(5) == 0xFF);
    CHECK(dashAsGearFromDi(7) == 0xFF);  // DI SNA

    if (g_fail == 0) {
        std::printf("native autoshift: ALL PASS\n");
    }
    return g_fail == 0 ? 0 : 1;
}
