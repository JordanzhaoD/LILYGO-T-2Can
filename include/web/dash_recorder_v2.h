// Recorder v2 — always-on raw-partition blackbox for the P4-3CAN board.
//
// Design: docs/superpowers/specs/2026-11-01-can-recorder-v2-design.md (D1/D6).
// The engine ingests ALL frames from all three buses, un-gated, from boot
// until power-off into a 9MB flash grid via PSRAM staging; per-bus switches
// only mask what a lock exports (D1 ruling).
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

// Ingest-side state. RX frames arrive from one task (app_can_task drains
// all three buses) and TX frames from sender contexts, so submit guards
// the staging update with a short critical section — cheaper than
// lock-free indexing and enough here. ringOverflow counts frames dropped
// when both staging buffers are full (the loss badge's input).
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
// Sub-chunk granularity for every erase (task-7 field fix): a whole-64KB
// erase runs 150ms typical / 2000ms datasheet-max with the flash guard
// masking interrupts on the erasing core — enough to trip the 300ms
// interrupt watchdog (INT_WDT on CPU1, bench boot #21, 2026-11-01). 4KB
// sector erases (typ 45ms) keep every guard window a fraction of the WDT
// budget; the yield between sub-erases lets ticks run.
static constexpr uint32_t kRecBBEraseChunk = 4096u;

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

// Erase `len` bytes at `off` as 4KB sub-chunks with a yield between guard
// windows (see kRecBBEraseChunk). Never called with recV2Mux held — the
// yield sleeps. Returns false on the first failed sub-erase.
static bool recBBEraseRangeSafe(uint32_t off, uint32_t len)
{
    while (len > 0)
    {
        uint32_t chunk = len < kRecBBEraseChunk ? len : kRecBBEraseChunk;
        if (esp_partition_erase_range(recBBPart, off, chunk) != ESP_OK)
            return false;
        off += chunk;
        len -= chunk;
        if (len > 0)
            vTaskDelay(pdMS_TO_TICKS(1)); // let ticks/interrupts breathe
    }
    return true;
}

// Stamp the 32B partition header. Called by init's fresh path AND by the
// writer's block-0 erase: the header lives inside erase-block 0, so every
// block-0 erase must re-write it or the next boot sees magic mismatch and
// wipes the whole grid (bench boot #22, 2026-11-01 — all data lost).
static bool recBBStampHeader()
{
    RecBBHeader stamp;
    memset(&stamp, 0, sizeof(stamp));
    memcpy(stamp.magic, kRecBBMagic, 4);
    stamp.version = kRecBBVersion;
    stamp.headerSize = (uint16_t)kRecBBHeaderSize;
    return esp_partition_write(recBBPart, 0, &stamp, sizeof(stamp)) == ESP_OK;
}

// --- Blackbox streaming writer (task 3, B2) ----------------------------------
// Producer (dashRecV2Submit, under recV2Mux) appends 18B frames into a
// 2×64KB PSRAM staging double-buffer; the writer task drains a full buffer
// with ONE flash write, erasing blocks lazily ahead of the write pointer
// (tracked by recBBErased bits). Protected blocks are skipped whole — the
// gap reads back as erased-zeros and the timestamp-filtered export ignores
// it; when every block is protected the writer pauses, staging keeps
// absorbing until full, then frames drop with the overflow counter so the
// loss stays visible in every session (badge semantics preserved).
struct RecBBBlockSum
{
    uint32_t firstTsUs;
    uint32_t lastTsUs;
    uint32_t cntA;
    uint32_t cntB;
    uint32_t cntC;
    uint32_t cntTx;
};
static_assert(sizeof(RecBBBlockSum) == 24, "block summary layout is frozen");
static RecBBBlockSum recBBSums[kRecBBBlockCount]; // RAM-only; zeroed at boot

// Freshness anchor for the close loop: the tsUs of the newest frame the
// submit path ever attributed, updated in the same critical section. Under
// load the attribution frontier runs 1-2 blocks ahead of the writer (both
// staging buffers undrained), so a block being entered already carries THIS
// pass's attribution; blind zeroing on entry destroyed it and the lock walk
// broke at the hole after one block (bench 2026-11-01: pre_ms_act 431ms /
// 1233ms of 40s / 20s requests) — and after traffic stopped, the final
// drain left the frontier slot empty so a later lock recorded NOTHING.
// Summary slots older than the grace window are previous-pass residue
// (the true pass seam) and still zero out.
static const uint32_t kRecBBAheadGraceUs = 60000000; // 60s >> any ahead gap
static volatile uint32_t recBBNewestTsUs = 0;
// Empty slots the walks may step back over AT THE FRONTIER only: the final
// writer geometry (fill-buffer partial that the tick never drains; a
// protection skip shifting landings past their attribution) can leave the
// frontier slot itself empty while the newest data sits one or two blocks
// behind. Deeper empties are a pass seam — the walks still stop there.
static const uint32_t kRecBBLeadSkip = 3;

static uint8_t *recBBStage[2] = {nullptr, nullptr}; // PSRAM, kRecBBBlockSize each
static volatile uint32_t recBBStageLen[2] = {0, 0};
static volatile uint8_t recBBStageFillIdx = 0;
static SemaphoreHandle_t recBBFlushSem = nullptr;
static TaskHandle_t recBBWriterHandle = nullptr;
static volatile bool recBBPaused = false;
static uint32_t recBBLiveBlk = 0;      // block the write pointer writes into
static uint32_t recBBClosedBlocks = 0; // blocks fully written through (NVS cadence)
static uint32_t recBBWptrBlocks = 0;   // closed-block count at the last wptr save

