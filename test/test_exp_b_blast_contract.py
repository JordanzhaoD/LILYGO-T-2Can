"""Contract tests for experiment B: bench NO_ACK self-reception flood.

Crash forensics round 7 (2026-11-03): the in-car 12-22s crash family is
register-file group zeroing under TWAI interrupt pressure (see
docs/verification/field-trip-20261103/ANALYSIS.md section 4). The bench
never reproduces because rv2_flood/rv2_frame inject ABOVE the driver —
the TWAI ISR never fires. Experiment B closes the gap the honest way:
reinstall the controllers in TWAI_MODE_NO_ACK and transmit with the
self-reception flag so every frame loops back through the REAL
ISR -> rx queue -> can_rt chain.

These tests pin the contract so the decisive experiment cannot rot:
- the DIAG command and help entry exist (P4-only, excluded from native),
- the blast is bounded (duration cap) and always restores the original
  controller modes, including on mode-switch failure,
- self-reception frames carry TWAI_MSG_FLAG_SELF and a zero transmit
  timeout (blast must never block the pacing loop),
- the ID sweep yields exactly 160 distinct ids per lane (the field
  trigger: the bus-2 discovered-ID table filling to capacity) with the
  hot law ids as EXTRA frames that never replace sweep entries.
"""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BLAST = ROOT / "include" / "drivers" / "can_bench_blast.h"
PRIMARY = ROOT / "include" / "drivers" / "twai_driver.h"
SECONDARY = ROOT / "include" / "drivers" / "p4_twai_secondary.h"
DASH = ROOT / "include" / "web" / "mcp2515_dashboard.h"
MAIN = ROOT / "src" / "main.cpp"


def _body(source: str, start_marker: str, end_marker: str) -> str:
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


class ExpBHelpAndDispatchContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = DASH.read_text(encoding="utf-8")
        cls.main = MAIN.read_text(encoding="utf-8")

    def test_help_lists_exp_b(self) -> None:
        self.assertIn("exp_b [fps] [s] [mask]", cls_dash_help(self.dash))

    def test_dispatch_calls_app_bench_blast(self) -> None:
        branch = _body(self.dash, 'strncmp(start, "exp_b", 5)', "#endif")
        self.assertIn("appBenchBlastCommand(start + 5);", branch)

    def test_dispatch_is_p4_only(self) -> None:
        # The branch must sit inside the P4 guard so other targets and the
        # native matrix never see it.
        idx = self.dash.index('strncmp(start, "exp_b", 5)')
        guard = self.dash.rindex("#if defined(DRIVER_P4_3CAN)", 0, idx)
        end = self.dash.index("#endif", guard)
        self.assertLess(guard, idx)
        self.assertLess(idx, end)

    def test_main_defines_command_under_p4_guard(self) -> None:
        idx = self.main.index("bool appBenchBlastCommand(const char *args)")
        guard = self.main.rindex("#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)", 0, idx)
        end = self.main.index("#endif", guard)
        self.assertLess(guard, idx)
        self.assertLess(idx, end)


def cls_dash_help(dash_source: str) -> str:
    return _body(dash_source, "static void dashSerialPrintHelp()", "static void dashSerialPrintSystemStatus()")


class ExpBBlastSemanticsContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.blast = BLAST.read_text(encoding="utf-8")
        cls.primary = PRIMARY.read_text(encoding="utf-8")
        cls.secondary = SECONDARY.read_text(encoding="utf-8")
        cls.main = MAIN.read_text(encoding="utf-8")

    def test_duration_is_capped(self) -> None:
        self.assertIn("kMaxSecs = 1800", self.blast)
        self.assertIn("if (secs > kMaxSecs)", self.blast)

    def test_always_restores_original_modes(self) -> None:
        # The restore calls sit AFTER the send loop, outside the
        # modeSwitchOk branch — they run even when the switch failed.
        restore = _body(self.blast, "// Always restore the pre-experiment modes.", "st.elapsedMs")
        for lane in ("d0", "d1", "d2"):
            self.assertIn(f"if ({lane})", restore)

    def test_send_self_uses_self_flag_and_zero_timeout(self) -> None:
        for name, src in (("primary", self.primary), ("secondary", self.secondary)):
            with self.subTest(driver=name):
                body = _body(src, "bool benchSendSelf(", "#endif")
                self.assertIn("TWAI_MSG_FLAG_SELF", body)
                # Zero timeout: a full tx queue drops the frame instead of
                # stalling the pacing loop (counted as fail, like the field).
                self.assertRegex(body, r"twai_transmit(_v2)?\([^;]*;\s*\n?\s*.*0\)") if False else None
                self.assertIn("twai_transmit(&msg, 0);" if name == "primary" else "twai_transmit_v2(handle_, &msg, 0);", body)

    def test_bench_set_mode_restores_on_failure(self) -> None:
        for name, src in (("primary", self.primary), ("secondary", self.secondary)):
            with self.subTest(driver=name):
                body = _body(src, "bool benchSetMode(", "bool benchSendSelf(")
                self.assertIn("g_config_.mode = keep;", body)
                self.assertIn("installAndStartLocked(); // best-effort restore", body)

    def test_sweep_yields_exactly_160_distinct_ids(self) -> None:
        # Reproduce the sweep formula from canBenchBlastFrame and prove the
        # field trigger: 160 distinct ids = the bus-2 table capacity.
        ids = sorted(((i * 13 + 0x21) & 0x7FF) for i in range(160))
        self.assertEqual(len(ids), 160)
        self.assertEqual(len(set(ids)), 160)
        # The sweep id formula in the header must match this model.
        self.assertIn("((sweepIdx % 160u) * 13u + 0x21u) & 0x7FFu", self.blast)

    def test_hot_ids_are_extra_not_sweep_replacements(self) -> None:
        # Hot law ids ride every 32nd frame and the sweep index only
        # advances on sweep frames — the table still sees all 160 ids.
        self.assertIn("if ((seq[lane] & 31u) != 0)", self.blast)
        self.assertIn("sweep[lane]++; // sweep ids only advance on sweep frames", self.blast)
        hot = _body(self.blast, "kHot[4]", ");")
        for hid in ("0x249", "0x118", "0x399", "0x045"):
            self.assertIn(hid, hot)

    def test_default_is_b1_shape(self) -> None:
        # Approved experiment B1: buses 1+2 (mask 3), 800 fps/lane, 300s.
        body = _body(self.main, "bool appBenchBlastCommand(const char *args)", "Serial.printf(\"[exp_b] BENCH-ONLY")
        self.assertIn("fps = 800, secs = 300, mask = 3", body)

    def test_restore_modes_match_construction(self) -> None:
        # bus1/bus2 install NORMAL, bus3 (tertiary) is LISTEN_ONLY — the
        # blast must put the world back exactly as it found it.
        body = _body(self.main, "canBenchBlastRun(", "Serial.printf(\"[exp_b] done:")
        self.assertIn("d0, TWAI_MODE_NORMAL", body)
        self.assertIn("d1, TWAI_MODE_NORMAL", body)
        self.assertIn("d2, TWAI_MODE_LISTEN_ONLY", body)


if __name__ == "__main__":
    unittest.main()
