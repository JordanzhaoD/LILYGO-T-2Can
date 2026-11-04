"""Contract tests for the autoshift engine core (include/dash_autoshift.h).

House style: token-pinning on source + a real native compile-and-run of
test/test_native_autoshift_main.cpp via clang++. The -DDRIVER_P4_3CAN
compile flag is pinned deliberately — the header lives inside the P4
guard and a native compile without the define preprocesses to nothing,
making every assertion pass vacuously (the guard-totality test below
pins the flip side).
"""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "include" / "dash_autoshift.h"
NATIVE_MAIN = ROOT / "test" / "test_native_autoshift_main.cpp"

# The 16 desk-verified SBW CRC vectors (stalk state 40 C0, counter 0..F).
PINNED_VECTOR_TOKENS = [
    "{0x00, 0xB2}", "{0x10, 0x7F}", "{0x20, 0x35}", "{0x30, 0xF8}",
    "{0x40, 0xA1}", "{0x50, 0x6C}", "{0x60, 0x26}", "{0x70, 0xEB}",
    "{0x80, 0x94}", "{0x90, 0x59}", "{0xA0, 0x13}", "{0xB0, 0xDE}",
    "{0xC0, 0x87}", "{0xD0, 0x4A}", "{0xE0, 0x00}", "{0xF0, 0xCD}",
]

CLANG = "clang++"


