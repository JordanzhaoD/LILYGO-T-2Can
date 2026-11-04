#!/usr/bin/env python3
"""Offline recon report for autoshift protocol discovery (Phase 2).

Digests recorder v2 session CSVs (columns ts_us,bus,dir,id,dlc,b0..b7,
DECIMAL ids) into the protocol-profile inputs:

- sbw_states: stalk-state clusters for CanC 0x228 (552) DLC4 — keyed by
  the b0/b1 state pair, with per-state CRC tallies (J1850 over b0..b2)
  and window attribution, so the Task-2 freeze can map state -> gear.
- id118_bytes / angle00e_bytes: per-byte maps for 0x118 (280, CanA) and
  0x00E (14, CanC), with change events annotated by --window labels
  (the stalk-sweep timeline disambiguates DI_gear vs DI_gearRequest).
- obstacle_rates: Hz per (id, bus) for the PARK obstacle suite.
- profile_draft: gear-code skeleton + stream health scalars.

Usage:
  python3 -I scripts/autoshift_recon_report.py sess_*.csv \
      --window 2900:3100:stalk_to_d [--json]
"""

import argparse
import csv
import json
import sys
from collections import defaultdict
from pathlib import Path

BUS_CANA = 1
BUS_CANC = 3
ID_SBW = 0x228      # 552 decimal — SBW_RQ_SCCM on CanC (DLC4)
ID_TORQUE2 = 0x118  # 280 decimal — DI_torque2 on CanA
ID_STW_ANGLE = 0x00E  # 14 decimal — STW_ANGLHP_STAT on CanC
OBSTACLE_IDS = (0x20E, 0x22E, 0x23E, 0x21E, 0x2BF, 0x31E)  # PARK suite


def crc8_j1850(data):
    """SAE J1850 CRC8: poly 0x1D, init 0xFF, xorout 0xFF, MSB-first."""
    crc = 0xFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1D) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ 0xFF


def parse_sessions(paths):
    """Merge session CSVs into one ts-sorted row stream."""
    rows = []
    for p in paths:
        with open(p, newline="") as f:
            for rec in csv.DictReader(f):
                try:
                    rows.append({
                        "ts_us": int(rec["ts_us"]),
                        "bus": int(rec["bus"]),
                        "id": int(rec["id"]),
                        "dlc": int(rec["dlc"]),
                        "data": [int(rec["b%d" % i]) for i in range(8)],
                    })
                except (KeyError, TypeError, ValueError):
                    continue  # malformed line — skip, report says so below
    rows.sort(key=lambda r: r["ts_us"])
    return rows


def _labels_for(ts_ms, windows):
    return [lbl for start, end, lbl in windows if start <= ts_ms <= end]


def _first_label(ts_ms, windows):
    labels = _labels_for(ts_ms, windows)
    return labels[0] if labels else None


def _analyze_sbw(rows, windows):
    states = {}
    order = []
    valid = []  # (counter, ts_ms) for CRC-valid rows
    for r in rows:
        if r["bus"] != BUS_CANC or r["id"] != ID_SBW or r["dlc"] != 4:
            continue
        b0, b1, b2, b3 = r["data"][:4]
        key = "%02X%02X" % (b0, b1)
        ok = crc8_j1850(bytes((b0, b1, b2))) == b3
        ts_ms = r["ts_us"] // 1000
        if key not in states:
            states[key] = {"count": 0, "crc_pass": 0, "crc_fail": 0,
                           "windows": []}
            order.append(key)
        st = states[key]
        st["count"] += 1
        st["crc_pass" if ok else "crc_fail"] += 1
        if ok:  # window attribution trusts CRC-valid rows only
            for lbl in _labels_for(ts_ms, windows):
                if lbl not in st["windows"]:
                    st["windows"].append(lbl)
        if ok:
            valid.append(((b2 >> 4) & 0x0F, ts_ms))
    pairs = total = 0
    for (prev, _), (cur, _) in zip(valid, valid[1:]):
        total += 1
        if (prev + 1) & 0x0F == cur:
            pairs += 1
    continuity = (pairs / total) if total else 0.0
    span_s = (valid[-1][1] - valid[0][1]) / 1000.0 if len(valid) > 1 else 0.0
    stream_hz = ((len(valid) - 1) / span_s) if span_s > 0 else 0.0
    return {k: states[k] for k in order}, continuity, stream_hz


def _byte_map(frames, windows, with_runs=False):
    """Per-byte summary over ALL bytes 0..7: min/max/distinct + change
    events (constant bytes appear with a single distinct value and no
    changes; with_runs adds the longest monotonic run)."""
    if not frames:
        return {}
    out = {
        str(i): {"min": 0xFF, "max": 0x00, "distinct": set(), "changes": []}
        for i in range(8)
    }
    prev = None
    for r in frames:
        ts_ms = r["ts_us"] // 1000
        for i in range(8):
            v = r["data"][i]
            entry = out[str(i)]
            entry["distinct"].add(v)
            entry["min"] = min(entry["min"], v)
            entry["max"] = max(entry["max"], v)
            if prev is not None and v != prev[i]:
                entry["changes"].append({
                    "ts_ms": ts_ms,
                    "from": prev[i],
                    "to": v,
                    "window": _first_label(ts_ms, windows),
                })
        prev = r["data"]
    for i in range(8):
        entry = out[str(i)]
        entry["distinct"] = sorted(entry["distinct"])
        if with_runs:
            values = [f["data"][i] for f in frames]
            best = run = 1
            direction = 0
            for a, b in zip(values, values[1:]):
                d = (b > a) - (b < a)
                if d != 0 and d == direction:
                    run += 1
                else:
                    direction, run = d, (2 if d != 0 else 1)
                best = max(best, run)
            entry["max_mono_run"] = best if len(values) > 1 else len(values)
    return out


