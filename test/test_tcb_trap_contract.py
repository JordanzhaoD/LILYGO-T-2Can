"""Contract tests for the TCB hardware write-trap (experiment F, v2).

Three verified TCB rewrites (round-8 on 82697ba, two on bc1d106, all
2026-11-03/04) hit exactly the httpd task's pxStackBase field (TCB+0x30)
with a constant +0x84 delta. Five bench arms (experiments C / D / E / E3,
~1055 requests across 10 fresh boots) failed to reproduce on demand, so
experiment F stops reproducing and traps instead.

v1 used RISC-V PMP TOR entries and was killed by on-chip evidence on
2026-11-04: the boot-time pmpcfg0..3 readback (0x9d9d9b9b / 0x808b8d80 /
0x8b808b8d / 0x80001d1d) showed entry0-11 locked by IDF startup memory
protection, entry15 locked-empty (writes silently rejected), and
entry12-14 borrowed at runtime by system code. No usable entry.

v2 uses the trigger module via the official esp_cpu_set_watchpoint() API
(slot 0, 4-byte NAPOT, store-only, armed on both harts): the next rewrite
raises a breakpoint exception at the writer's own PC before the store
commits.

These tests pin the safety properties so they cannot rot:
- the trap only builds inside the DASH_HEAP_GUARD_V2 gate (T2CAN golden
  build zero-diff),
- the protected word is armed only after the runtime offset self-check
  (field must currently hold pxStackBase — never trap an unknown word),
- the watchpoint is store-only via the official API on slot 0,
- arming is only claimed after the WARL readback verifies BOTH harts
  (v1 lesson: a successful CSR write is not proof the trap is live),
- arming keys on the adopted task being exactly "httpd",
- the bench self-test stores ONLY the trapped word and expects a fault at
  its own PC,
- the DIAG command is registered inside the P4-only target block.
"""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GUARD = ROOT / "include" / "dash_heap_guard.h"
TRAP = ROOT / "include" / "dash_tcb_trap.h"
DASH = ROOT / "include" / "web" / "mcp2515_dashboard.h"


class TcbTrapGateContractTests(unittest.TestCase):
    """The trap must stay inside the p4_3can-only gate."""

    @classmethod
    def setUpClass(cls):
        cls.guard = GUARD.read_text()
        cls.trap = TRAP.read_text()
        cls.dash = DASH.read_text()

    def test_trap_header_is_gated(self):
        self.assertIn(
            '#if defined(ESP_PLATFORM) && !defined(NATIVE_BUILD) '
            '&& defined(DASH_HEAP_GUARD_V2)',
            self.trap,
        )

    def test_guard_includes_trap_inside_same_gate(self):
        start = self.guard.index("defined(DASH_HEAP_GUARD_V2)")
        inc = self.guard.index('#include "dash_tcb_trap.h"')
        self.assertGreater(inc, start)

    def test_no_pmp_remnants(self):
        # v1 is dead; the PMP mechanism must not linger under a new name.
        self.assertNotIn("dash_pmp_trap.h", self.guard)
        self.assertNotIn("pmp_trap_test", self.dash)
        self.assertNotIn("dashHgPmpArm", self.guard)

    def test_diag_registration_is_inside_p4_block(self):
        # heap_guard_test (known-good P4-only) precedes tcb_trap_test, and
        # both sit before the shared #endif closing the target block.
        reg = self.dash.index('strncmp(start, "tcb_trap_test", 13)')
        anchor = self.dash.index('strncmp(start, "heap_guard_test ", 16)')
        self.assertGreater(reg, anchor)
        endif = self.dash.index("#endif", reg)
        self.assertIn("dashHgTrapBenchTest", self.dash[reg:endif])

    def test_help_line_registered(self):
        self.assertIn("tcb_trap_test", self.dash)


class TcbTrapSemanticsContractTests(unittest.TestCase):
    """The watchpoint config and arming rules."""

    @classmethod
    def setUpClass(cls):
        cls.trap = TRAP.read_text()
        cls.guard = GUARD.read_text()

    def test_uses_official_watchpoint_api_store_only(self):
        self.assertIn("esp_cpu_set_watchpoint(DASH_TCB_TRAP_SLOT", self.trap)
        self.assertIn("ESP_CPU_WATCHPOINT_STORE", self.trap)
        self.assertNotIn("ESP_CPU_WATCHPOINT_ACCESS", self.trap)
        self.assertNotIn("ESP_CPU_WATCHPOINT_LOAD", self.trap)

    def test_slot_is_zero(self):
        self.assertIn("#define DASH_TCB_TRAP_SLOT 0", self.trap)

    def test_readback_verify_before_claiming_armed(self):
        # dashHgTrapField is assigned only after dashHgTrapOkMask == all
        # harts — the v1 lesson (write accepted != trap live) made law.
        arm = self.trap.index("static uint32_t dashHgTcbTrapArm")
        body = self.trap[arm:]
        ok = body.index("dashHgTrapOkMask != all")
        claim = body.index("dashHgTrapField = field;")
        self.assertLess(ok, claim)

    def test_offset_selfcheck_refuses_unknown_word(self):
        self.assertIn("!= stackBase", self.trap)
        arm = self.trap.index("static uint32_t dashHgTcbTrapArm")
        body = self.trap[arm:]
        check = body.index("*(volatile uint32_t *)(uintptr_t)field != stackBase")
        call = body.index("dashTcbTrapArmCore((void *)(uintptr_t)field)")
        self.assertLess(check, call)

    def test_arming_keys_on_httpd_name(self):
        self.assertIn('b.nameA == 0x70747468u /* "httpd" */', self.guard)
        self.assertIn("b.nameB == 0x00000064u", self.guard)

    def test_arms_all_harts(self):
        self.assertIn("esp_ipc_call_blocking", self.trap)
        self.assertIn("portNUM_PROCESSORS", self.trap)

    def test_readback_checks_type_store_and_machine_bits(self):
        # WARL readback must check tdata1 type==2, STORE and MACHINE bits,
        # and the 4-byte NAPOT pattern in tdata2.
        self.assertIn("(t1 >> 28) == 2u", self.trap)
        self.assertIn("t1 & TDATA1_STORE", self.trap)
        self.assertIn("t1 & TDATA1_MACHINE", self.trap)
        self.assertIn("t2 == ((addr & ~3u) | 1u)", self.trap)

    def test_selftest_touches_only_trapped_word(self):
        bench = self.trap.index("static void dashHgTrapBenchTest")
        body = self.trap[bench:]
        self.assertIn(
            "*(volatile uint32_t *)(uintptr_t)f = *(volatile uint32_t *)(uintptr_t)f",
            body,
        )

    def test_unarmed_selftest_reports_instead_of_writing(self):
        bench = self.trap.index("static void dashHgTrapBenchTest")
        body = self.trap[bench:]
        guard = body.index("if (f == 0)")
        store = body.index("*(volatile uint32_t *)(uintptr_t)f =")
        self.assertLess(guard, store)


if __name__ == "__main__":
    unittest.main()