// Protection bitmap (144 bits). Task 4 rebuilds it from session metas at
// boot and sets bits at lock; the writer honors it via recBBProtGet.
static uint8_t recBBProt[kRecBBBlockCount / 8];
static bool recBBProtGet(uint32_t b)
{
    return (recBBProt[b >> 3] >> (b & 7)) & 1;
}
static void recBBProtSet(uint32_t b, bool on)
{
    // Byte RMW raced between loopTask (lock finish marks) and webTask
    // (delete clears) — final-review I-2: two interleaved RMWs on the
    // same byte lose one bit and silently unprotect a block. Readers
    // (writer task) do single-byte loads, which stay atomic.
    portENTER_CRITICAL(&recV2Mux);
    if (on)
        recBBProt[b >> 3] |= (uint8_t)(1u << (b & 7));
    else
        recBBProt[b >> 3] &= (uint8_t)~(1u << (b & 7));
    portEXIT_CRITICAL(&recV2Mux);
}

// Set/clear protection for every block covering the session's byte range.
// Block-rounded. WRAP CONVENTION (final-review C-1): endOff < startOff is
// a RING range — a lock window that crossed the rolling seam — and marks
// BOTH segments, [startOff, grid end) and [0, endOff). The old
// `endOff <= startOff` early return protected nothing for such sessions
// and the rolling window silently overwrote the locked evidence.
static void recBBRangeSet(uint32_t startOff, uint32_t endOff, bool on)
{
    if (endOff == startOff)
        return; // degenerate: no blocks
    bool wrapped = endOff < startOff;
    uint32_t b0 = startOff / kRecBBBlockSize;
    uint32_t b1 = wrapped ? kRecBBBlockCount - 1
                          : (endOff - 1) / kRecBBBlockSize;
    for (uint32_t b = b0; b <= b1 && b < kRecBBBlockCount; b++)
        recBBProtSet(b, on);
    if (wrapped)
    {
        // Segment B starts at block 0; bytes [0, 32) are the grid header,
        // protected with the block it lives in.
        uint32_t h1 = (endOff - 1) / kRecBBBlockSize;
        for (uint32_t b = 0; b <= h1; b++)
            recBBProtSet(b, on);
    }
}

// Protect a locked session's grid range (task 4).
static void recBBMarkRange(uint32_t startOff, uint32_t endOff)
{
    recBBRangeSet(startOff, endOff, true);
}

// Erased-ahead bitmap: each block is erased exactly once per pass over the
// grid; bits clear at boot (nothing is pre-erased after a resume).
static uint8_t recBBErased[kRecBBBlockCount / 8];
static bool recBBErasedGet(uint32_t b)
{
    return (recBBErased[b >> 3] >> (b & 7)) & 1;
}
static void recBBErasedSet(uint32_t b)
{
    recBBErased[b >> 3] |= (uint8_t)(1u << (b & 7));
}

// The bit means "safe to program without erasing"; a block that just
// received program data must re-erase before its next pass (flash only
// clears bits — programming dirty flash tears every frame in it).
static void recBBErasedClr(uint32_t b)
{
    recBBErased[b >> 3] &= (uint8_t)~(1u << (b & 7));
}

// NVS resume pointer: byte offset of the next grid write. Saved every 8
// closed blocks and at lock; read back at boot in recBBInitPartition.
static void recBBWptrSave()
{
    Preferences p;
    p.begin("rec_v2", false);
    p.putUInt("rv2_wptr", recBBWriteOff);
    p.end();
}

// Batch read primitive both download endpoints share: `bytes` from grid
// byte offset `off` (any grid offset — block edges are invisible to reads).
static bool recBBReadFrames(uint32_t off, uint8_t *dst, uint32_t bytes)
{
    return recBBPart != nullptr &&
           esp_partition_read(recBBPart, off, dst, bytes) == ESP_OK;
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
        if (!recBBEraseRangeSafe(0, kRecBBBlockSize) || !recBBStampHeader())
        {
            recBBEnabled = false;
            Serial.println("[REC2] blackbox header stamp failed - recorder disabled");
            return false;
        }
    }
    recBBWriteOff = kRecBBHeaderSize;
    if (!fresh)
    {
        // Resume: NVS pointer, frame-aligned down so a torn partial frame
        // from a mid-write power cut is dropped, not resurrected.
        Preferences p;
        p.begin("rec_v2", true);
        uint32_t saved = p.getUInt("rv2_wptr", 0);
        p.end();
        if (saved > kRecBBHeaderSize && saved < recBBPart->size)
        {
            uint32_t rel = (saved - kRecBBHeaderSize) / sizeof(RecRingFrame);
            recBBWriteOff = kRecBBHeaderSize + rel * sizeof(RecRingFrame);
        }
    }
    recBBLiveBlk = recBBWriteOff / kRecBBBlockSize;
    recBBEnabled = true;
    Serial.printf("[REC2] blackbox ready: %u bytes at 0x%08x (%s header)\n",
                  (unsigned)recBBPart->size, (unsigned)recBBPart->address,
                  fresh ? "fresh" : "existing");
    return true;
}

// First grid frame (header + k*18) at or inside block b. Block 0 snaps to
// the header itself — offset 0 is the 32-byte header, never a frame, and no
// block boundary is a grid point (65536 % 18 == 16), so skipping never
// re-anchors the grid: frames live at 32 + k*18 forever.
static uint32_t recBBGridSnap(uint32_t b)
{
    if (b == 0)
        return kRecBBHeaderSize;
    uint32_t rel = b * kRecBBBlockSize - kRecBBHeaderSize;
    return kRecBBHeaderSize +
           ((rel + sizeof(RecRingFrame) - 1) / sizeof(RecRingFrame)) *
               sizeof(RecRingFrame);
}

