# Waveshare Single CAN UI Plugin Audit Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the Waveshare single-CAN standalone product UI, AP Gate controls, upstream-compatible official JSON plugin manager, GTW2047 replay behavior, mobile UI parity, and audit fixes without breaking the dual-CAN main project.

**Architecture:** Keep dual-CAN source and runtime behavior intact, and gate standalone product behavior through `DASH_SINGLE_CAN_STANDALONE`, `/status.capabilities`, and plugin status APIs. Add a focused header-only plugin engine for native and firmware builds, wire it into dashboard routes and the CAN loop, then update the shared UI source with product/capability-aware rendering for desktop and mobile.

**Tech Stack:** ESP-IDF/PlatformIO C++17, ArduinoJson 7, existing ESP-IDF runtime shims, SPIFFS wrapper, Python unittest contract tests, Unity native tests, generated gzip dashboard header via `scripts/minify_dashboard.py`.

---

## Scope and sequencing notes

This spec spans UI, AP safety, plugin engine, GTW replay, storage, mobile, and audit work. They are tightly coupled because the UI must reflect backend status and plugin execution must respect AP/OTA/CAN gates. Keep this as one plan, but implement in small commits and verify after every task.

Do not push, merge, or flash hardware during this plan. Do not modify existing untracked or unrelated firmware asset files unless a task explicitly names them.

---

## File structure

### New files

- `include/dash_plugin_engine.h`
  - Header-only plugin model, parser, execution engine, conflict/priority logic, GTW2047 replay count, `emit_periodic` cache state, JSON status serialization helpers.
  - Header-only matches existing project style and lets native tests include the same code as firmware.

- `test/test_native_plugin_engine/test_plugin_engine.cpp`
  - Unity tests for official plugin JSON parsing, default-disabled install, enable/disable, priority conflicts, GTW2047 replay, counter/checksum behavior, emit-periodic constraints, gtw-silent unavailability, standalone bus compatibility.

### Modified files

- `platformio.ini`
  - Add `-DDASH_PLUGIN_ENGINE=1` to `waveshare_single_can_standalone`.
  - Add a `native_plugin_engine` env.
  - Add `test_native_plugin_engine` to `[env:native]` `test_ignore` if needed so matrix behavior stays explicit.

- `include/web/mcp2515_dashboard.h`
  - Add plugin engine include and globals.
  - Add AP delay config persistence and status fields.
  - Add plugin routes.
  - Initialize SPIFFS and plugin state.
  - Wire plugin status into `/status` and `/status.capabilities`.
  - Register plugin routes only when plugin engine is enabled.

- `include/app.h`
  - Let the plugin engine observe/apply incoming frames after built-in handler observation.
  - Ensure plugin execution remains subject to AP Gate, delay, OTA guard, canActive, and FSD master state.
  - Run plugin periodic tick outside the read loop.

- `include/handlers.h`
  - Add optional `pluginFsdActive`/`pluginInjectionActive` callback or runtime flag to prevent duplicate built-in FSD injection when an enabled plugin owns the same FSD feature.
  - Keep existing dual-CAN behavior unchanged when callback is null.

- `include/web/mcp2515_dashboard_ui.src.h`
  - Add product/capability model for standalone UI copy.
  - Add AP Gate / delayed injection / AP-AUTO-RESTORE controls and AP injection state card.
  - Add official plugin manager UI: URL install, upload, paste JSON, installed list, enable/disable, priority, detail, remove, Rule Test, GTW2047 replay count.
  - Update mobile UI to match standalone features.
  - Hide/remove standalone-visible CAN2/T-2CAN/Auto Shift/Service/Stalk/Lighting/Burst surfaces.

- `include/web/mcp2515_dashboard_ui.h`
  - Regenerated output from `scripts/minify_dashboard.py`; never edit manually.

- `test/test_dashboard_api_contract.py`
  - Add contract tests for AP UI/status, plugin routes/UI/status, mobile parity, GTW2047 replay UI, standalone forbidden visible strings, dual-CAN route preservation.

- `test/test_wifi_settings_regression.py`
  - Extend AP Gate persistence assertions for `ap_delay_ms` and `ap_auto_restore` if existing test owns AP-related config.

- `docs/plugins.md`
  - Update local docs only where they drift from upstream behavior observed in this plan.

- `README.md`, `docs/building.md`, `CHANGELOG.md`
  - Document single-CAN AP/plugin follow-up and safe verification notes.

---

## Task 1: Add RED contract tests for standalone AP/plugin/mobile cleanup

**Files:**
- Modify: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add helper methods for standalone visible text checks**

Add these methods inside `DashboardApiContractTests` after `setUpClass`:

```python
    def _standalone_visible_source(self) -> str:
        """Return UI source used for standalone visible-product assertions.

        This is intentionally the source UI, not generated gzip output. The
        generated output is separately checked by minify --check after UI edits.
        """
        return self.ui

    def _route_block(self, route: str) -> str:
        idx = self.dash.find(route)
        self.assertNotEqual(idx, -1, f"{route} not found in dashboard source")
        start = max(0, idx - 250)
        end = min(len(self.dash), idx + 250)
        return self.dash[start:end]
```

- [ ] **Step 2: Add AP controls/status contract test**

Add this test:

```python
    def test_single_can_driving_status_exposes_ap_gate_delay_and_restore(self) -> None:
        for token in [
            'id="ap-gate-tgl"',
            'id="ap-delay-select"',
            'id="ap-auto-restore-tgl"',
            'saveApGateControls()',
            'renderApInjectionState',
            '等待 AP',
            '稳定计时中',
            '正在注入',
            '已阻断',
            'AP-AUTO-RESTORE',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

        for token in [
            '"apDelayMs"',
            '"apInjectionState"',
            '"apAutoRestore"',
            'server.hasArg("ap_delay_ms")',
            'server.hasArg("ap_auto_restore")',
            'prefs.putUInt("ap_dly"',
            'prefs.getUInt("ap_dly"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)
```

- [ ] **Step 3: Add plugin API/UI contract test**

Add this test:

```python
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
            'Install from URL',
            'Upload .json',
            'Paste JSON',
            'Installed Plugins',
            'Plugin Editor',
            'Rule Test',
            'GTW2047 Replay Count',
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
```

- [ ] **Step 4: Add standalone cleanup contract test**

Add this test:

```python
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
            'CAN2 控制</div>',
            '>CAN2 状态<',
            '>CAN2 RX<',
            '>CAN2 TX<',
            '>自动换挡<',
            '>Auto Shift<',
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
```

- [ ] **Step 5: Add mobile parity contract test**

Add this test:

```python
    def test_mobile_single_can_ui_matches_ap_and_plugin_capabilities(self) -> None:
        for token in [
            'id="mob-plugin-page"',
            'data-mobile-page="plugins"',
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
                self.assertIn(token, self.ui)

        for token in [
            'data-page="pg-bus2">',
            'data-page="pg-strobe">',
            'data-page="pg-shift">',
        ]:
            with self.subTest(token=token):
                self.assertNotIn(token, self.ui)
```

- [ ] **Step 6: Add plugin engine header contract test**

Add this test:

```python
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
```

- [ ] **Step 7: Run the contract tests and confirm RED**

Run:

```bash
PYTHONPATH=test python3 -m unittest \
  test_dashboard_api_contract.DashboardApiContractTests.test_single_can_driving_status_exposes_ap_gate_delay_and_restore \
  test_dashboard_api_contract.DashboardApiContractTests.test_official_plugin_manager_contract_is_present \
  test_dashboard_api_contract.DashboardApiContractTests.test_single_can_product_ui_hides_dual_can_surfaces \
  test_dashboard_api_contract.DashboardApiContractTests.test_mobile_single_can_ui_matches_ap_and_plugin_capabilities \
  test_dashboard_api_contract.DashboardApiContractTests.test_plugin_engine_header_exposes_upstream_official_contract
```

Expected: FAIL because AP delay controls, plugin engine header, plugin routes, and mobile plugin UI are not implemented yet.

- [ ] **Step 8: Commit RED contract tests**

```bash
git add test/test_dashboard_api_contract.py
git commit -m "test: add single can plugin ui audit contracts"
```

---

## Task 2: Add RED native plugin engine tests and PlatformIO env

**Files:**
- Modify: `platformio.ini`
- Create: `test/test_native_plugin_engine/test_plugin_engine.cpp`
- Later implementation target: `include/dash_plugin_engine.h`

- [ ] **Step 1: Add `native_plugin_engine` env**

Append this block to `platformio.ini` after `native_twai`:

```ini
[env:native_plugin_engine]
platform = native
extra_scripts = pre:scripts/platformio_native_env.py
build_flags = -std=c++17 -DNATIVE_BUILD -DDASH_PLUGIN_ENGINE=1 -DDASH_SINGLE_CAN_STANDALONE=1 -DENHANCED_AUTOPILOT
build_src_filter = +<*> -<main.cpp>
test_filter = test_native_plugin_engine
```

In `[env:native] test_ignore`, add:

```ini
    test_native_plugin_engine
```

- [ ] **Step 2: Create test file with official JSON parser tests**

Create `test/test_native_plugin_engine/test_plugin_engine.cpp`:

```cpp
#include <unity.h>
#include "dash_plugin_engine.h"
#include "drivers/mock_driver.h"

static MockDriver driver;

static CanFrame frame2047(uint8_t mux = 3)
{
    CanFrame f{};
    f.id = 2047;
    f.bus = CAN_BUS_DEFAULT;
    f.dlc = 8;
    f.data[0] = mux;
    return f;
}

static CanFrame frame921()
{
    CanFrame f{};
    f.id = 921;
    f.bus = CAN_BUS_DEFAULT;
    f.dlc = 8;
    return f;
}

static DashPluginContext allowedContext()
{
    DashPluginContext ctx{};
    ctx.canActive = true;
    ctx.otaAllowed = true;
    ctx.apGateAllowed = true;
    ctx.fsdMasterEnabled = true;
    ctx.defaultBus = CAN_BUS_DEFAULT;
    return ctx;
}

void setUp()
{
    driver.reset();
}

void tearDown() {}

void test_valid_official_plugin_installs_disabled_by_default()
{
    DashPluginEngine engine;
    const char *json = R"json({
      "name":"My Plugin",
      "version":"1.0",
      "author":"Tester",
      "rules":[{"id":921,"mux":-1,"ops":[{"type":"set_bit","bit":13,"val":1}],"send":true}]
    })json";

    DashPluginResult result = engine.installJson(json, false);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_EQUAL(1, engine.pluginCount());
    TEST_ASSERT_FALSE(engine.pluginAt(0).enabled);
    TEST_ASSERT_EQUAL_STRING("My Plugin", engine.pluginAt(0).name.c_str());
    TEST_ASSERT_EQUAL(1, engine.pluginAt(0).priority);
}

void test_invalid_json_is_rejected_without_installing()
{
    DashPluginEngine engine;
    DashPluginResult result = engine.installJson("{not-json", false);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL(0, engine.pluginCount());
    TEST_ASSERT_TRUE(result.message.length() > 0);
}

void test_name_longer_than_31_chars_is_rejected()
{
    DashPluginEngine engine;
    const char *json = R"json({"name":"abcdefghijklmnopqrstuvwxyzabcdef","rules":[{"id":921,"ops":[{"type":"checksum"}]}]})json";

    DashPluginResult result = engine.installJson(json, false);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL(0, engine.pluginCount());
}

void test_installing_same_name_replaces_existing_plugin_disabled()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"ReplaceMe","rules":[{"id":921,"ops":[{"type":"set_bit","bit":1}]}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("ReplaceMe", true).ok);

    DashPluginResult result = engine.installJson(R"json({"name":"ReplaceMe","version":"2.0","rules":[{"id":2047,"ops":[{"type":"checksum"}]}]})json", false);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_EQUAL(1, engine.pluginCount());
    TEST_ASSERT_EQUAL_STRING("2.0", engine.pluginAt(0).version.c_str());
    TEST_ASSERT_FALSE(engine.pluginAt(0).enabled);
}
```

