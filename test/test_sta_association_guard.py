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
#
# Fix A (2026-11-03, round-5 follow-up) re-topologized the routing: the
# 250ms-1s pollers must not cross the hosted bus at all (status() P4 branch
# is event-cache + local netif table only; begin() consults the flag), and
# the display renderers go through a 10s lazy cache (espWifiStaInfoCached)
# that itself routes through the safe helper. Topology now:
#   raw RPC          -> exactly one site, inside espWifiStaApInfoSafe
#   runtime safe     -> begin(#else) / status(#else) / cached refresh = 3
#   runtime cached   -> SSID() = 1
#   gateway cached   -> dashGatewayStatusJson = 1
#   dash  fresh safe -> dashSyncApChannelToSta = 1 (control decision)
#   dash  cached     -> 2 hasRssi builders = 2
K_RUNTIME_SITES = 3  # begin(#else) / status(#else) / cached refresh
K_RUNTIME_CACHED_SITES = 1  # SSID()
K_GATEWAY_SITES = 0
K_GATEWAY_CACHED_SITES = 1
K_DASH_SITES = 1  # dashSyncApChannelToSta (fresh read: control path)
K_DASH_CACHED_SITES = 2  # RSSI/network JSON + serial status dump


def _case_block(source: str, label: str) -> str:
    """Slice from a switch-case label to the next case label (or 800 chars)."""
    start = source.index(label)
    rest = source[start + len(label):]
    end = rest.find("case WIFI_EVENT_")
    return rest[: end if end != -1 else 800]


def _strip_line_comments(block: str) -> str:
    """Drop `//` comments so code-level assertions ignore documentation."""
    return "\n".join(
        line.split("//", 1)[0] if "//" in line else line
        for line in block.splitlines()
    )


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
        self.assertIn("bool espWifiStaInfoCached(wifi_ap_record_t *ap);",
                      self.platform)

    def test_cached_variant_refreshes_through_helper(self) -> None:
        # The lazy cache must never own a second raw RPC site.
        start = self.runtime.index("bool espWifiStaInfoCached(wifi_ap_record_t")
        body = self.runtime[start:start + 1200]
        self.assertIn("espWifiStaApInfoSafe(&cached)", body)
        self.assertNotIn("esp_wifi_sta_get_ap_info(", body)

    def test_status_p4_branch_is_zero_rpc(self) -> None:
        # Fix A: the poll path must not cross the hosted bridge. The P4
        # branch answers from the event-maintained flag + local netif table.
        # (Comments stripped first: the branch documents the RPC it avoids.)
        start = self.runtime.index("wl_status_t WiFiClass::status()")
        body = self.runtime[start:start + 2400]
        self.assertIn("#if defined(DRIVER_P4_3CAN)", body)
        p4_branch = _strip_line_comments(body[: body.index("#else")])
        self.assertIn("if (wifiStaAssociated)", p4_branch)
        self.assertIn("esp_netif_get_ip_info", p4_branch)
        self.assertNotIn("espWifiStaApInfoSafe", p4_branch,
                         "P4 status must not call the RPC helper")
        self.assertNotIn("esp_wifi_sta_get_ap_info", p4_branch)

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
            "begin(#else)/status(#else)/cached-refresh route through the helper",
        )
        self.assertEqual(
            self.runtime.count("espWifiStaInfoCached(&"),
            K_RUNTIME_CACHED_SITES,
            "SSID() renders from the lazy cache",
        )
        self.assertNotIn("esp_wifi_sta_get_ap_info(", self.gateway)
        self.assertEqual(
            self.gateway.count("espWifiStaApInfoSafe(&"), K_GATEWAY_SITES
        )
        self.assertEqual(
            self.gateway.count("espWifiStaInfoCached(&"), K_GATEWAY_CACHED_SITES
        )
        self.assertNotIn("esp_wifi_sta_get_ap_info(", self.dash)
        self.assertEqual(
            self.dash.count("espWifiStaApInfoSafe(&"), K_DASH_SITES
        )
        self.assertEqual(
            self.dash.count("espWifiStaInfoCached(&"), K_DASH_CACHED_SITES
        )

    def test_sta_drop_diag_exists(self) -> None:
        self.assertIn("sta_drop", self.dash)
        self.assertIn('strcmp(start, "sta_drop")', self.dash)


if __name__ == "__main__":
    unittest.main()
