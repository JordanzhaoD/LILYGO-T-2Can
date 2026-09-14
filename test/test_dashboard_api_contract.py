import unittest
import re
from html.parser import HTMLParser
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI_SRC = ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h"
UI_GEN = ROOT / "include" / "web" / "mcp2515_dashboard_ui.h"
DASH = ROOT / "include" / "web" / "mcp2515_dashboard.h"
HANDLERS = ROOT / "include" / "handlers.h"
LEGACY_SPEED = ROOT / "include" / "dash_legacy_speed.h"
LEGACY_SMART_SPEED = ROOT / "include" / "dash_legacy_smart_offset.h"
MAIN = ROOT / "src" / "main.cpp"
VERSION = ROOT / "VERSION"
CHANGELOG = ROOT / "CHANGELOG.md"
README = ROOT / "README.md"
TESTS_WORKFLOW = ROOT / ".github" / "workflows" / "tests.yml"
RELEASE_WORKFLOW = ROOT / ".github" / "workflows" / "release.yml"
SIMULATOR = ROOT / "scripts" / "webui_simulator.py"
CMAKE = ROOT / "CMakeLists.txt"


class DashboardApiContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ui = UI_SRC.read_text(encoding="utf-8")
        cls.ui_gen = UI_GEN.read_text(encoding="utf-8")
        cls.dash = DASH.read_text(encoding="utf-8")
        cls.handlers = HANDLERS.read_text(encoding="utf-8")
        cls.legacy_speed = LEGACY_SPEED.read_text(encoding="utf-8")
        cls.legacy_smart_speed = LEGACY_SMART_SPEED.read_text(encoding="utf-8")
        cls.main = MAIN.read_text(encoding="utf-8")
        cls.version = VERSION.read_text(encoding="utf-8")
        cls.changelog = CHANGELOG.read_text(encoding="utf-8")
        cls.readme = README.read_text(encoding="utf-8")
        cls.tests_workflow = TESTS_WORKFLOW.read_text(encoding="utf-8")
        cls.release_workflow = RELEASE_WORKFLOW.read_text(encoding="utf-8")
        cls.simulator = SIMULATOR.read_text(encoding="utf-8")
        cls.cmake = CMAKE.read_text(encoding="utf-8")

    def _standalone_visible_source(self) -> str:
        """Return the source UI surface visible after standalone product gating.

        The shared source UI is allowed to keep dual-CAN copy inside elements
        hidden by standalone mode. This helper approximates the standalone DOM by
        removing elements marked data-single-hide and replacing data-single-text
        elements with their standalone text before visible-copy assertions run.
        """

        class StandaloneVisibleParser(HTMLParser):
            VOID_TAGS = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"}

            def __init__(self) -> None:
                super().__init__(convert_charrefs=True)
                self.parts: list[str] = []
                self.hidden_depth = 0
                self.replaced_depth = 0

            def _is_void(self, tag: str) -> bool:
                return tag.lower() in self.VOID_TAGS

            def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
                attr_map = {name: value for name, value in attrs}
                if self.hidden_depth:
                    if not self._is_void(tag):
                        self.hidden_depth += 1
                    return
                if "data-single-hide" in attr_map:
                    if not self._is_void(tag):
                        self.hidden_depth = 1
                    return
                if self.replaced_depth:
                    if not self._is_void(tag):
                        self.replaced_depth += 1
                    return
                if "data-single-text" in attr_map:
                    self.parts.append(attr_map.get("data-single-text") or "")
                    if not self._is_void(tag):
                        self.replaced_depth = 1
                    return

                attr_text = " ".join(
                    name if value is None else f'{name}="{value}"'
                    for name, value in attrs
                )
                self.parts.append(f"<{tag}{(' ' + attr_text) if attr_text else ''}>")

            def handle_endtag(self, tag: str) -> None:
                if self.hidden_depth:
                    self.hidden_depth -= 1
                    return
                if self.replaced_depth:
                    self.replaced_depth -= 1
                    return
                self.parts.append(f"</{tag}>")

            def handle_data(self, data: str) -> None:
                if not self.hidden_depth and not self.replaced_depth:
                    self.parts.append(data)

        parser = StandaloneVisibleParser()
        parser.feed(self.ui)
        return "".join(parser.parts)

    def _route_block(self, route: str) -> str:
        idx = self.dash.find(route)
        self.assertNotEqual(idx, -1, f"{route} not found in dashboard source")
        start = max(0, idx - 250)
        end = min(len(self.dash), idx + 250)
        return self.dash[start:end]

    def test_dashboard_ui_generation_is_dependency_aware(self) -> None:
        """PlatformIO must rebuild firmware when the generated dashboard header changes."""
        build_script = (ROOT / "scripts" / "update_ota_build_timestamp.py").read_text(encoding="utf-8")
        minify_script = (ROOT / "scripts" / "minify_dashboard.py").read_text(encoding="utf-8")
        self.assertIn("env.Command", build_script)
        self.assertIn("env.Depends(\"$BUILD_DIR/src/main.cpp.o\"", build_script)
        self.assertIn("env.Depends(\"$BUILD_DIR/${PROGNAME}.elf\"", build_script)
        self.assertIn("DASH_UI_BUILD_ID", minify_script)
        self.assertIn("--check", minify_script)

    def test_generated_dashboard_header_contains_current_phase_tokens(self) -> None:
        """Generated gzip header must be regenerated from the current source UI."""
        for token in [
            "DASH_HTML_GZ",
            "DASH_UI_BUILD_ID",
            "DASH_UI_BUILD_UTC",
            "pg-strobe",
            "pg-shift",
            "/speed_custom",
            "/defense_config",
            "/fog_light",
            "/burst",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui_gen)

    def test_generated_dashboard_header_contains_task1_ui_contract_tokens(self) -> None:
        """Generated dashboard header must stay synchronized with Task 1 source UI contracts."""
        for token in [
            "ap-core-gate-tgl",
            "mob-more-single",
            "renderPluginsStatus",
            "/plugins/status",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui_gen)
        # 4.5.0-beta02: hardware-page gate toggle removed (moved to defense page)
        self.assertNotIn("ap-gate-tgl", self.ui_gen)
        # 4.5.0-beta06: AP-settle delay select removed with the delay feature.
        self.assertNotIn("ap-delay-select", self.ui_gen)

    def test_destructive_buttons_use_post_helpers(self) -> None:
        self.assertIn('onclick="resetStats()"', self.ui)
        self.assertIn('onclick="rebootDevice()"', self.ui)
        self.assertNotIn("fetch('/reset_stats')", self.ui)
        self.assertNotIn("fetch('/reboot')", self.ui)

    def test_post_helper_surfaces_http_errors(self) -> None:
        self.assertIn("async function postForm(url,data)", self.ui)
        self.assertIn("if(!r.ok)", self.ui)
        self.assertIn("showToast((e&&e.message)?e.message:T('请求失败'))", self.ui)
        self.assertIn("throw e;", self.ui)

    def test_hardware_auto_matches_backend_mode_value(self) -> None:
        self.assertIn('onclick="setHW(3)"', self.ui)
        self.assertIn("var map=[3,0,1,2];", self.ui)
        self.assertIn("if (v <= 3 && v != hwMode)", self.dash)

    def test_status_exposes_build_and_legacy_diagnostics(self) -> None:
        """Device status must show which firmware/UI and handler mode are running."""
        for token in [
            "dashDefaultHw",
            "effectiveHw",
            "hwName",
            "buildEnv",
            "uiBuildId",
            "uiBuildUtc",
            "DASH_DEFAULT_HW",
            "DASH_BUILD_ENV",
            "DASH_UI_BUILD_ID",
            "DASH_UI_BUILD_UTC",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)
        self.assertIn("DASH_DEFAULT_HW=", (ROOT / "scripts" / "platformio_sync_profile.py").read_text(encoding="utf-8"))
        self.assertIn("DASH_BUILD_ENV", (ROOT / "scripts" / "platformio_sync_profile.py").read_text(encoding="utf-8"))
        self.assertIn("d.hwName||hwLabel(d.hw)", self.ui)
        self.assertIn("d.uiBuildUtc||d.uiBuildId||d.buildEnv", self.ui)

    def test_waveshare_single_can_standalone_env_is_declared(self) -> None:
        pio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        match = re.search(r"(?ms)^\[env:waveshare_single_can_standalone\]\n.*?(?=^\[|\Z)", pio)
        self.assertIsNotNone(match)
        standalone_env = match.group(0)
        self.assertIn("-DDRIVER_TWAI", standalone_env)
        self.assertIn("-DESP32_DASHBOARD", standalone_env)
        self.assertIn("-DDASH_SINGLE_CAN_STANDALONE=1", standalone_env)
        self.assertIn("-DDASH_INITIAL_HW_MODE=3", standalone_env)
        self.assertIn("-DTWAI_TX_PIN=GPIO_NUM_15", standalone_env)
        self.assertIn("-DTWAI_RX_PIN=GPIO_NUM_16", standalone_env)
        self.assertIn("partitions_16mb_ota_4096k_nvs64.csv", standalone_env)

    def test_status_exposes_single_can_capabilities(self) -> None:
        for token in [
            "dashCapabilitiesJson",
            '"capabilities"',
            '"singleCan"',
            '"can2Available"',
            '"lightingBusSupported"',
            '"serviceModeSupported"',
            '"stalkTestSupported"',
            '"bus2SnifferSupported"',
            '"fsdActivation"',
            '"speedStrategy"',
            '"driveProfile"',
            '"networkSettings"',
            '"otaUpdate"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)

    def test_single_can_build_gates_can2_only_routes(self) -> None:
        for token in [
            "DASH_SINGLE_CAN_STANDALONE",
            "server.on(\"/service_mode\", HTTP_GET, handleServiceMode);",
            "server.on(\"/stalk_test\", HTTP_GET, handleStalkTest);",
            "server.on(\"/burst\", HTTP_GET, handleBurst);",
            "server.on(\"/bus2_ids\", HTTP_GET, handleBus2Ids);",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)
        self.assertRegex(
            self.dash,
            r"#if defined\(DRIVER_T2CAN_DUAL\) && !defined\(DASH_SINGLE_CAN_STANDALONE\)(?s:.*?)/bus2_ids",
        )
        self.assertRegex(
            self.dash,
            r"#if !defined\(DASH_SINGLE_CAN_STANDALONE\)(?s:.*?)/lighting_config",
        )

    def test_single_can_ui_uses_capabilities_to_hide_can2_and_lighting(self) -> None:
        for token in [
            "var CAP=",
            "function applyCapabilities(c)",
            "data-cap=\"can2\"",
            "data-cap=\"lighting\"",
            "cap-hidden",
            "applyCapabilities(d.capabilities||{})",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_single_can_driving_status_exposes_ap_gate_and_restore(self) -> None:
        # 4.5.0-beta02: the gate toggle moved from the hardware page
        # (ap-gate-tgl, removed) to the FSD-defense page (ap-core-gate-tgl);
        # restore follows it there. 4.5.0-beta06: the AP-settle delay select
        # ("延迟注入", 0/1/2/3 s) was removed with the settle feature — the
        # 8.3.6 coordinator now owns injection timing.
        self.assertNotIn('id="ap-gate-tgl"', self.ui)
        self.assertNotIn("saveApGate()", self.ui)
        for token in [
            'id="ap-core-gate-tgl"',
            'id="ap-auto-restore-tgl"',
            'saveApGateControls()',
            'renderApInjectionState',
            '等待 AP',
            '正在注入',
            '已阻断',
            'AP 自动恢复',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)
        for token in [
            'ap-delay-select',
            'apDelayMs',
            '稳定计时中',
            'settling',
        ]:
            with self.subTest(token=token):
                self.assertNotIn(token, self.ui)

        for token in [
            r'\"apInjectionState\"',
            '"apAutoRestore"',
            'server.hasArg("ap_auto_restore")',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)
        for token in [
            r'\"apDelayMs\"',
            'server.hasArg("ap_delay_ms")',
            'prefs.putUInt("ap_dly"',
            'prefs.getUInt("ap_dly"',
        ]:
            with self.subTest(token=token):
                self.assertNotIn(token, self.dash)
        # Legacy NVS key is actively cleaned on load (one-way migration).
        self.assertIn('prefs.remove("ap_dly")', self.dash)

    def test_official_plugin_manager_contract_is_present(self) -> None:
        for token in [
            '#include "dash_plugin_engine.h"',
            'DashPluginEngine',
            'dashPluginEngine',
            'server.on("/plugins/status", HTTP_GET, handlePluginsStatus);',
            'server.on("/plugins/install_url", HTTP_POST, handlePluginsInstallUrl);',
            'server.on("/plugins/upload", HTTP_POST, handlePluginsUpload',
            'server.on("/plugins/install_json", HTTP_POST, handlePluginsInstallJson);',
            'server.on("/plugins/toggle", HTTP_POST, handlePluginsToggle);',
            'server.on("/plugins/priority", HTTP_POST, handlePluginsPriority);',
            'server.on("/plugins/remove", HTTP_POST, handlePluginsRemove);',
            'server.on("/plugins/replay_count", HTTP_POST, handlePluginsReplayCount);',
            'server.on("/plugins/rule_test", HTTP_POST, handlePluginsRuleTest);',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)

        for token in [
            'Plugins',
            'renderPluginsStatus',
            'installPluginUrl',
            'installPluginJson',
            'uploadPluginJson',
            'togglePlugin',
            'setPluginPriority',
            'removePlugin',
            'setPluginReplayCount',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

        # Task 5 (UI V1.1) replaced the English plugin UI labels with Chinese
        # cockpit-style copy. The old English labels must not come back.
        for token in [
            'Install from URL',
            'Upload .json',
            'Paste JSON',
            'Installed Plugins',
            'Plugin Editor',
            'Rule Test',
            'GTW2047 Replay Count',
        ]:
            with self.subTest(token=token):
                self.assertNotIn(token, self.ui)

    def test_single_can_product_ui_hides_dual_can_surfaces(self) -> None:
        for token in [
            'PRODUCT_SINGLE_CAN_NAME',
            'Atlas Single CAN',
            'function applyProductMode',
            'data-single-hide',
            'data-single-text',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

        forbidden_visible_tokens = [
            'CAN2 控制',
            'CAN2 状态',
            'CAN2 RX',
            'CAN2 TX',
            '自动换挡',
            'Auto Shift',
            'T-2CAN 控制面板',
            'ATLAS T-2CAN',
        ]
        source = self._standalone_visible_source()
        for token in forbidden_visible_tokens:
            with self.subTest(token=token):
                self.assertNotIn(token, source)

        for token in [
            'data-page="pg-bus2" data-cap="can2" data-single-hide="1"',
            'data-page="pg-strobe" data-cap="lighting" data-single-hide="1"',
            'data-page="pg-shift" data-single-hide="1"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_mobile_single_can_ui_matches_ap_and_plugin_capabilities(self) -> None:
        for token in [
            'id="mob-tabs"',
            'data-mobile-page="driving"',
            'data-mobile-page="hardware"',
            'data-mobile-page="speed"',
            'data-mobile-page="network"',
            'id="mob-more-single"',
            "driving:'pg-overview'",
            "hardware:'pg-hardware'",
            "speed:'pg-speed'",
            "network:'pg-network'",
            'id="plugins-card"',
            'id="ap-core-card"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

        for token in [
            'id="mob-plugin-page"',
            'data-mobile-page="plugins"',
            'data-mobile-page-panel="driving"',
            'data-mobile-page-panel="network"',
            'm-ap-state',
            'm-ap-gate-tgl',
            'm-ap-delay-select',
            'm-ap-auto-restore-tgl',
            'm-plugin-list',
            'm-plugin-json',
            'm-plugin-replay-count',
            'renderMobilePlugins',
            'renderMobileApState',
        ]:
            with self.subTest(token=token):
                self.assertNotIn(token, self.ui)

        source = self._standalone_visible_source()
        for token in [
            'data-page="pg-bus2">',
            'data-page="pg-strobe">',
            'data-page="pg-shift">',
        ]:
            with self.subTest(token=token):
                self.assertNotIn(token, source)

    def test_plugin_engine_header_exposes_upstream_official_contract(self) -> None:
        plugin = (ROOT / "include" / "dash_plugin_engine.h").read_text(encoding="utf-8")
        for token in [
            'kDashPluginMaxPlugins = 8',
            'kDashPluginMaxRules = 16',
            'kDashPluginMaxOps = 16',
            'kDashPluginMaxFilterIds = 32',
            'DashPluginEngine',
            'installJson',
            'setEnabled',
            'setPriority',
            'setReplayCount',
            'applyToFrame',
            'tickPeriodic',
            'emit_periodic',
            'gtw_silent',
            'PLUGIN_GTW_UDS_CUSTOM_KEY',
            '2047',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, plugin)

    def test_single_can_docs_are_present(self) -> None:
        building = (ROOT / "docs" / "building.md").read_text(encoding="utf-8")
        self.assertIn("waveshare_single_can_standalone", building)
        self.assertIn("Waveshare 单 CAN", self.readme)
        self.assertIn("DASH_SINGLE_CAN_STANDALONE", self.changelog)

    def test_legacy_fsd_diag_and_config_are_exposed(self) -> None:
        for token in [
            '"fsdDiag"',
            'legacyFsdPolicyName',
            'fsdTriggerSourceName',
            'fsdSkipReasonName',
            '"mux0"',
            '"mux1"',
            '"triggerSource"',
            '"forceRuntime"',
            'server.on("/legacy_fsd_config", HTTP_GET, handleLegacyFsdConfig);',
            'server.on("/legacy_fsd_config", HTTP_POST, handleLegacyFsdConfig);',
            'prefs.putUChar("lfsd_pol"',
            'prefs.getUChar("lfsd_pol"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)

    def test_legacy_tesla_parity_policy_is_wired(self) -> None:
        diag = (ROOT / "include" / "dash_fsd_diag.h").read_text(encoding="utf-8")
        for token in [
            'TeslaParity',
            '"legacy_tesla_parity"',
            'p == "legacy_tesla_parity"',
            'prefs.putUChar("lfsd_pol"',
            'prefs.getUChar("lfsd_pol"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, diag + self.dash)
        self.assertIn('Tesla Controller 对齐模式', self.ui)

    def test_legacy_can_a_fsd_diag_expanded_fields_are_exposed(self) -> None:
        for token in [
            'FsdHealthState',
            'FsdGateBlockReason',
            'FsdDiagBus',
            'FsdDiagDriver',
            'fsdHealthStateName',
            'fsdGateBlockReasonName',
            'classifyLegacyFsdHealth',
            'kLegacyFsdSourceStaleMs',
            'FsdGateBlockReason (*gateBlockReason)() = nullptr',
            'currentGateBlockReason()',
            'FsdGateBlockReason::ApGate',
            'firstMux0RxMs',
            'firstTxOkMs',
            'firstTxFailMs',
            'LegacyFsdSettle',
            'legacy_fsd_settle',
            'legacyFsdActivationAllowed',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.handlers + (ROOT / "include" / "dash_fsd_diag.h").read_text(encoding="utf-8"))
        for token in [
            '"health"',
            '"gateReason"',
            '"lastBlockedBy"',
            '"bus"',
            '"driver"',
            '"firstMux0RxAgeMs"',
            '"firstTxOkAgeMs"',
            '"firstTxFailAgeMs"',
            '"aux760"',
            '"aux1080"',
            'dashCurrentGateBlockReason',
            'dashHandler->gateBlockReason = dashCurrentGateBlockReason',
            'handlerPool[i]->gateBlockReason = dashCurrentGateBlockReason',
            'FsdGateBlockReason::CanActive',
            'FsdGateBlockReason::Ota',
            'FsdGateBlockReason::ApGate',
            'FsdGateBlockReason::CompileGate',
            'dashLegacyFsdActivationAllowed',
            'legacyFsdAllowed',
            'blockedFrame',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)
        # 4.5.0-beta06: the settle-timer diagnostics left with the feature.
        self.assertNotIn('requiredStableMs', self.dash)
        self.assertNotIn('apStableMs', self.dash)

    def test_legacy_can_a_health_ui_labels_are_present(self) -> None:
        for token in [
            'Legacy CAN-A 诊断',
            '0x3EE mux0',
            '0x3EE mux1',
            'TWAI 状态',
            'TEC/REC',
            'TX失败',
            'RX丢失',
            '健康状态',
            '最后阻止原因',
            'Tesla Controller 对齐模式',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)
                self.assertIn(token, self.ui_gen)

    def test_legacy_fsd_policy_uses_selection_cards(self) -> None:
        """Regression: policy card container must use the 3-column selection-card layout."""
        source_policy_cards_div = re.compile(
            r'<div\b(?=[^>]*\bid="legacy-fsd-policy-cards")(?=[^>]*\bclass="sel-cards c3")[^>]*>'
        )
        generated_policy_cards_div = re.compile(
            r'<div\b'
            r'(?=[^>]*\bid=(?:"legacy-fsd-policy-cards"|legacy-fsd-policy-cards)(?=[\s>/]))'
            r'(?=[^>]*\bclass=(?:"sel-cards c3"|sel-cards)(?=[\s>/]))'
            r'[^>]*>'
        )
        self.assertRegex(self.ui, source_policy_cards_div)
        self.assertRegex(self.ui_gen, generated_policy_cards_div)

        for token in [
            'data-policy="stable"',
            'data-policy="experimental"',
            'data-policy="legacy_tesla_parity"',
            'renderLegacyFsdPolicyCards(fsd.policy)',
            'renderLegacyFsdPolicyCards(policy)',
            'Tesla Controller 对齐模式',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

        source_policy_fn = re.search(r"function renderLegacyFsdPolicyCards\(policy\)\{.*?\n\}", self.ui, re.S)
        self.assertIsNotNone(source_policy_fn)
        source_policy_body = source_policy_fn.group(0)
        self.assertRegex(source_policy_body, r"policy==='legacy_stable'.*?'stable'")
        self.assertRegex(source_policy_body, r"policy==='legacy_experimental'.*?'experimental'")
        self.assertIn("policy||'stable'", source_policy_body)

        for pattern in [
            r'\bdata-policy=(?:"stable"|stable)(?=[\s>/])',
            r'\bdata-policy=(?:"experimental"|experimental)(?=[\s>/])',
            r'\bdata-policy=(?:"legacy_tesla_parity"|legacy_tesla_parity)(?=[\s>/])',
            r'renderLegacyFsdPolicyCards\(fsd\.policy\)',
            r'renderLegacyFsdPolicyCards\(policy\)',
            r"policy==='legacy_stable'.*?'stable'",
            r"policy==='legacy_experimental'.*?'experimental'",
            r"policy\|\|'stable'",
            r'Tesla Controller 对齐模式',
        ]:
            with self.subTest(pattern=pattern):
                self.assertRegex(self.ui_gen, pattern)

    def test_legacy_fsd_mux1_checkbox_has_unique_toggle_id(self) -> None:
        """Regression: mux1 checkbox and toggle must not share duplicate DOM ids."""
        self.assertIn('id="legacy-fsd-mux1"', self.ui)
        self.assertIn('id="legacy-fsd-mux1-tgl"', self.ui)
        self.assertIn("document.getElementById('legacy-fsd-mux1-tgl')", self.ui)
        self.assertEqual(self.ui.count('id="legacy-fsd-mux1"'), 1)
        self.assertEqual(self.ui.count('id="legacy-fsd-mux1-tgl"'), 1)

        mux1_id = r'\bid=(?:"legacy-fsd-mux1"|legacy-fsd-mux1)(?=[\s>/])'
        mux1_toggle_id = r'\bid=(?:"legacy-fsd-mux1-tgl"|legacy-fsd-mux1-tgl)(?=[\s>/])'
        self.assertRegex(self.ui_gen, mux1_id)
        self.assertRegex(self.ui_gen, mux1_toggle_id)
        self.assertIn("document.getElementById('legacy-fsd-mux1-tgl')", self.ui_gen)
        self.assertEqual(len(re.findall(mux1_id, self.ui_gen)), 1)
        self.assertEqual(len(re.findall(mux1_toggle_id, self.ui_gen)), 1)

    def test_twai_diagnostics_are_exposed_for_legacy_can_a(self) -> None:
        twai = (ROOT / "include" / "drivers" / "twai_driver.h").read_text(encoding="utf-8")
        diag = (ROOT / "include" / "dash_twai_diag.h").read_text(encoding="utf-8")
        for token in [
            '#define TWAI_RX_QUEUE_LEN 128',
            'DashTwaiDiag',
            'snapshotDiag()',
            'twai_get_status_info',
            'readDrainBudgetHits',
            'framesAccepted',
            'framesDropped',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, twai + diag)
        for token in [
            '"twai"',
            '"state"',
            '"tec"',
            '"rec"',
            '"txFailed"',
            '"rxMissed"',
            '"readDrainBudgetHits"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)

    def test_service_diag_evidence_is_exposed(self) -> None:
        for token in [
            '#include "dash_tx_evidence.h"',
            'DashTxEvidence serviceTxEvidence',
            'recordServiceTxEvidence',
            '"serviceDiag"',
            '"lastCommand"',
            '"burstFrames"',
            '"lastData"',
            '"vehicleResponse":"unknown"',
            'serviceDiagJson',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.main + self.dash)

    def test_wheel_dnd_uses_native_3c2_and_exposes_diag(self) -> None:
        for token in [
            'recordNative3c2',
            'native3c2Fresh',
            'DashWheelDndDiag',
            '"wheelDndDiag"',
            '"native3c2Seen"',
            '"baseFrameMode"',
            '"waiting_native_mux1"',
            't2canWheelDndRecordNative',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.main + self.dash + self.ui + self.handlers + self.legacy_speed + (ROOT / "include" / "dash_wheel_dnd.h").read_text(encoding="utf-8"))

    def test_legacy_fsd_config_accepts_get_emitted_policy_names(self) -> None:
        """POST /legacy_fsd_config must round-trip policy strings emitted by GET."""
        handler = re.search(r"static void handleLegacyFsdConfig\(\).*?server\.send\(200, \"application/json\", j\);", self.dash, re.S)
        self.assertIsNotNone(handler)
        body = handler.group(0)
        self.assertIn('p == "legacy_experimental"', body)
        self.assertIn('p == "legacy_stable"', body)
        self.assertIn('p == "experimental"', body)
        self.assertIn('p == "1"', body)
        self.assertLess(body.index('p == "legacy_stable"'), body.index('dashLegacyFsdPolicy = LegacyFsdPolicy::Stable'))

    def test_uptime_and_fsd_boot_persistence_are_wired(self) -> None:
        """Running time and FSD boot/default state must round-trip through /status and /config.

        bootCan can be changed via the "开机自动启用" toggle (saveConfig),
        and the master toggle also persists the chosen state as the boot default.
        """
        self.assertIn('uptime', self.dash)
        self.assertIn('bootCan', self.dash)
        self.assertIn('prefs.putBool("boot_can", bootCanActive)', self.dash)
        self.assertIn('bootCanActive = prefs.getBool("boot_can"', self.dash)
        self.assertIn('server.hasArg("bootCan")', self.dash)
        self.assertIn("var uptime=(d.uptime!==undefined)?d.uptime:(d.up||0);", self.ui)
        self.assertIn("fmtUp(uptime)", self.ui)
        # Jordan chose master-toggle persistence: toggleFsd() updates bootCan too.
        self.assertIn("bootCan:next?'1':'0'", self.ui)
        self.assertIn("data.bootCan=bt.checked?'1':'0'", self.ui)

    def test_config_get_route_returns_fsd_runtime_for_ui_reload(self) -> None:
        """Regression: GET /config 必须存在并返回 fsdRuntime 块供 UI 回填。

        UI loadLegacyFsdConfig() 用 fetchJson('/config') 取 conf，再读
        conf.fsdRuntime.legacyOffset / conf.fsdRuntime.overrideSpeedLimit 回填
        Legacy 速度偏移输入框与重写限速开关。曾因只注册了 POST /config、缺 GET
        路由，导致 fetchJson('/config') 取 null → 重启后输入框/开关恒显示默认
        （NVS 实际已持久化，只是 UI 读不出来）。此测试锁住 GET /config 路由、
        handler 返回 fsdRuntime 嵌套块，以及前后端字段契约对齐。
        """
        # GET /config 路由 + handler 必须存在（POST 路由同时保留）
        self.assertIn('server.on("/config", HTTP_GET, handleConfigGet);', self.dash)
        self.assertIn('server.on("/config", HTTP_POST, handleConfig);', self.dash)
        handler = re.search(r"static void handleConfigGet\(\).*?server\.send\(200", self.dash, re.S)
        self.assertIsNotNone(handler, "handleConfigGet handler missing")
        body = handler.group(0)
        # 返回 fsdRuntime 嵌套块，含 legacyOffset/overrideSpeedLimit（与 POST 契约对齐）
        self.assertIn("fsdRuntime", body)
        self.assertIn("legacyOffset", body)
        self.assertIn("nvsLegacyOffset", body)
        self.assertIn("overrideSpeedLimit", body)
        self.assertIn("nvsOverrideSpeedLimit", body)
        # UI 从 /config 的 fsdRuntime 读这两个字段（前后端契约对齐）
        self.assertIn("fetchJson('/config')", self.ui)
        self.assertIn("conf.fsdRuntime.legacyOffset", self.ui)
        self.assertIn("conf.fsdRuntime.overrideSpeedLimit", self.ui)

    def test_status_mux_json_is_closed_before_phase1_fields(self) -> None:
        """The /status JSON must close mux[] before appending Phase 1 root fields."""
        match = re.search(r"static void handleStatus\(\).*?server\.send\(200, \"application/json\", j\);", self.dash, re.S)
        self.assertIsNotNone(match)
        body = match.group(0)
        mux_pos = body.index('j += "]},\\"mux\\":[";')
        vehicle_pos = body.index('j += ",\\"vehicleOta\\":";')
        between = body[mux_pos:vehicle_pos]
        self.assertIn('j += "]";', between)
        self.assertNotIn('j += "]}";', body[vehicle_pos:])

    def test_settings_backup_exports_new_dashboard_config(self) -> None:
        """Backup JSON must include the newer FSD, speed, defense, lighting and power settings."""
        match = re.search(r"static void handleSettingsExport\(\).*?server\.send\(200, \"application/json\", j\);", self.dash, re.S)
        self.assertIsNotNone(match)
        body = match.group(0)
        for token in [
            '\\"bootCan\\"',
            '\\"apGate\\"',
            '\\"apAutoRestore\\"',
            '\\"driveProfile\\"',
            '\\"speedStrategy\\"',
            '\\"speed\\"',
            '\\"lighting\\"',
            '\\"defense\\"',
            '\\"power\\"',
            '\\"fsdRuntime\\"',
            '\\"legacyMpp\\"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_settings_import_restores_new_dashboard_config(self) -> None:
        """Restore must accept Auto HW and write all persisted dashboard config groups."""
        match = re.search(r"static void handleSettingsImport\(\).*?dashLog\(\"\[BACKUP\] Settings imported", self.dash, re.S)
        self.assertIsNotNone(match)
        body = match.group(0)
        self.assertIn("hw >= 0 && hw <= 3", body)
        self.assertNotIn("hw >= 0 && hw <= 2", body)
        for token in [
            'p.putBool("boot_can"',
            'p.putBool("ap_gate"',
            'p.putBool("ap_rst"',
            'p.putUChar("drv_prof"',
            'p.putUChar("offsetMode"',
            'p.putUChar("manualPct"',
            'p.putBool("lt_en"',
            'p.putBool("def_en"',
            'p.putBool(NVS_KEY_AUTO_SHUTDOWN',
            'p.putBool("fa"',
            'p.putBool("lg_mpp_en"',
            "i < kHw3CustomTargetCount && i < arr.size()",
            "i < kHw3HighSpeedBucketCount && i < arr.size()",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_fsd_injection_control_lives_in_module_page(self) -> None:
        """FSD injection controls belong to Module Config, not Driving Mode."""
        module = re.search(r'id="pg-overview".*?<!-- Page 2: Hardware Config -->', self.ui, re.S)
        drive = re.search(r'id="pg-drive".*?<!-- Page 4: Speed Offset -->', self.ui, re.S)
        self.assertIsNotNone(module)
        self.assertIsNotNone(drive)
        self.assertIn('id="fsd-toggle"', module.group(0))
        self.assertIn('id="fsd-boot-tgl"', module.group(0))
        self.assertNotIn('id="fsd-toggle"', drive.group(0))
        self.assertNotIn('id="fsd-boot-tgl"', drive.group(0))

    def test_backend_accepts_panel_control_methods(self) -> None:
        expected_routes = [
            'server.on("/reset_stats", HTTP_POST, handleResetStats);',
            'server.on("/reboot", HTTP_POST, handleReboot);',
            'server.on("/service_mode", HTTP_POST, handleServiceMode);',
            'server.on("/burst", HTTP_POST, handleBurst);',
        ]
        for route in expected_routes:
            with self.subTest(route=route):
                self.assertIn(route, self.dash)

    def test_bus2_diagnostics_are_exposed_in_status(self) -> None:
        """Dual-CAN builds should expose CAN2 RX/TX/TXErr/EFLG separately from CAN1."""
        self.assertIn('uint32_t t2canBus2RxCount(void);', self.dash)
        self.assertIn('uint32_t t2canBus2TxCount(void);', self.dash)
        self.assertIn('uint32_t t2canBus2TxErrCount(void);', self.dash)
        self.assertIn('uint8_t t2canBus2Eflg(void);', self.dash)
        self.assertIn(',\\"can2\\":{\\"rx\\":', self.dash)
        self.assertIn('t2canBus2TxCount()', self.dash)
        self.assertIn('t2canBus2TxErrCount()', self.dash)
        self.assertIn('t2canBus2Eflg()', self.dash)
        self.assertIn('id="b2-tx"', self.ui)
        self.assertIn('id="b2-txerr"', self.ui)
        self.assertIn('id="b2-eflg"', self.ui)

    def test_service_mode_uses_vcsec_four_frame_pulse(self) -> None:
        """Service mode should send spec-correct 0x339 pulses, not continuous 0xE0 spam."""
        self.assertIn('g_svcBurstRemaining = 4;', self.main)
        self.assertIn('g_svcBurstValue = on ? 0x80 : 0x00;', self.main)
        self.assertIn('f.data[5] = g_svcBurstValue;', self.main)
        self.assertIn('t2canTxSecondaryCounted(f);', self.main)
        self.assertNotIn('f.data[5] = 0xE0;', self.main)
        self.assertIn('VCSEC_serviceDiagnosticRequest', self.readme)
        self.assertIn('00 00 00 00 00 80 00 00', self.readme)
        self.assertIn('00 00 00 00 00 00 00 00', self.readme)
        self.assertIn('四帧脉冲', self.ui)

    def test_mcp2515_bus2_spi_and_filter_support(self) -> None:
        """MCP2515 driver should support returning from accept-all to filtered mode and use 10MHz SPI."""
        mcp = (ROOT / "include" / "drivers" / "espidf_mcp2515.h").read_text(encoding="utf-8")
        self.assertIn('void setReceiveAllMode()', mcp)
        self.assertIn('void setUseFiltersMode()', mcp)
        self.assertIn('RXBnCTRL_RXM_STDEXT', mcp)
        self.assertIn('dev.clock_speed_hz = 10000000', mcp)

    def test_high_beam_shared_bus_limit_is_documented(self) -> None:
        """README should record that shared-bus injection cannot force high-beam during FSD."""
        for token in [
            '0x3F5 byte1 bit7',
            '0x3F5 byte3',
            '0x293 byte2 bit6',
            'inline MITM',
            'shared-bus injection cannot override',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.readme)

    def test_all_panel_endpoints_have_matching_backend_routes(self) -> None:
        routes = set()
        for route, method in re.findall(r'server\.on\("([^"]+)",\s*(HTTP_\w+),', self.dash):
            routes.add((method.replace("HTTP_", ""), route))

        expected = {
            ("GET", "/status"),
            ("GET", "/system_status"),
            ("GET", "/can_pins"),
            ("GET", "/frames"),
            ("GET", "/bus2_ids"),
            ("GET", "/stalk_test"),
            ("GET", "/burst"),
            ("GET", "/ota_creds"),
            ("GET", "/wifi_scan"),
            ("GET", "/wifi_status"),
            ("GET", "/wifi_networks"),
            ("GET", "/ap_status"),
            ("GET", "/gateway_status"),
            ("GET", "/gateway_dns"),
            ("GET", "/gateway_dns_test"),
            ("GET", "/gateway_blocked"),
            ("GET", "/rec_download"),
            ("GET", "/mode_hw"),
            ("GET", "/drive_profile"),
            ("GET", "/speed_strategy"),
            ("GET", "/speed_custom"),
            ("GET", "/lighting_config"),
            ("GET", "/defense_config"),
            ("GET", "/gear_assist_status"),
            ("GET", "/hotspot_config"),
            ("GET", "/dns_rules"),
            ("POST", "/config"),
            ("POST", "/mode_hw"),
            ("POST", "/drive_profile"),
            ("POST", "/speed_strategy"),
            ("POST", "/speed_custom"),
            ("POST", "/lighting_config"),
            ("POST", "/defense_config"),
            ("POST", "/hotspot_config"),
            ("POST", "/relay_wifi_test"),
            ("POST", "/dns_rules"),
            ("POST", "/reset_stats"),
            ("POST", "/reboot"),
            ("POST", "/service_mode"),
            ("POST", "/burst"),
            ("POST", "/wifi_config"),
            ("POST", "/wifi_connect"),
            ("POST", "/wifi_delete"),
            ("POST", "/gateway_dns"),
            ("POST", "/gateway_blocked_clear"),
            ("POST", "/rec_start"),
            ("POST", "/rec_stop"),
            ("POST", "/logging"),
            ("POST", "/ap_config"),
            ("POST", "/update"),
            ("GET", "/update_check"),
            ("POST", "/update_install"),
            ("GET", "/update_beta"),
            ("POST", "/update_beta"),
            ("GET", "/auto_update"),
            ("POST", "/auto_update"),
            # Phase 1 新增端点
            ("GET", "/power_mgmt"),
            ("POST", "/power_mgmt"),
            ("GET", "/vehicle_ota_status"),
            ("GET", "/fog_light"),
            ("POST", "/fog_light"),
            ("POST", "/strobe_cont"),
        }
        missing = sorted(expected - routes)
        self.assertEqual([], missing)

    def test_ota_upload_matches_espidf_raw_upload_handler(self) -> None:
        self.assertIn("xhr.open('POST','/update',true);", self.ui)
        self.assertIn("xhr.setRequestHeader('Content-Type','application/octet-stream');", self.ui)
        self.assertIn("xhr.setRequestHeader('X-File-Name'", self.ui)
        self.assertIn("xhr.send(file);", self.ui)
        # The OTA /update handler streams a raw octet-stream body (xhr.send(file)),
        # never a multipart form. FormData is permitted elsewhere only for the
        # /plugins/upload route, whose backend consumes WebServer multipart chunks.
        # Guard that FormData usage stays tied to the plugin upload and never leaks
        # into the OTA path.
        ota_idx = self.ui.find("xhr.open('POST','/update',true);")
        plugin_idx = self.ui.find("/plugins/upload")
        self.assertGreater(ota_idx, -1)
        formdata_idx = self.ui.find("new FormData()")
        if formdata_idx != -1:
            self.assertNotEqual(plugin_idx, -1)
            # FormData must belong to the plugin upload scope, not the OTA path.
            self.assertLess(abs(formdata_idx - plugin_idx), 256)

    def test_phase2_dashboard_uses_explicit_contract_endpoints(self) -> None:
        expected = [
            "/mode_hw",
            "/drive_profile",
            "/speed_strategy",
            "/lighting_config",
            "/defense_config",
            "/hotspot_config",
            "/relay_wifi_test",
            "/dns_rules",
        ]
        for endpoint in expected:
            with self.subTest(endpoint=endpoint):
                self.assertIn(endpoint, self.ui)

    def test_bus2_controls_have_state_roundtrip(self) -> None:
        self.assertIn("setText('b2-rx',d.rx_total||0);", self.ui)
        self.assertIn("svc.checked=!!d.service_mode;", self.ui)
        self.assertIn('",\\"service_mode\\":', self.dash)
        self.assertIn('"],\\"rx_total\\":', self.dash)

    def test_stalk_duration_is_honored_by_backend(self) -> None:
        self.assertIn("duration_ms", self.ui)
        self.assertIn('server.hasArg("dur")', self.dash)
        self.assertIn('",\\"duration_ms\\":"', self.dash)

    def test_flash_burst_double_pull_is_wired(self) -> None:
        """Flash Burst should be firmware-timed, default-off, and controlled via /burst."""
        for token in [
            'static volatile bool g_burstEnabled = false;',
            't2canBurstCheckTrigger',
            't2canBurstTick',
            'uint8_t status = (f.data[1] >> 4) & 0x07;',
            'kBurstDoublePullWindowMs = 2000',
            'g_burstPrevStatus != 1 && status == 1',
            't2canSetBurstEnabled',
            't2canSetBurstParams',
            't2canBurstIsRunning',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.main)
        task = re.search(r"static void app_can_task\(void \*\).*?appCanTaskLoops", self.main, re.S)
        self.assertIsNotNone(task)
        self.assertLess(task.group(0).index('t2canBurstTick();'), task.group(0).index('t2canStalkInjectTick();'))
        for token in [
            'server.on("/burst", HTTP_GET, handleBurst);',
            'server.on("/burst", HTTP_POST, handleBurst);',
            'static void handleBurst()',
            '\\"phases_left\\"',
            '\\"last_trigger_ms\\"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)
        for token in [
            '双拨爆闪',
            '启用双拨触发',
            '/burst',
            'loadBurstConfig',
            'setBurstPreset',
            'BURST_PRESETS',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)
                self.assertIn(token if token != '双拨爆闪' else '/burst', self.ui_gen)

    def test_gateway_profile_and_poll_interval_are_functional(self) -> None:
        self.assertIn("var DNS_PROFILES = {", self.ui)
        self.assertIn("function applyDnsProfile(profile)", self.ui)
        self.assertIn("function detectDnsProfile(blacklist,whitelist)", self.ui)
        self.assertIn("function updateDnsProfileCards(profile)", self.ui)
        self.assertIn("var data={enabled:'1'};", self.ui)
        self.assertIn("function restartPoll(ms)", self.ui)
        self.assertIn("pollTick=function()", self.ui)

    def test_phase1_new_endpoints(self) -> None:
        """Phase 1 新增端点应在固件路由中注册，handler 函数存在"""
        new_endpoints = {
            "GET  /power_mgmt": "handlePowerMgmt",
            "POST /power_mgmt": "handlePowerMgmt",
            "GET  /vehicle_ota_status": "handleVehicleOtaStatus",
            "GET  /fog_light": "handleFogLight",
            "POST /fog_light": "handleFogLight",
            "POST /strobe_cont": "handleStrobeCont",
        }
        for endpoint, handler in new_endpoints.items():
            method, path = endpoint.split()
            with self.subTest(endpoint=endpoint):
                self.assertIn(f'"{path}"', self.dash, f"Missing route: {endpoint}")
                self.assertIn(handler, self.dash, f"Missing handler: {handler}")

    def test_phase1_status_fields_exist(self) -> None:
        """Phase 1 新增 /status 字段应在 handleStatus() JSON 中输出"""
        fields = ["vehicleOta", "autoShutdown", "wifiAutoOff", "fogStrategy", "strobeCont", "dndVolume", "dndSpeed"]
        for field in fields:
            with self.subTest(field=field):
                # In C++ source, JSON keys appear as \"fieldName\":
                self.assertIn(f',\\"{field}\\"', self.dash)

    def test_phase1_ota_guard_gates_handler_injection(self) -> None:
        """Handler-level AD checks must include OTA guard, not only dashboard post-processing."""
        check_ad = re.search(r"static bool dashCheckADEnabled\(\).*?\n\}", self.dash, re.S)
        self.assertIsNotNone(check_ad)
        self.assertIn("canActive", check_ad.group(0))
        self.assertIn("dashOtaGuardAllowInjection()", check_ad.group(0))
        self.assertIn("CAN_ID_OTA_STATUS", self.handlers)

    def test_phase1_power_mgmt_partial_update_and_wake_pin(self) -> None:
        """Power management updates should preserve omitted fields and use configurable wake pin."""
        power = re.search(r"static void handlePowerMgmt\(\).*?server\.send\(200", self.dash, re.S)
        self.assertIsNotNone(power)
        body = power.group(0)
        self.assertIn('if (server.hasArg("autoShutdown"))', body)
        self.assertIn('if (server.hasArg("wifiAutoOff"))', body)
        self.assertIn("dashArgTruthy(server.arg(\"autoShutdown\"))", body)
        self.assertIn("dashArgTruthy(server.arg(\"wifiAutoOff\"))", body)
        power_header = (ROOT / "include" / "dash_power_mgmt.h").read_text(encoding="utf-8")
        self.assertIn("DASH_WAKE_PIN", power_header)
        self.assertIn("TWAI_RX_PIN", power_header)
        self.assertIn("dashPowerMgmtConfigureWake()", power_header)

    def test_phase1_lilygo_release_defaults_injection_off(self) -> None:
        """LILYGO release profile should require explicit dashboard arming after boot."""
        platformio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        lilygo = re.search(r"\[env:lilygo_t2can_dual\].*?(?=\n\[env:|\Z)", platformio, re.S)
        self.assertIsNotNone(lilygo)
        self.assertNotIn("DASH_INJECTION_ON_BOOT", lilygo.group(0))


    def test_phase2_speed_strategy_syncs_new_offset_mode(self) -> None:
        """/speed_strategy must drive the new speed algorithm mode and persist it."""
        self.assertIn("offsetMode = dashSpeedStrategy;", self.dash)
        self.assertIn("dashSyncLegacyShims();", self.dash)
        self.assertIn('prefs.putUChar("offsetMode", offsetMode);', self.dash)
        self.assertIn('prefs.putUChar("spd_str", dashSpeedStrategy);', self.dash)

    def test_config_legacy_hw3_speed_flags_sync_new_strategy(self) -> None:
        """Old /config HW3 flags must map into the new offset strategy before syncing shims."""
        config = re.search(r"static void handleConfig\(\).*?static void handleLoggingConfig\(\)", self.dash, re.S)
        self.assertIsNotNone(config)
        body = config.group(0)
        self.assertIn('server.hasArg("hw3AutoSpeed")', body)
        self.assertIn('bool legacyHw3CustomSpeed = hw3CustomSpeed;', body)
        self.assertIn('bool legacyHw3HighSpeedEnable = hw3HighSpeedEnable;', body)
        self.assertIn('bool legacyHw3AutoSpeed = hw3AutoSpeed;', body)
        self.assertIn('if (server.hasArg("hw3CustomSpeed")) legacyHw3CustomSpeed = server.arg("hw3CustomSpeed") == "1";', body)
        self.assertIn('if (server.hasArg("hw3HighSpeedEnable")) legacyHw3HighSpeedEnable = server.arg("hw3HighSpeedEnable") == "1";', body)
        self.assertIn('if (server.hasArg("hw3AutoSpeed")) legacyHw3AutoSpeed = server.arg("hw3AutoSpeed") == "1";', body)
        self.assertNotIn('bool legacyHw3CustomSpeed = server.hasArg("hw3CustomSpeed") && server.arg("hw3CustomSpeed") == "1";', body)
        self.assertNotIn('bool legacyHw3HighSpeedEnable = server.hasArg("hw3HighSpeedEnable") && server.arg("hw3HighSpeedEnable") == "1";', body)
        self.assertNotIn('bool legacyHw3AutoSpeed = server.hasArg("hw3AutoSpeed") && server.arg("hw3AutoSpeed") == "1";', body)
        self.assertIn('dashSpeedStrategy = legacyHw3CustomSpeed ? 2 : ((legacyHw3HighSpeedEnable || legacyHw3AutoSpeed) ? 1 : 0);', body)
        self.assertIn('offsetMode = dashSpeedStrategy;', body)
        self.assertIn('dashSyncLegacyShims();', body)

    def test_hw3_mux2_runtime_uses_new_offset_algorithm_as_single_source(self) -> None:
        """HW3 mux-2 active raw must come from dashComputeHw3OffsetRaw/dashComputeOffset for fixed/auto/custom consistency."""
        mux2 = re.search(r"// ── Mux 2: Speed offset.*?if \(index == 0 && enablePrint\)", self.handlers, re.S)
        self.assertIsNotNone(mux2)
        body = mux2.group(0)
        self.assertRegex(body, r"uint8_t\s+activeRaw\s*=\s*dashComputeHw3OffsetRaw\([^;]+\);")
        self.assertLess(body.index("dashComputeHw3OffsetRaw"), body.index("hw3OffsetTargetRaw = activeRaw;"))
        self.assertNotIn("dashComputeHw3CustomTargetKph", body)
        self.assertNotIn("dashComputeHw3AutoTargetKph", body)
        self.assertNotIn("hw3HighSpeedTargetPct", body)

    def test_speed_strategy_does_not_enable_legacy_mpp(self) -> None:
        """/speed_strategy drives the shared 3-mode algorithm, not the old Legacy MPP path."""
        speed_strategy = re.search(r"static void handleSpeedStrategy\(\).*?static String dashSpeedCustomJson\(\)", self.dash, re.S)
        self.assertIsNotNone(speed_strategy)
        body = speed_strategy.group(0)
        self.assertNotIn('legacyMppCustomEnable = true;', body)
        self.assertNotIn('legacyMppCustomEnable =', body)
        self.assertIn('offsetMode = dashSpeedStrategy;', body)
        self.assertIn('dashSyncLegacyShims();', body)

    def test_legacy_can760_is_read_only_and_3ee_is_output(self) -> None:
        """Legacy 0x2F8 is telemetry only; the offset is written on native 0x3EE mux0."""
        can760 = re.search(r"if \(frame\.id == 760\).*?if \(frame\.id == 1080\)", self.handlers, re.S)
        self.assertIsNotNone(can760)
        body = can760.group(0)
        self.assertIn('legacySpeedDiag.gpsUserOffsetRaw = frame.data[5] & 0x3F;', body)
        self.assertIn('legacySpeedDiag.gpsMppLimitRaw = frame.data[6] & 0x1F;', body)
        self.assertNotIn('driver.send(', body)
        mux0 = re.search(r"if \(index == 0\).*?else if \(index == 1\)", self.handlers, re.S)
        self.assertIsNotNone(mux0)
        self.assertIn('dashWriteLegacyOffsetTo3eeMux0', mux0.group(0))
        self.assertIn('legacySpeedBus.accept(frame, nowMs)', mux0.group(0))

    def test_legacy_handler_captures_fused_speed_limit_from_921(self) -> None:
        """Legacy/HW0 needs the same fused speed limit input as the 3-mode speed UI."""
        can921 = re.search(r"if \(frame\.id == 921\).*?// 0x3EE", self.handlers, re.S)
        self.assertIsNotNone(can921)
        body = can921.group(0)
        self.assertIn('fusedSpeedLimitRaw = static_cast<uint8_t>(frame.data[1] & 0x1F);', body)
        self.assertIn('APActive = legacyNagApContextActive(apState);', body)

    def test_legacy_smart_offset_has_independent_four_mode_state(self) -> None:
        """Legacy Off/Manual/Auto/Custom must not be overwritten by HW3/HW4 strategy."""
        self.assertIn('static LegacySmartOffsetMode dashLegacySmartMode', self.dash)
        self.assertIn('static uint8_t dashLegacyManualOffsetKph', self.dash)
        self.assertIn('static uint8_t dashLegacyCustomPct[4]', self.dash)
        apply = re.search(r"static void dashApplyLegacySmartSpeedState\(CarManagerBase \*handler\).*?^}", self.dash, re.S | re.M)
        self.assertIsNotNone(apply)
        self.assertIn('handler->legacySmartOffsetConfig.mode = dashLegacySmartMode;', apply.group(0))
        self.assertIn('handler->legacySmartOffsetConfig.manualOffsetKph = dashLegacyManualOffsetKph;', apply.group(0))
        self.assertNotIn('dashSpeedStrategy ==', apply.group(0))
        self.assertIn('dashWriteLegacyOffsetTo3eeMux0', self.legacy_smart_speed)
        self.assertIn('if (v > 33)', self.legacy_smart_speed)

    def test_legacy_speed_config_contract_and_persistence(self) -> None:
        """Legacy bus routing and smoothing use strict API and persisted keys."""
        self.assertIn('server.on("/legacy_speed_config", HTTP_GET, handleLegacySpeedConfig);', self.dash)
        self.assertIn('server.on("/legacy_speed_config", HTTP_POST, handleLegacySpeedConfig);', self.dash)
        self.assertIn('prefs.putBool("lo_smooth", dashLegacySmoothDown);', self.dash)
        self.assertIn('prefs.putUChar("lo_rate", dashLegacySmoothRateKphS);', self.dash)
        self.assertIn('prefs.putUChar("lo_bus", static_cast<uint8_t>(dashLegacySpeedBusMode));', self.dash)
        for key in ('"lo_mode"', '"lo_man"', '"lo_cp0"', '"lo_cp1"', '"lo_cp2"', '"lo_cp3"'):
            self.assertIn(key, self.dash)
        config = re.search(r"static void handleLegacySpeedConfig\(\).*?static String dashLightingConfigJson\(\)", self.dash, re.S)
        self.assertIsNotNone(config)
        self.assertIn('dashArgUIntInRange("smooth_rate_kph_s", 1, 20, rate)', config.group(0))
        self.assertIn('mode must be off, manual, auto, or custom', config.group(0))
        self.assertIn('dashArgUIntInRange("manual_offset_kph", 0, 33, value)', config.group(0))
        self.assertIn('dashArgUIntInRange(key.c_str(), 0, 63, value)', config.group(0))
        self.assertNotIn('.toInt()', config.group(0))
        response = re.search(r"static String dashLegacySpeedConfigJson\(\).*?^}", self.dash, re.S | re.M)
        self.assertIsNotNone(response)
        self.assertIn('j += ",\\\"smooth_down\\\":";', response.group(0))
        self.assertNotIn('j += "\\\",\\\"smooth_down\\\":";', response.group(0))

    def test_legacy_speed_ui_exposes_four_mode_controls(self) -> None:
        """Vehicle Legacy UI must expose its distinct four-mode API surface."""
        for token in (
            'id="legacy-speed-mode"',
            "setLegacySpeedMode('off')",
            "setLegacySpeedMode('manual')",
            "setLegacySpeedMode('auto')",
            "setLegacySpeedMode('custom')",
            'id="legacy-speed-manual"',
            'id="legacy-speed-cp1"',
            'id="legacy-speed-cp4"',
            'manual_offset_kph:String(legacySpeedVal',
            'mode:legacySpeedModeState',
            'applyLegacySpeedConfig(d)',
        ):
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_status_exposes_legacy_smart_speed_runtime(self) -> None:
        """Status separates configured, runtime, source, bus, and failure state."""
        for token in (
            '"legacySmartSpeed"',
            '"runtimeActive"',
            '"limitSource"',
            '"targetBus"',
            '"busAmbiguous"',
            '"blockedReason"',
            '"offsetOnlyTxOk"',
        ):
            self.assertIn(token, self.dash)
        self.assertIn('fusedSpeedLimitRaw', self.legacy_speed)
        self.assertIn('dashComputeOffset(limitKph, 0.05f)', self.legacy_speed)
        self.assertIn('dashClampLegacySimpleOffsetKph', self.legacy_speed)

    def test_phase2_speed_custom_endpoint_contract(self) -> None:
        """/speed_custom exposes four validated custom percentage zones."""
        self.assertIn("static bool dashArgUIntInRange", self.dash)
        self.assertIn("static void handleSpeedCustomGet()", self.dash)
        self.assertIn("static void handleSpeedCustom()", self.dash)
        self.assertIn('server.on("/speed_custom", HTTP_GET, handleSpeedCustomGet);', self.dash)
        self.assertIn('server.on("/speed_custom", HTTP_POST, handleSpeedCustom);', self.dash)
        for idx, arg in enumerate(["cp1", "cp2", "cp3", "cp4"]):
            with self.subTest(arg=arg):
                self.assertIn(f'server.hasArg("{arg}")', self.dash)
                self.assertIn(f'dashArgUIntInRange("{arg}", 0, 50, next)', self.dash)
                self.assertIn(f'nextCustomPct[{idx}] = next;', self.dash)
        self.assertIn('server.hasArg("manualPct")', self.dash)
        self.assertIn('dashArgUIntInRange("manualPct", 0, 50, next)', self.dash)
        self.assertIn('nextManualPct = next;', self.dash)
        self.assertIn('manualOffsetPct = nextManualPct;', self.dash)
        self.assertIn('prefs.putUChar("cp0", customPct[0]);', self.dash)
        self.assertIn('\\"manualPct\\":', self.dash)
        self.assertIn('\\"customPct\\":[', self.dash)

    def test_speed_custom_get_is_always_read_only(self) -> None:
        """GET /speed_custom must ignore query args and never persist."""
        get_handler = re.search(r"static void handleSpeedCustomGet\(\).*?static void handleSpeedCustom\(\)", self.dash, re.S)
        self.assertIsNotNone(get_handler)
        body = get_handler.group(0)
        self.assertIn("dashSpeedCustomJson()", body)
        self.assertNotIn("dashSavePrefs", body)
        self.assertNotIn("server.hasArg", body)
        self.assertNotIn("server.method()", body)

        post_handler = re.search(r"static void handleSpeedCustom\(\).*?static String dashLightingConfigJson\(\)", self.dash, re.S)
        self.assertIsNotNone(post_handler)
        self.assertNotIn("server.method()", post_handler.group(0))

    def test_speed_custom_rejects_invalid_values_before_saving(self) -> None:
        """POST /speed_custom must reject non-decimal or out-of-range values atomically."""
        speed_custom = re.search(r"static void handleSpeedCustom\(\).*?static String dashLegacySpeedConfigJson\(\)", self.dash, re.S)
        self.assertIsNotNone(speed_custom)
        body = speed_custom.group(0)
        self.assertIn('server.send(400, "application/json", "{\\"ok\\":false,\\"error\\":\\"manualPct/cp1..cp4 must be decimal integers from 0 to 50\\"}");', body)
        self.assertIn('bool valid = true;', body)
        self.assertLess(body.index('bool valid = true;'), body.index('if (!valid)'))
        self.assertLess(body.index('if (!valid)'), body.index('dashSavePrefs();'))
        self.assertNotIn('.toInt()', body)

    def test_phase2_status_exposes_actual_offset_and_speed_limit(self) -> None:
        """/status must surface actual offset and fused speed limit in kph, with SNA/NONE as 0."""
        self.assertIn(',\\"actOffset\\":', self.dash)
        self.assertIn('actualOffset', self.dash)
        self.assertIn(',\\"speedLimit\\":', self.dash)
        self.assertIn('(fusedSpeedLimitRaw == 0 || fusedSpeedLimitRaw == 31)', self.dash)
        self.assertIn('(uint16_t)fusedSpeedLimitRaw * 5', self.dash)

    def test_phase2_speed_offset_ui_uses_new_three_mode_contract(self) -> None:
        """Speed page must use the fixed/auto/custom UI and sync the Phase 2 APIs."""
        required_ui = [
            'id="speed-mode-tabs"',
            "showSpeedMode('fixed')",
            "showSpeedMode('auto')",
            "showSpeedMode('custom')",
            'id="speed-panel-fixed"',
            'id="speed-panel-auto"',
            'id="speed-panel-custom"',
            "固定百分比",
            "自动偏移",
            "自定义偏移",
            "0-50 km/h",
            "51-70 km/h",
            "71-100 km/h",
            "101+ km/h",
            'id="speed-cp1"',
            'id="speed-cp2"',
            'id="speed-cp3"',
            'id="speed-cp4"',
            'id="sp-limit"',
            'id="sp-act-offset"',
            'id="sp-active-mode"',
            'id="sp-wire"',
            "/speed_strategy",
            "/speed_custom",
            "manualPct",
            "cp1",
            "cp2",
            "cp3",
            "cp4",
            "loadSpeedStrategy()",
            "saveSpeedCustom()",
            "setSpeedFixedPct(30)",
            "setText('sp-limit'",
            "setText('sp-act-offset'",
        ]
        for token in required_ui:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_phase2_auto_speed_algorithm_card_shows_actual_five_segments(self) -> None:
        """Auto speed card must document the firmware's actual five-segment algorithm."""
        panel_match = re.search(r'<div class="card" id="speed-panel-auto".*?</div>\s*<div class="card" id="speed-panel-custom"', self.ui, re.S)
        self.assertIsNotNone(panel_match)
        panel = panel_match.group(0)
        expected_rows = [
            ("≤ 40 km/h", "+50%，封顶 60"),
            ("≤ 60 km/h", "+50%，封顶 90"),
            ("≤ 90 km/h", "+30%，封顶 117"),
            ("≤ 110 km/h", "+20%，封顶 132"),
            ("> 110 km/h", "+10%，封顶 132"),
        ]
        for speed_limit, auto_offset in expected_rows:
            with self.subTest(speed_limit=speed_limit):
                self.assertIn(f"<tr><td>{speed_limit}</td><td>{auto_offset}</td></tr>", panel)
        self.assertNotIn("≤ 50 km/h", panel)
        self.assertNotIn("51-70 km/h", panel)
        self.assertNotIn("71-100 km/h", panel)
        self.assertNotIn("101+ km/h", panel)

    def test_update_profile_cards_does_not_write_speed_strategy_label(self) -> None:
        """Drive profile UI refresh must not overwrite speed-current, which belongs to speed strategy."""
        profile_cards = re.search(r"function updateProfileCards\(sp\)\{.*?function updateDriveCards", self.ui, re.S)
        self.assertIsNotNone(profile_cards)
        self.assertNotIn("speed-current", profile_cards.group(0))

    def test_phase2_generated_dashboard_header_contains_new_speed_ui(self) -> None:
        """Generated minified header should be refreshed from the new speed UI source."""
        for token in [
            "speed-mode-tabs",
            "speed-cp1",
            "sp-act-offset",
            "/speed_custom",
            "manualPct",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui_gen)

    def test_legacy_speed_algo_branch_exposes_dashboard_offset_state(self) -> None:
        """USE_NEW_SPEED_ALGO=0 rollback path must still compile with dashboard speed API."""
        speed_header = (ROOT / "include" / "dash_hw3_speed.h").read_text(encoding="utf-8")
        legacy_match = re.search(r"#else // !USE_NEW_SPEED_ALGO.*?#endif // USE_NEW_SPEED_ALGO", speed_header, re.S)
        self.assertIsNotNone(legacy_match)
        legacy = legacy_match.group(0)
        expected_symbols = [
            "offsetMode",
            "manualOffsetPct",
            "customPct[4]",
            "actualOffset",
            "dashSyncLegacyShims()",
        ]
        for symbol in expected_symbols:
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, legacy)
        self.assertIn("hw3AutoSpeed = (offsetMode == 1);", legacy)
        self.assertIn("hw3CustomSpeed = (offsetMode == 2);", legacy)
        self.assertIn("hw3HighSpeedEnable = (offsetMode != 0);", legacy)
        self.assertNotIn("hw3HighSpeedEnable = (offsetMode == 2);", legacy)

    def test_ap_status_mode_field_matches_ui(self) -> None:
        self.assertIn("setText('ap-mode',ap.mode||", self.ui)
        self.assertIn('",\\"mode\\":\\""', self.dash)

    # ── Phase 3: Bionic Steering + Wheel DND ──────────────────

    def test_phase3_bionic_steer_header_exists(self) -> None:
        """dash_bionic_steer.h must exist with core API surface."""
        bionic = (ROOT / "include" / "dash_bionic_steer.h").read_text(encoding="utf-8")
        for symbol in ["DashBionicSteer", "DashBionicPRNG", "computePerturbation",
                        "applyToFrame", "beginPhase", "reportFailure", "reportSuccess",
                        "isDisabled", "reset", "kPerturbCap", "kMaxConsecutiveFails"]:
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, bionic)
        # Safety cap
        self.assertIn("kPerturbCap{60}", bionic)
        # Amplitude range
        self.assertIn("kAmplitudeLo{30}", bionic)
        self.assertIn("kAmplitudeHi{55}", bionic)

    def test_phase3_bionic_steer_xorshift32(self) -> None:
        """xorshift32 PRNG must produce deterministic sequence from seed."""
        bionic = (ROOT / "include" / "dash_bionic_steer.h").read_text(encoding="utf-8")
        self.assertIn("s ^= s << 13", bionic)
        self.assertIn("s ^= s >> 17", bionic)
        self.assertIn("s ^= s << 5", bionic)

    def test_phase3_bionic_failure_disables_after_3(self) -> None:
        """3 consecutive failures must auto-disable bionic."""
        bionic = (ROOT / "include" / "dash_bionic_steer.h").read_text(encoding="utf-8")
        self.assertIn("kMaxConsecutiveFails{3}", bionic)
        self.assertIn("consecutiveFails++", bionic)
        self.assertIn("disabled = true", bionic)

    def test_phase3_wheel_dnd_header_exists(self) -> None:
        """dash_wheel_dnd.h must exist with four-step sequence state machine."""
        dnd = (ROOT / "include" / "dash_wheel_dnd.h").read_text(encoding="utf-8")
        for symbol in ["DashWheelDND", "startVolume", "startSpeed", "tick",
                        "isRunning", "reset", "kSteps", "kStepCount{4}",
                        "kCanId{0x3C2}"]:
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, dnd)
        # Sequence: 01→00→3F→00
        self.assertIn("0x01", dnd)
        self.assertIn("0x3F", dnd)
        self.assertIn("kStepIntervalMs{100}", dnd)
        self.assertIn("kNativeMaxAgeMs{1000}", dnd)
        self.assertIn("kReactiveCooldownMs{2000}", dnd)
        self.assertIn("kMaxActionsPerEpisode{3}", dnd)

    def test_phase3_wheel_dnd_preserves_native_frame_fields(self) -> None:
        """DND clones an idle native mux1 and changes only the selected wheel byte."""
        dnd = (ROOT / "include" / "dash_wheel_dnd.h").read_text(encoding="utf-8")
        self.assertIn("memcpy(outData, nativeData", dnd)
        self.assertIn("outData[2] = kSteps", dnd)
        self.assertIn("outData[3] = kSteps", dnd)
        self.assertNotIn("clean_base", dnd)

    def test_phase3_wheel_dnd_is_runtime_wired(self) -> None:
        """Legacy Wheel DND must follow CAN1 0x045 and native 0x399."""
        self.assertIn('#include "dash_legacy_wheel_045_dnd.h"', self.dash)
        self.assertIn("static DashLegacyWheel045Dnd dashLegacyWheel045DndCtrl;", self.dash)
        self.assertIn("dashLegacyWheel045DndCtrl.onDasStatus(apState, handsState", self.dash)
        self.assertIn("dashLegacyWheel045DndCtrl.onCan1Frame", self.dash)
        self.assertIn("dashLegacyWheel045DndCtrl.observeGatewayMirror", self.dash)
        self.assertIn("f.id == 0x399", self.dash)
        self.assertIn("static void t2canWheelDndTick()", self.main)
        self.assertIn("dashLegacyWheel045DndCtrl.configure(dashDndVolume)", self.main)
        self.assertIn("dashLegacyWheel045DndCtrl.nextFrame(now, data)", self.main)
        self.assertIn("f.id = DashLegacyWheel045Dnd::kCanId;", self.main)
        self.assertIn("appDriver->send(f)", self.main)
        self.assertIn("dashLegacyHandlerActive()", self.main)
        self.assertIn("vehicleOtaActive", self.main)
        self.assertIn("recordTxResult(ok, now)", self.main)
        can_task = re.search(r"static void app_can_task\(void \*\).*?appCanTaskLoops", self.main, re.S)
        self.assertIsNotNone(can_task)
        self.assertIn("t2canWheelDndTick();", can_task.group(0))

    def test_phase3_wheel_dnd_has_independent_legacy_gate(self) -> None:
        """Legacy 0x045 DND requires its own switch, Legacy, TWAI and no OTA."""
        tick = re.search(r"static void t2canWheelDndTick\(\).*?\n}\n", self.main, re.S)
        self.assertIsNotNone(tick)
        body = tick.group(0)
        self.assertIn("!canActive || !dashDndVolume", body)
        self.assertNotIn("dashDefenseEnabled", body)
        self.assertIn("!dashLegacyHandlerActive()", body)
        self.assertIn("vehicleOtaActive", body)
        self.assertIn("DashLegacyWheel045Block::TwaiUnavailable", body)
        self.assertNotIn("appDriverSecondary", body)

    def test_phase3_naghandler_bionic_branch(self) -> None:
        """NagHandler must branch on bionicSteering with fallback."""
        nag = re.search(r"struct NagHandler.*?^\};", self.handlers, re.S | re.M)
        self.assertIsNotNone(nag)
        body = nag.group(0)
        # Must check bionicSteering flag
        self.assertIn("bionicSteering", body)
        # Must have DashBionicSteer instance
        self.assertIn("DashBionicSteer bionic;", body)
        # Must use bionic path
        self.assertIn("bionic.beginPhase()", body)
        self.assertIn("bionic.computePerturbation()", body)
        self.assertIn("bionic.applyToFrame(", body)
        # Must have failure reporting
        self.assertIn("bionic.reportFailure()", body)
        self.assertIn("bionic.reportSuccess()", body)
        # Must still have legacy echo fallback
        self.assertIn("0xB6", body)

    def test_dashboard_runtime_state_syncs_defense_to_handlers(self) -> None:
        """Loaded NVS/UI defense state must reach active handler and handlerPool."""
        runtime = re.search(r"static void dashApplyRuntimeState\(\).*?#if defined\(DASH_RGB_STATUS_LED\)", self.dash, re.S)
        self.assertIsNotNone(runtime)
        runtime_body = runtime.group(0)
        for token in [
            "dashHandler->bionicSteering = dashBionicSteering",
            "dashHandler->isaChimeSuppress = nvsIsaChimeSuppress",
            "dashHandler->banShieldEnable = nvsBanShieldEnable",
            "dashHandler->legacyOffset = nvsLegacyOffset",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, runtime_body)

        nvs_sync = re.search(r"static void dashApplyNvsRuntimeSwitches\(\).*?\n}\n", self.dash, re.S)
        self.assertIsNotNone(nvs_sync)
        nvs_body = nvs_sync.group(0)
        self.assertIn("handlerPool[i]->bionicSteering = dashBionicSteering", nvs_body)
        self.assertIn("handlerPool[i]->banShieldEnable = nvsBanShieldEnable", nvs_body)
        self.assertIn("dashApplyNvsRuntimeSwitches();\n    dashSwapHandler(hwMode);", self.dash)

    def test_phase3_defense_config_exposes_dnd_params(self) -> None:
        """defense_config keeps Legacy DND and experimental safety controls wired."""
        defense = re.search(r"static void handleDefenseConfig\(\).*?server\.send\(200", self.dash, re.S)
        self.assertIsNotNone(defense)
        body = defense.group(0)
        self.assertIn('server.hasArg("dnd_volume")', body)
        self.assertIn('server.hasArg("legacy_scroll_dnd")', body)
        self.assertIn('server.hasArg("dnd_speed")', body)
        self.assertIn("dashDndVolume", body)
        self.assertIn("dashDndSpeed", body)
        self.assertIn("prevDndSpeed", body)
        self.assertIn('server.hasArg("abort_guard")', body)
        self.assertIn("dashAbortGuardEnabled", body)
        # #108 steer-jerk defense removed in 4.5.0-beta02: these keys must NOT
        # be accepted, persisted, or echoed anymore (anti-regression).
        self.assertNotIn('server.hasArg("minimal_inject")', body)
        self.assertNotIn("dashMinimalInjectEnabled", body)
        self.assertNotIn('server.hasArg("instant_engage")', body)
        self.assertNotIn('server.hasArg("soft_engage")', body)
        self.assertNotIn("dashInstantEngageEnabled", body)
        self.assertNotIn("dashSoftEngageEnabled", body)

    def test_phase3_defense_config_json_includes_bionic_status(self) -> None:
        """defense_config JSON must report bionic_disabled for UI warning."""
        json_fn = re.search(r"static String dashDefenseConfigJson\(\).*?return j;", self.dash, re.S)
        self.assertIsNotNone(json_fn)
        body = json_fn.group(0)
        self.assertIn('\\"bionic_disabled\\"', body)
        self.assertIn('\\"dnd_volume\\"', body)
        self.assertIn('\\"legacy_scroll_dnd\\"', body)
        self.assertIn('\\"dnd_speed\\"', body)
        self.assertIn('\\"abort_guard\\"', body)
        # #108 steer-jerk defense keys removed in 4.5.0-beta02.
        self.assertNotIn('\\"minimal_inject\\"', body)
        self.assertNotIn('\\"instant_engage\\"', body)
        self.assertNotIn('\\"soft_engage\\"', body)
        self.assertIn("dashAppendLegacySafetyControlsJson", body)
        self.assertIn("bionicDisabled()", body)
        self.assertIn("dashBionicDisabled", body)

    def test_phase3_defense_ui_has_legacy_045_toggle(self) -> None:
        """Defense page keeps active controls and removes Legacy 0x3C2 speed DND.
        """
        defense_page = re.search(r'id="pg-defense".*?id="pg-ota"', self.ui, re.S)
        self.assertIsNotNone(defense_page)
        body = defense_page.group(0)
        toggles = [
            'id="def-master-tgl"',
            'id="hw3-slew-tgl"',
            'id="def-bionic-tgl"',
            'id="def-sound-tgl"',
            'id="def-dnd-vol-tgl"',
            'id="def-speed-nd-tgl"',
            'id="def-apeap-tgl"',
            'id="def-abort-guard-tgl"',
        ]
        self.assertEqual(len(toggles), 8)
        for tid in toggles:
            with self.subTest(toggle=tid):
                self.assertIn(tid, body)
        # #108 steer-jerk defense removed in 4.5.0-beta02 (anti-regression):
        # none of its UI controls may come back.
        for gone in ('id="def-minimal-inject-tgl"',
                     'id="def-instant-engage-tgl"',
                     'id="def-soft-engage-tgl"',
                     'id="def-steer-state"'):
            with self.subTest(removed=gone):
                self.assertNotIn(gone, body)
        # 8.3.6 anti-jerk coordinator card (renamed from AP cancel/re-request).
        self.assertIn("8.3.6防甩机制", body)
        self.assertNotIn("AP 取消重请求", body)
        self.assertNotIn('id="def-dnd-spd-tgl"', body)
        self.assertIn("CAN1 原生 0x045", body)
        # Legacy NAG reuses the existing persisted bionic switch/API contract.
        self.assertNotIn('id="def-epnag-tgl"', body)

    def test_phase3_defense_ui_legacy_nag_runtime_status(self) -> None:
        """UI must expose verified Legacy NAG runtime state without an experiment badge."""
        self.assertIn("Legacy 0x370 NAG 主路径", self.ui)
        for element_id in ["legacy-nag-mode", "legacy-nag-bus", "legacy-nag-rxtx",
                           "legacy-nag-collision", "legacy-nag-state"]:
            self.assertIn(f'id="{element_id}"', self.ui)
        self.assertIn("updateLegacyNagStatus", self.ui)

    def test_legacy_scroll_dnd_diagnostics_are_exposed(self) -> None:
        for token in [
            '"nativeMux1IdleSeen"', '"handsState"', '"warningEpisodes"',
            '"triggerCount"', '"actionsThisEpisode"', '"cooldownRemainingMs"',
            '"positiveThisEpisode"', '"compensationPending"',
            '"volumeNeutralEpisodes"', '"blockedReason"', '"autoDisarmed"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)

    def test_phase3_defense_js_saves_legacy_045_dnd(self) -> None:
        """saveDefenseConfig JS posts the active Legacy left-wheel switch."""
        save_fn = re.search(r"async function saveDefenseConfig\(\)\{.*?\}", self.ui, re.S)
        self.assertIsNotNone(save_fn)
        body = save_fn.group(0)
        self.assertIn("def-dnd-vol-tgl", body)
        self.assertIn("dnd_volume", body)

    def test_phase3_defense_js_loads_dnd_params(self) -> None:
        """loadDefenseConfig JS must read dnd_volume and dnd_speed."""
        load_fn = re.search(r"async function loadDefenseConfig\(\)\{.*?setText\('tb-exp'", self.ui, re.S)
        self.assertIsNotNone(load_fn)
        body = load_fn.group(0)
        self.assertIn("d.dnd_volume", body)
        self.assertIn("d.dnd_speed", body)
        self.assertIn("def-dnd-vol-tgl", body)

    def test_phase3_bionicsteering_in_car_manager_base(self) -> None:
        """bionicSteering must be in CarManagerBase for dashboard access."""
        base = re.search(r"struct CarManagerBase.*?virtual ~CarManagerBase", self.handlers, re.S)
        self.assertIsNotNone(base)
        body = base.group(0)
        self.assertIn("Shared<bool> bionicSteering{false}", body)
        self.assertIn("bionicDisabled()", body)
        self.assertIn("resetBionic", body)

    def test_phase3_defense_runtime_and_persistence_are_wired(self) -> None:
        """Defense config should drive Nag/Bionic runtime and persist DND switches."""
        self.assertIn("nagKillerRuntime = canActive;", self.dash)
        self.assertNotIn("nagKillerRuntime = canActive && dashDefenseEnabled", self.dash)
        self.assertNotIn("hw3OffsetSlew = dashDefenseEnabled", self.dash)
        self.assertIn("dashHandler->resetBionic((uint32_t)millis())", self.dash)
        self.assertIn('prefs.putBool("def_dv", dashDndVolume);', self.dash)
        self.assertIn('prefs.putBool("def_ds", dashDndSpeed);', self.dash)
        self.assertIn('dashDndVolume = prefs.getBool("def_dv", false);', self.dash)
        self.assertIn('dashDndSpeed = prefs.getBool("def_ds", false);', self.dash)
        self.assertIn("slew_rate_enabled", self.dash)
        self.assertIn("defenseEnabled", self.dash)
        self.assertIn("bionicSteering", self.dash)
        self.assertIn("counterCollisionObserved", self.dash)
        self.assertIn("expectedNativeFollowerObserved", self.dash)
        self.assertIn("legacy-nag-collision", self.ui)
        status = re.search(r"static void handleStatus\(\).*?server\.send", self.dash, re.S)
        self.assertIsNotNone(status)
        self.assertIn("dashDndSpeed ? \"true\" : \"false\"", status.group(0))
        wheel_diag = re.search(r"appendWheelDndDiagJson\(String &j, unsigned long now\).*?\n}", self.dash, re.S)
        self.assertIsNotNone(wheel_diag)
        self.assertIn("dndActionType", wheel_diag.group(0))

    def test_phase3_handlers_includes_bionic_header(self) -> None:
        """handlers.h must include dash_bionic_steer.h."""
        self.assertIn('#include "dash_bionic_steer.h"', self.handlers)

    def test_legacy_continuous_nag_handler_and_status_contract(self) -> None:
        """Legacy must own Continuous Absolute v4 with beta1 UI/NVS controls."""
        legacy = re.search(r"struct LegacyHandler.*?^\};", self.handlers, re.S | re.M)
        self.assertIsNotNone(legacy)
        body = legacy.group(0)
        for token in [
            "DashLegacyNagV4 legacyNagV4",
            "DashNagBusLock legacyNagBus",
            "handleLegacyNag370",
            "legacyNagRuntimeAllowed",
            "legacyNagChecksum",
            "driver.send(echo)",
        ]:
            self.assertIn(token, body)
        for token in [
            '"legacyNag"',
            '"verified":false',
            '"vehicleTest":"pending_beta16_vehicle_test"',
            '"gearGateRequired":false',
            '"apContextStatusMax":6',
            '"strategy":"continuous_absolute"',
            '"torqueModel":"modec_488_one_sided"',
            '"directionSource":"0x488"',
            '"blockedReason"',
            '"tx370"',
            '"coveragePct"',
            '"txRetryOk"',
            "dashLegacyNagBusName",
        ]:
            self.assertIn(token, self.dash)

    def test_legacy_fsd_activation_honors_optional_strict_ap_gate(self) -> None:
        """Legacy AP gate must use 0x399 APActive, never Parked/Summoning."""
        fn = re.search(
            r"static bool dashLegacyFsdActivationAllowed\(uint32_t nowMs\)\s*\{.*?^}",
            self.dash,
            re.S | re.M,
        )
        self.assertIsNotNone(fn)
        body = fn.group(0)
        self.assertIn("!canActive", body)
        self.assertIn("!dashOtaGuardAllowInjection()", body)
        self.assertIn("dashLegacyApInjectionAllowed()", body)
        # 4.5.0-beta06: the AP-settle delay chain was removed (the 8.3.6
        # coordinator owns injection timing); the gate now allows immediately
        # once 0x399 confirms AP active.
        self.assertNotIn("legacyFsdApSettleTiming", body)
        self.assertNotIn("legacyFsdRequiredStableMs", body)
        self.assertIn("legacyFsdLastAllowed = true", body)
        strict_gate = re.search(
            r"static bool dashLegacyApInjectionAllowed\(\)\s*\{.*?^}",
            self.dash,
            re.S | re.M,
        )
        self.assertIsNotNone(strict_gate)
        strict_gate_body = strict_gate.group(0)
        self.assertIn("!apInjectionGate", strict_gate_body)
        self.assertIn("dashHandler->APActive", strict_gate_body)
        self.assertNotIn("injectionGateOpen", strict_gate_body)
        self.assertNotIn("Parked", strict_gate_body)
        self.assertNotIn("Summoning", strict_gate_body)
        check_ad = re.search(r"static bool dashCheckADEnabled\(\)\s*\{.*?^}", self.dash, re.S | re.M)
        self.assertIsNotNone(check_ad)
        self.assertIn("dashLegacyFsdActivationAllowed(millis())", check_ad.group(0))
        self.assertIn("activationGateMode", self.dash)
        self.assertIn("explicit_trigger_can_ota", self.dash)
        self.assertIn("legacy_ap_state_gate", self.dash)
        # 4.5.0-beta06: the delay-gate variant left with the settle feature.
        self.assertNotIn("legacy_ap_state_delay_gate", self.dash)
        self.assertIn("legacyIgnoresApGate", self.dash)
        self.assertIn("dashLegacyHandlerActive()", self.dash)
        self.assertIn("dashLegacyHandlerActive() ? dashLegacyApInjectionAllowed()", self.dash)

    # ── Phase 4: Light Stunt System ───────────────────────────

    def test_phase4_fog_light_header_exists(self) -> None:
        """dash_fog_light.h must exist with core API surface."""
        fog = (ROOT / "include" / "dash_fog_light.h").read_text(encoding="utf-8")
        for symbol in ["DashFogLight", "startStrobe", "startF1Pilot",
                        "startContinuous", "stop", "tick", "buildFrame",
                        "isActive", "kModeOff", "kModeStrobe",
                        "kModeF1Pilot", "kModeContinuous"]:
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, fog)

    def test_phase4_fog_light_safety_gear_check(self) -> None:
        """Fog light must auto-stop when gearRaw != 4 (Drive)."""
        fog = (ROOT / "include" / "dash_fog_light.h").read_text(encoding="utf-8")
        self.assertIn("gearRaw != 4", fog)
        self.assertIn("if (isActive()) stop()", fog)

    def test_phase4_fog_light_uses_can273_constants(self) -> None:
        """Fog light must use the 0x273 CAN frame constants from can_frame_types.h."""
        fog = (ROOT / "include" / "dash_fog_light.h").read_text(encoding="utf-8")
        # Includes can_frame_types.h which defines CAN_ID_REAR_FOG_LIGHT
        self.assertIn("can_frame_types.h", fog)
        self.assertIn("FOG_BASE_2_ON", fog)
        self.assertIn("FOG_BASE_2_OFF", fog)

    def test_phase4_fog_light_checksum(self) -> None:
        """Fog frames must include checksum calculation for 0x273."""
        fog = (ROOT / "include" / "dash_fog_light.h").read_text(encoding="utf-8")
        self.assertIn("0x73u + 0x02u", fog)  # CAN ID 0x273 bytes
        self.assertIn("data[7]", fog)         # checksum byte

    def test_phase4_fog_light_f1_timing(self) -> None:
        """F1 pilot mode must use 135ms flash + 1500ms pause."""
        fog = (ROOT / "include" / "dash_fog_light.h").read_text(encoding="utf-8")
        self.assertIn("kF1FlashDurationMs{135}", fog)
        self.assertIn("kF1PauseMs{1500}", fog)
        self.assertIn("kF1FlashCount{3}", fog)

    def test_phase4_fog_handler_has_trigger_param(self) -> None:
        """/fog_light must accept trigger parameter for execution."""
        fog_handler = re.search(r"static void handleFogLight\(\).*?server\.send", self.dash, re.S)
        self.assertIsNotNone(fog_handler)
        body = fog_handler.group(0)
        self.assertIn('server.hasArg("trigger")', body)
        self.assertIn("dashFogCtrl.startStrobe(", body)
        self.assertIn("dashFogCtrl.startF1Pilot(", body)
        self.assertIn("dashFogCtrl.startContinuous(", body)
        self.assertIn("dashFogOffRequested = true", body)
        self.assertIn("dashFogCtrl.isActive()", body)
        self.assertIn('driverSupported', body)
        self.assertIn('reason', body)
        self.assertIn('"driver_not_supported"', body)

    def test_phase4_strobe_cont_is_functional(self) -> None:
        """/strobe_cont must be functional, not a stub."""
        strobe_handler = re.search(r"static void handleStrobeCont\(\).*?server\.send", self.dash, re.S)
        self.assertIsNotNone(strobe_handler)
        body = strobe_handler.group(0)
        self.assertNotIn('"Phase 4"', body)
        self.assertIn("dashFogCtrl.startStrobe(0", body)  # 0 = infinite
        self.assertIn("dashFogOffRequested = true", body)
        self.assertIn('driverSupported', body)
        self.assertIn('reason', body)

    def test_phase4_status_strobeCont_is_dynamic(self) -> None:
        """/status strobeCont must reflect actual state, not hardcoded."""
        # The old code was: j += ",\"strobeCont\":false";
        # The new code uses dashFogCtrl.isActive()
        self.assertNotIn('"strobeCont\\":false', self.dash)
        self.assertIn("dashFogCtrl.isActive()", self.dash)

    def test_phase4_dashboard_includes_fog_light_header(self) -> None:
        """Dashboard must include dash_fog_light.h."""
        self.assertIn('#include "dash_fog_light.h"', self.dash)

    def test_phase4_dashboard_has_fog_ctrl_instance(self) -> None:
        """Dashboard must have a DashFogLight instance."""
        self.assertIn("DashFogLight dashFogCtrl", self.dash)
        self.assertIn("dashFogOffRequested", self.dash)

    def test_phase4_fog_fail_off_is_owned_by_can_task(self) -> None:
        """CAN task should send a final OFF frame on stop or unsafe/stale gear."""
        self.assertIn("gearMs", self.dash)
        self.assertIn("t2canGearIsFreshDrive", self.main)
        self.assertIn("kT2canGearFreshMs", self.main)
        self.assertIn("dashFogCtrl.buildFrame(offData, false)", self.main)
        self.assertIn("CAN_ID_REAR_FOG_LIGHT", self.main)
        self.assertIn("dashFogOffRequested || (active && !safeGear)", self.main)

    def test_phase4_ui_has_strobe_page(self) -> None:
        """UI must have pg-strobe page with all controls."""
        strobe_page = re.search(r'id="pg-strobe".*?id="pg-defense"', self.ui, re.S)
        self.assertIsNotNone(strobe_page)
        body = strobe_page.group(0)
        for element in ['id="fog-strategy"', 'id="strobe-count"',
                        'id="strobe-freq"', 'fogTrigger(\'strobe\')',
                        'fogTrigger(\'f1\')', 'fogTrigger(\'continuous\')',
                        'fogTrigger(\'stop\')', 'id="strobe-status"',
                        'id="strobe-gear"']:
            with self.subTest(element=element):
                self.assertIn(element, body)

    def test_phase4_ui_sidebar_has_stroke_nav(self) -> None:
        """Sidebar must have pg-stroke navigation item."""
        self.assertIn('data-page="pg-strobe"', self.ui)
        self.assertIn('灯光特技', self.ui)

    def test_phase4_ui_mobile_nav_has_stroke(self) -> None:
        """Mobile nav must have pg-stroke item."""
        # Count mobile nav items for pg-strobe
        self.assertEqual(self.ui.count('data-page="pg-strobe"'), 2)  # sidebar + mobile

    def test_phase4_js_has_fog_functions(self) -> None:
        """JS must have loadStrobePage, fogTrigger, saveFogStrategy."""
        for fn in ["loadStrobePage", "fogTrigger", "saveFogStrategy"]:
            with self.subTest(fn=fn):
                self.assertIn(f"async function {fn}", self.ui)
        self.assertIn("这里仅保存默认策略", self.ui)
        self.assertIn("这些按钮才会触发实际灯光动作", self.ui)
        self.assertIn("fetchJson('/fog_light')", self.ui)

    def test_phase4_js_navigates_to_strobe_page(self) -> None:
        """Page navigation must load strobe page data when lighting bus is supported (gated off for single-CAN)."""
        self.assertIn("pageId==='pg-strobe'&&CAP.lightingBusSupported&&!isSingleCan())loadStrobePage()", self.ui)

    def test_phase5a_shift_page_loads_read_only_telemetry(self) -> None:
        """Auto-shift placeholder page must populate its read-only telemetry fields (gated off for single-CAN)."""
        self.assertIn("/gear_assist_status", self.ui)
        self.assertIn("async function pollGearAssist()", self.ui)
        self.assertIn("pageId==='pg-shift'&&!isSingleCan())pollGearAssist()", self.ui)
        self.assertIn("pid==='pg-shift'&&!isSingleCan())pollGearAssist()", self.ui)
        for token in ["shift-speed", "shift-gear", "shift-brake", "shift-fsd"]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_phase5a_drive_profile_preserves_six_modes(self) -> None:
        """Drive UI should use driveProfile/driveProfileName so Auto/Sloth/MAX survive polling."""
        self.assertIn("function driveModeFromProfile(profile,name)", self.ui)
        self.assertIn("driveModeFromProfile(d.driveProfile,d.driveProfileName)", self.ui)
        self.assertIn("driveMap[savedMode]!==undefined?driveMap[savedMode]:3", self.ui)
        self.assertNotIn("[mode]||3", self.ui)

    def test_drive_style_selection_persists_and_updates_home_status(self) -> None:
        """Drive style cards must POST, persist returned mode, and mirror it to the overview status."""
        for token in [
            "function updateDriveSurfaces(mode, statusText)",
            "setText('ov-drive-current',label);",
            "setStatusTriplet('drive',label,label,statusText",
            "saved=await postForm('/drive_profile',{profile:mode});",
            "var savedMode=saved&&saved.value!==undefined?driveModeFromProfile(saved.value,saved.profile):mode;",
            "updateDriveSurfaces(savedMode,'已保存');",
            "setTimeout(poll,300);",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)
        for token in [
            '"driveProfile": self.drive_profile',
            '"driveProfileName": self.drive_profile_config()["profile"]',
            "STATE.sp_auto = STATE.drive_profile == 0",
        ]:
            with self.subTest(simulator_token=token):
                self.assertIn(token, self.simulator)

    def test_phase5a_temperature_uses_error_style_above_60c(self) -> None:
        """Temperature >60°C should be red/error, not warning/yellow."""
        self.assertIn("t>60?'v-err'", self.ui)
        self.assertNotIn("t>60?'v-warn'", self.ui)

    def test_batch_c_release_ota_ui_is_wired(self) -> None:
        """OTA page must expose the GitHub release update flow already provided by backend APIs."""
        for token in [
            "Release 在线更新",
            "id=\"rel-check-btn\"",
            "id=\"rel-install-btn\"",
            "async function loadOtaReleaseState()",
            "async function checkReleaseUpdate()",
            "async function installReleaseUpdate()",
            "async function toggleUpdateBeta()",
            "async function toggleAutoUpdate()",
            "fetch('/update_check')",
            "postForm('/update_install'",
            "postForm('/update_beta'",
            "postForm('/auto_update'",
            "pageId==='pg-ota'",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_batch_c_release_ota_backend_matches_lilygo_asset(self) -> None:
        """LILYGO builds must check the fork release and look for the LILYGO-specific artifact."""
        self.assertIn('#define DASH_GITHUB_REPO "JordanzhaoD/LILYGO-T-2Can"', self.dash)
        self.assertIn("static const char *GITHUB_REPO = DASH_GITHUB_REPO;", self.dash)
        self.assertIn("#if defined(DRIVER_T2CAN_DUAL)", self.dash)
        self.assertIn('return "firmware-lilygo-t2can-dual.bin";', self.dash)
        self.assertIn('server.on("/update_beta", HTTP_GET, handleUpdateBeta);', self.dash)

    def test_batch_c_sidebar_i18n_and_version_display_are_stable(self) -> None:
        """New UI pages must not break nav translations, and overview version should use /system_status firmware.

        The CAN2 / auto-shift zh strings are split via concatenation in source so the
        raw forbidden literals never appear in standalone-visible source (Task 10);
        the assertions check the split fragments instead.
        """
        for token in [
            "'灯光特技':'Light Show'",
            "['自动'+'换挡']:'Auto'+' Shift'",
            "'驾驶状态':'Drive Status'",
            "'硬件模式':'Hardware Mode'",
            "'驾驶风格':'Drive Style'",
            "'速度策略':'Speed Strategy'",
            "'CAN 诊断':'CAN Diagnostics'",
            "'FSD 防护':'FSD Guard'",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)
        self.assertIn("'驾驶状态','硬件模式','驾驶风格','速度策略','CAN2 '+'控制','灯光特技','FSD 防护','OTA 升级','网络设置','CAN 诊断','自动'+'换挡'", self.ui)
        self.assertIn("d&&(d.firmware||d.version)", self.ui)
        self.assertIn("setText('s-ver',d.firmware||d.version);", self.ui)

    def test_batch_c_profile_helper_supports_lilygo_dual_driver(self) -> None:
        """CI profile generation must understand the LILYGO dual-CAN driver."""
        helper = (ROOT / "scripts" / "platformio_set_profile.py").read_text(encoding="utf-8")
        profile_example = (ROOT / "platformio_profile.example.h").read_text(encoding="utf-8")
        self.assertIn('"DRIVER_T2CAN_DUAL"', helper)
        self.assertIn("#define DRIVER_T2CAN_DUAL", profile_example)
        self.assertIn("#define HW4", profile_example)
        self.assertIn("--driver DRIVER_T2CAN_DUAL", self.tests_workflow)
        self.assertIn("--driver DRIVER_T2CAN_DUAL", self.release_workflow)

    def test_capability_matrix_and_bus2_can2_are_exposed(self) -> None:
        combined = self.dash
        # ── Source-level tokens: include, function name, capability helpers ──
        for token in [
            '#include "dash_capabilities.h"',
            'appendCapabilitiesJson',
            '\\"cap\\"',
            'dashCapabilityLegacyFsd',
            'dashCapabilityBionicSteering',
            'dashCapabilityBanShield',
            'dashCapabilityLegacyMppCustom',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, combined)
        # ── Composed JSON tokens from appendCapabilitiesJson string literals ──
        # These verify the C++ source contains the exact JSON key-value fragments
        # that will appear in the HTTP response (with C++ escaped quotes as read by Python).
        for token in [
            ',\\"legacyFsd\\":\\"',       # j += ",\"legacyFsd\":\""
            ',\\"serviceMode\\":\\"can_b_direct\\"',  # inline composed
            ',\\"bionicSteering\\":\\"',   # j += ",\"bionicSteering\":\""
            ',\\"legacyMppCustom\\":\\"',  # j += ",\"legacyMppCustom\":\""
            ',\\"gearAssist\\":\\"not_implemented\\"}',  # inline composed, closes cap object
        ]:
            with self.subTest(token=token):
                self.assertIn(token, combined)
        # bus2_ids endpoint must include all four can2 diagnostic counters
        bus2 = re.search(r"static void handleBus2Ids\(\).*?server\.send", combined, re.S)
        self.assertIsNotNone(bus2)
        bus2_body = bus2.group(0)
        for token in ['t2canBus2RxCount()', 't2canBus2TxCount()', 't2canBus2TxErrCount()', 't2canBus2Eflg()']:
            with self.subTest(token=token):
                self.assertIn(token, bus2_body)

    def test_ui_explains_legacy_real_wiring_states(self) -> None:
        # Runtime / backend-backed Legacy real-wiring copy stays in the UI.
        for token in [
            "Legacy 运行时状态",
            "FSD 注入策略",
            "稳定模式",
            "实验模式",
            "/legacy_fsd_config",
            "fsdDiag",
            "serviceDiag",
            "wheelDndDiag",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

        # Task 2 (UI V1.1) removed the three low-value static display cards
        # from the overview bottom: 维修模式 0x339, 方向盘免打扰 0x3C2 and the
        # Legacy 功能支持矩阵 (which held the 仿生转向/不支持/未接线/未实现 copy).
        # These had no id/JS/backend wiring, so their absence is intentional.
        for token in [
            "维修模式 0x339",
            "方向盘免打扰 0x3C2",
            "Legacy 功能支持矩阵",
            "仿生转向",
            "不支持",
            "未接线",
            "未实现",
        ]:
            with self.subTest(token=token):
                self.assertNotIn(token, self.ui)

    def test_atlas_redesign_refined_chinese_copy_is_stable(self) -> None:
        """Driving-first redesign should keep the refined Task 4 Chinese copy labels."""
        required_copy = [
            "驾驶状态",
            "硬件模式",
            "驾驶风格",
            "速度策略",
            "CAN2 控制",
            "FSD 防护",
            "OTA 升级",
            "CAN 诊断",
            "FSD 硬件模式",
            "驾驶状态中心",
            "FSD 注入",
            "长按 2 秒启用 FSD 注入",
            "长按 2 秒关闭 FSD 注入",
            "现场遥控",
            "连接到设备热点",
            "长按启用",
            "长按关闭",
            "工程模式",
            "错误优先",
            "帧筛选",
            "实时帧",
            "记录器",
            "控制器",
            "调试",
            "最近写入",
        ]
        for token in required_copy:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

        forbidden_default_copy = [
            'data-page="pg-overview"><span class="nav-icon">▣</span>模块配置',
            'data-page="pg-hardware"><span class="nav-icon">◇</span>激活模式',
            'data-page="pg-drive"><span class="nav-icon">◉</span>驾驶模式',
            'data-page="pg-speed"><span class="nav-icon">↗</span>速度偏移',
            'data-page="pg-bus2"><span class="nav-icon">✦</span>CAN2控制',
            'data-page="pg-defense"><span class="nav-icon">◈</span>FSD防御',
            'data-page="pg-ota"><span class="nav-icon">⇧</span>OTA升级',
            'data-page="pg-can"><span class="nav-icon">⌘</span>CAN工具',
            '<div class="page-title">破解模式</div>',
            '<div class="card-title">FSD 智能破解模式</div>',
            "Cockpit Home",
            "Remote Card",
            "Engineering Mode",
            "Drive Console",
            "Hold to Enable",
            "Hold to Disable",
            "PRIMARY",
            "防封",
        ]
        for token in forbidden_default_copy:
            with self.subTest(token=token):
                self.assertNotIn(token, self.ui)

    def test_atlas_redesign_cockpit_home_structure_is_present(self) -> None:
        """Overview page must become the driving-first cockpit home while preserving existing IDs."""
        overview_page = re.search(r'id="pg-overview".*?id="pg-hardware"', self.ui, re.S)
        self.assertIsNotNone(overview_page)
        body = overview_page.group(0)
        for token in [
            'class="cockpit-home"',
            'class="cockpit-primary"',
            'id="fsd-toggle"',
            'id="fsd-label"',
            'id="ov-master-tgl"',
            'id="ov-fsd-tgl"',
            'id="fsd-boot-tgl"',
            'id="s-can"',
            'id="ov-ap"',
            'id="s-fps"',
            'id="s-temp"',
            'id="s-ver"',
            'id="ov-hw"',
            'id="m-hw"',
            'id="m-drive"',
            'id="m-speed"',
            'id="m-defense"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_atlas_redesign_mobile_remote_structure_is_present(self) -> None:
        """Mobile home must be a remote-control surface, not a compressed desktop stack."""
        for token in [
            'class="mobile-remote"',
            'class="mobile-primary-card"',
            'id="m-fsd-tgl"',
            'id="m-can"',
            'id="m-can2"',
            'id="m-fsd"',
            'id="m-fps"',
            'id="m-alert"',
            '现场遥控',
            '底部导航',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_atlas_redesign_can_diagnostics_structure_is_present(self) -> None:
        """CAN tools must expose engineering-mode hierarchy and preserve existing tabs/IDs."""
        can_page = re.search(r'id="pg-can".*?id="pg-shift"', self.ui, re.S)
        self.assertIsNotNone(can_page)
        body = can_page.group(0)
        for token in [
            'class="diag-shell"',
            'class="diag-kpi-rail"',
            'id="sniff-filter"',
            'id="sniff-pause"',
            'id="sniff-rows"',
            'id="can-recorder"',
            'id="can-controller"',
            'id="can-debug"',
            'id="ctrl-eflg"',
            'id="ctrl-rxerr"',
            'id="ctrl-txerr"',
            'id="lw-injected"',
            'id="lw-match"',
            'diag-rxtx',
            'diag-eflg',
            'diag-txerr',
            'diag-last-write',
            '错误优先',
            '帧筛选',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_atlas_polish_cockpit_visual_depth_is_present(self) -> None:
        """Cockpit home should feel like a car display without losing production ids."""
        overview_page = re.search(r'id="pg-overview".*?id="pg-hardware"', self.ui, re.S)
        self.assertIsNotNone(overview_page)
        body = overview_page.group(0)
        for token in [
            'class="cockpit-shell"',
            'class="cockpit-ambient"',
            'class="cockpit-vehicle-visual"',
            'class="vehicle-lane"',
            'class="vehicle-silhouette"',
            'class="cockpit-safety-strip"',
            'id="ov-drive-current"',
            'id="tb-fsd-local"',
            '驾驶状态中心',
            '车辆态势',
            '安全优先',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_atlas_polish_mobile_remote_app_surface_is_present(self) -> None:
        """Mobile home should use a native remote-card visual hierarchy."""
        for token in [
            'class="mobile-app-shell"',
            'class="mobile-primary-card"',
            'class="mobile-remote-grid"',
            'class="mobile-risk-note"',
            '现场遥控',
            '长按启用',
            '危险操作已收纳',
            '底部导航',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_atlas_polish_can_engineering_console_is_present(self) -> None:
        """CAN diagnostics should expose a deliberate engineering-console hierarchy."""
        can_page = re.search(r'id="pg-can".*?id="pg-shift"', self.ui, re.S)
        self.assertIsNotNone(can_page)
        body = can_page.group(0)
        for token in [
            'class="diag-shell diag-console"',
            'class="diag-timeline"',
            'class="diag-kpi-rail"',
            'class="diag-table-shell"',
            'class="diag-priority-row"',
            '工程模式',
            '错误优先',
            '发现 ID',
            'LIVE TIMELINE',
            'DLC',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_webui_simulator_serves_gear_assist_status(self) -> None:
        """Local browser verification should not produce a 404 for gear assist status."""
        self.assertIn('"/gear_assist_status"', self.simulator)
        self.assertIn('gear_assist_status', self.simulator)

    def test_release_metadata_and_lilygo_ci_are_wired(self) -> None:
        """Release metadata and workflows must cover the LILYGO T-2CAN artifact."""
        self.assertEqual("4.5.0-beta14", self.version.strip())
        self.assertIn('file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/VERSION" PROJECT_VER LIMIT_COUNT 1)', self.cmake)
        self.assertIn("## [4.0.2] - 2026-05-31", self.changelog)
        self.assertIn("lilygo_t2can_dual", self.tests_workflow)
        self.assertIn("lilygo_t2can_dual", self.release_workflow)
        self.assertIn("firmware-lilygo-t2can-dual", self.release_workflow)
        self.assertIn("release-assets/firmware-lilygo-t2can-dual.bin", self.release_workflow)

    def test_ap_re_request_switch_is_wired_fail_closed(self) -> None:
        """The 4.5.0 re-request toggle must persist, apply, and diagnose end to end."""
        # NVS persistence: short key, default off, saved and loaded.
        self.assertIn('prefs.putBool("apr_on", dashApReRequestEnabled)', self.dash)
        self.assertIn('prefs.getBool("apr_on", false)', self.dash)
        # /defense_config GET flat key + POST parsing + runtime reconfigure.
        self.assertIn('j += ",\\"ap_re_request\\":"', self.dash)
        self.assertIn('server.hasArg("ap_re_request")', self.dash)
        self.assertIn(
            "dashApReRequestCtrl.configure(dashApReRequestEnabled,", self.dash
        )
        # The compiled profile is the activation authority and now carries
        # the 2026-09-10 real-vehicle 0x045 calibration. Pin the calibrated
        # constants: an accidental revert to the empty profile would
        # silently break the switch again, and an inconsistent edit would
        # fail apReRequestProfileError() validation.
        module_src = (ROOT / "include" / "dash_ap_rerequest_activation.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("2026-09-10", module_src)
        self.assertIn("p.sourceBus = CAN_BUS_VEH;", module_src)
        self.assertIn("p.stalkByte = 0;", module_src)
        self.assertIn("p.stalkMask = 0x03;", module_src)
        self.assertIn("p.stalkFwdValue = 1;", module_src)
        self.assertIn("p.stalkRwdValue = 2;", module_src)
        # 4.5.0-beta06: window widened 1200 -> 2400 ms after the beta05 Tier 1
        # CSVs (f2 round A: the only in-window mux0 carrier landed inside the
        # press->release gap and the next carrier was 2000 ms after
        # t_request — 1200 ms still expired, ~12% of rounds).
        self.assertIn("p.qualificationWindowMs = 2400;", module_src)
        self.assertIn("p.postQualApMask = 0x007C;", module_src)
        self.assertIn("p.cancelEvidenceApMask = 0x0006;", module_src)
        self.assertIn("p.cancelFaultApMask = 0x0300;", module_src)
        # 4.5.0-beta04 Tier 1 CSV fixes: the request press waits for the
        # available set (state 2; ~940 ms observed recovery after the cancel)
        # instead of firing 1 ms after the cancel release.
        self.assertIn("p.requestApMask = 0x0004;", module_src)
        self.assertIn("p.availableWaitTimeoutMs = 3000;", module_src)
        self.assertIn('"waitAvailable"', module_src)
        self.assertIn('"availableWaitTimeout"', module_src)
        self.assertIn('"apActiveDuringWait"', module_src)
        # Rolling-counter anchor is a TIME-recency rule (4.5.0-beta05):
        # anchor own last TX only when it is newer than the last native
        # template refresh (the beta03 1 ms-collision shape); otherwise
        # always template+1 (the beta04 CSV proved a ring comparison lets
        # a stale lastTx from the previous round hijack the anchor 83 s
        # later -> press skips the counter the car waits for ->
        # counterConflict lock). The ring must not come back.
        self.assertIn(
            "static_cast<uint32_t>(state_.lastTxMs - state_.templateMs) <",
            module_src,
        )
        self.assertIn("state_.lastTxMs = now;", module_src)
        self.assertIn("state_.lastTxValid = true;", module_src)
        self.assertNotIn("ringAhead", module_src)
        # 4.5.0-beta03: the AP injection gate is the coordinator's arm-level
        # precondition. Pin the wiring end to end.
        self.assertIn("bool apGateOpen = true", module_src)  # default keeps tests unchanged
        active_fn = re.search(r"bool active\(\) const\s*\{.*?^    \}", module_src, re.S | re.M)
        self.assertIsNotNone(active_fn)
        self.assertIn("state_.apGateOpen", active_fn.group(0))
        # The single production call site passes the live gate explicitly
        # (never relies on the default-true).
        self.assertIn(
            "dashApReRequestCtrl.configure(dashApReRequestEnabled,\n"
            "                                      compiledApReRequestProfile(),\n"
            "                                      apInjectionGate);",
            self.dash,
        )
        # Gate-closed mode reason + diagnostics JSON keys.
        self.assertIn('"apGateOff"', module_src)
        self.assertIn('R"JSON(,"unavailableReason":"apGateOff")JSON"', self.dash)
        self.assertIn('R"JSON(,"apGateOpen":)JSON"', self.dash)
        self.assertIn("(rr.enabled && rr.profileReady && rr.apGateOpen)", self.dash)
        # Diagnostics are shared by /status and /defense_config.
        self.assertIn('"apReRequest":{"requested"', self.dash)
        self.assertIn("unavailableReason", self.dash)
        # UI: toggle + diagnostics in both the source and generated headers.
        # rrPhaseMap localizes every coordinator reason for field triage
        # (Tier 1), including the three new beta04 reasons.
        for surface in (self.ui, self.ui_gen):
            self.assertIn("ap_re_request", surface)
            self.assertIn("apReRequest", surface)
            self.assertIn("def-ap-rerequest-tgl", surface)
            self.assertIn("rrPhaseMap", surface)
            self.assertIn("等待车辆可用", surface)
            self.assertIn("等待可用超时", surface)
            self.assertIn("AP 自恢复", surface)
        # Real send path: the coordinator tick must run on the CAN task.
        self.assertIn("t2canApReRequestTick();", self.main)

    def test_ap_re_request_requires_ap_gate_and_ui_order(self) -> None:
        """4.5.0-beta03: gate precondition, defense-page toggle order, and the
        sole-authority branch ordering that keeps a closed gate on the legal
        legacy direct path."""
        # (a) Defense page order: master -> AP gate -> 8.3.6 coordinator
        # (user-defined hierarchy), with the coordinator's diag grid directly
        # after its toggle and before the Legacy NAG row.
        defense_page = re.search(r'id="pg-defense".*?id="pg-ota"', self.ui, re.S)
        self.assertIsNotNone(defense_page)
        body = defense_page.group(0)
        master = body.index('id="def-master-tgl"')
        gate = body.index('id="ap-core-gate-tgl"')
        rerequest = body.index('id="def-ap-rerequest-tgl"')
        self.assertLess(master, gate, "defense master toggle must sit above the AP gate")
        self.assertLess(gate, rerequest, "AP gate must sit above the 8.3.6 coordinator")
        self.assertLess(body.index('id="ap-rr-effective"'), body.index('id="def-bionic-tgl"'))
        # (b) The precondition copy + localized gate-off reason must exist in
        # both the source and generated UI.
        for surface in (self.ui, self.ui_gen):
            self.assertIn("需先开启上方「AP 注入门控」", surface)
            self.assertIn("AP 门控未开启", surface)
        # (c) Sole-authority ordering: the coordinator's active() check must
        # come BEFORE the gate-off early-true, so a closed gate falls through
        # to the legacy direct path (legal normal mode) only when the
        # coordinator is inert.
        fn = re.search(
            r"static bool dashLegacyFsdActivationAllowed\(uint32_t nowMs\)\s*\{.*?^}",
            self.dash,
            re.S | re.M,
        )
        self.assertIsNotNone(fn)
        fn_body = fn.group(0)
        self.assertLess(
            fn_body.index("dashApReRequestCtrl.active()"),
            fn_body.index("if (!apInjectionGate)"),
        )

    def test_ap_re_request_beta06_auto_rearm_configure_preserve_and_diag(self) -> None:
        """4.5.0-beta06: vehicle-side locks auto re-arm (capped), configure()
        preserves an ActiveInjection round, and the end-reason survives."""
        module_src = (ROOT / "include" / "dash_ap_rerequest_activation.h").read_text(
            encoding="utf-8"
        )
        # Vehicle-side re-arm set: the beta06 founding four (hardware-class
        # reasons must keep locking — the fail-closed answer). beta14 adds
        # apExitedDuringRequest + cancelIgnored (pinned in the beta14 test);
        # the full set is now six members.
        self.assertIn("static bool vehicleSideRearmReason(const char *r)", module_src)
        for reason in ("windowExpired", "apExitedDuringWindow", "apActiveDuringWait", "apFaultState"):
            self.assertIn(f'strcmp(r, "{reason}") == 0', module_src)
        # Cap: 3 consecutive vehicle-side locks without a completed round
        # between them degrade to a real FailedLocked.
        self.assertIn("kVehicleAutoRearmMax = 3", module_src)
        self.assertIn("state_.consecutiveVehicleLocks < kVehicleAutoRearmMax", module_src)
        self.assertIn('"autoRearmLimit"', module_src)
        self.assertIn("state_.consecutiveVehicleLocks = 0;", module_src)  # completeRound resets
        # A re-arm is the Complete fence: WaitDriverIntent with exitSeen
        # cleared, never a round while the car is still in a weird state.
        self.assertIn("state_.exitSeen = false;", module_src)
        # configure() preservation: ONLY an identical (enabled, gate,
        # byte-identical valid profile) re-apply during ActiveInjection keeps
        # the round; every real mode change takes the full reset. This is the
        # fix for mid-injection /config POSTs killing the bit46 chain.
        self.assertIn(
            "state_.phase == Phase::ActiveInjection &&\n"
            "            state_.enabled && state_.profileReady && state_.apGateOpen &&",
            module_src,
        )
        self.assertIn("memcmp(&state_.profile, &profile, sizeof(profile)) == 0", module_src)
        # Barrier B: each round starts with a FRESH cancel-evidence latch (a
        # stale latch + a pre-evidence active state used to fire a false
        # apActiveDuringWait lock in round 2+).
        self.assertIn("state_.cancelEvidenceSeen = false;", module_src)
        # lastEndedReason survives re-arms and state changes; cleared only by
        # configure(). This is the field answer to "record the reason before
        # touching any switch" from the beta05 Tier 1 lesson.
        self.assertIn("state_.lastEndedReason = reason;", module_src)
        self.assertIn('const char *lastEndedReason = "none";', module_src)
        # Dashboard JSON + simulator mirror.
        self.assertIn(',"lastEndedReason":"', self.dash)
        self.assertIn(',"autoRearms":', self.dash)
        self.assertIn('"lastEndedReason": "none"', self.simulator)
        self.assertIn('"autoRearms": 0', self.simulator)
        self.assertIn('"windowMs": 2400', self.simulator)
        # UI: two new diag cells + localized new reasons (source + generated;
        # bare id tokens because the minifier strips attribute quotes).
        self.assertIn('id="ap-rr-lastend"', self.ui)
        self.assertIn('id="ap-rr-rearm"', self.ui)
        for surface in (self.ui, self.ui_gen):
            self.assertIn("ap-rr-lastend", surface)
            self.assertIn("ap-rr-rearm", surface)
            self.assertIn("上次结束原因", surface)
            self.assertIn("自动重武装", surface)
            self.assertIn("已自动重武装", surface)
            self.assertIn("autoRearmLimit", surface)
            self.assertIn("2.4s", surface)

    def test_status_json_auto_rearms_emission_is_valid(self) -> None:
        """4.5.0-beta07: beta06 shipped `","round":` after the NUMERIC
        autoRearms append — one stray quote made /status and /defense_config
        unparseable device-wide (UI: every switch rendered OFF after
        re-entry, top badge stuck on 未连接; the saves themselves were fine).
        Boundary pins both ways: string-close form only after the string
        lastEndedReason, bare form after the numeric autoRearms."""
        # Correct chain: string value -> `","autoRearms":` (closes the string),
        # numeric value -> bare `,"round":` (no leading quote).
        self.assertIn('j += R"JSON(","autoRearms":)JSON";', self.dash)
        self.assertIn("j += rr.autoRearms;", self.dash)
        self.assertIn('j += R"JSON(,"round":)JSON";', self.dash)
        # The beta06 stray-quote form must stay dead. (Substring asserts like
        # `,"autoRearms":` pass either way — `","autoRearms":` contains them —
        # which is exactly how this shipped green in beta06.)
        self.assertNotIn('R"JSON(","round":)JSON"', self.dash)

    def test_ap_settle_delay_chain_removed_in_beta06(self) -> None:
        """4.5.0-beta06: the AP-settle delay (0/1/2/3 s "延迟注入") is gone
        end to end — UI select, POST param, NVS key, settle timer, gate-mode
        variant, and the settling injection state."""
        for surface in (self.ui, self.ui_gen):
            self.assertNotIn("ap-delay-select", surface)
            self.assertNotIn("apDelayMs", surface)
            self.assertNotIn("settling", surface)
            self.assertNotIn("稳定计时中", surface)
            self.assertNotIn("延迟注入", surface)
        # The dashboard and simulator keep one-line tombstone comments that
        # NAME the removed symbols ("dashClampApDelayMs removed in ..."), so a
        # bare token search would trip over the tombstones, not live code.
        # Assert against comment-stripped code surfaces instead.
        dash_code = "\n".join(
            line.split("//", 1)[0] for line in self.dash.splitlines()
        )
        for token in (
            "dashClampApDelayMs",
            "legacyFsdRequiredStableMs",
            "legacyFsdApSettleTiming",
            "legacyFsdApActiveSinceMs",
            "kLegacyFsdActivationSettle",
        ):
            with self.subTest(dash_token=token):
                self.assertNotIn(token, dash_code)
        sim_code = "\n".join(
            line.split("#", 1)[0] for line in self.simulator.splitlines()
        )
        self.assertNotIn("apDelayMs", sim_code)
        self.assertNotIn("ap_delay_ms", sim_code)
        # saveApGateControls no longer posts the delay param.
        self.assertNotIn("ap_delay_ms:", self.ui)

    def test_dual_can_legacy_activation_follows_native_source_driver(self) -> None:
        """Legacy 0x3EE must be handled on whichever controller received it."""
        self.assertIn("the handler modifies and", self.main)
        self.assertIn("appActiveHandler->handleMessage(f, *appDriverSecondary)", self.main)

    def test_mobile_fixed_chrome_hoisted_to_body(self) -> None:
        """4.5.0-beta08: mobile fixed chrome must be hoisted out of .content.

        Root cause (same as single-CAN v1.0.7): the bottom tab bars and
        "more" panels are position:fixed but emitted inside .content
        (overflow-y:auto scroll container). iOS Safari fails to repaint a
        fixed element nested in a scroll container during momentum scroll on
        long pages, so the tab bar renders blank/disappears after scrolling.
        hoistMobileChrome() re-parents them to <body> at DOMContentLoaded.
        """
        for surface, quote in [(self.ui, '"'), (self.ui_gen, "")]:
            with self.subTest(surface="src" if quote else "generated"):
                self.assertIn("function hoistMobileChrome(", surface)
                self.assertRegex(
                    surface,
                    r"addEventListener\('DOMContentLoaded',function\(\)\{\s*hoistMobileChrome\(\);",
                )
                for element_id in ("mob-tabs", "mob-more-single", "mob-tabs-dual", "mob-more"):
                    self.assertIn(f"'{element_id}'", surface)

        # #mob-tabs must carry data-dual-hide itself: the hoist removes it
        # from the old <div data-dual-hide="1"> wrapper, so product-mode
        # gating (applyProductMode) has to keep working on the element alone.
        self.assertIn('<div class="mob-tabs" id="mob-tabs" data-dual-hide="1">', self.ui)
        self.assertIn('<div class=mob-tabs id=mob-tabs data-dual-hide=1>', self.ui_gen)

        # Anti-regression: the old wrapper around the standalone chrome must
        # stay deleted (it would re-nest the fixed bar inside .content after
        # the hoist moved only its children).
        self.assertNotIn('<div data-dual-hide="1">\n<!-- Mobile Bottom Tab Bar', self.ui)

        # The hoist must not change the positioning strategy itself.
        self.assertRegex(self.ui, r"\.mob-tabs\s*\{[^}]*position:\s*fixed")
        self.assertRegex(self.ui, r"\.mob-tabs\s*\{[^}]*bottom:\s*0")

        # Inline onclick handlers survive the re-parent (they stay bound to
        # the moved node), and the dual tab bar keeps its data-page wiring.
        self.assertIn("onclick=\"showMobilePage('driving')\"", self.ui)
        self.assertIn('<div class="mob-tab active" data-page="pg-overview">', self.ui)

    def test_beta09_vehicle_refusal_watch_and_flag_tally(self) -> None:
        """4.5.0-beta09 P1/P2, both purely observational (no gate, timeout, or
        injection semantics touched — 8.3.6 anti-jerk behavior is unchanged).

        P1 roots in the 162535 Tier 1 CSV: one native RWD press the car
        silently ignored (flag1 up in 6 ms, no state-3..6 edge for 6 s). The
        coordinator correctly stayed idle, but the driver had no way to tell
        a declined car from a broken box. P1 watches WaitDriverIntent RWD
        press edges and latches "vehicle refused" when no activation edge
        follows within kVehicleRefusalMs.
        P2 tallies the unknown-semantics high nibble of 0x399 byte0 into a
        16-bin histogram so future CSVs can decode it.
        """
        module_src = (ROOT / "include" / "dash_ap_rerequest_activation.h").read_text(
            encoding="utf-8"
        )
        # P1 window constant + the tick ORDER pin: the refusal check must run
        # BEFORE the roundActive early return, or WaitDriverIntent (not
        # roundActive) would never reach it and the watch would never fire.
        self.assertIn("static constexpr uint32_t kVehicleRefusalMs = 2500;", module_src)
        tick_fn = module_src.split("void tick(uint32_t now)", 1)[1][:1400]
        refusal_idx = tick_fn.find("state_.rwdPressPending")
        early_return_idx = tick_fn.find("if (!roundActive(state_.phase)) return;")
        self.assertGreater(refusal_idx, -1, "tick() must evaluate the refusal watch")
        self.assertLess(
            refusal_idx, early_return_idx,
            "refusal watch must precede the roundActive early return",
        )
        # Press-edge detection only in WaitDriverIntent, only for RWD, only on
        # a value change (held 10Hz rollover frames are one press).
        self.assertIn(
            "state_.phase == Phase::WaitDriverIntent &&\n"
            "                stalk == state_.profile.stalkRwdValue &&\n"
            "                state_.prevNativeStalk != stalk",
            module_src,
        )
        # The activation edge clears both pending and refusal, BEFORE
        # startRound — an accepted press must never read as refused even when
        # device-side gating (exitSeen/intent/permit) stops the round.
        edge_block = module_src.split("if (active && !state_.prevApActive)", 1)[1][:600]
        self.assertIn("state_.rwdPressPending = false;", edge_block)
        self.assertIn("state_.vehicleRefusal = false;", edge_block)
        self.assertLess(
            edge_block.find("state_.rwdPressPending = false;"),
            edge_block.find("startRound(now)"),
        )
        # P2: default param keeps 30+ legacy 3-arg test calls compiling; the
        # tally runs after the bus filter for every phase (Disabled included).
        self.assertIn(
            "void observeDasStatus(uint8_t apState, uint8_t bus, uint32_t now,\n"
            "                          uint8_t dasFlags = 0)",
            module_src,
        )
        obs = module_src.split("++state_.otherBusObs;\n            return;\n        }", 1)[
            1
        ][:700]
        self.assertIn("++state_.apFlagCounts[dasFlags & 0x0F];", obs)
        # diag() exports the new fields.
        for token in (
            "d.vehicleRefusal = state_.vehicleRefusal;",
            "d.rwdPressMs = state_.rwdPressMs;",
            "d.lastApFlag = state_.lastApFlag;",
            "memcpy(d.apFlagCounts, state_.apFlagCounts, sizeof(d.apFlagCounts));",
        ):
            self.assertIn(token, module_src)

        # Dashboard call site forwards the nibble the old code masked away.
        self.assertIn(
            "const uint8_t dasFlags = static_cast<uint8_t>(f.data[0] >> 4);", self.dash
        )
        self.assertIn(
            "static_cast<uint32_t>(now), dasFlags);", self.dash
        )

        # JSON chain: every new field is bool/numeric (or an array), so all
        # literals must be the BARE `,"key":` form — a `","key":` after any of
        # them is the beta06 stray-quote class of bug. Pin both ways.
        for token in (
            ',"vehicleRefusal":', ',"rwdPressPending":', ',"vehicleRefusals":',
            ',"rwdPressMs":', ',"lastApFlag":', ',"lastApFlagMs":',
            ',"apFlagCounts":[',
            "for (uint8_t i = 0; i < 16; ++i)",
        ):
            self.assertIn(token, self.dash)
        for stray in (
            '","vehicleRefusal":', '","rwdPressPending":', '","vehicleRefusals":',
            '","rwdPressMs":', '","lastApFlag":', '","lastApFlagMs":',
            '","apFlagCounts":',
        ):
            self.assertNotIn(stray, self.dash)

        # UI: the new cell exists in source and generated surfaces (ids are
        # unquoted in the generated file — bare-token asserts per convention).
        self.assertIn('id="ap-rr-vehicle"', self.ui)
        for surface in (self.ui, self.ui_gen):
            self.assertIn("ap-rr-vehicle", surface)
            self.assertIn("车辆响应", surface)
            self.assertIn("未接受激活 · 车侧拒绝", surface)
            self.assertIn("等待车辆响应", surface)

        # Simulator mirrors the firmware /status shape (beta07 lesson: a
        # missing simulator field makes poll() flip real UI state).
        for token in (
            '"vehicleRefusal": False',
            '"rwdPressPending": False',
            '"vehicleRefusals": 0',
            '"rwdPressMs": 0',
            '"lastApFlag": 0',
            '"lastApFlagMs": 0',
            '"apFlagCounts": [0] * 16',
        ):
            self.assertIn(token, self.simulator)

    def test_beta10_observation_layer_permit_blackbox(self) -> None:
        """4.5.0-beta10 P1 (Jordan-approved 2026-09-13): pure observation
        AROUND the coordinator — permit-input decomposition, tick liveness,
        OTA-watch counters, and an NVS blackbox. Zero 8.3.6 semantics: the
        coordinator module is untouched and the permit expression is pinned
        verbatim below.

        Roots in the 2026-09-13 field feedback: symptoms 2/3 (post-force-exit
        / long-run injection death) survive 8.3.6 switch cycling and recover
        only on power cycle — the wedge lives OUTSIDE the coordinator, in
        RAM, and rebooting destroys the evidence. beta10 decomposes the
        five-input permit AND, counts tick heartbeats, watches the OTA guard
        (0x318 dashOtaGuardProcessFrame CAN clear-wedge: 6 consecutive
        non-OTA frames required, none if 0x318 stops flowing), and persists a
        blackbox so the first /status after an unexpected power loss shows
        what the box recorded before it died.
        """
        # 1. Zero-semantics proof: the permit expression is byte-identical to
        #    beta09 — observation was added around it, never inside it.
        self.assertIn(
            "const bool permit = canActive && dashLegacyHandlerActive() && appDriver &&\n"
            "                        !vehicleOtaActive &&\n"
            "                        (!dashHandler || dashHandler->abortGuard.allowsInjection());",
            self.main,
        )
        # And the coordinator module itself carries no beta10 edits.
        module_rr = (ROOT / "include" / "dash_ap_rerequest_activation.h").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("Blackbox", module_rr)
        self.assertNotIn("blackbox", module_rr)

        # 2. Blackbox module contract (behavior pinned by the native suite;
        #    here: capacity, flap guard, stack-order doc, NVS blob shape).
        module_bb = (ROOT / "include" / "dash_ap_blackbox.h").read_text(encoding="utf-8")
        for token in (
            "enum class DashApBbKind : uint8_t",
            "static constexpr uint8_t kCapacity = 8;",
            "static constexpr uint32_t kFlapGuardMs = 5000;",
            "entries()[0] is the NEWEST",
            "size_t storageBytes() const { return sizeof(entries_); }",
        ):
            self.assertIn(token, module_bb)

        # 3. main.cpp wiring: NVS persistence, boot restore + Boot marker,
        #    permit/intent/round/heartbeat records, bridge exports.
        for token in (
            's_apBbPrefs.begin("aprbbx", true)',
            's_apBbPrefs.putBytes("ring", buf, len)',
            "DashApBbKind::Boot",
            "DashApBbKind::PermitChange",
            "DashApBbKind::IntentChange",
            "DashApBbKind::RoundStart",
            "DashApBbKind::Heartbeat",
            "DashApBbKind::Lock",
            "static constexpr uint32_t kApBbHeartbeatMs = 300000;",
            "uint32_t t2canApReRequestTickCount(void) {",
            "uint8_t t2canApReRequestPermitBits(void) {",
            "const DashApBbEntry *t2canApBlackboxEntries(void) {",
        ):
            self.assertIn(token, self.main)

        # 4. Dashboard: otaWatch statics + per-0x318 counting in the
        #    dispatcher; bridge declarations beside the can2Recovery block;
        #    JSON sections under the DRIVER_T2CAN_DUAL guard (native tests
        #    exclude them, same pattern as the can2Recovery keys).
        for token in (
            "static uint32_t dashOta318Seen = 0;",
            "if (f.id == CAN_ID_OTA_STATUS)",
            "++dashOta318Seen;",
            "uint8_t t2canApReRequestDriverPresent(void);",
            "const DashApBbEntry *t2canApBlackboxEntries(void);",
            ',"permitInputs":{"canActive":',
            ',"legacyHandler":',
            ',"otaGuardClear":',
            ',"abortGuardOk":',
            ',"tickHeartbeat":{"count":',
            ',"blackbox":[',
            ',"otaWatch":{"seen318":',
            ',"last318Ms":',
            ',"permitBits":',
        ):
            self.assertIn(token, self.dash)
        # permitInputs must decompose exactly the five inputs of the pinned
        # expression (abortGuardOk inverted-guard form included).
        self.assertIn(
            'j += (dashHandler && !dashHandler->abortGuard.allowsInjection()) ? "false" : "true";',
            self.dash,
        )
        # The blackbox/heartbeat/driver sections sit inside the dual-driver
        # guard (native tests exclude them, same as the can2Recovery keys);
        # permitInputs/otaWatch sit OUTSIDE it (TU statics, emitted by every
        # build). Prove containment per token: the nearest
        # #ifdef DRIVER_T2CAN_DUAL before the token must be nearer than the
        # nearest #endif before it (and inverted for the unguarded two).
        for token in ('R"JSON(,"tickHeartbeat":{"count":)JSON"',
                      'R"JSON(,"blackbox":[)JSON"', 'R"JSON(,"driver":)JSON"'):
            with self.subTest(guarded_token=token):
                before = self.dash[: self.dash.index(token)]
                guard_idx = before.rfind("#ifdef DRIVER_T2CAN_DUAL")
                endif_idx = before.rfind("\n#endif")
                self.assertGreater(guard_idx, -1, f"{token} must follow the dual guard")
                self.assertGreater(
                    guard_idx, endif_idx,
                    f"{token} must sit INSIDE #ifdef DRIVER_T2CAN_DUAL",
                )
        for token in ('R"JSON(,"permitInputs":{"canActive":)JSON"',
                      'R"JSON(,"otaWatch":{"seen318":)JSON"'):
            with self.subTest(unguarded_token=token):
                before = self.dash[: self.dash.index(token)]
                guard_idx = before.rfind("#ifdef DRIVER_T2CAN_DUAL")
                endif_idx = before.rfind("\n#endif")
                self.assertGreater(guard_idx, -1)
                self.assertLess(
                    guard_idx, endif_idx,
                    f"{token} must sit OUTSIDE #ifdef DRIVER_T2CAN_DUAL",
                )
        # otaWatch reads the OTA guard's own counters (same TU statics).
        for token in ("j += otaConfirmCount;", "j += otaClearCount;"):
            self.assertIn(token, self.dash)

        # 5. Chain-format discipline (beta06 stray-quote class): every new
        #    key is bool/numeric — bare `,"key":` form only; the string-valued
        #    kind/reason keys inside blackbox entries use `","key":"`.
        for stray in (
            '","permitInputs":', '","otaGuardClear":', '","abortGuardOk":',
            '","tickHeartbeat":', '","blackbox":', '","otaWatch":',
            '","seen318":', '","last318Ms":', '","permitBits":',
        ):
            self.assertNotIn(stray, self.dash)

        # 5b. Emission moved into the pure function (beta12, LittleGong port
        #     5): per-entry JSON comes from dashApBbEntryToJson in
        #     dash_ap_blackbox.h — a native-compilable header with
        #     golden-string tests — instead of inline String appends in this
        #     dashboard header (which native tests never compile; that blind
        #     spot shipped the beta06/beta07/beta10 quote bugs). Pin the new
        #     call form, kill the old inline forms (kind/reason String
        #     appends) as anti-regression, and require the pure function +
        #     kind table to live in the blackbox module.
        self.assertIn(
            "if (dashApBbEntryToJson(entryJson, sizeof(entryJson), bb[k - 1]) == 0)\n"
            "                    continue;\n"
            "                if (!firstEntry) j += ',';\n"
            "                firstEntry = false;\n"
            "                j += entryJson;",
            self.dash,
        )
        # The dashboard no longer assembles entry JSON itself — any inline
        # kind/reason append here is the pre-beta12 form reappearing.
        self.assertNotIn("j += dashApBbKindName(e.kind);", self.dash)
        self.assertNotIn("j += e.reason;", self.dash)
        # The pure function and its kind table live in the blackbox module,
        # pinned by the native golden tests (test_entry_json_golden_*).
        for token in (
            "inline size_t dashApBbEntryToJson(char *out, size_t cap, const DashApBbEntry &e)",
            "static constexpr size_t kDashApBbEntryJsonCap = 160;",
            'case 7: return "intentChange";',
            "inline const char *dashApBbKindName(uint8_t kind)",
        ):
            self.assertIn(token, module_bb)

        # 6. UI: 许可链 + 心跳 cells on both surfaces; dead-input naming so a
        #    wedge names its cause in Chinese on the status page.
        self.assertIn('id="ap-rr-permit"', self.ui)
        self.assertIn('id="ap-rr-tick"', self.ui)
        for surface in (self.ui, self.ui_gen):
            self.assertIn("许可链", surface)
            self.assertIn("心跳", surface)
            self.assertIn("OTA守卫", surface)
            self.assertIn("tickHeartbeat", surface)
            self.assertIn("permitInputs", surface)

        # 7. Simulator mirrors the new /status shape (beta07 lesson).
        for token in (
            '"permitInputs": {',
            '"otaGuardClear": not STATE.vehicle_ota',
            '"tickHeartbeat": {',
            '"blackbox": [',
            '"otaWatch": {',
            '"active": STATE.vehicle_ota',
        ):
            self.assertIn(token, self.simulator)

        # 8. The native env for the blackbox suite exists (20 native envs now).
        pio_src = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("[env:native_ap_blackbox]", pio_src)
        self.assertIn("test_filter = test_native_ap_blackbox", pio_src)

    def test_beta10_blackbox_nvs_survives_reboot_shape(self) -> None:
        """The boot restore must run BEFORE any other record and stamp a Boot
        marker — the entries BELOW the newest boot record are exactly the
        pre-powerloss evidence the blackbox exists to keep (order pin in
        main.cpp wiring)."""
        tick_fn = self.main.split("static void t2canApReRequestTick()", 1)[1][:3000]
        boot_idx = tick_fn.find("DashApBbKind::Boot")
        permit_idx = tick_fn.find("DashApBbKind::PermitChange")
        self.assertGreater(boot_idx, -1)
        self.assertGreater(permit_idx, -1)
        self.assertLess(
            boot_idx, permit_idx,
            "boot restore + Boot marker must precede the first event record",
        )
        self.assertLess(
            tick_fn.find('s_apBbPrefs.begin("aprbbx", true)'),
            boot_idx,
            "NVS load must precede the Boot record",
        )

    def test_beta12_littlegong_port_ota_guard_carrier_tx(self) -> None:
        """4.5.0-beta12 (Jordan-approved 2026-09-13): the four LittleGong
        reverse-engineering ports in ONE release.

        Port 1 — OTA guard three-path rework (top suspect for beta08
        symptoms 2/3): the old clear condition needed 6 consecutive non-OTA
        0x318 frames, so a 0x318 stream stall wedged the flag true forever
        (permit silently dead, only unplug recovered). Added: stream-stall
        timeout downgrade (10 min, staleClears counter) + user override
        switch (NVS otg_ovr, defense_config ota_guard_override, UI toggle).
        The permit expression itself stays VERBATIM (pinned in
        test_beta10_observation_layer_permit_blackbox) — every change lives
        in the guard's own set/clear conditions.

        Port 2 — 0x3EE mux0 carrier cadence (symptom 1: "only EAP" =
        carrier missing inside the qualification window): JSON-only
        observation block carrier3ee, no UI cell (approved scope).

        Port 3 — injection TX accounting (txAccounting block) off
        legacyFsdDiag.mux0, fed by recordMuxTx on the activation-allowed
        path only.

        Port 5 — blackbox pure-function JSON emission (covered by the 5b
        rewrite above + native golden tests).
        """
        module_guard = (ROOT / "include" / "dash_ota_guard.h").read_text(encoding="utf-8")
        module_cad = (ROOT / "include" / "dash_3ee_cadence.h").read_text(encoding="utf-8")
        module_diag = (ROOT / "include" / "dash_fsd_diag.h").read_text(encoding="utf-8")
        pio_src = (ROOT / "platformio.ini").read_text(encoding="utf-8")

        # 1. Guard module: three clear paths + thresholds pinned.
        for token in (
            "#define OTA_CONFIRM_THRESHOLD 3",
            "#define OTA_CLEAR_THRESHOLD   6",
            "#define OTA_STALE_TIMEOUT_MS 600000UL",
            "inline void dashOtaGuardProcessFrame(const CanFrame& frame, uint32_t nowMs)",
            "inline bool dashOtaGuardTick(uint32_t nowMs)",
            "inline void dashOtaGuardSetOverride(bool v)",
            "inline bool dashOtaGuardAllowInjection() { return !vehicleOtaActive; }",
            # Any-dlc carrier stamping happens BEFORE the dlc gate: the
            # stale-timeout input must not depend on the OTA flag decoding.
            "otaLastSeenMs = nowMs;",
        ):
            self.assertIn(token, module_guard)
        # The stale timeout only touches the CLEAR side of the guard.
        self.assertNotIn("dashOtaGuardTick", module_guard.replace("inline bool dashOtaGuardTick(uint32_t nowMs)", ""))

        # 2. main.cpp tick wiring: outside DRIVER_T2CAN_DUAL so BOTH builds
        #    degrade the wedge, and the log line names the counter.
        for token in (
            "if (dashOtaGuardTick((uint32_t)millis()))",
            "[OTA] guard stale-clear after stream stall (staleClears=",
        ):
            self.assertIn(token, self.main)
        can_task = self.main.split("static void app_can_task(void *)", 1)[1][:2500]
        tick_idx = can_task.find("if (dashOtaGuardTick((uint32_t)millis()))")
        dual_idx = can_task.find("#ifdef DRIVER_T2CAN_DUAL")
        self.assertGreater(tick_idx, -1)
        self.assertGreater(dual_idx, -1)
        self.assertLess(
            tick_idx, dual_idx,
            "guard tick must run in BOTH builds (before the dual #ifdef)",
        )

        # 3. Dashboard: dispatcher feeds the guard + cadence observer with
        #    the dispatcher's own now (millis() snapshot).
        for token in (
            "dashOtaGuardProcessFrame(f, static_cast<uint32_t>(now));",
            "dash3eeCadence.observe(f, static_cast<uint32_t>(now));",
            "static Dash3eeCadence dash3eeCadence;",
            '#include "dash_3ee_cadence.h"',
        ):
            self.assertIn(token, self.dash)

        # 4. otaWatch gains the degradation bookkeeping — bare-key form
        #    (numeric/bool), stray-quote negatives per the beta06 class.
        for token in (
            'R"JSON(,"staleClears":)JSON"',
            'R"JSON(,"override":)JSON"',
            'R"JSON(,"blockedMs":)JSON"',
            "dashOtaGuardStaleClears()",
            "dashOtaGuardOverrideEnabled() ? \"true\" : \"false\"",
        ):
            self.assertIn(token, self.dash)
        for stray in (
            '","staleClears":', '","override":', '","blockedMs":',
            '","carrier3ee":', '","txAccounting":',
            '","lastGapMs":', '","attempts":', '","consecutiveFails":',
        ):
            self.assertNotIn(stray, self.dash)

        # 5. Override switch full chain: NVS load/save (short key otg_ovr),
        #    defense_config POST + JSON, /vehicle_ota_status sync.
        self.assertIn('dashOtaGuardSetOverride(prefs.getBool("otg_ovr", false));', self.dash)
        self.assertIn('prefs.putBool("otg_ovr", dashOtaGuardOverrideEnabled());', self.dash)
        self.assertIn('server.hasArg("ota_guard_override")', self.dash)
        self.assertIn('dashOtaGuardSetOverride(dashArgTruthy(server.arg("ota_guard_override")));', self.dash)
        self.assertIn('",\\"ota_guard_override\\":"', self.dash)
        ota_status = self.dash.split("handleVehicleOtaStatus", 1)[1][:2200]
        for token in ('",\\"staleClears\\":"', '",\\"override\\":"', '",\\"blockedMs\\":"'):
            self.assertIn(token, ota_status)

        # 6. carrier3ee (port 2): JSON-only observation block, filter pinned
        #    in the module (id 0x3EE + mux0 + CAN_BUS_VEH = the exact stream
        #    the qualification injection watches; longGap > 2000ms strict).
        for token in (
            'R"JSON(,"carrier3ee":{"lastGapMs":)JSON"',
            'R"JSON(,"maxGapMs":)JSON"',
            'R"JSON(,"longGaps":)JSON"',
            'R"JSON(,"total":)JSON"',
        ):
            self.assertIn(token, self.dash)
        # No UI cell was added for carrier3ee (approved JSON-only scope):
        # the raw token must not leak into either UI surface.
        self.assertNotIn("carrier3ee", self.ui)
        self.assertNotIn("carrier3ee", self.ui_gen)
        for token in (
            "static constexpr uint32_t kDash3eeLongGapMs = 2000;",
            "if (frame.id != 0x3EE || frame.dlc == 0) return;",
            "if ((frame.data[0] & 0x07) != 0) return; // mux0 only",
            "if (frame.bus != CAN_BUS_VEH) return;    // coordinator source bus",
            "struct Dash3eeCadence",
        ):
            self.assertIn(token, module_cad)

        # 7. txAccounting (port 3): attempts ledger on FsdMuxDiag.recordTx;
        #    ok==tx, fails==err at emission (recordMuxTx feeds mux0 only on
        #    the activation-allowed path = the injection-frame ledger).
        for token in (
            'R"JSON(,"txAccounting":{"attempts":)JSON"',
            'R"JSON(,"ok":)JSON"',
            'R"JSON(,"fails":)JSON"',
            'R"JSON(,"consecutiveFails":)JSON"',
            'R"JSON(,"lastOkAgeMs":)JSON"',
            "const FsdMuxDiag m0 = dashHandler ? dashHandler->legacyFsdDiag.mux0 : FsdMuxDiag{};",
        ):
            self.assertIn(token, self.dash)
        for token in (
            "uint32_t attempts = 0;",
            "uint32_t consecutiveFails = 0;",
            "uint32_t lastFailMs = 0;",
            "uint32_t lastOkMs = 0;",
            "        attempts++;",
            "            ++consecutiveFails;",
        ):
            self.assertIn(token, module_diag)

        # 8. UI: the override toggle exists on both surfaces (the minifier
        #    strips attribute quotes — source keeps id="…", generated uses
        #    id=…), loads from d.ota_guard_override, and posts
        #    ota_guard_override.
        self.assertIn("忽略车辆 OTA 守卫", self.ui)
        self.assertIn('id="def-ota-ovr-tgl"', self.ui)
        self.assertIn("忽略车辆 OTA 守卫", self.ui_gen)
        self.assertIn("id=def-ota-ovr-tgl", self.ui_gen)
        self.assertIn("otaOvr.checked=!!d.ota_guard_override;", self.ui)
        self.assertIn("ota_guard_override:otaOvr&&otaOvr.checked?'1':'0',", self.ui)

        # 9. Simulator mirrors the full new shape (beta07 lesson: a missing
        #    mirror field flips real UI state in poll()).
        for token in (
            "self.ota_guard_override = False",
            "self.ota_stale_clears = 0",
            '"ota_guard_override": STATE.ota_guard_override',
            '"staleClears": STATE.ota_stale_clears',
            '"override": STATE.ota_guard_override',
            '"carrier3ee": {',
            '"txAccounting": {',
        ):
            self.assertIn(token, self.simulator)
        # Override-on clears the simulated vehicle_ota flag (firmware parity:
        # dashOtaGuardSetOverride(true) clears vehicleOtaActive). Anchor on
        # the POST-only form parse — /defense_config also has a GET handler.
        sim_post = self.simulator.split('if "ota_guard_override" in form:', 1)[1][:400]
        self.assertIn("if STATE.ota_guard_override:", sim_post)
        self.assertIn("STATE.vehicle_ota = False", sim_post)

        # 10. Two new native envs (22 native envs now).
        for env in ("native_ota_guard", "native_3ee_cadence"):
            self.assertIn(f"[env:{env}]", pio_src)
            self.assertIn(f"test_filter = test_{env}", pio_src)

    def test_beta13_permitinputs_close_reconstruction_guard(self) -> None:
        """4.5.0-beta13: permitInputs close brace + full-endpoint mechanical
        reconstruction guard.

        Field defect (Jordan, 2026-09-13, beta12 OTA flash): top bar
        「未连接」, CAN offline, config all grey, RX/TX 0/0 — while beta09 on
        the same car worked. Root cause: the beta10 observation layer opened
        `,"permitInputs":{` and never closed it (the beta10 reason-quote
        hotfix in beta11 fixed a DIFFERENT bug in the same increment), so
        legacyInjectionSafety never closed, the root `}` was consumed
        mid-stack, and BOTH /status and /defense_config became unparseable
        end-to-end — poll() null → 未连接 + defaults everywhere. Fifth
        incident of the quote/brace class; substring pins saw every key yet
        could not see the missing delimiter.

        The fix: close permitInputs after the dual-only `driver` key, with
        the #ifdef split so non-dual builds (4 inputs) close too;
        permitBits/tickHeartbeat/blackbox/otaWatch stay SIBLINGS at the
        apReRequest level (simulator + UI readers shape).

        The guard: scripts/reconstruct_status_json.py walks the C++ emission
        statements in source order (inlining every builder, modeling macro
        sandwiches / ternaries / String() calls as placeholders) and
        json.loads the literal skeleton of all three dynamic endpoints.
        The dashboard header is not compiled by native tests, so this is
        the only automated full-parse gate.
        """
        import subprocess
        import sys

        # 1. The fix itself, order-pinned inside the emission region.
        driver_lit = 'j += R"JSON(,"driver":)JSON";'
        permitbits_lit = 'j += R"JSON(,"permitBits":)JSON";'
        self.assertIn(driver_lit, self.dash)
        self.assertIn(permitbits_lit, self.dash)
        seg = self.dash[self.dash.index(driver_lit) : self.dash.index(permitbits_lit)]
        self.assertIn("#endif", seg)
        self.assertIn("j += '}';", seg)

        # 2. Sibling shape on the simulator mirror (permitInputs holds the
        #    five inputs only; tickHeartbeat/blackbox/otaWatch outside it).
        sim_seg = self.simulator.split('"permitInputs": {', 1)[1].split('"otaWatch"', 1)[0]
        self.assertIn('"driver": True', sim_seg)
        self.assertIn("}", sim_seg)
        self.assertIn('"tickHeartbeat": {', sim_seg.split('"permitBits"', 1)[1])

        # 3. Full mechanical reconstruction of all three endpoints parses.
        script = ROOT / "scripts" / "reconstruct_status_json.py"
        result = subprocess.run(
            [sys.executable, str(script)],
            cwd=str(ROOT),
            capture_output=True,
            text=True,
            timeout=120,
        )
        self.assertEqual(
            result.returncode,
            0,
            f"reconstructor failed:\n{result.stdout}\n{result.stderr}",
        )
        self.assertIn("ALL EMISSIONS PARSE CLEAN", result.stdout)

    def test_beta14_field_robustness_tolerance_reclass_retries(self) -> None:
        """4.5.0-beta14: beta13 field robustness — counter +2 tolerance,
        refusal/ignored-cancel reclassification, bounded in-round retries.

        Field basis (2026-09-13, 4 CSVs, ~30 rounds, 9 failures): 4-6
        refusals of a byte-perfect request (2->1 at 190-579 ms, second press
        engaged in the 162535 CSV), 2 ignored cancels (state 3 held 4-5 s
        while the 0x45 echo/counter/10 Hz stream stayed healthy), 1
        counterConflict misfire (the native module pauses its broadcast
        213-290 ms after our release TX and resumes having skipped one
        counter — all observed +2 skips benign; the ONE true conflict shape,
        beta04, is the native stream BEHIND our last TX). All barriers stay:
        Barrier A, physical-stalk lock, 8/9 fault lock, permit chain.
        """
        module_src = (ROOT / "include" / "dash_ap_rerequest_activation.h").read_text(
            encoding="utf-8"
        )

        # 1. Counter tolerance (方案一): conflict = native behind / more than
        #    2 ahead of lastTx on the 4-bit ring; a +2 skip tallies
        #    counterSkips and the round continues.
        self.assertIn(
            "static_cast<uint8_t>((nativeCounter - state_.lastTxCounter) & 0x0F)",
            module_src,
        )
        self.assertIn("if (ahead > 2)", module_src)
        self.assertIn('failLocked("counterConflict");', module_src)
        self.assertIn("if (ahead == 2)\n                ++state_.counterSkips;", module_src)
        self.assertIn("uint32_t counterSkips = 0;", module_src)

        # 2. Reclassification (方案二): request-refusal and ignored-cancel are
        #    vehicle-side transients — auto re-arm inside the same capped set.
        for reason in ("apExitedDuringRequest", "cancelIgnored"):
            self.assertIn(f'strcmp(r, "{reason}") == 0', module_src)
        # Timeout classification: car still active at the evidence deadline =
        # cancelIgnored (re-arm); any other limbo keeps the hardware-class
        # cancelEvidenceTimeout lock.
        self.assertIn(
            'failLocked(carActive ? "cancelIgnored" : "cancelEvidenceTimeout")',
            module_src,
        )

        # 3a. In-round request retry after a refusal, covering BOTH detection
        #     phases (0x399 is 2 Hz — the 2->1 frame lands after the release
        #     most of the time).
        self.assertIn("uint8_t requestRetries = 0;", module_src)
        self.assertIn("p.requestRetries = 2;", module_src)
        # Budget counts retries USED, not presses — requestRetries == 0 must
        # stay strictly identical to the beta13 behavior.
        self.assertIn(
            "return state_.requestRetries < state_.profile.requestRetries;",
            module_src,
        )
        self.assertIn("void enterRequestRetryWait(uint32_t now)", module_src)
        self.assertIn('state_.reason = "requestRetryWait";', module_src)
        reseq = module_src.index("case Phase::RequestSequence:")
        waitq = module_src.index("case Phase::WaitQualification:")
        self.assertIn("enterRequestRetryWait(now);", module_src[reseq:waitq])
        self.assertIn("enterRequestRetryWait(now);", module_src[waitq:])

        # 3b. Cancel re-send: one extra gesture after the delay, deadline
        #     extends to cancelRetryTimeoutMs, profile pair validated.
        self.assertIn("uint32_t cancelRetryDelayMs = 0;", module_src)
        self.assertIn("uint32_t cancelRetryTimeoutMs = 0;", module_src)
        self.assertIn('return "cancelRetryUnconfigured";', module_src)
        self.assertIn("kCancelRetryMax = 1;", module_src)
        self.assertIn("cancelEvidenceDeadlineLocked()", module_src)
        self.assertIn('state_.reason = "cancelRetryArmed";', module_src)
        self.assertIn("p.cancelRetryDelayMs = 700;", module_src)
        self.assertIn("p.cancelRetryTimeoutMs = 2200;", module_src)

        # 4. memcmp preserve guard: the profile must stay padding-free or the
        #    configure() identical-profile compare reads stale stack bytes and
        #    spuriously full-resets mid-injection (surfaced by the beta14
        #    native tests once the retry fields opened a hole).
        self.assertIn(
            "static_assert(sizeof(ApReRequestProfile) == 40,",
            module_src,
        )

        # 5. JSON chain: three NUMERIC keys after counterConflicts (numeric
        #    values close with ,"key": — never a string close; fifth JSON
        #    incident class). Order-pinned: counterConflicts -> new keys ->
        #    native3eeRx.
        cc = 'j += R"JSON(,"counterConflicts":)JSON";'
        n3 = 'j += R"JSON(,"native3eeRx":)JSON";'
        self.assertIn(cc, self.dash)
        self.assertIn(n3, self.dash)
        seg = self.dash[self.dash.index(cc) : self.dash.index(n3)]
        for key in ("counterSkips", "cancelRetries", "requestRetries"):
            self.assertIn(f'j += R"JSON(,"{key}":)JSON";', seg)
            self.assertIn(f"j += rr.{key};", seg)
        # Simulator mirror: same keys, default zeros.
        for key in ("counterSkips", "cancelRetries", "requestRetries"):
            self.assertIn(f'"{key}": 0,', self.simulator)

        # 6. UI rrPhaseMap: the three new reasons localize in BOTH the source
        #    and the generated (minified) header.
        for surface in (self.ui, self.ui_gen):
            for token in ("requestRetryWait", "cancelRetryArmed", "cancelIgnored"):
                self.assertIn(token, surface)
            self.assertIn("轮内重试等待", surface)
            self.assertIn("车辆无视取消", surface)


if __name__ == "__main__":
    unittest.main()
