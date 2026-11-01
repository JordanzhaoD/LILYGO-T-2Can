// Recorder v2 — always-on PSRAM ring buffer (blackbox) for the P4-3CAN board.
//
// Design: docs/superpowers/specs/2026-11-01-can-recorder-v2-design.md (D1/D6).
// The ring ingests ALL frames from all three buses, un-gated, from boot until
// power-off; per-bus switches only mask what a lock exports (D1 ruling).
// Timestamps are esp_timer_get_time() microseconds (32-bit, wraps every
// 71.6 min); every window computation must use unsigned subtraction, never
// ordered compares. The whole engine is macro-gated so the shared dashboard
// header stays byte-identical on every non-P4 board (T2CAN red line, D8).
#ifndef DASH_RECORDER_V2_H
#define DASH_RECORDER_V2_H

#if defined(DASH_RECORDER_V2)

// Serial/String are provided by the including translation unit: the
// dashboard header pulls platform/espidf_runtime.h (ESP-IDF builds) or
// Arduino.h (native) before this file.
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <ArduinoJson.h>

#include "can_frame_types.h"
#include "lwip/apps/sntp.h"

// 18-byte packed ring record (spec D6 field list; the "16B" label in the
// spec prose was an arithmetic slip — 4+4+1+1+8 = 18). flags: bit0-1 bus
// (1=A/2=B/3=C), bit2-3 dir (0=R/1=T/2=E), bit4-7 reserved zero.
struct RecRingFrame
{
    uint32_t tsUs;
    uint32_t id;
    uint8_t dlc;
    uint8_t flags;
    uint8_t data[8];
} __attribute__((packed));
static_assert(sizeof(RecRingFrame) == 18, "ring record layout is frozen");

static constexpr uint8_t kRecV2BusA = 1;
static constexpr uint8_t kRecV2BusB = 2;
static constexpr uint8_t kRecV2BusC = 3;

// Ring storage. Single array; head = oldest live frame, count = live frames.
// RX frames arrive from one task (app_can_task drains all three buses) and
// TX frames from sender contexts, so submit guards the update with a short
// critical section — cheaper than lock-free indexing and enough here.
static RecRingFrame *ring = nullptr;
static uint32_t ringCapacity = 0;
static volatile uint32_t ringHead = 0;
static volatile uint32_t ringCount = 0;
static volatile uint32_t ringOverflow = 0;
static portMUX_TYPE recV2Mux = portMUX_INITIALIZER_UNLOCKED;
static bool recV2Initialized = false;

// --- Blackbox partition: frame grid + header (task 2, B2) -------------------
// Raw-partition recorder skeleton. The 9MB `blackbox` partition holds ONE
// global 18-byte frame grid anchored at a 32B header: frame k lives at
// kRecBBHeaderSize + k*18 forever — 64KB blocks are erase units only and
// never re-anchor the grid (a frame may straddle a block edge). Geometry
// comes from the found esp_partition_t; the constants below are grid
// constants, and a partition smaller than the design is refused.
static constexpr const char *kRecBBPartLabel = "blackbox";
static constexpr uint16_t kRecBBVersion = 1;
static constexpr char kRecBBMagic[5] = "BBX1";
static constexpr uint32_t kRecBBHeaderSize = 32u;
static constexpr uint32_t kRecBBBlockSize = 65536u; // erase unit
static constexpr uint32_t kRecBBBlockCount = 144u;  // 144 * 64KB = 9MB

struct RecBBHeader
{
    char magic[4];
    uint16_t version;
    uint16_t headerSize; // grid anchor travels with the header
    uint8_t reserved[24];
} __attribute__((packed));
static_assert(sizeof(RecBBHeader) == 32, "blackbox header layout is frozen");

static const esp_partition_t *recBBPart = nullptr;
static bool recBBEnabled = false;
static uint32_t recBBWriteOff = 0; // next grid byte to write (writer lands in task 3)

// Grid addressing: header-anchored, block edges deliberately ignored.
static uint32_t recBBFrameOff(uint32_t idx)
{
    return kRecBBHeaderSize + idx * sizeof(RecRingFrame);
}

// Locate the partition, stamp a fresh header on first boot (or a stale
// layout), and arm the engine. Any failure leaves recBBEnabled=false —
// the PSRAM ring above keeps running and lock/export degrade later.
static bool recBBInitPartition()
{
    recBBPart = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                         (esp_partition_subtype_t)0x40,
                                         kRecBBPartLabel);
    if (!recBBPart)
    {
        recBBEnabled = false;
        Serial.println("[REC2] blackbox partition missing - recorder disabled");
        return false;
    }
    if (recBBPart->size < kRecBBBlockSize * kRecBBBlockCount)
    {
        recBBEnabled = false;
        Serial.printf("[REC2] blackbox partition too small (%u < %u) - recorder disabled\n",
                      (unsigned)recBBPart->size,
                      (unsigned)(kRecBBBlockSize * kRecBBBlockCount));
        return false;
    }
    RecBBHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    bool fresh = esp_partition_read(recBBPart, 0, &hdr, sizeof(hdr)) != ESP_OK ||
                 memcmp(hdr.magic, kRecBBMagic, 4) != 0 ||
                 hdr.version != kRecBBVersion;
    if (fresh)
    {
        RecBBHeader stamp;
        memset(&stamp, 0, sizeof(stamp));
        memcpy(stamp.magic, kRecBBMagic, 4);
        stamp.version = kRecBBVersion;
        stamp.headerSize = (uint16_t)kRecBBHeaderSize;
        if (esp_partition_erase_range(recBBPart, 0, kRecBBBlockSize) != ESP_OK ||
            esp_partition_write(recBBPart, 0, &stamp, sizeof(stamp)) != ESP_OK)
        {
            recBBEnabled = false;
            Serial.println("[REC2] blackbox header stamp failed - recorder disabled");
            return false;
        }
    }
    recBBWriteOff = kRecBBHeaderSize;
    recBBEnabled = true;
    Serial.printf("[REC2] blackbox ready: %u bytes at 0x%08x (%s header)\n",
                  (unsigned)recBBPart->size, (unsigned)recBBPart->address,
                  fresh ? "fresh" : "existing");
    return true;
}

