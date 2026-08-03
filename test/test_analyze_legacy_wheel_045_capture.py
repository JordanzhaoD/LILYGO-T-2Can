import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "analyze_legacy_wheel_045_capture.py"
SPEC = importlib.util.spec_from_file_location("analyze_legacy_wheel_045_capture", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)


class LegacyWheel045AnalyzerTest(unittest.TestCase):
    def test_decodes_primary_and_gateway_aliases_without_tx(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "capture.csv"
            with path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.writer(stream)
                writer.writerow(["ts_ms", "dir", "bus", "id", "dlc", *[f"b{i}" for i in range(8)]])
                frames = [
                    [0x40, 0x21, 0, 0x30, 0x40, 0x00, 0x10, 0],
                    [0x40, 0x21, 0, 0x30, 0xC0, 0x03, 0x20, 0],
                    [0x40, 0x21, 0, 0x30, 0x00, 0x04, 0x30, 0],
                    [0x40, 0x21, 0, 0x30, 0x00, 0x3C, 0x40, 0],
                    [0x40, 0x21, 0, 0x30, 0x02, 0x00, 0x50, 0],
                ]
                for frame in frames:
                    frame[7] = MODULE.crc8_j1850(frame[:7])
                writer.writerow([0, "R", 1, 0x045, 8, *frames[0]])
                writer.writerow([10, "R", 1, 0x045, 8, *frames[1]])
                writer.writerow([20, "R", 1, 0x045, 8, *frames[2]])
                writer.writerow([30, "R", 1, 0x045, 8, *frames[3]])
                writer.writerow([40, "R", 1, 0x045, 8, *frames[4]])
                writer.writerow([41, "R", 2, 0x292, 8, *frames[4]])
            report = MODULE.analyze(path)
        self.assertTrue(report["native_stw_actn_visible"])
        self.assertTrue(report["receive_only"])
        controls = report["decoded_controls"]
        self.assertEqual(controls["left_tick_positive_frames"], 1)
        self.assertEqual(controls["left_tick_negative_frames"], 1)
        self.assertEqual(controls["right_tick_positive_frames"], 1)
        self.assertEqual(controls["right_tick_negative_frames"], 1)
        self.assertEqual(controls["left_press_frames"], 2)
        self.assertEqual(report["crc8_j1850"]["valid"], 6)
        self.assertEqual(report["gateway_mirror"]["identical_payload_within_5ms"], 1)
        self.assertTrue(report["mapping_evidence"])

    def test_decodes_generated_can1_tx_direction_and_crc(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "tx_capture.csv"
            positive = [0x40, 0x21, 0x04, 0x30, 0x40, 0x00, 0x70, 0]
            negative = [0x40, 0x21, 0x04, 0x30, 0xC0, 0x03, 0x80, 0]
            positive[7] = MODULE.crc8_j1850(positive[:7])
            negative[7] = MODULE.crc8_j1850(negative[:7])
            with path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.writer(stream)
                writer.writerow(["ts_ms", "dir", "bus", "id", "dlc", *[f"b{i}" for i in range(8)]])
                writer.writerow([10, "T", 1, 0x045, 8, *positive])
                writer.writerow([20, "T", 1, 0x045, 8, *negative])
            report = MODULE.analyze(path)
        tx = report["decoded_tx_controls"]
        self.assertEqual(tx["can1_045_frames"], 2)
        self.assertEqual(tx["left_tick_positive_frames"], 1)
        self.assertEqual(tx["left_tick_negative_frames"], 1)
        self.assertEqual(tx["crc_valid"], 2)


if __name__ == "__main__":
    unittest.main()