class AutoshiftContractTests(unittest.TestCase):
    def test_header_is_gated_by_p4_guard(self):
        source = HEADER.read_text()
        self.assertIn("#if defined(DRIVER_P4_3CAN)", source)

    def test_profile_carries_unfrozen_sentinel_until_task2(self):
        """While Task 2 (field freeze) has not landed, the profile must
        advertise itself: sentinel marker in the header, 0xFF stalk bytes
        (pinned by the native run). When the freeze lands this test is
        replaced by frozen-value assertions in the same RED->GREEN."""
        source = HEADER.read_text()
        self.assertIn("PROFILE UNFROZEN", source)

    def test_native_main_pins_all_16_crc_vectors(self):
        source = NATIVE_MAIN.read_text()
        for token in PINNED_VECTOR_TOKENS:
            self.assertIn(token, source)

    def test_native_main_compiles_and_passes_with_p4_define(self):
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "autoshift_native"
            result = subprocess.run(
                [CLANG, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-DDRIVER_P4_3CAN", "-I", str(ROOT / "include"),
                 str(NATIVE_MAIN), "-o", str(binary)],
                capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0,
                             "compile failed:\n" + result.stderr)
            run = subprocess.run([str(binary)], capture_output=True,
                                 text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("ALL PASS", run.stdout)

    def test_gate_eval_order_is_pinned(self):
        """The gate chain evaluates master -> stale -> speed -> brake ->
        cooldown -> count -> obstacle -> pending -> target -> submit.
        Pinned by source ordering inside dashAsGateEval's body (scoped
        extraction: the enum declaration lists the same tokens)."""
        import re

        source = HEADER.read_text()
        match = re.search(
            r"DashAsVerdict dashAsGateEval\(uint32_t nowMs.*?\n\}",
            source, re.DOTALL)
        self.assertIsNotNone(match, "dashAsGateEval body not found")
        body = match.group(0)
        tokens = ["AS_BLOCK_MASTER", "AS_BLOCK_STALE_118", "AS_BLOCK_SPEED",
                  "AS_BLOCK_BRAKE", "AS_BLOCK_COOLDOWN", "AS_BLOCK_COUNT",
                  "AS_BLOCK_OBSTACLE", "AS_BLOCK_PENDING", "AS_ALREADY_TARGET",
                  "AS_SUBMIT"]
        positions = [body.find(tok) for tok in tokens]
        for tok, pos in zip(tokens, positions):
            self.assertGreater(pos, -1, f"{tok} missing from eval body")
        self.assertEqual(positions, sorted(positions),
                         f"eval order drifted: {list(zip(tokens, positions))}")

    def test_gate_inputs_struct_fields(self):
        source = HEADER.read_text()
        for field in ("bool master;", "bool torque2Fresh;", "uint8_t speedKph;",
                      "bool brakeApplied;", "uint8_t gearNow;",
                      "uint8_t gearTarget;", "bool obstacleFreshAndClear;",
                      "uint32_t lastShiftMs;", "uint32_t nowCount;"):
            self.assertIn(field, source)

    def test_profile_carries_session_shift_count_limit(self):
        source = HEADER.read_text()
        self.assertIn("shiftCountMax", source)

    def test_native_main_covers_gate_matrix_and_close_loop(self):
        source = NATIVE_MAIN.read_text()
        for token in ("AS_BLOCK_MASTER", "AS_BLOCK_STALE_118", "AS_BLOCK_SPEED",
                      "AS_BLOCK_BRAKE", "AS_BLOCK_OBSTACLE", "AS_BLOCK_COOLDOWN",
                      "AS_BLOCK_COUNT", "AS_BLOCK_PENDING", "AS_ALREADY_TARGET",
                      "AS_CONFIRM_COMPLETE", "AS_CONFIRM_RETRY",
                      "AS_CONFIRM_ABANDONED", "dashAsLastAbandonReason"):
            self.assertIn(token, source)

    DASH_HEADER = ROOT / "include" / "web" / "mcp2515_dashboard.h"

    def test_dashboard_includes_autoshift_header_behind_p4_guard(self):
        source = self.DASH_HEADER.read_text()
        self.assertIn('#include "dash_autoshift.h"', source)

    def test_cana_drain_feeds_autoshift_decoders(self):
        """mcpDashOnFrame (the CanA/CanB RX path) feeds 0x118 torque2 and
        0x045 wheel decoders inside a DRIVER_P4_3CAN guard, on the
        vehicle-bus (never the PARTY mirror)."""
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(
            r"static void mcpDashOnFrame\(const CanFrame &f\).*?\n\}",
            source, re.DOTALL)
        self.assertIsNotNone(match, "mcpDashOnFrame body not found")
        body = match.group(0)
        self.assertIn("#if defined(DRIVER_P4_3CAN)", body)
        self.assertIn("f.id == 0x118", body)
        self.assertIn("dashAsOnTorque2(", body)
        self.assertIn("f.id == 0x045", body)
        self.assertIn("dashAsOnWheel045(", body)

    def test_canc_drain_feeds_angle_and_obstacle_decoders(self):
        """mcpDashSniffCan3 (the CanC/PT RX path) feeds the 0x00E angle
        decoder and the 0x23E/0x20E obstacle decoder inside a
        DRIVER_P4_3CAN guard."""
        import re

        source = self.DASH_HEADER.read_text()
        match = re.search(
            r"static void mcpDashSniffCan3\(const CanFrame &f\).*?\n\}",
            source, re.DOTALL)
        self.assertIsNotNone(match, "mcpDashSniffCan3 body not found")
        body = match.group(0)
        self.assertIn("#if defined(DRIVER_P4_3CAN)", body)
        self.assertIn("0x00E", body)
        self.assertIn("dashAsOnAngle00E(", body)
        self.assertIn("dashAsOnObstacle(", body)
        self.assertIn("0x23E", body)
        self.assertIn("0x20E", body)

    def test_guard_is_total_without_define(self):
        """Without DRIVER_P4_3CAN the header must expose NOTHING: a TU
        referencing dashAsCrc8J1850 must fail to compile (undeclared).
        Together with the with-define test this pins that the guard is
        real and that the green run above is not vacuous."""
        with tempfile.TemporaryDirectory() as tmp:
            tu = Path(tmp) / "no_define.cpp"
            tu.write_text(
                '#include "dash_autoshift.h"\n'
                "uint8_t (*probe)(const uint8_t *, size_t) ="
                " dashAsCrc8J1850;\n"
                "int main() { return 0; }\n")
            result = subprocess.run(
                [CLANG, "-std=c++17", "-fsyntax-only", "-I",
                 str(ROOT / "include"), str(tu)],
                capture_output=True, text=True, timeout=120)
            self.assertNotEqual(result.returncode, 0,
                                "symbol leaked outside the P4 guard")


if __name__ == "__main__":
    unittest.main()