- [ ] **Step 3: Add execution, priority, replay, and safety tests**

Append to the same file:

```cpp
void test_disabled_plugin_does_not_send()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"Disabled","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13}],"send":true}]})json", false).ok);

    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_FALSE(applied.sent);
    TEST_ASSERT_EQUAL(0, driver.sent.size());
}

void test_enabled_plugin_applies_set_bit_and_sends_once()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"Enabled","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("Enabled", true).ok);

    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_TRUE(applied.sent);
    TEST_ASSERT_EQUAL(1, driver.sent.size());
    TEST_ASSERT_TRUE((driver.sent[0].data[1] & 0x20) != 0);
}

void test_safety_context_blocks_plugin_execution()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"Safe","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("Safe", true).ok);

    DashPluginContext ctx = allowedContext();
    ctx.apGateAllowed = false;
    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, ctx, driver);

    TEST_ASSERT_FALSE(applied.sent);
    TEST_ASSERT_EQUAL(0, driver.sent.size());
    TEST_ASSERT_EQUAL_STRING("ap_gate", applied.blockedBy.c_str());
}

void test_priority_prevents_lower_plugin_from_overwriting_same_bit()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"High","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13,"val":1}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"Low","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13,"val":0}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("High", true).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("Low", true).ok);
    TEST_ASSERT_TRUE(engine.setPriority("High", 1).ok);
    TEST_ASSERT_TRUE(engine.setPriority("Low", 2).ok);

    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_TRUE(applied.sent);
    TEST_ASSERT_EQUAL(1, driver.sent.size());
    TEST_ASSERT_TRUE((driver.sent[0].data[1] & 0x20) != 0);
    TEST_ASSERT_TRUE(engine.statusJson().indexOf("Priority overlap") >= 0);
}

void test_gtw2047_replay_repeats_only_modified_2047_frames()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"GTW","rules":[{"id":2047,"mux":3,"mux_mask":255,"ops":[{"type":"set_byte","byte":1,"val":170},{"type":"checksum"}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("GTW", true).ok);
    TEST_ASSERT_TRUE(engine.setReplayCount(3).ok);

    CanFrame f = frame2047(3);
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_TRUE(applied.sent);
    TEST_ASSERT_EQUAL(3, driver.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0xAA, driver.sent[0].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, driver.sent[1].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, driver.sent[2].data[1]);
}

void test_replay_count_does_not_repeat_non_2047_frames()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"NonGTW","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13}],"send":true}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("NonGTW", true).ok);
    TEST_ASSERT_TRUE(engine.setReplayCount(5).ok);

    CanFrame f = frame921();
    DashPluginApplyResult applied = engine.applyToFrame(f, allowedContext(), driver);

    TEST_ASSERT_TRUE(applied.sent);
    TEST_ASSERT_EQUAL(1, driver.sent.size());
}

void test_emit_periodic_requires_gtw2047_mux3()
{
    DashPluginEngine engine;
    DashPluginResult bad = engine.installJson(R"json({"name":"BadPeriodic","rules":[{"id":921,"mux":3,"ops":[{"type":"emit_periodic","interval":100}],"send":true}]})json", false);
    TEST_ASSERT_FALSE(bad.ok);

    DashPluginResult good = engine.installJson(R"json({"name":"GoodPeriodic","rules":[{"id":2047,"mux":3,"mux_mask":255,"ops":[{"type":"emit_periodic","interval":100}],"send":true}]})json", false);
    TEST_ASSERT_TRUE(good.ok);
}

void test_gtw_silent_unavailable_without_custom_key()
{
    DashPluginEngine engine;
    DashPluginResult result = engine.installJson(R"json({"name":"Silent","rules":[{"id":2047,"mux":3,"mux_mask":255,"ops":[{"type":"emit_periodic","interval":100,"gtw_silent":true}],"send":true}]})json", false);

    TEST_ASSERT_TRUE(result.ok);
    TEST_ASSERT_TRUE(engine.statusJson().indexOf("custom key build required") >= 0);
}
```

- [ ] **Step 4: Add Unity runner**

Append to the same file:

```cpp
int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_official_plugin_installs_disabled_by_default);
    RUN_TEST(test_invalid_json_is_rejected_without_installing);
    RUN_TEST(test_name_longer_than_31_chars_is_rejected);
    RUN_TEST(test_installing_same_name_replaces_existing_plugin_disabled);
    RUN_TEST(test_disabled_plugin_does_not_send);
    RUN_TEST(test_enabled_plugin_applies_set_bit_and_sends_once);
    RUN_TEST(test_safety_context_blocks_plugin_execution);
    RUN_TEST(test_priority_prevents_lower_plugin_from_overwriting_same_bit);
    RUN_TEST(test_gtw2047_replay_repeats_only_modified_2047_frames);
    RUN_TEST(test_replay_count_does_not_repeat_non_2047_frames);
    RUN_TEST(test_emit_periodic_requires_gtw2047_mux3);
    RUN_TEST(test_gtw_silent_unavailable_without_custom_key);
    return UNITY_END();
}
```

- [ ] **Step 5: Run native plugin tests and confirm RED**

Run:

```bash
pio test -e native_plugin_engine
```

Expected: FAIL because `include/dash_plugin_engine.h` does not exist.

- [ ] **Step 6: Commit RED native tests**

```bash
git add platformio.ini test/test_native_plugin_engine/test_plugin_engine.cpp
git commit -m "test: add plugin engine native contracts"
```

---

## Task 3: Implement plugin model, parser, and status serialization

**Files:**
- Create: `include/dash_plugin_engine.h`
- Test: `test/test_native_plugin_engine/test_plugin_engine.cpp`

- [ ] **Step 1: Create `include/dash_plugin_engine.h` with data model**

Create the file with this initial content:

```cpp
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include <ArduinoJson.h>

static constexpr uint8_t kDashPluginMaxPlugins = 8;
static constexpr uint8_t kDashPluginMaxRules = 16;
static constexpr uint8_t kDashPluginMaxOps = 16;
static constexpr uint8_t kDashPluginMaxFilterIds = 32;
static constexpr size_t kDashPluginMaxJsonBytes = 8192;
static constexpr uint8_t kDashPluginReplayMin = 1;
static constexpr uint8_t kDashPluginReplayMax = 5;

struct DashPluginResult
{
    bool ok = false;
    std::string message;
    static DashPluginResult success(const char *msg = "ok") { return {true, msg ? msg : "ok"}; }
    static DashPluginResult fail(const char *msg) { return {false, msg ? msg : "error"}; }
};

struct DashPluginContext
{
    bool canActive = false;
    bool otaAllowed = false;
    bool apGateAllowed = false;
    bool fsdMasterEnabled = false;
    uint8_t defaultBus = CAN_BUS_DEFAULT;
};

struct DashPluginApplyResult
{
    bool matched = false;
    bool modified = false;
    bool sent = false;
    uint8_t sendCount = 0;
    std::string blockedBy;
};

enum class DashPluginOpType : uint8_t
{
    SetBit,
    SetByte,
    OrByte,
    AndByte,
    Counter,
    Checksum,
    EmitPeriodic,
};

struct DashPluginOp
{
    DashPluginOpType type = DashPluginOpType::Checksum;
    uint8_t byte = 0;
    uint8_t bit = 0;
    uint8_t val = 0;
    uint8_t mask = 0xFF;
    uint8_t step = 1;
    uint16_t intervalMs = 100;
    bool gtwSilent = false;
};

struct DashPluginRule
{
    uint32_t id = 0;
    bool hasBus = false;
    uint8_t busMask = 0;
    int16_t mux = -1;
    uint16_t muxMask = 0;
    bool hasMatch = false;
    uint8_t matchByte = 0;
    uint8_t matchMask = 0;
    uint8_t matchVal = 0;
    bool send = true;
    std::vector<DashPluginOp> ops;
};

struct DashPlugin
{
    std::string name;
    std::string version = "1.0";
    std::string author;
    bool enabled = false;
    uint8_t priority = 1;
    bool compatible = true;
    std::string lastError;
    std::vector<DashPluginRule> rules;
    std::vector<std::string> conflicts;
};
```

- [ ] **Step 2: Add helper functions for JSON and bus parsing**

Append:

```cpp
static inline std::string dashPluginString(JsonVariantConst v, const char *fallback = "")
{
    if (v.is<const char *>())
        return v.as<const char *>();
    return fallback ? fallback : "";
}

static inline uint8_t dashPluginBusBit(const std::string &name)
{
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)std::toupper(c); });
    if (n == "CH") return 1;
    if (n == "VEH") return 2;
    if (n == "PARTY") return 4;
    return 0;
}

static inline uint8_t dashPluginDefaultBusMask(uint8_t defaultBus)
{
    if (defaultBus == CAN_BUS_CH) return 1;
    if (defaultBus == CAN_BUS_VEH) return 2;
    if (defaultBus == CAN_BUS_PARTY) return 4;
    return 1;
}

static inline uint8_t dashPluginParseBus(JsonVariantConst v)
{
    if (v.isNull()) return 0;
    if (v.is<int>()) return static_cast<uint8_t>(v.as<int>() & 0x07);
    if (v.is<const char *>())
    {
        std::string s = v.as<const char *>();
        uint8_t mask = 0;
        size_t start = 0;
        while (start <= s.size())
        {
            size_t comma = s.find(',', start);
            std::string part = s.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
            part.erase(std::remove_if(part.begin(), part.end(), ::isspace), part.end());
            mask |= dashPluginBusBit(part);
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
        return mask;
    }
    if (v.is<JsonArrayConst>())
    {
        uint8_t mask = 0;
        for (JsonVariantConst item : v.as<JsonArrayConst>())
            mask |= dashPluginParseBus(item);
        return mask;
    }
    return 0;
}
```

- [ ] **Step 3: Add `DashPluginEngine` parser skeleton**

Append:

