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


class UiClosureDisciplineTests(unittest.TestCase):
    """2026-11-15 bug: loadAlertEvents was appended OUTSIDE the dashboard
    closure, so the closure-private set()/T() helpers threw a silent
    ReferenceError and the event list rendered '暂无告警' forever. Pin:
    standalone alert functions must write DOM directly, never borrow
    closure helpers."""

    def test_loader_never_borrows_closure_helpers(self) -> None:
        ui = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text(
            encoding="utf-8")
        start = ui.index("async function loadAlertEvents")
        body = ui[start : ui.index("}", ui.index("catch(e){}", start)) + 1]
        self.assertNotIn("set(", body)
        self.assertNotIn("T(", body)
        self.assertIn("alwSet(", body)


class FinalReviewFixPassTests(unittest.TestCase):
    """Whole-branch review 2026-11-15: C1/C2/I1-I4 + M1/M6 re-graded
    Important. Each test pins one fix; the stories live in the review
    result and the ledger."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(
            encoding="utf-8")
        cls.engine = (ROOT / "include" / "web" / "dash_recorder_v2.h").read_text(
            encoding="utf-8")
        cls.core = (ROOT / "include" / "dash_alert_watch.h").read_text(
            encoding="utf-8")

    def _loop(self) -> str:
        start = self.dash.rindex("static void mcpDashboardLoop")
        return self.dash[start : start + 2600]

    def test_c1_guards_precede_the_update_gate(self) -> None:
        # In the true OTA wedge (Update.begin ran, FILE_END never comes)
        # Update.isRunning() stays true forever — guards placed after the
        # early return are dead code exactly when they are needed.
        body = self._loop()
        gate = body.index("if (Update.isRunning())")  # executable form; the fix comment names it earlier
        self.assertLess(body.index("if (g_otaRxActive"), gate)
        self.assertLess(body.index("g_webAliveMs"), gate)

    def test_c2_defense_get_carries_alertwatch_and_post_guard(self) -> None:
        # Without the GET field the UI toggle never initializes; any
        # defense-page save then posts alert_watch:'0' and silently,
        # persistently disables the sentinel. And a bare alert_watch POST
        # must clear the outer arg guard too.
        get_start = self.dash.index("static void handleDefenseConfig()")
        region = self.dash[get_start : get_start + 2200]
        self.assertIn('server.hasArg("alert_watch")', region)
        cfg = self.dash[self.dash.rindex("static String dashDefenseConfigJson()"):]
        self.assertIn('\\"alertWatch\\"', cfg[:1200])  # escaped form in C++ source

    def test_i1_auto_marker_only_on_own_captures(self) -> None:
        # The external path locks the USER's manual session; tagging it
        # auto makes it evictable — violating the never-evict invariant.
        start = self.dash.index("static void dashAlertWatchService()")
        body = self.dash[start : start + 4000]
        i_guard = body.index("if (g_alertWeArmed)")
        i_patch = body.index("dashAlertPatchAutoMarker()")
        self.assertLess(i_guard, i_patch, "patch must sit inside the we-armed guard")

    def test_i2_ring_draining_pauses_pushes(self) -> None:
        self.assertIn("g_alertRingDraining", self.core)
        push = self.core[self.core.index("inline bool dashAlertWatchRingPush"):
                         self.core.index("inline size_t dashAlertWatchRingCount")]
        self.assertIn("g_alertRingDraining", push)
        svc = self.dash[self.dash.index("static void dashAlertWatchService()"):
                        self.dash.rindex("static void handleAlertEvents()")]  # rindex: definition, not fwd decl
        self.assertIn("g_alertRingDraining = true", svc)
        self.assertIn("g_alertRingDraining = false", svc)

    def test_i3_backfill_mode_protects_stale_summaries(self) -> None:
        # Backfilled blocks carry minutes-old tsUs while live frames keep
        # newest≈now; the close loop's 60s ahead-grace would zero their
        # summaries and the lock walk would stop at the hole, truncating
        # pre-context to ~60-90s. Backfill mode suppresses the zeroing.
        self.assertIn("recBBBackfill", self.engine)
        self.assertIn("dashRecV2SetBackfill", self.engine)
        zero_zone = self.engine[
            self.engine.index("uint32_t ageUs = recBBNewestTsUs"):
            self.engine.index("memset(&recBBSums[recBBLiveBlk], 0,") + 60]
        self.assertIn("!recBBBackfill", zero_zone)
        svc = self.dash[self.dash.index("static void dashAlertWatchService()"):
                        self.dash.rindex("static void handleAlertEvents()")]  # rindex: definition, not fwd decl
        self.assertIn("dashRecV2SetBackfill(true)", svc)
        self.assertIn("dashRecV2SetBackfill(false)", svc)

    def test_i4_capacity_budget_covers_lock_post(self) -> None:
        # SM caps at 60s but the lock keeps streaming its own 30s post:
        # armed time reaches 90s; the eviction budget must cover it plus
        # margin (120s), not the bare 60s cap.
        svc = self.dash[self.dash.index("needBytes"):
                        self.dash.index("needBytes") + 300]
        self.assertIn("120", svc)

    def test_m1_eviction_sorts_ascending(self) -> None:
        # recV2ScanSessions returns directory order; the repo's own
        # precedent sorts ascending before walking newest-first. Eviction
        # must delete the OLDEST auto session, never the newest evidence.
        fn = self.dash[self.dash.index("static uint32_t dashAlertOldestAutoSession"):
                       self.dash.index("static void dashAlertPatchAutoMarker")]
        self.assertIn("seqs[k] > key", fn)  # insertion sort, repo-precedent shape

    def test_m6_event_persist_survives_recorder_death(self) -> None:
        # Events are the surviving-evidence channel; their persistence
        # must not be gated on the blackbox partition's health.
        svc = self.dash[self.dash.index("static void dashAlertWatchService()"):
                        self.dash.rindex("static void handleAlertEvents()")]  # rindex: definition, not fwd decl
        self.assertLess(svc.index("dashAlertPersistEvents"),
                        svc.index("!recBBEnabled"))


class C2FollowUpTests(unittest.TestCase):
    """C2 follow-up (2026-11-15 field catch): the defense GET field is a
    plain bool, but the toggle restore read the /status object shape
    (.enabled) — the toggle never initialized even with the GET fix."""

    def test_toggle_restore_reads_the_bool_form(self) -> None:
        ui = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text(
            encoding="utf-8")
        i = ui.index("alwRTgl")
        line = ui[i : ui.index("\n", i)]
        self.assertIn("!!d.alertWatch", line)
        self.assertNotIn("d.alertWatch.enabled", line)
