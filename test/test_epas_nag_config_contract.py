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


if __name__ == "__main__":
    unittest.main()