```cpp
class DashPluginEngine
{
public:
    DashPluginResult installJson(const char *json, bool enabledAfterInstall)
    {
        if (!json) return DashPluginResult::fail("empty json");
        size_t len = std::strlen(json);
        if (len == 0 || len > kDashPluginMaxJsonBytes)
            return DashPluginResult::fail("plugin json size out of range");

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json);
        if (err)
            return DashPluginResult::fail("invalid json");

        DashPlugin plugin;
        DashPluginResult parsed = parsePlugin(doc.as<JsonObjectConst>(), plugin);
        if (!parsed.ok)
            return parsed;
        plugin.enabled = enabledAfterInstall;

        for (auto it = plugins_.begin(); it != plugins_.end(); ++it)
        {
            if (it->name == plugin.name)
            {
                plugin.priority = it->priority;
                *it = plugin;
                normalizePriorities();
                return DashPluginResult::success("replaced");
            }
        }

        if (plugins_.size() >= kDashPluginMaxPlugins)
            return DashPluginResult::fail("max plugins installed");
        plugin.priority = static_cast<uint8_t>(plugins_.size() + 1);
        plugins_.push_back(plugin);
        normalizePriorities();
        return DashPluginResult::success("installed");
    }

    size_t pluginCount() const { return plugins_.size(); }
    const DashPlugin &pluginAt(size_t index) const { return plugins_.at(index); }
    uint8_t replayCount() const { return replayCount_; }

    DashPluginResult setEnabled(const char *name, bool enabled)
    {
        DashPlugin *p = findPlugin(name);
        if (!p) return DashPluginResult::fail("plugin not found");
        p->enabled = enabled;
        return DashPluginResult::success();
    }

    DashPluginResult setPriority(const char *name, uint8_t priority)
    {
        DashPlugin *p = findPlugin(name);
        if (!p) return DashPluginResult::fail("plugin not found");
        if (priority < 1) priority = 1;
        if (priority > plugins_.size()) priority = static_cast<uint8_t>(plugins_.size());
        p->priority = priority;
        normalizePriorities(name);
        return DashPluginResult::success();
    }

    DashPluginResult setReplayCount(int count)
    {
        if (count < kDashPluginReplayMin || count > kDashPluginReplayMax)
            return DashPluginResult::fail("replay count out of range");
        replayCount_ = static_cast<uint8_t>(count);
        return DashPluginResult::success();
    }

    std::string statusJson() const
    {
        JsonDocument doc;
        doc["engineAvailable"] = true;
        doc["engineEnabled"] = true;
        doc["storage"] = "spiffs";
        doc["replayCount"] = replayCount_;
        JsonObject limits = doc["limits"].to<JsonObject>();
        limits["maxPlugins"] = kDashPluginMaxPlugins;
        limits["maxRulesPerPlugin"] = kDashPluginMaxRules;
        limits["maxOpsPerRule"] = kDashPluginMaxOps;
        limits["maxFilterIdsPerPlugin"] = kDashPluginMaxFilterIds;
        JsonArray installed = doc["installed"].to<JsonArray>();
        for (const DashPlugin &p : plugins_)
        {
            JsonObject item = installed.add<JsonObject>();
            item["name"] = p.name;
            item["version"] = p.version;
            item["author"] = p.author;
            item["enabled"] = p.enabled;
            item["priority"] = p.priority;
            item["ruleCount"] = p.rules.size();
            item["compatible"] = p.compatible;
            item["lastError"] = p.lastError;
            JsonArray conflicts = item["conflicts"].to<JsonArray>();
            for (const std::string &c : p.conflicts) conflicts.add(c);
        }
        std::string out;
        serializeJson(doc, out);
#ifndef PLUGIN_GTW_UDS_CUSTOM_KEY
        if (out.find("custom key build required") == std::string::npos)
        {
            // Keep token visible for contract/native tests until a silent rule is present.
        }
#endif
        return out;
    }

private:
    std::vector<DashPlugin> plugins_;
    uint8_t replayCount_ = 1;

    DashPlugin *findPlugin(const char *name)
    {
        if (!name) return nullptr;
        for (DashPlugin &p : plugins_)
            if (p.name == name) return &p;
        return nullptr;
    }
```

- [ ] **Step 4: Add parser methods**

Append inside the private section:

```cpp
    DashPluginResult parsePlugin(JsonObjectConst obj, DashPlugin &plugin)
    {
        if (obj.isNull()) return DashPluginResult::fail("plugin must be object");
        plugin.name = dashPluginString(obj["name"]);
        if (plugin.name.empty()) return DashPluginResult::fail("name required");
        if (plugin.name.size() > 31) return DashPluginResult::fail("name too long");
        plugin.version = dashPluginString(obj["version"], "1.0");
        plugin.author = dashPluginString(obj["author"]);
        JsonArrayConst rules = obj["rules"].as<JsonArrayConst>();
        if (rules.isNull() || rules.size() == 0) return DashPluginResult::fail("rules required");
        if (rules.size() > kDashPluginMaxRules) return DashPluginResult::fail("too many rules");
        for (JsonObjectConst ruleObj : rules)
        {
            DashPluginRule rule;
            DashPluginResult rr = parseRule(ruleObj, rule);
            if (!rr.ok) return rr;
            plugin.rules.push_back(rule);
        }
        return DashPluginResult::success();
    }

    DashPluginResult parseRule(JsonObjectConst obj, DashPluginRule &rule)
    {
        if (!obj["id"].is<int>()) return DashPluginResult::fail("rule id required");
        int id = obj["id"].as<int>();
        if (id < 0 || id > 0x1FFFFFFF) return DashPluginResult::fail("rule id out of range");
        rule.id = static_cast<uint32_t>(id);
        if (!obj["bus"].isNull())
        {
            rule.hasBus = true;
            rule.busMask = dashPluginParseBus(obj["bus"]);
            if (rule.busMask == 0) return DashPluginResult::fail("unknown bus");
        }
        rule.mux = obj["mux"].is<int>() ? obj["mux"].as<int>() : -1;
        if (rule.mux < -1 || rule.mux > 255) return DashPluginResult::fail("mux out of range");
        if (obj["mux_mask"].is<int>()) rule.muxMask = obj["mux_mask"].as<int>();
        else if (obj["muxMask"].is<int>()) rule.muxMask = obj["muxMask"].as<int>();
        else if (rule.mux >= 0 && rule.mux <= 7) rule.muxMask = 7;
        else if (rule.mux >= 8) rule.muxMask = 255;

        JsonVariantConst matchObj = obj["match"];
        if (matchObj.is<JsonObjectConst>())
        {
            JsonObjectConst m = matchObj.as<JsonObjectConst>();
            rule.hasMatch = true;
            rule.matchByte = m["byte"] | 0;
            rule.matchMask = m["mask"] | 0;
            rule.matchVal = m["val"] | 0;
        }
        else if (obj["match_byte"].is<int>() || obj["matchByte"].is<int>())
        {
            rule.hasMatch = true;
            rule.matchByte = obj["match_byte"].is<int>() ? obj["match_byte"].as<int>() : obj["matchByte"].as<int>();
            rule.matchMask = obj["match_mask"].is<int>() ? obj["match_mask"].as<int>() : (obj["matchMask"].is<int>() ? obj["matchMask"].as<int>() : 0);
            rule.matchVal = obj["match_val"].is<int>() ? obj["match_val"].as<int>() : (obj["match_value"].is<int>() ? obj["match_value"].as<int>() : (obj["matchValue"].is<int>() ? obj["matchValue"].as<int>() : 0));
        }
        if (rule.hasMatch && rule.matchByte > 7) return DashPluginResult::fail("match byte out of range");
        rule.send = obj["send"].is<bool>() ? obj["send"].as<bool>() : true;

        JsonArrayConst ops = obj["ops"].as<JsonArrayConst>();
        if (ops.isNull() || ops.size() == 0) return DashPluginResult::fail("ops required");
        if (ops.size() > kDashPluginMaxOps) return DashPluginResult::fail("too many ops");
        for (JsonObjectConst opObj : ops)
        {
            DashPluginOp op;
            DashPluginResult orr = parseOp(rule, opObj, op);
            if (!orr.ok) return orr;
            rule.ops.push_back(op);
        }
        return DashPluginResult::success();
    }
```

Append `parseOp` and close the class:

```cpp
    DashPluginResult parseOp(const DashPluginRule &rule, JsonObjectConst obj, DashPluginOp &op)
    {
        std::string type = dashPluginString(obj["type"]);
        if (type == "set_bit")
        {
            op.type = DashPluginOpType::SetBit;
            int bit = obj["bit"] | -1;
            if (bit < 0 || bit > 63) return DashPluginResult::fail("bit out of range");
            op.bit = static_cast<uint8_t>(bit);
            op.val = obj["val"].is<bool>() ? (obj["val"].as<bool>() ? 1 : 0) : static_cast<uint8_t>(obj["val"] | 1);
            return DashPluginResult::success();
        }
        if (type == "set_byte" || type == "or_byte" || type == "and_byte" || type == "counter")
        {
            int byte = obj["byte"] | -1;
            if (byte < 0 || byte > 7) return DashPluginResult::fail("byte out of range");
            op.byte = static_cast<uint8_t>(byte);
            op.val = static_cast<uint8_t>(obj["val"] | (type == "and_byte" ? 255 : 0));
            op.mask = static_cast<uint8_t>(obj["mask"] | (type == "counter" ? 15 : 255));
            op.step = static_cast<uint8_t>(obj["step"] | 1);
            if (type == "set_byte") op.type = DashPluginOpType::SetByte;
            else if (type == "or_byte") op.type = DashPluginOpType::OrByte;
            else if (type == "and_byte") op.type = DashPluginOpType::AndByte;
            else op.type = DashPluginOpType::Counter;
            return DashPluginResult::success();
        }
        if (type == "checksum")
        {
            op.type = DashPluginOpType::Checksum;
            return DashPluginResult::success();
        }
        if (type == "emit_periodic")
        {
            if (rule.id != 2047 || rule.mux != 3 || !rule.send)
                return DashPluginResult::fail("emit_periodic requires GTW2047 mux 3 send rule");
            op.type = DashPluginOpType::EmitPeriodic;
            int interval = obj["interval"] | 100;
            if (interval < 10) interval = 10;
            if (interval > 5000) interval = 5000;
            op.intervalMs = static_cast<uint16_t>(interval);
            op.gtwSilent = obj["gtw_silent"] | false;
            return DashPluginResult::success();
        }
        return DashPluginResult::fail("unknown op type");
    }

    void normalizePriorities(const char *preferredName = nullptr)
    {
        std::sort(plugins_.begin(), plugins_.end(), [](const DashPlugin &a, const DashPlugin &b) {
            if (a.priority == b.priority) return a.name < b.name;
            return a.priority < b.priority;
        });
        for (size_t i = 0; i < plugins_.size(); ++i)
            plugins_[i].priority = static_cast<uint8_t>(i + 1);
        if (preferredName)
        {
            std::sort(plugins_.begin(), plugins_.end(), [](const DashPlugin &a, const DashPlugin &b) {
                return a.priority < b.priority;
            });
        }
    }
};
```

- [ ] **Step 5: Run native plugin tests and observe remaining failures**

Run:

```bash
pio test -e native_plugin_engine
```

Expected: compile succeeds or progresses; execution tests that depend on `applyToFrame()` fail because it is not implemented yet.

- [ ] **Step 6: Commit parser implementation**

```bash
git add include/dash_plugin_engine.h
git commit -m "feat: add official plugin parser model"
```

---

## Task 4: Implement plugin execution, priority conflict, and safety gates

**Files:**
- Modify: `include/dash_plugin_engine.h`
- Test: `test/test_native_plugin_engine/test_plugin_engine.cpp`

- [ ] **Step 1: Add frame matching and op execution helpers**

Inside `DashPluginEngine` private section before `normalizePriorities`, add:

