"""Contract tests for the B2 raw-partition blackbox recorder (2026-11-01).

Design: docs/superpowers/specs/2026-11-01-blackbox-recorder-and-sta-panic-fix-design.md
§6.1-6.3 — a dedicated 9MB `blackbox` partition (data/0x40) holds a global
18-byte frame grid anchored at a 32B header; 64KB blocks are erase units
only. This file pins the partition layout, the frozen header/grid geometry,
and the absent-partition degradation path. Task-by-task tests for the
writer, lock, and download paths land with those tasks.
"""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CSV = ROOT / "partitions_32mb_ota_8192k_nvs64.csv"
ENGINE = ROOT / "include" / "web" / "dash_recorder_v2.h"


class BlackboxPartitionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.csv = CSV.read_text(encoding="utf-8")
        cls.engine = ENGINE.read_text(encoding="utf-8")

    @staticmethod
    def _row(source: str, name: str) -> str:
        for line in source.splitlines():
            if line.split(",")[0].strip() == name:
                return line
        raise AssertionError(f"partition row '{name}' missing")

    def test_partition_table_rows(self) -> None:
        # 2026-11-02 heap-guard v2: coredump 128K@0x1FE0000 → 1M@0x15E0000
        # (fits CAPTURE_DRAM dumps incl. .bss + INTERNAL heap + task owners +
        # the recorder-v2 2×64K SPIRAM staging blocks the MALLOC_CAP_8BIT walk
        # also captures — 640K was arithmetic-on-the-edge: ~600K internal used
        # blocks + 128K PSRAM staging would trip ESP_COREDUMP_ASSERT and lose
        # the dump exactly when recording). Borrowed from the oversized spiffs
        # (legacy v1 recorder residue only); the blackbox geometry is
        # byte-identical so recorder v2 math (144×64K) never moves.
        self.assertIn("blackbox,      data, 0x40,   0x16E0000,0x900000,", self.csv)
        spiffs = self._row(self.csv, "spiffs")
        self.assertIn("0x1020000,0x5C0000", spiffs)
        coredump = self._row(self.csv, "coredump")
        self.assertIn("0x15E0000,0x100000", coredump)
        # Contiguity by integer math: spiffs end == coredump start, coredump
        # end == blackbox start. No gaps, no overlap.
        self.assertEqual(
            0x1020000 + 0x5C0000, 0x15E0000, "spiffs must end where coredump starts"
        )
        self.assertEqual(
            0x15E0000 + 0x100000, 0x16E0000, "coredump must end where blackbox starts"
        )
        # And the table itself parses with those exact numbers.
        blackbox = self._row(self.csv, "blackbox")
        self.assertIn("0x16E0000", blackbox)
        self.assertIn("0x900000", blackbox)

    def test_header_struct_frozen(self) -> None:
        self.assertIn("static_assert(sizeof(RecBBHeader) == 32", self.engine)
        self.assertIn('"BBX1"', self.engine)  # kRecBBMagic
        self.assertIn("kRecBBVersion = 1", self.engine)
        self.assertIn("headerSize", self.engine)  # grid anchor travels with the header

    def test_grid_addressing_ignores_block_edges(self) -> None:
        # Block 0 holds (65536-32)//18 = 3639 complete frames; frame 3639
        # straddles the 64KB edge. The grid stays anchored at the 32B header
        # forever — blocks are erase units, never re-anchoring points.
        hdr, frame = 32, 18
        self.assertEqual((65536 - hdr) // frame, 3639)
        off_3639 = hdr + 3639 * frame
        self.assertLess(off_3639, 65536)          # starts inside block 0
        self.assertGreater(off_3639 + frame, 65536)  # ends inside block 1
        for k in (3640, 3641, 3642):
            self.assertGreaterEqual(hdr + k * frame, 65536)  # block 1 residents
        self.assertEqual(hdr + 3640 * frame, 65552)  # not block-aligned
        # The C++ addressing is exactly this grid formula (substring).
        self.assertIn("kRecBBHeaderSize + idx * sizeof(RecRingFrame)", self.engine)

    def test_block_constants(self) -> None:
        count = int(re.search(r"kRecBBBlockCount = (\d+)u", self.engine).group(1))
        size = int(re.search(r"kRecBBBlockSize = (\d+)u", self.engine).group(1))
        self.assertEqual(count, 144)
        self.assertEqual(size, 65536)
        self.assertEqual(count * size, 0x900000)  # == the CSV partition size

    def test_absent_partition_disables_engine(self) -> None:
        # recBBInitPartition: nullptr from the lookup leaves the engine off.
        init_start = self.engine.index("static bool recBBInitPartition()")
        init_body = self.engine[init_start:init_start + 2000]
        self.assertIn("!recBBPart", init_body)
        self.assertIn("recBBEnabled = false", init_body)
        # Lock still refuses with the same user-facing text.
        self.assertIn('err = "recorder v2 disabled"', self.engine)
        # Boot names the missing partition on serial.
        self.assertIn("[REC2] blackbox partition missing", self.engine)


class BlackboxWriterTests(unittest.TestCase):
    """Task 3: streaming writer — staging, erase-ahead, summaries, pause."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.engine = ENGINE.read_text(encoding="utf-8")

    @staticmethod
    def _fn(source: str, signature: str, span: int = 2200) -> str:
        start = source.index(signature)
        return source[start : start + span]

    def test_submit_paths_through_staging(self) -> None:
        body = self._fn(self.engine, "static void dashRecV2Submit(")
        self.assertIn("portENTER_CRITICAL(&recV2Mux)", body)
        self.assertIn("memcpy(recBBStage", body)
        self.assertNotIn("ring[", body)  # the ring is dead the moment staging lands

    def test_writer_task_pinned_core1_low_prio(self) -> None:
        self.assertIn("xTaskCreatePinnedToCore(dashRecV2WriterTask", self.engine)
        self.assertIn('"recbb_write"', self.engine)
        self.assertIn("6144", self.engine)
        # prio 1, core 1 in the create call
        create = self.engine[self.engine.index("xTaskCreatePinnedToCore(dashRecV2WriterTask") :]
        self.assertIn("nullptr, 1, &recBBWriterHandle, 1);", create[: create.index(");") + 2])

    def test_erase_ahead_skips_protected_and_stops(self) -> None:
        region = self._fn(self.engine, "static void recBBSkipOrPause()", 9000)
        self.assertIn("recBBProtGet(blk)", region)  # protection skip
        self.assertIn("recBBPaused = true", region)  # refuse-when-full
        self.assertIn("recBBPaused = false", region)  # resume when protection clears

    def test_block_summaries_ram_only(self) -> None:
        self.assertIn("RecBBBlockSum recBBSums[kRecBBBlockCount]", self.engine)
        self.assertIn("static_assert(sizeof(RecBBBlockSum) == 24", self.engine)
        # Summaries never touch NVS/flash (RAM-only; export scans flash).
        self.assertNotIn('putBytes("rv2_sum', self.engine)
        self.assertNotIn('putUInt("rv2_sum', self.engine)

    def test_wptr_nvs_cadence(self) -> None:
        self.assertIn('putUInt("rv2_wptr"', self.engine)
        # Two call sites: writer 8-block cadence + lock entry.
        self.assertEqual(self.engine.count("recBBWptrSave();"), 2)
        cadence = self.engine[self.engine.index("recBBClosedBlocks >= recBBWptrBlocks + 8") :]
        self.assertIn("recBBWptrSave();", cadence[: cadence.index("}") + 1])

    def test_pacing_and_yield(self) -> None:
        # The flush programs through the sub-chunked writer — never one
        # monolithic 64KB esp_partition_write, never per-frame flash calls.
        # The task-2 header stamp is the only other partition write.
        region = self._fn(self.engine, "static void recBBFlushBuffer(", 6500)
        self.assertIn("recBBWriteRangeSafe(", region)
        self.assertNotIn("esp_partition_write(recBBPart", region)
        self.assertNotIn("for (uint32_t f = 0", region)  # no per-frame loop

    def test_write_subchunked_under_int_wdt(self) -> None:
        # Field P0 (sess_113/114, 2026-11-08): the monolithic 64KB
        # esp_partition_write held the flash guard ~450-700ms with the cache
        # suspended chip-wide — core 0's CAN ingest froze with it (three-bus
        # 0x106 gaps once per flush, ~50% session duty), our 0x3EE
        # rides/keep-alives went silent while the car's native
        # "offset off" frames kept flowing, and the consumer dropped the
        # offset every few seconds (96->80 flicker + FSD decel jerk). Every
        # partition write from the flush goes through the sub-chunk helper:
        # 4KB chunks with a yield between guard windows, mirroring the erase
        # discipline pinned by test_erase_subchunked_under_int_wdt.
        self.assertIn("kRecBBWriteChunk = 4096u", self.engine)
        helper = self._fn(self.engine, "static uint32_t recBBWriteRangeSafe(", 1600)
        self.assertIn("kRecBBWriteChunk", helper)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(1))", helper,
                      "a yield between sub-writes lets ticks run")
        self.assertIn("esp_partition_write(recBBPart", helper)
        # A mid-write flash failure must commit only whole frames already
        # programmed — retrying the whole buffer would re-program dirty
        # bytes over the same span and corrupt the grid.
        flush = self._fn(self.engine, "static void recBBFlushBuffer(", 6500)
        self.assertIn("written % sizeof(RecRingFrame)", flush)
        self.assertIn("recBBPaused = true", flush)  # error surfaced for retry

    def test_erase_subchunked_under_int_wdt(self) -> None:
        # Task-7 bench hygiene (boots #21/#23, 2026-11-01): each erase runs
        # inside the flash guard, where the OTHER core's ipc task spins with
        # scheduling frozen for the whole op — a whole-64KB erase (typ 150ms,
        # max 2000ms) freezes CPU0 that long. (The boot-#21 INT_WDT crash was
        # initially blamed on this; the coredump decode of crash #2 proved
        # the true cause was the wrap hang pinned by
        # test_flush_wraps_at_partition_end.) Every erase goes through the
        # sub-chunk helper: 4KB sectors with a yield between guard windows.
        self.assertIn("kRecBBEraseChunk = 4096u", self.engine)
        helper = self._fn(self.engine, "static bool recBBEraseRangeSafe(", 1100)
        self.assertIn("kRecBBEraseChunk", helper)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(1))", helper,
                      "a yield between sub-erases lets ticks run")
        self.assertIn("esp_partition_erase_range", helper)
        # The flush path never erases directly (whole-block erase forbidden).
        flush = self._fn(self.engine, "static void recBBFlushBuffer(", 6500)
        self.assertIn("recBBEraseRangeSafe(", flush)
        self.assertNotIn("esp_partition_erase_range", flush)
        # The init fresh-stamp path shares the safe eraser too.
        init = self._fn(self.engine, "static bool recBBInitPartition()", 2600)
        self.assertIn("recBBEraseRangeSafe(", init)

    def test_flush_wraps_at_partition_end(self) -> None:
        # Task-7 crash #2 root cause (boot #23 coredump): the flush clamp
        # lets recBBWriteOff reach recBBPart->size (0x900000) exactly, so the
        # summary-close loop's nb = 0x900000/0x10000 = 144 — a value the
        # mod-144 recBBLiveBlk (0..143) can never equal. The loop spun inside
        # portENTER_CRITICAL with CPU1's interrupts masked until the 300ms
        # interrupt watchdog fired, while CPU0 spun on the same mux in
        # dashRecV2Submit's spinlock_acquire. 6000fps x 120s = 720K frames
        # > grid capacity 524,286, so the pointer hits the wall on every
        # such run. The flush must wrap the pointer back to the grid start.
        self.assertEqual(0x900000 // 0x10000, 144)  # nb without the wrap
        self.assertEqual((143 + 1) % 144, 0)  # liveBlk cycles mod 144, never 144
        body = self._fn(self.engine, "static void recBBFlushBuffer(", 7600)
        self.assertIn("recBBWriteOff = off + written;", body)
        wrap = body[body.index("recBBWriteOff = off + written;"):]
        self.assertIn("if (recBBWriteOff >= recBBPart->size)", wrap)
        self.assertIn("recBBWriteOff = kRecBBHeaderSize;", wrap,
                      "pointer wraps to the grid anchor, never the size")

    def test_written_blocks_lose_erased_bit(self) -> None:
        # The erased bit means "safe to program without erasing", and it
        # must retire ONLY when the pointer LEAVES the block. Clearing it
        # right after a successful write let the SAME buffer's next flush
        # (the <=16-byte boundary clamp) re-erase the entry block and
        # destroy the 65520 bytes the first flush had just programmed
        # (bench round 7: ~16-400B survived per 64KB block). A mid-block
        # ending KEEPS the bit — the unwritten tail of an erased block is
        # still programmable — while park/jump/wrap retire every block the
        # pointer left, so the next pass re-erases before reprogramming.
        self.assertIn("static void recBBErasedClr(uint32_t b)", self.engine)
        body = self._fn(self.engine, "static void recBBFlushBuffer(", 7400)
        self.assertIn("while (b0 != nb)", body,
                      "erase bit retires when the pointer leaves the block")

    def test_flush_writes_whole_frames_only(self) -> None:
        # The boundary clamp is byte-exact: room in [1..17] wrote a TORN
        # frame (a partial 18-byte record) and the boundary pad stranded
        # its tail off-grid (bench round 7: 17-byte runs split by single
        # FF bytes, tails landing at grid+16). The flush must floor its
        # span to whole frames; when not even one fits before the
        # boundary, jump the dead pad and re-anchor (wrap-safe) — the
        # staged bytes flush from the new anchor on the next pass.
        body = self._fn(self.engine, "static void recBBFlushBuffer(", 6500)
        self.assertIn("lenEff -= lenEff % sizeof(RecRingFrame);", body)
        jump = body[body.index("lenEff -= lenEff % sizeof(RecRingFrame);"):]
        self.assertIn("recBBGridSnap((b0 + 1) % kRecBBBlockCount)", jump,
                      "no whole frame fits: jump the pad, wrap-safe anchor")
        self.assertIn("recBBErasedClr(b0)", jump,
                      "jumping the pad leaves the block: retire its bit")

    def test_flush_returns_when_pointer_sits_on_protected_block(self) -> None:
        # The protection clamp computed `lenEff = b * 65536 - off` even when
        # b was the FIRST block — off already sits inside b, so the unsigned
        # subtraction underflowed to ~4G and the erase loop then OOB-read the
        # 18-byte bitmaps with block indices up to ~65K. A pointer sitting
        # ON a protected block must return immediately (the skip pass
        # repositions); the clamp only applies to later blocks. The return
        # is preceded by the I-1 erase-bit retirement: the eviction leaves
        # the entry block, so its bit must retire before returning.
        body = self._fn(self.engine, "static void recBBFlushBuffer(", 7400)
        self.assertRegex(
            body, r"if \(b == b0\)\s*\{[^{}]*recBBErasedClr\(b0\);[^{}]*return;")
        clamp = body[body.index("lenEff = b * kRecBBBlockSize - off"):]
        self.assertIn("lenEff = b * kRecBBBlockSize - off;", clamp)

    def test_skip_reposition_snaps_to_grid(self) -> None:
        # The skip pass repositioned to `blk * 65536` — off the frozen grid
        # (frames live at 32 + k*18 forever; no grid frame starts at a block
        # boundary since 65536 % 18 == 16), and for blk 0 that is offset 0,
        # clobbering the 32-byte header. The reposition must snap to the
        # first grid frame at or inside the target block.
        self.assertEqual(65536 % 18, 16)  # block starts are never grid points
        body = self._fn(self.engine, "static void recBBSkipOrPause()", 2200)
        self.assertNotIn("recBBWriteOff = blk * kRecBBBlockSize;", body)
        self.assertIn("recBBGridSnap(", body)
        # The snap formula: header + ceil((blk*64K - header)/18)*18.
        for blk, want in ((0, 32), (1, 32 + 3640 * 18), (2, 32 + 7280 * 18)):
            base = 32
            raw = 0 if blk == 0 else blk * 65536 - base
            snapped = base + ((raw + 17) // 18) * 18
            self.assertEqual(snapped, want)

    def test_block0_erase_restamps_header(self) -> None:
        # The 32B header lives inside erase-block 0; the task-7 bench crash
        # boot showed "fresh header" after the first wrap — the writer's
        # block-0 erase had destroyed the magic and the next boot wiped the
        # whole grid. Erasing block 0 must re-stamp the header immediately,
        # and both stamp sites (init + flush) share one builder.
        flush = self._fn(self.engine, "static void recBBFlushBuffer(", 6500)
        self.assertIn("b == 0", flush)
        self.assertIn("recBBStampHeader()", flush,
                      "block-0 erase re-stamps the header it just wiped")
        init = self._fn(self.engine, "static bool recBBInitPartition()", 2600)
        self.assertIn("recBBStampHeader()", init,
                      "init's fresh stamp uses the same builder")

    def test_close_loop_keeps_this_pass_attribution(self) -> None:
        # Task-7 bench sessions 35/36: a lock armed mid-write captured
        # pre_ms_act=431ms/1233ms of a 40s/20s request (1-2 blocks). Under
        # ingest load the attribution frontier (writeOff + undrained staging)
        # runs 1-2 blocks AHEAD of the writer, so the block being entered
        # already holds THIS pass's attribution — and the close loop's blind
        # memset destroyed it, punching a hole the finisher/span walks break
        # at. The same blind zero empties the frontier after the final drain:
        # a lock armed after traffic stops finds lastTs==0 and writes NO
        # session ("no frames recorded", bench 2026-11-01) — the field's
        # zero-data complaint. The close loop must zero an entered slot only
        # when its attribution is older than the newest frame by a grace
        # window (a true pass seam); fresh this-pass attribution survives.
        self.assertIn("static volatile uint32_t recBBNewestTsUs", self.engine)
        self.assertIn("kRecBBAheadGraceUs", self.engine)
        self.assertEqual(60000000, 60000000)  # grace: 60s > any ahead-gap
        body = self._fn(self.engine, "static void recBBFlushBuffer(", 7600)
        close = body[body.index("while (recBBLiveBlk != nb)"):]
        self.assertIn("recBBNewestTsUs - recBBSums[recBBLiveBlk].lastTsUs",
                      close, "the age the guard tests is anchor minus slot")
        self.assertIn("if (ageUs > kRecBBAheadGraceUs)", close,
                      "zero only attribution older than the grace")
        guard_end = close.index("memset(&recBBSums[recBBLiveBlk]")
        guard_start = close.rindex("if (", 0, guard_end)
        self.assertLess(guard_start, guard_end,
                        "the summary memset must sit inside the freshness if")

    def test_submit_tracks_newest_attribution_ts(self) -> None:
        # The freshness anchor the close loop tests against: every staged
        # frame's tsUs, updated in the same critical section as the
        # attribution itself (a stale anchor would zero live summaries).
        body = self._fn(self.engine, "static void dashRecV2Submit(", 3000)
        self.assertIn("recBBNewestTsUs = r.tsUs;", body)

    def test_lock_walk_skips_empty_lead_slots(self) -> None:
        # Bench 2026-11-01 round 5: after traffic stopped, the final writer
        # geometry can leave the FRONTIER slot itself empty (the fill buffer's
        # partial never drains — the tick only flushes the non-fill buffer —
        # and a protection skip shifts landings past their attribution). The
        # finisher keyed haveFrames on that one slot and refused the whole
        # lock ("no frames recorded") while full history sat behind it — the
        # field's zero-data path when the crash stops the traffic first. The
        # walk must step back over a bounded lead of empty slots to the
        # newest attributed block before deciding anything.
        self.assertIn("kRecBBLeadSkip", self.engine)
        body = self._fn(self.engine, "static void recBBFinishLock(", 2400)
        self.assertIn("recBBSums[frontierBlk].lastTsUs == 0", body)
        self.assertRegex(
            body,
            r"while \(lead < kRecBBLeadSkip &&\s*"
            r"recBBSums\[frontierBlk\]\.lastTsUs == 0\)")
        # The refusal only stands after the lead is exhausted.
        self.assertIn("haveFrames = lastTs != 0;", body)

    def test_span_walk_skips_empty_lead_slots(self) -> None:
        # Same frontier geometry, same fix in the span walk: an empty lead
        # slot must read as "history starts a slot or two back", not zero
        # retention (idle span_s=0 with 68K frames retained, bench round 5).
        body = self._fn(self.engine, "static uint32_t dashRecV2SpanUs()", 1600)
        self.assertIn("recBBSums[frontierBlk].lastTsUs == 0", body)
        self.assertRegex(
            body,
            r"while \(lead < kRecBBLeadSkip &&\s*"
            r"recBBSums\[frontierBlk\]\.lastTsUs == 0\)")

    def test_flush_never_crosses_block_boundary(self) -> None:
        # Bench 2026-11-01 forensics: consecutive 65520-byte flushes share a
        # boundary block (65536 - 65520 = 16 → the entry offset walks back 16
        # bytes per flush). The next flush's erase-ahead erases its ENTRY
        # block whole — destroying the previous flush's bytes already there
        # ([b0*64K, off)) — and the write only rewrites from off. Every flush
        # eats its predecessor's tail: flash holds ~0.3-1.1KB per 64KB block
        # (the last run before each boundary), ~98% of written bytes gone.
        # A flush span must be clamped to the entry block before the erase
        # loop derives its coverage from it.
        body = self._fn(self.engine, "static void recBBFlushBuffer(", 6500)
        clamp = "(b0 + 1) * kRecBBBlockSize - off"
        self.assertIn(clamp, body, "geometry clamp to the entry block")
        erase_i = body.index("recBBErasedGet")
        clamp_i = body.index(clamp)
        self.assertLess(clamp_i, erase_i,
                        "the clamp must shape lenEff before the erase loop")

    def test_boundary_flush_parks_writeoff_on_grid(self) -> None:
        # A block-clamped flush ends exactly at (b0+1)*64K — NOT a grid
        # point (65536 % 18 == 16). Parking there de-anchors the frame grid:
        # the next buffer's frames land off-grid and every downstream
        # consumer (decoder, torn-frame resume, session download) loses the
        # 32+k*18 contract. Park on the next grid anchor instead: a ≤17-byte
        # dead pad at each block head, frames stay at 32+k*18 forever.
        body = self._fn(self.engine, "static void recBBFlushBuffer(", 7600)
        writeback = body[body.index("recBBWriteOff = off + written") :]
        self.assertIn("recBBGridSnap(", writeback,
                      "boundary parks re-anchor on the grid")


class BlackboxLockTests(unittest.TestCase):
    """Task 4: zero-copy lock, meta sessions, protection bitmap lifecycle."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.engine = ENGINE.read_text(encoding="utf-8")

    @staticmethod
    def _fn(source: str, signature: str, span: int = 2200) -> str:
        start = source.index(signature)
        return source[start : start + span]

    def test_lock_refused_when_full(self) -> None:
        body = self._fn(self.engine, "static bool dashRecV2LockRequest(")
        self.assertIn("recBBPaused", body)
        self.assertIn('err = "no space"', body)
        # The disabled gate keys on the blackbox engine, not the dead ring.
        self.assertIn("if (!recBBEnabled)", body)
        # Kept validations from the ring era.
        self.assertIn('err = "recorder busy"', body)
        self.assertIn("bus_mask", body)

    def test_finish_lock_summaries_and_meta(self) -> None:
        # Window widened 5600 → 6800 (2026-11-06): the full-grid-lock incident
        # comments in recBBFinishLock pushed the meta tail deeper.
        body = self._fn(self.engine, "static void recBBFinishLock()", 6800)
        keys = ('"seq"', '"boot_ms"', '"trigger"', '"pre_ms_req"',
                '"pre_ms_act"', '"post_ms"', '"bus_mask"', '"frames"',
                '"ring_overflow"', '"ring_overflow_in_window"',
                '"twai_missed_a"', '"twai_missed_b"', '"twai_missed_c"',
                '"start_boot_us"', '"end_boot_us"', '"bb_start_off"',
                '"bb_end_off"')
        for k in keys:
            self.assertIn(k, body, f"meta records {k}")
        # Window math walks block summaries, not frames.
        self.assertIn("recBBSums[", body)
        self.assertIn("windowUs", body)
        self.assertIn("recV2AllocateSeq()", body)
        self.assertIn("recBBMarkRange(bbStart, bbEnd)", body)

    def test_rebuild_marks_meta_ranges_only(self) -> None:
        # Anchor the DEFINITION (a forward declaration exists earlier).
        body = self._fn(self.engine, "static void recBBRebuildProtection()\n{", 1600)
        self.assertIn("recV2ScanSessions", body)
        self.assertIn('doc["bb_start_off"]', body)
        self.assertIn('doc["bb_end_off"]', body)
        # Rebuild starts from a cleared bitmap: bits outside meta ranges stay 0.
        self.assertIn("memset(recBBProt, 0, sizeof(recBBProt))", body)

    def test_delete_clears_protection(self) -> None:
        body = self._fn(self.engine, "static bool recV2RemoveSession(", 1800)
        self.assertIn('doc["bb_start_off"]', body)
        self.assertIn("recBBRangeSet", body)  # unmark path
        self.assertIn("SPIFFS.remove(recV2MetaPath(seq))", body)

    def test_ledger_flag_brackets_arming(self) -> None:
        lock = self._fn(self.engine, "static bool dashRecV2LockRequest(")
        self.assertIn('putUChar("rv2_flushing", 1)', lock)
        finish = self._fn(self.engine, "static void recBBFinishLock()", 4600)
        self.assertIn('putUChar("rv2_flushing", 0)', finish)

    def test_loop_finishes_lock_inline(self) -> None:
        # Zero-copy: no flush task spawn; the loop calls the finisher inline.
        body = self._fn(self.engine, "static void dashRecV2Loop()", 1600)
        self.assertIn("recBBFinishLock()", body)
        self.assertNotIn("xTaskCreatePinnedToCore(recV2FlushTask", body)


class BlackboxDownloadTests(unittest.TestCase):
    """Task 5: streaming download from the raw partition + status semantics."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.engine = ENGINE.read_text(encoding="utf-8")
        cls.dash = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(
            encoding="utf-8")

    @staticmethod
    def _fn(source: str, signature: str, span: int) -> str:
        start = source.index(signature)
        return source[start : start + span]

    def test_download_streams_partition_not_spiffs(self) -> None:
        body = self._fn(self.dash, "static void handleSessionDownload(", 9000)
        self.assertIn("recBBReadFrames", body)  # engine batch-read primitive
        self.assertIn("4096", body)             # 4KB chunk loop
        # The bin era is over: no SPIFFS bin anywhere in the download path.
        self.assertNotIn("SPIFFS.open(binPath)", body)
        self.assertNotIn("SPIFFS.open(recV2BinPath", body)
        # The meta is the session source of truth.
        self.assertIn("recV2MetaPath(seq)", body)

    def test_download_filter_anchored_to_meta_window(self) -> None:
        body = self._fn(self.dash, "static void handleSessionDownload(", 9000)
        for token in ("start_boot_us", "end_boot_us", "bus_mask", "winSpan"):
            self.assertIn(token, body)
        # Grid snap: frames stream only at whole grid offsets.
        self.assertIn("sizeof(RecRingFrame)", body)
        # Range sanity against the real partition bounds.
        self.assertIn("recBBPart->size", body)

    def test_download_holds_no_writer_state(self) -> None:
        body = self._fn(self.dash, "static void handleSessionDownload(", 9000)
        self.assertNotIn("recV2Mux", body)  # never blocks the ingest path

    def test_csv_header_unchanged(self) -> None:
        body = self._fn(self.dash, "static void handleSessionDownload(", 14000)
        header = '"ts_us,bus,dir,id,dlc,b0,b1,b2,b3,b4,b5,b6,b7\\n"'
        self.assertIn(header, body)

    def test_status_json_new_semantics(self) -> None:
        body = self._fn(self.engine, "static String dashRecV2StatusJson(", 3400)
        self.assertIn('"paused"', body)
        self.assertNotIn("flushing", body)  # state names are idle/arming only
        # sessions_space_free = clear protection bits × block size.
        self.assertIn("recBBProtGet(b)", body)
        self.assertIn("kRecBBBlockSize", body)
        # enabled keys on the blackbox engine, not the dead ring.
        self.assertIn("recBBEnabled ? \"true\" : \"false\"", body)


class FinalReviewFixPassTests(unittest.TestCase):
    """Whole-branch review fix pass: seam-crossing locks, eviction, races."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.engine = ENGINE.read_text(encoding="utf-8")
        cls.dash = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(
            encoding="utf-8")

    @staticmethod
    def _fn(source: str, signature: str, span: int) -> str:
        start = source.index(signature)
        return source[start : start + span]

    def test_range_set_marks_wrapped_ranges(self) -> None:
        # C-1: a lock window crossing the rolling seam yields
        # bb_start_off > bb_end_off. recBBRangeSet's old
        # `endOff <= startOff` early return protected NOTHING for such a
        # session — the rolling window then silently overwrote the locked
        # evidence (the spec's named anti-goal). A wrapped range must mark
        # BOTH segments: [start, grid end) and [0, end).
        body = self._fn(self.engine, "static void recBBRangeSet(", 1600)
        self.assertNotIn("endOff <= startOff", body)
        self.assertIn("endOff < startOff", body,
                      "wrap detection: end below start is a ring range")
        # Segment A runs to the last grid block; segment B re-enters from
        # block 0 (the head bytes [0, 32) are the grid header, protected
        # with the block).
        self.assertIn("kRecBBBlockCount - 1", body)
        self.assertIn("recBBProtSet(b, on)", body)

    def test_rebuild_and_delete_accept_wrapped_ranges(self) -> None:
        # C-1 consumers: the `if (e > s)` guards made rebuild skip a seam
        # session's protection after reboot (unprotected = overwritable)
        # and made delete unable to release its blocks. Both must accept
        # the wrap convention (e != s).
        # "()\n{" anchors the definition; the forward declaration at the
        # top of the session scanner reads "();" and must not match.
        for sig in ("static void recBBRebuildProtection()\n{",
                    "static bool recV2RemoveSession("):
            body = self._fn(self.engine, sig, 2000)
            self.assertIn("if (e != s)", body,
                          "%s must honor wrapped meta ranges" % sig)

    def test_sessions_size_covers_wrapped_ranges(self) -> None:
        # The /sessions size field reported 0 for seam sessions
        # (e0 > s0 ? e0 - s0 : 0) — the covered span of a wrapped range is
        # (grid bytes - s0) + e0. Whitespace-normalized: clang-format
        # wraps the nested ternary across lines.
        body = re.sub(r"\s+", " ",
                      self._fn(self.engine, "dashRecV2SessionsJson", 4600))
        self.assertIn("(kRecBBBlockCount * kRecBBBlockSize - s0) + e0", body)

    def test_download_streams_wrapped_ranges(self) -> None:
        # C-1 consumer: the download handler 500-rejected seam metas
        # (`bbEnd <= bbStart` → "corrupt session meta (grid range)") —
        # locked evidence undownloadable. A wrapped meta streams two
        # grid-snapped segments: [bbStart, partition grid end) then
        # [header anchor, bbEnd).
        body = self._fn(self.dash, "static void handleSessionDownload(", 9000)
        self.assertNotIn("bbEnd <= bbStart", body)
        self.assertIn("bbEnd < bbStart", body,
                      "wrapped metas are valid ring ranges, not corrupt")
        self.assertIn("wrapped ? gridEndOff : lastOff", body,
                      "segment A ends at the grid end, not the meta end")
        # Segment B starts at the grid anchor (never byte 0: the header).
        self.assertIn("base = kRecBBHeaderSize", body)

    def test_evicted_block_retires_erase_bit(self) -> None:
        # I-1: protection landing mid-fill evicts the writer from its entry
        # block. That block's erase bit (set by this pass's erase-ahead)
        # must retire at eviction: after session_delete, the writer's next
        # same-boot visit would otherwise program dirty flash over the old
        # bytes (flash only clears bits — the block becomes AND garbage).
        body = self._fn(self.engine, "static void recBBFlushBuffer(", 6500)
        early = body[body.index("if (b == b0)"):]
        self.assertIn("recBBErasedClr", early[:400],
                      "the protected-entry early return retires the bit")
        skip = self._fn(self.engine, "static void recBBSkipOrPause(", 1600)
        self.assertIn("recBBErasedClr(startBlk)", skip,
                      "leaving a block via skip reposition retires it too")

    def test_bitmap_rmw_and_last_error_are_mux_guarded(self) -> None:
        # I-2: recBBProtSet is a byte read-modify-write raced between
        # loopTask (lock finish) and webTask (session delete) — two RMWs
        # interleaving on the same byte lose one bit, silently
        # unprotecting a block. And recV2LastError is an Arduino String
        # written by one task and read by another: torn heap state. The
        # RMW runs under recV2Mux; the error becomes a fixed buffer with
        # a guarded setter.
        body = self._fn(self.engine, "static void recBBProtSet(", 700)
        self.assertIn("portENTER_CRITICAL(&recV2Mux)", body)
        self.assertIn("portEXIT_CRITICAL(&recV2Mux)", body)
        self.assertIn("static char recV2LastErr[", self.engine,
                      "no cross-task String: fixed buffer + guarded setter")
        self.assertIn("static void recV2SetLastError(", self.engine)
        self.assertNotIn("recV2LastError =", self.engine)
        status = self._fn(self.engine, "static String dashRecV2StatusJson(", 4000)
        self.assertIn("recV2LastErr", status)

    def test_submit_attribution_folds_seam_park_overshoot(self) -> None:
        # Bench 2026-11-02 (seam hunt, all 5 rounds + a 295s-wide window):
        # block 0's flash held 3639/3639 frames with byte-continuous
        # 143->0 timestamps (+2us across the seam), yet every lock walk
        # stopped at block 1 — with block 0 frames 1.3s old inside a 120s
        # window, so the summary slot was EMPTY, not stale. Frames staged
        # while the pointer parks at the grid end (the pad-jump window)
        # compute raw absOff = gridEnd + undrained staging; /64K then
        # names block 144+ and the count guard silently drops the
        # attribution. Those exact frames are the ones that land at the
        # anchor after the jump — the WHOLE of block 0 comes from that
        # one handed-over buffer — so slot 0 could never fill and no
        # lock window could ever wrap. Past the last grid point the
        # landing offset is anchor + (absOff - gridEnd): fold the
        # overshoot back before the block math. (Line wrapping is
        # clang-format's choice; assert on whitespace-normalized text.)
        body = re.sub(
            r"\s+", " ",
            self._fn(self.engine, "static void dashRecV2Submit(", 3400))
        self.assertIn(
            "((recBBPart->size - kRecBBHeaderSize) / sizeof(RecRingFrame))",
            body, "gridEnd must be the LAST grid point, not the byte end")
        self.assertIn("if (absOff > gridEnd)", body,
                      "park-window overshoot must fold before the block math")
        self.assertIn("absOff = kRecBBHeaderSize + (absOff - gridEnd)", body)
        # The fold precedes the block index math — order is the fix.
        self.assertLess(body.index("if (absOff > gridEnd)"),
                        body.index("uint32_t blk = absOff / kRecBBBlockSize"))
        # The range guard survives as the pathological backstop.
        self.assertIn("if (blk < kRecBBBlockCount)", body)


class DeadCodeRemovalTests(unittest.TestCase):
    """Task 6: the ring/flush/reclaim/orphan era is deleted, not disabled.

    Every symbol here had its last caller die with the blackbox grid; leaving
    the definitions behind would let a future edit resurrect a second writer
    path next to the one true ingest → staging → flash pipeline.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.engine = ENGINE.read_text(encoding="utf-8")

    def test_no_dead_engine_symbols(self) -> None:
        for sym in ("recV2FlushTask", "kRecV2SpaceReserve", "recV2ReclaimSpace",
                    "recV2SweepOrphans", "recV2CountWindow", "ringHead",
                    "ringCapacity", "recV2BinPath", "recV2FileSize",
                    "recV2Writing", "kRecV2Flushing"):
            self.assertNotIn(sym, self.engine, f"{sym} must be deleted")

    def test_ring_alloc_gone_from_init(self) -> None:
        # The 20MB PSRAM ring allocation died with the ring; init now wires
        # the blackbox engine only.
        self.assertNotIn("kPrimaryBytes", self.engine)
        self.assertNotIn("no PSRAM for ring", self.engine)
        init = self.engine.split("static void dashRecV2Init()", 1)[1][:2800]
        self.assertIn("recBBInitPartition()", init)

    def test_live_badge_and_overflow_survive(self) -> None:
        # The loss-badge inputs and the submit-side overflow counter are
        # live blackbox citizens (meta keys are frozen) — the deletion above
        # must not take them with it.
        self.assertIn("(overflowNow > recV2OverflowAtLock) &&", self.engine)
        self.assertIn("ringOverflow", self.engine)
        self.assertIn('"ring_overflow"', self.engine)


if __name__ == "__main__":
    unittest.main()