// --- Lock state machine (task 2) -------------------------------------------
// idle → arming (post window counting down) → flushing (one-shot task) → idle.
enum RecV2LockState : uint8_t
{
    kRecV2Idle = 0,
    kRecV2Arming = 1,
    kRecV2Flushing = 2
};
static volatile uint8_t recV2State = kRecV2Idle;
static volatile bool recV2Writing = false; // single-writer guard (one flush task ever)
static uint32_t recV2PreMsReq = 0;
static uint32_t recV2PreMsAct = 0;
static uint32_t recV2PostMs = 0;
static uint8_t recV2BusMask = 0;
static uint32_t recV2LockStartMs = 0;
static uint32_t recV2OverflowAtLock = 0;
static uint32_t recV2TwaiMissedSnapshot[3] = {0, 0, 0};
static String recV2LastError = "";
static uint32_t recV2Seq = 0; // seq of the most recent session (0 = none)

// --- Wall clock (task 4) -----------------------------------------------------
// Sessions prefer a wall timestamp; until SNTP lands, meta omits "wall"
// rather than writing a fake 1970 date. The STA-connected edge kicks the
// sync off (dashboard hook) and the 1s loop tick keeps probing until time()
// is plausible — the edge fires once, but SNTP lands seconds later.
static volatile bool wallSynced = false;
static bool sntpStarted = false;

static void dashRecV2TrySyncTime()
{
    if (wallSynced)
        return;
    if (!sntpStarted)
    {
        // espidf framework: lwIP SNTP, not Arduino configTime (ledger, task 4).
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, "pool.ntp.org");
        sntp_setservername(1, "ntp.aliyun.com");
        sntp_init();
        sntpStarted = true;
        Serial.println("[REC2] SNTP sync started");
    }
    // Pre-sync time() returns 1970-era garbage; only trust a post-2020 epoch.
    if (time(nullptr) > (time_t)1600000000)
    {
        wallSynced = true;
        Serial.println("[REC2] wall clock synced");
    }
}

static bool dashRecV2HasWall()
{
    return wallSynced;
}

// Per-bus fps (1s rolling window, sampled in dashRecV2Loop).
static volatile uint32_t recV2FpsAccum[3] = {0, 0, 0};
static uint32_t recV2FpsWindowMs = 0;
static uint32_t recV2Fps[3] = {0, 0, 0};

static uint8_t recV2DirBits(char dir)
{
    switch (dir)
    {
    case 'T':
        return 1;
    case 'E':
        return 2;
    default:
        return 0; // 'R'
    }
}

// Idempotent: called once from dashboard setup after the SPIFFS mount.
// 20MB ≈ 9.7 min of full-rate three-bus history (1800fps × 600s × 18B ≈
// 19.4MB); falls back to 10MB, then to disabled (ring == nullptr: submit
// becomes a no-op and the status API reports the engine as disabled).
// Crash forensics (field fix 2026-11-01): the in-car reboots left no evidence
// — reset reasons died with each power cycle and no serial was attached. The
// ledger keeps the last 8 boots in NVS as {esp_reset_reason, diedMidFlush}
// pairs; rv2_flushing is raised at flush start and cleared on both completion
// paths, so a boot that reads it set DIED inside a flush. reason=BROWNOUT +
// midFlush=1 is the closed case for the in-car lock crashes; reason=TWDT or
// PANIC would instead point back at firmware.
static void recV2SweepOrphans(); // defined after the session path builders
static void recV2LedgerBoot()
{
    Preferences p;
    p.begin("rec_v2", false);
    uint32_t boots = p.getUInt("rv2_boots", 0) + 1;
    p.putUInt("rv2_boots", boots);
    uint8_t midFlush = p.getUChar("rv2_flushing", 0);
    p.putUChar("rv2_flushing", 0);
    uint8_t slot[16]; // 8 pairs of {reason, midFlush}, oldest first
    memset(slot, 0, sizeof(slot));
    p.getBytes("rv2_resets", slot, sizeof(slot));
    memmove(slot, slot + 2, sizeof(slot) - 2); // drop the oldest pair
    slot[sizeof(slot) - 2] = (uint8_t)esp_reset_reason();
    slot[sizeof(slot) - 1] = midFlush;
    p.putBytes("rv2_resets", slot, sizeof(slot));
    p.end();
    Serial.printf("[REC2] boot #%u reason=%d diedMidFlush=%u\n",
                  (unsigned)boots, (int)esp_reset_reason(),
                  (unsigned)midFlush);
}