// Advance/skip the write pointer past protected blocks (wrapping at the
// partition end); pause when every block is protected. Also the recovery
// path: a flash-error pause re-runs this and retries on the next tick.
static void recBBSkipOrPause()
{
    uint32_t startBlk = recBBWriteOff / kRecBBBlockSize;
    uint32_t blk = startBlk;
    bool wrapped = false;
    for (;;)
    {
        if (blk >= kRecBBBlockCount)
        {
            if (wrapped)
                break; // full circle: every block protected → refuse-when-full
            wrapped = true;
            blk = 0; // rolling window wraps to the grid start
        }
        if (!recBBProtGet(blk))
        {
            recBBPaused = false;
            if (blk != startBlk)
            {
                // Leaving a protected block behind: retire its erase bit
                // with it (same I-1 reasoning as the flush early return).
                recBBErasedClr(startBlk);
                recBBWriteOff = recBBGridSnap(blk);
            }
            return;
        }
        blk++;
    }
    recBBPaused = true;
}

// Drain one full staging buffer into the grid: ONE erase pass over the
// entry block (lazy, tracked by recBBErased) and ONE esp_partition_write
// per flush — never per-frame flash calls. Every flush is clamped to the
// entry block's boundary and floored to WHOLE 18-byte frames, so a block
// is only ever programmed by sequential intra-block writes over one
// erase; a boundary park (never a grid point) re-anchors via
// recBBGridSnap, and when not even one frame fits the flush jumps the
// dead pad the same way. The erase bit retires only when the pointer
// LEAVES a block — the two round-7 bench destructions were (a) byte-
// exact boundary clamps whose erase-ahead re-erased the shared boundary
// block under the previous flush's tail, and (b) clearing the bit after
// every write, which made the same buffer's follow-up flush erase the
// entry block over the 65520 bytes it had just programmed. Both left
// ~16-400B per 64KB block while the summaries counted full buffers.
static void recBBFlushBuffer(uint8_t di)
{
    portENTER_CRITICAL(&recV2Mux);
    uint32_t len = recBBStageLen[di];
    uint32_t off = recBBWriteOff;
    portEXIT_CRITICAL(&recV2Mux);
    if (!len)
        return;
    uint32_t lenEff = len;
    if (off + lenEff > recBBPart->size)
        lenEff = recBBPart->size - off; // clamp at the partition end
    uint32_t b0 = off / kRecBBBlockSize;
    uint32_t b1 = lenEff ? (off + lenEff - 1) / kRecBBBlockSize : b0;
    for (uint32_t b = b0; b <= b1; ++b)
    {
        if (recBBProtGet(b))
        {
            if (b == b0)
            {
                // Evicted mid-fill (protection landed while this block was
                // being written): retire the erase bit now — after the
                // session is deleted, the next same-boot visit must
                // re-erase, not program dirty flash over these bytes
                // (final-review I-1).
                recBBErasedClr(b0);
                return; // pointer sits on a protected block: skip pass repositions
            }
            lenEff = b * kRecBBBlockSize - off; // clamp before the protected block
            break;
        }
    }
    if (lenEff == 0)
        return; // nothing left after the clamps
    // Geometry clamp: never cross the entry block's boundary. A protection
    // clamp (b > b0) is always ≤ this room, so this only bites when no
    // protected block limits the span — exactly the case that let each
    // flush erase its predecessor's tail out of the shared boundary block.
    uint32_t room = (b0 + 1) * kRecBBBlockSize - off;
    if (lenEff > room)
        lenEff = room;
    // Whole frames only: the clamps above are byte-exact, and a 1..17-byte
    // boundary clamp writes a TORN frame whose tail the boundary pad then
    // strands off-grid (bench round 7: 17-byte runs split by single FF
    // bytes). Floor the span to whole frames; when not even one fits
    // before the boundary, jump the dead pad and re-anchor — the staged
    // frames flush from the new anchor on the next pass (the writer loop
    // calls this twice per tick, so the retry is same-tick).
    lenEff -= lenEff % sizeof(RecRingFrame);
    if (lenEff == 0)
    {
        portENTER_CRITICAL(&recV2Mux);
        recBBWriteOff = recBBGridSnap((b0 + 1) % kRecBBBlockCount);
        portEXIT_CRITICAL(&recV2Mux);
        recBBErasedClr(b0); // the pointer left b0: retire its erase bit
        return;
    }
    for (uint32_t b = off / kRecBBBlockSize; b <= (off + lenEff - 1) / kRecBBBlockSize; ++b)
    {
        if (recBBErasedGet(b))
            continue;
        // Sub-chunked 4KB erases with yields (task-7 bench hygiene): each
        // erase runs inside the flash guard, where the OTHER core's ipc task
        // spins with scheduling frozen for the whole op — a whole-64KB erase
        // (typ 150ms, max 2000ms) would freeze CPU0 for that long. 4KB
        // sectors bound every guard window (boot #21/#23 forensics).
        if (!recBBEraseRangeSafe(b * kRecBBBlockSize, kRecBBBlockSize))
        {
            recBBPaused = true; // flash error: retry on the next tick
            return;
        }
        if (b == 0 && !recBBStampHeader())
        {
            recBBPaused = true;
            return; // header must never be missing past an erase of block 0
        }
        recBBErasedSet(b);
    }
    if (esp_partition_write(recBBPart, off, recBBStage[di], lenEff) != ESP_OK)
    {
        recBBPaused = true; // write error: retry on the next tick
        return;
    }
    // The erase bit does NOT retire here: a mid-block ending leaves the
    // unwritten tail of the erased block programmable, and clearing now
    // let the SAME buffer's next flush re-erase the entry block over the
    // bytes just programmed (bench round 7: ~16-400B survived per 64KB
    // block). Retirement happens below, only for blocks the pointer
    // actually left this flush.
    portENTER_CRITICAL(&recV2Mux);
    if (lenEff == len)
    {
        recBBStageLen[di] = 0;
    }
    else
    {
        memmove(recBBStage[di], recBBStage[di] + lenEff, len - lenEff);
        recBBStageLen[di] = len - lenEff;
    }
    recBBWriteOff = off + lenEff;
    // Wrap at the wall (task-7 crash #2): the end clamp can park the pointer
    // at recBBPart->size exactly, and /64K then names block 144 — a value
    // the mod-144 live pointer can never reach, so the close loop below
    // would spin inside this critical section until the interrupt watchdog
    // fires (INT_WDT on CPU1, CPU0 masked in submit's spinlock). Wrap to
    // the grid anchor; a staged remainder lands there on the next pass.
    if (recBBWriteOff >= recBBPart->size)
        recBBWriteOff = kRecBBHeaderSize;
    else if (recBBWriteOff % kRecBBBlockSize == 0)
    {
        // Parked exactly on a block boundary — never a grid point
        // (65536 % 18 == 16): writing from here de-anchors the frame grid
        // and every downstream consumer (decoder, session download, torn-
        // frame resume) loses the 32 + k*18 contract. Re-anchor on the
        // next grid point; the ≤17-byte dead pad at each block head is the
        // price of keeping the grid global.
        recBBWriteOff = recBBGridSnap(recBBWriteOff / kRecBBBlockSize);
    }
    // Close summaries for blocks the pointer has fully left; open the new
    // live block with a zeroed slot — but only if the slot's attribution is
    // older than the newest frame by the ahead grace. A slot holding THIS
    // pass's attribution (the frontier ran ahead of the writer; skipped
    // blocks never hold any) keeps its counts: zeroing it punched a hole
    // that collapsed mid-write lock windows to 1-2 blocks and left the
    // post-traffic frontier empty ("no frames recorded").
    uint32_t nb = recBBWriteOff / kRecBBBlockSize;
    while (recBBLiveBlk != nb)
    {
        recBBLiveBlk = (recBBLiveBlk + 1) % kRecBBBlockCount;
        recBBClosedBlocks = recBBClosedBlocks + 1;
        uint32_t ageUs = recBBNewestTsUs - recBBSums[recBBLiveBlk].lastTsUs;
        if (ageUs > kRecBBAheadGraceUs)
            memset(&recBBSums[recBBLiveBlk], 0, sizeof(recBBSums[recBBLiveBlk]));
    }
    // Retire erase bits for blocks the pointer LEFT this flush (boundary
    // park, pad jump, partition wrap): a stale set bit would let the next
    // pass program a dirty block without erasing. A mid-block ending (nb
    // == b0) retires nothing — the erased tail stays programmable and the
    // next flush continues in the same block without re-erasing it.
    while (b0 != nb)
    {
        recBBErasedClr(b0);
        b0 = (b0 + 1) % kRecBBBlockCount;
    }
    portEXIT_CRITICAL(&recV2Mux);
}

