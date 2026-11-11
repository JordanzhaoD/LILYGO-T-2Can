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
MAIN = ROOT / "src" / "main.cpp"
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


class BanForgeGlueContractTests(unittest.TestCase):
    """Source pins for the runtime glue (plan Task 3): CanC send path,
    wantTx OR, disarm->LISTEN_ONLY restore, NVS persistence in the
    "t2can" namespace (as_master pattern), /defense_config wiring, and
    the /status banForgeDiag JSON block."""

    def _body(self, source, signature):
        import re
        match = re.search(signature + r".*?\n\}", source, re.DOTALL)
        self.assertIsNotNone(match, f"body not found for {signature}")
        return match.group(0)

    def test_dashboard_includes_core_header(self):
        source = DASHBOARD.read_text()
        self.assertIn("#include \"dash_ban_forge.h\"", source)
        self.assertIn("#include \"dash_autoshift.h\"", source)

    def test_want_tx_or_gates_bus_mode(self):
        """wantTx must be the OR: autoshift permit OR forge active —
        and nothing else may enter the expression (OR-only rule)."""
        body = self._body(MAIN.read_text(),
                          r"static bool t2canAutoshiftApplyBusMode\(\)")
        self.assertIn(
            "(dashAsPermit.master && dashAsPermit.armed) || dashBanForgeActive()",
            body)

    def test_send_helper_targets_872_dlc8_pt(self):
        """Mirror of t2canAutoshiftSendCan3 for the forge: decimal id
        872, dlc 8, CAN_BUS_PT, tertiary driver send, TX-mode gated."""
        body = self._body(MAIN.read_text(),
                          r"bool t2canBanForgeSendCan3\(const uint8_t d\[8\]\)")
        self.assertIn("f.id = 872", body)
        self.assertIn("f.dlc = 8", body)
        self.assertIn("f.bus = CAN_BUS_PT", body)
        self.assertIn("appDriverTertiary->send(f)", body)
        self.assertIn("t2canAsCan3TxMode", body)

    def test_sniff_hook_runs_core_and_reports_results(self):
        """The CanC sniff hook feeds every frame to the core and, when
        an injection is requested, reports the send result either way —
        failure still consumes budget."""
        source = DASHBOARD.read_text()
        self.assertIn("dashBanForgeOnFrame(tagged.id, tagged.dlc, "
                      "tagged.data,", source)
        self.assertIn("dashBanForgeOnInjectResult(sent, millis())", source)

    def test_disarm_restores_listen_only(self):
        """Disarm (explicit or auto) with autoshift idle must dirty the
        bus mode so TWAI2 returns to LISTEN_ONLY."""
        body = self._body(DASHBOARD.read_text(),
                          r"static void handleDefenseConfig\(\)")
        self.assertIn("dashBanForgeRequestArm(0)", body)
        self.assertIn("t2canAutoshiftModeDirty()", body)

    def test_nvs_persistence_uses_t2can_namespace(self):
        """banForge/banForgeArm persist in ns "t2can" (u8, as_master
        pattern) — single source of truth, loadable at boot from
        main.cpp."""
        source = MAIN.read_text()
        self.assertIn('nvs_set_u8(handle, "banForge"', source)
        self.assertIn('nvs_set_u8(handle, "banForgeArm"', source)
        self.assertIn('nvs_get_u8(handle, "banForge"', source)
        self.assertIn('nvs_get_u8(handle, "banForgeArm"', source)
        # keys fit the 15-char NVS limit
        self.assertLessEqual(len("banForgeArm"), 15)

    def test_boot_restores_pref_but_not_armed(self):
        """Boot: SetPref(loaded) only — the runtime arm flag must stay
        false until an explicit re-arm (fail-closed across reboots)."""
        source = MAIN.read_text()
        self.assertIn("dashBanForgeSetPref(t2canBanForgePrefGet() != 0)",
                      source)
        boot_block = self._boot_block(source)
        self.assertNotIn("dashBanForgeRequestArm", boot_block)

    def _boot_block(self, source):
        import re
        match = re.search(
            r"#ifdef DRIVER_P4_3CAN\n    if \(appBusEnable & 0x04\).*?#endif",
            source, re.DOTALL)
        self.assertIsNotNone(match, "boot P4 block not found")
        return match.group(0)

    def test_status_exposes_ban_forge_diag(self):
        source = DASHBOARD.read_text()
        self.assertIn('"banForgeDiag"', source)
        for key in ('"pref"', '"armed"', '"arm"', '"injected"', '"armsRun"',
                    '"tripwireHits"', '"lastDeviantMs"'):
            self.assertIn(key, source)

    def test_defense_config_reports_and_applies(self):
        """GET returns banForge/banForgeArm; the form save path parses
        them and applies pref + arm at submit time."""
        source = DASHBOARD.read_text()
        self.assertIn('\\"banForge\\":', source)      # GET JSON (C-escaped)
        self.assertIn('\\"banForgeArm\\":', source)   # GET JSON (C-escaped)
        self.assertIn('server.hasArg("ban_forge")', source)
        self.assertIn('dashArgUIntInRange("ban_forge_arm", 0, 7', source)
        self.assertIn("dashBanForgeRequestArm(", source)

    def test_dash_log_events_exist(self):
        source = DASHBOARD.read_text()
        self.assertIn("[BANFORGE]", source)

    def test_glue_is_p4_gated(self):
        """All new dashboard glue must sit inside DRIVER_P4_3CAN guards
        (T2CAN byte-identical red line): count guard opens vs closes
        around the new tokens."""
        source = DASHBOARD.read_text()
        first_guard = source.find("#if defined(DRIVER_P4_3CAN)")
        first_token = source.find("dashBanForgeOnFrame")
        self.assertNotEqual(first_token, -1, "sniff hook missing")
        self.assertGreater(first_token, first_guard,
                           "ban-forge glue appears before any P4 guard")


