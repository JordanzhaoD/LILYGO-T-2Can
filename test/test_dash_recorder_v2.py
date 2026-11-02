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
        # Blackbox era: PSRAM backs the 2×64KB staging double-buffer (the
        # 20MB ring died with the ring). No staging → recorder disabled:
        # the engine refuses to run with a flash-blocking hot path.
        self.assertIn("heap_caps_malloc", self.rec2)
        self.assertIn("MALLOC_CAP_SPIRAM", self.rec2)
        self.assertGreaterEqual(
            self.rec2.count("heap_caps_malloc(kRecBBBlockSize"), 2,
            "both staging buffers are block-sized PSRAM allocations")
        self.assertIn(
            "no PSRAM for blackbox staging - recorder disabled", self.rec2,
            "missing staging disables the engine instead of degrading it")

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


class DashRecorderV2LockContractTests(unittest.TestCase):
    """Task 2: lock state machine + session flush (shrink / rolling delete)."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = DASH.read_text(encoding="utf-8")
        cls.rec2 = (
            REC2.read_text(encoding="utf-8") if REC2.exists() else ""
        )

    def test_lock_request_validates_bus_mask(self) -> None:
        self.assertIn("if (!busMask)", self.rec2,
                      "bus_mask of 0 (all switches off) must be rejected")
        self.assertIn("bus_mask", self.rec2,
                      "rejection error names the offending field")
        self.assertIn("400", self.dash,
                      "handler answers 400 on validation failures")

    def test_single_writer_guard(self) -> None:
        # Blackbox era: the state machine is idle/arming only — a lock is
        # refused while an arming window is counting down (the one-writer
        # invariant; the flush-task flag died with the flush task).
        self.assertIn("dashRecV2LockRequest", self.rec2)
        lock_start = self.rec2.index("dashRecV2LockRequest")
        guard_region = self.rec2[lock_start:lock_start + 700]
        self.assertIn("if (recV2State != kRecV2Idle)", guard_region,
                      "lock request refuses while arming is in flight")
        self.assertIn("busy", guard_region,
                      "lock request refuses while a flush is in flight")
        self.assertIn("409", self.dash,
                      "handler answers 409 on the busy path")

    def test_seq_nvs_monotonic(self) -> None:
        self.assertIn("rv2_seq", self.rec2)
        self.assertIn("putUInt", self.rec2)
        self.assertIn("getUInt", self.rec2)

    def test_routes_and_loop_wired(self) -> None:
        self.assertIn(
            'server.on("/ring_lock", HTTP_POST, handleRingLock)', self.dash)
        self.assertIn(
            'server.on("/ring_status", HTTP_GET, handleRingStatus)', self.dash)
        loop_fn = self.dash.index("static void mcpDashboardLoop()")
        loop_body = self.dash[loop_fn:loop_fn + 900]
        self.assertIn("dashRecV2Loop()", loop_body,
                      "lock state machine is polled from the dashboard loop")

    def test_status_json_shape(self) -> None:
        for token in ('"enabled"', '"span_s"', '"fps"', '"lock"',
                      '"state"', '"remain_ms"', '"bus_mask"'):
            self.assertIn(token, self.rec2,
                          f"status JSON exposes {token}")

    def test_meta_field_set_complete(self) -> None:
        # Blackbox-era schema: ring-era keys preserved + grid byte range.
        for token in ('"seq"', '"boot_ms"', '"trigger"', '"pre_ms_req"',
                      '"pre_ms_act"', '"post_ms"', '"bus_mask"', '"frames"',
                      '"ring_overflow"', '"ring_overflow_in_window"',
                      '"twai_missed_a"', '"twai_missed_b"', '"twai_missed_c"',
                      '"start_boot_us"', '"end_boot_us"', '"bb_start_off"',
                      '"bb_end_off"'):
            self.assertIn(token, self.rec2,
                          f"session meta records {token}")


class DashRecorderV2SessionsContractTests(unittest.TestCase):
    """Task 3: session list / download (csv|bin) / delete API."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = DASH.read_text(encoding="utf-8")
        cls.rec2 = (
            REC2.read_text(encoding="utf-8") if REC2.exists() else ""
        )

    def test_download_validates_frame_alignment(self) -> None:
        # Blackbox era: the meta names a grid byte range; the handler snaps
        # it onto the 18B frame grid so only whole grid frames stream, and
        # refuses a range that escapes the real partition bounds.
        dl_at = self.dash.index("static void handleSessionDownload(")
        dl = self.dash[dl_at:dl_at + 3000]
        self.assertIn(
            "sizeof(RecRingFrame)", dl,
            "range is snapped onto whole grid frames")
        self.assertIn(
            "corrupt", dl,
            "the alignment failure names the problem")

    def test_csv_header_exact(self) -> None:
        self.assertIn(
            "ts_us,bus,dir,id,dlc,b0,b1,b2,b3,b4,b5,b6,b7", self.dash,
            "CSV header is frozen (tools parse it positionally)")

    def test_delete_removes_bin_and_meta(self) -> None:
        # Blackbox era: a session is its meta (the grid range it protects).
        # Delete drops the meta and unprotects the range; the bin file died
        # with the flush engine.
        self.assertIn(
            "recV2RemoveSession", self.dash,
            "delete handler routes through the session remover")
        helper_at = self.rec2.index("recV2RemoveSession")
        helper = self.rec2[helper_at:helper_at + 900]
        self.assertIn(
            "SPIFFS.remove(recV2MetaPath(seq))", helper,
            "the remover drops the meta")
        self.assertNotIn(
            "SPIFFS.remove(recV2BinPath", helper,
            "there is no bin to remove")

    def test_sessions_json_newest_first(self) -> None:
        self.assertIn("dashRecV2SessionsJson", self.rec2)
        self.assertRegex(
            self.rec2, r">= 0; i--",
            "list is emitted newest-first (descending seq)")

    def test_sessions_json_fields(self) -> None:
        for token in ('"seq"', '"boot_ms"', '"pre_ms_act"', '"post_ms"',
                      '"bus_mask"', '"frames"', '"size"', '"loss_badge"'):
            self.assertIn(token, self.rec2,
                          f"sessions list exposes {token}")

    def test_routes_registered(self) -> None:
        self.assertIn(
            'server.on("/sessions", HTTP_GET, handleSessions)', self.dash)
        self.assertIn(
            'server.on("/session_download", HTTP_GET, handleSessionDownload)',
            self.dash)
        self.assertIn(
            'server.on("/session_delete", HTTP_POST, handleSessionDelete)',
            self.dash)


