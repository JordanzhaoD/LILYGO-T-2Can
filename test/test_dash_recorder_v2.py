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
        self.assertIn("recV2Writing", self.rec2,
                      "single-writer flag exists")
        # LockRequest must refuse while arming or flushing.
        self.assertIn("dashRecV2LockRequest", self.rec2)
        lock_start = self.rec2.index("dashRecV2LockRequest")
        guard_region = self.rec2[lock_start:lock_start + 700]
        self.assertIn("busy", guard_region,
                      "lock request refuses while a flush is in flight")
        self.assertIn("409", self.dash,
                      "handler answers 409 on the busy path")

    def test_flush_failure_cleans_partial_file(self) -> None:
        # A half-written session must never masquerade as a complete one:
        # the write-failure branch itself must scrub the partial file.
        write_fail = self.rec2.find("write failed")
        self.assertGreaterEqual(
            write_fail, 0, "flush names its write-failure branch")
        branch = self.rec2[write_fail:write_fail + 300]
        self.assertIn("SPIFFS.remove", branch,
                      "partial file is removed on the write-failure path")
        self.assertIn("last_error", self.rec2,
                      "failure surfaces in status as last_error")

    def test_shrink_keeps_recent(self) -> None:
        self.assertIn("shrink_reason", self.rec2)
        self.assertIn("recV2CountWindow", self.rec2,
                      "pre-scan counts matching frames before copying")
        self.assertIn("- 128 * 1024", self.rec2,
                      "128KB head-room reserve on SPIFFS free space")
        self.assertIn("pre_ms_req", self.rec2)
        self.assertIn("pre_ms_act", self.rec2,
                      "meta distinguishes requested vs actual window")

    def test_seq_nvs_monotonic(self) -> None:
        self.assertIn("rv2_seq", self.rec2)
        self.assertIn("putUInt", self.rec2)
        self.assertIn("getUInt", self.rec2)

    def test_wraparound_window_extraction(self) -> None:
        # The flush window test must be wrap-safe: age = (uint32_t)(now - ts).
        self.assertRegex(self.rec2, r"\(\s*uint32_t\s*\)\s*\(\s*now[A-Za-z]* - ")

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
        for token in ('"seq"', '"boot_ms"', '"trigger"', '"post_ms"',
                      '"frames"', '"ring_overflow"',
                      '"ring_overflow_in_window"', '"twai_missed_a"'):
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
        self.assertIn(
            "size % sizeof(RecRingFrame)", self.dash,
            "download refuses files whose size is not a whole record count")
        self.assertIn(
            "corrupt", self.dash,
            "the alignment failure names the problem")

    def test_csv_header_exact(self) -> None:
        self.assertIn(
            "ts_us,bus,dir,id,dlc,b0,b1,b2,b3,b4,b5,b6,b7", self.dash,
            "CSV header is frozen (tools parse it positionally)")

    def test_delete_removes_bin_and_meta(self) -> None:
        self.assertIn(
            "recV2RemoveSession", self.dash,
            "delete handler routes through the both-files remover")
        helper_at = self.rec2.index("recV2RemoveSession")
        helper = self.rec2[helper_at:helper_at + 220]
        self.assertGreaterEqual(
            helper.count("SPIFFS.remove"), 2,
            "the remover drops meta AND bin")

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


if __name__ == "__main__":
    unittest.main()