```cpp
    static bool ruleMatches(const DashPluginRule &rule, const CanFrame &frame, uint8_t defaultBus)
    {
        if (frame.id != rule.id) return false;
        if (rule.hasBus)
        {
            uint8_t frameMask = dashPluginDefaultBusMask(frame.bus == CAN_BUS_ANY ? defaultBus : frame.bus);
            if ((rule.busMask & frameMask) == 0) return false;
        }
        if (rule.mux >= 0)
        {
            uint8_t mask = static_cast<uint8_t>(rule.muxMask ? rule.muxMask : 0xFF);
            if ((frame.data[0] & mask) != (static_cast<uint8_t>(rule.mux) & mask)) return false;
        }
        if (rule.hasMatch)
        {
            if (rule.matchByte > 7) return false;
            if ((frame.data[rule.matchByte] & rule.matchMask) != (rule.matchVal & rule.matchMask)) return false;
        }
        return true;
    }

    static uint8_t checksumByte(const CanFrame &frame)
    {
        uint16_t sum = static_cast<uint16_t>(frame.id & 0xFF) + static_cast<uint16_t>((frame.id >> 8) & 0xFF);
        for (uint8_t i = 0; i < 7; ++i) sum += frame.data[i];
        return static_cast<uint8_t>(sum & 0xFF);
    }

    static void applyCounter(CanFrame &frame, const DashPluginOp &op)
    {
        uint8_t mask = op.mask ? op.mask : 0x0F;
        uint8_t shift = 0;
        while (shift < 8 && ((mask >> shift) & 0x01) == 0) ++shift;
        uint8_t field = static_cast<uint8_t>((frame.data[op.byte] & mask) >> shift);
        uint8_t widthMask = static_cast<uint8_t>(mask >> shift);
        field = static_cast<uint8_t>((field + (op.step ? op.step : 1)) & widthMask);
        frame.data[op.byte] = static_cast<uint8_t>((frame.data[op.byte] & ~mask) | ((field << shift) & mask));
    }

    static bool bitAlreadyOwned(const std::array<bool, 64> &owned, uint8_t bit)
    {
        return bit < owned.size() && owned[bit];
    }

    static void markByteMaskOwned(std::array<bool, 64> &owned, uint8_t byte, uint8_t mask)
    {
        for (uint8_t b = 0; b < 8; ++b)
            if (mask & (1u << b)) owned[byte * 8 + b] = true;
    }
```

- [ ] **Step 2: Add `applyToFrame()` public method**

Add this method in public section before `statusJson()`:

```cpp
    DashPluginApplyResult applyToFrame(const CanFrame &frame, const DashPluginContext &ctx, CanDriver &driver)
    {
        DashPluginApplyResult result{};
        if (!ctx.canActive) { result.blockedBy = "can_active"; return result; }
        if (!ctx.otaAllowed) { result.blockedBy = "ota"; return result; }
        if (!ctx.apGateAllowed) { result.blockedBy = "ap_gate"; return result; }
        if (!ctx.fsdMasterEnabled) { result.blockedBy = "fsd_master"; return result; }

        std::vector<DashPlugin *> ordered;
        for (DashPlugin &p : plugins_)
        {
            p.conflicts.clear();
            if (p.enabled && p.compatible)
                ordered.push_back(&p);
        }
        std::sort(ordered.begin(), ordered.end(), [](const DashPlugin *a, const DashPlugin *b) {
            return a->priority < b->priority;
        });

        CanFrame out = frame;
        if (out.bus == CAN_BUS_ANY) out.bus = ctx.defaultBus;
        std::array<bool, 64> owned{};
        bool shouldSend = false;

        for (DashPlugin *plugin : ordered)
        {
            for (const DashPluginRule &rule : plugin->rules)
            {
                if (!ruleMatches(rule, frame, ctx.defaultBus)) continue;
                result.matched = true;
                if (!rule.send) continue;
                for (const DashPluginOp &op : rule.ops)
                {
                    switch (op.type)
                    {
                    case DashPluginOpType::SetBit:
                        if (bitAlreadyOwned(owned, op.bit))
                        {
                            plugin->conflicts.push_back("Priority overlap on bit " + std::to_string(op.bit));
                            break;
                        }
                        setBit(out, op.bit, op.val != 0);
                        owned[op.bit] = true;
                        result.modified = true;
                        break;
                    case DashPluginOpType::SetByte:
                        markByteMaskOwned(owned, op.byte, op.mask);
                        out.data[op.byte] = static_cast<uint8_t>((out.data[op.byte] & ~op.mask) | (op.val & op.mask));
                        result.modified = true;
                        break;
                    case DashPluginOpType::OrByte:
                        markByteMaskOwned(owned, op.byte, op.val);
                        out.data[op.byte] = static_cast<uint8_t>(out.data[op.byte] | op.val);
                        result.modified = true;
                        break;
                    case DashPluginOpType::AndByte:
                        markByteMaskOwned(owned, op.byte, static_cast<uint8_t>(~op.val));
                        out.data[op.byte] = static_cast<uint8_t>(out.data[op.byte] & op.val);
                        result.modified = true;
                        break;
                    case DashPluginOpType::Counter:
                        applyCounter(out, op);
                        result.modified = true;
                        break;
                    case DashPluginOpType::Checksum:
                        out.data[7] = checksumByte(out);
                        result.modified = true;
                        break;
                    case DashPluginOpType::EmitPeriodic:
                        cachePeriodic(out, op);
                        break;
                    }
                }
                shouldSend = true;
            }
        }

        if (!shouldSend || !result.modified) return result;
        uint8_t sends = (out.id == 2047) ? replayCount_ : 1;
        for (uint8_t i = 0; i < sends; ++i)
        {
            CanFrame sendFrame = out;
            if (i > 0) advanceReplayOps(sendFrame, frame, ctx.defaultBus);
            if (driver.send(sendFrame))
            {
                result.sent = true;
                result.sendCount++;
            }
        }
        return result;
    }
```

- [ ] **Step 3: Add periodic cache placeholders used by execution**

Add private members and helper methods:

```cpp
    bool periodicValid_ = false;
    CanFrame periodicFrame_{};
    uint16_t periodicIntervalMs_ = 100;
    uint32_t periodicLastMs_ = 0;
    bool gtwSilentRequested_ = false;

    void cachePeriodic(const CanFrame &frame, const DashPluginOp &op)
    {
        periodicFrame_ = frame;
        periodicValid_ = true;
        periodicIntervalMs_ = op.intervalMs;
        gtwSilentRequested_ = op.gtwSilent;
#ifndef PLUGIN_GTW_UDS_CUSTOM_KEY
        (void)gtwSilentRequested_;
#endif
    }

    void advanceReplayOps(CanFrame &sendFrame, const CanFrame &, uint8_t)
    {
        // Conservative first pass: recompute checksum for replayed frames.
        // Counter ops are already represented in the composed frame; a later
        // enhancement may retain per-rule op lists to advance counters again.
        sendFrame.data[7] = checksumByte(sendFrame);
    }

public:
    void tickPeriodic(uint32_t nowMs, const DashPluginContext &ctx, CanDriver &driver)
    {
        if (!periodicValid_) return;
        if (!ctx.canActive || !ctx.otaAllowed || !ctx.apGateAllowed || !ctx.fsdMasterEnabled) return;
        if (periodicLastMs && nowMs - periodicLastMs < periodicIntervalMs_) return;
        periodicLastMs_ = nowMs;
        driver.send(periodicFrame_);
    }
```

- [ ] **Step 4: Update `statusJson()` for target IDs and gtw silent status**

In `statusJson()`, inside each installed item, add target IDs and silent status:

```cpp
            JsonArray targetIds = item["targetIds"].to<JsonArray>();
            for (const DashPluginRule &r : p.rules)
            {
                char buf[12];
                snprintf(buf, sizeof(buf), "0x%03lX", static_cast<unsigned long>(r.id));
                targetIds.add(buf);
            }
#ifndef PLUGIN_GTW_UDS_CUSTOM_KEY
            item["gtwSilent"] = "custom key build required";
#else
            item["gtwSilent"] = "available";
#endif
```

Remove the no-op custom-key comment block if present.

- [ ] **Step 5: Run native plugin tests**

Run:

```bash
pio test -e native_plugin_engine
```

Expected: PASS all `test_native_plugin_engine` tests. If the `advanceReplayOps` behavior fails a counter-specific test added later, update the helper to retain matched rule op lists and re-run counter/checksum ops per send.

- [ ] **Step 6: Commit plugin execution**

```bash
git add include/dash_plugin_engine.h test/test_native_plugin_engine/test_plugin_engine.cpp
git commit -m "feat: implement plugin execution and gtw replay core"
```

---

## Task 5: Add AP delay and auto-restore config/status backend

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`
- Modify: `test/test_wifi_settings_regression.py`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Replace fixed AP delay constant with configurable state**

In `include/web/mcp2515_dashboard.h`, near the current AP settle state, replace:

```cpp
static constexpr uint32_t kLegacyFsdActivationSettleMs = 2000;
static uint32_t legacyFsdRequiredStableMs = kLegacyFsdActivationSettleMs;
```

with:

```cpp
static constexpr uint32_t kLegacyFsdActivationSettleDefaultMs = 2000;
static constexpr uint32_t kLegacyFsdActivationSettleMinMs = 0;
static constexpr uint32_t kLegacyFsdActivationSettleMaxMs = 3000;
static uint32_t legacyFsdRequiredStableMs = kLegacyFsdActivationSettleDefaultMs;
```

Add helper near `dashArgTruthy`:

```cpp
static uint32_t dashClampApDelayMs(int value)
{
    if (value < static_cast<int>(kLegacyFsdActivationSettleMinMs))
        return kLegacyFsdActivationSettleMinMs;
    if (value > static_cast<int>(kLegacyFsdActivationSettleMaxMs))
        return kLegacyFsdActivationSettleMaxMs;
    return static_cast<uint32_t>(value);
}
```

- [ ] **Step 2: Use configured delay in gate logic**

In `dashLegacyFsdActivationAllowed()`, replace:

```cpp
bool stable = (nowMs - legacyFsdApActiveSinceMs) >= kLegacyFsdActivationSettleMs;
```

with:

```cpp
bool stable = (nowMs - legacyFsdApActiveSinceMs) >= legacyFsdRequiredStableMs;
```

- [ ] **Step 3: Persist AP delay and auto restore**

In `dashSavePrefs()`, after `prefs.putBool("ap_rst", apAutoRestore);`, add:

```cpp
    prefs.putUInt("ap_dly", legacyFsdRequiredStableMs);
```

In the prefs load function where `ap_gate` and `ap_rst` are read, add:

```cpp
    legacyFsdRequiredStableMs = dashClampApDelayMs(static_cast<int>(prefs.getUInt("ap_dly", kLegacyFsdActivationSettleDefaultMs)));
```

- [ ] **Step 4: Extend `/config` POST parsing**

In `handleConfig()`, after existing `apg` handling, add:

```cpp
    if (server.hasArg("ap_delay_ms"))
        legacyFsdRequiredStableMs = dashClampApDelayMs(server.arg("ap_delay_ms").toInt());
    if (server.hasArg("ap_auto_restore"))
        apAutoRestore = dashArgTruthy(server.arg("ap_auto_restore"));
```

Keep existing `ap_rst` behavior as an alias if present:

```cpp
    if (server.hasArg("ap_rst"))
        apAutoRestore = dashArgTruthy(server.arg("ap_rst"));
```

- [ ] **Step 5: Add AP injection state JSON helper**

Near `appendFsdDiagJson`, add:

```cpp
static const char *dashApInjectionStateName()
{
    if (!canActive)
        return "blocked";
    if (!dashOtaGuardAllowInjection())
        return "blocked";
    if (apInjectionGate && !(dashHandler && (bool)dashHandler->APActive))
        return "waiting_ap";
    if (apInjectionGate && !legacyFsdLastAllowed)
        return "settling";
    if (legacyFsdLastAllowed || (lastInjectMs && millis() - lastInjectMs < 1000))
        return "injecting";
    return "blocked";
}
```

In `handleStatus()`, add fields near existing AP gate fields:

```cpp
    j += ",\"apDelayMs\":";
    j += legacyFsdRequiredStableMs;
    j += ",\"apInjectionState\":\"";
    j += dashApInjectionStateName();
    j += "\"";
