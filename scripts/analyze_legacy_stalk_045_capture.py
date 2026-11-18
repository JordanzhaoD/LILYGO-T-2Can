#!/usr/bin/env python3
"""Analyze Legacy 0x045 STW_ACTN_RQ captures for the speed-control lever
(SpdCtrlLvr_Stat) gesture encoding.

The wheel-DND analyzer only decodes data[3] (StW_Lvr_Stat) and the
data[4]/data[5] switch word. This script targets the byte0 lever field
(SpdCtrlLvr_Stat, bits 0-5 + VSL_Enbl_Rq bit 6) — the field the stalk-DND
feature must reproduce to clear the NAG via an AP-safe speed-set gesture.

Expected opendbc value domain (VAL_ SpdCtrlLvr_Stat):
    0x40 idle (VSL_Enbl_Rq held 1)
    0x41 FWD  (cancel AP)
    0x42 RWD  (request AP)
    0x44 UP_2ND / 0x48 DN_2ND
    0x50 UP_1ST / 0x60 DN_1ST
This script reports the ACTUAL byte0 values seen in a capture so we can pin
which bit pattern the vehicle emits for a physical up/down nudge.
"""

from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path


CAN_IDS = {0x045: "CH/BFT", 0x238: "PT", 0x292: "BDY"}

# opendbc SpdCtrlLvr_Stat names (byte0 low 6 bits, bit6 = VSL_Enbl_Rq)
LEVER_BYTE_NAMES = {
    0x40: "idle(base)",
    0x41: "FWD(cancel_AP)",
    0x42: "RWD(request_AP)",
    0x44: "UP_2ND",
    0x48: "DN_2ND",
    0x50: "UP_1ST",
    0x60: "DN_1ST",
}


def crc8_j1850(data: list[int]) -> int:
    crc = 0xFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1D) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ 0xFF


def read_rows(path: Path) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        required = {"ts_ms", "dir", "bus", "id", "dlc", *(f"b{i}" for i in range(8))}
        missing = required.difference(reader.fieldnames or [])
        if missing:
            raise ValueError(f"{path}: missing columns: {', '.join(sorted(missing))}")
        for raw in reader:
            rows.append(
                {
                    "ts_ms": int(raw["ts_ms"]),
                    "dir": raw["dir"].upper(),
                    "bus": int(raw["bus"]),
                    "id": int(raw["id"], 0),
                    "dlc": int(raw["dlc"]),
                    "data": [int(raw[f"b{i}"], 0) for i in range(8)],
                }
            )
    return rows


def byte0_name(value: int) -> str:
    if value in LEVER_BYTE_NAMES:
        return LEVER_BYTE_NAMES[value]
    low6 = value & 0x3F
    vsl = (value >> 6) & 1
    return f"0x{value:02X}(low6=0x{low6:02X},VSL={vsl})"


