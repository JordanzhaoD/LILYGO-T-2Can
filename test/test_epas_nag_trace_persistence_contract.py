import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
D = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(encoding="utf-8")


class PeriodicFlushContract(unittest.TestCase):
    """Root cause A2 fix: flush must not depend solely on the onFrame stop
    check (which never runs once frames stop arriving). The ring must flush
    periodically while active so a sudden power loss (unplugging USB after a
    drive) still leaves recoverable data on SPIFFS."""

    def test_last_flush_ms_global_exists(self):
        self.assertIn("dashEpasNagLastFlushMs", D)

    def test_poll_flushes_periodically_while_active(self):
        # Match the DEFINITION (body opens with "{"), not the forward
        # declaration at line ~221 which ends with ";".
        idx = D.find("static void dashEpasNagTracePoll()\n{")
        self.assertGreater(idx, 0)
        body = D[idx:idx + 700]
        self.assertIn("dashEpasNagTrace.active", body)
        self.assertIn("5000", body)  # 5 s periodic flush interval
        self.assertIn("dashEpasNagLastFlushMs", body)


class StopDeadlockContract(unittest.TestCase):
    """Root cause A2 fix: the can-offline stop check must not live only
    inside dashEpasNagOnFrame (it never fires once frames stop). The loop
    task must stop the trace using the real bus-quiet signal (canOnline)."""

    def test_loop_stops_trace_when_bus_quiet(self):
        idx = D.find("static void mcpDashboardLoop()")
        self.assertGreater(idx, 0)
        body = D[idx:idx + 1400]
        self.assertIn("dashEpasNagTraceStop", body)
        self.assertIn("canOnline", body)


class SerialReadSpiffsContract(unittest.TestCase):
    """Root cause A1 fix: opening USB-CDC resets the board and clears the RAM
    ring. The serial command must fall back to reading the persisted SPIFFS
    CSV so the last drive's data is recoverable after a reset."""

    def test_serial_trace_reads_spiffs_csv_when_ring_empty(self):
        idx = D.find("static void dashSerialPrintEpasNagTrace()")
        self.assertGreater(idx, 0)
        body = D[idx:idx + 1600]
        self.assertIn('SPIFFS.open("/epas_nag_trace.csv"', body)
        # Must be a read ("r"), not the flush write ("w").
        w_idx = body.find('SPIFFS.open("/epas_nag_trace.csv"')
        snippet = body[w_idx:w_idx + 48]
        self.assertIn('"r"', snippet)


if __name__ == "__main__":
    unittest.main()
