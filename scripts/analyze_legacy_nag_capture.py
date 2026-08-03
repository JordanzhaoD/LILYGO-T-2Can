#!/usr/bin/env python3
"""Read-only analyzer for Legacy 0x370/0x399/0x488 recorder captures."""

from __future__ import annotations

import argparse
import csv
import json
import math
import statistics
from collections import Counter, defaultdict
from pathlib import Path


def load_capture(path: Path) -> list[dict]:
    rows = []
    with path.open(newline="", encoding="utf-8-sig") as handle:
        for raw in csv.DictReader(handle):
            rows.append(
                {
                    "ts": int(raw["ts_ms"]),
                    "dir": raw["dir"],
                    "bus": int(raw["bus"]),
                    "id": int(raw["id"]),
                    "dlc": int(raw["dlc"]),
                    "data": [int(raw[f"b{i}"]) for i in range(8)],
                }
            )
    return rows


def torque_centi_nm(data: list[int]) -> int:
    return (((data[2] & 0x0F) << 8) | data[3]) - 2050


def checksum_370(data: list[int]) -> int:
    return (sum(data[:7]) + 0x73) & 0xFF


def steering_488(data: list[int]) -> tuple[int, int]:
    control_type = (data[2] >> 6) & 0x03
    angle_x10 = (((data[0] & 0x7F) << 8) | data[1]) - 16384
    return control_type, angle_x10


def sign(value: int, deadband: int = 0) -> int:
    return 1 if value > deadband else -1 if value < -deadband else 0