def analyze(path: Path) -> dict[str, object]:
    rows = read_rows(path)
    wheel = [row for row in rows if row["id"] in CAN_IDS]
    rx = [row for row in wheel if row["dir"] == "R" and row["dlc"] >= 8]
    tx = [row for row in wheel if row["dir"] == "T"]
    errors = [row for row in wheel if row["dir"] == "E"]

    # Primary 0x045 stream (CH/VEH) — the field the stalk-DND must mirror.
    primary = [row for row in rx if row["id"] == 0x045]

    # byte0 distribution across every observed frame.
    byte0_all = Counter(row["data"][0] for row in rx)  # type: ignore[index]
    byte0_045 = Counter(row["data"][0] for row in primary)  # type: ignore[index]

    # Non-idle byte0 values = gestures (any low-6-bit residue).
    gestures = [
        row for row in primary if (row["data"][0] & 0x3F) != 0  # type: ignore[index]
    ]
    gesture_bytes = Counter(row["data"][0] for row in gestures)  # type: ignore[index]

    # CRC validity over the primary 0x045 stream.
    crc_valid = sum(
        1 for row in primary if crc8_j1850(row["data"][:7]) == row["data"][7]  # type: ignore[index]
    )

    # Rolling counter continuity (data[6] high nibble) on the 0x045 stream.
    ordered = sorted(primary, key=lambda row: int(row["ts_ms"]))
    counter_discontinuities = 0
    counter_steps = 0
    for prev, cur in zip(ordered, ordered[1:]):
        pc = (prev["data"][6] >> 4) & 0x0F  # type: ignore[index]
        cc = (cur["data"][6] >> 4) & 0x0F  # type: ignore[index]
        counter_steps += 1
        if cc != ((pc + 1) & 0x0F):
            counter_discontinuities += 1

    # Transience: how long a gesture byte0 value stays on the bus before
    # returning to idle (0x40). A physical up/down nudge should be brief.
    gesture_bursts: list[dict[str, object]] = []
    burst_start = None
    burst_end = None
    for row in ordered:
        is_gesture = (row["data"][0] & 0x3F) != 0  # type: ignore[index]
        ts = int(row["ts_ms"])
        if is_gesture and burst_start is None:
            burst_start = ts
        if not is_gesture and burst_start is not None:
            burst_end = ts
            gesture_bursts.append(
                {
                    "start_ms": burst_start,
                    "end_ms": burst_end,
                    "duration_ms": burst_end - burst_start,
                }
            )
            burst_start = None
    if burst_start is not None:
        gesture_bursts.append(
            {
                "start_ms": burst_start,
                "end_ms": int(ordered[-1]["ts_ms"]),
                "duration_ms": int(ordered[-1]["ts_ms"]) - burst_start,
            }
        )

    # Sample raw frames for each distinct byte0 value (first 3 occurrences).
    samples: dict[str, list[str]] = {}
    for row in primary:
        key = byte0_name(row["data"][0])  # type: ignore[index]
        if key not in samples:
            samples[key] = []
        if len(samples[key]) < 3:
            samples[key].append(
                " ".join(f"{b:02X}" for b in row["data"])  # type: ignore[index]
            )

    return {
        "file": str(path),
        "duration_ms": max((int(row["ts_ms"]) for row in rows), default=0),
        "frames": len(rows),
        "receive_only": not tx and not errors,
        "native_045_frames": len(primary),
        "crc_valid": crc_valid,
        "crc_checked": len(primary),
        "counter_steps": counter_steps,
        "counter_discontinuities": counter_discontinuities,
        "byte0_distribution_all_buses": {
            byte0_name(key): value for key, value in sorted(byte0_all.items())
        },
        "byte0_distribution_045": {
            byte0_name(key): value for key, value in sorted(byte0_045.items())
        },
        "gesture_byte0_values": {
            byte0_name(key): value for key, value in sorted(gesture_bytes.items())
        },
        "gesture_frames": len(gestures),
        "gesture_bursts": gesture_bursts,
        "samples_by_byte0": samples,
        # opendbc mapping hint: which expected lever values actually appeared
        "expected_values_seen": {
            name: byte0_045.get(value, 0)
            for value, name in LEVER_BYTE_NAMES.items()
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="+", type=Path, help="recorder CSV file(s)")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()
    reports = [analyze(path) for path in args.csv]
    if args.json:
        print(json.dumps(reports, ensure_ascii=False, indent=2))
        return 0
    for report in reports:
        print(f"\n{report['file']}")
        print(f"  0x045 frames: {report['native_045_frames']} "
              f"(CRC valid {report['crc_valid']}/{report['crc_checked']}, "
              f"counter discontinuities {report['counter_discontinuities']}/{report['counter_steps']})")
        print(f"  byte0 (SpdCtrlLvr_Stat) distribution:")
        for name, count in report["byte0_distribution_045"].items():
            print(f"    {name:24s} x{count}")
        print(f"  gesture frames (byte0 low6 != 0): {report['gesture_frames']}")
        for name, count in report["gesture_byte0_values"].items():
            print(f"    {name:24s} x{count}")
        print(f"  gesture bursts (transience): {report['gesture_bursts']}")
        print(f"  expected opendbc values seen: {report['expected_values_seen']}")
        print(f"  samples:")
        for name, frames in report["samples_by_byte0"].items():
            for frame in frames:
                print(f"    {name:24s} {frame}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