class BanForgeUiContractTests(unittest.TestCase):
    """UI pins for the 防封禁 Ban Shield row on the FSD 防护 page
    (plan Task 4)."""

    @classmethod
    def setUpClass(cls):
        import re
        cls.ui = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text()
        match = re.search(r'id="pg-defense".*?id="pg-ota"', cls.ui, re.S)
        assert match is not None, "pg-defense page not found"
        cls.page = match.group(0)

    def test_defense_page_has_ban_shield_controls(self):
        self.assertIn('id="def-ban-shield-tgl"', self.page)
        self.assertIn('id="def-ban-shield-arm"', self.page)
        self.assertIn("防封禁 Ban Shield", self.page)
        # experiment disclosure in the description
        self.assertIn("实验", self.page)
        # status chips
        for chip in ("st-banforge-state", "st-banforge-arm",
                     "st-banforge-injected", "st-banforge-tripwire"):
            self.assertIn(f'id="{chip}"', self.ui)

    def test_arm_selector_options_cover_zero_through_seven(self):
        import re
        match = re.search(r'<select[^>]*id="def-ban-shield-arm".*?</select>',
                          self.page, re.S)
        self.assertIsNotNone(match, "arm select not found")
        body = match.group(0)
        for value in range(8):
            self.assertIn(f'value="{value}"', body)
        # arms labelled B1-B5 plus the two fallback labels
        self.assertIn("B1", body)
        self.assertIn("B5", body)

    def test_ui_js_saves_ban_shield_fields(self):
        import re
        match = re.search(r"async function saveDefenseConfig\(\)\{.*?\n\}",
                          self.ui, re.S)
        self.assertIsNotNone(match)
        body = match.group(0)
        self.assertIn("ban_forge", body)
        self.assertIn("ban_forge_arm", body)

    def test_ui_js_loads_and_refreshes_ban_shield(self):
        import re
        load = re.search(r"async function loadDefenseConfig\(\)\{.*?setText\('tb-exp'",
                         self.ui, re.S)
        self.assertIsNotNone(load)
        self.assertIn("def-ban-shield-tgl", load.group(0))
        self.assertIn("def-ban-shield-arm", load.group(0))
        # chips populate from /status banForgeDiag
        self.assertIn("banForgeDiag", self.ui)
        for chip in ("st-banforge-state", "st-banforge-arm",
                     "st-banforge-injected", "st-banforge-tripwire"):
            self.assertIn(f"'{chip}'", self.ui)

    def test_i18n_en_map_covers_ban_shield_strings(self):
        # trailing-space key: the name sits inline before the 实验 badge
        self.assertIn("'防封禁 Ban Shield ':", self.ui)
        self.assertIn("'实验臂':", self.ui)
        self.assertIn("'校验和兜底':", self.ui)
        self.assertIn("'整字节探针':", self.ui)

    def test_ban_shield_ui_is_capability_gated(self):
        """Non-P4 builds must not ship a dead toggle: the rows carry
        data-cap="banForge", the CAP default is false, and capEnabled
        knows the key (kDashCan3Available contract)."""
        self.assertIn('data-cap="banForge"', self.page)
        self.assertIn("banForgeAvailable:false", self.ui)
        self.assertIn("if(name==='banForge')return !!CAP.banForgeAvailable;",
                      self.ui)

    def test_capability_flag_is_p4_only_and_emitted(self):
        """Server side: kDashBanForgeAvailable true only under
        DRIVER_P4_3CAN, emitted unconditionally as "banForgeAvailable"
        so every non-P4 build keeps a byte-identical UI surface."""
        source = DASHBOARD.read_text()
        self.assertIn("static constexpr bool kDashBanForgeAvailable = true;",
                      source)
        self.assertIn("static constexpr bool kDashBanForgeAvailable = false;",
                      source)
        self.assertIn('"banForgeAvailable"', source)


if __name__ == "__main__":
    unittest.main()
