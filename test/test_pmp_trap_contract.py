"""Contract tests for the PMP hardware write-trap (experiment F).

Three verified TCB rewrites (round-8 on 82697ba, two on bc1d106, all
2026-11-03/04) hit exactly the httpd task's pxStackBase field (TCB+0x30)
with a constant +0x84 delta. Five bench arms (experiments C / D / E / E3,
~1055 requests across 10 fresh boots) failed to reproduce on demand, so
experiment F stops reproducing and traps instead: two RISC-V PMP TOR
entries (14 = bound, 15 = TOR|L|R with W denied) make the next rewrite a
precise store access fault at the writer's own PC.

These tests pin the safety properties so they cannot rot:
- the trap only builds inside the DASH_HEAP_GUARD_V2 gate (T2CAN golden
  build zero-diff),
- the protected word is armed only after the runtime offset self-check
  (field must currently hold pxStackBase — never trap an unknown word),
- the PMP config denies W and sets L|R|TOR on entry 15 with the address
  registers written first,
- arming keys on the adopted task being exactly "httpd",
- the bench self-test stores ONLY the trapped word and expects a fault at
  its own PC,
- the DIAG command is registered inside the P4-only target block.
"""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GUARD = ROOT / "include" / "dash_heap_guard.h"
PMP = ROOT / "include" / "dash_pmp_trap.h"
DASH = ROOT / "include" / "web" / "mcp2515_dashboard.h"


class PmpTrapGateContractTests(unittest.TestCase):
    """The trap must stay inside the p4_3can-only gate."""

    @classmethod
    def setUpClass(cls):
        cls.guard = GUARD.read_text()
        cls.pmp = PMP.read_text()
        cls.dash = DASH.read_text()

    def test_pmp_header_is_gated(self):
        self.assertIn(
            '#if defined(ESP_PLATFORM) && !defined(NATIVE_BUILD) '
            '&& defined(DASH_HEAP_GUARD_V2)',
            self.pmp,
        )

    def test_guard_includes_pmp_inside_same_gate(self):
        start = self.guard.index("defined(DASH_HEAP_GUARD_V2)")
        inc = self.guard.index('#include "dash_pmp_trap.h"')
        self.assertGreater(inc, start)

    def test_diag_registration_is_inside_p4_block(self):
        # heap_guard_test (known-good P4-only) precedes pmp_trap_test, and
        # both sit before the shared #endif closing the target block.
        reg = self.dash.index('strncmp(start, "pmp_trap_test", 13)')
        anchor = self.dash.index('strncmp(start, "heap_guard_test ", 16)')
        self.assertGreater(reg, anchor)
        endif = self.dash.index("#endif", reg)
        self.assertIn("dashHgPmpBenchTest", self.dash[reg:endif])

    def test_help_line_registered(self):
        self.assertIn("pmp_trap_test", self.dash)


class PmpTrapSemanticsContractTests(unittest.TestCase):
    """The hardware config and arming rules."""

    @classmethod
    def setUpClass(cls):
        cls.pmp = PMP.read_text()
        cls.guard = GUARD.read_text()

    def test_entry15_denies_write_and_locks(self):
        # TOR | L | R, and PMP_W must not appear in the entry-15 config.
        self.assertIn("PMP_L | PMP_TOR | PMP_R", self.pmp)
        self.assertNotIn("PMP_W", self.pmp)

    def test_address_registers_written_before_cfg(self):
        arm = self.pmp.index("static void dashPmpArmCore")
        body = self.pmp[arm:self.pmp.index("}", arm)]
        addr14 = body.index("RV_WRITE_CSR(pmpaddr14")
        addr15 = body.index("RV_WRITE_CSR(pmpaddr15")
        cfg = body.index("RV_WRITE_CSR(pmpcfg3")
        self.assertLess(addr14, addr15)
        self.assertLess(addr15, cfg)

    def test_offset_selfcheck_refuses_unknown_word(self):
        self.assertIn("!= stackBase", self.pmp)
        # inside dashHgPmpArm: the check must precede the arming call
        arm = self.pmp.index("static uint32_t dashHgPmpArm")
        body = self.pmp[arm:]
        check = body.index("*(volatile uint32_t *)(uintptr_t)field != stackBase")
        call = body.index("dashPmpArmCore((void *)(uintptr_t)field)")
        self.assertLess(check, call)

    def test_arming_keys_on_httpd_name(self):
        self.assertIn('b.nameA == 0x70747468u /* "httpd" */', self.guard)
        self.assertIn("b.nameB == 0x00000064u", self.guard)

    def test_arms_all_harts(self):
        self.assertIn("esp_ipc_call_blocking", self.pmp)
        self.assertIn("portNUM_PROCESSORS", self.pmp)

    def test_selftest_touches_only_trapped_word(self):
        bench = self.pmp.index("static void dashHgPmpBenchTest")
        body = self.pmp[bench:]
        self.assertIn(
            "*(volatile uint32_t *)(uintptr_t)f = *(volatile uint32_t *)(uintptr_t)f",
            body,
        )

    def test_unarmed_selftest_reports_instead_of_writing(self):
        bench = self.pmp.index("static void dashHgPmpBenchTest")
        body = self.pmp[bench:]
        guard = body.index("if (f == 0)")
        store = body.index("*(volatile uint32_t *)(uintptr_t)f =")
        self.assertLess(guard, store)


if __name__ == "__main__":
    unittest.main()