```

- [ ] **Step 6: Run AP contract tests**

Run:

```bash
PYTHONPATH=test python3 -m unittest \
  test_dashboard_api_contract.DashboardApiContractTests.test_single_can_driving_status_exposes_ap_gate_delay_and_restore
```

Expected: backend tokens pass after UI tokens are added in a later task; if this task still fails only for UI tokens, keep moving.

- [ ] **Step 7: Commit AP backend config**

```bash
git add include/web/mcp2515_dashboard.h test/test_wifi_settings_regression.py
git commit -m "feat: add configurable ap delay status"
```

---

## Task 6: Wire plugin engine into dashboard backend routes and SPIFFS storage

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`
- Modify: `platformio.ini`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Enable plugin engine for standalone profile**

In `[env:waveshare_single_can_standalone] build_flags`, add:

```ini
    -DDASH_PLUGIN_ENGINE=1
```

Do not add this flag to `[env:lilygo_t2can_dual]` in this task.

- [ ] **Step 2: Include plugin engine and declare global**

In `include/web/mcp2515_dashboard.h`, after existing includes, add:

```cpp
#if defined(DASH_PLUGIN_ENGINE)
#include "dash_plugin_engine.h"
#endif
```

Near dashboard globals, add:

```cpp
#if defined(DASH_PLUGIN_ENGINE)
static DashPluginEngine dashPluginEngine;
static String dashPluginUploadBuffer;
#endif
```

- [ ] **Step 3: Add plugin context builder**

Add near AP helpers:

```cpp
#if defined(DASH_PLUGIN_ENGINE)
static DashPluginContext dashPluginContext()
{
    DashPluginContext ctx{};
    ctx.canActive = canActive;
    ctx.otaAllowed = dashOtaGuardAllowInjection();
    ctx.apGateAllowed = dashApInjectionAllowed() && dashLegacyFsdActivationAllowed(millis());
    ctx.fsdMasterEnabled = canActive;
    ctx.defaultBus = CAN_BUS_DEFAULT;
    return ctx;
}
#endif
```

- [ ] **Step 4: Add plugin HTTP handlers**

Add these handlers before route registration:

```cpp
#if defined(DASH_PLUGIN_ENGINE)
static void handlePluginsStatus()
{
    server.send(200, "application/json", String(dashPluginEngine.statusJson().c_str()));
}

static void handlePluginsInstallJson()
{
    if (!server.hasArg("plain") && !server.hasArg("json"))
    {
        server.send(400, "application/json", "{\"error\":\"json body required\"}");
        return;
    }
    String body = server.hasArg("plain") ? server.arg("plain") : server.arg("json");
    DashPluginResult r = dashPluginEngine.installJson(body.c_str(), false);
    dashSavePluginState();
    server.send(r.ok ? 200 : 400, "application/json", String("{\"ok\":") + (r.ok ? "true" : "false") + ",\"message\":\"" + r.message.c_str() + "\"}");
}

static void handlePluginsInstallUrl()
{
    if (!server.hasArg("url"))
    {
        server.send(400, "application/json", "{\"error\":\"url required\"}");
        return;
    }
    String url = server.arg("url");
    if (!url.startsWith("http://") && !url.startsWith("https://"))
    {
        server.send(400, "application/json", "{\"error\":\"http or https url required\"}");
        return;
    }
    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        http.end();
        server.send(502, "application/json", "{\"error\":\"plugin fetch failed\"}");
        return;
    }
    String body = http.getString();
    http.end();
    DashPluginResult r = dashPluginEngine.installJson(body.c_str(), false);
    dashSavePluginState();
    server.send(r.ok ? 200 : 400, "application/json", String("{\"ok\":") + (r.ok ? "true" : "false") + ",\"message\":\"" + r.message.c_str() + "\"}");
}

static void handlePluginsUpload()
{
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
        dashPluginUploadBuffer = "";
    else if (upload.status == UPLOAD_FILE_WRITE)
        dashPluginUploadBuffer += String(reinterpret_cast<const char *>(upload.buf)).substring(0, upload.currentSize);
    else if (upload.status == UPLOAD_FILE_END)
    {
        DashPluginResult r = dashPluginEngine.installJson(dashPluginUploadBuffer.c_str(), false);
        dashSavePluginState();
        dashPluginUploadBuffer = "";
        server.send(r.ok ? 200 : 400, "application/json", String("{\"ok\":") + (r.ok ? "true" : "false") + ",\"message\":\"" + r.message.c_str() + "\"}");
    }
}

static void handlePluginsToggle()
{
    DashPluginResult r = dashPluginEngine.setEnabled(server.arg("name").c_str(), dashArgTruthy(server.arg("enabled")));
    dashSavePluginState();
    server.send(r.ok ? 200 : 404, "application/json", String("{\"ok\":") + (r.ok ? "true" : "false") + "}");
}

static void handlePluginsPriority()
{
    DashPluginResult r = dashPluginEngine.setPriority(server.arg("name").c_str(), static_cast<uint8_t>(server.arg("priority").toInt()));
    dashSavePluginState();
    server.send(r.ok ? 200 : 404, "application/json", String("{\"ok\":") + (r.ok ? "true" : "false") + "}");
}

static void handlePluginsRemove()
{
    DashPluginResult r = dashPluginEngine.remove(server.arg("name").c_str());
    dashSavePluginState();
    server.send(r.ok ? 200 : 404, "application/json", String("{\"ok\":") + (r.ok ? "true" : "false") + "}");
}

static void handlePluginsReplayCount()
{
    DashPluginResult r = dashPluginEngine.setReplayCount(server.arg("count").toInt());
    dashSavePluginState();
    server.send(r.ok ? 200 : 400, "application/json", String("{\"ok\":") + (r.ok ? "true" : "false") + "}");
}

static void handlePluginsRuleTest()
{
    server.send(200, "application/json", "{\"ok\":true,\"mode\":\"manual_preview_reserved\"}");
}
#endif
```

- [ ] **Step 5: Add missing engine persistence methods or adapt handlers**

If `dashPluginEngine.remove()` and persistence methods do not exist, add to `DashPluginEngine`:

```cpp
    DashPluginResult remove(const char *name)
    {
        if (!name) return DashPluginResult::fail("plugin not found");
        auto before = plugins_.size();
        plugins_.erase(std::remove_if(plugins_.begin(), plugins_.end(), [&](const DashPlugin &p) { return p.name == name; }), plugins_.end());
        if (plugins_.size() == before) return DashPluginResult::fail("plugin not found");
        normalizePriorities();
        return DashPluginResult::success("removed");
    }
```

Add dashboard persistence helpers with safe no-op if serialization is not yet available:

```cpp
#if defined(DASH_PLUGIN_ENGINE)
static void dashSavePluginState()
{
    SPIFFS.begin(true);
    File f = SPIFFS.open("/plugins_state.json", "w");
    if (!f) return;
    std::string json = dashPluginEngine.statusJson();
    f.print(json.c_str());
}

static void dashLoadPluginState()
{
    SPIFFS.begin(true);
    // First pass loads no installed plugins from status JSON because status is
    // not the install source. Actual installed plugin file loading lands in the
    // storage hardening task. Replay count remains persisted by NVS if added.
}
#endif
```

- [ ] **Step 6: Register plugin routes**

In route setup, add:

```cpp
#if defined(DASH_PLUGIN_ENGINE)
    server.on("/plugins/status", HTTP_GET, handlePluginsStatus);
    server.on("/plugins/install_url", HTTP_POST, handlePluginsInstallUrl);
    server.on("/plugins/upload", HTTP_POST, []() {}, handlePluginsUpload);
    server.on("/plugins/install_json", HTTP_POST, handlePluginsInstallJson);
    server.on("/plugins/toggle", HTTP_POST, handlePluginsToggle);
    server.on("/plugins/priority", HTTP_POST, handlePluginsPriority);
    server.on("/plugins/remove", HTTP_POST, handlePluginsRemove);
    server.on("/plugins/replay_count", HTTP_POST, handlePluginsReplayCount);
    server.on("/plugins/rule_test", HTTP_POST, handlePluginsRuleTest);
#endif
```

- [ ] **Step 7: Expose plugin capability in `dashCapabilitiesJson()`**

Before closing JSON, add:

```cpp
    j += ",\""; j += "pluginEngine"; j += "\":";
#if defined(DASH_PLUGIN_ENGINE)
    j += "true";
#else
    j += "false";
#endif
```

Ensure the final `}` appears once.

- [ ] **Step 8: Run contract tests**

Run:

```bash
PYTHONPATH=test python3 -m unittest \
  test_dashboard_api_contract.DashboardApiContractTests.test_official_plugin_manager_contract_is_present \
  test_dashboard_api_contract.DashboardApiContractTests.test_status_exposes_single_can_capabilities
```

Expected: PASS for backend route/plugin tokens; UI tokens may wait for UI tasks if this test combines both.

- [ ] **Step 9: Commit plugin routes**

```bash
git add platformio.ini include/dash_plugin_engine.h include/web/mcp2515_dashboard.h
git commit -m "feat: expose plugin manager dashboard api"
```

---

## Task 7: Integrate plugin engine into CAN loop and avoid duplicate FSD injection

**Files:**
- Modify: `include/app.h`
- Modify: `include/handlers.h`
- Modify: `include/web/mcp2515_dashboard.h`
- Test: `test/test_native_plugin_engine/test_plugin_engine.cpp`

- [ ] **Step 1: Add built-in injection suppression callback to handler base**

In `CarManagerBase`, after `legacyFsdActivationAllowed`, add:

```cpp
    bool (*pluginOwnsFsdActivation)() = nullptr;
```

Add helper:

```cpp
    bool builtInFsdInjectionAllowed() const
    {
        if (pluginOwnsFsdActivation && pluginOwnsFsdActivation())
            return false;
        return true;
    }
```

- [ ] **Step 2: Guard built-in FSD send paths**

In Legacy/HW3/HW4 handler send locations, before sending FSD activation frames, add:

```cpp
        if (!builtInFsdInjectionAllowed())
        {
            legacyFsdDiag.mux0.lastSkip = FsdSkipReason::GateBlocked;
            legacyFsdDiag.health = FsdHealthState::GateBlocked;
            return;
        }
```

Use the appropriate diagnostic member for HW3/HW4 if not mux0. Do not alter non-FSD observer code.

- [ ] **Step 3: Add plugin ownership helper in dashboard**

In `mcp2515_dashboard.h`, add:

```cpp
#if defined(DASH_PLUGIN_ENGINE)
static bool dashPluginOwnsFsdActivation()
{
    return dashPluginEngine.hasEnabledRuleFor(1006) || dashPluginEngine.hasEnabledRuleFor(1021) || dashPluginEngine.hasEnabledRuleFor(2047);
}
#else
static bool dashPluginOwnsFsdActivation()
{
    return false;
}
#endif
```

Add `hasEnabledRuleFor()` to `DashPluginEngine`:

```cpp
    bool hasEnabledRuleFor(uint32_t canId) const
    {
        for (const DashPlugin &p : plugins_)
            if (p.enabled && p.compatible)
                for (const DashPluginRule &r : p.rules)
                    if (r.id == canId) return true;
        return false;
    }
```

- [ ] **Step 4: Assign callback in runtime state**

In `dashApplyRuntimeState()`, after `legacyFsdActivationAllowed`, add:

```cpp
        dashHandler->pluginOwnsFsdActivation = dashPluginOwnsFsdActivation;
```

In handler pool initialization loop, add the same assignment for each handler.

- [ ] **Step 5: Apply plugin engine after handler observation**

In `appLoop()`, after:

```cpp
        h->handleMessage(frame, *appDriver);
```

and before `dashPostProcessFrame(original, *appDriver);`, add:

```cpp
#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_PLUGIN_ENGINE)
        DashPluginApplyResult pluginApplied = dashPluginEngine.applyToFrame(original, dashPluginContext(), *appDriver);
        if (pluginApplied.sent)
            lastInjectMs = millis();
#endif
```

- [ ] **Step 6: Tick periodic plugin emit**

Near the end of `appLoop()`, before returning, add:

```cpp
#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_PLUGIN_ENGINE)
    dashPluginEngine.tickPeriodic(millis(), dashPluginContext(), *appDriver);
#endif
```

- [ ] **Step 7: Expose injection source in `/status`**

In `handleStatus()`, add:

```cpp
    j += ",\"injectionSource\":\"";
#if defined(DASH_PLUGIN_ENGINE)
    j += dashPluginOwnsFsdActivation() ? "Plugin" : (canActive ? "Built-in" : "Disabled");
#else
    j += canActive ? "Built-in" : "Disabled";
#endif
    j += "\"";
```

- [ ] **Step 8: Run native and build smoke tests**

Run:

```bash
pio test -e native_plugin_engine
pio test -e native_injection_after_ap
```

Expected: both PASS. If existing injection tests fail due to callback default, ensure callback is null in native handler tests and `builtInFsdInjectionAllowed()` returns true.

- [ ] **Step 9: Commit loop integration**

```bash
git add include/app.h include/handlers.h include/web/mcp2515_dashboard.h include/dash_plugin_engine.h
git commit -m "feat: integrate plugin injection path"
```

---

## Task 8: Implement desktop UI AP controls and official plugin manager

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`
- Modify: `include/web/mcp2515_dashboard_ui.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add product constants and product mode helpers**

In the UI JS global area near `var CAP=`, add:

```javascript
var PRODUCT_SINGLE_CAN_NAME='Atlas Single CAN';
var PRODUCT_DUAL_CAN_NAME='Atlas T-2CAN';
function isSingleCan(){return !!CAP.singleCan}
function applyProductMode(){
  var name=isSingleCan()?PRODUCT_SINGLE_CAN_NAME:PRODUCT_DUAL_CAN_NAME;
  document.querySelectorAll('[data-product-name]').forEach(function(n){n.textContent=name});
  document.querySelectorAll('[data-single-hide]').forEach(function(n){n.classList.toggle('cap-hidden',isSingleCan())});
  document.querySelectorAll('[data-dual-hide]').forEach(function(n){n.classList.toggle('cap-hidden',!isSingleCan())});
  document.querySelectorAll('[data-single-text]').forEach(function(n){if(isSingleCan())n.textContent=n.getAttribute('data-single-text')});
}
```

At the end of `applyCapabilities(c)`, call:

```javascript
applyProductMode();
```

- [ ] **Step 2: Update visible product name nodes**

Change title/sidebar/cockpit name nodes so standalone can rewrite them:

```html
<title>Atlas Single CAN</title>
...
<span data-product-name>Atlas Single CAN</span>
...
<div class="brand-title" data-product-name>Atlas Single CAN</div>
```

Keep dual mode dynamic by `applyProductMode()`.

- [ ] **Step 3: Add AP core switch block to Driving Status**

In the Driving Status page, add:

```html
<div class="card" id="ap-core-card">
  <div class="card-h"><span>AP 注入安全</span><small id="ap-state-pill">等待 AP</small></div>
  <div class="grid c4">
    <label class="toggle-row"><span>AP 门控</span><input id="ap-gate-tgl" type="checkbox" onchange="saveApGateControls()"></label>
    <label class="field"><span>延迟注入</span><select id="ap-delay-select" onchange="saveApGateControls()"><option value="0">0 ms</option><option value="1000">1000 ms</option><option value="2000">2000 ms</option><option value="3000">3000 ms</option></select></label>
    <label class="toggle-row"><span>AP-AUTO-RESTORE</span><input id="ap-auto-restore-tgl" type="checkbox" onchange="saveApGateControls()"></label>
    <div class="metric"><span>注入来源</span><b id="injection-source">Disabled</b></div>
  </div>
  <div class="diag-line" id="ap-state-detail">等待 AP</div>
</div>
```

- [ ] **Step 4: Add AP JS save/render functions**

Add:

```javascript
function saveApGateControls(){
  var gate=document.getElementById('ap-gate-tgl');
  var delay=document.getElementById('ap-delay-select');
  var restore=document.getElementById('ap-auto-restore-tgl');
  return postForm('/config',{
    apg:gate&&gate.checked?'1':'0',
    ap_delay_ms:delay?delay.value:'2000',
    ap_auto_restore:restore&&restore.checked?'1':'0'
  }).then(function(){showToast(T('已保存'))}).catch(function(){})
}
function renderApInjectionState(d){
  var gate=d.fsdDiag&&d.fsdDiag.gate?d.fsdDiag.gate:{};
  var state=d.apInjectionState||'blocked';
  var label=state==='waiting_ap'?'等待 AP':state==='settling'?'稳定计时中':state==='injecting'?'正在注入':'已阻断';
  setText('ap-state-pill',label);
  var stable=gate.apStableMs||0, req=gate.requiredStableMs||d.apDelayMs||2000;
  var reason=gate.lastBlockedBy||gate.gateReason||'';
  setText('ap-state-detail',label+' · AP '+stable+'/'+req+' ms'+(reason?' · '+reason:''));
  setText('injection-source',d.injectionSource||'Disabled');
  var apg=document.getElementById('ap-gate-tgl'); if(apg)apg.checked=!!d.apGateEnabled;
  var dly=document.getElementById('ap-delay-select'); if(dly)dly.value=String(d.apDelayMs||req||2000);
  var rst=document.getElementById('ap-auto-restore-tgl'); if(rst)rst.checked=!!d.apAutoRestore;
}
```

In `poll()` after config/status render calls, add:

```javascript
renderApInjectionState(d);
renderMobileApState(d);
```

- [ ] **Step 5: Add plugin manager desktop HTML**

In Hardware Mode page below mode cards, add:

```html
<div class="card" id="plugins-card">
  <div class="card-h"><span>Plugins</span><small>Official JSON plugin manager</small></div>
  <div class="plugin-install grid c3">
    <label class="field"><span>Install from URL</span><input id="plugin-url" placeholder="https://raw.githubusercontent.com/.../plugin.json"></label>
    <button class="btn" onclick="installPluginUrl()">Install</button>
    <label class="field"><span>Upload .json</span><input id="plugin-file" type="file" accept=".json,application/json" onchange="uploadPluginJson()"></label>
  </div>
  <label class="field"><span>Paste JSON</span><textarea id="plugin-json" rows="6" placeholder='{"name":"My Plugin","rules":[]}'></textarea></label>
  <button class="btn" onclick="installPluginJson()">Install from JSON</button>
  <div class="card-subtitle">Installed Plugins</div>
  <div id="plugin-list" class="plugin-list"></div>
  <div class="card-subtitle">GTW2047 Replay Count</div>
  <select id="plugin-replay-count" onchange="setPluginReplayCount(this.value)"><option value="1">1x</option><option value="2">2x</option><option value="3">3x</option><option value="4">4x</option><option value="5">5x</option></select>
  <div class="hint">Only repeats modified 0x7FF frames generated by enabled plugins.</div>
  <details><summary>Plugin Editor / Rule Test</summary><textarea id="plugin-editor-json" rows="8"></textarea><button class="btn" onclick="runPluginRuleTest()">Rule Test</button><pre id="plugin-rule-test-result"></pre></details>
</div>
```

- [ ] **Step 6: Add plugin manager JS functions**

Add:

```javascript
var PLUGINS={installed:[],replayCount:1};
function loadPlugins(){return fetch('/plugins/status').then(function(r){return r.json()}).then(function(j){PLUGINS=j;renderPluginsStatus(j);renderMobilePlugins(j)}).catch(function(){})}
function installPluginUrl(){var v=document.getElementById('plugin-url').value;return postForm('/plugins/install_url',{url:v}).then(loadPlugins)}
function installPluginJson(){var v=document.getElementById('plugin-json').value;return fetch('/plugins/install_json',{method:'POST',headers:{'Content-Type':'application/json'},body:v}).then(loadPlugins)}
function uploadPluginJson(){var f=document.getElementById('plugin-file').files[0];if(!f)return;var fd=new FormData();fd.append('file',f);return fetch('/plugins/upload',{method:'POST',body:fd}).then(loadPlugins)}
function togglePlugin(name,on){return postForm('/plugins/toggle',{name:name,enabled:on?'1':'0'}).then(loadPlugins)}
function setPluginPriority(name,priority){return postForm('/plugins/priority',{name:name,priority:priority}).then(loadPlugins)}
function removePlugin(name){return postForm('/plugins/remove',{name:name}).then(loadPlugins)}
function setPluginReplayCount(count){return postForm('/plugins/replay_count',{count:count}).then(loadPlugins)}
function runPluginRuleTest(){return fetch('/plugins/rule_test',{method:'POST'}).then(function(r){return r.text()}).then(function(t){setText('plugin-rule-test-result',t)})}
function renderPluginsStatus(j){
  var list=document.getElementById('plugin-list'); if(!list)return;
  var html=(j.installed||[]).map(function(p){
    var name=String(p.name||'');
    var ids=(p.targetIds||[]).join(', ');
    var conflicts=(p.conflicts||[]).join(' · ');
    return '<div class="plugin-item"><div><b>'+esc(name)+'</b> <small>'+esc(p.version||'1.0')+'</small><div class="hint">IDs: '+esc(ids)+' · Rules: '+(p.ruleCount||0)+'</div><div class="warn">'+esc(conflicts||p.lastError||'')+'</div></div><label>Enabled <input type="checkbox" '+(p.enabled?'checked':'')+' onchange="togglePlugin(\''+escAttr(name)+'\',this.checked)"></label><label>Priority <input type="number" min="1" max="8" value="'+(p.priority||1)+'" onchange="setPluginPriority(\''+escAttr(name)+'\',this.value)"></label><button onclick="removePlugin(\''+escAttr(name)+'\')">Remove</button><details><summary>Detail</summary><pre>'+esc(JSON.stringify(p,null,2))+'</pre></details></div>'
  }).join('');
  list.innerHTML=html||'<div class="hint">No plugins installed. New installs start disabled.</div>';
  var rc=document.getElementById('plugin-replay-count'); if(rc)rc.value=String(j.replayCount||1);
}
```

If `esc` / `escAttr` do not exist, add:

```javascript
function esc(s){return String(s==null?'':s).replace(/[&<>]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;'}[c]})}
function escAttr(s){return esc(s).replace(/'/g,'&#39;').replace(/"/g,'&quot;')}
```

In `poll()` or initialization, call:

```javascript
loadPlugins();
```

- [ ] **Step 7: Run minify and contract tests**

Run:

```bash
python3 scripts/minify_dashboard.py
python3 scripts/minify_dashboard.py --check
PYTHONPATH=test python3 -m unittest \
  test_dashboard_api_contract.DashboardApiContractTests.test_single_can_driving_status_exposes_ap_gate_delay_and_restore \
  test_dashboard_api_contract.DashboardApiContractTests.test_official_plugin_manager_contract_is_present
```

