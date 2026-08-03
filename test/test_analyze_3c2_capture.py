import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "analyze_3c2_capture.py"
SPEC = importlib.util.spec_from_file_location("analyze_3c2_capture", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)


class Analyze3c2CaptureTest(unittest.TestCase):
    def test_detects_native_mux1_idle_manual_actions_and_tx_sequence(self):
        columns = ["ts_ms", "dir", "bus", "id", "dlc", *(f"b{i}" for i in range(8))]
        rows = [
            [0, "R", 1, 0x399, 8, 6, 0, 0, 0, 0, 12, 0, 0],
            [10, "R", 2, 0x3C2, 8, 1, 0, 0, 0, 0, 0, 0, 0],
            [50, "R", 2, 0x3C2, 8, 1, 0, 1, 0, 0, 0, 0, 0],
            [90, "R", 2, 0x3C2, 8, 1, 0, 0x3F, 0, 0, 0, 0, 0],
            [100, "T", 2, 0x3C2, 8, 1, 0, 1, 0, 0, 0, 0, 0],
            [200, "T", 2, 0x3C2, 8, 1, 0, 0, 0, 0, 0, 0, 0],
            [300, "T", 2, 0x3C2, 8, 1, 0, 0x3F, 0, 0, 0, 0, 0],
            [400, "T", 2, 0x3C2, 8, 1, 0, 0, 0, 0, 0, 0, 0],
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.csv"
            with path.open("w", newline="") as stream:
                writer = csv.writer(stream)
                writer.writerow(columns)
                writer.writerows(rows)
            report = MODULE.analyze(path)

        self.assertTrue(report["capture_pass"])
        self.assertEqual(3, report["can2_rx_mux1"])
        self.assertEqual(1, report["can2_rx_mux1_idle"])
        self.assertEqual(1, report["volume_up_01"])
        self.assertEqual(1, report["volume_down_3f"])
        self.assertEqual(1, report["complete_tx_sequences"])


if __name__ == "__main__":
    unittest.main()