static void dashRecV2Init()
{
    if (recV2Initialized)
        return;
    recV2Initialized = true;
    const size_t kPrimaryBytes = 20u * 1024u * 1024u;
    const size_t kFallbackBytes = 10u * 1024u * 1024u;
    size_t bytes = kPrimaryBytes;
    void *mem = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!mem)
    {
        bytes = kFallbackBytes;
        mem = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    }
    if (!mem)
    {
        Serial.println("[REC2] no PSRAM for ring, recorder v2 disabled");
        return;
    }
    ring = static_cast<RecRingFrame *>(mem);
    ringCapacity = bytes / sizeof(RecRingFrame);
    ringHead = 0;
    ringCount = 0;
    ringOverflow = 0;
    Serial.printf("[REC2] ring ready: %u frames (%.1f MB)\n",
                  static_cast<unsigned>(ringCapacity),
                  static_cast<double>(bytes) / (1024.0 * 1024.0));
    recBBInitPartition(); // raw-partition blackbox skeleton (task 2)
    // Boot-time only, before any flush can exist: record why we last died,
    // then reclaim space from flushes that died before their meta write.
    recV2LedgerBoot();
    recV2SweepOrphans();
}

// Hot path: one frame into the ring. Never blocks, never drops the caller's
// frame silently — a full ring overwrites the oldest entry and counts it.
static void dashRecV2Submit(const CanFrame &f, uint8_t busTag, char dir)
{
    if (!ring)
        return;
    RecRingFrame r;
    r.tsUs = static_cast<uint32_t>(esp_timer_get_time());
    r.id = f.id;
    r.dlc = (f.dlc <= 8) ? f.dlc : 8;
    r.flags = static_cast<uint8_t>((busTag & 0x03) | (recV2DirBits(dir) << 2));
    memcpy(r.data, f.data, r.dlc);
    if (r.dlc < 8)
        memset(r.data + r.dlc, 0, 8 - r.dlc);
    portENTER_CRITICAL(&recV2Mux);
    if (ringCount >= ringCapacity)
    {
        // Full: overwrite the oldest slot and advance head (rolling loss,
        // by design — overflow counter lands in every session meta).
        ring[ringHead] = r;
        ringHead = (ringHead + 1 < ringCapacity) ? ringHead + 1 : 0;
        ringOverflow = ringOverflow + 1;
    }
    else
    {
        uint32_t idx = ringHead + ringCount;
        if (idx >= ringCapacity)
            idx -= ringCapacity;
        ring[idx] = r;
        ringCount = ringCount + 1;
    }
    if (busTag >= kRecV2BusA && busTag <= kRecV2BusC)
        recV2FpsAccum[busTag - 1] = recV2FpsAccum[busTag - 1] + 1;
    portEXIT_CRITICAL(&recV2Mux);
}

// Oldest→newest duration in µs. Unsigned subtraction is wrap-safe: when
// tsUs wraps mid-window the difference is still the true elapsed time.
static uint32_t dashRecV2SpanUs()
{
    if (!ring || ringCount == 0)
        return 0;
    portENTER_CRITICAL(&recV2Mux);
    uint32_t oldest = ring[ringHead].tsUs;
    uint32_t lastIdx = ringHead + ringCount - 1;
    if (lastIdx >= ringCapacity)
        lastIdx -= ringCapacity;
    uint32_t newest = ring[lastIdx].tsUs;
    portEXIT_CRITICAL(&recV2Mux);
    return (uint32_t)(newest - oldest);
}

// --- Session files ----------------------------------------------------------
// /sess_<seq>.bin (raw RecRingFrame records) + /sess_<seq>.meta.json.
// seq is NVS-backed and monotonic: rv2_seq holds the last allocated value.
static uint32_t recV2AllocateSeq()
{
    Preferences p;
    p.begin("rec_v2", false);
    uint32_t seq = p.getUInt("rv2_seq", 0) + 1;
    p.putUInt("rv2_seq", seq);
    p.end();
    return seq;
}

static String recV2BinPath(uint32_t seq)
{
    return String("/sess_") + seq + ".bin";
}
static String recV2MetaPath(uint32_t seq)
{
    return String("/sess_") + seq + ".meta.json";
}

// Collect session seq numbers present on SPIFFS (meta files are the source
// of truth: a bin without meta is a cleaned-up failure). Returns count.
static size_t recV2ScanSessions(uint32_t *seqs, size_t cap)
{
    size_t n = 0;
    File root = SPIFFS.open("/");
    if (!root)
        return 0;
    for (File entry = root.openNextFile(); entry; entry = root.openNextFile())
    {
        const char *nm = entry.name(); // e.g. "sess_12.meta.json"
        while (*nm == '/')             // tolerate path-shaped names
            nm++;
        if (strncmp(nm, "sess_", 5) != 0)
            continue;
        const char *rest = nm + 5;
        if (strncmp(rest + strlen(rest) - 10, ".meta.json", 10) != 0)
            continue;
        uint32_t seq = 0;
        bool digits = false;
        while (*rest >= '0' && *rest <= '9')
        {
            seq = seq * 10 + (uint32_t)(*rest - '0');
            rest++;
            digits = true;
        }
        if (digits && *rest == '.' && n < cap)
            seqs[n++] = seq;
    }
    return n;
}