class DashRecorderV2WallContractTests(unittest.TestCase):
    """Task 4: SNTP wall clock with boot-relative fallback.

    Plan named Arduino's configTime()/getLocalTime(), which do not exist in
    this espidf (non-Arduino) framework — ruled to lwIP sntp_* + libc time()
    (ledger, task 4). Tests pin the real API surface.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = DASH.read_text(encoding="utf-8")
        cls.rec2 = (
            REC2.read_text(encoding="utf-8") if REC2.exists() else ""
        )

    def test_sntp_dual_servers_polled_once(self) -> None:
        for token in ('sntp_setservername(0, "pool.ntp.org")',
                      'sntp_setservername(1, "ntp.aliyun.com")',
                      "sntp_init()", "SNTP_OPMODE_POLL"):
            self.assertIn(token, self.rec2, f"wall sync uses {token}")

    def test_sync_once_gated_by_wall_synced(self) -> None:
        # 幂等：成功过就跳过 — guard must live at the top of the attempt.
        self.assertIn("wallSynced", self.rec2)
        attempt = self.rec2.index("dashRecV2TrySyncTime")
        self.assertIn(
            "wallSynced", self.rec2[attempt:attempt + 400],
            "sync attempt returns early once the wall clock is set")

    def test_hook_on_sta_connected_edge(self) -> None:
        self.assertIn("dashRecV2TrySyncTime()", self.dash)
        hook = self.dash.index("dashRecV2TrySyncTime()")
        edge = self.dash.index("dashGatewayOnStaConnected(WiFi.staNetif()")
        self.assertGreater(hook, edge,
                           "hook fires after the STA connected callback")
        self.assertLess(
            hook - edge, 500,
            "hook sits in the connected branch, not pages away")

    def test_loop_reprobes_until_synced(self) -> None:
        # The connect edge fires once; SNTP lands seconds later, so the
        # 1s tick must keep probing until the wall clock is real.
        loop_at = self.rec2.index("static void dashRecV2Loop()")
        loop_body = self.rec2[loop_at:loop_at + 900]
        self.assertIn(
            "dashRecV2TrySyncTime", loop_body,
            "dashRecV2Loop re-probes wall sync each 1s tick")

    def test_wall_never_written_unconditional(self) -> None:
        # No fake wall times: every JSON emission of the wall key sits
        # behind a has-wall guard (flush gate or meta read). Emissions
        # are the `,"wall":` raw-string chunks; bare `doc["wall"]`
        # reads are guarded on their own line and are not writes.
        code_only = "\n".join(
            ln.split("//", 1)[0] for ln in self.rec2.splitlines())
        hits = [i for i in range(len(code_only))
                if code_only.startswith(',"wall":', i)]
        self.assertGreaterEqual(
            len(hits), 2, "wall key must be emitted by flush and list")
        for at in hits:
            window = code_only[max(0, at - 250):at]
            self.assertTrue(
                "HasWall" in window or "isNull" in window,
                f'wall emission at {at} must be conditional')

    def test_wall_probe_uses_plausibility_floor(self) -> None:
        # time() before sync returns 1970-ish garbage; the probe must
        # demand a post-2020 epoch before trusting it.
        self.assertIn("1600000000", self.rec2)


class DashRecorderV2UiContractTests(unittest.TestCase):
    """Task 5: mobile-first capture UI (three zones) + capabilities."""

    UI_SRC = ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h"

    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = DASH.read_text(encoding="utf-8")
        cls.src = cls.UI_SRC.read_text(encoding="utf-8")

    def test_capabilities_expose_recorder_v2(self) -> None:
        self.assertIn("recorderV2Available", self.dash,
                      "dashCapabilitiesJson carries the recorder v2 flag")
        self.assertIn("recorderV2Available", self.src,
                      "UI CAP defaults include recorderV2Available")
        # The hasCap branch: repo's capEnabled() must gate the new zone.
        cap_at = self.src.index("function capEnabled")
        self.assertIn(
            "recorderV2Available", self.src[cap_at:cap_at + 500],
            "capEnabled learns the recorderV2 capability")

    def test_ui_has_three_zones(self) -> None:
        for token in ("ring_status", "ring_lock", "/sessions",
                      "session_download", "session_delete"):
            self.assertIn(token, self.src, f"UI fetches {token}")
        self.assertIn('id="rec-v2-lock"', self.src, "big lock button id")
        self.assertIn('id="rec-v2-bus"', self.src, "bus toggle container id")
        # Touch target: the lock button must be a ≥64px tap target.
        lock_at = self.src.index('id="rec-v2-lock"')
        self.assertIn(
            "64px", self.src[lock_at:lock_at + 400],
            "lock button styles a 64px+ touch target")

    def test_ui_keeps_legacy_panel_folded(self) -> None:
        # The legacy recorder form survives untouched, folded away.
        self.assertIn('id="rec-start"', self.src, "legacy panel intact")
        self.assertIn('id="rec-advanced"', self.src, "fold container id")

    def test_i18n_new_labels_registered(self) -> None:
        keys = ("一键锁定", "回溯窗口", "录制会话", "下载CSV",
                "下载BIN", "删除会话", "高级设置")
        for k in keys:
            self.assertIn(f"'{k}':", self.src, f"{k} registered in I18N dict")
            self.assertIn(f'data-i18n="{k}"', self.src,
                          f"{k} used as a data-i18n label")

    def test_lock_button_polls_status(self) -> None:
        self.assertIn("setInterval(rv2Poll", self.src,
                      "recorder tab arms a 2s status poll")
        poll_at = self.src.index("function rv2Poll")
        self.assertIn(
            "ring_status", self.src[poll_at:poll_at + 600],
            "the poll fetches /ring_status (arming countdown needs it)")

    def test_lock_failure_reason_surfaced(self) -> None:
        # 2026-11-02 bench: a one-tap lock on a silent bus finishes with
        # last_error="no frames recorded" and NO session — the UI must
        # show the firmware's reason next to the lock button, or the
        # button looks dead (Jordan's bench report).
        poll_at = self.src.index("function rv2Poll")
        self.assertIn(
            "s.last_error", self.src[poll_at:poll_at + 1000],
            "the poll reads last_error from /ring_status")
        self.assertIn("RV2.lastErr=", self.src[poll_at:poll_at + 1000],
                      "the poll caches the reason for the note painter")
        paint_at = self.src.index("function rv2PaintBus")
        self.assertIn(
            "RV2.lastErr", self.src[paint_at:paint_at + 700],
            "the note painter shows the last lock error, escaped")
        self.assertIn("esc(RV2.lastErr)", self.src[paint_at:paint_at + 700],
                      "error text is HTML-escaped before display")
        self.assertIn("'上次锁定':'Last lock'", self.src,
                      "error label registered in I18N dict")


class DashRecorderV2DiagContractTests(unittest.TestCase):
    """Task 6: serial DIAG bench commands (board may be off-LAN)."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = DASH.read_text(encoding="utf-8")

    def test_rv2_status_command_registered(self) -> None:
        # The bench drives the lock chain over serial when the board is
        # unreachable on LAN: rv2_status prints the live status JSON.
        self.assertIn('"rv2_status"', self.dash,
                      "help lists rv2_status")
        self.assertIn('strcmp(start, "rv2_status")', self.dash,
                      "dispatcher routes rv2_status")
        route = self.dash.index('strcmp(start, "rv2_status")')
        self.assertIn(
            "dashRecV2StatusJson", self.dash[route:route + 200],
            "rv2_status prints the recorder v2 status JSON")

    def test_rv2_lock_command_parses_three_args(self) -> None:
        # rv2_lock <pre_s> <post_s> <bus_mask> arms the lock from serial.
        self.assertIn('strncmp(start, "rv2_lock", 8)', self.dash,
                      "dispatcher prefix-matches rv2_lock with args")
        self.assertIn("%u %u %u", self.dash,
                      "three unsigned args: pre_s post_s bus_mask")
        lock_at = self.dash.index('strncmp(start, "rv2_lock", 8)')
        self.assertIn(
            "dashRecV2LockRequest",
            self.dash[lock_at:lock_at + 700],
            "serial lock routes into the same request path as HTTP")

    def test_diag_rv2_commands_macro_gated(self) -> None:
        # Help + dispatch branches sit inside the DASH_RECORDER_V2 guard.
        gate = self.dash.count("#if defined(DASH_RECORDER_V2)")
        self.assertGreaterEqual(
            gate, 6,
            "hooks + init + handlers + serial help + serial dispatch gated")


