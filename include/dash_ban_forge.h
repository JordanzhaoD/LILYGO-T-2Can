#pragma once

// Ban Shield X — 0x368 GTW_carConfig tier-bit forge experiment + passive
// template tripwire (design: Atlas-3CAN-P4/docs/
// ban-shield-0x368-forge-experiment-design-20261111.md v3, plan
// docs/superpowers/plans/2026-11-12-ban-shield-x.md).
//
// Pure core only: template check, seven arm transforms, attempt-based
// budget, wallclock cap, inter-arm cooldown, echo-muted tripwire.
// No heap, no Serial, no Arduino calls. Vehicle-facing wiring (CanC
// send path, wantTx OR, NVS, defense_config, /status) lives in
// src/main.cpp and include/web/mcp2515_dashboard.h (plan Task 3).
//
// Semantics (fail-closed everywhere):
//   * Preference default OFF. Runtime armed flag is separate from the
//     preference; an arm auto-disarms when its budget (3 injection
//     attempts) or wallclock cap (600 ms from the FIRST injection
//     attempt) is exhausted, and a 30 s cooldown then gates re-arming.
//   * The tripwire is passive observation and runs even with the forge
//     preference off; it is muted while an arm is live and for 2 s
//     after the last successful injection so our own echo (the
//     transformed bytes returning on CanC) is never mis-read as a
//     template deviation.
//   * Cooldown also starts when an arm that consumed at least one
//     attempt is disarmed explicitly (anti-hammer: arm -> 1 inject ->
//     disarm -> re-arm must not bypass the budget spacing).
//   * Time enters only through OnFrame/OnInjectResult nowMs params;
//     requestArm has no clock, so cooldown is evaluated against the
//     last observed time. On the vehicle 0x368 flows at ~10 Hz, so the
//     last observed time is never stale in practice.
//
// Linkage: plain `inline` functions + `inline` globals (dash_autoshift.h
// style) because this header is included from BOTH src/main.cpp and
// include/web/mcp2515_dashboard.h — `static` linkage would fork the
// state per translation unit and the two would disagree silently.

#if defined(DRIVER_P4_3CAN)

#include <cstdint>
#include <cstring>

// PT 0x368 GTW_carConfig.GTW_autopilot — id is decimal 872.
static constexpr uint32_t kBanForgeId = 872;
// Field template (hex) 51 31 59 0F 88 49 67 00 — byte-identical across
// six weeks and stationary/driving states (sess_120/121/122, 20261028).
static constexpr uint8_t kBanForgeTemplate[8] = {
    0x51, 0x31, 0x59, 0x0F, 0x88, 0x49, 0x67, 0x00};
// Hard-coded budget/caps (not configurable, plan Global Constraints).
static constexpr uint8_t kBanForgeMaxAttempts = 3;
static constexpr uint32_t kBanForgeWindowMs = 600;
static constexpr uint32_t kBanForgeCooldownMs = 30000;
static constexpr uint32_t kBanForgeEchoMuteMs = 2000;

struct DashBanForgeDiag
{
    uint32_t tripwireHits = 0;
    uint32_t injected = 0; // successful sends only (E rows excluded)
    uint32_t armsRun = 0;
    uint8_t lastDeviant[8] = {};
    uint32_t lastDeviantMs = 0;
    uint8_t lastArm = 0;
    bool armed = false;
};

// Internal state (diag-visible fields live in g_banForgeDiag).
inline bool g_banForgePref = false;
inline uint8_t g_banForgeArm = 0;         // 0 none, 1-7 B1..B7
inline uint8_t g_banForgeAttempts = 0;    // attempts consumed this arm
inline uint32_t g_banForgeFirstAttemptMs = 0; // 0 = no attempt yet
inline uint32_t g_banForgeLastInjectOkMs = 0; // 0 = never injected ok
inline uint32_t g_banForgeCooldownUntilMs = 0;
inline uint32_t g_banForgeLastNowMs = 0;
inline DashBanForgeDiag g_banForgeDiag = {};

inline void dashBanForgeSetPref(bool on)
{
    g_banForgePref = on;
    if (!on && g_banForgeDiag.armed)
    {
        // Fail-closed kill: pref off disarms immediately, without
        // waiting for the next frame (Stage A abort semantics).
        g_banForgeDiag.armed = false;
        if (g_banForgeAttempts > 0)
            g_banForgeCooldownUntilMs = g_banForgeLastNowMs + kBanForgeCooldownMs;
        g_banForgeAttempts = 0;
        g_banForgeFirstAttemptMs = 0;
    }
}

inline bool dashBanForgePref(void) { return g_banForgePref; }

// Shared disarm path: cooldown applies when the arm consumed attempts.
inline void dashBanForgeFinish(uint32_t nowMs)
{
    g_banForgeDiag.armed = false;
    if (g_banForgeAttempts > 0)
        g_banForgeCooldownUntilMs = nowMs + kBanForgeCooldownMs;
    g_banForgeAttempts = 0;
    g_banForgeFirstAttemptMs = 0;
    g_banForgeArm = 0;
}