// Writer: pinned to core 1 at prio 1 — the CAN ingest on core 0 keeps its
// headroom; a stalled flash only stalls this task. Wakes on a full staging
// buffer (producer's semaphore) or a 250ms tick; the tick re-checks pause
// (protection may have cleared) and retries after flash errors.
static void dashRecV2WriterTask(void *)
{
    for (;;)
    {
        xSemaphoreTake(recBBFlushSem, pdMS_TO_TICKS(250));
        for (uint8_t pass = 0; pass < 2; ++pass)
        {
            uint8_t di = (uint8_t)(recBBStageFillIdx ^ 1);
            if (recBBStageLen[di] == 0 || recBBPaused)
                break;
            recBBFlushBuffer(di);
        }
        recBBSkipOrPause(); // skip protected blocks / wrap / unpause
        if (recBBClosedBlocks >= recBBWptrBlocks + 8)
        {
            recBBWptrSave(); // NVS cadence: every 8 closed blocks
            recBBWptrBlocks = recBBClosedBlocks;
        }
    }
}

// --- Lock state machine (task 2) -------------------------------------------
// idle → arming (post window counting down) → idle: the lock finishes
// inline in the dashboard loop; the flush-task state is gone with the
// flush task.
enum RecV2LockState : uint8_t
{
    kRecV2Idle = 0,
    kRecV2Arming = 1
};
static volatile uint8_t recV2State = kRecV2Idle;
static uint32_t recV2PreMsReq = 0;
static uint32_t recV2PreMsAct = 0;
static uint32_t recV2PostMs = 0;
static uint8_t recV2BusMask = 0;
static uint32_t recV2LockStartMs = 0;
static uint32_t recV2OverflowAtLock = 0;
static uint32_t recV2TwaiMissedSnapshot[3] = {0, 0, 0};
// Cross-task error text (final-review I-2): written by loopTask at lock
// finish, read by webTask in the status JSON. A String would be a torn
// heap object between tasks; a fixed buffer with a guarded bounded copy
// is race-free.
static char recV2LastErr[48] = {0};
static void recV2SetLastError(const char *msg)
{
    portENTER_CRITICAL(&recV2Mux);
    if (msg && msg[0])
        strlcpy(recV2LastErr, msg, sizeof(recV2LastErr));
    else
        recV2LastErr[0] = '\0';
    portEXIT_CRITICAL(&recV2Mux);
}
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
static void recBBRebuildProtection(); // defined after the session scanner
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
    ringOverflow = 0;
    recBBInitPartition(); // raw-partition blackbox skeleton (task 2)
    if (recBBEnabled)
        recBBRebuildProtection(); // locked sessions survive reboots (task 4)
    if (recBBEnabled)
    {
        recBBStage[0] = (uint8_t *)heap_caps_malloc(kRecBBBlockSize, MALLOC_CAP_SPIRAM);
        recBBStage[1] = (uint8_t *)heap_caps_malloc(kRecBBBlockSize, MALLOC_CAP_SPIRAM);
        if (!recBBStage[0] || !recBBStage[1])
        {
            recBBEnabled = false; // staging fallback is disabled by design
            Serial.println("[REC2] no PSRAM for blackbox staging - recorder disabled");
        }
    }
    if (recBBEnabled)
    {
        recBBFlushSem = xSemaphoreCreateBinary();
        xTaskCreatePinnedToCore(dashRecV2WriterTask, "recbb_write", 6144,
                                nullptr, 1, &recBBWriterHandle, 1);
        Serial.println("[REC2] blackbox writer task started");
    }
    // Boot-time only, before any session can exist: record why we last died.
    recV2LedgerBoot();
}

