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
        self.assertIn("blackbox,      data, 0x40,   0x16E0000,0x900000,", self.csv)
        spiffs = self._row(self.csv, "spiffs")
        self.assertIn("0x1020000,0x6C0000", spiffs)
        coredump = self._row(self.csv, "coredump")
        self.assertIn("0x1FE0000,0x20000", coredump)
        # Contiguity by integer math: spiffs end == blackbox start, blackbox
        # end == coredump start. No gaps, no overlap.
        self.assertEqual(
            0x1020000 + 0x6C0000, 0x16E0000, "spiffs must end where blackbox starts"
        )
        self.assertEqual(
            0x16E0000 + 0x900000, 0x1FE0000, "blackbox must end where coredump starts"
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
        # One esp_partition_write per staging buffer — never per-frame flash
        # calls. The task-2 header stamp is the only other partition write.
        region = self._fn(self.engine, "static void recBBFlushBuffer(", 3200)
        self.assertEqual(region.count("esp_partition_write(recBBPart"), 1)
        self.assertNotIn("for (uint32_t f = 0", region)  # no per-frame loop


if __name__ == "__main__":
    unittest.main()
