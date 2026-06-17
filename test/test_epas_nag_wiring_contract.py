import unittest
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = (ROOT / "include" / "handlers.h").read_text(encoding="utf-8")
D = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(encoding="utf-8")


class WiringContract(unittest.TestCase):
    def test_include_dash_epas_nag_h(self):
        self.assertIn('#include "dash_epas_nag.h"', D)

    def test_globals_declared(self):
        self.assertIn("dashEpasNagEnabled", D)
        self.assertIn("dashEpasNagCtrl", D)
        self.assertIn("DashEpasNagEngine dashEpasNagCtrl", D)

    def test_glue_function_defined_and_called(self):
        self.assertIn("void dashEpasNagOnFrame(const CanFrame &f)", D)
        # called from mcpDashOnFrame: the call must appear after the function header.
        # (We can't use a simple "no } before call" guard because the body legitimately
        #  contains inner blocks with braces; instead assert the call site follows the header.)
        head = re.search(r"static void mcpDashOnFrame\([^)]*\)\s*\{", D)
        self.assertIsNotNone(head, "mcpDashOnFrame header must exist")
        body = D[head.end():]
        self.assertIn("dashEpasNagOnFrame(f)", body, "dashEpasNagOnFrame(f) must be called from within mcpDashOnFrame")

    def test_runtime_gate_set_in_apply_runtime_state(self):
        self.assertRegex(D, r"dashHandler->epasNagActive\s*=\s*dashEpasNagEnabled\s*&&\s*canActive\s*&&\s*dashDefenseEnabled")

    def test_epas_nag_active_field_on_car_manager_base(self):
        self.assertTrue(
            re.search(r"struct CarManagerBase(?:(?!\nstruct ).)*?Shared<bool> epasNagActive\{false\}", H, re.S),
            "CarManagerBase must declare Shared<bool> epasNagActive{false}",
        )

    def test_bit19_gated_by_epas_nag_active(self):
        # both HW3 and HW4 mux1 nag blocks must include the !epasNagActive gate
        self.assertEqual(D.count("!(bool)epasNagActive"), 0)  # gating lives in handlers.h
        self.assertGreaterEqual(H.count("!(bool)epasNagActive"), 2)

    def _hw4_handler_block(self):
        # Extract the HW4Handler struct body from handlers.h. Robust against the
        # current file style where the struct is declared as `struct HW4Handler ... {`
        # and ends at the matching closing brace at column-0 indentation level.
        m = re.search(r"struct\s+HW4Handler\b[^{]*\{", H)
        self.assertIsNotNone(m, "HW4Handler struct must exist in handlers.h")
        start = m.end()
        depth = 1
        i = start
        while i < len(H) and depth > 0:
            if H[i] == "{":
                depth += 1
            elif H[i] == "}":
                depth -= 1
            i += 1
        self.assertEqual(depth, 0, "HW4Handler struct braces must be balanced")
        return H[start:i - 1]

    def test_hw4_bit47_fsd_ready_decoupled_from_nag(self):
        """D7: HW4 bit-47 (FSD-ready) must be set unconditionally within mux1;
        only the bit-19 nag clear may be gated by !epasNagActive.

        Regression guard: previously the whole HW4 mux1 block was gated by
        !(bool)epasNagActive, which silently dropped bit-47 when the EPAS-faithful
        nag engine activated. This test would have caught that.
        """
        hw4 = self._hw4_handler_block()

        # (a) bit-47 FSD-ready set must still exist inside HW4.
        self.assertIn(
            "setBit(frame, 47, true)", hw4,
            "HW4Handler must set bit 47 (FSD-ready) in mux1",
        )

        # (b) The HW4 mux1 OUTER `index == 1` condition must NOT contain
        #     epasNagActive — so the block always runs and bit-47 is always set.
        mux1 = re.search(
            r"if\s*\(\s*index\s*==\s*1\b[^{]*\{",
            hw4,
        )
        self.assertIsNotNone(mux1, "HW4Handler must have an `index == 1` mux1 block")
        self.assertNotIn(
            "epasNagActive", mux1.group(0),
            "HW4 mux1 outer condition must NOT reference epasNagActive "
            "(bit-47 FSD-ready is independent of the nag engine)",
        )

        # (c) There MUST be an inner `if (!(bool)epasNagActive)` inside the HW4
        #     block so the bit-19 nag clear alone is gated (D7).
        self.assertRegex(
            hw4, r"if\s*\(\s*!\s*\(\s*bool\s*\)\s*epasNagActive\s*\)",
            "HW4 mux1 must inner-gate the bit-19 nag clear with !(bool)epasNagActive",
        )


if __name__ == "__main__":
    unittest.main()