// Hot path: one frame into the blackbox staging grid. Never blocks; when
// both staging buffers are full (writer behind, or paused on a full grid)
// the frame drops and the overflow counter keeps the loss visible in every
// session meta (badge semantics preserved from the ring era).
static void dashRecV2Submit(const CanFrame &f, uint8_t busTag, char dir)
{
    if (!recBBEnabled || !recBBStage[0])
        return;
    RecRingFrame r;
    r.tsUs = static_cast<uint32_t>(esp_timer_get_time());
    r.id = f.id;
    r.dlc = (f.dlc <= 8) ? f.dlc : 8;
    r.flags = static_cast<uint8_t>((busTag & 0x03) | (recV2DirBits(dir) << 2));
    memcpy(r.data, f.data, r.dlc);
    if (r.dlc < 8)
        memset(r.data + r.dlc, 0, 8 - r.dlc);
    bool wake = false;
    portENTER_CRITICAL(&recV2Mux);
    uint8_t fi = recBBStageFillIdx;
    if (recBBStageLen[fi] + sizeof(r) > kRecBBBlockSize)
    {
        uint8_t other = fi ^ 1;
        if (recBBStageLen[other] != 0)
        {
            // Writer still holds the other buffer (behind or paused): drop
            // the frame, count it — never block the CAN ingest path.
            ringOverflow = ringOverflow + 1;
            portEXIT_CRITICAL(&recV2Mux);
            return;
        }
        recBBStageFillIdx = other; // hand the full buffer to the writer
        fi = other;
        wake = true;
    }
    memcpy(recBBStage[fi] + recBBStageLen[fi], &r, sizeof(r));
    // Attribute to the grid block this frame will land in: the write
    // pointer plus the undrained buffers plus the position in this buffer.
    uint32_t absOff = recBBWriteOff + recBBStageLen[fi ^ 1] + recBBStageLen[fi];
    // Seam-park fold (bench 2026-11-02): while the pointer parks at the
    // grid end waiting for the pad jump, raw absOff runs past the partition
    // and /64K names block 144+ — the count guard below silently dropped
    // exactly the frames that land at the anchor after the jump (block 0's
    // entire 64KB comes from that handed-over buffer), so slot 0 could
    // never fill and no lock window could ever wrap. Past the last grid
    // point the landing offset is anchor + (absOff - gridEnd).
    const uint32_t gridEnd =
        kRecBBHeaderSize +
        ((recBBPart->size - kRecBBHeaderSize) / sizeof(RecRingFrame)) *
            sizeof(RecRingFrame);
    if (absOff > gridEnd)
        absOff = kRecBBHeaderSize + (absOff - gridEnd);
    uint32_t blk = absOff / kRecBBBlockSize;
    if (blk < kRecBBBlockCount)
    {
        RecBBBlockSum &s = recBBSums[blk];
        if (s.firstTsUs == 0)
            s.firstTsUs = r.tsUs; // first frame of this block's pass
        s.lastTsUs = r.tsUs;
        if ((r.flags >> 2) == 0)
        {
            if (busTag == kRecV2BusA)
                s.cntA = s.cntA + 1;
            else if (busTag == kRecV2BusB)
                s.cntB = s.cntB + 1;
            else if (busTag == kRecV2BusC)
                s.cntC = s.cntC + 1;
        }
        else
        {
            s.cntTx = s.cntTx + 1; // T and E both count as our own traffic
        }
    }
    recBBNewestTsUs = r.tsUs; // freshness anchor for the close loop's zeroing
    recBBStageLen[fi] = recBBStageLen[fi] + sizeof(r);
    if (busTag >= kRecV2BusA && busTag <= kRecV2BusC)
        recV2FpsAccum[busTag - 1] = recV2FpsAccum[busTag - 1] + 1;
    portEXIT_CRITICAL(&recV2Mux);
    if (wake)
        xSemaphoreGive(recBBFlushSem); // outside the critical section
}

// Retention in µs: oldest live block summary firstTs → now. Unsigned
// subtraction is wrap-safe across the 71.6-min tsUs rollover.
static uint32_t dashRecV2SpanUs()
{
    if (!recBBEnabled)
        return 0;
    portENTER_CRITICAL(&recV2Mux);
    uint32_t frontierBlk =
        (recBBWriteOff + recBBStageLen[0] + recBBStageLen[1]) / kRecBBBlockSize;
    if (frontierBlk >= kRecBBBlockCount)
        frontierBlk = kRecBBBlockCount - 1;
    uint32_t oldestTs = 0;
    // Same frontier-lead rule as the finisher: an empty lead slot is where
    // the writer's endgame parked, not proof of empty retention.
    uint32_t lead = 0;
    while (lead < kRecBBLeadSkip &&
           recBBSums[frontierBlk].lastTsUs == 0)
    {
        frontierBlk = (frontierBlk + kRecBBBlockCount - 1) % kRecBBBlockCount;
        lead++;
    }
    uint32_t b = frontierBlk;
    for (uint32_t guard = 0; guard < kRecBBBlockCount; guard++)
    {
        const RecBBBlockSum &s = recBBSums[b];
        if (s.firstTsUs == 0 && s.lastTsUs == 0)
            break; // pass boundary: nothing older belongs to this pass
        oldestTs = s.firstTsUs;
        if (b == 0)
            b = kRecBBBlockCount - 1;
        else
            b--;
    }
    portEXIT_CRITICAL(&recV2Mux);
    if (!oldestTs)
        return 0;
    return (uint32_t)((uint32_t)esp_timer_get_time() - oldestTs);
}

