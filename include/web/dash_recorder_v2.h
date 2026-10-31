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

// Bin size via stat() — the espidf File shim has no size().
static size_t recV2FileSize(const String &path)
{
    struct stat st;
    return (stat(path.c_str(), &st) == 0) ? (size_t)st.st_size : 0;
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
// a 128KB head-room reserve for meta writes and FS bookkeeping.
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
        size_t freeNow = SPIFFS.totalBytes() > used + 128 * 1024
                             ? SPIFFS.totalBytes() - used - 128 * 1024
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
    size_t freeBytes = SPIFFS.totalBytes() > used + 128 * 1024
                           ? SPIFFS.totalBytes() - used - 128 * 1024
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
    portEXIT_CRITICAL(&recV2Mux);

    File recFile = SPIFFS.open(binPath, "w");
    if (!recFile)
        writeFailed = true; // named below, on the single failure path
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
            }
        }
        if (!writeFailed && batchLen > 0 &&
            recFile.write((const uint8_t *)batch,
                          batchLen * sizeof(RecRingFrame)) !=
                batchLen * sizeof(RecRingFrame))
            writeFailed = true;
        recFile.close();
    }

    uint32_t overflowNow = ringOverflow;
    bool overflowInWindow = (overflowNow > recV2OverflowAtLock) ||
                            (windowUs > dashRecV2SpanUs());

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
    if (recV2LastError.length())
    {
        j += R"JSON(,"last_error":")JSON";
        j += recV2LastError;
        j += "\"";
    }
    j += "}";
    return j;
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

#endif // DASH_RECORDER_V2
#endif // DASH_RECORDER_V2_H
