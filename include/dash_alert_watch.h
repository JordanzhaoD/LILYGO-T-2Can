#pragma once

// Alert Telemetry — passive five-source sentinel core for downgrade-day
// forensics (design: docs/superpowers/specs/2026-11-13-alert-telemetry-design.md,
// plan: docs/superpowers/plans/2026-11-14-alert-telemetry.md).
//
// Pure core only: sentinel detection (0x346 rising edges, 0x526/0x5A5/0x524
// log frames), tripwire payload dedupe, severity classification, the
// 14-ID watch-set table, decode names, and a 24-slot RAM event ring.
// No heap, no console, no framework calls. Vehicle-facing wiring (CanC
// dispatch hook, PSRAM ring, NVS persistence, auto-capture executor,
// WebUI) lives in src/main.cpp and include/web/mcp2515_dashboard.h.
//
// Sentinel semantics (spec §5.1 + §8 rulings):
//   * 0x368 tripwire events arrive via OnTripwire (the ban-forge core
//     owns template comparison; wiring feeds observed deviant payloads).
//     Dedupe is per DISTINCT 8-byte payload over a rolling set of 8 —
//     on downgrade day every frame deviates at ~10 Hz and an undeduped
//     stream would flood the 24-slot log in 2.4 s (§8-①).
//   * 0x346 is a standing bitmap the vehicle re-broadcasts ~2.6 s cadence
//     and latches until vehicle power-down; only 0→1 RISING EDGES make
//     events, so re-broadcasts dedupe naturally. A fresh boot with empty
//     RAM state re-events latched bits deliberately (§8-③: "bit X was
//     latched at boot N" is itself forensic fact; bootSeq tags it).
//   * 0x526/0x5A5/0x524 are event-driven logs, silent in normal driving —
//     any frame is an event, code = b0 (a-code space).
//
// Severity (spec §5.2, Jordan ruling 2026-11-14): CAPTURE = tripwire any,
// 0x524 any (powertrain management class), log a144 (config alert) /
// a062 (limp), matrix w062/w063/w064. Everything else is LOG-only.
//
// Linkage: plain `inline` functions + `inline` globals (dash_ban_forge.h
// precedent) — this header is included from BOTH translation units and
// `static` would fork the state silently.

#if defined(DRIVER_P4_3CAN)

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ---- watch set (spec §5.1, pinned) --------------------------------------
static constexpr uint32_t kAlertWatchIds[] = {
    0x368, 0x346, 0x526, 0x5A5, 0x524,              // sentinel five
    0x3EE, 0x398,                                   // observation anchors
    0x2F1, 0x126, 0x356, 0x3C5, 0x396, 0x33A, 0x728 // Stage-B fingerprint family
};
static constexpr size_t kAlertWatchIdCount =
    sizeof(kAlertWatchIds) / sizeof(kAlertWatchIds[0]);

// Frame record for the forensic ring — layout-compatible with the
// recorder's 18-byte grid record (frozen; wiring memcpy's between them).
struct DashAlertRec
{
    uint32_t tsUs;
    uint32_t id;
    uint8_t dlc;
    uint8_t flags;
    uint8_t data[8];
} __attribute__((packed));
static_assert(sizeof(DashAlertRec) == 18, "alert record layout is frozen");

// ---- event model ---------------------------------------------------------
enum : uint16_t
{
    kAlertSrcTripwire = 1,
    kAlertSrcMatrix346 = 2,
    kAlertSrcLog526 = 3,
    kAlertSrcLog5A5 = 4,
    kAlertSrcLog524 = 5,
};
enum : uint8_t
{
    kSevLog = 0,
    kSevCapture = 1,
};

struct DashAlertEvent
{
    uint32_t bootSeq;
    uint32_t tMs;
    uint16_t src;
    uint8_t code; // matrix: bit 1-64 (w-code); logs: b0 (a-code); tripwire: 0
    uint8_t sev;
    uint8_t data[8];
    bool captureFailed; // §8-④: auto-capture denied (capacity) marks the event
};

inline uint32_t g_alertBootSeq = 0;
inline uint32_t g_alertCaptureSeq = 0; // capture-severity events ever (SM trigger)
inline DashAlertEvent g_alertEvents[24] = {};
inline size_t g_alertEventCount = 0;  // number of valid events (capped at 24)
inline size_t g_alertEventNext = 0;   // ring write slot
inline uint8_t g_alertMatrix[8] = {}; // last observed 0x346 bitmap
inline bool g_alertMatrixSeen = false;
inline uint8_t g_alertDeviant[8][8] = {}; // rolling distinct-payload set
inline size_t g_alertDeviantCount = 0;
inline size_t g_alertDeviantNext = 0;

