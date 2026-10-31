"""Contract tests for the P4-3CAN recorder v2 (always-on PSRAM ring).

Source-token assertions in the style of test_dashboard_api_contract.py: the
recorder v2 engine lives in a header consumed by the firmware build, so the
contract pins the structural tokens that later tasks (lock/flush, sessions,
UI) build on and the T2CAN isolation guarantees.
"""

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DASH = ROOT / "include" / "web" / "mcp2515_dashboard.h"
REC2 = ROOT / "include" / "web" / "dash_recorder_v2.h"
PIO = ROOT / "platformio.ini"

# Baseline pinned 2026-11-01 (task 1, base 843857f): one definition plus four
# call sites (sniff-CanC / onFrame / onTxFrame / AP auto-restore). Recorder v2
# must not add a fifth — TX capture rides mcpDashOnTxFrame only, and extra
# dashRecordCanFrame calls would double-count TX frames.
K_LEGACY_RECORDER_OCCURRENCES = 5


class DashRecorderV2ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = DASH.read_text(encoding="utf-8")
        cls.rec2 = (
            REC2.read_text(encoding="utf-8") if REC2.exists() else ""
        )
        cls.pio = PIO.read_text(encoding="utf-8")

    # --- record format -----------------------------------------------------

    def test_ring_frame_is_18_bytes_and_packed(self) -> None:
        # Spec D6 field list (tsUs4+id4+dlc1+flags1+data8) = 18 bytes; the
        # spec prose said "16B" but the layout arithmetic wins (ledgered
        # ruling, task 1).
        self.assertIn(
            "static_assert(sizeof(RecRingFrame) == 18", self.rec2,
            "RecRingFrame must be exactly 18 bytes (tsUs/id/dlc/flags/data[8])")
        self.assertIn("uint32_t tsUs", self.rec2)
        self.assertIn("uint8_t flags", self.rec2)

    def test_submit_uses_unsigned_wraparound_safe_span(self) -> None:
        # tsUs wraps every 71.6 min; every window computation must rely on
        # unsigned subtraction, never on ordered comparisons of raw tsUs.
        self.assertRegex(
            self.rec2, r"\(\s*uint32_t\s*\)\s*\([A-Za-z_]+ - [A-Za-z_]+",
            "span computation must use a (uint32_t)(newest - oldest) form")
        self.assertNotIn(
            "tsUs <", self.rec2,
            "ordered compares on wrapping timestamps are forbidden")

    def test_psram_first_with_fallback(self) -> None:
        self.assertIn("heap_caps_malloc", self.rec2)
        self.assertIn("MALLOC_CAP_SPIRAM", self.rec2)
        self.assertIn("20u * 1024u * 1024u", self.rec2,
                      "primary ring budget is 20MB of PSRAM")
        self.assertIn("10u * 1024u * 1024u", self.rec2,
                      "degraded ring budget is 10MB of PSRAM")
        self.assertGreaterEqual(
            self.rec2.count("heap_caps_malloc"), 2,
            "allocation must retry at the smaller budget before giving up")

    # --- dashboard hook wiring ---------------------------------------------

    def test_three_hooks_gated_by_macro(self) -> None:
        self.assertEqual(
            self.dash.count("dashRecV2Submit"), 3,
            "exactly three submit hooks: onFrame / sniffCan3 / onTxFrame")
        self.assertGreaterEqual(
            self.dash.count("DASH_RECORDER_V2"), 4,
            "every hook (3 submits + init) sits inside its own macro guard")

    def test_init_hook_runs_after_spiffs_mount(self) -> None:
        self.assertIn("dashRecV2Init()", self.dash)
        init_line = self.dash.index("dashRecV2Init()")
        spiffs_line = self.dash.index("SPIFFS.begin(true)")
        self.assertLess(
            spiffs_line, init_line,
            "ring init runs in dashboard setup after the SPIFFS mount")

    def test_tx_hook_is_single_path(self) -> None:
        self.assertIn("static void mcpDashOnTxFrame", self.dash)
        # TX submit must live inside mcpDashOnTxFrame (the driver onSendFrame
        # callback — one invocation per transmitted frame).
        tx_start = self.dash.index("static void mcpDashOnTxFrame")
        tx_end = self.dash.index("static ", tx_start + 10)
        tx_body = self.dash[tx_start:tx_end]
        self.assertIn("dashRecV2Submit", tx_body)
        # ...and the legacy recorder call-site count must not grow.
        self.assertEqual(
            self.dash.count("dashRecordCanFrame("),
            K_LEGACY_RECORDER_OCCURRENCES,
            "legacy /rec_start chain stays byte-identical (no double-count)")

    def test_p4_env_defines_recorder_v2(self) -> None:
        self.assertIn("-DDASH_RECORDER_V2", self.pio)

    def test_engine_is_macro_gated(self) -> None:
        self.assertIn(
            "#if defined(DASH_RECORDER_V2)", self.rec2,
            "the whole engine compiles away on non-P4 boards")


if __name__ == "__main__":
    unittest.main()
