"""Contract tests for the P4 stability baseline bus_enable mask (route A).

Crash forensics rounds 1-10 (2026-11-01..03, see
docs/verification/p4-3can-expl-round10-forensics-2026-11-03.md) concluded the
in-car crash family is platform-level register/control-flow corruption whose
delay scales with TWAI interrupt load: T2CAN (1 bus) is field-stable for
months, the P4 board (3 buses) crashes in seconds-to-minutes, and the bench
(silent buses) never crashes. Route A ships stability by subtraction: the
baseline runs only CanA — the proven regime — and CanB/CanC light back up
one per release, validated in the car each time.

These tests pin the contract so the safety property cannot rot:
- the mask defaults to CanA-only (0x01) and CanA can never be disabled,
- boot gates CanB/CanC construction on the mask (disabled controllers never
  install their ISRs), while the T2CAN dual build keeps its unconditional
  behavior,
- every consumer of the optional drivers null-guards (drain, TX helper,
  health tick, onSendFrame wiring) so a disabled lane no-ops safely,
- DIAG `bus_enable` (target-only) gets/sets the NVS value and requires a
  reboot to apply — v0.1 never constructs/destructs drivers at runtime.
"""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"
DASH = ROOT / "include" / "web" / "mcp2515_dashboard.h"


def _body(source: str, start_marker: str, end_marker: str) -> str:
    start = source.index(start_marker)
    end = source.index(end_marker, start)
    return source[start:end]


class BusMaskLogicContractTests(unittest.TestCase):
    """Mask semantics live in main.cpp and must stay P4-only."""

    @classmethod
    def setUpClass(cls):
        cls.main = MAIN.read_text()

    def test_default_mask_is_cana_only(self):
        self.assertIn("T2CAN_BUS_ENABLE_DEFAULT = 0x01", self.main)

    def test_normalize_forces_cana_bit(self):
        body = _body(self.main, "uint8_t t2canBusEnableNormalize(", "\n}")
        self.assertIn("| 0x01", body)

    def test_get_reads_nvs_namespace_t2can_key_busmask(self):
        body = _body(self.main, "uint8_t t2canBusEnableGet(void)", "\n}\n")
        self.assertIn('nvs_open("t2can"', body)
        self.assertIn('"busmask"', body)

    def test_get_falls_back_to_default_when_nvs_absent(self):
        body = _body(self.main, "uint8_t t2canBusEnableGet(void)", "\n}\n")
        self.assertIn("T2CAN_BUS_ENABLE_DEFAULT", body)
        # stored value is read into a default-initialized local; a missing key
        # keeps the default instead of leaking an uninitialized read.
        self.assertRegex(body, r"nvs_get_u8\(.+busmask.+stored\)")

    def test_set_persists_and_commits(self):
        body = _body(self.main, "bool t2canBusEnableSet(", "\n}\n")
        self.assertIn("NVS_READWRITE", body)
        self.assertIn("nvs_set_u8", body)
        self.assertIn("nvs_commit", body)

    def test_mask_helpers_are_p4_gated(self):
        body = _body(self.main, "#ifdef DRIVER_P4_3CAN\n// Stability baseline", "\n#endif\nstatic void t2canSetupSecondary")
        self.assertIn("t2canBusEnableNormalize", body)
        self.assertIn("t2canBusEnableGet", body)
        self.assertIn("t2canBusEnableSet", body)


