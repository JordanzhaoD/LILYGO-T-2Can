import unittest
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI_SRC = ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h"
UI_GEN = ROOT / "include" / "web" / "mcp2515_dashboard_ui.h"
DASH = ROOT / "include" / "web" / "mcp2515_dashboard.h"
HANDLERS = ROOT / "include" / "handlers.h"
LEGACY_SPEED = ROOT / "include" / "dash_legacy_speed.h"
MAIN = ROOT / "src" / "main.cpp"
VERSION = ROOT / "VERSION"
CHANGELOG = ROOT / "CHANGELOG.md"
README = ROOT / "README.md"
TESTS_WORKFLOW = ROOT / ".github" / "workflows" / "tests.yml"
RELEASE_WORKFLOW = ROOT / ".github" / "workflows" / "release.yml"


class DashboardApiContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ui = UI_SRC.read_text(encoding="utf-8")
        cls.ui_gen = UI_GEN.read_text(encoding="utf-8")
        cls.dash = DASH.read_text(encoding="utf-8")
        cls.handlers = HANDLERS.read_text(encoding="utf-8")
        cls.legacy_speed = LEGACY_SPEED.read_text(encoding="utf-8")
        cls.main = MAIN.read_text(encoding="utf-8")
        cls.version = VERSION.read_text(encoding="utf-8")
        cls.changelog = CHANGELOG.read_text(encoding="utf-8")
        cls.readme = README.read_text(encoding="utf-8")
        cls.tests_workflow = TESTS_WORKFLOW.read_text(encoding="utf-8")
        cls.release_workflow = RELEASE_WORKFLOW.read_text(encoding="utf-8")

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
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui_gen)

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
        self.assertNotIn("new FormData()", self.ui)
        self.assertNotIn("fd.append('file',file)", self.ui)

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

    def test_legacy_can760_uses_simple_offset_helper_not_mpp(self) -> None:
        """Legacy speed offset must use the verified CAN760 byte5 UI_userSpeedOffset path."""
        can760 = re.search(r"if \(frame\.id == 760\).*?if \(frame\.id == 1080\)", self.handlers, re.S)
        self.assertIsNotNone(can760)
        body = can760.group(0)
        self.assertIn('dashComputeLegacySimpleOffsetKph', body)
        self.assertIn('frame.data[5]', body)
        self.assertNotIn('dashComputeLegacyMppTargetKph', body)
        self.assertNotIn('frame.data[6]', body)

    def test_legacy_handler_captures_fused_speed_limit_from_921(self) -> None:
        """Legacy/HW0 needs the same fused speed limit input as the 3-mode speed UI."""
        can921 = re.search(r"if \(frame\.id == 921\).*?// 0x3EE", self.handlers, re.S)
        self.assertIsNotNone(can921)
        body = can921.group(0)
        self.assertIn('fusedSpeedLimitRaw = static_cast<uint8_t>(frame.data[1] & 0x1F);', body)
        self.assertIn('APActive = isDASAutopilotActive(readDASAutopilotStatus(frame));', body)

    def test_legacy_simple_offset_helper_reuses_three_mode_state(self) -> None:
        """Legacy simple offset should reuse the speed page algorithm and clamp to byte5 wire range."""
        self.assertIn('kLegacySimpleOffsetMaxKph = 33', self.legacy_speed)
        self.assertIn('dashComputeLegacySimpleOffsetKph', self.legacy_speed)
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
        speed_custom = re.search(r"static void handleSpeedCustom\(\).*?static String dashLightingConfigJson\(\)", self.dash, re.S)
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
        # 50ms step interval
        self.assertIn("kStepIntervalMs{50}", dnd)

    def test_phase3_wheel_dnd_checksum(self) -> None:
        """DND frames must include Tesla checksum calculation."""
        dnd = (ROOT / "include" / "dash_wheel_dnd.h").read_text(encoding="utf-8")
        self.assertIn("0xC2u + 0x03u", dnd)  # CAN ID 0x3C2 bytes
        self.assertIn("outData[7]", dnd)

    def test_phase3_wheel_dnd_is_runtime_wired(self) -> None:
        """Wheel DND must be instantiated, started by API, and ticked from CAN task."""
        self.assertIn('#include "dash_wheel_dnd.h"', self.dash)
        self.assertIn("static DashWheelDND dashWheelDndCtrl;", self.dash)
        defense = re.search(r"static void handleDefenseConfig\(\).*?server\.send\(200", self.dash, re.S)
        self.assertIsNotNone(defense)
        body = defense.group(0)
        self.assertIn("dashWheelDndCtrl.startVolume()", body)
        self.assertIn("dashWheelDndCtrl.startSpeed()", body)
        self.assertIn("static void t2canWheelDndTick()", self.main)
        self.assertIn("dashWheelDndCtrl.tick((int)millis(), data)", self.main)
        self.assertIn("f.id = 0x3C2;", self.main)
        self.assertIn("gateOpen = canActive && dashDefenseEnabled", self.main)
        self.assertIn("g_wheelDndGateWasOpen", self.main)
        self.assertIn("dashWheelDndCtrl.reset()", self.main)
        can_task = re.search(r"static void app_can_task\(void \*\).*?appCanTaskLoops", self.main, re.S)
        self.assertIsNotNone(can_task)
        self.assertIn("t2canWheelDndTick();", can_task.group(0))

    def test_phase3_wheel_dnd_only_starts_when_defense_enabled(self) -> None:
        """DND switches should not inject frames unless the defense system is enabled."""
        defense = re.search(r"static void handleDefenseConfig\(\).*?server\.send\(200", self.dash, re.S)
        self.assertIsNotNone(defense)
        body = defense.group(0)
        self.assertIn("dashDefenseEnabled && dashDndVolume && (!prevDefenseEnabled || !prevDndVolume)", body)
        self.assertIn("dashDefenseEnabled && dashDndSpeed && (!prevDefenseEnabled || !prevDndSpeed)", body)

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
        """defense_config must accept and return dnd_volume and dnd_speed."""
        defense = re.search(r"static void handleDefenseConfig\(\).*?server\.send\(200", self.dash, re.S)
        self.assertIsNotNone(defense)
        body = defense.group(0)
        self.assertIn('server.hasArg("dnd_volume")', body)
        self.assertIn('server.hasArg("dnd_speed")', body)
        self.assertIn("dashDndVolume", body)
        self.assertIn("dashDndSpeed", body)

    def test_phase3_defense_config_json_includes_bionic_status(self) -> None:
        """defense_config JSON must report bionic_disabled for UI warning."""
        json_fn = re.search(r"static String dashDefenseConfigJson\(\).*?return j;", self.dash, re.S)
        self.assertIsNotNone(json_fn)
        body = json_fn.group(0)
        self.assertIn('\\"bionic_disabled\\"', body)
        self.assertIn('\\"dnd_volume\\"', body)
        self.assertIn('\\"dnd_speed\\"', body)
        self.assertIn("bionicDisabled()", body)
        self.assertIn("dashBionicDisabled", body)

    def test_phase3_defense_ui_has_7_toggles(self) -> None:
        """Defense page must have all 7 toggle switches in pg-defense section."""
        defense_page = re.search(r'id="pg-defense".*?id="pg-ota"', self.ui, re.S)
        self.assertIsNotNone(defense_page)
        body = defense_page.group(0)
        toggles = [
            'id="hw3-slew-tgl"',
            'id="def-bionic-tgl"',
            'id="def-sound-tgl"',
            'id="def-dnd-vol-tgl"',
            'id="def-speed-nd-tgl"',
            'id="def-dnd-spd-tgl"',
            'id="def-apeap-tgl"',
        ]
        for tid in toggles:
            with self.subTest(toggle=tid):
                self.assertIn(tid, body)

    def test_phase3_defense_ui_bionic_warning_element(self) -> None:
        """UI must have bionic-disabled warning element."""
        self.assertIn('id="def-bionic-warn"', self.ui)
        self.assertIn("bionic_disabled", self.ui)

    def test_phase3_defense_js_saves_dnd_params(self) -> None:
        """saveDefenseConfig JS must POST dnd_volume and dnd_speed."""
        save_fn = re.search(r"async function saveDefenseConfig\(\)\{.*?\}", self.ui, re.S)
        self.assertIsNotNone(save_fn)
        body = save_fn.group(0)
        self.assertIn("def-dnd-vol-tgl", body)
        self.assertIn("def-dnd-spd-tgl", body)
        self.assertIn("dnd_volume", body)
        self.assertIn("dnd_speed", body)

    def test_phase3_defense_js_loads_dnd_params(self) -> None:
        """loadDefenseConfig JS must read dnd_volume and dnd_speed."""
        load_fn = re.search(r"async function loadDefenseConfig\(\)\{.*?setText\('tb-exp'", self.ui, re.S)
        self.assertIsNotNone(load_fn)
        body = load_fn.group(0)
        self.assertIn("d.dnd_volume", body)
        self.assertIn("d.dnd_speed", body)
        self.assertIn("def-dnd-vol-tgl", body)
        self.assertIn("def-dnd-spd-tgl", body)

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
        self.assertIn("nagKillerRuntime = canActive && dashDefenseEnabled", self.dash)
        self.assertIn("dashHandler->resetBionic((uint32_t)millis())", self.dash)
        self.assertIn('prefs.putBool("def_dv", dashDndVolume);', self.dash)
        self.assertIn('prefs.putBool("def_ds", dashDndSpeed);', self.dash)
        self.assertIn('dashDndVolume = prefs.getBool("def_dv", false);', self.dash)
        self.assertIn('dashDndSpeed = prefs.getBool("def_ds", false);', self.dash)
        status = re.search(r"static void handleStatus\(\).*?server\.send", self.dash, re.S)
        self.assertIsNotNone(status)
        self.assertIn("dashDndSpeed ? \"true\" : \"false\"", status.group(0))

    def test_phase3_handlers_includes_bionic_header(self) -> None:
        """handlers.h must include dash_bionic_steer.h."""
        self.assertIn('#include "dash_bionic_steer.h"', self.handlers)

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
        """Page navigation must load strobe page data."""
        self.assertIn("pageId==='pg-strobe')loadStrobePage()", self.ui)

    def test_phase5a_shift_page_loads_read_only_telemetry(self) -> None:
        """Auto-shift placeholder page must populate its read-only telemetry fields."""
        self.assertIn("/gear_assist_status", self.ui)
        self.assertIn("async function pollGearAssist()", self.ui)
        self.assertIn("pageId==='pg-shift')pollGearAssist()", self.ui)
        self.assertIn("pid==='pg-shift')pollGearAssist()", self.ui)
        for token in ["shift-speed", "shift-gear", "shift-brake", "shift-fsd"]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_phase5a_drive_profile_preserves_six_modes(self) -> None:
        """Drive UI should use driveProfile/driveProfileName so Auto/Sloth/MAX survive polling."""
        self.assertIn("function driveModeFromProfile(profile,name)", self.ui)
        self.assertIn("driveModeFromProfile(d.driveProfile,d.driveProfileName)", self.ui)
        self.assertIn("driveMap[mode]!==undefined?driveMap[mode]:3", self.ui)
        self.assertNotIn("[mode]||3", self.ui)

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
        """New UI pages must not break nav translations, and overview version should use /system_status firmware."""
        self.assertIn("'灯光特技':'Light Show'", self.ui)
        self.assertIn("'自动换挡':'Auto Shift'", self.ui)
        self.assertIn("'模块配置','激活模式','驾驶模式','速度偏移','CAN2控制','灯光特技','FSD防御','OTA升级','网络设置','CAN工具','自动换挡'", self.ui)
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

    def test_release_metadata_and_lilygo_ci_are_wired(self) -> None:
        """Release metadata and workflows must cover the LILYGO T-2CAN artifact."""
        self.assertEqual("4.0.2", self.version.strip())
        self.assertIn("## [4.0.2] - 2026-05-31", self.changelog)
        self.assertIn("lilygo_t2can_dual", self.tests_workflow)
        self.assertIn("lilygo_t2can_dual", self.release_workflow)
        self.assertIn("firmware-lilygo-t2can-dual", self.release_workflow)
        self.assertIn("release-assets/firmware-lilygo-t2can-dual.bin", self.release_workflow)


if __name__ == "__main__":
    unittest.main()
