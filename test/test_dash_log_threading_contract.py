"""Contract tests for the dashboard log ring serialization.

Field forensics 2026-11-02 (board 2, in-car): 7x ESP_RST_PANIC with the
crashing task can_rt mid-``dashLog`` (OTA-guard stale-clear log line). The
log ring (logBuf / logHead / logCount / logSeq) is mutated from multiple
RTOS tasks — can_rt (per-frame edges, OTA guard tick) and web (WIFI/AP logs
from dashCheckWifi, HTTP handlers) — with zero serialization. Two tasks
assigning ``logBuf[logHead] = {...}`` can both free the old entry's String
heap chunk: double free -> heap corruption -> random panics plus
esp_hosted/lwip faults (more AP/STA drops -> more logs -> feedback loop).

These tests pin the fix: every logBuf access must be serialized by a
FreeRTOS mutex (not a spinlock — the critical section allocates), created
before the scheduler starts (static initializer, no lazy-init race).
"""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DASH_FILE = ROOT / "include" / "web" / "mcp2515_dashboard.h"


def _body(source: str, start_marker: str, end_marker: str) -> str:
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


class DashLogThreadingContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dash = DASH_FILE.read_text(encoding="utf-8")
        cls.dash_log = _body(
            cls.dash, "static void dashLog(const String &s)\n{", "// Pull all new entries"
        )
        cls.drain = _body(
            cls.dash, "static void dashDrainLogRing()", "static void handleLog()"
        )
        cls.handle_log = _body(
            cls.dash, "static void handleLog()", "static void handleResetStats()"
        )

    def test_dash_log_takes_mutex_before_touching_shared_ring(self) -> None:
        take = self.dash_log.find("xSemaphoreTake(dashLogMux")
        assign = self.dash_log.find("logBuf[logHead] =")
        self.assertGreaterEqual(
            take, 0, "dashLog must take dashLogMux (field crash 2026-11-02)"
        )
        self.assertGreater(
            assign, take, "logBuf assignment must happen with the mutex held"
        )
        give = self.dash_log.find("xSemaphoreGive(dashLogMux")
        self.assertGreater(
            give, assign, "dashLog must release the mutex after the ring update"
        )

    def test_dash_log_critical_section_spans_head_and_count(self) -> None:
        take = self.dash_log.index("xSemaphoreTake(dashLogMux")
        give = self.dash_log.index("xSemaphoreGive(dashLogMux")
        window = self.dash_log[take:give]
        self.assertIn("logHead = (logHead + 1)", window)
        self.assertIn("logCount", window)
        self.assertIn("++logSeq", window)

    def test_dash_log_keeps_serial_print_outside_the_lock(self) -> None:
        give = self.dash_log.index("xSemaphoreGive(dashLogMux")
        window = self.dash_log[give:]
        self.assertIn(
            "Serial.println",
            window,
            "USB println can block; keep it outside the mutex window",
        )

    def test_dash_log_does_not_use_spinlock_around_heap_ops(self) -> None:
        self.assertNotIn(
            "portENTER_CRITICAL",
            self.dash_log,
            "String assign/free inside a spinlock is illegal on multicore IDF",
        )

    def test_drain_path_serializes_its_ring_writes(self) -> None:
        take = self.drain.find("xSemaphoreTake(dashLogMux")
        self.assertGreaterEqual(
            take, 0, "dashDrainLogRing writes logBuf and must hold dashLogMux"
        )
        give = self.drain.find("xSemaphoreGive(dashLogMux", take)
        self.assertGreater(give, take)
        window = self.drain[take:give]
        self.assertIn("logBuf[logHead]", window)
        self.assertIn("logHead = (logHead + 1)", window)

    def test_log_read_endpoint_serializes_its_ring_reads(self) -> None:
        take = self.handle_log.find("xSemaphoreTake(dashLogMux")
        self.assertGreaterEqual(
            take, 0, "handleLog reads logBuf/logHead while writers run"
        )
        give = self.handle_log.find("xSemaphoreGive(dashLogMux", take)
        self.assertGreater(give, take)
        window = self.handle_log[take:give]
        self.assertIn("logBuf[idx]", window)

    def test_mutex_created_pre_scheduler_via_static_initializer(self) -> None:
        self.assertIn(
            "xSemaphoreCreateMutex()",
            self.dash,
            "dashLogMux must be created by a static initializer (runs before "
            "app_main/scheduler, so no lazy-init race)",
        )
        self.assertIn("static SemaphoreHandle_t dashLogMux", self.dash)


if __name__ == "__main__":
    unittest.main()