static bool recV2RemoveSession(uint32_t seq)
{
    SPIFFS.remove(recV2MetaPath(seq));
    return SPIFFS.remove(recV2BinPath(seq));
}

// Boot-time janitor (field fix 2026-11-01): a bin without its meta is a flush
// that died before the meta write — the five in-car crashes left exactly that
// (seq 7-12 orphans, one torn mid-batch). Meta is the source of truth, so the
// orphans are invisible to /sessions, undownloadable, and leak their bytes
// forever. Sweep them at boot (init runs before any flush can exist, so this
// can never race a live session). Victims are collected first and removed
// after the directory walk closes.
static void recV2SweepOrphans()
{
    File root = SPIFFS.open("/");
    if (!root)
        return;
    String victims[32];
    size_t nVictims = 0;
    for (File entry = root.openNextFile();
         entry && nVictims < 32; entry = root.openNextFile())
    {
        const char *nm = entry.name(); // basename per the File shim
        while (*nm == '/')
            nm++;
        if (strncmp(nm, "sess_", 5) != 0)
            continue;
        char *end = nullptr;
        unsigned long seq = strtoul(nm + 5, &end, 10);
        if (end == nm + 5 || strncmp(end, ".bin", 4) != 0)
            continue; // metas and anything else pass through
        if (SPIFFS.exists(recV2MetaPath((uint32_t)seq)))
            continue; // live session — keep
        victims[nVictims++] = recV2BinPath((uint32_t)seq);
    }
    root.close();
    uint32_t removed = 0;
    for (size_t i = 0; i < nVictims; i++)
        removed += SPIFFS.remove(victims[i]) ? 1 : 0;
    if (removed)
        Serial.printf("[REC2] swept %u orphan bin(s) from dead flushes\n",
                      (unsigned)removed);
}

// Bin size via the shim — the espidf File shim has no size(), and a raw
// stat() on the SPIFFS-relative path hits the VFS root (ENOENT, size always
// 0 — field bug 2026-11-01, seq 22: 9.6MB bin reported as 0). The shim
// prepends the /spiffs mount prefix the same way exists() does.
static size_t recV2FileSize(const String &path)
{
    return SPIFFS.fileSize(path);
}

// Sessions list, newest first: one JSON object per on-flash meta.
static String dashRecV2SessionsJson()
{
    uint32_t seqs[32];
    size_t n = recV2ScanSessions(seqs, 32);
    // ascending insertion sort, then walk backwards → newest first
    for (size_t i = 1; i < n; i++)
    {
        uint32_t key = seqs[i];
        int k = (int)i - 1;
        while (k >= 0 && seqs[k] > key)
        {
            seqs[k + 1] = seqs[k];
            k--;
        }
        seqs[k + 1] = key;
    }
    String j = "[";
    for (int i = (int)n - 1; i >= 0; i--)
    {
        uint32_t seq = seqs[i];
        if (i < (int)n - 1)
            j += ",";
        j += R"JSON({"seq":)JSON";
        j += String(seq);
        JsonDocument doc;
        File mf = SPIFFS.open(recV2MetaPath(seq), "r");
        if (mf)
        {
            String body = mf.readString();
            mf.close();
            deserializeJson(doc, body);
        }
        if (!doc["wall"].isNull())
        {
            j += R"JSON(,"wall":)JSON";
            j += String(doc["wall"].as<unsigned long>());
        }
        j += R"JSON(,"boot_ms":)JSON";
        j += String(doc["boot_ms"] | 0UL);
        j += R"JSON(,"pre_ms_act":)JSON";
        j += String(doc["pre_ms_act"] | 0UL);
        j += R"JSON(,"post_ms":)JSON";
        j += String(doc["post_ms"] | 0UL);
        j += R"JSON(,"bus_mask":)JSON";
        j += String(doc["bus_mask"] | 0U);
        j += R"JSON(,"frames":{"a":)JSON";
        j += String(doc["frames"]["a"] | 0UL);
        j += R"JSON(,"b":)JSON";
        j += String(doc["frames"]["b"] | 0UL);
        j += R"JSON(,"c":)JSON";
        j += String(doc["frames"]["c"] | 0UL);
        j += R"JSON(,"tx":)JSON";
        j += String(doc["frames"]["tx"] | 0UL);
        j += R"JSON(},"size":)JSON";
        j += String((uint32_t)recV2FileSize(recV2BinPath(seq)));
        j += R"JSON(,"loss_badge":)JSON";
        j += String(doc["ring_overflow_in_window"] | false ? "true" : "false");
        j += "}";
    }
    j += "]";
    return j;
}