class DashRecorderV2BenchBugTests(unittest.TestCase):
    """Task 6 bench finding: /sessions listed [] with seq=1 on flash.

    Root cause: the espidf File shim's openNextFile() fed full child paths
    into name_, so name() returned "/sess_1.meta.json" — Arduino semantics
    are basename. The scanner's strncmp(nm, "sess_", 5) missed the slash.
    Fixed at both layers: shim stores basename; scanner tolerates slashes.
    """

    RUNTIME_CPP = ROOT / "src" / "espidf_runtime.cpp"

    @classmethod
    def setUpClass(cls) -> None:
        cls.cpp = cls.RUNTIME_CPP.read_text(encoding="utf-8")
        cls.rec2 = REC2.read_text(encoding="utf-8")

    def test_shim_file_name_is_basename(self) -> None:
        ctor_at = self.cpp.index("File::File(const std::string &path")
        body = self.cpp[ctor_at:ctor_at + 500]
        self.assertIn(
            "rfind('/')", body,
            "File ctor stores the basename (Arduino name() semantics), "
            "not the full path with leading slash")

    def test_scanner_tolerates_leading_slash(self) -> None:
        scan_at = self.rec2.index("recV2ScanSessions")
        body = self.rec2[scan_at:scan_at + 900]
        self.assertIn(
            "while (*nm == '/')", body,
            "scanner skips leading slashes before the sess_ prefix match")


