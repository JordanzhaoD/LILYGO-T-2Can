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


if __name__ == "__main__":
    unittest.main()