// --- Session files ----------------------------------------------------------
// A session is /sess_<seq>.meta.json naming a protected grid range; seq is
// NVS-backed and monotonic: rv2_seq holds the last allocated value.
static uint32_t recV2AllocateSeq()
{
    Preferences p;
    p.begin("rec_v2", false);
    uint32_t seq = p.getUInt("rv2_seq", 0) + 1;
    p.putUInt("rv2_seq", seq);
    p.end();
    return seq;
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

// Rebuild the protection bitmap at boot: every session meta names the grid
// byte range it locked; blocks outside those ranges stay clear (fair game
// for the rolling writer).
static void recBBRebuildProtection()
{
    memset(recBBProt, 0, sizeof(recBBProt));
    uint32_t seqs[32];
    size_t n = recV2ScanSessions(seqs, 32);
    for (size_t i = 0; i < n; i++)
    {
        File mf = SPIFFS.open(recV2MetaPath(seqs[i]), "r");
        if (!mf)
            continue;
        String body = mf.readString();
        mf.close();
        JsonDocument doc;
        if (deserializeJson(doc, body))
            continue; // torn meta: its range stays unprotected (swept later)
        uint32_t s = doc["bb_start_off"] | 0u;
        uint32_t e = doc["bb_end_off"] | 0u;
        if (e != s) // e < s = wrapped seam range (C-1): mark both segments
            recBBMarkRange(s, e);
    }
    Serial.printf("[REC2] protection rebuilt from %u session meta(s)\n",
                  (unsigned)n);
}

// Delete: clear this session's block protection (the meta is the only
// record of the range), then remove the meta. The blackbox era has no bin
// files; the legacy bin remove stays as a harmless no-op for old sessions.
static bool recV2RemoveSession(uint32_t seq)
{
    File mf = SPIFFS.open(recV2MetaPath(seq), "r");
    if (mf)
    {
        String body = mf.readString();
        mf.close();
        JsonDocument doc;
        if (!deserializeJson(doc, body))
        {
            uint32_t s = doc["bb_start_off"] | 0u;
            uint32_t e = doc["bb_end_off"] | 0u;
            if (e != s) // e < s = wrapped seam range (C-1): free both
                recBBRangeSet(s, e, false);
        }
    }
    return SPIFFS.remove(recV2MetaPath(seq));
}

// --- Zero-copy lock finish (task 4, B2) --------------------------------------
// Arming expired: the window's frames are ALREADY in the blackbox grid (or
// in staging behind it) — no frame is ever copied. Walk the touched block
// summaries for the window counts, compute the byte range, protect it, and
// write the meta. Runs inline in the dashboard loop: µs of RAM walking plus
// one small meta write; the flush-task era is over.
static void recBBFinishLock()
{
    const uint32_t preMs = recV2PreMsReq;
    const uint32_t windowUs = (preMs + recV2PostMs) * 1000UL;
    uint32_t cntA = 0, cntB = 0, cntC = 0, cntTx = 0;
    uint32_t lastTs = 0, oldestTs = 0;
    uint32_t frontierBlk = 0, startBlk = 0;
    uint32_t overflowNow = 0;
    bool haveFrames = false;
    // 2026-11-06 field incident (ka750, fw 0b87bfd): pre_s 300/600 with FSD
    // traffic (retention ~55-60s) covers EVERY block — the backward walk
    // exhausts its guard, startBlk lands on frontier+1 and the seam
    // collapses to bbStart == bbEnd: size=0 sessions, loss_badge=false,
    // three silent total losses (300/5, 300/5, 600/10). The flag corrects
    // the range and the badge after the walk.
    bool windowCoversGrid = false;
    portENTER_CRITICAL(&recV2Mux);
    // Frontier = the block the NEXT frame lands in minus one correction: the
    // absolute offset of the newest staged frame (writeOff + undrained
    // buffers), same attribution formula the submit path uses.
    frontierBlk = (recBBWriteOff + recBBStageLen[0] + recBBStageLen[1]) /
                  kRecBBBlockSize;
    if (frontierBlk >= kRecBBBlockCount)
        frontierBlk = kRecBBBlockCount - 1; // full-grid edge
    // The frontier slot itself can sit empty after traffic stops (fill-buffer
    // partial never drains; a protection skip shifts landings) — step back
    // over the lead before deciding the ring holds nothing (bench round 5:
    // "no frames recorded" with 68K retained frames behind the hole).
    uint32_t lead = 0;
    while (lead < kRecBBLeadSkip &&
           recBBSums[frontierBlk].lastTsUs == 0)
    {
        frontierBlk = (frontierBlk + kRecBBBlockCount - 1) % kRecBBBlockCount;
        lead++;
    }
    lastTs = recBBSums[frontierBlk].lastTsUs;
    haveFrames = lastTs != 0;
    if (haveFrames)
    {
        uint32_t b = frontierBlk;
        startBlk = frontierBlk;
        oldestTs = lastTs;
        uint32_t guard = 0;
        for (; guard < kRecBBBlockCount; guard++)
        {
            const RecBBBlockSum &s = recBBSums[b];
            // Unsigned age compare is wrap-safe across the 71.6-min tsUs
            // rollover; a zeroed slot is a pass boundary (skip gap) — stop.
            if (b != frontierBlk &&
                (s.lastTsUs == 0 || (uint32_t)(lastTs - s.lastTsUs) > windowUs))
                break; // block ends before the window's cut
            cntA += s.cntA;
            cntB += s.cntB;
            cntC += s.cntC;
            cntTx += s.cntTx;
            startBlk = b;
            oldestTs = s.firstTsUs;
            if (b == 0)
                b = kRecBBBlockCount - 1;
            else
                b--;
        }
        // Guard exhausted = every block in-window (no hole, no stale age):
        // the requested window reaches past retention itself.
        windowCoversGrid = guard == kRecBBBlockCount;
    }
    overflowNow = ringOverflow;
    portEXIT_CRITICAL(&recV2Mux);

    // Common tail for every exit: arming is over, the ledger flag must not
    // outlive it (a boot reading rv2_flushing=1 died INSIDE an arming).
    Preferences p;
    p.begin("rec_v2", false);
    p.putUChar("rv2_flushing", 0);
    p.end();
    recV2State = kRecV2Idle;

    if (!haveFrames)
    {
        recV2SetLastError("no frames recorded");
        Serial.println("[REC2] lock finished with no frames - meta skipped");
        return;
    }

    // Window reaching past retention: honor what the grid actually holds
    // (same clamp semantics as the ring era's pre_ms_act).
    uint32_t spanMs = (uint32_t)(lastTs - oldestTs) / 1000UL;
    uint32_t preEff = preMs;
    if (preEff > spanMs)
        preEff = spanMs;
    recV2PreMsAct = preEff;
    const uint32_t tCutUs = (uint32_t)(lastTs - preEff * 1000UL);

    // Block-rounded byte range of the window; protect it before the writer
    // can ever wrap around onto it.
    uint32_t bbStart = startBlk * kRecBBBlockSize;
    uint32_t bbEnd = (frontierBlk + 1) * kRecBBBlockSize;
    if (bbEnd > kRecBBBlockCount * kRecBBBlockSize)
        bbEnd = kRecBBBlockCount * kRecBBBlockSize;
    if (windowCoversGrid)
    {
        // Full-grid window: honor everything the grid holds instead of the
        // empty seam (startBlk wrapped onto frontier+1). The plain 0..grid
        // span keeps size, protection rebuild, delete and the CSV walker
        // in their s < e paths.
        bbStart = 0;
        bbEnd = kRecBBBlockCount * kRecBBBlockSize;
    }
    recBBMarkRange(bbStart, bbEnd);
    // A protected write pointer right here (partition filled during arming)
    // is the writer's problem to skip, not the lock's.

    const uint32_t seq = recV2AllocateSeq();
    String meta = "{";
    meta += R"JSON("seq":)JSON";
    meta += String(seq);
    meta += R"JSON(,"boot_ms":)JSON";
    meta += String(millis());
    meta += R"JSON(,"trigger":"manual")JSON";
    if (dashRecV2HasWall())
    {
        meta += R"JSON(,"wall":)JSON";
        meta += String((uint32_t)time(nullptr));
    }
    meta += R"JSON(,"pre_ms_req":)JSON";
    meta += String(recV2PreMsReq);
    meta += R"JSON(,"pre_ms_act":)JSON";
    meta += String(preEff);
    meta += R"JSON(,"post_ms":)JSON";
    meta += String(recV2PostMs);
    meta += R"JSON(,"bus_mask":)JSON";
    meta += String(recV2BusMask);
    meta += R"JSON(,"frames":{"a":)JSON";
    meta += String(cntA);
    meta += R"JSON(,"b":)JSON";
    meta += String(cntB);
    meta += R"JSON(,"c":)JSON";
    meta += String(cntC);
    meta += R"JSON(,"tx":)JSON";
    meta += String(cntTx);
    meta += R"JSON(},"ring_overflow":)JSON";
    meta += String(overflowNow);
    meta += R"JSON(,"ring_overflow_in_window":)JSON";
    // AND semantics preserved from the ring era's final review: drops since
    // lock AND the window reaching past what this pass retains. A full-grid
    // walk is a certain loss of its own — raise the badge even with no
    // eviction delta (2026-11-06: all three size=0 sessions badged
    // lossless under the AND-only form).
    bool overflowInWindow =
        ((overflowNow > recV2OverflowAtLock) && (windowUs > (uint32_t)(lastTs - oldestTs))) ||
        windowCoversGrid;
    meta += String(overflowInWindow ? "true" : "false");
    meta += R"JSON(,"twai_missed_a":)JSON";
    meta += String(recV2TwaiMissedSnapshot[0]);
    meta += R"JSON(,"twai_missed_b":)JSON";
    meta += String(recV2TwaiMissedSnapshot[1]);
    meta += R"JSON(,"twai_missed_c":)JSON";
    meta += String(recV2TwaiMissedSnapshot[2]);
    meta += R"JSON(,"start_boot_us":)JSON";
    meta += String(tCutUs);
    meta += R"JSON(,"end_boot_us":)JSON";
    meta += String(lastTs);
    meta += R"JSON(,"bb_start_off":)JSON";
    meta += String(bbStart);
    meta += R"JSON(,"bb_end_off":)JSON";
    meta += String(bbEnd);
    meta += "}";
    File metaFile = SPIFFS.open(recV2MetaPath(seq), "w");
    if (metaFile)
    {
        metaFile.print(meta);
        metaFile.close();
        recV2Seq = seq;
        recV2SetLastError("");
        Serial.printf("[REC2] session %u locked zero-copy: a=%u b=%u c=%u "
                      "tx=%u pre=%lums blocks %u..%u\n",
                      (unsigned)seq, (unsigned)cntA, (unsigned)cntB,
                      (unsigned)cntC, (unsigned)cntTx,
                      (unsigned long)preEff, (unsigned)startBlk,
                      (unsigned)frontierBlk);
    }
    else
    {
        recV2SetLastError("meta write failed");
        Serial.printf("[REC2] session %u meta write FAILED\n", (unsigned)seq);
    }
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
        // Blackbox era: the session's grid byte range from its own meta
        // (ring-era sessions without bb keys report 0).
        {
            uint32_t s0 = doc["bb_start_off"] | 0u;
            uint32_t e0 = doc["bb_end_off"] | 0u;
            // e0 < s0 = wrapped seam range (C-1): report both segments
            j += String(e0 > s0 ? e0 - s0
                                : (e0 < s0)
                                      ? (kRecBBBlockCount *
                                             kRecBBBlockSize -
                                         s0) +
                                            e0
                                      : 0);
        }
        j += R"JSON(,"loss_badge":)JSON";
        j += String(doc["ring_overflow_in_window"] | false ? "true" : "false");
        j += "}";
    }
    j += "]";
    return j;
}