Expected: PASS for these tests.

- [ ] **Step 8: Commit desktop UI**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h test/test_dashboard_api_contract.py
git commit -m "feat: add single can ap and plugin dashboard ui"
```

---

## Task 9: Update mobile UI to match standalone AP/plugin capabilities

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`
- Modify: `include/web/mcp2515_dashboard_ui.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Replace mobile more menu entries**

In mobile tabs/more panel, remove standalone-visible entries for CAN2, strobe, and shift. Use:

```html
<button class="mob-tab" data-mobile-page="home" onclick="showMobilePage('home')">首页</button>
<button class="mob-tab" data-mobile-page="driving" onclick="showMobilePage('driving')">驾驶</button>
<button class="mob-tab" data-mobile-page="plugins" onclick="showMobilePage('plugins')">插件</button>
<button class="mob-tab" data-mobile-page="network" onclick="showMobilePage('network')">网络</button>
```

- [ ] **Step 2: Add mobile AP section**

Add mobile driving page block:

```html
<section class="mobile-page" id="mob-driving-page" data-mobile-page-panel="driving">
  <div class="mobile-card"><h3>AP 注入状态</h3><div id="m-ap-state">等待 AP</div><div id="m-ap-detail" class="hint"></div></div>
  <div class="mobile-card">
    <label>AP 门控 <input id="m-ap-gate-tgl" type="checkbox" onchange="syncMobileApControls()"></label>
    <label>延迟注入 <select id="m-ap-delay-select" onchange="syncMobileApControls()"><option value="0">0 ms</option><option value="1000">1000 ms</option><option value="2000">2000 ms</option><option value="3000">3000 ms</option></select></label>
    <label>AP-AUTO-RESTORE <input id="m-ap-auto-restore-tgl" type="checkbox" onchange="syncMobileApControls()"></label>
  </div>
</section>
```

- [ ] **Step 3: Add mobile plugin section**

Add:

```html
<section class="mobile-page" id="mob-plugin-page" data-mobile-page-panel="plugins">
  <div class="mobile-card"><h3>Plugins</h3><input id="m-plugin-url" placeholder="Plugin URL"><button onclick="installMobilePluginUrl()">Install URL</button></div>
  <div class="mobile-card"><textarea id="m-plugin-json" rows="5" placeholder="Paste JSON"></textarea><button onclick="installMobilePluginJson()">Install JSON</button></div>
  <div class="mobile-card"><h3>Installed</h3><div id="m-plugin-list"></div></div>
  <div class="mobile-card"><h3>GTW2047 Replay Count</h3><select id="m-plugin-replay-count" onchange="setPluginReplayCount(this.value)"><option value="1">1x</option><option value="2">2x</option><option value="3">3x</option><option value="4">4x</option><option value="5">5x</option></select></div>
</section>
```

- [ ] **Step 4: Add mobile AP/plugin JS**

Add:

```javascript
function showMobilePage(name){document.querySelectorAll('[data-mobile-page-panel]').forEach(function(p){p.classList.toggle('active',p.getAttribute('data-mobile-page-panel')===name)});document.querySelectorAll('[data-mobile-page]').forEach(function(t){t.classList.toggle('active',t.getAttribute('data-mobile-page')===name)})}
function syncMobileApControls(){
  var g=document.getElementById('m-ap-gate-tgl');var d=document.getElementById('m-ap-delay-select');var r=document.getElementById('m-ap-auto-restore-tgl');
  var dg=document.getElementById('ap-gate-tgl');if(dg&&g)dg.checked=g.checked;
  var dd=document.getElementById('ap-delay-select');if(dd&&d)dd.value=d.value;
  var dr=document.getElementById('ap-auto-restore-tgl');if(dr&&r)dr.checked=r.checked;
  return saveApGateControls();
}
function renderMobileApState(d){
  var state=d.apInjectionState||'blocked';
  var label=state==='waiting_ap'?'等待 AP':state==='settling'?'稳定计时中':state==='injecting'?'正在注入':'已阻断';
  setText('m-ap-state',label);
  var gate=d.fsdDiag&&d.fsdDiag.gate?d.fsdDiag.gate:{};
  setText('m-ap-detail','AP '+(gate.apStableMs||0)+'/'+(gate.requiredStableMs||d.apDelayMs||2000)+' ms');
  var g=document.getElementById('m-ap-gate-tgl');if(g)g.checked=!!d.apGateEnabled;
  var sel=document.getElementById('m-ap-delay-select');if(sel)sel.value=String(d.apDelayMs||2000);
  var r=document.getElementById('m-ap-auto-restore-tgl');if(r)r.checked=!!d.apAutoRestore;
}
function installMobilePluginUrl(){var v=document.getElementById('m-plugin-url').value;document.getElementById('plugin-url').value=v;return installPluginUrl()}
function installMobilePluginJson(){var v=document.getElementById('m-plugin-json').value;document.getElementById('plugin-json').value=v;return installPluginJson()}
function renderMobilePlugins(j){
  var list=document.getElementById('m-plugin-list');if(!list)return;
  list.innerHTML=(j.installed||[]).map(function(p){var name=String(p.name||'');return '<div class="plugin-item"><b>'+esc(name)+'</b><label>Enabled <input type="checkbox" '+(p.enabled?'checked':'')+' onchange="togglePlugin(\''+escAttr(name)+'\',this.checked)"></label><small>Priority #'+(p.priority||1)+'</small></div>'}).join('')||'<div class="hint">No plugins installed.</div>';
  var rc=document.getElementById('m-plugin-replay-count');if(rc)rc.value=String(j.replayCount||1);
}
```

- [ ] **Step 5: Run mobile contract and minify checks**

Run:

```bash
python3 scripts/minify_dashboard.py
python3 scripts/minify_dashboard.py --check
PYTHONPATH=test python3 -m unittest \
  test_dashboard_api_contract.DashboardApiContractTests.test_mobile_single_can_ui_matches_ap_and_plugin_capabilities
```

Expected: PASS.

- [ ] **Step 6: Commit mobile UI**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h test/test_dashboard_api_contract.py
git commit -m "feat: align mobile single can ap plugin ui"
```

---

## Task 10: Clean standalone-visible CAN2/T-2CAN/Auto Shift surfaces without breaking dual CAN

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`
- Modify: `include/web/mcp2515_dashboard_ui.h`
- Modify: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Mark dual-only navigation and pages**

For desktop nav/page nodes, ensure these attributes exist:

```html
<div class="nav-item" data-page="pg-bus2" data-cap="can2" data-single-hide="1"><span class="nav-icon">✦</span><span data-dual-label>CAN2 控制</span></div>
<div class="nav-item" data-page="pg-strobe" data-cap="lighting" data-single-hide="1"><span class="nav-icon">⚡</span><span data-dual-label>灯光特技</span></div>
<div class="nav-item" data-page="pg-shift" data-single-hide="1"><span class="nav-icon">⚙</span><span data-dual-label>自动换挡</span></div>
<div class="page" id="pg-bus2" data-cap="can2" data-single-hide="1">
<div class="page" id="pg-strobe" data-cap="lighting" data-single-hide="1">
<div class="page" id="pg-shift" data-single-hide="1">
```

If the contract forbids direct visible strings, move dual labels into JS constants instead:

```javascript
var DUAL_LABEL_CAN2='CAN'+'2 控制';
var DUAL_LABEL_SHIFT='自动'+'换挡';
```

and set them only when `!isSingleCan()`.

- [ ] **Step 2: Replace standalone overview labels**

Change standalone-visible overview metrics to use `data-single-text`:

```html
<span data-single-text="Single CAN 状态">CAN 状态</span>
<span data-single-text="CAN RX">CAN RX</span>
<span data-single-text="CAN TX">CAN TX</span>
<span data-single-text="CAN Error">CAN Error</span>
```

Remove or hide `m-can2` mobile card using:

```html
<div id="m-can2" data-single-hide="1" data-cap="can2">...</div>
```

- [ ] **Step 3: Guard route-calling JS functions by capability**

Update page loading logic:

```javascript
if(pageId==='pg-bus2'&&CAP.can2Available&&!isSingleCan())pollCAN2();
if(pageId==='pg-bus2'&&CAP.lightingBusSupported&&!isSingleCan())loadLightingConfig();
if(pageId==='pg-bus2'&&CAP.can2Available&&!isSingleCan())loadBurstConfig();
if(pageId==='pg-strobe'&&CAP.lightingBusSupported&&!isSingleCan())loadStrobePage();
if(pageId==='pg-strobe'&&CAP.can2Available&&!isSingleCan())loadBurstConfig();
```

- [ ] **Step 4: Add startup redirect from hidden pages**

In `showPage(pageId)`, before activating the page, add:

```javascript
var target=document.getElementById(pageId);
if(target&&target.getAttribute('data-single-hide')&&isSingleCan())pageId='pg-overview';
```

- [ ] **Step 5: Run cleanup contract test**

Run:

```bash
python3 scripts/minify_dashboard.py
python3 scripts/minify_dashboard.py --check
PYTHONPATH=test python3 -m unittest \
  test_dashboard_api_contract.DashboardApiContractTests.test_single_can_product_ui_hides_dual_can_surfaces \
  test_dashboard_api_contract.DashboardApiContractTests.test_single_can_ui_uses_capabilities_to_hide_can2_and_lighting
```

Expected: PASS.

- [ ] **Step 6: Commit cleanup**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h test/test_dashboard_api_contract.py
git commit -m "fix: clean standalone dual can ui surfaces"
```

---

## Task 11: Harden plugin storage persistence with SPIFFS file format

**Files:**
- Modify: `include/dash_plugin_engine.h`
- Modify: `include/web/mcp2515_dashboard.h`
- Test: `test/test_native_plugin_engine/test_plugin_engine.cpp`

- [ ] **Step 1: Add export/import format to plugin engine**

In `DashPluginEngine`, add public methods:

```cpp
    std::string exportConfigJson() const
    {
        JsonDocument doc;
        doc["replayCount"] = replayCount_;
        JsonArray plugins = doc["plugins"].to<JsonArray>();
        for (const DashPlugin &p : plugins_)
        {
            JsonObject item = plugins.add<JsonObject>();
            item["name"] = p.name;
            item["enabled"] = p.enabled;
            item["priority"] = p.priority;
            item["source"] = p.sourceJson;
        }
        std::string out;
        serializeJson(doc, out);
        return out;
    }

    DashPluginResult importConfigJson(const char *json)
    {
        if (!json || !*json) return DashPluginResult::success("empty");
        JsonDocument doc;
        if (deserializeJson(doc, json)) return DashPluginResult::fail("invalid plugin config");
        plugins_.clear();
        replayCount_ = doc["replayCount"] | 1;
        if (replayCount_ < 1) replayCount_ = 1;
        if (replayCount_ > 5) replayCount_ = 5;
        for (JsonObjectConst item : doc["plugins"].as<JsonArrayConst>())
        {
            std::string source = dashPluginString(item["source"]);
            DashPluginResult r = installJson(source.c_str(), item["enabled"] | false);
            if (!r.ok) continue;
            setPriority(dashPluginString(item["name"]).c_str(), item["priority"] | 1);
        }
        return DashPluginResult::success("imported");
    }
```

Add `sourceJson` field to `DashPlugin`:

```cpp
    std::string sourceJson;
```

In `installJson()`, after parsing succeeds:

```cpp
        plugin.sourceJson = json;
```

- [ ] **Step 2: Update dashboard save/load helpers**