inline void dashAlertWatchSetBootSeq(uint32_t seq) { g_alertBootSeq = seq; }

inline void dashAlertWatchPushEvent(const DashAlertEvent &e)
{
    g_alertEvents[g_alertEventNext] = e;
    g_alertEventNext = (g_alertEventNext + 1) % 24;
    if (g_alertEventCount < 24)
        g_alertEventCount++;
    if (e.sev == kSevCapture)
        g_alertCaptureSeq++; // the state machine's trigger counter
}

inline const DashAlertEvent *dashAlertWatchLastEvent()
{
    if (g_alertEventCount == 0)
        return nullptr;
    size_t last = (g_alertEventNext + 24 - 1) % 24;
    return &g_alertEvents[last];
}

// ---- severity (spec §5.2) ------------------------------------------------
inline uint8_t dashAlertWatchClassify(uint32_t id, uint8_t code)
{
    if (id == 0x368 || id == 0x524)
        return kSevCapture; // tripwire any; PM any = powertrain class
    if (id == 0x526 || id == 0x5A5)
        return (code == 144 || code == 62) ? kSevCapture : kSevLog;
    if (id == 0x346)
        return (code == 62 || code == 63 || code == 64) ? kSevCapture : kSevLog;
    return kSevLog;
}

// ---- decode names ---------------------------------------------------------
// w/a share one code space on the DI/DIS side (a144 = the whole-car config
// alert family; a124 = cruise fault; a062 = limp reason). PM has its own
// alert list — a043_configInconsistent carries the GTW_autopilot vs
// expectedAutopilot pair, the write-back-route probe (spec §1).
struct DashAlertNameEntry
{
    uint8_t code;
    const char *name;
};

static constexpr DashAlertNameEntry kAlertWNames[64] = {
    {1, "w001_hwPhaseAgateDrive"},
    {2, "w002_hwPhaseBgateDrive"},
    {3, "w003_hwPhaseCgateDrive"},
    {4, "w004_hwPhaseApeak"},
    {5, "w005_hwPhaseBpeak"},
    {6, "w006_hwPhaseCpeak"},
    {7, "w007_hwHvilNotPresent"},
    {8, "w008_hwEncoderA"},
    {9, "w009_hwEncoderB"},
    {10, "w010_hwPedalMonitorReset"},
    {11, "w011_swPowerStageNotReady"},
    {12, "w012_accel5VSupply"},
    {13, "w013_powerPcbSensor"},
    {14, "w014_activeDamping"},
    {15, "w015_immobilizer"},
    {16, "w016_safeStateApplied"},
    {17, "w017_hwPedalMonitor"},
    {18, "w018_hwLVSupplyUV"},
    {19, "w019_hwAccelPedalPower"},
    {20, "w020_hwMotorEncoder"},
    {21, "w021_hwBusOV"},
    {22, "w022_hw5vSupplyUV"},
    {23, "w023_carNotParked"},
    {24, "w024_selfTest"},
    {25, "w025_phaseApeak"},
    {26, "w026_phaseBpeak"},
    {27, "w027_phaseCpeak"},
    {28, "w028_phaseArms"},
    {29, "w029_phaseBrms"},
    {30, "w030_phaseCrms"},
    {31, "w031_phaseAcurrentOffset"},
    {32, "w032_phaseBcurrentOffset"},
    {33, "w033_phaseAcurrentSensor"},
    {34, "w034_phaseBcurrentSensor"},
    {35, "w035_phaseCurrentBalance"},
    {36, "w036_busOV"},
    {37, "w037_busUV"},
    {38, "w038_busVsensor"},
    {39, "w039_regenLimited"},
    {40, "w040_disMIA"},
    {41, "w041_unintendedReset"},
    {42, "w042_inletSensor"},
    {43, "w043_outletOT"},
    {44, "w044_outletUT"},
    {45, "w045_outletSensor"},
    {46, "w046_statorOT"},
    {47, "w047_statorSensor1"},
    {48, "w048_statorSensor2"},
    {49, "w049_statorSensorDiff"},
    {50, "w050_noStatorSensor"},
    {51, "w051_batteryOvercurrent"},
    {52, "w052_rotorBrazeFail"},
    {53, "w053_externalAlert"},
    {54, "w054_driveInverterOT"},
    {55, "w055_trqCrossCheck"},
    {56, "w056_ambientOT"},
    {57, "w057_ambientUT"},
    {58, "w058_ambientSensor"},
    {59, "w059_heatsinkOT"},
    {60, "w060_heatsinkUT"},
    {61, "w061_heatsinkSensor"},
    {62, "w062_systemLimpMode"},
    {63, "w063_systemGracefulPowerOff"},
    {64, "w064_torqueIntervention"},
};