class DashRecorderV2UiBenchBugTests(unittest.TestCase):
    """Task 6 bench finding: session rows vanished after render.

    Root cause: the #rv2-sessions container carried data-i18n="暂无会话",
    so every applyI18n() pass reset its textContent — wiping the freshly
    rendered rows back to the empty-state label. Also: the list never
    reloaded when a lock finished flushing (idle edge).
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.src = (ROOT / "include" / "web" /
                   "mcp2515_dashboard_ui.src.h").read_text(encoding="utf-8")

    def test_sessions_container_has_no_i18n_attribute(self) -> None:
        # Dynamic labels (#rec-v2-lock countdown, #rv2-state machine text,
        # #rv2-sessions rows) are written through T() at set time; a
        # data-i18n attribute on them would let applyI18n() reset the
        # node to its static default on every pass.
        for el_id in ("rv2-sessions", "rec-v2-lock", "rv2-state"):
            at = self.src.index(f'id="{el_id}"')
            tag_start = self.src.rfind("<", 0, at)
            tag = self.src[tag_start:self.src.index(">", at) + 1]
            self.assertNotIn(
                "data-i18n", tag,
                f"#{el_id} is a dynamic label — applyI18n() would clobber it")

    def test_flush_completion_edge_reloads_list(self) -> None:
        poll_at = self.src.index("function rv2Poll")
        body = self.src[poll_at:poll_at + 1200]
        self.assertIn(
            "lastState", body,
            "rv2Poll tracks the previous lock state")
        self.assertRegex(
            body, r"lastState[^;]*!=='idle'",
            "transition back to idle (flush just wrote a session) "
            "reloads the session list once")

    def test_language_toggle_repolls_dynamic_labels(self) -> None:
        # Dynamic labels translate at set time (T() inside rv2Poll/paint),
        # so a language switch must immediately re-run the recorder poll —
        # otherwise the countdown/state stay in the old language for 2s.
        toggle_at = self.src.index("function toggleLanguage")
        body = self.src[toggle_at:toggle_at + 600]
        self.assertIn(
            "rv2Poll", body,
            "toggleLanguage re-polls recorder v2 dynamic labels")


class DashRecorderV2FinalReviewTests(unittest.TestCase):
    """Final whole-branch review fixes (fable reviewer, 2026-11-01).

    Finding 1 (Important): loss_badge was `(overflowDelta > 0) ||
    (windowUs > liveSpanUs())` evaluated AFTER the bin write — once the ring
    reached steady-state full every submit evicts, so every session lit the
    badge (D7 says evictions past the window are harmless), and the live span
    read post-write shrank further during multi-second SPIFFS writes. Boot
    truncation (window > young ring, zero evictions) also lit it.

    Fix: badge = evictions-since-lock AND window beyond the retention the
    SCAN snapshot saw. Loss ⇒ both; boot truncation ⇒ delta 0 ⇒ quiet.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.rec2 = REC2.read_text(encoding="utf-8")
        cls.src = (ROOT / "include" / "web" /
                   "mcp2515_dashboard_ui.src.h").read_text(encoding="utf-8")

    def test_loss_badge_is_and_of_eviction_delta_and_span(self) -> None:
        # Blackbox era: recBBFinishLock computes the badge from block
        # summaries — BOTH an actual drop since lock AND a window reaching
        # past the retention the summaries saw; OR would be always-true.
        self.assertIn(
            "(overflowNow > recV2OverflowAtLock) &&", self.rec2,
            "badge needs BOTH an actual drop since lock AND a window "
            "reaching past the retention the summaries saw")
        self.assertIn(
            "(windowUs > (uint32_t)(lastTs - oldestTs))", self.rec2)
        self.assertNotIn(
            "(overflowNow > recV2OverflowAtLock) ||", self.rec2,
            "the pre-fix OR form must be gone")

    def test_flush_reads_badge_inputs_from_scan_snapshot(self) -> None:
        # Blackbox era: recBBFinishLock walks the block summaries — the
        # retention set as the writer sealed it — and never reads a live
        # span that the ingest path is still advancing.
        finish_at = self.rec2.index("static void recBBFinishLock()")
        body = self.rec2[finish_at:finish_at + 5600]
        self.assertIn("recBBSums[", body,
                      "badge and frame counts come from the block summaries")
        self.assertIn("(overflowNow > recV2OverflowAtLock) &&", body,
                      "overflow delta read from the frozen snapshot")
        self.assertIn("(lastTs - oldestTs)", body,
                      "span derived from the summaries' oldest/newest")
        self.assertNotIn(
            "dashRecV2SpanUs()", body,
            "finish must not read the live span — the ingest path keeps "
            "advancing the grid while the meta is written")

    def test_legacy_fold_defaults_open_and_gates_once(self) -> None:
        # Finding 2 (Important): the <details> fold changed the legacy
        # recorder layout on T2CAN too, against the spec's zero-behavior-
        # change anti-goal. Default `open` restores the flat layout on
        # every board; only P4 (recorderV2 capability) closes it, once —
        # a forced re-close every 2s poll would fight the user's click.
        self.assertRegex(
            self.src, r'id="rec-advanced"[^>]*\bopen\b',
            "the legacy fold ships open so non-P4 boards keep the flat "
            "layout")
        self.assertIn(
            "recAdv.open=!capEnabled('recorderV2')", self.src,
            "P4 closes the fold via the capability, not the static HTML")
        self.assertIn(
            "recAdv.dataset.gated='1'", self.src,
            "the fold is gated once — later polls must not re-close a "
            "details the user just opened")


