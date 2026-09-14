#!/usr/bin/env python3
"""Calibrate the AP cancel/re-request 0x045 stalk profile from a recorder CSV.

Companion to analyze_legacy_wheel_045_capture.py. That script decodes the
wheel-DND switch-word hypothesis; this one answers the 4.5.0-beta01
coordinator question: which byte/mask/value encode the physical gear-stalk
forward (AP cancel) and rearward (AP request) gestures, and does the 0x399
DAS timeline corroborate that mapping?

The analyzer is deliberately assumption-free about the stalk field: it
learns the idle 0x045 payload (bytes 0..5; byte 6 is the rolling counter,
byte 7 the CRC), detects frames that deviate, groups them into press
events, diffs each event against the baseline, and cross-correlates every
event with the 0x399 AP-state timeline. A profile proposal is emitted only
when forward and rearward classes are internally consistent and
0x399-correlated; everything is reported as evidence, never as a validated
vehicle fact.
"""

from __future__ import annotations

import argparse
import csv
import json
import statistics
from collections import Counter
from pathlib import Path


AP_STATE_NAMES = {
    0: "0_OFF",
    1: "1_STANDBY",
    2: "2_AVAILABLE",
    3: "3_ACTIVE_NOMINAL",
    4: "4_ACTIVE_CHANGED_DEST",
    5: "5_ACTIVE_NEAR_CHANGE",
    6: "6_ACTIVE_LATERAL",
    7: "7_UNKNOWN",
    8: "8_TAKEOVER_IMMEDIATE",
    9: "9_TAKEOVER_URGENT",
}
AP_ACTIVE_STATES = {3, 4, 5, 6}
# 4.5.0-beta01 design contract: FWD cancels AP, RWD requests it. The
# design-time placeholder encoding (FWD=1, RWD=2 under mask 0x07 at byte 3)
# and the wheel-DND enumeration guess (3=forward, 4=back) are cross-checked
# against what the capture actually shows.
DESIGN_ASSUMPTION = {"byte": 3, "mask": 0x07, "fwd": 1, "rwd": 2}
WHEEL_DND_ENUMERATION = {"byte": 3, "mask": 0x07, "fwd": 3, "rwd": 4}
EVENT_GAP_MS = 350
AP_CORRELATION_WINDOW_MS = 3000
CRC_BYTES = range(7)  # data[0..6]; byte 7 is the CRC


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


def baseline_bytes(frames: list[dict[str, object]]) -> dict[str, object]:
    """Learn the idle 0x045 payload as the per-byte mode over bytes 0..5."""
    result: dict[str, object] = {"sample_frames": len(frames)}
    if not frames:
        result["idle"] = None
        return result
    modes: dict[int, int] = {}
    for index in range(6):
        values = Counter(int(frame["data"][index]) for frame in frames)  # type: ignore[index]
        modes[index] = values.most_common(1)[0][0]
    idle = [modes[index] for index in range(6)]
    idle_frames = [
        frame
        for frame in frames
        if all(int(frame["data"][index]) == idle[index] for index in range(6))
    ]
    result["idle"] = idle
    result["idle_frames"] = len(idle_frames)
    result["idle_ratio"] = round(len(idle_frames) / len(frames), 3)
    return result


def detect_events(
    frames: list[dict[str, object]], idle: list[int]
) -> list[dict[str, object]]:
    """Group non-idle 0x045 frames into press events (>350 ms gap splits)."""
    active = [
        frame
        for frame in frames
        if any(int(frame["data"][index]) != idle[index] for index in range(6))
    ]
    events: list[list[dict[str, object]]] = []
    for frame in active:
        if events and int(frame["ts_ms"]) - int(events[-1][-1]["ts_ms"]) <= EVENT_GAP_MS:
            events[-1].append(frame)
        else:
            events.append([frame])

    parsed: list[dict[str, object]] = []
    stream_sorted = sorted(frames, key=lambda frame: int(frame["ts_ms"]))
    for group in events:
        group.sort(key=lambda frame: int(frame["ts_ms"]))
        start_ts = int(group[0]["ts_ms"])
        end_ts = int(group[-1]["ts_ms"])
        next_idle_ts = None
        for frame in stream_sorted:
            ts = int(frame["ts_ms"])
            if ts > end_ts and all(
                int(frame["data"][index]) == idle[index] for index in range(6)
            ):
                next_idle_ts = ts
                break
        diff: dict[int, int] = {}
        extra_bits = 0
        for index in range(6):
            values = Counter(int(frame["data"][index]) for frame in group)  # type: ignore[index]
            event_value = values.most_common(1)[0][0]
            if event_value != idle[index]:
                diff[index] = event_value
                extra_bits |= event_value ^ idle[index]
        parsed.append(
            {
                "start_ms": start_ts,
                "end_ms": end_ts,
                "frames": len(group),
                "press_duration_ms": end_ts - start_ts,
                "release_gap_ms": (next_idle_ts - end_ts) if next_idle_ts else None,
                "diff": {f"byte{index}": {"idle": idle[index], "event": value} for index, value in sorted(diff.items())},
                "changed_bits": f"0x{extra_bits:02X}" if extra_bits else None,
            }
        )
    return parsed