// Log-frame names beyond the shared w-space (Stage-B decoded trio).
static constexpr DashAlertNameEntry kAlertLogNames[3] = {
    {62, "a062_limpReason"},
    {124, "a124_cruiseFaultReason"},
    {144, "a144_config"},
};

// PM alert list (partial: enum heads + the §1 probe code).
static constexpr DashAlertNameEntry kAlertPmNames[22] = {
    {0, "a000_none"},
    {1, "a001_absoluteTorque"},
    {2, "a002_accumulatedTorque"},
    {3, "a003_torqueReversal"},
    {4, "a004_regenTorque"},
    {5, "a005_torqueInNeutral"},
    {6, "a006_inconsistentDIGear"},
    {7, "a007_watchdogReset"},
    {8, "a008_memoryError"},
    {9, "a009_espMIA"},
    {10, "a010_diMIA"},
    {11, "a011_phaseCurrentIrrational"},
    {12, "a012_canData"},
    {13, "a013_canHardware"},
    {14, "a014_accelPedalError"},
    {15, "a015_shifterMIA"},
    {16, "a016_cruiseMIA"},
    {17, "a017_brakeMIA"},
    {18, "a018_encoderIrrational"},
    {19, "a019_statorTempIrrational"},
    {43, "a043_configInconsistent"},
};

inline const char *dashAlertWatchName(uint16_t src, uint8_t code)
{
    static char fallback[20];
    if (src == kAlertSrcLog524)
    {
        for (const auto &n : kAlertPmNames)
            if (n.code == code)
                return n.name;
    }
    else if (src == kAlertSrcLog526 || src == kAlertSrcLog5A5)
    {
        for (const auto &n : kAlertLogNames)
            if (n.code == code)
                return n.name;
    }
    if (code >= 1 && code <= 64)
        return kAlertWNames[code - 1].name;
    std::snprintf(fallback, sizeof(fallback), "a%03u",
                  static_cast<unsigned>(code));
    return fallback;
}

// ---- forensic watch ring (spec §4 pre-context substrate) ------------------
// Rolling store of the last N watch-set frames in externally provided
// memory (wiring hands over PSRAM; the core never allocates). Full ring
// evicts the oldest; PopOldest drains in insertion order for the
// event-time backfill. A null/short buffer degrades to safe drops.
inline uint8_t *g_alertRingBuf = nullptr;
inline size_t g_alertRingCap = 0;   // in records
inline size_t g_alertRingCount = 0; // valid records
inline size_t g_alertRingNext = 0;  // write slot

inline void dashAlertWatchRingInit(uint8_t *buf, size_t capBytes)
{
    g_alertRingBuf = buf;
    g_alertRingCap = buf ? capBytes / sizeof(DashAlertRec) : 0;
    g_alertRingCount = 0;
    g_alertRingNext = 0;
}

inline bool dashAlertWatchIdWatched(uint32_t id)
{
    for (size_t i = 0; i < kAlertWatchIdCount; i++)
        if (kAlertWatchIds[i] == id)
            return true;
    return false;
}

inline bool dashAlertWatchRingPush(uint32_t id, uint8_t dlc, const uint8_t *data,
                                   uint32_t tsUs)
{
    if (g_alertRingBuf == nullptr || g_alertRingCap == 0 ||
        data == nullptr || !dashAlertWatchIdWatched(id))
        return false;
    DashAlertRec *slot = reinterpret_cast<DashAlertRec *>(
        g_alertRingBuf + g_alertRingNext * sizeof(DashAlertRec));
    slot->tsUs = tsUs;
    slot->id = id;
    slot->dlc = dlc < 8 ? dlc : 8;
    // The whole watch set lives on CanC and is receive-only: bus tag 3,
    // dir R (spec §5.1 — matches the recorder grid's flags encoding).
    slot->flags = 3;
    std::memset(slot->data, 0, 8);
    std::memcpy(slot->data, data, slot->dlc);
    g_alertRingNext = (g_alertRingNext + 1) % g_alertRingCap;
    if (g_alertRingCount < g_alertRingCap)
        g_alertRingCount++; // else: just evicted the oldest
    return true;
}

inline size_t dashAlertWatchRingCount() { return g_alertRingCount; }