// Rolling-delete space reclaim: drop oldest sessions (keeping >= 1) until
// `need` bytes fit. Returns how many were removed. Free space always keeps
// The space floor every flush/reclaim decision shares. 128KB (0.8% of the
// 15MB FS) let a space-pressured flush crawl into deep SPIFFS GC — 1.15KB/s,
// CPU1 starvation, task-wdt prints (bench round 4, 2026-11-01) — while a
// roomy FS writes steadily. ~15% free is the observed safe zone, so the
// reserve keeps every flush's tail above the thrash region.
static constexpr size_t kRecV2SpaceReserve = 2560u * 1024u;

// a head-room reserve for meta writes and FS bookkeeping (shared constant
// above — see kRecV2SpaceReserve).
static size_t recV2ReclaimSpace(size_t need)
{
    size_t removed = 0;
    uint32_t seqs[32];
    for (;;)
    {
        size_t n = recV2ScanSessions(seqs, 32);
        if (n <= 1)
            break; // never delete the last session
        size_t used = SPIFFS.usedBytes();
        size_t freeNow = SPIFFS.totalBytes() > used + kRecV2SpaceReserve
                             ? SPIFFS.totalBytes() - used - kRecV2SpaceReserve
                             : 0;
        if (freeNow >= need)
            break;
        uint32_t oldest = seqs[0];
        for (size_t i = 1; i < n; i++)
            if (seqs[i] < oldest)
                oldest = seqs[i];
        recV2RemoveSession(oldest);
        removed++;
    }
    return removed;
}

// Pre-scan: how many frames in the ring are inside the window and match the
// bus mask. Wrap-safe: age = (uint32_t)(nowUs - tsUs).
static uint32_t recV2CountWindow(uint32_t nowUs, uint32_t windowUs, uint8_t busMask)
{
    if (!ring)
        return 0;
    portENTER_CRITICAL(&recV2Mux);
    uint32_t head = ringHead, count = ringCount;
    portEXIT_CRITICAL(&recV2Mux);
    uint32_t matching = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t idx = head + i;
        if (idx >= ringCapacity)
            idx -= ringCapacity;
        const RecRingFrame &f = ring[idx];
        uint32_t age = (uint32_t)(nowUs - f.tsUs);
        if (age > windowUs)
            continue; // too old (unsigned compare is wrap-safe)
        uint8_t bus = f.flags & 0x03;
        if (bus >= kRecV2BusA && bus <= kRecV2BusC && (busMask & (1 << (bus - 1))))
            matching++;
    }
    return matching;
}

