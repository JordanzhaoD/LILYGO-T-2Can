"""Contract tests for Ban Shield X (0x368 forge + tripwire).

House style (mirror test_autoshift_contract.py): token-pinning on source
plus a real native compile-and-run of test/test_native_ban_forge_main.cpp
via clang++ with -DDRIVER_P4_3CAN (the header lives inside the P4 guard).

Task 1 owns only test_v2_tx_submit_maps_pt_bus — the v2 blackbox TX
submit must tag CanC (CAN_BUS_PT) frames as bus C. Without the 3-way
mapping, every forged/TX row on the PT bus is mis-tagged bus A and the
Stage-B evidence chain (which arm ran, which bus carried the injection)
is unreadable.
"""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "include" / "dash_ban_forge.h"
DASHBOARD = ROOT / "include" / "web" / "mcp2515_dashboard.h"
NATIVE_MAIN = ROOT / "test" / "test_native_ban_forge_main.cpp"

CLANG = "clang++"


class BanForgeContractTests(unittest.TestCase):
    def test_v2_tx_submit_maps_pt_bus(self):
        """The mcpDashOnTxFrame v2 submit must map CAN_BUS_PT to
        kRecV2BusC before falling back to the PARTY/A split, matching the
        v1 CSV export's 3-way mapping. Pinned on the function body so a
        matching token elsewhere in the dashboard header cannot
        satisfy it vacuously."""
        import re

        source = DASHBOARD.read_text()
        match = re.search(
            r"static void mcpDashOnTxFrame\(const CanFrame &frame, bool ok\).*?\n\}",
            source, re.DOTALL)
        self.assertIsNotNone(match, "mcpDashOnTxFrame body not found")
        body = match.group(0)
        self.assertIn(
            "frame.bus == CAN_BUS_PT ? kRecV2BusC",
            body,
            "v2 TX submit does not map CAN_BUS_PT to kRecV2BusC — "
            "CanC TX rows are mis-tagged as bus A")

    def test_header_is_gated_by_p4_guard(self):
        """The core lives inside the DRIVER_P4_3CAN guard so non-P4
        builds stay byte-identical (T2CAN red line); the guard test
        also proves the header exists and has content."""
        source = HEADER.read_text()
        self.assertIn("#if defined(DRIVER_P4_3CAN)", source)
        self.assertIn("#endif", source)

    def test_native_main_compiles_and_passes_with_p4_define(self):
        """Real native compile-and-run of the full state machine:
        template check, seven transforms, budget/wallclock/cooldown,
        echo-muted tripwire, pref fail-closed. -DDRIVER_P4_3CAN is
        mandatory — without it the header preprocesses to nothing and
        every assertion would pass vacuously."""
        with tempfile.TemporaryDirectory() as tmp:
            binary = Path(tmp) / "banforge_native"
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


if __name__ == "__main__":
    unittest.main()
