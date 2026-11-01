"""Behavioral tests for scripts/decode_blackbox.py (task 6, 2026-11-01).

The decoder turns a `sess_N.bin` blackbox download (raw 18-byte RecRingFrame
records: tsUs u32 LE, id u32 LE, dlc u8, flags u8, data[8]) into the exact CSV
the board streams — same header, same row format — so a downloaded bin and a
downloaded csv decode to byte-identical text. It is the host-side truth check
for the B2 raw-partition recorder.
"""

import importlib.util
import struct
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "decode_blackbox.py"

HEADER = "ts_us,bus,dir,id,dlc,b0,b1,b2,b3,b4,b5,b6,b7\n"


def load_module():
    spec = importlib.util.spec_from_file_location("decode_blackbox", SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def frame(ts_us: int, can_id: int, dlc: int, bus: int, dir_bits: int,
          data: bytes = b"") -> bytes:
    payload = (data + b"\x00" * 8)[:8]
    return struct.pack("<IIBB8s", ts_us, can_id, dlc,
                       (bus & 0x03) | ((dir_bits & 0x03) << 2), payload)


class DecodeBlackboxTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.mod = load_module()

    def test_roundtrip_synthetic_frames(self) -> None:
        # 300 frames = 5400 bytes: frame 228 (offset 4086) spans the decoder's
        # 4096-byte read boundary — a frame whose bytes arrive split across two
        # chunk reads must still decode. Mix buses (A/B/C), dirs (R/T/E) and
        # dlcs.
        frames = []
        for i in range(300):
            bus = 1 + (i % 3)
            dir_bits = i % 3
            dlc = i % 9
            frames.append(frame(1_000_000 + i * 1000, 0x118 + (i % 4), dlc,
                                bus, dir_bits, bytes(range(dlc))))
        out = self.mod.decode(b"".join(frames))
        lines = out.splitlines()
        self.assertEqual(lines[0], HEADER.rstrip("\n"))
        self.assertEqual(len(lines), 301)
        # Spot-check the boundary frame (index 228 → row 229).
        row = lines[229].split(",")
        self.assertEqual(row[0], str(1_000_000 + 228 * 1000))
        self.assertEqual(row[1], "1")  # bus A (1 + 228 % 3 == 1)
        self.assertEqual(row[2], "R")  # dir bits 228 % 3 == 0 → R
        # First data row parses back to the first synthetic frame's fields.
        first = lines[1].split(",")
        self.assertEqual(first[0], "1000000")
        self.assertEqual(first[1], "1")
        self.assertEqual(first[3], str(0x118))
        self.assertEqual(first[4], "0")  # dlc 0
        self.assertEqual(first[5:], ["0"] * 8)

    def test_rejects_misaligned_input(self) -> None:
        # A torn tail (bytes % 18 != 0) must be refused loudly, never decoded
        # as if the trailing bytes were a frame.
        with self.assertRaises(ValueError):
            self.mod.decode(frame(1, 2, 8, 1, 0) + b"\x01\x02\x03")
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            bad = Path(td) / "torn.bin"
            bad.write_bytes(b"\x01\x02")
            proc = subprocess.run(
                [sys.executable, str(SCRIPT), str(bad)], capture_output=True)
            # CLI form: nonzero exit, error named on stderr.
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn(b"18", proc.stderr)

    def test_csv_columns_exact(self) -> None:
        # Byte-compatible with the board CSV: header identical to
        # handleSessionDownload's frozen kHeader, row order preserved.
        out = self.mod.decode(frame(5, 0x3EE, 3, 2, 0, b"\x01\x02\x03") +
                              frame(6, 0x45, 1, 1, 1, b"\xff"))
        self.assertEqual(
            out,
            HEADER +
            "5,2,R,1006,3,1,2,3,0,0,0,0,0\n" +
            "6,1,T,69,1,255,0,0,0,0,0,0,0\n")

    def test_cli_writes_file_and_stdout(self) -> None:
        import tempfile
        data = frame(7, 0x118, 2, 3, 2, b"\xaa\xbb")
        with tempfile.TemporaryDirectory() as td:
            src = Path(td) / "sess_9.bin"
            dst = Path(td) / "out.csv"
            src.write_bytes(data)
            proc = subprocess.run(
                [sys.executable, str(SCRIPT), str(src), "-o", str(dst)],
                capture_output=True)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertEqual(dst.read_text(),
                             HEADER + "7,3,E,280,2,170,187,0,0,0,0,0,0\n")
            # No -o: CSV lands on stdout.
            proc = subprocess.run(
                [sys.executable, str(SCRIPT), str(src)], capture_output=True)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertEqual(proc.stdout.decode(),
                             HEADER + "7,3,E,280,2,170,187,0,0,0,0,0,0\n")


if __name__ == "__main__":
    unittest.main()