// One-shot flush task: arming expired → copy [flushNow-(pre+post), flushNow]
// from the ring into a session file, then write the meta. The ring keeps
// recording throughout (copy iterates a head/count snapshot; the writer can
// only lap this reader if it overwrites 20MB while we scan, i.e. never).
static void recV2FlushTask(void *)
{
    const uint32_t nowUs = (uint32_t)esp_timer_get_time();
    uint32_t preMs = recV2PreMsAct ? recV2PreMsAct : recV2PreMsReq;
    bool shrunk = false;
    recV2ReclaimSpace(
        (size_t)recV2CountWindow(nowUs, (preMs + recV2PostMs) * 1000UL,
                                 recV2BusMask) *
        sizeof(RecRingFrame));
    size_t used = SPIFFS.usedBytes();
    size_t freeBytes = SPIFFS.totalBytes() > used + kRecV2SpaceReserve
                           ? SPIFFS.totalBytes() - used - kRecV2SpaceReserve
                           : 0;
    size_t estBytes = (size_t)recV2CountWindow(nowUs, (preMs + recV2PostMs) * 1000UL,
                                               recV2BusMask) *
                      sizeof(RecRingFrame);
    if (estBytes > freeBytes && estBytes > 0)
    {
        // Space shrink, single proportional pass, keeping the most recent
        // part of the window (the incident is usually seconds old).
        uint32_t scaled = (uint32_t)(((uint64_t)preMs * freeBytes) / estBytes);
        if (scaled > preMs)
            scaled = preMs;
        preMs = scaled;
        recV2PreMsAct = scaled;
        shrunk = true;
    }

    const uint32_t windowUs = (preMs + recV2PostMs) * 1000UL;
    const uint32_t seq = recV2AllocateSeq();
    const String binPath = recV2BinPath(seq);

    uint32_t framesA = 0, framesB = 0, framesC = 0, framesTx = 0;
    uint32_t startUs = 0, endUs = 0;
    bool writeFailed = false;

    portENTER_CRITICAL(&recV2Mux);
    uint32_t head = ringHead, count = ringCount;
    // Badge inputs come from this same scan snapshot: by meta-write time the
    // ring has kept advancing (SPIFFS writes take seconds) and a live span
    // read there would false-positive the loss badge.
    uint32_t oldestTs = count ? ring[head].tsUs : 0;
    uint32_t lastIdx = count ? (head + count - 1) : 0;
    if (count && lastIdx >= ringCapacity)
        lastIdx -= ringCapacity;
    uint32_t newestTs = count ? ring[lastIdx].tsUs : 0;
    uint32_t overflowAtScan = ringOverflow;
    portEXIT_CRITICAL(&recV2Mux);
    // Retention the scan actually saw (0 µs when the ring is empty).
    uint32_t spanUs = count ? (uint32_t)(newestTs - oldestTs) : 0;

    // Forensics flag (see recV2LedgerBoot): raised for the whole flush so a
    // boot that finds it set knows the previous session died mid-write.
    {
        Preferences p;
        p.begin("rec_v2", false);
        p.putUChar("rv2_flushing", 1);
        p.end();
    }
    File recFile = SPIFFS.open(binPath, "w");
    if (!recFile)
    {
        writeFailed = true; // named below, on the single failure path
        Preferences p;
        p.begin("rec_v2", false);
        p.putUChar("rv2_flushing", 0);
        p.end();
    }
    else
    {
        RecRingFrame batch[64];
        size_t batchLen = 0;
        for (uint32_t i = 0; i < count && !writeFailed; i++)
        {
            uint32_t idx = head + i;
            if (idx >= ringCapacity)
                idx -= ringCapacity;
            const RecRingFrame &f = ring[idx];
            uint32_t age = (uint32_t)(nowUs - f.tsUs);
            if (age > windowUs)
                continue;
            uint8_t bus = f.flags & 0x03;
            uint8_t dir = (f.flags >> 2) & 0x03;
            if (bus < kRecV2BusA || bus > kRecV2BusC ||
                !(recV2BusMask & (1 << (bus - 1))))
                continue;
            batch[batchLen++] = f;
            if (startUs == 0)
                startUs = f.tsUs;
            endUs = f.tsUs;
            if (dir == 0)
            {
                if (bus == kRecV2BusA)
                    framesA++;
                else if (bus == kRecV2BusB)
                    framesB++;
                else
                    framesC++;
            }
            else
                framesTx++;
            if (batchLen == 64)
            {
                if (recFile.write((const uint8_t *)batch,
                                  batchLen * sizeof(RecRingFrame)) !=
                    batchLen * sizeof(RecRingFrame))
                    writeFailed = true;
                batchLen = 0;
                // Pace the writer (bench round 4): one yield per batch bounds
                // this task's CPU bursts — CPU1's idle task gets scheduled
                // between batches instead of starving into task-wdt prints —
                // and the flash-write current draws spread out, which also
                // widens the car's brownout margin. One 10ms tick per 1152B
                // caps throughput at ~115KB/s, far above the ~8KB/s the FS
                // actually sustains, so pacing costs nothing real.
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        if (!writeFailed && batchLen > 0 &&
            recFile.write((const uint8_t *)batch,
                          batchLen * sizeof(RecRingFrame)) !=
                batchLen * sizeof(RecRingFrame))
            writeFailed = true;
        recFile.close();
        // Common completion path (success or mid-write failure): the bin is
        // sealed, so the next boot must not read diedMidFlush=1.
        {
            Preferences p;
            p.begin("rec_v2", false);
            p.putUChar("rv2_flushing", 0);
            p.end();
        }
    }

    uint32_t overflowNow = ringOverflow;
    // D7: evictions that only removed frames older than this window are
    // harmless — they were rolling out regardless. The badge lights only
    // when the ring actually evicted since lock (frames were LOST, not
    // merely never recorded after boot) AND the window reaches past the
    // retention the scan saw. Both from the scan snapshot: once the ring
    // reaches steady-state full every submit evicts, so an OR here (the
    // pre-review form) made the badge always-true (final review, 2026-11-01).
    bool overflowInWindow = (overflowAtScan > recV2OverflowAtLock) &&
                            (windowUs > spanUs);

    if (writeFailed)
    {
        // session write failed: scrub the partial file so it can never
        // masquerade as a complete session, and surface the reason.
        recV2LastError = "session write failed";
        SPIFFS.remove(binPath);
        Serial.printf("[REC2] session %u flush FAILED\n", (unsigned)seq);
    }
    else
    {
        String meta = "{";
        meta += R"JSON("seq":)JSON";
        meta += String(seq);
        meta += R"JSON(,"boot_ms":)JSON";
        meta += String(millis());
        meta += R"JSON(,"trigger":"manual")JSON";
        // Wall timestamp only when SNTP has landed — meta never carries a
        // fake 1970 date (task 4).
        if (dashRecV2HasWall())
        {
            meta += R"JSON(,"wall":)JSON";
            meta += String((uint32_t)time(nullptr));
        }
        meta += R"JSON(,"pre_ms_req":)JSON";
        meta += String(recV2PreMsReq);
        meta += R"JSON(,"pre_ms_act":)JSON";
        meta += String(preMs);
        meta += R"JSON(,"post_ms":)JSON";
        meta += String(recV2PostMs);
        meta += R"JSON(,"bus_mask":)JSON";
        meta += String(recV2BusMask);
        if (shrunk)
            meta += R"JSON(,"shrink_reason":"space")JSON";
        meta += R"JSON(,"frames":{"a":)JSON";
        meta += String(framesA);
        meta += R"JSON(,"b":)JSON";
        meta += String(framesB);
        meta += R"JSON(,"c":)JSON";
        meta += String(framesC);
        meta += R"JSON(,"tx":)JSON";
        meta += String(framesTx);
        meta += R"JSON(},"ring_overflow":)JSON";
        meta += String(overflowNow);
        meta += R"JSON(,"ring_overflow_in_window":)JSON";
        meta += String(overflowInWindow ? "true" : "false");
        meta += R"JSON(,"twai_missed_a":)JSON";
        meta += String(recV2TwaiMissedSnapshot[0]);
        meta += R"JSON(,"twai_missed_b":)JSON";
        meta += String(recV2TwaiMissedSnapshot[1]);
        meta += R"JSON(,"twai_missed_c":)JSON";
        meta += String(recV2TwaiMissedSnapshot[2]);
        meta += R"JSON(,"start_boot_us":)JSON";
        meta += String(startUs);
        meta += R"JSON(,"end_boot_us":)JSON";
        meta += String(endUs);
        meta += "}";
        File metaFile = SPIFFS.open(recV2MetaPath(seq), "w");
        if (metaFile)
        {
            metaFile.print(meta);
            metaFile.close();
            recV2Seq = seq;
            recV2LastError = "";
            Serial.printf("[REC2] session %u flushed: a=%u b=%u c=%u tx=%u "
                          "(pre %lums%s)\n",
                          (unsigned)seq, (unsigned)framesA, (unsigned)framesB,
                          (unsigned)framesC, (unsigned)framesTx,
                          (unsigned long)preMs,
                          shrunk ? ", shrunk" : "");
        }
        else
        {
            recV2LastError = "meta write failed";
            SPIFFS.remove(binPath);
        }
    }

    recV2Writing = false;
    recV2State = kRecV2Idle;
    vTaskDelete(nullptr);
}

static void dashRecV2SetTwaiMissed(uint32_t a, uint32_t b, uint32_t c)
{
    recV2TwaiMissedSnapshot[0] = a;
    recV2TwaiMissedSnapshot[1] = b;
    recV2TwaiMissedSnapshot[2] = c;
}

// Lock the ring: capture the last `preMs` (plus `postMs` of continued
// recording) for the buses in `busMask` (bit0=A, bit1=B, bit2=C).
static bool dashRecV2LockRequest(uint32_t preMs, uint32_t postMs,
                                 uint8_t busMask, String &err)
{
    if (!ring)
    {
        err = "recorder v2 disabled";
        return false;
    }
    if (!busMask)
    {
        err = "bus_mask must select at least one bus";
        return false;
    }
    if (recV2State != kRecV2Idle || recV2Writing)
    {
        err = "recorder busy";
        return false;
    }
    recV2PreMsReq = preMs;
    recV2PreMsAct = 0; // decided at flush time (may shrink)
    recV2PostMs = postMs;
    recV2BusMask = busMask;
    recV2LockStartMs = millis();
    recV2OverflowAtLock = ringOverflow;
    recV2State = kRecV2Arming;
    return true;
}

static String dashRecV2StatusJson()
{
    const char *stateName = recV2State == kRecV2Arming
                                ? "arming"
                                : (recV2State == kRecV2Flushing ? "flushing" : "idle");
    uint32_t remain = 0;
    if (recV2State == kRecV2Arming)
    {
        uint32_t elapsed = (uint32_t)(millis() - recV2LockStartMs);
        remain = elapsed < recV2PostMs ? recV2PostMs - elapsed : 0;
    }
    String j = "{";
    j += R"JSON("enabled":)JSON";
    j += String(ring ? "true" : "false");
    j += R"JSON(,"span_s":)JSON";
    j += String((uint32_t)(dashRecV2SpanUs() / 1000000UL));
    j += R"JSON(,"fps":{"a":)JSON";
    j += String(recV2Fps[0]);
    j += R"JSON(,"b":)JSON";
    j += String(recV2Fps[1]);
    j += R"JSON(,"c":)JSON";
    j += String(recV2Fps[2]);
    j += R"JSON(},"lock":{"state":")JSON";
    j += String(stateName);
    j += R"JSON(","remain_ms":)JSON";
    j += String(remain);
    j += R"JSON(,"pre_ms":)JSON";
    j += String(recV2State == kRecV2Idle ? 0 : recV2PreMsReq);
    j += R"JSON(,"post_ms":)JSON";
    j += String(recV2PostMs);
    j += R"JSON(,"bus_mask":)JSON";
    j += String(recV2State == kRecV2Idle ? 0 : recV2BusMask);
    size_t freeBytes = SPIFFS.totalBytes() > SPIFFS.usedBytes()
                           ? SPIFFS.totalBytes() - SPIFFS.usedBytes()
                           : 0;
    j += R"JSON(},"sessions_space_free":)JSON";
    j += String((uint32_t)freeBytes);
    j += R"JSON(,"seq":)JSON";
    j += String(recV2Seq);
    // Reset ledger, newest first: [reasonCode, diedMidFlush] pairs from NVS.
    // Written at every boot (recV2LedgerBoot) — survives power cycles, so a
    // field run with no serial still classifies its reboots afterwards.
    {
        uint8_t slot[16];
        memset(slot, 0, sizeof(slot));
        Preferences p;
        p.begin("rec_v2", true);
        p.getBytes("rv2_resets", slot, sizeof(slot));
        p.end();
        j += R"JSON(,"resets":[)JSON";
        bool wroteAny = false;
        for (int i = 7; i >= 0; i--)
        {
            uint8_t reason = slot[i * 2], mid = slot[i * 2 + 1];
            if (!reason && !mid)
                continue; // slot never filled
            if (wroteAny)
                j += ",";
            j += "[";
            j += String((unsigned)reason);
            j += ",";
            j += String((unsigned)mid);
            j += "]";
            wroteAny = true;
        }
        j += "]";
    }
    if (recV2LastError.length())
    {
        j += R"JSON(,"last_error":")JSON";
        j += recV2LastError;
        j += "\"";
    }
    j += "}";
    return j;
}

