import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
D = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(encoding="utf-8")


class ConfigContract(unittest.TestCase):
    def test_persist_save_key(self):
        self.assertIn('putBool("def_epnag", dashEpasNagEnabled)', D)

    def test_persist_load_key(self):
        self.assertIn('dashEpasNagEnabled = prefs.getBool("def_epnag", false)', D)

    def test_http_handler_reads_epas_nag(self):
        self.assertIn('server.hasArg("epas_nag")', D)
        self.assertIn('dashEpasNagEnabled = dashArgTruthy(server.arg("epas_nag"))', D)

    def test_defense_config_json_has_field(self):
        self.assertIn('",\\"epas_nag\\":', D)

    def test_status_json_has_field(self):
        self.assertIn('",\\"epasNag\\":', D)


def test_0x129_feeds_onSteeringAngle():
    import pathlib
    p = pathlib.Path("include/web/mcp2515_dashboard.h")
    src = p.read_text()
    # the 0x129 block must call onSteeringAngle with the parsed steerAngleX10
    block = src[src.index("frame.id == 0x129 && frame.dlc >= 6"):]
    block = block[:block.index("return;")]
    assert "dashEpasNagCtrl.onSteeringAngle" in block, \
        "0x129 block must feed steerAngleX10 to the nag engine"
    assert "steerAngleX10" in block


def test_epasNagDiag_has_modec_fields():
    import pathlib
    src = pathlib.Path("include/web/mcp2515_dashboard.h").read_text()
    # The C++ builds JSON via string literals with escaped quotes, so the
    # source contains \"epasNagDiag\". Accept both escaped and unescaped.
    key = '\\"epasNagDiag\\"' if '\\"epasNagDiag\\"' in src else '"epasNagDiag"'
    block = src[src.index(key):]
    # bound the block at the next function/section; appendCapabilitiesJson
    # terminates the epasNagDiag JSON construction in handleStatus.
    end_anchor = "appendCapabilitiesJson" if "appendCapabilitiesJson" in block else None
    if end_anchor:
        block = block[:block.index(end_anchor)]
    # Mode-C: nested "demand" object (acceptance: /status.epasNagDiag.demand.gate
    # toggles s2_mild/strong_ramp/strong_hold; demand.torque / demand.handsLevel).
    assert '\\"demand\\"' in block or '"demand"' in block, 'epasNagDiag JSON missing "demand" object'
    assert '\\"state\\"' in block, 'demand object missing state'
    assert '\\"torque\\"' in block, 'demand object missing torque'
    assert '\\"handsLevel\\"' in block, 'demand object missing handsLevel'
    assert '\\"gate\\"' in block, 'demand object missing gate'


if __name__ == "__main__":
    unittest.main()