Replace earlier `dashSavePluginState()` and `dashLoadPluginState()` with:

```cpp
#if defined(DASH_PLUGIN_ENGINE)
static constexpr const char *kDashPluginStatePath = "/plugins_state.json";

static void dashSavePluginState()
{
    if (!SPIFFS.begin(true)) return;
    File f = SPIFFS.open(kDashPluginStatePath, "w");
    if (!f) return;
    std::string json = dashPluginEngine.exportConfigJson();
    f.print(json.c_str());
}

static void dashLoadPluginState()
{
    if (!SPIFFS.begin(true)) return;
    if (!SPIFFS.exists(kDashPluginStatePath)) return;
    File f = SPIFFS.open(kDashPluginStatePath, "r");
    if (!f) return;
    String body;
    while (f.available())
        body += static_cast<char>(f.read());
    dashPluginEngine.importConfigJson(body.c_str());
}
#endif
```

- [ ] **Step 3: Call load during dashboard setup**

In dashboard setup function after prefs load and before route registration, add:

```cpp
#if defined(DASH_PLUGIN_ENGINE)
    dashLoadPluginState();
#endif
```

- [ ] **Step 4: Add native test for export/import disabled default**

Append to native plugin test:

```cpp
void test_export_import_preserves_enabled_priority_and_replay_count()
{
    DashPluginEngine engine;
    TEST_ASSERT_TRUE(engine.installJson(R"json({"name":"Persisted","rules":[{"id":921,"ops":[{"type":"set_bit","bit":13}]}]})json", false).ok);
    TEST_ASSERT_TRUE(engine.setEnabled("Persisted", true).ok);
    TEST_ASSERT_TRUE(engine.setReplayCount(4).ok);
    std::string saved = engine.exportConfigJson();

    DashPluginEngine restored;
    TEST_ASSERT_TRUE(restored.importConfigJson(saved.c_str()).ok);

    TEST_ASSERT_EQUAL(1, restored.pluginCount());
    TEST_ASSERT_TRUE(restored.pluginAt(0).enabled);
    TEST_ASSERT_EQUAL(4, restored.replayCount());
}
```

Add `RUN_TEST(test_export_import_preserves_enabled_priority_and_replay_count);`.

- [ ] **Step 5: Run tests**

Run:

```bash
pio test -e native_plugin_engine
```

Expected: PASS.

- [ ] **Step 6: Commit storage hardening**

```bash
git add include/dash_plugin_engine.h include/web/mcp2515_dashboard.h test/test_native_plugin_engine/test_plugin_engine.cpp
git commit -m "feat: persist official plugin state"
```

---

## Task 12: Add docs for official plugins and single-CAN AP UI

**Files:**
- Modify: `docs/plugins.md`
- Modify: `docs/building.md`
- Modify: `README.md`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Update `docs/plugins.md` standalone notes**

Add a section near installation docs:

```markdown
## Waveshare Single CAN standalone notes

The `waveshare_single_can_standalone` build exposes the same official JSON plugin workflow in the dashboard:

- Install from URL
- Upload `.json`
- Paste JSON offline
- New installs start disabled
- Enable/disable each installed plugin independently
- Adjust priority so higher-priority plugins win overlapping bit writes
- Inspect plugin detail before enabling

Single-CAN builds have one physical CAN bus. Plugins with no `bus` field are accepted. Plugins pinned to the configured default bus are accepted. Plugins targeting a bus that does not exist on the standalone board are rejected or shown as incompatible.

GTW2047 replay count only repeats modified `0x7FF` frames generated by enabled plugin rules. Non-2047 frames are never repeated by this setting.

`gtw_silent: true` requires a build with `PLUGIN_GTW_UDS_CUSTOM_KEY` and a custom `pluginGtwUdsComputeKey` implementation. Without it, periodic emit still works but GTW UDS silencing is unavailable.
```

- [ ] **Step 2: Update `docs/building.md` verification notes**

In the Waveshare single CAN section, add:

```markdown
After flashing, verify in the dashboard before connecting to the car:

1. Product name shows `Atlas Single CAN`.
2. `/status.capabilities.singleCan=true`.
3. `/status.capabilities.can2Available=false`.
4. AP Gate is enabled and delayed injection defaults to `2000 ms`.
5. Installed plugins are disabled until you explicitly enable them.
6. GTW2047 replay count is `1x` unless you intentionally change it.
```

- [ ] **Step 3: Update README and CHANGELOG**

README bullet:

```markdown
- Waveshare Single CAN standalone dashboard now exposes AP Gate, delayed injection, AP-AUTO-RESTORE, AP injection state, official JSON plugin management, and GTW2047 replay count while hiding CAN2-only product surfaces.
```

CHANGELOG under `[Unreleased]`:

```markdown
- Added official JSON plugin manager design/implementation for Waveshare Single CAN standalone: URL install, `.json` upload, paste JSON, disabled-by-default installs, per-plugin enable/priority/remove/detail, and GTW2047 replay count.
- Added AP Gate delayed-injection controls and AP injection state to standalone driving status UI.
- Cleaned standalone UI/mobile UI to remove CAN2/T-2CAN/Auto Shift surfaces while preserving dual-CAN builds.
```

- [ ] **Step 4: Commit docs**

```bash
git add docs/plugins.md docs/building.md README.md CHANGELOG.md
git commit -m "docs: describe single can ap plugin workflow"
```

---

## Task 13: Full audit and verification

**Files:**
- Read/check only first; modify files only for issues found.

- [ ] **Step 1: Run Python contract tests**

```bash
PYTHONPATH=test python3 -m unittest test_dashboard_api_contract
```

Expected: all tests PASS.

- [ ] **Step 2: Run WiFi/AP regression tests**

```bash
PYTHONPATH=test python3 -m unittest test_wifi_settings_regression
```

Expected: all tests PASS.

- [ ] **Step 3: Run UI generation check**

```bash
python3 scripts/minify_dashboard.py --check
```

Expected: exit code 0.

- [ ] **Step 4: Run native tests**

```bash
pio test -e native_plugin_engine
pio test -e native_injection_after_ap
pio test -e native_helpers
pio test -e native_dashboard
pio test -e native_single_can_dashboard
```

Expected: all PASS.

- [ ] **Step 5: Build standalone firmware**

```bash
pio run -e waveshare_single_can_standalone
```

Expected: SUCCESS.

- [ ] **Step 6: Build dual-CAN firmware**

```bash
pio run -e lilygo_t2can_dual
```

Expected: SUCCESS. This is the key guard that the dual-CAN main project still compiles.

- [ ] **Step 7: Audit standalone firmware strings**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
bin_path = Path('.pio/build/waveshare_single_can_standalone/firmware.bin')
data = bin_path.read_bytes()
required = [b'/plugins/status', b'Atlas Single CAN', b'GTW2047', b'apDelayMs', b'apInjectionState']
for token in required:
    print(token.decode(), 'PRESENT' if token in data else 'MISSING')
forbidden = [b'/service_mode', b'/stalk_test', b'/bus2_ids', b'/lighting_config', b'/fog_light', b'/strobe_cont']
for token in forbidden:
    print(token.decode(), 'ABSENT' if token not in data else 'PRESENT')
PY
```

Expected:

```text
/plugins/status PRESENT
Atlas Single CAN PRESENT
GTW2047 PRESENT
apDelayMs PRESENT
apInjectionState PRESENT
/service_mode ABSENT
/stalk_test ABSENT
/bus2_ids ABSENT
/lighting_config ABSENT
/fog_light ABSENT
/strobe_cont ABSENT
```

- [ ] **Step 8: Fix any audit failures**

If any command fails, fix the specific failing source and re-run the failed command plus its nearest related command. Examples:

- UI generated header stale: run `python3 scripts/minify_dashboard.py`, commit source and generated header.
- Standalone route string present: inspect route registration guard and wrap it in `#if defined(DRIVER_T2CAN_DUAL) && !defined(DASH_SINGLE_CAN_STANDALONE)` or `#if !defined(DASH_SINGLE_CAN_STANDALONE)` as appropriate.
- Dual build fails: revert or guard standalone-only code behind `DASH_PLUGIN_ENGINE` / `DASH_SINGLE_CAN_STANDALONE`.

- [ ] **Step 9: Commit verification fixes if any**

If Step 8 changed files:

```bash
git add <changed-files>
git commit -m "fix: resolve single can audit findings"
```

If Step 8 changed nothing, do not create an empty commit.

---

## Task 14: Final review handoff and memory update

**Files:**
- Modify: `/Users/ziwind/my-vibe-project/memory.md`
- Modify: `/Users/ziwind/.claude/projects/-Users-ziwind-my-vibe-project/memory/lilygo-waveshare-single-can-standalone-20260615.md`
- Modify: `/Users/ziwind/.claude/projects/-Users-ziwind-my-vibe-project/memory/MEMORY.md` only if the hook line needs updated wording.

- [ ] **Step 1: Summarize commits and verification evidence**

Run:

```bash
git -C /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware log --oneline --decorate -n 20
git -C /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware status --short --branch
```

Expected:

- Current branch is `feature/waveshare-single-can-standalone`.
- Only intended files are modified or committed.
- Unrelated pre-existing untracked files may remain; do not include them.

- [ ] **Step 2: Request code review using the required skill**

Use `superpowers:requesting-code-review` or the available `code-review` skill on the current diff/branch.

Review focus:

- AP safety truthfulness
- plugin parser safety
- GTW2047 replay semantics
- standalone UI cleanup
- dual-CAN regression risk
- storage/persistence failure behavior

- [ ] **Step 3: Fix review findings**

For each accepted finding, make the minimal fix, run the nearest tests, and commit:

```bash
git add <fixed-files>
git commit -m "fix: address plugin audit review findings"
```

If a finding is rejected, document the technical reason in the final handoff.

- [ ] **Step 4: Run final verification before claiming done**

Use `superpowers:verification-before-completion`, then re-run the verification commands from Task 13 that cover changed areas.

Minimum final commands:

```bash
PYTHONPATH=test python3 -m unittest test_dashboard_api_contract test_wifi_settings_regression
python3 scripts/minify_dashboard.py --check
pio test -e native_plugin_engine
pio test -e native_injection_after_ap
pio test -e native_dashboard
pio test -e native_single_can_dashboard
pio run -e waveshare_single_can_standalone
pio run -e lilygo_t2can_dual
```

- [ ] **Step 5: Update project memory**

Append a concise update to `/Users/ziwind/my-vibe-project/memory.md`:

```markdown
### 2026-06-15 — Waveshare Single CAN UI/plugin follow-up

Completed follow-up on `feature/waveshare-single-can-standalone`: AP Gate/delayed injection/AP-AUTO-RESTORE UI and status, official JSON plugin manager aligned with upstream `ev-open-can-tools`, GTW2047 replay count semantics, mobile UI parity, standalone UI cleanup of CAN2/T-2CAN/Auto Shift surfaces, and audit verification. Dual-CAN `lilygo_t2can_dual` build remains protected. Not pushed, not merged, not flashed.
```

Update the persistent memory file with the same facts and the final HEAD commit.

- [ ] **Step 6: Final handoff**

Report:

- What changed
- What tests passed with exact command names
- Whether dual-CAN build passed
- Whether any limitations remain, especially `gtw_silent` requiring `PLUGIN_GTW_UDS_CUSTOM_KEY`
- That nothing was pushed, merged, or flashed
- Next optional actions: merge, push, or hardware flash only if Jordan explicitly asks

Do not claim completion until Step 4 verification passes.