def ap_state_timeline(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    """0x399 DAS_status transitions: [{ts_ms, from, to}] plus full span."""
    frames = sorted(
        (row for row in rows if row["id"] == 0x399 and row["dir"] == "R" and row["dlc"] >= 1),
        key=lambda row: int(row["ts_ms"]),
    )
    samples: list[tuple[int, int]] = [(int(row["ts_ms"]), int(row["data"][0]) & 0x0F) for row in frames]  # type: ignore[index]
    transitions: list[dict[str, object]] = []
    for (prev_ts, prev), (ts, state) in zip(samples, samples[1:]):
        if state != prev:
            transitions.append({"ts_ms": ts, "from": prev, "to": state})
    return {
        "samples": len(samples),
        "ap_at_capture_start": samples[0][1] if samples else None,
        "ap_at_capture_end": samples[-1][1] if samples else None,
        "transitions": transitions,
    }


def state_at(samples: list[tuple[int, int]], ts: int) -> int | None:
    current = None
    for sample_ts, state in samples:
        if sample_ts > ts:
            break
        current = state
    return current


def classify_events(
    events: list[dict[str, object]], rows: list[dict[str, object]]
) -> list[dict[str, object]]:
    """Correlate each press event with the 0x399 timeline.

    A forward (cancel) gesture: AP active (3..6) at the event, dropping to
    <=2 within the window. A rearward (request) gesture: AP <=2 at the
    event (the frozen qualification set admits AVAILABLE=2, so a press
    while already at 2 that engages 3 is a request edge), rising strictly
    above the event-time state into {2,3} within the window. The window is
    truncated at the next press event: a state change observed after the
    driver already made the next gesture belongs to that gesture, not this
    one (real-vehicle 2026-09-10 capture: a FWD press at AP=2 followed by a
    separate RWD press was otherwise credited with the later engagement).
    """
    frames = sorted(
        (row for row in rows if row["id"] == 0x399 and row["dir"] == "R" and row["dlc"] >= 1),
        key=lambda row: int(row["ts_ms"]),
    )
    samples: list[tuple[int, int]] = [(int(row["ts_ms"]), int(row["data"][0]) & 0x0F) for row in frames]  # type: ignore[index]
    for index, event in enumerate(events):
        ap_before = state_at(samples, int(event["start_ms"]))
        window_end = int(event["end_ms"]) + AP_CORRELATION_WINDOW_MS
        if index + 1 < len(events):
            window_end = min(window_end, int(events[index + 1]["start_ms"]))
        after = [
            (ts, state)
            for ts, state in samples
            if int(event["end_ms"]) < ts <= window_end
        ]
        reached = {state for _, state in after}
        event["ap_before"] = ap_before
        event["ap_states_after"] = sorted(reached)
        if ap_before in AP_ACTIVE_STATES:
            dropped = sorted(state for state in reached if state <= 2)
            event["gesture_hypothesis"] = "forward_cancel" if dropped else "ambiguous"
            event["ap_drop_after"] = dropped
        elif ap_before is not None and ap_before <= 2:
            started = sorted(state for state in reached if state in (2, 3) and state > ap_before)
            event["gesture_hypothesis"] = "rearward_request" if started else "ambiguous"
            event["ap_start_after"] = started
        else:
            event["gesture_hypothesis"] = "ambiguous"
    return events


def intent_timeline(rows: list[dict[str, object]]) -> dict[str, object]:
    """0x3EE mux0 bit38 (driver FSD intent) transitions."""
    frames = sorted(
        (
            row
            for row in rows
            if row["id"] == 0x3EE
            and row["dir"] == "R"
            and row["dlc"] >= 6
            and (int(row["data"][0]) & 0x0F) == 0  # type: ignore[index]
        ),
        key=lambda row: int(row["ts_ms"]),
    )
    samples = [(int(row["ts_ms"]), bool(int(row["data"][4]) & 0x40)) for row in frames]  # type: ignore[index]
    transitions: list[dict[str, object]] = []
    for (prev_ts, prev), (ts, value) in zip(samples, samples[1:]):
        if value != prev:
            transitions.append({"ts_ms": ts, "from": prev, "to": value})
    return {"mux0_frames": len(frames), "intent_transitions": transitions}


def propose_profile(
    events: list[dict[str, object]], idle: list[int], source_bus: int | None
) -> dict[str, object]:
    """Derive ApReRequestProfile candidates from classified press events.

    Mirrors the firmware validators in dash_ap_rerequest_activation.h
    (apReRequestProfileError): byte 0..7, nonzero mask, values within the
    mask, distinct, nonzero.
    """
    proposal: dict[str, object] = {
        "source_bus": ("CAN_BUS_VEH (TWAI, CSV bus 1)" if source_bus == 1 else "CAN_BUS_PARTY (MCP2515, CSV bus 2)" if source_bus == 2 else None)
    }
    forward = [event for event in events if event.get("gesture_hypothesis") == "forward_cancel"]
    rearward = [event for event in events if event.get("gesture_hypothesis") == "rearward_request"]
    proposal["forward_events"] = len(forward)
    proposal["rearward_events"] = len(rearward)
    if not forward or not rearward:
        proposal["status"] = "incomplete_capture"
        proposal["reason"] = (
            "no forward-correlated events"
            if not forward
            else "no rearward-correlated events"
        )
        return proposal

    def event_value(event: dict[str, object]) -> tuple[int, int] | None:
        diff = event.get("diff", {})
        if len(diff) != 1:
            return None
        ((byte, payload),) = diff.items()
        return int(byte.replace("byte", "")), int(payload["event"])  # type: ignore[index]

    forward_pairs = [pair for pair in (event_value(event) for event in forward) if pair]
    rearward_pairs = [pair for pair in (event_value(event) for event in rearward) if pair]
    if len(forward_pairs) != len(forward) or len(rearward_pairs) != len(rearward):
        proposal["status"] = "ambiguous_diff"
        proposal["reason"] = "some events change more than one byte; manual review required"
        return proposal
    forward_bytes = {byte for byte, _ in forward_pairs}
    rearward_bytes = {byte for byte, _ in rearward_pairs}
    if len(forward_bytes) != 1 or forward_bytes != rearward_bytes:
        proposal["status"] = "ambiguous_diff"
        proposal["reason"] = "forward/rearward gestures do not share exactly one changed byte"
        return proposal
    byte = forward_pairs[0][0]
    forward_values = {value for _, value in forward_pairs}
    rearward_values = {value for _, value in rearward_pairs}
    if len(forward_values) != 1 or len(rearward_values) != 1:
        proposal["status"] = "inconsistent_values"
        proposal["reason"] = "gesture class uses more than one value across its events"
        return proposal
    fwd_value = forward_values.pop()
    rwd_value = rearward_values.pop()
    mask = (fwd_value ^ idle[byte]) | (rwd_value ^ idle[byte])
    fwd_masked = fwd_value & mask
    rwd_masked = rwd_value & mask
    valid = (
        0 <= byte <= 7
        and mask != 0
        and (fwd_masked & ~mask) == 0
        and (rwd_masked & ~mask) == 0
        and fwd_masked != rwd_masked
        and fwd_masked != 0
        and rwd_masked != 0
    )
    proposal.update(
        {
            "status": "proposed" if valid else "invalid",
            "stalk_byte": byte,
            "stalk_mask": f"0x{mask:02X}",
            "stalk_fwd_value": fwd_masked,
            "stalk_rwd_value": rwd_masked,
            "raw_event_values": {"forward": fwd_value, "rearward": rwd_value},
        }
    )
    if not valid:
        return proposal

    def matches(guess: dict[str, int]) -> bool:
        return (
            byte == guess["byte"]
            and (mask & ~guess["mask"]) == 0
            and fwd_masked == guess["fwd"]
            and rwd_masked == guess["rwd"]
        )

    proposal["matches_design_assumption_F1_R2"] = matches(DESIGN_ASSUMPTION)
    proposal["matches_wheel_dnd_enumeration_F3_R4"] = matches(WHEEL_DND_ENUMERATION)
    press_durations = [int(event["press_duration_ms"]) for event in forward + rearward]  # type: ignore[index]
    proposal["observed_press_duration_ms"] = {
        "median": statistics.median(press_durations),
        "min": min(press_durations),
        "max": max(press_durations),
    }
    drops = sorted(
        {state for event in forward for state in event.get("ap_drop_after", [])}  # type: ignore[index]
    )
    starts = sorted(
        {state for event in rearward for state in event.get("ap_start_after", [])}  # type: ignore[index]
    )
    proposal["cancel_evidence_ap_states_observed"] = drops
    proposal["post_qual_ap_states_observed"] = starts
    proposal["timing_notes"] = {
        "stalk_release_gap_ms_candidate": statistics.median(press_durations),
        "qualification_window_ms": 500,
        "note": "qualification window 500 ms is the frozen design candidate, not capture-derived",
    }
    return proposal


def analyze(path: Path) -> dict[str, object]:
    rows = read_rows(path)
    rows_045 = [row for row in rows if row["id"] == 0x045]
    rx_045 = [row for row in rows_045 if row["dir"] == "R" and row["dlc"] >= 8]
    tx_045 = [row for row in rows_045 if row["dir"] == "T"]
    by_bus = Counter(int(row["bus"]) for row in rx_045)
    source_bus = by_bus.most_common(1)[0][0] if by_bus else None
    frames = sorted(
        (row for row in rx_045 if int(row["bus"]) == source_bus),
        key=lambda row: int(row["ts_ms"]),
    )

    baseline = baseline_bytes(frames)
    crc_valid = sum(
        1 for frame in frames if crc8_j1850(frame["data"][:7]) == frame["data"][7]  # type: ignore[index]
    )
    periods = [
        int(current["ts_ms"]) - int(previous["ts_ms"])
        for previous, current in zip(frames, frames[1:])
    ]
    idle = baseline["idle"]  # type: ignore[index]
    events = classify_events(detect_events(frames, idle), rows) if idle else []
    ap = ap_state_timeline(rows)
    intent = intent_timeline(rows)
    profile = propose_profile(events, idle, source_bus) if idle else {"status": "no_045_frames"}

    return {
        "file": str(path),
        "duration_ms": max((int(row["ts_ms"]) for row in rows), default=0),
        "frames": len(rows),
        "source_bus": source_bus,
        "rx_045_frames": len(rx_045),
        "tx_045_frames": len(tx_045),
        "tx_warning": "device 0x045 TX rows present; disable wheel DND and all 0x045 senders before capturing" if tx_045 else None,
        "crc8_j1850_valid": crc_valid,
        "crc8_j1850_checked": len(frames),
        "period_ms_median": statistics.median(periods) if periods else None,
        "idle_baseline": baseline,
        "press_events": events,
        "ap_state_timeline": ap,
        "intent_0x3ee_timeline": intent,
        "profile_proposal": profile,
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
        print(f"  0x045 RX {report['rx_045_frames']} frames on bus {report['source_bus']}, period median {report['period_ms_median']} ms")
        if report["tx_warning"]:
            print(f"  ⚠ {report['tx_warning']}")
        print(f"  CRC valid: {report['crc8_j1850_valid']}/{report['crc8_j1850_checked']}")
        print(f"  idle bytes 0..5: {[f'0x{value:02X}' for value in report['idle_baseline']['idle']] if report['idle_baseline']['idle'] else None} (idle ratio {report['idle_baseline'].get('idle_ratio')})")
        print(f"  press events ({len(report['press_events'])}):")
        for event in report["press_events"]:
            diff = ", ".join(
                f"{name} 0x{payload['idle']:02X}->0x{payload['event']:02X}"
                for name, payload in event["diff"].items()
            )
            print(
                f"    {event['start_ms']}ms dur {event['press_duration_ms']}ms ap_before={event['ap_before']} "
                f"after={event['ap_states_after']} -> {event['gesture_hypothesis']} [{diff}]"
            )
        print(f"  AP transitions: {report['ap_state_timeline']['transitions']}")
        print(f"  0x3EE intent transitions: {report['intent_0x3ee_timeline']['intent_transitions']}")
        print(f"  profile proposal: {json.dumps(report['profile_proposal'], ensure_ascii=False)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
