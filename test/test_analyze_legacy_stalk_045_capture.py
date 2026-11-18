import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "analyze_legacy_stalk_045_capture.py"
SPEC = importlib.util.spec_from_file_location("analyze_legacy_stalk_045_capture", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)


class LegacyStalk045AnalyzerTest(unittest.TestCase):
    def _write(self, path, frames):
        with path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(["ts_ms", "dir", "bus", "id", "dlc", *[f"b{i}" for i in range(8)]])
            for frame in frames:
                writer.writerow(frame)

    def test_byte0_distribution_and_gesture_detection(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "stalk_capture.csv"
            # idle 0x40 x3, FWD 0x41, UP_1ST 0x50, DN_1ST 0x60
            frames = [
                [0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xC0],
                [0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xD0],
                [0x40, 0x21, 0x00, 0x30, 0x00, 0x00, 0xE0],
                [0x41, 0x21, 0x00, 0x30, 0x00, 0x00, 0xF0],
                [0x50, 0x21, 0x00, 0x30, 0x00, 0x00, 0x00],
                [0x60, 0x21, 0x00, 0x30, 0x00, 0x00, 0x10],
            ]
            rows = []
            for i, d in enumerate(frames):
                d.append(MODULE.crc8_j1850(d[:7]))
                rows.append([i * 10, "R", 1, 0x045, 8, *d])
            self._write(path, rows)
            report = MODULE.analyze(path)
        self.assertEqual(report["native_045_frames"], 6)
        self.assertEqual(report["crc_valid"], 6)
        # byte0 distribution: idle x3, FWD x1, UP_1ST x1, DN_1ST x1
        dist = report["byte0_distribution_045"]
        self.assertEqual(dist.get("idle(base)"), 3)
        self.assertEqual(dist.get("FWD(cancel_AP)"), 1)
        self.assertEqual(dist.get("UP_1ST"), 1)
        self.assertEqual(dist.get("DN_1ST"), 1)
        # gestures = FWD + UP_1ST + DN_1ST = 3
        self.assertEqual(report["gesture_frames"], 3)
        self.assertEqual(report["expected_values_seen"]["UP_1ST"], 1)
        self.assertEqual(report["expected_values_seen"]["DN_1ST"], 1)

    def test_gesture_transience_bursts(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "burst.csv"
            # idle 0..40ms, UP_1ST 50..90ms (burst), idle 100..130ms
            frames = [
                (0, 0x40, 0xC0),
                (10, 0x40, 0xD0),
                (40, 0x40, 0xE0),
                (50, 0x50, 0xF0),  # gesture start
                (70, 0x50, 0x00),
                (90, 0x50, 0x10),  # gesture end
                (100, 0x40, 0x20),
                (130, 0x40, 0x30),
            ]
            rows = []
            for ts, b0, counter in frames:
                d = [b0, 0x21, 0x00, 0x30, 0x00, 0x00, counter]
                d.append(MODULE.crc8_j1850(d[:7]))
                rows.append([ts, "R", 1, 0x045, 8, *d])
            self._write(path, rows)
            report = MODULE.analyze(path)
        bursts = report["gesture_bursts"]
        self.assertEqual(len(bursts), 1)
        self.assertEqual(bursts[0]["duration_ms"], 50)  # 100 - 50

    def test_unknown_byte0_value_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "unknown.csv"
            d = [0x63, 0x21, 0x00, 0x30, 0x00, 0x00, 0xC0]  # 0x63 = low6 0x23
            d.append(MODULE.crc8_j1850(d[:7]))
            self._write(path, [[0, "R", 1, 0x045, 8, *d]])
            report = MODULE.analyze(path)
        # 0x63 not in the known table -> reported with decoded low6/VSL
        self.assertTrue(any("0x63" in k for k in report["byte0_distribution_045"]))


if __name__ == "__main__":
    unittest.main()
