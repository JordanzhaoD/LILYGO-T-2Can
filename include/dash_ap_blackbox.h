#pragma once
// dash_ap_blackbox.h — 4.5.0-beta10 P1-4: NVS flight recorder for the 8.3.6
// anti-jerk coordinator's SURROUNDINGS (not the coordinator itself — the
// module dash_ap_rerequest_activation.h is untouched in beta10).
//
// Why this exists (2026-09-13 field feedback): symptoms 2/3 (after force-exit
// / after long runs, injection stops; 8.3.6 switch cycling does NOT recover,
// only unplugging power does). Anything the switch cycle cannot clear lives
// OUTSIDE the coordinator, in RAM — and power-cycling destroys that RAM. So
// the evidence we need is exactly the evidence a reboot erases. This ring is
// written to NVS as one small blob on every event and reloaded on boot: the
// next /status after an unexpected power loss shows what the box recorded
// before it died (permit-input transitions, intent drops, tick heartbeats,
// round outcomes), turning "unplug to recover" from evidence destruction into
// evidence capture.
//
// Persistence contract: the caller owns storage. After boot the caller loads
// the blob (NVS getBytes) via load(); after every record() that returns true
// the caller re-stores the blob via dump(). The module itself has no ESP
// dependencies so native tests can drive it with RAM-backed blobs.
//
// permitBits layout (mirrors the five permit inputs of t2canApReRequestTick
// in src/main.cpp — observation only, the permit expression is unchanged):
//   bit0 canActive            bit3 otaClear (!vehicleOtaActive)
//   bit1 legacyHandlerActive  bit4 abortGuardOk
//   bit2 driverPresent        all five set == permit true

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum class DashApBbKind : uint8_t
{
    None = 0,
    Boot = 1,          // written once after the NVS load, marks the reboot
    Heartbeat = 2,     // slow liveness pulse (tick counter still advancing)
    RoundStart = 3,    // coordinator left WaitDriverIntent into a handshake
    RoundEnd = 4,      // round finished cleanly / vehicle-side re-arm
    Lock = 5,          // round finished in FailedLocked
    PermitChange = 6,  // one of the five permit inputs flipped
    IntentChange = 7,  // intentPresent flipped
};

struct DashApBbEntry
{
    uint32_t ms;         // millis() at record time (0 for Boot)
    uint32_t tick;       // coordinator tick counter at record time
    uint16_t seq;        // monotonic per boot, continues after a load()
    uint8_t kind;        // DashApBbKind
    uint8_t phase;       // coordinator Phase value snapshot
    uint8_t permitBits;  // layout above
    uint8_t apState;     // 0x399 low nibble snapshot
    char reason[16];     // coordinator reason code, truncated to fit ("" ok)
};

class DashApBlackbox
{
public:
    static constexpr uint8_t kCapacity = 8;
    // Flappy kinds (permit/intent transitions can chatter if an input
    // flickers) are rate-limited: a second record of the SAME kind within
    // this window is dropped so the ring cannot be flooded by one glitch.
    // Distinct kinds are never rate-limited.
    static constexpr uint32_t kFlapGuardMs = 5000;

    void clear()
    {
        count_ = 0;
        nextSeq_ = 1;
        lastFlapMs_[0] = 0;
        lastFlapMs_[1] = 0;
        memset(entries_, 0, sizeof(entries_));
    }

    // Boot-time restore. Only a blob of exactly storageBytes() is accepted
    // (anything else — missing key, format drift — leaves the box cleared).
    // The blob carries all kCapacity slots; never-written slots are all-zero
    // (seq == 0) and are excluded from the live count. seq numbering resumes
    // after the newest restored entry.
    void load(const uint8_t *blob, size_t len)
    {
        clear();
        if (blob == nullptr || len != storageBytes())
            return;
        memcpy(entries_, blob, sizeof(entries_));
        uint8_t live = 0;
        uint16_t maxSeq = 0;
        for (uint8_t i = 0; i < kCapacity; ++i)
        {
            if (entries_[i].seq == 0)
                continue;
            ++live;
            if (entries_[i].seq > maxSeq)
                maxSeq = entries_[i].seq;
        }
        count_ = live;
        nextSeq_ = static_cast<uint16_t>(maxSeq + 1);
    }

    size_t storageBytes() const { return sizeof(entries_); }

    // Serialize the full ring (all kCapacity slots, including unused ones)
    // for NVS. Returns bytes written or 0 if the buffer is too small.
    size_t dump(uint8_t *buf, size_t cap) const
    {
        if (buf == nullptr || cap < sizeof(entries_))
            return 0;
        memcpy(buf, entries_, sizeof(entries_));
        return sizeof(entries_);
    }

