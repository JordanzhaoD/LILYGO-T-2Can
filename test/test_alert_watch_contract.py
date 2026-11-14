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


class AlertWatchUiContractTests(unittest.TestCase):
    """Plan Task 6: defense-page alert forensics card (spec §5.4)."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.ui = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text(
            encoding="utf-8")
        cls.dash = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(
            encoding="utf-8")

    def test_card_elements_exist(self) -> None:
        for el in ('id="alw-status"', 'id="alw-matrix"', 'id="alw-events"',
                   'id="alw-tgl"'):
            self.assertIn(el, self.ui)

    def test_events_endpoint_fetched(self) -> None:
        self.assertIn("fetch('/alert_events')", self.ui)

    def test_toggle_rides_defense_config(self) -> None:
        self.assertIn("alert_watch:", self.ui)  # saveDefenseConfig payload key

    def test_status_poll_feeds_card(self) -> None:
        self.assertIn("d.alertWatch", self.ui)

    def test_events_endpoint_exposes_matrix(self) -> None:
        # The live 0x346 latch (decoded names) ships with the event log —
        # and must live INSIDE handleAlertEvents (a 2026-11-14 field
        # incident landed it in the /frames handler via a silent anchor).
        start = self.dash.rindex("static void handleAlertEvents()")  # definition, past the fwd decl
        body = self.dash[start : start + 3000]
        self.assertIn("matrixBits", body)
        self.assertIn('\\"matrix\\"', body)  # as escaped in C++ source

    def test_i18n_covers_new_terms(self) -> None:
        # One representative term appears both as data-i18n and in the
        # EN dictionary (the zh source string is the dictionary key).
        self.assertIn('data-i18n="告警取证"', self.ui)
        self.assertIn("'告警取证':", self.ui)


class UiScriptSyntaxTests(unittest.TestCase):
    """2026-11-14 field incident: a double comma in the i18n dictionary
    shipped through the minifier silently and killed the ENTIRE page at
    runtime (one script block = one syntax unit: every control died).
    Pin: every <script> block in the UI source must parse under node."""

    def test_all_script_blocks_parse(self) -> None:
        import re
        import shutil
        import subprocess
        import tempfile
        node = shutil.which("node")
        if not node:
            self.skipTest("node not available")
        src = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text(
            encoding="utf-8", errors="replace")
        blocks = re.findall(r"<script[^>]*>(.*?)</script>", src, re.S)
        self.assertGreaterEqual(len(blocks), 1, "UI must have script blocks")
        with tempfile.TemporaryDirectory() as tmp:
            for i, block in enumerate(blocks):
                if not block.strip():
                    continue
                p = Path(tmp) / f"ui_{i}.js"
                p.write_text(block, encoding="utf-8")
                r = subprocess.run([node, "--check", str(p)],
                                   capture_output=True, text=True, timeout=60)
                self.assertEqual(r.returncode, 0,
                                 f"script block {i} has a syntax error "
                                 f"(this kills the whole page):\n{r.stderr[:400]}")


class OtaStallGuardContractTests(unittest.TestCase):
    """2026-11-15 field root cause: a mid-upload client death leaves the
    WebServer parse loop spinning on a half-dead socket forever; the OTA
    handler deinit'd the task watchdog at start and NOTHING re-arms or
    watches — webTask (httpd + ArduinoOTA + wifi self-heal) wedges until
    power cycle. Fix: a stall sentinel in the LOOP task (proven survivor):
    upload active && >30 s without a WRITE callback -> controlled restart."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(
            encoding="utf-8")

    def test_upload_callbacks_track_progress(self) -> None:
        start = self.dash.index("static void handleOtaUpload()")
        body = self.dash[start : start + 2500]  # result handler sits earlier in file
        self.assertIn("UPLOAD_FILE_START", body)
        self.assertIn("g_otaRxActive = true", body)       # armed at start
        self.assertIn("g_otaRxLastMs = millis()", body)    # touched on write
        self.assertIn("UPLOAD_FILE_ABORTED", body)
        self.assertIn("g_otaRxActive = false", body)       # cleared on end/abort

    def test_stall_sentinel_lives_in_the_loop_task(self) -> None:
        # The sentinel must NOT live in webTask (the task that hangs).
        loop_start = self.dash.index("static void mcpDashboardLoop")
        loop = self.dash[loop_start : loop_start + 2000]
        self.assertIn("kOtaStallMs", loop)
        self.assertIn("ESP.restart()", loop)
        web = self.dash[self.dash.index("static void webTask"):
                        self.dash.index("static void mcpDashboardSetup")]
        self.assertNotIn("kOtaStallMs", web)


class WebTaskLivenessContractTests(unittest.TestCase):
    """2026-11-15 field pattern: httpd dies occasionally during NORMAL
    driving, not only during OTA. webTask (8KB stack) alone runs httpd +
    ArduinoOTA + wifi self-heal, and any of several stall vectors wedges
    it until power cycle: stalled upload parse (fixed separately), a
    stalled download/response send to a locked-screen phone, an
    esp_hosted radio death, or handler stack pressure. Unified hardening:
    a liveness heartbeat — webTask stamps every loop, streaming handlers
    stamp every chunk, and the loop task (proven survivor) restarts the
    board after 60 s of zero progress (the restart also reboots the C6,
    healing hosted-side wedges). Plus a stack bump 8K -> 12K."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(
            encoding="utf-8")

    def test_webtask_stamps_heartbeat(self) -> None:
        start = self.dash.index("static void webTask")
        body = self.dash[start : start + 400]
        self.assertIn("g_webAliveMs", body)

    def test_loop_task_owns_liveness_guard(self) -> None:
        start = self.dash.rindex("static void mcpDashboardLoop")
        body = self.dash[start : start + 2200]
        self.assertIn("kWebDeadMs", body)
        self.assertIn("ESP.restart()", body)
        self.assertIn("g_webAliveMs", body)

    def test_streaming_download_stamps_per_chunk(self) -> None:
        # The session download is the longest legit handler; its chunk
        # loops must feed the heartbeat so a healthy slow client never
        # trips the guard (only a truly stalled socket does).
        start = self.dash.index("static void handleSessionDownload")
        body = self.dash[start : start + 9000]
        self.assertIn("g_webAliveMs", body)

    def test_webtask_stack_raised(self) -> None:
        # 8K + String-heavy handlers (sessions list, alert events ~3KB)
        # ran close to the documented overflow edge; 12K is cheap
        # insurance on a PSRAM-rich board.
        self.assertIn("xTaskCreatePinnedToCore(webTask, \"web\", 12288",
                      self.dash)
        self.assertIn("xTaskCreate(webTask, \"web\", 12288", self.dash)
