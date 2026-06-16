import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "include" / "web" / "mcp2515_dashboard_ui.src.h").read_text(encoding="utf-8")


class UiV11RedesignTests(unittest.TestCase):
    def assert_present(self, needle: str) -> None:
        self.assertIn(needle, SRC, f"UI 源缺少: {needle!r}")

    def assert_absent(self, needle: str) -> None:
        self.assertNotIn(needle, SRC, f"UI 源不应再含: {needle!r}")

    def assert_id_present(self, element_id: str) -> None:
        pattern = rf'\bid="{re.escape(element_id)}"'
        self.assertRegex(SRC, pattern, f"缺少 id={element_id}")


class DesignSystemTests(UiV11RedesignTests):
    def test_ds_tokens_defined(self):
        for tok in ["--ds-cyan", "--ds-indigo", "--ds-accent-grad"]:
            self.assert_present(tok + ":")

    def test_card_is_cockpit_glass(self):
        # .card 重定义为驾驶舱玻璃风：深色 + backdrop-filter
        self.assertRegex(SRC, r"\.card\s*\{[^}]*backdrop-filter")
        self.assert_present("--ds-accent-grad")


class RemoveBottomBlocksTests(UiV11RedesignTests):
    def test_removed_titles_absent(self):
        for t in ["维修模式 0x339", "方向盘免打扰 0x3C2", "Legacy 功能支持矩阵"]:
            self.assert_absent(t)

    def test_retained_neighbors_present(self):
        # 相邻保留块
        self.assert_present("Legacy CAN-A 诊断")
        self.assert_present("AP 注入安全")


class Can2ToApTests(UiV11RedesignTests):
    def test_ap_metric_replaces_can2(self):
        self.assert_id_present("ov-ap")          # 新 AP 激活格
        self.assert_present("AP 激活")
        # 原 CAN2 metric 的 id 不应再作为 CAN2 指标出现
        self.assert_absent('<div class="lbl">CAN2</div><div class="val" id="s-can2">')

    def test_ap_update_js_present(self):
        self.assert_present("setText('ov-ap'")


class ApInjectionRedesignTests(UiV11RedesignTests):
    def test_ap_card_cockpit_structure(self):
        self.assert_present("🛡")                    # 卡头图标
        self.assert_present("AP 注入安全")
        self.assert_id_present("ap-state-panel")     # 状态主区
        self.assert_id_present("ap-gate-bar")        # Gate 进度条

    def test_controls_chinese(self):
        self.assert_present("AP 门控")
        self.assert_present("延迟注入")
        self.assert_present("AP 自动恢复")

    def test_failclosed_note_present(self):
        self.assert_present("Fail-closed")           # 安全条

    def test_original_ids_preserved(self):
        # 硬要求：原 id 全保留（review 会查）
        for i in ["ap-core-card", "ap-core-state-pill", "ap-core-gate-tgl",
                  "ap-delay-select", "ap-auto-restore-tgl", "injection-source",
                  "ap-core-state-detail"]:
            self.assert_id_present(i)


class PluginRedesignTests(UiV11RedesignTests):
    def test_plugin_card_cockpit(self):
        self.assert_present("🧩")
        self.assert_present("插件管理")
        self.assert_id_present("plugin-url")
        self.assert_id_present("plugin-file")
        self.assert_id_present("plugin-json")
        self.assert_id_present("plugin-list")
        self.assert_id_present("plugin-replay-count")

    def test_no_english_install_labels(self):
        # 中文化：英文按钮文案应移除
        self.assert_absent("Official JSON plugin manager")
        self.assert_absent("Install from URL")

    def test_original_ids_preserved(self):
        for i in ["plugins-card","plugin-editor-json","plugin-rule-test-result"]:
            self.assert_id_present(i)


class MobileNavTests(UiV11RedesignTests):
    def test_single_can_mobile_nav_has_five_task6_tabs(self):
        for page in ["driving", "hardware", "speed", "network", "more"]:
            self.assert_present(f'data-mobile-page="{page}"')
        self.assert_absent('data-mobile-page="home"')
        self.assert_absent('data-mobile-page="plugins"')

    def test_single_can_more_panel_contains_only_task6_entries(self):
        self.assert_present('id="mob-more-single"')
        match = re.search(r'<div class="mob-more-panel" id="mob-more-single"[\s\S]*?<!-- Mobile Dual-CAN Nav', SRC)
        self.assertIsNotNone(match, "缺少单 CAN 更多面板")
        panel = match.group(0) if match else ""
        for label in ["驾驶风格", "FSD 防护", "OTA 升级", "CAN 诊断"]:
            self.assertIn(label, panel)
        for label in ["CAN2 控制", "灯光特技", "自动换挡"]:
            self.assertNotIn(label, panel, f"单 CAN 更多面板不应包含 {label}")

    def test_mobile_page_map_targets_hardware_and_speed(self):
        self.assertRegex(SRC, r"hardware\s*:\s*'pg-hardware'")
        self.assertRegex(SRC, r"speed\s*:\s*'pg-speed'")

    def test_single_can_more_entries_route_to_expected_pages(self):
        show_mobile = re.search(r"function showMobilePage\(name\)\{[\s\S]*?var map=\{([^}]*)\}", SRC)
        self.assertIsNotNone(show_mobile, "缺少 showMobilePage map")
        show_mobile_map = show_mobile.group(1) if show_mobile else ""
        for mobile_name, page_id in [
            ("drivingstyle", "pg-drive"),
            ("defense", "pg-defense"),
            ("ota", "pg-ota"),
            ("can", "pg-can"),
        ]:
            self.assert_present(f"showStandaloneMorePage('{mobile_name}')")
            self.assertRegex(show_mobile_map, rf"{mobile_name}\s*:\s*'{page_id}'")

    def test_dual_can_mobile_nav_is_preserved(self):
        self.assert_present('id="mob-tabs-dual"')
        self.assert_present('id="mob-more" data-single-hide="1"')
        for page in ["pg-bus2", "pg-strobe", "pg-shift"]:
            self.assertRegex(SRC, rf'id="mob-more"[\s\S]*data-page="{page}"')


if __name__ == "__main__":
    unittest.main()