class BootGatingContractTests(unittest.TestCase):
    """Disabled controllers must never be constructed (no ISR install)."""

    @classmethod
    def setUpClass(cls):
        cls.main = MAIN.read_text()

    def test_boot_reads_mask_once_and_logs_it(self):
        body = _body(self.main, "#if defined(DRIVER_T2CAN_DUAL) || defined(DRIVER_P4_3CAN)\n#ifdef DRIVER_P4_3CAN", "#endif")
        self.assertIn("t2canBusEnableGet()", body)
        self.assertIn("[BUS] bus_enable", body)

    def test_canb_gated_on_bit1_with_explicit_disabled_log(self):
        body = _body(self.main, "#ifdef DRIVER_P4_3CAN\n    // Stability baseline", "#endif")
        self.assertRegex(body, r"if \(appBusEnable & 0x02\)\s*\n\s*t2canSetupSecondary\(\);")
        self.assertIn("CAN B (TWAI1) disabled by bus_enable", body)

    def test_canc_gated_on_bit2_with_explicit_disabled_log(self):
        body = _body(self.main, "#ifdef DRIVER_P4_3CAN\n    if (appBusEnable & 0x04)", "#endif")
        self.assertIn("t2canSetupCan3();", body)
        self.assertIn("CAN C (TWAI2) disabled by bus_enable", body)

    def test_t2can_dual_keeps_unconditional_secondary_setup(self):
        # The #else branch of the P4 gate must keep calling the setup so the
        # T2CAN dual build is untouched by this change. (Two-line start marker:
        # the same combined gate also opens the driver-type block near the top
        # of main.cpp, this one is the boot setup.)
        body = _body(self.main, "#if defined(DRIVER_T2CAN_DUAL) || defined(DRIVER_P4_3CAN)\n#ifdef DRIVER_P4_3CAN", "#endif\n#ifdef DRIVER_P4_3CAN\n    if (appBusEnable & 0x04")
        self.assertRegex(body, r"#else\n    t2canSetupSecondary\(\);\n#endif")

    def test_onsendframe_wiring_still_null_guarded(self):
        self.assertRegex(
            self.main,
            r"if \(appDriverSecondary && dashDriver\)\s*\n\s*appDriverSecondary->onSendFrame",
        )


class DisabledLaneSafetyContractTests(unittest.TestCase):
    """Every consumer of the optional drivers must null-guard."""

    @classmethod
    def setUpClass(cls):
        cls.main = MAIN.read_text()

    def test_drain_secondary_guards_null(self):
        body = _body(self.main, "static void t2canDrainSecondary()", "\n{")
        idx = self.main.index("static void t2canDrainSecondary()")
        fn = self.main[idx : self.main.index("\n}", idx)]
        self.assertRegex(fn, r"if \(!appDriverSecondary\)\s*\n\s*return;")

    def test_drain_can3_guards_null(self):
        idx = self.main.index("static void t2canDrainCan3()")
        fn = self.main[idx : self.main.index("\n}", idx)]
        self.assertRegex(fn, r"if \(!appDriverTertiary\)\s*\n\s*return;")

    def test_tx_helper_guards_null(self):
        idx = self.main.index("static bool t2canTxSecondaryCounted(")
        fn = self.main[idx : self.main.index("\n}", idx)]
        self.assertRegex(fn, r"if \(!appDriverSecondary\)\s*\n\s*return false;")

    def test_health_tick_guards_null(self):
        idx = self.main.index("static void t2canBus2HealthTick()")
        fn = self.main[idx : self.main.index("\n}", idx)]
        self.assertRegex(fn, r"if \(!appDriverSecondary\)\s*\n\s*return;")


class BusEnableDiagContractTests(unittest.TestCase):
    """DIAG `bus_enable` — target-only get/set with reboot-to-apply."""

    @classmethod
    def setUpClass(cls):
        cls.dash = DASH.read_text()

    def test_prototypes_declared_with_can3_counters(self):
        body = _body(self.dash, "uint32_t t2canCan3RxCount(void);", "#endif")
        self.assertIn("uint8_t t2canBusEnableGet(void);", body)
        self.assertIn("bool t2canBusEnableSet(uint8_t", body)

    def test_help_entry_present_and_p4_target_gated(self):
        body = _body(self.dash, '#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)\n    Serial.println("  exp_b', "#endif")
        self.assertIn("bus_enable", body)

    def test_dispatch_validates_range_and_requires_reboot(self):
        idx = self.dash.index('strncmp(start, "bus_enable", 10)')
        fn = self.dash[idx - 400 : idx + 900]
        self.assertIn("t2canBusEnableSet", fn)
        self.assertIn("reboot to apply", fn)
        # values outside 1..7 must be rejected (0 would even try to disable
        # CanA; >7 sets undefined bits)
        self.assertIn("> 7", fn)


if __name__ == "__main__":
    unittest.main()
