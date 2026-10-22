"""Simulator parity guards (2026-10-22 beta04 P0 regression).

Jordan reported three UI bugs during browser acceptance — all three
traced to the simulator mock drifting from the firmware, not to the
firmware itself:

  1. Speed page legacy smart-offset: the four mode cards snap back to
     "off" — the mock never persisted/replayed ``mode`` (firmware
     ``dashLegacySpeedConfigJson()`` does).
  2. FSD-defense page: AP gate OFF showed "已阻断/blocked" — the mock
     mapped ``ap_gate=False`` to ``blocked``, inverting the firmware
     semantics (gate off = pass-through, gate on = wait for AP).
  3. Overview FSD policy: "Tesla Controller 对齐" card snaps back to
     stable — the mock /status had no ``fsdDiag.policy``.

These tests lock the simulator to the firmware shapes so browser
acceptance reflects real behavior.
"""

import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

U = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text(encoding="utf-8")


def load_simulator():
    spec = importlib.util.spec_from_file_location(
        "webui_simulator", ROOT / "scripts" / "webui_simulator.py"
    )
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


SIM = load_simulator()


class LegacySpeedConfigParity(unittest.TestCase):
    """Symptom 1: mode/manual/cp must round-trip like the firmware does."""

    def setUp(self):
        self.st = SIM.SimState()

    def test_mode_roundtrip_via_apply(self):
        # Same form the UI posts in saveLegacySpeedConfig().
        self.st.apply_legacy_speed_form(
            {"mode": "auto", "cp1": "44", "cp2": "22", "cp3": "11", "cp4": "5",
             "manual_offset_kph": "7"}
        )
        cfg = self.st.legacy_speed_config()
        self.assertEqual(cfg.get("mode"), "auto")
        self.assertEqual(cfg.get("manual_offset_kph"), 7)
        self.assertEqual(cfg.get("custom_pct"), [44, 22, 11, 5])

    def test_config_payload_covers_ui_consumed_keys(self):
        # Structural guard: every key applyLegacySpeedConfig() reads from
        # the POST response must exist in the mock payload — otherwise
        # the card snaps back after save.
        cfg = self.st.legacy_speed_config()
        for key in ("mode", "manual_offset_kph", "custom_pct", "bus",
                    "smooth_down", "smooth_rate_kph_s"):
            self.assertIn(key, cfg, f"mock /legacy_speed_config missing {key}")
        self.assertIn("custom_pct", cfg) and self.assertEqual(len(cfg["custom_pct"]), 4)

    def test_do_post_wires_the_apply_helper(self):
        # The HTTP layer must route through the tested helper (and share
        # the payload shape), not duplicate a partial copy.
        src = (ROOT / "scripts" / "webui_simulator.py").read_text(encoding="utf-8")
        self.assertIn("apply_legacy_speed_form", src)
        self.assertIn("STATE.legacy_speed_config()", src)

    def test_ui_still_reads_mode_from_response(self):
        # Contract anchor on the UI side: applyLegacySpeedConfig drives
        # the mode cards from d.mode — keep the two sides pinned.
        self.assertIn("updateLegacySpeedMode(d.mode||'off')", U)


class ApInjectionStateSemantics(unittest.TestCase):
    """Symptom 2: gate OFF must NOT report blocked (firmware semantics)."""

    def setUp(self):
        self.st = SIM.SimState()

    def test_gate_off_is_passthrough_not_blocked(self):
        self.st.ap_gate = False
        state = self.st.status()["apInjectionState"]
        self.assertIn(state, ("armed", "injecting"))
        self.assertNotEqual(state, "blocked")

    def test_gate_on_without_ap_waits_for_ap(self):
        # status() derives apActive from uptime (self.ad toggles every
        # 12s), so drive uptime instead of assigning self.ad directly.
        self.st.ap_gate = True
        self.st.uptime = lambda: 0  # ad False
        self.assertEqual(self.st.status()["apInjectionState"], "waiting_ap")

    def test_gate_on_with_ap_injects(self):
        self.st.ap_gate = True
        self.st.uptime = lambda: 12  # ad True
        self.assertEqual(self.st.status()["apInjectionState"], "injecting")


class FsdDiagParity(unittest.TestCase):
    """Symptom 3: /status must expose fsdDiag.policy like the firmware."""

    def setUp(self):
        self.st = SIM.SimState()

    def test_status_has_fsd_diag_policy(self):
        diag = self.st.status().get("fsdDiag")
        self.assertIsNotNone(diag, "mock /status missing fsdDiag")
        self.assertIn("policy", diag)

    def test_policy_tracks_state(self):
        self.st.legacy_fsd_policy = "legacy_tesla_parity"
        self.assertEqual(self.st.status()["fsdDiag"]["policy"], "legacy_tesla_parity")
        self.st.legacy_fsd_policy = "legacy_stable"
        self.assertEqual(self.st.status()["fsdDiag"]["policy"], "legacy_stable")

    def test_policy_values_match_ui_normalization(self):
        # renderLegacyFsdPolicyCards normalizes legacy_stable->stable and
        # legacy_experimental->experimental; legacy_tesla_parity stays as
        # the data-policy token. The mock must speak the firmware names.
        for policy in ("legacy_stable", "legacy_experimental", "legacy_tesla_parity"):
            self.st.legacy_fsd_policy = policy
            self.assertEqual(self.st.status()["fsdDiag"]["policy"], policy)


if __name__ == "__main__":
    unittest.main()