    // Returns true if the event was recorded (caller should persist).
    bool record(DashApBbKind kind, uint8_t phase, const char *reason,
                uint8_t permitBits, uint8_t apState, uint32_t ms, uint32_t tick)
    {
        if (static_cast<uint8_t>(kind) == 0 ||
            static_cast<uint8_t>(kind) > static_cast<uint8_t>(DashApBbKind::IntentChange))
            return false;
        if (kind == DashApBbKind::PermitChange || kind == DashApBbKind::IntentChange)
        {
            const uint8_t idx = (kind == DashApBbKind::PermitChange) ? 0 : 1;
            if (lastFlapMs_[idx] != 0 && ms - lastFlapMs_[idx] < kFlapGuardMs &&
                ms >= lastFlapMs_[idx])
                return false;
            lastFlapMs_[idx] = ms;
        }
        // Physically ordered oldest -> newest: shift up, drop the oldest.
        // Eight 32-byte entries make memmove cheaper than a ring index.
        if (count_ == kCapacity)
            memmove(&entries_[1], &entries_[0],
                    (kCapacity - 1) * sizeof(DashApBbEntry));
        else
            memmove(&entries_[1], &entries_[0], count_ * sizeof(DashApBbEntry));
        DashApBbEntry &e = entries_[0];
        e.ms = ms;
        e.tick = tick;
        e.seq = nextSeq_++;
        e.kind = static_cast<uint8_t>(kind);
        e.phase = phase;
        e.permitBits = permitBits;
        e.apState = apState;
        const size_t cap = sizeof(e.reason) - 1;
        if (reason != nullptr)
        {
            strncpy(e.reason, reason, cap);
            e.reason[cap] = '\0';
        }
        else
            e.reason[0] = '\0';
        if (count_ < kCapacity)
            ++count_;
        return true;
    }

    // Stack order: entries()[0] is the NEWEST retained entry, entries()[count()-1]
    // the oldest (a full box drops entries_[kCapacity-1]). The /status JSON
    // walks the array backwards to emit chronological oldest -> newest.
    const DashApBbEntry *entries() const { return entries_; }
    uint8_t count() const { return count_; }

private:
    DashApBbEntry entries_[kCapacity] = {};
    uint8_t count_ = 0;
    uint16_t nextSeq_ = 1;
    uint32_t lastFlapMs_[2] = {0, 0};
};

// ── 4.5.0-beta12 (LittleGong port 5): pure JSON emission ──────────────────
// The entry JSON used to be assembled inline in mcp2515_dashboard.h with
// String appends — the dashboard header is not compiled by native tests, so
// THREE releases (beta06 / beta07 / beta10) shipped quote-closure bugs here
// that every test suite passed. Moving the emission into this native-
// compilable header lets golden-string tests pin every kind × reason
// combination character-by-character before firmware ever links.

inline const char *dashApBbKindName(uint8_t kind)
{
    switch (kind)
    {
    case 1: return "boot";
    case 2: return "heartbeat";
    case 3: return "roundStart";
    case 4: return "roundEnd";
    case 5: return "lock";
    case 6: return "permitChange";
    case 7: return "intentChange";
    default: return "?";
    }
}

// Serialize ONE entry exactly as /status emits it (no trailing comma):
// {"seq":N,"kind":"NAME","ms":N,"tick":N,"phase":N,"permitBits":N,
//  "apState":N,"reason":"R"}
// Writes a NUL-terminated string into out and returns the length written
// (excluding the NUL). Returns 0 and out[0]='\0' if the buffer is too small
// (callers pass kEntryJsonCap; a zero here means the entry is skipped, never
// a truncated/garbled JSON fragment).
static constexpr size_t kDashApBbEntryJsonCap = 160;
inline size_t dashApBbEntryToJson(char *out, size_t cap, const DashApBbEntry &e)
{
    if (cap < kDashApBbEntryJsonCap)
    {
        if (cap) out[0] = '\0';
        return 0;
    }
    size_t n = 0;
    auto num = [&](uint32_t v)
    {
        char tmp[12];
        size_t k = 0;
        do { tmp[k++] = static_cast<char>('0' + (v % 10)); v /= 10; } while (v);
        while (k) out[n++] = tmp[--k];
    };
    auto lit = [&](const char *s)
    {
        while (*s) out[n++] = *s++;
    };
    lit("{\"seq\":");            num(e.seq);
    lit(",\"kind\":\"");         lit(dashApBbKindName(e.kind));
    lit("\",\"ms\":");           num(e.ms);
    lit(",\"tick\":");           num(e.tick);
    lit(",\"phase\":");          num(e.phase);
    lit(",\"permitBits\":");     num(e.permitBits);
    lit(",\"apState\":");        num(e.apState);
    lit(",\"reason\":\"");       lit(e.reason);
    lit("\"}");
    out[n] = '\0';
    return n;
}
