import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
D = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(encoding="utf-8")
H = (ROOT / "include" / "dash_epas_nag.h").read_text(encoding="utf-8")
T = (ROOT / "include" / "dash_epas_nag_trace.h").read_text(encoding="utf-8")


class TraceStructContract(unittest.TestCase):
    def test_ring_struct_present(self):
        self.assertIn("struct EpasNagTraceRing", T)

    def test_ring_uses_shared_head_not_volatile(self):
        # Concurrency: must use Shared<T> (FreeRTOS atomic), not bare volatile.
        self.assertIn("Shared<uint32_t> head", T)
        self.assertNotIn("volatile", T)

    def test_ring_capacity_2048(self):
        self.assertIn("kCap = 2048", T)

    def test_entry_has_decision_and_torque_fields(self):
        self.assertIn("decision", T)
        self.assertIn("realTorque", T)
        self.assertIn("echoTorque", T)
        self.assertIn("twaiTec", T)
        self.assertIn("swTxErr", T)
        self.assertIn("framesSent", T)


class EngineDiagContract(unittest.TestCase):
    def test_diag_records_real_and_echo_torque(self):
        # Decisive evidence: the engine must snapshot the real 0x370 torque
        # and the torque our echo used, side by side.
        self.assertIn("lastRealTorque", H)
        self.assertIn("lastRealCnt", H)
        self.assertIn("lastEchoTorque", H)


class StatusJsonContract(unittest.TestCase):
    def test_status_has_epasNagDiag_object(self):
        # Source is C++ with escaped quotes: j += ",\"epasNagDiag\":{...}".
        self.assertIn('\\"epasNagDiag\\":', D)

    def test_status_has_real370_echo370(self):
        self.assertIn('\\"real370\\":{', D)
        self.assertIn('\\"echo370\\":{', D)

    def test_status_uses_twai_tec_not_mcpEflg(self):
        # Single-CAN board: mcpEflg is static const 0 (meaningless).
        # Bus health must come from the TWAI tx error counter + software count.
        self.assertIn('\\"twaiTec\\":', D)
        self.assertIn('\\"swTxErr\\":', D)


class TraceCaptureContract(unittest.TestCase):
    def test_trace_route_registered(self):
        self.assertIn('"/epas_nag_trace"', D)

    def test_trace_csv_header_fixed(self):
        # Flush must write a stable CSV header for offline analysis.
        self.assertIn("ts_ms,decision,gate,handsOn,dasHandsOn", D)

    def test_trace_csv_header_has_no_eflg(self):
        # Hard constraint: never carry the meaningless single-CAN mcpEflg.
        hdr = D.find("ts_ms,decision,gate,handsOn,dasHandsOn")
        self.assertGreaterEqual(hdr, 0)
        snippet = D[hdr : hdr + 260]
        self.assertNotIn("eflg", snippet)

    def test_serial_command_present(self):
        self.assertIn('strcmp(start, "epas_nag_trace")', D)


if __name__ == "__main__":
    unittest.main()