class DashRecorderV2CarCrashReproTests(unittest.TestCase):
    """Field finding 2026-11-01: in-car lock → flush-phase reboot, 6 orphan
    bins (one torn mid-batch, 88101 % 18 == 9), board unstable while driving.
    Prime suspects: (a) brownout under sustained erase-heavy SPIFFS burst,
    (b) TWDT/panic in the never-bench-tested multi-MB flush regime.

    The bench has no CAN traffic source, so the big-flush regime could never
    be exercised. rv2_flood injects paced synthetic frames through the real
    submit path to reproduce the car's flush load on the bench.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.rec2 = REC2.read_text(encoding="utf-8")
        cls.dash = DASH.read_text(encoding="utf-8")

    def test_flood_injector_exists_and_paces_batches(self) -> None:
        # Paced injection (batched per 10ms tick), not a tight busy loop:
        # a tight loop would starve the sender task itself and distort the
        # repro (and the fps counters would lie).
        self.assertIn("dashRecV2BenchFlood", self.rec2,
                      "bench-only flood injector lives in the engine")
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(10))", self.rec2,
                      "injection is paced in 10ms batches")
        self.assertIn("dashRecV2Submit(f,", self.rec2,
                      "flood rides the REAL submit path (ring + fps + lock)")

    def test_flood_is_serial_dispatched_and_gated(self) -> None:
        self.assertIn('strncmp(start, "rv2_flood", 9) == 0', self.dash,
                      "DIAG dispatcher routes rv2_flood")
        self.assertIn("dashRecV2BenchFlood(", self.dash,
                      "dispatcher calls the injector")
        self.assertGreaterEqual(
            self.dash.count("DASH_RECORDER_V2"), 4,
            "flood branch stays macro-gated like every other v2 hook")

    def test_flood_spreads_buses_by_mask(self) -> None:
        # Injection must honor the bus mask so a flush's bus_mask filtering
        # sees the same frame mix the car produces.
        self.assertIn("buses[sent % n]", self.rec2,
                      "frames round-robin across the masked buses")


class DashRecorderV2CarFieldFixTests(unittest.TestCase):
    """Fixes from the 2026-11-01 in-car lock-crash forensics, session 2.

    Bench discrimination twice over: 2000fps x 300s flood + lock + 9.6MB
    flush survived clean (uptime continuous, no panic/WDT/brownout, HTTP
    alive throughout, bin streams at 444KB/s) — the car crashes point at
    supply collapse under the flash-write burst, not at the flush code.
    These contracts carry the field fixes that follow from that verdict:

    1. sessions size truth   — /sessions size was always 0 (stat() at the
       VFS root misses the /spiffs mount prefix; seq 22 proved it: 9.6MB
       bin reported as size 0).
    2. boot orphan sweep     — five bins (seq 7-12) left orphaned by the
       in-car crashes leak space forever; meta is the source of truth.
    3. crash forensics       — NVS ledger of the last 8 reset reasons plus
       a died-mid-flush flag: the next car run classifies every reboot
       (BROWNOUT vs TASK_WDT vs PANIC) with no serial attached.
    4. coredump to flash     — the partition exists (0x1FE0000); enable it
       so any panic leaves a retrievable backtrace.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.rec2 = REC2.read_text(encoding="utf-8") if REC2.exists() else ""
        cls.dash = DASH.read_text(encoding="utf-8")
        cls.shim_cpp = (ROOT / "src" / "espidf_runtime.cpp").read_text(
            encoding="utf-8")
        cls.shim_h = (
            ROOT / "include" / "platform" / "espidf_runtime.h"
        ).read_text(encoding="utf-8")
        cls.sdkcfg = (ROOT / "sdkconfig.p4_3can").read_text(encoding="utf-8")
        cls.parts = (
            ROOT / "partitions_32mb_ota_8192k_nvs64.csv"
        ).read_text(encoding="utf-8")

    # --- fix 1: sessions size truth ---------------------------------------

    def test_sessions_size_uses_shim_file_size(self) -> None:
        # Blackbox era: a session's size is its grid byte range from the
        # meta (bb_end_off - bb_start_off) — no file is stat'd at all; the
        # VFS-root stat() bug class is deleted with the bin era.
        self.assertNotIn("recV2FileSize", self.rec2,
                         "the file-size query died with the bin files")
        list_at = self.rec2.index("static String dashRecV2SessionsJson()")
        list_body = self.rec2[list_at:list_at + 2600]
        self.assertIn('"bb_start_off"', list_body,
                      "sessions list reads the grid range from each meta")
        self.assertIn('"bb_end_off"', list_body)

    def test_shim_file_size_prepends_mount_base(self) -> None:
        # exists() prepends kSpiffsBase before stat; fileSize must do the
        # same or it inherits the ENOENT bug.
        self.assertIn("size_t SPIFFSClass::fileSize", self.shim_cpp,
                      "shim implements fileSize")
        body = self.shim_cpp.split("size_t SPIFFSClass::fileSize", 1)[1][:400]
        self.assertIn("kSpiffsBase", body,
                      "fileSize stat()s under the SPIFFS mount prefix")
        self.assertIn("st.st_size", body,
                      "fileSize returns the stat size")

    def test_shim_declares_file_size(self) -> None:
        self.assertRegex(
            self.shim_h, r"size_t fileSize\(const String &path\)",
            "SPIFFSClass declares fileSize next to exists/remove")

    # --- fix 2: boot orphan sweep ------------------------------------------
    # Removed in 6.0.0-beta11: the bin era ended with the blackbox grid —
    # no flush ever writes a bin again, so there is nothing to sweep. (The
    # five field-crash orphans died with the spiffs reformat the beta11
    # partition-table change forces on first flash.) The no-dead-symbols
    # contract now lives in test_blackbox_recorder.DeadCodeRemovalTests.

    # --- fix 3: crash forensics ledger --------------------------------------

    def test_ledger_records_boot_reset_reason(self) -> None:
        # esp_reset_reason() at boot says why the PREVIOUS session ended
        # (BROWNOUT / TASK_WDT / PANIC ...). 8 rotating slots; the reason
        # survives power cycles, so the next car run needs no serial.
        self.assertIn("recV2LedgerBoot", self.rec2,
                      "engine records boot reset reasons")
        init_body = self.rec2.split("static void dashRecV2Init()", 1)[1][:2800]
        self.assertIn("recV2LedgerBoot()", init_body,
                      "init feeds the ledger at every boot")
        ledger_pos = self.rec2.find("recV2LedgerBoot")
        ledger_body = self.rec2[ledger_pos:ledger_pos + 1400]
        self.assertIn("esp_reset_reason()", ledger_body,
                      "ledger stores the IDF reset reason code")
        self.assertIn("rv2_resets", ledger_body,
                      "ledger persists under the rec_v2 NVS namespace")

    def test_flush_marks_died_mid_flush(self) -> None:
        # Arming-era semantics (blackbox, task 4): raised at lock request
        # (arming start), cleared inside recBBFinishLock — a boot that reads
        # flag==1 died inside an arming window. The boot ledger clears the
        # flag too, after reading it (scope counts to the live functions).
        lock_at = self.rec2.index("static bool dashRecV2LockRequest(")
        lock_body = self.rec2[lock_at:lock_at + 1400]
        self.assertEqual(lock_body.count('putUChar("rv2_flushing", 1)'), 1,
                         "arming start raises the flag exactly once")
        finish_at = self.rec2.index("static void recBBFinishLock()")
        finish_body = self.rec2[finish_at:finish_at + 2900]
        self.assertEqual(finish_body.count('putUChar("rv2_flushing", 0)'), 1,
                         "the finish path clears the flag")

    def test_ring_status_carries_resets(self) -> None:
        # /ring_status is the recorder's own endpoint; the UI and any
        # browser-based field check read the ledger from there.
        self.assertIn('"resets"', self.rec2,
                      "status JSON exposes the reset ledger")

    def test_diag_rv2_resets_registered(self) -> None:
        self.assertIn('strncmp(start, "rv2_resets", 10) == 0', self.dash,
                      "DIAG dispatcher routes rv2_resets")
        self.assertIn("rv2_resets", self.dash,
                      "DIAG help mentions the command")

    # --- fix 4: coredump to flash -------------------------------------------

    def test_sdkconfig_coredump_to_flash(self) -> None:
        self.assertIn("CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y", self.sdkcfg,
                      "coredumps persist to the flash partition")
        self.assertNotIn("CONFIG_ESP_COREDUMP_ENABLE_TO_NONE=y", self.sdkcfg,
                         "coredump-to-none must be off")

    def test_partition_table_keeps_coredump(self) -> None:
        self.assertIn("coredump", self.parts,
                      "the coredump partition row stays in the table")