def percentile(values: list[int], fraction: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    index = fraction * (len(ordered) - 1)
    low = math.floor(index)
    high = math.ceil(index)
    if low == high:
        return float(ordered[low])
    return ordered[low] * (high - index) + ordered[high] * (index - low)


def analyze(path: Path) -> dict:
    rows = load_capture(path)
    id_bus_dir = Counter((r["id"], r["bus"], r["dir"]) for r in rows)
    rx370 = [r for r in rows if r["id"] == 0x370 and r["dir"] == "R" and r["dlc"] >= 8]
    tx370 = [r for r in rows if r["id"] == 0x370 and r["dir"] == "T" and r["dlc"] >= 8]
    rx399 = [r for r in rows if r["id"] == 0x399 and r["dir"] == "R" and r["dlc"] >= 6]
    rx488 = [r for r in rows if r["id"] == 0x488 and r["dir"] == "R" and r["dlc"] >= 3]

    torque = [torque_centi_nm(r["data"]) for r in tx370]
    hands = Counter((r["data"][4] >> 6) & 0x03 for r in tx370)
    checksum_valid = sum(checksum_370(r["data"]) == r["data"][7] for r in tx370)

    latest_steering: dict[int, tuple[int, int, int]] = {}
    latest_hands_state = None
    latest_ap_state = None
    correlated = []
    steering_ages = []
    tx_by_hands_state = Counter()
    rx370_by_hands_state = Counter()
    source_pairs = []
    latest_rx370 = {}
    for row in rows:
        if row["id"] == 0x399 and row["dir"] == "R" and row["dlc"] >= 6:
            latest_ap_state = row["data"][0] & 0x0F
            latest_hands_state = (row["data"][5] >> 2) & 0x0F
        elif row["id"] == 0x488 and row["dir"] == "R" and row["dlc"] >= 3:
            control_type, angle = steering_488(row["data"])
            latest_steering[row["bus"]] = (row["ts"], control_type, angle)
        elif row["id"] == 0x370 and row["dir"] == "R" and row["dlc"] >= 8:
            latest_rx370[row["bus"]] = row
            rx370_by_hands_state[latest_hands_state] += 1
        elif row["id"] == 0x370 and row["dir"] == "T" and row["dlc"] >= 8:
            tx_by_hands_state[latest_hands_state] += 1
            source = latest_rx370.get(row["bus"])
            if source:
                source_pairs.append(
                    {
                        "counter": (row["data"][6] & 0x0F)
                        == (((source["data"][6] & 0x0F) + 1) & 0x0F),
                        "byte4": row["data"][4] == source["data"][4],
                        "base": all(row["data"][i] == source["data"][i] for i in (0, 1, 5)),
                    }
                )
            steering = latest_steering.get(row["bus"])
            if not steering:
                continue
            steer_ts, control_type, angle = steering
            age = row["ts"] - steer_ts
            if control_type != 1 or age < 0 or age > 250:
                continue
            torque_value = torque_centi_nm(row["data"])
            steering_ages.append(age)
            correlated.append(
                {
                    "angle": angle,
                    "torque": torque_value,
                    "directional": sign(angle, 5) != 0,
                    "opposite": sign(angle, 5) != 0 and sign(torque_value) == -sign(angle, 5),
                }
            )

    states = []
    transitions = []
    previous = None
    for row in rx399:
        state = (row["data"][5] >> 2) & 0x0F
        states.append(state)
        if state != previous:
            transitions.append((row["ts"], state))
            previous = state
    state3_starts = [ts for ts, state in transitions if state == 3]
    state3_intervals = [b - a for a, b in zip(state3_starts, state3_starts[1:])]
    state_durations = Counter()
    for index, (start, state) in enumerate(transitions):
        end = transitions[index + 1][0] if index + 1 < len(transitions) else rows[-1]["ts"]
        state_durations[state] += max(0, end - start)

    tx_timestamps = [r["ts"] for r in tx370]
    tx_gaps = [b - a for a, b in zip(tx_timestamps, tx_timestamps[1:])]
    type_counts = Counter(steering_488(r["data"])[0] for r in rx488)
    ap_counts = Counter((r["data"][0] & 0x0F) for r in rx399)

    tx_by_bus = Counter(r["bus"] for r in tx370)
    rx_by_bus = Counter(r["bus"] for r in rx370)
    ids_by_bus = defaultdict(Counter)
    for (frame_id, bus, direction), count in id_bus_dir.items():
        ids_by_bus[str(bus)][f"0x{frame_id:X}_{direction}"] = count

    return {
        "file": str(path),
        "duration_ms": rows[-1]["ts"] - rows[0]["ts"] if rows else 0,
        "rows": len(rows),
        "rx370": len(rx370),
        "tx370": len(tx370),
        "tx_coverage_pct": round(100 * len(tx370) / len(rx370), 2) if rx370 else 0,
        "rx370_by_bus": dict(rx_by_bus),
        "tx370_by_bus": dict(tx_by_bus),
        "tx_checksum_valid_pct": round(100 * checksum_valid / len(tx370), 2) if tx370 else 0,
        "tx_hands_levels": dict(hands),
        "native_rx_hands_levels": dict(Counter((r["data"][4] >> 6) & 0x03 for r in rx370)),
        "source_pair_counter_pct": round(100 * sum(p["counter"] for p in source_pairs) / len(source_pairs), 2)
        if source_pairs else 0,
        "source_pair_byte4_pct": round(100 * sum(p["byte4"] for p in source_pairs) / len(source_pairs), 2)
        if source_pairs else 0,
        "source_pair_base_pct": round(100 * sum(p["base"] for p in source_pairs) / len(source_pairs), 2)
        if source_pairs else 0,
        "tx_torque": {
            "min_centi_nm": min(torque) if torque else None,
            "max_centi_nm": max(torque) if torque else None,
            "mean_centi_nm": round(statistics.fmean(torque), 2) if torque else None,
            "stdev_centi_nm": round(statistics.pstdev(torque), 2) if torque else None,
            "p50_abs_centi_nm": percentile([abs(v) for v in torque], 0.50),
            "p90_abs_centi_nm": percentile([abs(v) for v in torque], 0.90),
            "ge_80_pct": round(100 * sum(abs(v) >= 80 for v in torque) / len(torque), 2) if torque else 0,
            "ge_180_pct": round(100 * sum(abs(v) >= 180 for v in torque) / len(torque), 2) if torque else 0,
            "positive_pct": round(100 * sum(v > 0 for v in torque) / len(torque), 2) if torque else 0,
            "negative_pct": round(100 * sum(v < 0 for v in torque) / len(torque), 2) if torque else 0,
        },
        "rx399": len(rx399),
        "hands_state_counts": dict(Counter(states)),
        "hands_state_duration_ms": dict(state_durations),
        "rx370_by_hands_state": {str(k): v for k, v in rx370_by_hands_state.items()},
        "tx370_by_hands_state": {str(k): v for k, v in tx_by_hands_state.items()},
        "tx_coverage_by_hands_state_pct": {
            str(k): round(100 * tx_by_hands_state[k] / value, 2) if value else 0
            for k, value in rx370_by_hands_state.items()
        },
        "ap_state_counts": dict(ap_counts),
        "hands_state_transitions": transitions,
        "state3_starts_ms": state3_starts,
        "state3_intervals_ms": state3_intervals,
        "rx488": len(rx488),
        "rx488_valid_type1": sum(steering_488(r["data"])[0] == 1 for r in rx488),
        "rx488_type_counts": dict(type_counts),
        "tx_with_fresh_type1_488": len(correlated),
        "tx_opposite_488_pct": round(100 * sum(c["opposite"] for c in correlated) / len(correlated), 2)
        if correlated
        else 0,
        "tx_directional_488": sum(c["directional"] for c in correlated),
        "tx_opposite_directional_488_pct": round(
            100 * sum(c["opposite"] for c in correlated) /
            sum(c["directional"] for c in correlated), 2
        ) if any(c["directional"] for c in correlated) else 0,
        "steering_age_p90_ms": percentile(steering_ages, 0.90),
        "tx_gap_p90_ms": percentile(tx_gaps, 0.90),
        "tx_gap_max_ms": max(tx_gaps) if tx_gaps else None,
        "ids_by_bus": {bus: dict(counts) for bus, counts in ids_by_bus.items()},
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--pretty", action="store_true")
    args = parser.parse_args()
    result = [analyze(path) for path in args.captures]
    print(json.dumps(result, ensure_ascii=False, indent=2 if args.pretty else None))


if __name__ == "__main__":
    main()
