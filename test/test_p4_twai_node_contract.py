"""Contract tests for the P4 CAN hot path on the new TWAI node API.

Reference-firmware comparison (docs/verification/albert-p4-reference-comparison-20261104.md,
Jordan-approved 2026-11-04): the stable in-car Albert-P4 build runs the whole
CAN hot path IRAM/cache-safe on the new node API with hot state in internal
RAM and its CAN task on core 1; our legacy-driver build keeps every one of
those in flash cache / core 0 and dies in-car in 12-39 s. These tests pin the
ported mechanism so it cannot rot back:

- sdkconfig.p4_3can enables the new-driver IRAM/cache-safe trio,
- include/drivers/p4_twai_node.h owns an internal-RAM SPSC RX ring + TX
  staging pool, with IRAM callbacks that never block,
- both driver classes delegate their P4 private layer to the core — no
  legacy driver call survives on P4-active lines (T2CAN keeps them),
- can_rt moves to core 1 under DRIVER_P4_3CAN,
- the new-API mask polarity (1 = must match) is inverted from the legacy
  acceptance mask (1 = don't care) at exactly one place, with a marker.

Design: docs/superpowers/specs/2026-11-04-p4-twai-node-hotpath-design.md.
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "src" / "main.cpp"
SDKCONFIG_P4 = ROOT / "sdkconfig.p4_3can"
SDKCONFIG_T2 = ROOT / "sdkconfig.lilygo_t2can_dual"
TWAI_DRIVER = ROOT / "include" / "drivers" / "twai_driver.h"
P4_SECONDARY = ROOT / "include" / "drivers" / "p4_twai_secondary.h"
NODE_CORE = ROOT / "include" / "drivers" / "p4_twai_node.h"

# The only functions allowed to touch the legacy driver are on lines that are
# NOT compiled for the P4 vehicle build (T2CAN dual / native paths).
LEGACY_CALLS = (
    "twai_driver_install(",
    "twai_receive(",
    "twai_transmit(",
    "twai_get_status_info(",
    "twai_start(",
    "twai_stop(",
    "twai_initiate_recovery(",
)


def _p4_active_lines(source: str):
    """Yield (lineno, text, active) for lines compiled for the P4 vehicle build.

    A line is P4-active if it sits inside the then-branch of some '#if' whose
    condition implies the P4 vehicle build — either defined(DRIVER_P4_3CAN)
    or the derived gate macro T2CAN_TWAI_NODE_CORE (defined by twai_driver.h
    right under the DRIVER_P4_3CAN gate) — and not inside that condition's
    #else branch.
    """
    def _implies_p4(cond: str) -> bool:
        return "DRIVER_P4_3CAN" in cond or "T2CAN_TWAI_NODE_CORE" in cond

    stack = []  # each entry: dict(cond_has_p4, in_else)
    for lineno, raw in enumerate(source.splitlines(), start=1):
        stripped = raw.strip()
        m_if = re.match(r"#\s*(if|ifdef)\b(.*)", stripped)
        m_elif = re.match(r"#\s*elif\b(.*)", stripped)
        if m_if:
            stack.append({"p4": _implies_p4(m_if.group(2)), "else": False})
        elif m_elif and stack:
            stack[-1]["p4"] = _implies_p4(m_elif.group(1))
            stack[-1]["else"] = False
        elif re.match(r"#\s*else\b", stripped) and stack:
            stack[-1]["else"] = True
        elif re.match(r"#\s*endif\b", stripped) and stack:
            stack.pop()
        active = any(f["p4"] and not f["else"] for f in stack)
        yield lineno, raw, active


class SdkConfigContractTests(unittest.TestCase):
    """The IRAM/cache-safe trio is the mechanism; it must be pinned on."""

    @classmethod
    def setUpClass(cls):
        cls.cfg = SDKCONFIG_P4.read_text()

    def test_isr_in_iram(self):
        self.assertIn("CONFIG_TWAI_ISR_IN_IRAM=y", self.cfg)

    def test_io_func_in_iram(self):
        self.assertIn("CONFIG_TWAI_IO_FUNC_IN_IRAM=y", self.cfg)

    def test_isr_cache_safe(self):
        # Selects TWAI_ISR_IN_IRAM + TWAI_OBJ_CACHE_SAFE (driver objects in
        # internal RAM) per components/esp_driver_twai/Kconfig.
        self.assertIn("CONFIG_TWAI_ISR_CACHE_SAFE=y", self.cfg)

    def test_t2can_sdkconfig_stays_legacy(self):
        cfg = SDKCONFIG_T2.read_text()
        for flag in ("CONFIG_TWAI_ISR_IN_IRAM=y",
                     "CONFIG_TWAI_IO_FUNC_IN_IRAM=y",
                     "CONFIG_TWAI_ISR_CACHE_SAFE=y"):
            self.assertNotIn(flag, cfg)


class NodeCoreContractTests(unittest.TestCase):
    """p4_twai_node.h — the Albert-mechanism core (P4-only file)."""

    @classmethod
    def setUpClass(cls):
        cls.assertTrue(NODE_CORE.exists(), "include/drivers/p4_twai_node.h missing")
        cls.core = NODE_CORE.read_text()

    def test_whole_file_gated_to_p4_vehicle_build(self):
        gate = "#if defined(DRIVER_P4_3CAN) && !defined(NATIVE_BUILD)"
        head = self.core[: self.core.index("class P4TwaiNodeCore")]
        self.assertIn(gate, head)

    def test_rx_callback_is_iram(self):
        self.assertRegex(self.core, r"IRAM_ATTR\s+bool\s+onRxDone")

    def test_rx_callback_path(self):
        body = self._body("bool onRxDone(twai_node_handle_t")
        self.assertIn("twai_node_receive_from_isr", body)
        self.assertIn("vTaskNotifyGiveFromISR", body)

    def test_rx_callback_never_blocks(self):
        body = self._body("bool onRxDone(twai_node_handle_t")
        for forbidden in ("xSemaphoreTake", "malloc(", "calloc(", "Serial",
                          "printf", "vTaskDelay", "xQueueSend("):
            self.assertNotIn(forbidden, body)

    def test_ring_and_tx_pool_in_internal_ram(self):
        self.assertRegex(self.core, r"MALLOC_CAP_INTERNAL")
        allocs = re.findall(r"heap_caps_(?:calloc|malloc)\([^;]+;", self.core)
        self.assertTrue(allocs, "no heap_caps allocation found")
        for a in allocs:
            self.assertIn("MALLOC_CAP_INTERNAL", a)

    def test_ring_geometry(self):
        self.assertIn("kRingFrames = 128", self.core)

    def test_tx_pool_freed_by_txdone_pointer_match(self):
        body = self._body("bool onTxDone(twai_node_handle_t")
        self.assertIn("done_tx_frame", body)

    def test_tx_transmit_uses_2ms_timeout(self):
        self.assertRegex(self.core, r"twai_node_transmit\([^;]+,\s*2\)")

    def test_node_lifecycle_uses_new_api(self):
        self.assertIn("twai_new_node_onchip(", self.core)
        self.assertIn("twai_node_register_event_callbacks(", self.core)
        self.assertIn("twai_node_enable(", self.core)
        self.assertIn("twai_node_recover(", self.core)

    def test_busoff_cooldown_preserved(self):
        self.assertIn("BUSOFF_COOLDOWN_MS = 1000", self.core)

    def test_mask_polarity_inverted_from_legacy(self):
        # New-API mask bit 1 = must match; legacy acceptance mask 1 = don't
        # care. The core takes the legacy-shaped (base, differ) pair and must
        # pass ~differ, with the marker so nobody "simplifies" it back.
        body = self._body("applyMaskFilter")
        self.assertIn("~differ", body)
        self.assertIn("0x7FF", body)
        self.assertIn("1 = must match", self.core)

    def test_status_synthesis_from_node_info(self):
        body = self._body("statusInfo")
        self.assertIn("twai_node_get_info", body)
        self.assertIn("twai_status_info_t", self.core)

    def _body(self, name):
        start = self.core.index(name)
        end = self.core.index("\n    }", start)
        return self.core[start:end]


class DriverDelegationContractTests(unittest.TestCase):
    """Both driver classes: P4-active lines have zero legacy driver calls."""

    def _assert_split(self, path):
        source = path.read_text()
        legacy_on_p4 = []
        legacy_anywhere = False
        has_core = False
        for _, line, active in _p4_active_lines(source):
            if any(call in line for call in LEGACY_CALLS):
                legacy_anywhere = True
                if active:
                    legacy_on_p4.append(line.strip())
            if "P4TwaiNodeCore" in line and active:
                has_core = True
        self.assertTrue(legacy_anywhere,
                        f"{path.name}: legacy calls vanished even from the "
                         "T2CAN path — T2CAN invariance broken")
        self.assertFalse(legacy_on_p4,
                         f"{path.name}: legacy driver calls on P4-active "
                         f"lines: {legacy_on_p4}")
        self.assertTrue(has_core,
                        f"{path.name}: P4TwaiNodeCore not wired in")
        return source

    def test_primary_driver_splits_cleanly(self):
        src = self._assert_split(TWAI_DRIVER)
        # bench-inject replay hook must survive on the P4 path.
        p4_lines = [l for _, l, a in _p4_active_lines(src) if a]
        self.assertTrue(any("canBenchInjectPop" in l for l in p4_lines))

    def test_secondary_driver_is_pure_node_core(self):
        # p4_twai_secondary.h is a P4-only header (the T2CAN dual build uses
        # the MCP2515 driver): after the mechanism port it must contain ZERO
        # legacy driver calls anywhere and delegate to the node core.
        source = P4_SECONDARY.read_text()
        offenders = [l.strip() for l in source.splitlines()
                     if any(call in l for call in LEGACY_CALLS)]
        self.assertEqual(offenders, [],
                         f"legacy driver calls survived: {offenders}")
        self.assertIn('#include "p4_twai_node.h"', source)
        self.assertIn("P4TwaiNodeCore core_", source)
        self.assertIn("canBenchInjectPop", source)
        # recorder-meta consumer (main.cpp:414) still gets twai_status_info_t
        self.assertIn("statusInfo(twai_status_info_t &out)", source)


class CanTaskCoreContractTests(unittest.TestCase):
    """can_rt moves to core 1 on the P4 build (reference mechanism)."""

    @classmethod
    def setUpClass(cls):
        cls.main = MAIN.read_text()

    def test_app_can_task_core_is_1_under_p4(self):
        idx = self.main.index("#ifndef APP_CAN_TASK_CORE")
        window = self.main[idx: idx + 400]
        self.assertIn("DRIVER_P4_3CAN", window)
        # 1 inside the P4 branch, 0 as the T2CAN default.
        self.assertRegex(window, r"#define APP_CAN_TASK_CORE 1")
        self.assertRegex(window, r"#define APP_CAN_TASK_CORE 0")

    def test_task_still_created_pinned(self):
        self.assertIn("xTaskCreatePinnedToCore(app_can_task", self.main)


if __name__ == "__main__":
    unittest.main()
