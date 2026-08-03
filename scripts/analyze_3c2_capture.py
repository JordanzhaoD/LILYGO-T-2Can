#!/usr/bin/env python3
"""Analyze dual-CAN 0x3C2 wheel-DND evidence from recorder CSV files."""

from __future__ import annotations

import argparse
import csv
import json
import statistics
from collections import Counter
from pathlib import Path


CAN_ID_WHEEL = 0x3C2
CAN_ID_DAS = 0x399
EXPECTED_TX_SEQUENCE = [0x01, 0x00, 0x3F, 0x00]


def percentile(values: list[int], pct: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    position = (len(ordered) - 1) * pct / 100.0
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def read_rows(path: Path) -> list[dict[str, int | str | list[int]]]:
    rows = []
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


def count_tx_sequences(tx_rows: list[dict[str, int | str | list[int]]]) -> int:
    count = 0
    for index in range(len(tx_rows) - 3):
        group = tx_rows[index : index + 4]
        values = [row["data"][2] for row in group]  # type: ignore[index]
        times = [int(row["ts_ms"]) for row in group]
        if values == EXPECTED_TX_SEQUENCE and all(70 <= times[i + 1] - times[i] <= 140 for i in range(3)):
            count += 1
    return count


def analyze(path: Path) -> dict[str, object]:
    rows = read_rows(path)
    wheel = [row for row in rows if row["id"] == CAN_ID_WHEEL]
    can2_rx = [row for row in wheel if row["bus"] == 2 and row["dir"] == "R"]
    can2_tx = [row for row in wheel if row["bus"] == 2 and row["dir"] == "T"]
    can2_errors = [row for row in wheel if row["bus"] == 2 and row["dir"] == "E"]
    can1_rx = [row for row in wheel if row["bus"] == 1 and row["dir"] == "R"]
    mux1 = [row for row in can2_rx if row["data"][0] & 0x03 == 1]  # type: ignore[index]
    mux1_idle = [row for row in mux1 if row["data"][2] == 0 and row["data"][3] == 0]  # type: ignore[index]
    intervals = [int(can2_rx[i]["ts_ms"]) - int(can2_rx[i - 1]["ts_ms"]) for i in range(1, len(can2_rx))]
    das_rows = [row for row in rows if row["id"] == CAN_ID_DAS and row["dir"] == "R"]
    das_by_bus = Counter(int(row["bus"]) for row in das_rows)
    hands = Counter((row["data"][5] >> 2) & 0x0F for row in das_rows)  # type: ignore[index]
    ap_states = Counter(row["data"][0] & 0x0F for row in das_rows)  # type: ignore[index]
    muxes = Counter(row["data"][0] & 0x03 for row in can2_rx)  # type: ignore[index]
    volume = Counter(row["data"][2] for row in mux1)  # type: ignore[index]
    speed_nonzero = sum(1 for row in mux1 if row["data"][3] != 0)  # type: ignore[index]

    return {
        "file": str(path),
        "duration_ms": max((int(row["ts_ms"]) for row in rows), default=0),
        "frames": len(rows),
        "can2_native_3c2_visible": bool(can2_rx),
        "can2_mux1_visible": bool(mux1),
        "can2_mux1_idle_visible": bool(mux1_idle),
        "can2_rx_3c2": len(can2_rx),
        "can2_rx_mux1": len(mux1),
        "can2_rx_mux1_idle": len(mux1_idle),
        "can1_rx_3c2": len(can1_rx),
        "can2_tx_3c2": len(can2_tx),
        "can2_tx_error_3c2": len(can2_errors),
        "complete_tx_sequences": count_tx_sequences(can2_tx),
        "mux_counts": {str(key): value for key, value in sorted(muxes.items())},
        "volume_up_01": volume[0x01],
        "volume_down_3f": volume[0x3F],
        "speed_nonzero": speed_nonzero,
        "rx_period_ms": {
            "median": statistics.median(intervals) if intervals else None,
            "p95": percentile(intervals, 95),
            "min": min(intervals) if intervals else None,
            "max": max(intervals) if intervals else None,
        },
        "das_399_rx": len(das_rows),
        "das_399_rx_by_bus": {str(key): value for key, value in sorted(das_by_bus.items())},
        "hands_state_counts": {str(key): value for key, value in sorted(hands.items())},
        "ap_state_counts": {str(key): value for key, value in sorted(ap_states.items())},
        "capture_pass": bool(can2_rx and mux1 and mux1_idle and das_rows and not can2_errors),
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
        print(f"  capture_pass: {report['capture_pass']}")
        print(
            "  CAN2 0x3C2 RX/mux1/idle: "
            f"{report['can2_rx_3c2']}/{report['can2_rx_mux1']}/{report['can2_rx_mux1_idle']}"
        )
        print(
            "  volume up/down, speed nonzero: "
            f"{report['volume_up_01']}/{report['volume_down_3f']}, {report['speed_nonzero']}"
        )
        print(
            "  TX/error/complete sequences: "
            f"{report['can2_tx_3c2']}/{report['can2_tx_error_3c2']}/{report['complete_tx_sequences']}"
        )
        print(f"  0x399 RX by bus: {report['das_399_rx_by_bus']}")
        print(f"  0x399 hands states: {report['hands_state_counts']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