// Human-readable ledger dump for the DIAG console (rv2_resets).
static const char *recV2ResetName(uint8_t code)
{
    // esp_reset_reason_t values this board can actually produce; unknown
    // codes fall through to the number itself.
    switch (code)
    {
    case 1: return "POWERON";
    case 3: return "SW";
    case 4: return "PANIC";
    case 5: return "INT_WDT";
    case 6: return "TASK_WDT";
    case 7: return "TASK_WDT_OTHER";
    case 9: return "BROWNOUT";
    case 10: return "SW_BROWNOUT";
    case 11: return "SDIO";
    case 12: return "USB";
    default: return "?";
    }
}

static void dashRecV2DumpResets()
{
    uint8_t slot[16];
    memset(slot, 0, sizeof(slot));
    Preferences p;
    p.begin("rec_v2", true);
    p.getBytes("rv2_resets", slot, sizeof(slot));
    uint32_t boots = p.getUInt("rv2_boots", 0);
    p.end();
    Serial.printf("[REC2] boots=%u, reset ledger newest first:\n",
                  (unsigned)boots);
    for (int i = 7; i >= 0; i--)
    {
        uint8_t reason = slot[i * 2], mid = slot[i * 2 + 1];
        if (!reason && !mid)
            continue;
        Serial.printf("  boot-%d: reason=%d (%s) diedMidFlush=%u\n",
                      7 - i, (int)reason, recV2ResetName(reason),
                      (unsigned)mid);
    }
}

