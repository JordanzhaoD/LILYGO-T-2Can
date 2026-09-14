import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "analyze_rerequest_045_calibration.py"
SPEC = importlib.util.spec_from_file_location("analyze_rerequest_045_calibration", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(MODULE)

IDLE_STALK = 0x30  # wheel-DND fixtures: byte3 idle = lever 0 + condition 3
FRAME_PERIOD_MS = 100


def build_rows(
    total_ms: int,
    stalk_presses: list[tuple[int, int, int]],
    ap_transitions: list[tuple[int, int]],
    intent_edges: list[tuple[int, bool]] | None = None,
    with_ap: bool = True,
) -> list[list[object]]:
    """Synthesize a recorder CSV body from event specs.

    stalk_presses: (start_ms, active_frame_count, byte3_value)
    ap_transitions: (ts_ms, new_ap_state) over an initial state of 3
    intent_edges: (ts_ms, bit38) over an initial state of False
    """
    stalk_at: dict[int, int] = {}
    for start, count, value in stalk_presses:
        for offset in range(count):
            stalk_at[start + offset * FRAME_PERIOD_MS] = value

    def stalk_value(ts: int) -> int:
        return stalk_at.get(ts, IDLE_STALK)

    def ap_state(ts: int) -> int:
        state = 3
        for edge_ts, edge_state in ap_transitions:
            if edge_ts <= ts:
                state = edge_state
        return state

    def intent(ts: int) -> bool:
        value = False
        if intent_edges:
            for edge_ts, edge_value in intent_edges:
                if edge_ts <= ts:
                    value = edge_value
        return value

    rows: list[list[object]] = []
    counter = 0
    for ts in range(0, total_ms + 1, FRAME_PERIOD_MS):
        data = [0x40, 0x21, 0x00, stalk_value(ts), 0x00, 0x00, (counter & 0x0F) << 4, 0]
        data[7] = MODULE.crc8_j1850(data[:7])
        rows.append([ts, "R", 1, 0x045, 8, *data])
        counter += 1
        if with_ap:
            rows.append([ts, "R", 1, 0x399, 8, ap_state(ts), 0, 0, 0, 0, 0, 0, 0])
            rows.append(
                [ts, "R", 1, 0x3EE, 8, 0x00, 0, 0, 0, 0x40 if intent(ts) else 0x00, 0, 0, 0]
            )
    return rows


def write_csv(path: Path, rows: list[list[object]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["ts_ms", "dir", "bus", "id", "dlc", *[f"b{i}" for i in range(8)]])
        writer.writerows(rows)


class Rerequest045CalibrationTest(unittest.TestCase):
    def test_proposes_profile_from_cancel_then_request_gestures(self):
        rows = build_rows(
            total_ms=4000,
            stalk_presses=[(1500, 2, 0x31), (3000, 2, 0x32)],
            ap_transitions=[(1700, 2), (1900, 1), (3300, 2), (3500, 3)],
            intent_edges=[(3400, True), (3900, False)],
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "capture.csv"
            write_csv(path, rows)
            report = MODULE.analyze(path)
        self.assertEqual(report["rx_045_frames"], 41)
        self.assertEqual(report["crc8_j1850_valid"], 41)
        self.assertEqual(report["source_bus"], 1)
        self.assertEqual(report["idle_baseline"]["idle"], [0x40, 0x21, 0x00, IDLE_STALK, 0x00, 0x00])
        events = report["press_events"]
        self.assertEqual(len(events), 2)
        self.assertEqual(events[0]["gesture_hypothesis"], "forward_cancel")
        self.assertEqual(events[1]["gesture_hypothesis"], "rearward_request")
        self.assertEqual(events[0]["diff"], {"byte3": {"idle": IDLE_STALK, "event": 0x31}})
        proposal = report["profile_proposal"]
        self.assertEqual(proposal["status"], "proposed")
        self.assertEqual(proposal["source_bus"], "CAN_BUS_VEH (TWAI, CSV bus 1)")
        self.assertEqual(proposal["stalk_byte"], 3)
        self.assertEqual(proposal["stalk_mask"], "0x03")
        self.assertEqual(proposal["stalk_fwd_value"], 1)
        self.assertEqual(proposal["stalk_rwd_value"], 2)
        self.assertTrue(proposal["matches_design_assumption_F1_R2"])
        self.assertFalse(proposal["matches_wheel_dnd_enumeration_F3_R4"])
        self.assertEqual(proposal["cancel_evidence_ap_states_observed"], [1, 2])
        self.assertEqual(proposal["post_qual_ap_states_observed"], [2, 3])
        self.assertEqual(proposal["timing_notes"]["stalk_release_gap_ms_candidate"], 100)
        transitions = report["intent_0x3ee_timeline"]["intent_transitions"]
        self.assertEqual(len(transitions), 2)
        self.assertEqual(transitions[0], {"ts_ms": 3400, "from": False, "to": True})

    def test_wheel_only_capture_degrades_without_ap_frames(self):
        rows = build_rows(
            total_ms=2000,
            stalk_presses=[(1500, 2, 0x31)],
            ap_transitions=[],
            intent_edges=None,
            with_ap=False,
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "wheel_only.csv"
            write_csv(path, rows)
            report = MODULE.analyze(path)
        self.assertEqual(len(report["press_events"]), 1)
        self.assertEqual(report["press_events"][0]["gesture_hypothesis"], "ambiguous")
        proposal = report["profile_proposal"]
        self.assertEqual(proposal["status"], "incomplete_capture")
        self.assertEqual(proposal["reason"], "no forward-correlated events")
        self.assertNotIn("stalk_byte", proposal)

    def test_rearward_press_at_ap_available_engaging_3_is_a_request(self):
        # Real-vehicle 2026-09-10 captures: the RWD press typically lands
        # while AP already reports 2 (AVAILABLE) and engages 3 afterwards.
        # The frozen qualification set admits 2, so ap_before=2 must still
        # classify as rearward_request, not ambiguous.
        rows = build_rows(
            total_ms=4000,
            stalk_presses=[(1500, 2, 0x31), (3000, 2, 0x32)],
            ap_transitions=[(1700, 2), (1900, 1), (2900, 2), (3500, 3)],
            intent_edges=None,
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "ap2_request.csv"
            write_csv(path, rows)
            report = MODULE.analyze(path)
        hypotheses = [event["gesture_hypothesis"] for event in report["press_events"]]
        self.assertEqual(hypotheses, ["forward_cancel", "rearward_request"])
        self.assertEqual(report["press_events"][1]["ap_before"], 2)
        self.assertEqual(report["press_events"][1]["ap_start_after"], [3])
        proposal = report["profile_proposal"]
        self.assertEqual(proposal["status"], "proposed")
        self.assertEqual(proposal["stalk_fwd_value"], 1)
        self.assertEqual(proposal["stalk_rwd_value"], 2)
        self.assertEqual(proposal["post_qual_ap_states_observed"], [3])

    def test_rise_after_next_press_is_not_attributed_to_earlier_gesture(self):
        # Real-vehicle 2026-09-10 capture: FWD (0x31) pressed while AP=2
        # (no effect), then RWD (0x32) pressed ~1.7s later which engaged
        # AP=3. The engagement must credit the RWD press, not leak into the
        # earlier FWD press's 3s window and misclassify it as a request.
        rows = build_rows(
            total_ms=6000,
            stalk_presses=[(1500, 2, 0x31), (3200, 2, 0x32)],
            ap_transitions=[(0, 2), (3600, 3)],
            intent_edges=None,
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "attribution.csv"
            write_csv(path, rows)
            report = MODULE.analyze(path)
        hypotheses = [event["gesture_hypothesis"] for event in report["press_events"]]
        # The FWD press sat at AP=2 with no rise before the next press.
        self.assertEqual(hypotheses, ["ambiguous", "rearward_request"])
        self.assertEqual(report["press_events"][1]["ap_start_after"], [3])
        proposal = report["profile_proposal"]
        self.assertEqual(proposal["status"], "incomplete_capture")
        self.assertEqual(proposal["reason"], "no forward-correlated events")

    def test_conflicting_forward_values_block_proposal(self):
        rows = build_rows(
            total_ms=6500,
            stalk_presses=[(1500, 2, 0x31), (3000, 2, 0x33), (5000, 2, 0x32)],
            ap_transitions=[
                (1700, 2),
                (1900, 1),
                (2500, 3),
                (3200, 2),
                (3400, 1),
                (5300, 2),
                (5500, 3),
            ],
            intent_edges=None,
        )
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "conflict.csv"
            write_csv(path, rows)
            report = MODULE.analyze(path)
        hypotheses = [event["gesture_hypothesis"] for event in report["press_events"]]
        self.assertEqual(hypotheses, ["forward_cancel", "forward_cancel", "rearward_request"])
        proposal = report["profile_proposal"]
        self.assertEqual(proposal["status"], "inconsistent_values")
        self.assertNotIn("stalk_fwd_value", proposal)


if __name__ == "__main__":
    unittest.main()
