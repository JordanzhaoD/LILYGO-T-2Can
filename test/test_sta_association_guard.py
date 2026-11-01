"""Contract tests for the STA association gate (field panic fix 2026-11-01).

Field evidence (coredump, resets=[[4,1]x5]): esp_hosted 2.12.13 returns
"SUCCESS + NULL payload" from rpc_wifi_sta_get_ap_info whenever the STA is
not associated; hosted_memcpy(dest, NULL, ...) asserts and panics the whole
board. The web task polls WiFi.status() every second, so the first poll
after any STA drop crashed the board. The fix routes every
esp_wifi_sta_get_ap_info call site through espWifiStaApInfoSafe(), which
consults an event-maintained association flag (P4 only — T2CAN's native API
returns a safe error instead of asserting).
"""

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "src" / "espidf_runtime.cpp"
PLATFORM_H = ROOT / "include" / "platform" / "espidf_runtime.h"
GATEWAY = ROOT / "include" / "web" / "dash_gateway.h"
DASH = ROOT / "include" / "web" / "mcp2515_dashboard.h"

# Field crash call sites (coredump stack + repo audit 2026-11-01): three in
# the runtime shim, one in the AP gateway status, three in the dashboard
# (channel sync probe + two RSSI/JSON builders).
K_RUNTIME_SITES = 3  # begin / status / SSID
K_GATEWAY_SITES = 1
K_DASH_SITES = 3  # dashSyncApChannelToSta + 2 hasRssi builders


def _case_block(source: str, label: str) -> str:
    """Slice from a switch-case label to the next case label (or 800 chars)."""
    start = source.index(label)
    rest = source[start + len(label):]
    end = rest.find("case WIFI_EVENT_")
    return rest[: end if end != -1 else 800]


class StaAssociationGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime = RUNTIME.read_text(encoding="utf-8")
        cls.platform = PLATFORM_H.read_text(encoding="utf-8")
        cls.gateway = GATEWAY.read_text(encoding="utf-8")
        cls.dash = DASH.read_text(encoding="utf-8")

    def test_flag_exists_and_starts_false(self) -> None:
        self.assertIn("static volatile bool wifiStaAssociated = false;",
                      self.runtime)

    def test_handler_sets_flag_on_both_events(self) -> None:
        connected = _case_block(self.runtime, "case WIFI_EVENT_STA_CONNECTED:")
        self.assertIn("wifiStaAssociated = true;", connected)
        dropped = _case_block(self.runtime, "case WIFI_EVENT_STA_DISCONNECTED:")
        self.assertIn("wifiStaAssociated = false;", dropped)

    def test_helper_guards_under_p4_macro(self) -> None:
        start = self.runtime.index("bool espWifiStaApInfoSafe(wifi_ap_record_t")
        body = self.runtime[start:start + 700]
        self.assertIn("#if defined(DRIVER_P4_3CAN)", body)
        guard_at = body.index("if (!wifiStaAssociated)")
        rpc_at = body.index("esp_wifi_sta_get_ap_info")
        self.assertLess(guard_at, rpc_at, "flag check must precede the RPC")

    def test_helper_declared_in_platform_header(self) -> None:
        self.assertIn("bool espWifiStaApInfoSafe(wifi_ap_record_t *ap);",
                      self.platform)

    def test_all_seven_sites_routed(self) -> None:
        # The only surviving raw RPC call is the helper's own, in the shim.
        self.assertEqual(
            self.runtime.count("esp_wifi_sta_get_ap_info("),
            1,
            "runtime must call the RPC exactly once (inside the helper)",
        )
        self.assertEqual(
            self.runtime.count("espWifiStaApInfoSafe(&"),
            K_RUNTIME_SITES,
            "begin/status/SSID must route through the safe helper",
        )
        self.assertNotIn("esp_wifi_sta_get_ap_info(", self.gateway)
        self.assertEqual(
            self.gateway.count("espWifiStaApInfoSafe(&"), K_GATEWAY_SITES
        )
        self.assertNotIn("esp_wifi_sta_get_ap_info(", self.dash)
        self.assertEqual(
            self.dash.count("espWifiStaApInfoSafe(&"), K_DASH_SITES
        )

    def test_sta_drop_diag_exists(self) -> None:
        self.assertIn("sta_drop", self.dash)
        self.assertIn('strcmp(start, "sta_drop")', self.dash)


if __name__ == "__main__":
    unittest.main()