inline bool dashAlertWatchRingPopOldest(DashAlertRec *out)
{
    if (out == nullptr || g_alertRingCount == 0)
        return false;
    size_t oldest = (g_alertRingNext + g_alertRingCap - g_alertRingCount) %
                    g_alertRingCap;
    const DashAlertRec *slot = reinterpret_cast<const DashAlertRec *>(
        g_alertRingBuf + oldest * sizeof(DashAlertRec));
    std::memcpy(out, slot, sizeof(DashAlertRec));
    g_alertRingCount--;
    return true;
}

// ---- capture state machine (spec §5.2 + §8 rulings) ------------------------
// poll returns each action exactly once per phase; wiring executes it and
// reports back via the notify entries. Time enters ONLY through nowMs.
enum class DashAlertAction : uint8_t
{
    None,
    ArmFlush, // wiring: capacity-check + evict + arm stream + drain ring
    Lock,     // wiring: dashRecV2LockRequest(pre, post, mask)
};
struct DashAlertPoll
{
    DashAlertAction action;
    uint32_t preMs;
    uint32_t postMs;
    uint8_t busMask;
};

enum class DashAlertSm : uint8_t
{
    Idle,
    PendingFlush, // ArmFlush delivered, awaiting NotifyCaptureBegin
    Capturing,    // post window running, retriggers extend (cap 60 s)
    LockPending,  // Lock delivered, awaiting NotifyLockDone
    Cooldown,     // 60 s anti-hammer gap after a finished/denied capture
};

inline DashAlertSm g_alertSm = DashAlertSm::Idle;
inline uint32_t g_alertSmSeqSeen = 0;  // captureSeq consumed by the SM
inline uint32_t g_alertSmDeadline = 0; // post-window end (Capturing) or cooldown end
inline uint32_t g_alertSmCapBegin = 0; // capture start (extension cap anchor)
inline uint32_t g_alertSmLastNow = 0;  // latest poll clock (notify anchors)
inline bool g_alertExternalRecording = false;
inline const char *g_alertDenyReason = nullptr; // last NotifyCaptureBegin(false) reason

// §8-④: post window base 30 s, retrigger +15 s, hard cap 60 s from begin.
static constexpr uint32_t kAlertPostBaseMs = 30000;
static constexpr uint32_t kAlertPostExtMs = 15000;
static constexpr uint32_t kAlertPostCapMs = 60000;
static constexpr uint32_t kAlertCooldownMs = 60000;
// Lock window: pre must cover the ring backfill depth (ring holds up to
// ~4 min at watch-set rates; 300 s is within the recorder's 10-600 s range).
static constexpr uint32_t kAlertLockPreMs = 300000;
static constexpr uint8_t kAlertBusMask = 0x05; // CanA + CanC (spec §5.2)

inline void dashAlertWatchSetExternalRecording(bool active)
{
    g_alertExternalRecording = active;
}

inline void dashAlertWatchNotifyCaptureBegin(bool ok, const char *reason)
{
    if (g_alertSm != DashAlertSm::PendingFlush)
        return;
    if (ok)
    {
        g_alertSm = DashAlertSm::Capturing;
        g_alertSmCapBegin = g_alertSmLastNow;
        g_alertSmDeadline = g_alertSmCapBegin + kAlertPostBaseMs;
    }
    else
    {
        // §8-④ capacity degrade: mark the triggering event, keep the
        // reason for /status, and cool down so a starved grid cannot
        // turn every capture event into an arm/deny spin.
        g_alertDenyReason = reason;
        DashAlertEvent *e = const_cast<DashAlertEvent *>(
            dashAlertWatchLastEvent());
        if (e)
            e->captureFailed = true;
        g_alertSmSeqSeen = g_alertCaptureSeq;
        g_alertSm = DashAlertSm::Cooldown;
        g_alertSmDeadline = g_alertSmLastNow + kAlertCooldownMs;
    }
}

inline void dashAlertWatchNotifyLockDone(bool)
{
    if (g_alertSm != DashAlertSm::LockPending)
        return;
    g_alertSmSeqSeen = g_alertCaptureSeq;
    g_alertSm = DashAlertSm::Cooldown;
    g_alertSmDeadline = g_alertSmLastNow + kAlertCooldownMs;
}

