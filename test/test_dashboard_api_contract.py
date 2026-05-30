import unittest
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UI_SRC = ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h"
DASH = ROOT / "include" / "web" / "mcp2515_dashboard.h"


class DashboardApiContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ui = UI_SRC.read_text(encoding="utf-8")
        cls.dash = DASH.read_text(encoding="utf-8")

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

    def test_backend_accepts_panel_control_methods(self) -> None:
        expected_routes = [
            'server.on("/reset_stats", HTTP_POST, handleResetStats);',
            'server.on("/reboot", HTTP_POST, handleReboot);',
            'server.on("/service_mode", HTTP_POST, handleServiceMode);',
        ]
        for route in expected_routes:
            with self.subTest(route=route):
                self.assertIn(route, self.dash)

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
            ("GET", "/lighting_config"),
            ("GET", "/defense_config"),
            ("GET", "/gear_assist_status"),
            ("GET", "/hotspot_config"),
            ("GET", "/dns_rules"),
            ("POST", "/config"),
            ("POST", "/mode_hw"),
            ("POST", "/drive_profile"),
            ("POST", "/speed_strategy"),
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

    def test_ap_status_mode_field_matches_ui(self) -> None:
        self.assertIn("setText('ap-mode',ap.mode||", self.ui)
        self.assertIn('",\\"mode\\":\\""', self.dash)


if __name__ == "__main__":
    unittest.main()
