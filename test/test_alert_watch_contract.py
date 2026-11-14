"""Contract tests for the alert-watch core (dash_alert_watch.h) + a real
native compile-and-run of test/test_native_alert_watch_main.cpp via
clang++ with -DDRIVER_P4_3CAN (the header lives inside the P4 guard).

Source-token assertions follow test_ban_forge_contract.py: the core is a
header consumed by the firmware build, so the contract pins the structural
tokens (pure-core discipline, frozen record layout, watch-set table) that
the vehicle wiring (plan Task 5) builds on.
"""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HDR = ROOT / "include" / "dash_alert_watch.h"
NATIVE_MAIN = ROOT / "test" / "test_native_alert_watch_main.cpp"
CLANG = "clang++"


class AlertWatchContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.hdr = HDR.read_text(encoding="utf-8") if HDR.exists() else ""
        cls.main = (
            NATIVE_MAIN.read_text(encoding="utf-8") if NATIVE_MAIN.exists() else ""
        )

    def test_header_exists_in_p4_guard(self) -> None:
        self.assertIn("#if defined(DRIVER_P4_3CAN)", self.hdr)
        self.assertIn("#endif", self.hdr)

    def test_pure_core_discipline(self) -> None:
        # No Serial, no heap, no Arduino — the dash_ban_forge.h rule the
        # spec pins (§3): the core must stay includable from both TUs and
        # natively testable.
        for token in ("Serial", "heap_caps", "Arduino", "esp_timer"):
            self.assertNotIn(token, self.hdr)

    def test_record_layout_frozen_18_bytes(self) -> None:
        self.assertIn("static_assert(sizeof(DashAlertRec) == 18", self.hdr)
        self.assertIn("__attribute__((packed))", self.hdr)

    def test_watch_set_table_pinned(self) -> None:
        # The 14-ID watch set (spec §5.1) is a pinned constant, not a
        # runtime choice.
        for id_tok in ("0x368", "0x346", "0x526", "0x5A5", "0x524",
                       "0x3EE", "0x398", "0x2F1", "0x126", "0x356",
                       "0x3C5", "0x396", "0x33A", "0x728"):
            self.assertIn(id_tok, self.hdr)
        self.assertIn("kAlertWatchIdCount", self.hdr)

    def test_nvs_keys_are_wiring_owned(self) -> None:
        # The three pinned keys (all ≤15 chars, spec §3 silent-reject
        # trap) belong to the vehicle wiring (plan Task 5), never to the
        # pure core — the core must stay storage-agnostic.
        for key in ('"alertEvLog"', '"alertBoot"', '"alert_watch"'):
            self.assertNotIn(key, self.hdr)  # quoted literals = wiring only

    def test_native_main_has_pio_inert_guard(self) -> None:
        self.assertIn("#ifndef NATIVE_BUILD", self.main)
        self.assertIn("#endif // NATIVE_BUILD", self.main)

    def test_native_main_compiles_and_passes_with_p4_define(self) -> None:
        """Real native compile-and-run: sentinels, severity gate, matrix
        rising edges, tripwire payload dedupe (rolling 8), decode names,
        event ring capacity. Without -DDRIVER_P4_3CAN the header
        preprocesses to nothing and the run would fail to link — the
        define is the vacuous-pass guard."""
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "alertwatch_native"
            result = subprocess.run(
                [CLANG, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-DDRIVER_P4_3CAN", "-I", str(ROOT / "include"),
                 str(NATIVE_MAIN), "-o", str(binary)],
                capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0,
                             "compile failed:\n" + result.stderr)
            run = subprocess.run([str(binary)], capture_output=True,
                                 text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("ALL PASS", run.stdout)


if __name__ == "__main__":
    unittest.main()


class AlertWatchWiringContractTests(unittest.TestCase):
    """Plan Task 5: vehicle wiring — hook, NVS events, auto-capture
    executor, endpoints. Source-token pins against the wiring files."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(
            encoding="utf-8")
        cls.main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

    def test_dispatch_hook_rides_the_banforge_block(self) -> None:
        # Same CanC dispatch, after the ban-forge block: frame feed, ring
        # push, and the tripwire delta off the SAME diag the forge logs.
        self.assertIn("dashAlertWatchOnFrame(tagged.id", self.dash)
        self.assertIn("dashAlertWatchRingPush(tagged.id", self.dash)
        self.assertIn("dashAlertWatchOnTripwire(bfDiag.lastDeviant", self.dash)
        i_forge = self.dash.index("dashBanForgeOnFrame(tagged.id")
        i_alert = self.dash.index("dashAlertWatchOnFrame(tagged.id")
        self.assertLess(i_forge, i_alert, "alert hook sits after the forge block")

    def test_nvs_keys_pinned_in_wiring(self) -> None:
        for key in ('"alertEvLog"', '"alertBoot"', '"alert_watch"'):
            self.assertIn(key, self.dash + self.main)

    def test_alert_events_route_exists(self) -> None:
        self.assertIn('"/alert_events"', self.dash)

    def test_status_has_alertwatch_block(self) -> None:
        self.assertIn('"alertWatch"', self.dash)

    def test_executor_evicts_auto_sessions_and_degrades(self) -> None:
        # §8-④: evict oldest AUTO session on capacity pressure, and a
        # still-starved grid must degrade to log-only (deny), not spin.
        self.assertIn("recV2RemoveSession", self.dash)
        self.assertIn('"auto"', self.dash)
        self.assertIn("dashAlertWatchNotifyCaptureBegin(ok, reason)",
                      self.dash)  # the deny path rides reason="no space"
        self.assertIn('"no space"', self.dash)

    def test_psram_ring_alloc_with_fail_closed(self) -> None:
        # 256KB PSRAM ring; alloc failure self-disables the feature
        # (fail-closed) instead of running sentinels without pre-context.
        wiring = self.main + self.dash
        self.assertIn("heap_caps_malloc", wiring)
        self.assertIn("MALLOC_CAP_SPIRAM", wiring)
        self.assertIn("dashAlertWatchSetEnabled(false", self.main +
                      self.dash)

    def test_pref_defaults_on(self) -> None:
        # Passive listening is zero-risk: preference ships ON.
        i = self.main.find("t2canAlertWatchPrefGet")
        body = self.main[i : i + 400]
        self.assertIn("stored = 1", body)
