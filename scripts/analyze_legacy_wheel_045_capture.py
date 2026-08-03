#!/usr/bin/env python3
"""Analyze receive-only Legacy Model S/X STW_ACTN_RQ recorder captures."""

from __future__ import annotations

import argparse
import csv
import json
import statistics
from collections import Counter, defaultdict
from pathlib import Path


CAN_IDS = {0x045: "CH/BFT", 0x238: "PT", 0x292: "BDY"}
LEVER_NAMES = {0: "idle", 1: "up", 2: "down", 3: "forward", 4: "back"}
COND_NAMES = {0: "idle", 1: "up", 2: "down", 3: "invalid"}


def signed_nibble(value: int) -> int:
    value &= 0x0F
    return value - 16 if value & 0x08 else value


def crc8_j1850(data: list[int]) -> int:
    crc = 0xFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1D) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ 0xFF


def count_action_episodes(rows: list[dict[str, object]], selector) -> int:
    times = [int(row["ts_ms"]) for row in rows if selector(row)]
    return sum(1 for index, timestamp in enumerate(times) if index == 0 or timestamp - times[index - 1] > 350)


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


def analyze(path: Path) -> dict[str, object]:
    rows = read_rows(path)
    wheel = [row for row in rows if row["id"] in CAN_IDS]
    rx = [row for row in wheel if row["dir"] == "R" and row["dlc"] >= 8]
    tx = [row for row in wheel if row["dir"] == "T"]
    errors = [row for row in wheel if row["dir"] == "E"]

    rx_by_route = Counter((int(row["bus"]), int(row["id"])) for row in rx)
    lever = Counter((row["data"][3] & 0x07) for row in rx)  # type: ignore[index]
    cond = Counter(((row["data"][3] >> 4) & 0x03) for row in rx)  # type: ignore[index]
    switch_masks = Counter((row["data"][4] | (row["data"][5] << 8)) for row in rx)  # type: ignore[index]
    active_switch_masks = {f"0x{key:04X}": value for key, value in switch_masks.items() if key}
    # The PT 0x238 representation carries a constant 0x0006 switch word on the
    # target vehicle. Decode physical wheel controls only from the live
    # CH/BFT 0x045 stream and its byte-identical BDY 0x292 gateway copy.
    physical_rows = [row for row in rx if row["id"] in (0x045, 0x292)]
    left_ticks = Counter()
    right_ticks = Counter()
    for row in physical_rows:
        mask = row["data"][4] | (row["data"][5] << 8)  # type: ignore[index]
        left_ticks[signed_nibble(mask >> 6)] += 1
        right_ticks[signed_nibble(mask >> 10)] += 1

    tx_045 = [row for row in tx if row["id"] == 0x045 and row["dlc"] >= 8]
    tx_left_ticks = Counter()
    for row in tx_045:
        mask = row["data"][4] | (row["data"][5] << 8)  # type: ignore[index]
        tx_left_ticks[signed_nibble(mask >> 6)] += 1
    tx_crc_valid = sum(
        1 for row in tx_045 if crc8_j1850(row["data"][:7]) == row["data"][7]  # type: ignore[index]
    )

    primary = physical_rows
    crc_valid = sum(
        1 for row in primary if crc8_j1850(row["data"][:7]) == row["data"][7]  # type: ignore[index]
    )
    primary_045 = [row for row in rx if row["id"] == 0x045]
    primary_292 = [row for row in rx if row["id"] == 0x292]
    mirror_matches = 0
    mirror_index = 0
    for source in primary_045:
        source_ts = int(source["ts_ms"])
        while mirror_index < len(primary_292) and int(primary_292[mirror_index]["ts_ms"]) < source_ts - 5:
            mirror_index += 1
        for candidate in primary_292[mirror_index : mirror_index + 3]:
            if abs(int(candidate["ts_ms"]) - source_ts) <= 5 and candidate["data"] == source["data"]:
                mirror_matches += 1
                break

    periods: list[int] = []
    counter_steps = 0
    counter_discontinuities = 0
    grouped: dict[tuple[int, int], list[dict[str, object]]] = defaultdict(list)
    for row in rx:
        grouped[(int(row["bus"]), int(row["id"]))].append(row)
    for group in grouped.values():
        group.sort(key=lambda row: int(row["ts_ms"]))
        for previous, current in zip(group, group[1:]):
            periods.append(int(current["ts_ms"]) - int(previous["ts_ms"]))
            previous_counter = (previous["data"][6] >> 4) & 0x0F  # type: ignore[index]
            current_counter = (current["data"][6] >> 4) & 0x0F  # type: ignore[index]
            counter_steps += 1
            if current_counter != ((previous_counter + 1) & 0x0F):
                counter_discontinuities += 1

    return {
        "file": str(path),
        "duration_ms": max((int(row["ts_ms"]) for row in rows), default=0),
        "frames": len(rows),
        "receive_only": not tx and not errors,
        "native_stw_actn_visible": bool(rx),
        "rx_by_bus_and_id": {
            f"CAN{bus}:0x{can_id:03X}:{CAN_IDS[can_id]}": count
            for (bus, can_id), count in sorted(rx_by_route.items())
        },
        "tx_frames": len(tx),
        "tx_errors": len(errors),
        "lever_counts": {LEVER_NAMES.get(key, f"unknown_{key}"): value for key, value in sorted(lever.items())},
        "condition_counts": {COND_NAMES.get(key, f"unknown_{key}"): value for key, value in sorted(cond.items())},
        "active_switch_masks": active_switch_masks,
        "decoded_controls": {
            "left_tick_positive_frames": left_ticks[1],
            "left_tick_negative_frames": left_ticks[-1],
            "right_tick_positive_frames": right_ticks[1],
            "right_tick_negative_frames": right_ticks[-1],
            "left_press_frames": sum(1 for row in physical_rows if row["data"][4] & 0x02),  # type: ignore[index]
            "left_side_bit0_frames": sum(1 for row in physical_rows if row["data"][4] & 0x01),  # type: ignore[index]
            "left_side_bit2_frames": sum(1 for row in physical_rows if row["data"][4] & 0x04),  # type: ignore[index]
            "right_press_frames": sum(1 for row in physical_rows if row["data"][4] & 0x10),  # type: ignore[index]
            "right_side_bit3_frames": sum(1 for row in physical_rows if row["data"][4] & 0x08),  # type: ignore[index]
            "right_side_bit5_frames": sum(1 for row in physical_rows if row["data"][4] & 0x20),  # type: ignore[index]
        },
        "decoded_tx_controls": {
            "can1_045_frames": len(tx_045),
            "left_tick_positive_frames": tx_left_ticks[1],
            "left_tick_negative_frames": tx_left_ticks[-1],
            "crc_valid": tx_crc_valid,
        },
        "action_episodes": {
            "left_tick_positive": count_action_episodes(physical_rows, lambda row: signed_nibble((row["data"][4] | (row["data"][5] << 8)) >> 6) == 1),  # type: ignore[index]
            "left_tick_negative": count_action_episodes(physical_rows, lambda row: signed_nibble((row["data"][4] | (row["data"][5] << 8)) >> 6) == -1),  # type: ignore[index]
            "right_tick_positive": count_action_episodes(physical_rows, lambda row: signed_nibble((row["data"][4] | (row["data"][5] << 8)) >> 10) == 1),  # type: ignore[index]
            "right_tick_negative": count_action_episodes(physical_rows, lambda row: signed_nibble((row["data"][4] | (row["data"][5] << 8)) >> 10) == -1),  # type: ignore[index]
        },
        "crc8_j1850": {
            "poly": "0x1D",
            "init": "0xFF",
            "xorout": "0xFF",
            "input": "data[0..6]",
            "valid": crc_valid,
            "checked": len(primary),
        },
        "gateway_mirror": {
            "can1_045_frames": len(primary_045),
            "can2_292_frames": len(primary_292),
            "identical_payload_within_5ms": mirror_matches,
        },
        "counter_steps": counter_steps,
        "counter_discontinuities": counter_discontinuities,
        "period_ms": {
            "median": statistics.median(periods) if periods else None,
            "min": min(periods) if periods else None,
            "max": max(periods) if periods else None,
        },
        "mapping_evidence": bool(
            lever[1] or lever[2] or lever[3] or lever[4] or cond[1] or cond[2] or active_switch_masks
        ),
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
        print(f"  STW_ACTN_RQ visible: {report['native_stw_actn_visible']}")
        print(f"  RX routes: {report['rx_by_bus_and_id']}")
        print(f"  lever: {report['lever_counts']}")
        print(f"  condition: {report['condition_counts']}")
        print(f"  switch masks: {report['active_switch_masks']}")
        print(f"  decoded controls: {report['decoded_controls']}")
        print(f"  decoded TX controls: {report['decoded_tx_controls']}")
        print(f"  action episodes: {report['action_episodes']}")
        print(f"  CRC-8/J1850: {report['crc8_j1850']}")
        print(f"  gateway mirror: {report['gateway_mirror']}")
        print(f"  TX/error: {report['tx_frames']}/{report['tx_errors']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