class DashRecorderV2SpacePressureTests(unittest.TestCase):
    """Round-4 bench finding (2026-11-01): lock during traffic with only
    2.8MB free → shrink left a 128KB margin → the flush crawled into deep
    SPIFFS GC (1.15KB/s), CPU1 starved (task-wdt prints on IDLE1, HTTP
    timeouts climbed past 6s). Round 3 with a roomy FS wrote 9.6MB at a
    steady 8KB/s with zero starvation, so the reserve margin is the
    discriminator.

    The reserve/pacing machinery itself died with the flush engine in
    6.0.0-beta11 (blackbox grid: one 64KB partition write per staging
    buffer, writer paced by its own 250ms tick — no multi-MB SPIFFS flush
    exists to starve anything). The crash-dump prerequisites it motivated
    survive below.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.rec2 = REC2.read_text(encoding="utf-8") if REC2.exists() else ""
        cls.sdkcfg = (ROOT / "sdkconfig.p4_3can").read_text(encoding="utf-8")

    def test_frame_pointer_enabled_for_backtraces(self) -> None:
        # The round-4 watchdog printout asked for it verbatim; without frame
        # pointers a panic or watchdog leaves registers but no stack.
        self.assertIn("CONFIG_ESP_SYSTEM_USE_FRAME_POINTER=y", self.sdkcfg,
                      "crash dumps must carry a real backtrace")