static void dashRecV2SetTwaiMissed(uint32_t a, uint32_t b, uint32_t c)
{
    recV2TwaiMissedSnapshot[0] = a;
    recV2TwaiMissedSnapshot[1] = b;
    recV2TwaiMissedSnapshot[2] = c;
}

// Lock: mark the last `preMs` (plus `postMs` of continued recording) for
// the buses in `busMask` (bit0=A, bit1=B, bit2=C) — the grid keeps
// recording; the finisher walks the summaries when arming expires.
static bool dashRecV2LockRequest(uint32_t preMs, uint32_t postMs,
                                 uint8_t busMask, String &err)
{
    // 2026-11-05 晚场事故：串口层裸调本函数，0/0 与 99999/99999 巨窗
    // 双双被放行（HTTP 层有范围检查而这里没有）→ 27.7h 巨窗只能整机重启
    // 解除。范围检查下沉到本函数 = 单一真源，串口/HTTP 任何入口同权。
    // 界限与 handleRingLock 完全一致：pre 10-600s，post 5-300s。
    if (preMs < 10000UL || preMs > 600000UL || postMs < 5000UL || postMs > 300000UL)
    {
        err = "pre_s/post_s out of range (pre 10-600s, post 5-300s)";
        return false;
    }
    if (!recBBEnabled)
    {
        err = "recorder v2 disabled";
        return false;
    }
    if (!busMask)
    {
        err = "bus_mask must select at least one bus";
        return false;
    }
    if (recV2State != kRecV2Idle)
    {
        err = "recorder busy"; // an arming window is still counting down
        return false;
    }
    if (recBBPaused)
    {
        err = "no space"; // every block protected or unwritable: grid full
        return false;
    }
    recV2PreMsReq = preMs;
    recV2PreMsAct = 0; // decided at finish (may shrink to retention)
    recV2PostMs = postMs;
    recV2BusMask = busMask;
    recV2LockStartMs = millis();
    recV2OverflowAtLock = ringOverflow;
    recBBWptrSave(); // persist the grid pointer the moment a session locks
    // Forensics flag: raised for the whole arming so a boot that finds it
    // set knows the previous session died INSIDE that arming window.
    {
        Preferences p;
        p.begin("rec_v2", false);
        p.putUChar("rv2_flushing", 1);
        p.end();
    }
    recV2State = kRecV2Arming;
    return true;
}

