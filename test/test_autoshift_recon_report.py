"""Contract + behavior tests for the autoshift recon report toolkit.

Fixture layout mirrors recorder v2 session CSVs exactly:
columns ts_us,bus,dir,id,dlc,b0..b7 with DECIMAL ids
(0x228=552, 0x118=280, 0x00E=14, 0x20E=526, 0x23E=574).
"""

import csv
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import autoshift_recon_report as rr  # noqa: E402

# The 16 desk-verified static vectors for stalk state 40 C0:
# crc8_j1850(40 C0 c0) for c = 0..F. Ground truth; the module must agree.
PINNED_SBW_CRC = {
    0x0: 0xB2, 0x1: 0x7F, 0x2: 0x35, 0x3: 0xF8,
    0x4: 0xA1, 0x5: 0x6C, 0x6: 0x26, 0x7: 0xEB,
    0x8: 0x94, 0x9: 0x59, 0xA: 0x13, 0xB: 0xDE,
    0xC: 0x87, 0xD: 0x4A, 0xE: 0x00, 0xF: 0xCD,
}


def crc8_j1850_ref(data):
    """Reference SAE J1850: poly 0x1D, init 0xFF, xorout 0xFF, MSB-first."""
    crc = 0xFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1D) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ 0xFF


def sbw_row(ts_us, b0, b1, counter, crc):
    b2 = (counter & 0x0F) << 4
    return (ts_us, 3, 552, [b0, b1, b2, crc])


def write_session(path, rows):
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["ts_us", "bus", "dir", "id", "dlc",
                    "b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7"])
        for ts_us, bus, can_id, data in rows:
            assert len(data) <= 8
            padded = list(data) + [0] * (8 - len(data))
            w.writerow([ts_us, bus, "rx", can_id, len(data)] + padded)


def build_fixture(path):
    rows = []
    # SBW stream on bus 3 (CanC): stalk state 40 C0, counters 0..15 @10 ms.
    for c in range(16):
        rows.append(sbw_row(c * 10_000, 0x40, 0xC0, c,
                            crc8_j1850_ref([0x40, 0xC0, (c & 0x0F) << 4])))
    # Second stalk state 41 C1, counters 0..3 (stream wraps past 15).
    for c in range(4):
        rows.append(sbw_row(200_000 + c * 10_000, 0x41, 0xC1, c,
                            crc8_j1850_ref([0x41, 0xC1, (c & 0x0F) << 4])))
    # One CRC-corrupt 40 C0 row (counter 4, crc deliberately wrong).
    rows.append(sbw_row(240_000, 0x40, 0xC0, 4, 0x00))
    # 0x118 on bus 1: two byte patterns — gear b3 = 0x10 (P), then 0x40 (D);
    # brake b6 goes 0x00 -> 0x20 in the same step.
    for i in range(10):
        rows.append((1_000_000 + i * 10_000, 1, 280,
                     [0, 0, 0, 0x10, 0, 0, 0x00, 0]))
    for i in range(10):
        rows.append((3_000_000 + i * 10_000, 1, 280,
                     [0, 0, 0, 0x40, 0, 0, 0x20, 0]))
    # 0x00E sweep on bus 3: byte 0 steps 0x00..0xF0.
    for i in range(16):
        rows.append((5_000_000 + i * 10_000, 3, 14, [i * 0x10] + [0] * 7))
    # Obstacle suite on bus 3: 0x20E x10 @100 ms -> 10.0 Hz (edges formula),
    # 0x23E x5 @250 ms -> 4.0 Hz.
    for i in range(10):
        rows.append((7_000_000 + i * 100_000, 3, 526, [1] * 8))
    for i in range(5):
        rows.append((9_000_000 + i * 250_000, 3, 574, [2] * 8))
    rows.sort(key=lambda r: r[0])
    write_session(path, rows)
    return rows


WINDOWS = [
    (0, 190, "stalk_idle_p"),      # covers the 40 C0 block (ms)
    (200, 260, "stalk_press"),     # covers the 41 C1 block
    (2900, 3100, "stalk_to_d"),    # covers the 0x118 b3 flip at t=3000 ms
]


class CrcTests(unittest.TestCase):
    def test_module_crc8_matches_all_16_pinned_vectors(self):
        for counter, want in PINNED_SBW_CRC.items():
            got = rr.crc8_j1850(bytes([0x40, 0xC0, counter << 4]))
            self.assertEqual(got, want, f"counter {counter:#x}")


class AnalyzeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.sess = Path(cls.tmp.name) / "sess_fixture.csv"
        build_fixture(cls.sess)
        cls.report = rr.analyze([str(cls.sess)], window_labels=WINDOWS)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_report_has_the_five_plan_keys(self):
        for key in ("sbw_states", "id118_bytes", "angle00e_bytes",
                    "obstacle_rates", "profile_draft"):
            self.assertIn(key, self.report)

    def test_sbw_clusters_by_state_bytes_with_counts(self):
        states = self.report["sbw_states"]
        self.assertEqual(set(states), {"40C0", "41C1"})
        self.assertEqual(states["40C0"]["count"], 17)  # 16 valid + 1 corrupt
        self.assertEqual(states["41C1"]["count"], 4)

    def test_sbw_per_state_crc_tallies(self):
        states = self.report["sbw_states"]
        self.assertEqual(states["40C0"]["crc_pass"], 16)
        self.assertEqual(states["40C0"]["crc_fail"], 1)
        self.assertEqual(states["41C1"]["crc_pass"], 4)
        self.assertEqual(states["41C1"]["crc_fail"], 0)

    def test_sbw_counter_continuity_fraction_over_valid_rows(self):
        # 0..15 then wrap to 0..3: every consecutive pair is +1 mod 16 -> 1.0.
        self.assertAlmostEqual(self.report["sbw_continuity"], 1.0)

    def test_profile_draft_gear_codes_keyed_by_state_bytes(self):
        gear_codes = self.report["profile_draft"]["gear_codes"]
        self.assertEqual(set(gear_codes), {"40C0", "41C1"})
        self.assertEqual(gear_codes["40C0"]["windows"], ["stalk_idle_p"])
        self.assertEqual(gear_codes["41C1"]["windows"], ["stalk_press"])

    def test_id118_byte_map_distinct_values_and_window_annotation(self):
        b3 = self.report["id118_bytes"]["3"]
        self.assertEqual(b3["distinct"], [0x10, 0x40])
        self.assertEqual(len(b3["changes"]), 1)
        ch = b3["changes"][0]
        self.assertEqual((ch["from"], ch["to"]), (0x10, 0x40))
        self.assertEqual(ch["window"], "stalk_to_d")
        b6 = self.report["id118_bytes"]["6"]
        self.assertEqual(b6["distinct"], [0x00, 0x20])
        self.assertEqual(b6["changes"][0]["window"], "stalk_to_d")

    def test_angle00e_byte_min_max(self):
        b0 = self.report["angle00e_bytes"]["0"]
        self.assertEqual(b0["min"], 0x00)
        self.assertEqual(b0["max"], 0xF0)
        self.assertEqual(len(b0["distinct"]), 16)
        self.assertEqual(b0["max_mono_run"], 16)  # sweep is one rising run

    def test_obstacle_rates_per_bus_hz(self):
        rates = self.report["obstacle_rates"]
        self.assertAlmostEqual(rates["526/3"], 10.0, places=1)
        self.assertAlmostEqual(rates["574/3"], 4.0, places=1)


class CliTests(unittest.TestCase):
    def test_cli_emits_parseable_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            sess = Path(tmp) / "sess_cli.csv"
            build_fixture(sess)
            out = subprocess.run(
                [sys.executable, "-I", str(ROOT / "scripts" / "autoshift_recon_report.py"),
                 str(sess), "--json"],
                capture_output=True, text=True, timeout=60)
            self.assertEqual(out.returncode, 0, out.stderr)
            payload = json.loads(out.stdout)
            for key in ("sbw_states", "id118_bytes", "angle00e_bytes",
                        "obstacle_rates", "profile_draft"):
                self.assertIn(key, payload)

    def test_cli_accepts_repeatable_windows(self):
        with tempfile.TemporaryDirectory() as tmp:
            sess = Path(tmp) / "sess_cli2.csv"
            build_fixture(sess)
            out = subprocess.run(
                [sys.executable, "-I", str(ROOT / "scripts" / "autoshift_recon_report.py"),
                 str(sess), "--window", "0:190:stalk_idle_p",
                 "--window", "2900:3100:stalk_to_d", "--json"],
                capture_output=True, text=True, timeout=60)
            self.assertEqual(out.returncode, 0, out.stderr)
            payload = json.loads(out.stdout)
            self.assertEqual(
                payload["id118_bytes"]["3"]["changes"][0]["window"],
                "stalk_to_d")


if __name__ == "__main__":
    unittest.main()