inline DashAlertPoll dashAlertWatchPoll(uint32_t nowMs)
{
    g_alertSmLastNow = nowMs;
    DashAlertPoll p{};
    p.busMask = kAlertBusMask;
    p.preMs = kAlertLockPreMs;
    p.postMs = kAlertPostBaseMs;
    switch (g_alertSm)
    {
    case DashAlertSm::Idle:
        if (g_alertCaptureSeq != g_alertSmSeqSeen)
        {
            g_alertSmSeqSeen = g_alertCaptureSeq;
            if (g_alertExternalRecording)
            {
                // Manual recording already rolling: no flush/backfill,
                // just ride the post window and lock (spec §5.2 rule 1).
                g_alertSm = DashAlertSm::Capturing;
                g_alertSmCapBegin = nowMs;
                g_alertSmDeadline = nowMs + kAlertPostBaseMs;
            }
            else
            {
                g_alertSm = DashAlertSm::PendingFlush;
                p.action = DashAlertAction::ArmFlush;
            }
        }
        break;
    case DashAlertSm::PendingFlush:
        break; // wiring owns this phase; nothing until NotifyCaptureBegin
    case DashAlertSm::Capturing:
        if (g_alertCaptureSeq != g_alertSmSeqSeen)
        {
            // Retrigger: extend the window, capped at begin + 60 s.
            g_alertSmSeqSeen = g_alertCaptureSeq;
            uint32_t extended =
                g_alertSmDeadline + kAlertPostExtMs;
            uint32_t cap = g_alertSmCapBegin + kAlertPostCapMs;
            g_alertSmDeadline = extended > cap ? cap : extended;
        }
        if ((int32_t)(nowMs - g_alertSmDeadline) >= 0)
        {
            g_alertSm = DashAlertSm::LockPending;
            p.action = DashAlertAction::Lock;
        }
        break;
    case DashAlertSm::LockPending:
        break; // wiring is locking
    case DashAlertSm::Cooldown:
        if ((int32_t)(nowMs - g_alertSmDeadline) >= 0)
            g_alertSm = DashAlertSm::Idle;
        break;
    }
    return p;
}

// ---- sentinels ------------------------------------------------------------

inline bool dashAlertWatchOnTripwire(const uint8_t deviant[8], uint32_t nowMs)
{
    for (size_t i = 0; i < g_alertDeviantCount; i++)
        if (std::memcmp(g_alertDeviant[i], deviant, 8) == 0)
            return false; // §8-①: distinct payloads only
    std::memcpy(g_alertDeviant[g_alertDeviantNext], deviant, 8);
    g_alertDeviantNext = (g_alertDeviantNext + 1) % 8;
    if (g_alertDeviantCount < 8)
        g_alertDeviantCount++;

    DashAlertEvent e{};
    e.bootSeq = g_alertBootSeq;
    e.tMs = nowMs;
    e.src = kAlertSrcTripwire;
    e.code = 0;
    e.sev = kSevCapture;
    std::memcpy(e.data, deviant, 8);
    dashAlertWatchPushEvent(e);
    return true;
}

// Returns true when at least one new event was recorded.
inline bool dashAlertWatchOnFrame(uint32_t id, uint8_t dlc, const uint8_t *data,
                                  uint32_t nowMs)
{
    if (dlc < 1 || data == nullptr)
        return false;
    if (id == 0x346)
    {
        if (dlc < 8)
            return false;
        bool any = false;
        for (uint8_t byte = 0; byte < 8; byte++)
        {
            uint8_t risen = data[byte] & ~g_alertMatrix[byte];
            // LSB-first packing, empirically pinned by Stage B: byte7
            // bit5 (0x20) = w062 systemLimpMode — 7*8+5+1 = 62.
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                if (risen & (1u << bit))
                {
                    uint8_t code =
                        static_cast<uint8_t>(byte * 8 + bit + 1); // w-code 1-64
                    DashAlertEvent e{};
                    e.bootSeq = g_alertBootSeq;
                    e.tMs = nowMs;
                    e.src = kAlertSrcMatrix346;
                    e.code = code;
                    e.sev = dashAlertWatchClassify(id, code);
                    std::memcpy(e.data, data, 8);
                    dashAlertWatchPushEvent(e);
                    any = true;
                }
            }
        }
        std::memcpy(g_alertMatrix, data, 8);
        g_alertMatrixSeen = true;
        return any;
    }
    if (id == 0x526 || id == 0x5A5 || id == 0x524)
    {
        DashAlertEvent e{};
        e.bootSeq = g_alertBootSeq;
        e.tMs = nowMs;
        e.src = (id == 0x526)   ? kAlertSrcLog526
                : (id == 0x5A5) ? kAlertSrcLog5A5
                                : kAlertSrcLog524;
        e.code = data[0];
        e.sev = dashAlertWatchClassify(id, e.code);
        std::memcpy(e.data, data, dlc < 8 ? dlc : 8);
        dashAlertWatchPushEvent(e);
        return true;
    }
    return false; // non-sentinel watch-set IDs are ring business (Task 2)
}

#endif // DRIVER_P4_3CAN