// 取消一个 arming 中的锁窗（2026-11-05 晚场事故后补）：不发 session、
// 不保护任何块区间，环继续滚动记录。只回收锁窗副作用——rv2_flushing
// 台账位（boot 读到 1 会误判"死在锁窗内"）。锁入时的 recBBWptrSave
// 只是环写指针的 NVS 检查点，取消后依然成立，无需回滚。
static bool dashRecV2UnlockRequest(String &err)
{
    if (!recBBEnabled)
    {
        err = "recorder v2 disabled";
        return false;
    }
    if (recV2State != kRecV2Arming)
    {
        err = "not arming";
        return false;
    }
    Preferences p;
    p.begin("rec_v2", false);
    p.putUChar("rv2_flushing", 0);
    p.end();
    recV2State = kRecV2Idle;
    recV2SetLastError("lock cancelled");
    Serial.println("[REC2] lock cancelled - ring keeps rolling, no session");
    return true;
}

static String dashRecV2StatusJson()
{
    // Zero-copy era: idle/arming only — the finisher runs inline, no flush
    // state exists anymore.
    const char *stateName = recV2State == kRecV2Arming ? "arming" : "idle";
    uint32_t remain = 0;
    if (recV2State == kRecV2Arming)
    {
        uint32_t elapsed = (uint32_t)(millis() - recV2LockStartMs);
        remain = elapsed < recV2PostMs ? recV2PostMs - elapsed : 0;
    }
    String j = "{";
    j += R"JSON("enabled":)JSON";
    j += String(recBBEnabled ? "true" : "false");
    j += R"JSON(,"paused":)JSON";
    j += String(recBBPaused ? "true" : "false");
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
    // Free grid space = unprotected blocks × block size (the SPIFFS free
    // byte count belongs to the bin era).
    uint32_t freeBlks = 0;
    for (uint32_t b = 0; b < kRecBBBlockCount; b++)
        if (!recBBProtGet(b))
            freeBlks++;
    j += R"JSON(},"sessions_space_free":)JSON";
    j += String(freeBlks * kRecBBBlockSize);
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
    char errCopy[sizeof(recV2LastErr)];
    portENTER_CRITICAL(&recV2Mux);
    memcpy(errCopy, recV2LastErr, sizeof(errCopy));
    portEXIT_CRITICAL(&recV2Mux);
    if (errCopy[0])
    {
        j += R"JSON(,"last_error":")JSON";
        j += errCopy;
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
// state machine (arming expiry → zero-copy finish, inline — no task spawn,
// the flush-task era ended with the blackbox writer).
static void dashRecV2Loop()
{
    if (!recBBEnabled)
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
        recBBFinishLock(); // → idle (or idle + last_error)
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
    if (!recBBEnabled || fps == 0 || seconds == 0)
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