// arm: 0 = disarm, 1-5 = B1-B5, 6 = additive-checksum fallback,
// 7 = whole-b6 probe. Rejects while pref off, while armed, or while
// the inter-arm cooldown is active (evaluated against last observed
// time). Invalid codes (>7) reject.
inline bool dashBanForgeRequestArm(uint8_t arm)
{
    if (arm == 0)
    {
        if (g_banForgeDiag.armed)
            dashBanForgeFinish(g_banForgeLastNowMs);
        return true;
    }
    if (arm > 7)
        return false;
    if (!g_banForgePref || g_banForgeDiag.armed)
        return false;
    if (g_banForgeCooldownUntilMs > g_banForgeLastNowMs)
        return false;
    g_banForgeArm = arm;
    g_banForgeAttempts = 0;
    g_banForgeFirstAttemptMs = 0;
    g_banForgeDiag.armed = true;
    g_banForgeDiag.lastArm = arm;
    g_banForgeDiag.armsRun++;
    return true;
}

inline bool dashBanForgeActive(void) { return g_banForgeDiag.armed; }

// Apply the arm's transform to a template-matching frame. All arms
// rewrite a 3-bit window holding 3 (SELF_DRIVING) to 2 (ENHANCED) —
// the harmless direction; only the target bits change.
inline void dashBanForgeTransform(uint8_t arm, const uint8_t src[8],
                                  uint8_t out[8])
{
    std::memcpy(out, src, 8);
    switch (arm)
    {
    case 1: // B1: b6 bits 7:5
        out[6] = static_cast<uint8_t>((src[6] & ~0xE0u) | (2u << 5));
        break;
    case 2: // B2: b6 bits 3:1
        out[6] = static_cast<uint8_t>((src[6] & ~0x0Eu) | (2u << 1));
        break;
    case 3: // B3: b1 bits 6:4
        out[1] = static_cast<uint8_t>((src[1] & ~0x70u) | (2u << 4));
        break;
    case 4: // B4: b2 bits 5:3
        out[2] = static_cast<uint8_t>((src[2] & ~0x38u) | (2u << 3));
        break;
    case 5: // B5: b3 bits 4:2
        out[3] = static_cast<uint8_t>((src[3] & ~0x1Cu) | (2u << 2));
        break;
    case 6:
    { // B6: B1 + additive b7 (sum of b0..b6 after the B1 edit)
        out[6] = static_cast<uint8_t>((src[6] & ~0xE0u) | (2u << 5));
        uint16_t sum = 0;
        for (int i = 0; i < 7; ++i)
            sum += out[i];
        out[7] = static_cast<uint8_t>(sum & 0xFFu);
        break;
    }
    case 7: // B7: whole b6 probe
        out[6] = 0x00;
        break;
    default:
        break;
    }
}

// RX hook (called for every received frame, any id). Returns true and
// fills out[8] when this frame should be answered with an injection
// (id/dlc/payload exactly match the field template, an arm is live,
// budget remains, and the wallclock cap has not expired). Also runs
// the passive tripwire: any 872 frame that is not the exact template
// counts as deviant unless muted (arm live, or within the 2 s echo
// tail after the last successful injection).
inline bool dashBanForgeOnFrame(uint32_t id, uint8_t dlc,
                                const uint8_t data[8], uint32_t nowMs,
                                uint8_t out[8])
{
    if (nowMs > g_banForgeLastNowMs)
        g_banForgeLastNowMs = nowMs;
    if (id != kBanForgeId)
        return false;

    const bool templateMatch =
        (dlc == 8) && (std::memcmp(data, kBanForgeTemplate, 8) == 0);

    if (!templateMatch)
    {
        const bool echoMuted =
            g_banForgeDiag.armed ||
            (g_banForgeLastInjectOkMs != 0 &&
             nowMs - g_banForgeLastInjectOkMs < kBanForgeEchoMuteMs);
        if (!echoMuted)
        {
            g_banForgeDiag.tripwireHits++;
            std::memcpy(g_banForgeDiag.lastDeviant, data, 8);
            g_banForgeDiag.lastDeviantMs = nowMs;
        }
        return false;
    }

    if (!g_banForgeDiag.armed)
        return false;

    // Wallclock cap: measured from the FIRST injection attempt, so the
    // trigger wait (arm until the first matching frame) never stretches
    // the window.
    if (g_banForgeAttempts > 0 &&
        nowMs - g_banForgeFirstAttemptMs >= kBanForgeWindowMs)
    {
        dashBanForgeFinish(nowMs);
        return false;
    }
    if (g_banForgeAttempts >= kBanForgeMaxAttempts)
    {
        dashBanForgeFinish(nowMs);
        return false;
    }
    dashBanForgeTransform(g_banForgeArm, data, out);
    return true;
}

// Send-result hook (caller must invoke exactly once per true OnFrame,
// with ok = whether the frame actually reached the bus). Every call —
// success or failure — consumes one budget attempt; the first call
// starts the 600 ms wallclock window. Budget exhaustion auto-disarms
// and starts the cooldown.
inline void dashBanForgeOnInjectResult(bool ok, uint32_t nowMs)
{
    if (nowMs > g_banForgeLastNowMs)
        g_banForgeLastNowMs = nowMs;
    if (!g_banForgeDiag.armed)
        return; // stray result after disarm; ignore
    if (++g_banForgeAttempts == 1)
        g_banForgeFirstAttemptMs = nowMs;
    if (ok)
    {
        g_banForgeDiag.injected++;
        g_banForgeLastInjectOkMs = nowMs;
    }
    if (g_banForgeAttempts >= kBanForgeMaxAttempts)
        dashBanForgeFinish(nowMs);
}

inline const DashBanForgeDiag &dashBanForgeDiag(void)
{
    return g_banForgeDiag;
}

#endif // defined(DRIVER_P4_3CAN)
