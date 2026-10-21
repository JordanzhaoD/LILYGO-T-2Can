import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
D = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(encoding="utf-8")
U = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text(encoding="utf-8")
M = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
S = (ROOT / "scripts" / "webui_simulator.py").read_text(encoding="utf-8")


class RecStreamingContract(unittest.TestCase):
    def test_recorder_has_no_fixed_frame_cap(self):
        self.assertNotIn("#define REC_CAP", D)
        self.assertIn("kRecDefaultDurationMs = 120000UL", D)

    def test_can_producer_is_nonblocking(self):
        idx = D.find("static void dashRecordCanFrame")
        self.assertGreater(idx, 0)
        body = D[idx:idx + 1800]
        self.assertIn("xQueueSend(recQueue, &record, 0)", body)
        self.assertIn("recDropped", body)
        self.assertNotIn("SPIFFS", body)

    def test_low_priority_writer_streams_binary(self):
        self.assertIn("dashRecorderWriterTask", D)
        self.assertIn('SPIFFS.open("/rec.bin", "w")', D)
        self.assertIn("kRecQueueDepth = 2048", D)
        self.assertIn("sizeof(RecFrame) == 19", D)
        self.assertIn("xQueueCreateStatic", D)
        self.assertIn("MALLOC_CAP_SPIRAM", D)
        self.assertIn("MALLOC_CAP_INTERNAL", D)

    def test_secondary_rx_is_not_recorded_twice(self):
        idx = M.find("static void t2canDrainSecondary")
        self.assertGreater(idx, 0)
        body = M[idx:idx + 1800]
        self.assertNotIn("dashRecordCanFrame(f, 'R')", body)
        self.assertIn("appActiveHandler->handleMessage", body)


class RecSaveReliabilityContract(unittest.TestCase):
    def test_start_removes_old_recording_before_open(self):
        idx = D.find("static void handleRecStart")
        self.assertGreater(idx, 0)
        body = D[idx:idx + 5000]
        self.assertIn('SPIFFS.remove("/rec.bin")', body)
        self.assertIn('SPIFFS.open("/rec.bin", "w")', body)

    def test_download_converts_binary_to_chunked_csv(self):
        idx = D.find("static void handleRecDownload")
        self.assertGreater(idx, 0)
        body = D[idx:idx + 3500]
        self.assertIn("beginChunkedResponse", body)
        self.assertIn("sendChunk", body)
        self.assertIn("ts_ms,dir,bus,id,dlc", body)

    def test_status_exposes_truncation_signals(self):
        for token in ('"dropped"', '"storage_full"', '"file_bytes"', '"stop_reason"'):
            self.assertIn(token.replace('"', '\\\"'), D)
        self.assertIn("queue_psram", D)

    def test_recorder_exports_start_and_end_nag_diagnostics(self):
        self.assertIn("DashRecNagSnapshot", D)
        self.assertIn("dashCaptureRecNagSnapshot(recNagStart)", D)
        self.assertIn("dashCaptureRecNagSnapshot(recNagEnd)", D)
        self.assertIn('server.on("/rec_meta", HTTP_GET, handleRecMetaDownload);', D)
        self.assertIn("downloadRecMeta", U)

    def test_recorder_exports_can2_native_3c2_evidence(self):
        for token in (
            "recCan2Rx3c2",
            "recCan2Rx3c2Mux1",
            "recCan2Rx3c2Mux1Idle",
            "recCan2Rx3c2VolumeUp",
            "recCan2Rx3c2VolumeDown",
            "recCan2Tx3c2",
            "recCan1Rx3c2",
            "recCan1Tx3c2",
            "recCan1TxError3c2",
            '"wheel_3c2"',
            '"can2_native_visible"',
        ):
            self.assertIn(token.replace('"', '\\"'), D)

    def test_default_dnd_filter_contains_wheel_and_warning_context(self):
        expected = "045,118,129,186,238,292,370,389,399,3C2,3EE,3FD,47D,47F,488,5D9"
        self.assertIn(f'value="{expected}"', U)
        self.assertIn('id="rec-3c2-can1"', U)
        self.assertIn('id="rec-3c2-can2"', U)
        for can_id in ("0x045", "0x238", "0x292", "0x3C2", "0x3FD", "0x47D", "0x47F", "0x488"):
            self.assertIn(can_id, D)

    def test_legacy_045_runtime_tx_and_observation_are_exposed(self):
        for token in (
            "recCan1Rx045", "recCan2Rx045", "rec045LeverUp", "rec045LeverDown",
            "rec045CondUp", "rec045CondDown", '"wheel_045"', '"receive_only":false',
            '"protocol":"legacy_0x045_left_scroll"',
        ):
            self.assertIn(token.replace('"', '\\"'), D)
        self.assertIn("f.id = DashLegacyWheel045Dnd::kCanId", M)
        self.assertIn("appDriver->send(f)", M)
        self.assertIn('id="rec-045-can1"', U)
        self.assertIn('id="rec-045-can2"', U)

    def test_invalid_or_excessive_filters_are_rejected(self):
        self.assertIn("invalid hexadecimal CAN ID filter", D)
        self.assertIn("at most 16 filter IDs are allowed", D)

    def test_elapsed_time_freezes_when_recording_stops(self):
        self.assertIn("recElapsedMs = millis() - recStartMs", D)
        self.assertIn("recActive || recFinalizing", D)

    def test_simulator_stop_is_idempotent(self):
        idx = S.find('elif path == "/rec_stop":')
        self.assertGreater(idx, 0)
        body = S[idx:idx + 600]
        self.assertIn("if STATE.rec_active:", body)
        self.assertIn("STATE.rec_active = False", body)


class RecUiFeedbackContract(unittest.TestCase):
    """The UI must expose finalization and loss rather than claiming success."""

    def test_stoprec_shows_saving_progress(self):
        idx = U.find("async function stopRec()")
        self.assertGreater(idx, 0)
        body = U[idx:idx + 450]
        self.assertTrue("收尾中" in body or "Finalizing" in body)

    def test_ui_defaults_to_120_second_nag_capture(self):
        self.assertIn('id="rec-duration"', U)
        # 5.0.0-beta04 task 8c: option labels carry data-i18n — attribute-tolerant match
        self.assertRegex(U, r'<option value="120" selected[^>]*>')
        self.assertIn('value="045,118,129,186,238,292,370,389,399,3C2,3EE,3FD,47D,47F,488,5D9"', U)
        self.assertIn("pollRecStatus", U)


if __name__ == "__main__":
    unittest.main()
