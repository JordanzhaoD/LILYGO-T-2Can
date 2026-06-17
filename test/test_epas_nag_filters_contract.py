import unittest
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = (ROOT / "include" / "handlers.h").read_text(encoding="utf-8")


def handler_block(name):
    m = re.search(r"struct %s\s*:[^=]*?\n(?:.*?\n)*?\s*uint8_t filterIdCount\(\) const override \{ return \d+; \}" % name, H)
    return m.group(0) if m else ""


def filter_entries_and_count(name):
    b = handler_block(name)
    if not b:
        return [], None
    arr = re.search(r"ids\[\]\s*=\s*\{([^}]*)\}", b)
    cnt = re.search(r"filterIdCount\(\) const override \{ return (\d+); \}", b)
    if not arr or not cnt:
        return [], None
    entries = [x.strip() for x in arr.group(1).split(",") if x.strip()]
    return entries, int(cnt.group(1))


class FiltersContract(unittest.TestCase):
    def assert_filter_contract(self, name, required_ids):
        entries, count = filter_entries_and_count(name)
        self.assertTrue(entries, "%s ids[] entries not found" % name)
        self.assertIsNotNone(count, "%s filterIdCount() not found" % name)
        self.assertEqual(10, count, "%s filterIdCount() must remain exactly 10" % name)
        self.assertEqual(count, len(entries), "%s count mismatch" % name)
        for required_id in required_ids:
            self.assertIn(required_id, entries, "%s missing %s in ids[]" % (name, required_id))

    def test_legacy_includes_0x370(self):
        self.assert_filter_contract("LegacyHandler", ["880"])

    def test_hw3_includes_0x370_and_0x39b(self):
        self.assert_filter_contract("HW3Handler", ["880", "923"])

    def test_hw4_includes_0x370_and_0x39b(self):
        self.assert_filter_contract("HW4Handler", ["880", "923"])


if __name__ == "__main__":
    unittest.main()