def _analyze_obstacle_rates(rows):
    buckets = defaultdict(list)
    for r in rows:
        if r["id"] in OBSTACLE_IDS:
            buckets["%d/%d" % (r["id"], r["bus"])].append(r["ts_us"] / 1000.0)
    rates = {}
    for key, ts in buckets.items():
        span_s = (ts[-1] - ts[0]) / 1000.0  # ts holds ms
        rates[key] = round((len(ts) - 1) / span_s, 2) if span_s > 0 else 0.0
    return dict(sorted(rates.items()))


def analyze(paths, window_labels=None):
    """Build the recon report dict from session CSV paths."""
    windows = [(int(a), int(b), lbl) for a, b, lbl in (window_labels or [])]
    rows = parse_sessions([str(p) for p in paths])

    sbw_states, continuity, sbw_hz = _analyze_sbw(rows, windows)
    id118_bytes = _byte_map(
        [r for r in rows if r["bus"] == BUS_CANA and r["id"] == ID_TORQUE2],
        windows)
    angle00e_bytes = _byte_map(
        [r for r in rows if r["bus"] == BUS_CANC and r["id"] == ID_STW_ANGLE],
        windows, with_runs=True)
    obstacle_rates = _analyze_obstacle_rates(rows)

    profile_draft = {
        "gear_codes": {
            key: {"windows": st["windows"], "count": st["count"],
                  "crc_pass": st["crc_pass"], "crc_fail": st["crc_fail"]}
            for key, st in sbw_states.items()
        },
        "sbw_continuity": round(continuity, 4),
        "sbw_stream_hz": round(sbw_hz, 2),
        "rows_analyzed": len(rows),
        "sessions": [Path(str(p)).name for p in paths],
    }
    return {
        "sbw_states": sbw_states,
        "sbw_continuity": round(continuity, 4),
        "id118_bytes": id118_bytes,
        "angle00e_bytes": angle00e_bytes,
        "obstacle_rates": obstacle_rates,
        "profile_draft": profile_draft,
    }


def _markdown(report):
    lines = ["# autoshift recon report", ""]
    pd = report["profile_draft"]
    lines.append("sessions: %s (%d rows)" % (", ".join(pd["sessions"]),
                                             pd["rows_analyzed"]))
    lines.append("")
    lines.append("## SBW stalk states (CanC 0x228, DLC4)")
    lines.append("| state | count | crc pass | crc fail | windows |")
    lines.append("|---|---|---|---|---|")
    for key, st in report["sbw_states"].items():
        lines.append("| %s | %d | %d | %d | %s |" % (
            key, st["count"], st["crc_pass"], st["crc_fail"],
            ", ".join(st["windows"]) or "-"))
    lines.append("")
    lines.append("counter continuity: %.4f, stream ~%.1f Hz" % (
        report["sbw_continuity"], pd["sbw_stream_hz"]))
    for title, key in (("DI_torque2 0x118 (CanA)", "id118_bytes"),
                       ("STW angle 0x00E (CanC)", "angle00e_bytes")):
        lines.append("")
        lines.append("## %s byte map" % title)
        if not report[key]:
            lines.append("(no frames)")
        for byte, entry in report[key].items():
            extra = ""
            if "max_mono_run" in entry:
                extra = ", max mono run %d" % entry["max_mono_run"]
            lines.append("byte %s: distinct %s%s" % (
                byte, entry["distinct"], extra))
            for ch in entry["changes"]:
                lines.append("  t=%dms %d->%d @%s" % (
                    ch["ts_ms"], ch["from"], ch["to"],
                    ch["window"] if ch["window"] is not None else "-"))
    lines.append("")
    lines.append("## obstacle rates (id/bus)")
    if not report["obstacle_rates"]:
        lines.append("(no obstacle frames)")
    for key, hz in report["obstacle_rates"].items():
        lines.append("%s: %.2f Hz" % (key, hz))
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="+", help="session CSV path(s)")
    parser.add_argument("--window", action="append", default=[],
                        metavar="START_MS:END_MS:LABEL",
                        help="annotate changes inside a labeled time window "
                             "(repeatable)")
    parser.add_argument("--json", action="store_true",
                        help="print JSON only (default: markdown + JSON)")
    args = parser.parse_args(argv)

    windows = []
    for spec in args.window:
        parts = spec.split(":", 2)
        if len(parts) != 3:
            parser.error("--window expects START_MS:END_MS:LABEL, got %r"
                         % spec)
        windows.append((int(parts[0]), int(parts[1]), parts[2]))

    report = analyze(args.paths, window_labels=windows)
    if not args.json:
        print(_markdown(report))
        print()
    json.dump(report, sys.stdout, indent=2)
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