// Called from the dashboard loop: sample fps windows and advance the lock
// state machine (arming expiry → one-shot flush task, pinned to core 1 like
// the legacy recorder writer).
static void dashRecV2Loop()
{
    if (!ring)
        return;
    uint32_t now = millis();
    if ((uint32_t)(now - recV2FpsWindowMs) >= 1000)
    {
        recV2FpsWindowMs = now;
        dashRecV2TrySyncTime(); // re-probe until SNTP lands (edge fires once)
        for (int i = 0; i < 3; i++)
        {
            recV2Fps[i] = recV2FpsAccum[i];
            recV2FpsAccum[i] = 0;
        }
    }
    if (recV2State == kRecV2Arming &&
        (uint32_t)(now - recV2LockStartMs) >= recV2PostMs)
    {
        recV2State = kRecV2Flushing;
        recV2Writing = true;
        if (xTaskCreatePinnedToCore(recV2FlushTask, "rec2_flush", 6144,
                                    nullptr, 1, nullptr, 1) != pdPASS)
        {
            recV2Writing = false;
            recV2State = kRecV2Idle;
            recV2LastError = "flush task create failed";
        }
    }
}

// --- Bench-only synthetic traffic injector (car-crash repro, 2026-11-01) ---
// The bench has no CAN source, so the multi-MB flush regime was never
// exercised before the field run (in-car lock → flush-phase reboot, 6
// orphan bins, one torn mid-batch). rv2_flood pushes paced synthetic frames
// through the REAL submit path so a subsequent lock reproduces the car's
// flush load: ring writes, fps counters, overflow, SPIFFS burst — all real.
// Paced in 10ms batches (not a tight loop) so the sender task breathes and
// the fps counters report the truth. Blocks the caller for `seconds`.
static void dashRecV2BenchFlood(uint32_t fps, uint32_t seconds, uint8_t busMask)
{
    if (!ring || fps == 0 || seconds == 0)
    {
        Serial.println("[rv2_flood] disabled or no-op args");
        return;
    }
    uint8_t buses[3];
    uint8_t n = 0;
    for (uint8_t b = 0; b < 3; b++)
        if (busMask & (1 << b))
            buses[n++] = kRecV2BusA + b;
    if (!n)
    {
        Serial.println("[rv2_flood] empty bus mask");
        return;
    }
    const uint32_t kTicksPerSec = 100; // 10ms ticks
    const uint32_t perTick = fps / kTicksPerSec;
    const uint32_t frac = fps % kTicksPerSec;
    uint32_t carry = 0;
    uint32_t sent = 0;
    uint32_t id = 0;
    CanFrame f;
    f.dlc = 8;
    for (uint32_t s = 0; s < seconds; s++)
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
            for (uint32_t i = 0; i < burst; i++)
            {
                f.id = 0x100 + (id & 0x3FF);
                id++;
                f.data[0] = (uint8_t)(sent >> 24);
                f.data[1] = (uint8_t)(sent >> 16);
                f.data[2] = (uint8_t)(sent >> 8);
                f.data[3] = (uint8_t)sent;
                dashRecV2Submit(f, buses[sent % n], 'R');
                sent++;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    Serial.printf("[rv2_flood] done: %u frames over %us\n",
                  (unsigned)sent, (unsigned)seconds);
}

#endif // DASH_RECORDER_V2
#endif // DASH_RECORDER_V2_H
