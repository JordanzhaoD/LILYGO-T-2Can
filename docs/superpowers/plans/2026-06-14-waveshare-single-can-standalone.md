# Waveshare Single CAN Standalone Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a long-term maintainable Waveshare ESP32-S3-RS485-CAN standalone firmware profile with single-CAN capabilities, UI/API pruning, and upstream v3.0.2-beta.3 AP-gate safety parity.

**Architecture:** Add a dedicated `waveshare_single_can_standalone` PlatformIO environment that keeps the existing TWAI Waveshare hardware path but declares `DASH_SINGLE_CAN_STANDALONE` and boots hardware mode in Auto. Expose runtime capabilities from `/status`; use them to hide CAN2/lighting surfaces in the dashboard and to gate CAN2-only routes. Add a narrow Legacy `0x3EE mux0` AP-active-for-2-seconds gate that only applies when AP Injection Gate is enabled, while making live unknown gear fail closed.

**Tech Stack:** ESP-IDF via PlatformIO, C++17 firmware, ESP32 Dashboard embedded HTML/CSS/JS, Python unittest contract tests, Unity native PlatformIO tests.

---

## File Structure and Responsibilities

**Create:**
- `docs/superpowers/plans/2026-06-14-waveshare-single-can-standalone.md` — this implementation plan.

**Modify:**
- `platformio.ini` — add `waveshare_single_can_standalone` firmware env and `native_single_can_dashboard` native env.
- `scripts/platformio_sync_profile.py` — allow dashboard initial hardware mode values to sync cleanly when a profile or env provides one.
- `include/web/mcp2515_dashboard.h` — define standalone capability booleans, append `/status.capabilities`, set Auto as initial hardware mode, gate unsupported routes, and add Dashboard-side Legacy FSD activation settle callback.
- `include/web/mcp2515_dashboard_ui.src.h` — add capability-aware visibility pruning without changing the Atlas visual style.
- `include/web/mcp2515_dashboard_ui.h` — regenerated gzip dashboard header.
- `include/can_helpers.h` — change gear parked helper to fail closed for invalid/SNA/unknown live gear.
- `include/dash_fsd_diag.h` — add a diagnostic gate reason for Legacy FSD activation settle blocking.
- `include/handlers.h` — add a Legacy mux0 activation callback and use it before injecting `0x3EE mux0`.
- `test/test_dashboard_api_contract.py` — static contract tests for profile, capabilities, UI pruning, unsupported API behavior, and docs.
- `test/test_native_helpers/test_helpers.cpp` — gear fail-closed tests.
- `test/test_native_injection_after_ap/test_injection_after_ap.cpp` — Legacy `0x3EE mux0` AP-settle tests.
- `docs/building.md` — document the standalone env.
- `README.md` — add a concise Waveshare single-CAN entry.
- `CHANGELOG.md` — record the standalone design and safety behavior.

**Do not modify in this plan:**
- `src/main.cpp` dual-CAN sections, unless a later compile error proves the standalone macro leaks into that path. Existing `#ifdef DRIVER_T2CAN_DUAL` already keeps MCP2515 runtime out of TWAI-only builds.
- MCP2515 driver files. They remain available for existing dual-CAN builds.

---

### Task 1: Add Static Contract Tests for the Standalone Product Shape

**Files:**
- Modify: `test/test_dashboard_api_contract.py`
- Test: `python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_waveshare_single_can_standalone_env_is_declared -v`

- [ ] **Step 1: Write failing tests for the env, capabilities, UI pruning, route gating, and docs**

Add these methods inside `DashboardApiContractTests`, after `test_status_exposes_build_and_legacy_diagnostics`:

```python
    def test_waveshare_single_can_standalone_env_is_declared(self) -> None:
        pio = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        self.assertIn("[env:waveshare_single_can_standalone]", pio)
        self.assertIn("-DDRIVER_TWAI", pio)
        self.assertIn("-DESP32_DASHBOARD", pio)
        self.assertIn("-DDASH_SINGLE_CAN_STANDALONE=1", pio)
        self.assertIn("-DDASH_INITIAL_HW_MODE=3", pio)
        self.assertIn("-DTWAI_TX_PIN=GPIO_NUM_15", pio)
        self.assertIn("-DTWAI_RX_PIN=GPIO_NUM_16", pio)
        self.assertIn("partitions_16mb_ota_4096k_nvs64.csv", pio)

    def test_status_exposes_single_can_capabilities(self) -> None:
        for token in [
            "dashCapabilitiesJson",
            '"capabilities"',
            '"singleCan"',
            '"can2Available"',
            '"lightingBusSupported"',
            '"serviceModeSupported"',
            '"stalkTestSupported"',
            '"bus2SnifferSupported"',
            '"fsdActivation"',
            '"speedStrategy"',
            '"driveProfile"',
            '"networkSettings"',
            '"otaUpdate"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)

    def test_single_can_build_gates_can2_only_routes(self) -> None:
        for token in [
            "DASH_SINGLE_CAN_STANDALONE",
            "server.on(\"/service_mode\", HTTP_GET, handleServiceMode);",
            "server.on(\"/stalk_test\", HTTP_GET, handleStalkTest);",
            "server.on(\"/burst\", HTTP_GET, handleBurst);",
            "server.on(\"/bus2_ids\", HTTP_GET, handleBus2Ids);",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)
        self.assertRegex(
            self.dash,
            r"#if defined\(DRIVER_T2CAN_DUAL\) && !defined\(DASH_SINGLE_CAN_STANDALONE\).*?/bus2_ids",
        )
        self.assertRegex(
            self.dash,
            r"#if !defined\(DASH_SINGLE_CAN_STANDALONE\).*?/lighting_config",
        )

    def test_single_can_ui_uses_capabilities_to_hide_can2_and_lighting(self) -> None:
        for token in [
            "var CAP=",
            "function applyCapabilities(c)",
            "data-cap=\"can2\"",
            "data-cap=\"lighting\"",
            "cap-hidden",
            "applyCapabilities(d.capabilities||{})",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_single_can_docs_are_present(self) -> None:
        building = (ROOT / "docs" / "building.md").read_text(encoding="utf-8")
        self.assertIn("waveshare_single_can_standalone", building)
        self.assertIn("Waveshare 单 CAN", self.readme)
        self.assertIn("DASH_SINGLE_CAN_STANDALONE", self.changelog)
```

- [ ] **Step 2: Run the first new test and confirm it fails**

Run:

```bash
python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_waveshare_single_can_standalone_env_is_declared -v
```

Expected: FAIL because `[env:waveshare_single_can_standalone]` is not in `platformio.ini` yet.

- [ ] **Step 3: Run the full contract file and record current failures**

Run:

```bash
python3 -m unittest test.test_dashboard_api_contract -v
```

Expected: FAIL in the new standalone tests only. If old tests fail too, stop and inspect the exact failure before editing production code.

- [ ] **Step 4: Commit the failing contract tests**

```bash
git add test/test_dashboard_api_contract.py
git commit -m "test: define waveshare single can standalone contracts"
```

---

### Task 2: Add Native Safety Tests for Fail-Closed Gear and Legacy AP Settle

**Files:**
- Modify: `test/test_native_helpers/test_helpers.cpp`
- Modify: `test/test_native_injection_after_ap/test_injection_after_ap.cpp`
- Test: `pio test -e native_helpers -f test_native_helpers`
- Test: `pio test -e native_injection_after_ap -f test_native_injection_after_ap`

- [ ] **Step 1: Change gear tests to describe fail-closed behavior**

In `test/test_native_helpers/test_helpers.cpp`, replace `test_isVehicleParked_true_for_sna` and `test_isVehicleParked_true_for_invalid` with:

```cpp
void test_isVehicleParked_false_for_sna()
{
    // Live SNA must fail closed. It must not open AP Injection Gate as Park.
    TEST_ASSERT_FALSE(isVehicleParked(7));
}

void test_isVehicleParked_false_for_invalid()
{
    // Live INVALID must fail closed. It must not open AP Injection Gate as Park.
    TEST_ASSERT_FALSE(isVehicleParked(0));
}
```

Then replace the two `RUN_TEST` entries near the bottom:

```cpp
    RUN_TEST(test_isVehicleParked_false_for_sna);
    RUN_TEST(test_isVehicleParked_false_for_invalid);
```

- [ ] **Step 2: Run helper tests and confirm they fail**

Run:

```bash
pio test -e native_helpers -f test_native_helpers
```

Expected: FAIL because `isVehicleParked(0)` and `isVehicleParked(7)` still return true.

- [ ] **Step 3: Add Legacy AP settle tests**

In `test/test_native_injection_after_ap/test_injection_after_ap.cpp`, add these helpers after `activateAp`:

```cpp
static CanFrame legacyMux0Frame(bool fsdSelected = true)
{
    CanFrame f = {.id = 1006, .dlc = 8};
    f.data[0] = 0x00;
    if (fsdSelected)
        markFsdSelectedInUI(f);
    return f;
}

static bool legacyGateAllowAfter2000(uint32_t nowMs)
{
    return nowMs >= 2000;
}

static bool legacyGateAlwaysBlocks(uint32_t)
{
    return false;
}
```

Add these tests before `main()`:

```cpp
void test_legacy_mux0_waits_for_explicit_activation_gate_even_when_parked()
{
    LegacyHandler handler;
    handler.enablePrint = false;
    handler.legacyFsdActivationAllowed = legacyGateAlwaysBlocks;

    CanFrame park = gearFrame(1);
    handler.handleMessage(park, mock);
    TEST_ASSERT_TRUE(handler.Parked);

    CanFrame mux0 = legacyMux0Frame();
    handler.handleMessage(mux0, mock);

    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL(FsdGateBlockReason::LegacyFsdSettle, handler.legacyFsdDiag.lastBlockedBy);
    TEST_ASSERT_EQUAL(FsdSkipReason::GateBlocked, handler.legacyFsdDiag.mux0.lastSkip);
}

void test_legacy_mux0_sends_after_explicit_activation_gate_allows()
{
    LegacyHandler handler;
    handler.enablePrint = false;
    handler.legacyFsdActivationAllowed = legacyGateAllowAfter2000;

    CanFrame drive = gearFrame(4);
    handler.handleMessage(drive, mock);
    TEST_ASSERT_FALSE(handler.Parked);

    CanFrame mux0 = legacyMux0Frame();
    for (int i = 0; i < 1998; ++i)
        (void)dashDiagNowMs();
    handler.handleMessage(mux0, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_TRUE((mock.sent[0].data[5] >> 6) & 0x01);
}
```

Register both tests in `main()` after the existing HW4 tests:

```cpp
    RUN_TEST(test_legacy_mux0_waits_for_explicit_activation_gate_even_when_parked);
    RUN_TEST(test_legacy_mux0_sends_after_explicit_activation_gate_allows);
```

- [ ] **Step 4: Run injection tests and confirm they fail**

Run:

```bash
pio test -e native_injection_after_ap -f test_native_injection_after_ap
```

Expected: FAIL to compile because `legacyFsdActivationAllowed` and `FsdGateBlockReason::LegacyFsdSettle` do not exist yet.

- [ ] **Step 5: Commit failing safety tests**

```bash
git add test/test_native_helpers/test_helpers.cpp test/test_native_injection_after_ap/test_injection_after_ap.cpp
git commit -m "test: specify single can AP gate safety behavior"
```

---

### Task 3: Add the Standalone PlatformIO Environments and Initial Auto Hardware Mode

**Files:**
- Modify: `platformio.ini`
- Modify: `include/web/mcp2515_dashboard.h`
- Modify: `scripts/platformio_sync_profile.py`
- Test: `python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_waveshare_single_can_standalone_env_is_declared -v`

- [ ] **Step 1: Add the standalone firmware env**

In `platformio.ini`, insert this block immediately after `[env:waveshare_ESP32_S3_RS485_CAN]`:

```ini
[env:waveshare_single_can_standalone]
; Waveshare ESP32-S3-RS485-CAN standalone product profile.
; Single CAN: native TWAI only, no MCP2515/CAN2 runtime.
platform = ${espidf6.platform}
platform_packages = ${espidf6.platform_packages}
board = esp32s3box
framework = espidf
extra_scripts =
    pre:scripts/update_ota_build_timestamp.py
    pre:scripts/platformio_sync_profile.py
lib_deps = bblanchon/ArduinoJson@^7
board_build.flash_size = 16MB
build_flags =
    -I include
    -DDRIVER_TWAI
    -DESP32_DASHBOARD
    -DDASH_SINGLE_CAN_STANDALONE=1
    -DDASH_INITIAL_HW_MODE=3
    -DTWAI_TX_PIN=GPIO_NUM_15
    -DTWAI_RX_PIN=GPIO_NUM_16
    -DTWAI_RX_QUEUE_LEN=64
    -DTWAI_TX_QUEUE_LEN=16
    -DDASH_FSD_252_COMPAT=1
    -DPIN_LED=14
    -DDASH_STA_AP_GATEWAY
    -DESP_IDF_LWIP_HOOK_FILENAME=\"lwip_hooks.h\"
    -std=gnu++17
build_unflags = -std=gnu++11
board_build.partitions = partitions_16mb_ota_4096k_nvs64.csv
monitor_speed = 115200
```

Do not add this env to `default_envs` in this task. Keep default builds unchanged until final verification proves the standalone build is stable.

- [ ] **Step 2: Add a native standalone dashboard env**

In `platformio.ini`, insert this block after `[env:native_dashboard]`:

```ini
[env:native_single_can_dashboard]
platform = native
extra_scripts = pre:scripts/platformio_native_env.py
build_flags = -std=c++17 -DNATIVE_BUILD -DESP32_DASHBOARD -DDASH_SINGLE_CAN_STANDALONE=1 -DDASH_INITIAL_HW_MODE=3 -DISA_SPEED_CHIME_SUPPRESS -DEMERGENCY_VEHICLE_DETECTION -DENHANCED_AUTOPILOT -DNAG_KILLER
build_src_filter = +<*> -<main.cpp>
test_filter = test_native_dashboard
```

- [ ] **Step 3: Make dashboard initial hardware mode overridable**

In `include/web/mcp2515_dashboard.h`, replace:

```cpp
static uint8_t hwMode = DASH_DEFAULT_HW;
```

with:

```cpp
#ifndef DASH_INITIAL_HW_MODE
#define DASH_INITIAL_HW_MODE DASH_DEFAULT_HW
#endif
static uint8_t hwMode = DASH_INITIAL_HW_MODE;
```

This keeps existing builds unchanged and makes `waveshare_single_can_standalone` boot into Auto mode (`3`).

- [ ] **Step 4: Teach the sync script that `DASH_INITIAL_HW_MODE` is a dashboard value define**

In `scripts/platformio_sync_profile.py`, replace:

```python
DASHBOARD_VALUE_DEFINES = ()
```

with:

```python
DASHBOARD_VALUE_DEFINES = ("DASH_INITIAL_HW_MODE",)
```

This keeps profile-defined dashboard values legal if they are added later. The standalone env already defines `DASH_INITIAL_HW_MODE=3` directly in `platformio.ini`.

- [ ] **Step 5: Run the profile contract test**

Run:

```bash
python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_waveshare_single_can_standalone_env_is_declared -v
```

Expected: PASS.

- [ ] **Step 6: Run a smoke compile of the native single-CAN dashboard env**

Run:

```bash
pio test -e native_single_can_dashboard -f test_native_dashboard
```

Expected: May still FAIL because capabilities and UI pruning are not implemented. It must not fail because the env is unknown.

- [ ] **Step 7: Commit the env and initial Auto mode**

```bash
git add platformio.ini include/web/mcp2515_dashboard.h scripts/platformio_sync_profile.py
git commit -m "feat: add waveshare single can standalone profile"
```

---

### Task 4: Implement `/status.capabilities` and Gate Unsupported Routes

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`
- Test: `python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_status_exposes_single_can_capabilities -v`
- Test: `python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_single_can_build_gates_can2_only_routes -v`

- [ ] **Step 1: Add capability constants and JSON helper**

In `include/web/mcp2515_dashboard.h`, after `static constexpr uint8_t dashLedBrightness = kDashLedBrightnessDefault;`, insert:

```cpp
#if defined(DASH_SINGLE_CAN_STANDALONE)
static constexpr bool kDashSingleCanStandalone = true;
#else
static constexpr bool kDashSingleCanStandalone = false;
#endif

#if defined(DRIVER_T2CAN_DUAL) && !defined(DASH_SINGLE_CAN_STANDALONE)
static constexpr bool kDashCan2Available = true;
#else
static constexpr bool kDashCan2Available = false;
#endif

static constexpr bool kDashLightingBusSupported = kDashCan2Available;
static constexpr bool kDashServiceModeSupported = kDashCan2Available;
static constexpr bool kDashStalkTestSupported = kDashCan2Available;
static constexpr bool kDashBus2SnifferSupported = kDashCan2Available;

static String dashCapabilitiesJson()
{
    String j = "{\"singleCan\":";
    j += kDashSingleCanStandalone ? "true" : "false";
    j += ",\"can2Available\":";
    j += kDashCan2Available ? "true" : "false";
    j += ",\"lightingBusSupported\":";
    j += kDashLightingBusSupported ? "true" : "false";
    j += ",\"serviceModeSupported\":";
    j += kDashServiceModeSupported ? "true" : "false";
    j += ",\"stalkTestSupported\":";
    j += kDashStalkTestSupported ? "true" : "false";
    j += ",\"bus2SnifferSupported\":";
    j += kDashBus2SnifferSupported ? "true" : "false";
    j += ",\"fsdActivation\":true";
    j += ",\"speedStrategy\":true";
    j += ",\"driveProfile\":true";
    j += ",\"networkSettings\":true";
    j += ",\"otaUpdate\":true";
    j += ",\"canDiagnostics\":true}";
    return j;
}
```

- [ ] **Step 2: Append capabilities to `/status`**

In `handleStatus()`, after these lines:

```cpp
    j += ",\"uiBuildUtc\":\"" DASH_UI_BUILD_UTC "\"";
```

insert:

```cpp
    j += ",\"capabilities\":";
    j += dashCapabilitiesJson();
```

- [ ] **Step 3: Gate CAN2-only and lighting routes**

In `mcpDashboardSetup()`, replace the route section from `/lighting_config` through `/strobe_cont` and the `DRIVER_T2CAN_DUAL` block with this exact structure:

```cpp
#if !defined(DASH_SINGLE_CAN_STANDALONE)
    server.on("/lighting_config", HTTP_GET, handleLightingConfig);
    server.on("/lighting_config", HTTP_POST, handleLightingConfig);
#endif
    server.on("/defense_config", HTTP_GET, handleDefenseConfig);
    server.on("/defense_config", HTTP_POST, handleDefenseConfig);
    server.on("/legacy_fsd_config", HTTP_GET, handleLegacyFsdConfig);
    server.on("/legacy_fsd_config", HTTP_POST, handleLegacyFsdConfig);
    server.on("/gear_assist_status", HTTP_GET, handleGearAssistStatus);
    // Phase 1 新增端点
    server.on("/power_mgmt",       HTTP_GET,  handlePowerMgmt);
    server.on("/power_mgmt",       HTTP_POST, handlePowerMgmt);
    server.on("/vehicle_ota_status", HTTP_GET, handleVehicleOtaStatus);
#if !defined(DASH_SINGLE_CAN_STANDALONE)
    server.on("/fog_light",        HTTP_GET,  handleFogLight);
    server.on("/fog_light",        HTTP_POST, handleFogLight);
    server.on("/strobe_cont",      HTTP_POST, handleStrobeCont);
#endif
```

Then replace:

```cpp
#ifdef DRIVER_T2CAN_DUAL
```

above `/service_mode` with:

```cpp
#if defined(DRIVER_T2CAN_DUAL) && !defined(DASH_SINGLE_CAN_STANDALONE)
```

- [ ] **Step 4: Run capabilities and route-gating tests**

Run:

```bash
python3 -m unittest \
  test.test_dashboard_api_contract.DashboardApiContractTests.test_status_exposes_single_can_capabilities \
  test.test_dashboard_api_contract.DashboardApiContractTests.test_single_can_build_gates_can2_only_routes \
  -v
```

Expected: PASS.

- [ ] **Step 5: Run dashboard native smoke test**

Run:

```bash
pio test -e native_single_can_dashboard -f test_native_dashboard
```

Expected: PASS or only UI-generation-related failure that will be fixed in Task 5. If it fails due to missing symbols from route gating, fix those symbols before committing.

- [ ] **Step 6: Commit capabilities and route gating**

```bash
git add include/web/mcp2515_dashboard.h
git commit -m "feat: expose single can capabilities"
```

---

### Task 5: Add Capability-Aware Dashboard UI Pruning

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`
- Modify: `include/web/mcp2515_dashboard_ui.h`
- Test: `python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_single_can_ui_uses_capabilities_to_hide_can2_and_lighting -v`
- Test: `python3 scripts/minify_dashboard.py --check`

- [ ] **Step 1: Add CSS class for hidden capabilities**

In `include/web/mcp2515_dashboard_ui.src.h`, inside the CSS block near the other utility/page classes, add:

```css
.cap-hidden{display:none!important}
```

- [ ] **Step 2: Mark CAN2 and lighting nav items**

Replace the two sidebar nav lines:

```html
    <div class="nav-item" data-page="pg-bus2"><span class="nav-icon">✦</span>CAN2 控制</div>
    <div class="nav-item" data-page="pg-strobe"><span class="nav-icon">⚡</span>灯光特技</div>
```

with:

```html
    <div class="nav-item" data-page="pg-bus2" data-cap="can2"><span class="nav-icon">✦</span>CAN2 控制</div>
    <div class="nav-item" data-page="pg-strobe" data-cap="lighting"><span class="nav-icon">⚡</span>灯光特技</div>
```

- [ ] **Step 3: Mark CAN2 and lighting pages**

Replace:

```html
    <div class="page" id="pg-bus2">
```

with:

```html
    <div class="page" id="pg-bus2" data-cap="can2">
```

Replace:

```html
    <div class="page" id="pg-strobe">
```

with:

```html
    <div class="page" id="pg-strobe" data-cap="lighting">
```

- [ ] **Step 4: Add capability state and helpers**

In the JavaScript section before `function applyI18n(){`, insert:

```javascript
var CAP={singleCan:false,can2Available:true,lightingBusSupported:true,serviceModeSupported:true,stalkTestSupported:true,bus2SnifferSupported:true,fsdActivation:true,speedStrategy:true,driveProfile:true,networkSettings:true,otaUpdate:true,canDiagnostics:true};
function capEnabled(name){
  if(name==='can2')return !!CAP.can2Available;
  if(name==='lighting')return !!CAP.lightingBusSupported;
  return true;
}
function applyCapabilities(c){
  if(c){for(var k in c)CAP[k]=c[k]}
  var nodes=document.querySelectorAll('[data-cap]');
  for(var i=0;i<nodes.length;i++){
    var n=nodes[i];
    var show=capEnabled(n.getAttribute('data-cap'));
    n.classList.toggle('cap-hidden',!show);
  }
  var active=document.querySelector('.page.active');
  if(active&&active.getAttribute('data-cap')&&!capEnabled(active.getAttribute('data-cap'))){
    showPage('pg-overview');
  }
}
```

- [ ] **Step 5: Apply capabilities from `/status`**

In `poll()`, after:

```javascript
  S.hw=d.hw;S.ci=d.ci;S.sp=d.sp;S.spa=d.spAuto;S.can=d.can;S.ia=d.ia;S.driveProfile=d.driveProfile!==undefined?d.driveProfile:S.driveProfile;
```

insert:

```javascript
  applyCapabilities(d.capabilities||{});
```

- [ ] **Step 6: Guard page loaders**

In `showPage(pageId)`, replace:

```javascript
  if(pageId==='pg-can')pollCanTab();
  if(pageId==='pg-drive')loadDriveProfile();
  if(pageId==='pg-bus2')pollCAN2();
  if(pageId==='pg-bus2')loadLightingConfig();
  if(pageId==='pg-bus2')loadBurstConfig();
  if(pageId==='pg-strobe')loadStrobePage();
  if(pageId==='pg-strobe')loadBurstConfig();
```

with:

```javascript
  if(pageId==='pg-can')pollCanTab();
  if(pageId==='pg-drive')loadDriveProfile();
  if(pageId==='pg-bus2'&&CAP.can2Available)pollCAN2();
  if(pageId==='pg-bus2'&&CAP.lightingBusSupported)loadLightingConfig();
  if(pageId==='pg-bus2'&&CAP.can2Available)loadBurstConfig();
  if(pageId==='pg-strobe'&&CAP.lightingBusSupported)loadStrobePage();
  if(pageId==='pg-strobe'&&CAP.can2Available)loadBurstConfig();
```

- [ ] **Step 7: Regenerate dashboard header**

Run:

```bash
python3 scripts/minify_dashboard.py
```

Expected: output says the generated dashboard header was written.

- [ ] **Step 8: Run UI tests and minify check**

Run:

```bash
python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_single_can_ui_uses_capabilities_to_hide_can2_and_lighting -v
python3 scripts/minify_dashboard.py --check
```

Expected: both PASS.

- [ ] **Step 9: Commit UI pruning**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "feat: hide unsupported single can dashboard surfaces"
```

---

### Task 6: Implement Upstream v3.0.2-beta.3 Safety Parity

**Files:**
- Modify: `include/can_helpers.h`
- Modify: `include/dash_fsd_diag.h`
- Modify: `include/handlers.h`
- Modify: `include/web/mcp2515_dashboard.h`
- Test: `pio test -e native_helpers -f test_native_helpers`
- Test: `pio test -e native_injection_after_ap -f test_native_injection_after_ap`

- [ ] **Step 1: Make live gear fail closed**

In `include/can_helpers.h`, replace `isVehicleParked` with:

```cpp
inline bool isVehicleParked(uint8_t gear)
{
    // Live gear must fail closed for INVALID (0), SNA (7), and reserved values.
    // Only true Park (1) opens the parked branch of AP Injection Gate.
    return gear == 1;
}
```

- [ ] **Step 2: Add a diagnostic reason for Legacy FSD settle blocking**

In `include/dash_fsd_diag.h`, change `FsdGateBlockReason` to include `LegacyFsdSettle`:

```cpp
enum class FsdGateBlockReason : uint8_t
{
    None = 0,
    CheckAd = 1,
    CanActive = 2,
    Ota = 3,
    ApGate = 4,
    CompileGate = 5,
    LegacyFsdSettle = 6,
};
```

Then add this case to `fsdGateBlockReasonName`:

```cpp
    case FsdGateBlockReason::LegacyFsdSettle: return "legacy_fsd_settle";
```

- [ ] **Step 3: Add a narrow Legacy activation callback to `CarManagerBase`**

In `include/handlers.h`, after:

```cpp
    FsdGateBlockReason (*gateBlockReason)() = nullptr;
```

insert:

```cpp
    bool (*legacyFsdActivationAllowed)(uint32_t nowMs) = nullptr;
```

In `LegacyHandler` mux0 path, replace:

```cpp
                if (!injectionAllowed())
                {
                    legacyFsdDiag.mux0.lastSkip = FsdSkipReason::GateBlocked;
                    legacyFsdDiag.health = FsdHealthState::GateBlocked;
                    legacyFsdDiag.lastBlockedBy = currentGateBlockReason();
                    return;
                }
```

with:

```cpp
                bool legacyActivationAllowed = legacyFsdActivationAllowed
                    ? legacyFsdActivationAllowed(nowMs)
                    : injectionAllowed();
                if (!legacyActivationAllowed)
                {
                    legacyFsdDiag.mux0.lastSkip = FsdSkipReason::GateBlocked;
                    legacyFsdDiag.health = FsdHealthState::GateBlocked;
                    legacyFsdDiag.lastBlockedBy = legacyFsdActivationAllowed
                        ? FsdGateBlockReason::LegacyFsdSettle
                        : currentGateBlockReason();
                    return;
                }
```

Do not change mux1, HW3, or HW4 paths in this step.

- [ ] **Step 4: Add Dashboard-side 2-second AP settle state**

In `include/web/mcp2515_dashboard.h`, after `static volatile uint32_t lastInjectMs = 0;`, insert:

```cpp
static constexpr uint32_t kLegacyFsdActivationSettleMs = 2000;
static uint32_t legacyFsdApActiveSinceMs = 0;
static uint32_t legacyFsdRequiredStableMs = kLegacyFsdActivationSettleMs;
static uint32_t legacyFsdLastBlockedMs = 0;
static bool legacyFsdLastAllowed = false;
```

- [ ] **Step 5: Implement Dashboard Legacy activation gate helper**

In `include/web/mcp2515_dashboard.h`, after `dashApInjectionAllowed()`, insert:

```cpp
static bool dashLegacyFsdActivationAllowed(uint32_t nowMs)
{
    if (!canActive)
    {
        legacyFsdApActiveSinceMs = 0;
        legacyFsdLastAllowed = false;
        legacyFsdLastBlockedMs = nowMs;
        return false;
    }
    if (!dashOtaGuardAllowInjection())
    {
        legacyFsdApActiveSinceMs = 0;
        legacyFsdLastAllowed = false;
        legacyFsdLastBlockedMs = nowMs;
        return false;
    }
    if (!apInjectionGate)
    {
        legacyFsdLastAllowed = true;
        return true;
    }
    bool apActive = dashHandler && (bool)dashHandler->APActive;
    if (!apActive)
    {
        legacyFsdApActiveSinceMs = 0;
        legacyFsdLastAllowed = false;
        legacyFsdLastBlockedMs = nowMs;
        return false;
    }
    if (legacyFsdApActiveSinceMs == 0)
        legacyFsdApActiveSinceMs = nowMs;
    bool stable = (nowMs - legacyFsdApActiveSinceMs) >= kLegacyFsdActivationSettleMs;
    legacyFsdLastAllowed = stable;
    if (!stable)
        legacyFsdLastBlockedMs = nowMs;
    return stable;
}
```

- [ ] **Step 6: Wire the callback into handler setup**

In the function that assigns dashboard callbacks to `dashHandler` and `handlerPool[i]`, add this assignment wherever `gateBlockReason` is assigned:

```cpp
    dashHandler->legacyFsdActivationAllowed = dashLegacyFsdActivationAllowed;
```

and for handler pool entries:

```cpp
        handlerPool[i]->legacyFsdActivationAllowed = dashLegacyFsdActivationAllowed;
```

Use the existing nearby lines that assign `dashCurrentGateBlockReason` as the anchor.

- [ ] **Step 7: Add gate diagnostics fields**

In `appendFsdDiagJson`, inside the `"gate"` object after `"apActive"`, append:

```cpp
    j += ",\"apStableMs\":";
    j += (apActive && legacyFsdApActiveSinceMs > 0) ? String(now - legacyFsdApActiveSinceMs) : String(0);
    j += ",\"requiredStableMs\":";
    j += legacyFsdRequiredStableMs;
    j += ",\"legacyFsdAllowed\":";
    j += legacyFsdLastAllowed ? "true" : "false";
    j += ",\"blockedFrame\":\"0x3EE mux0\"";
```

Keep the existing `parked`, `summoning`, `finalAllowed`, and `gateReason` fields.

- [ ] **Step 8: Run helper tests**

Run:

```bash
pio test -e native_helpers -f test_native_helpers
```

Expected: PASS.

- [ ] **Step 9: Run injection-after-AP tests**

Run:

```bash
pio test -e native_injection_after_ap -f test_native_injection_after_ap
```

Expected: PASS.

- [ ] **Step 10: Run dashboard contract coverage for new diagnostic names**

If Task 1 did not already assert the new gate names, add this token list to `test_legacy_can_a_fsd_diag_expanded_fields_are_exposed` in `test/test_dashboard_api_contract.py`:

```python
            'LegacyFsdSettle',
            'legacy_fsd_settle',
            'legacyFsdActivationAllowed',
            'dashLegacyFsdActivationAllowed',
            'requiredStableMs',
            'legacyFsdAllowed',
            'blockedFrame',
```

Then run:

```bash
python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_legacy_can_a_fsd_diag_expanded_fields_are_exposed -v
```

Expected: PASS.

- [ ] **Step 11: Commit safety parity implementation**

```bash
git add include/can_helpers.h include/dash_fsd_diag.h include/handlers.h include/web/mcp2515_dashboard.h test/test_dashboard_api_contract.py
git commit -m "fix: align AP gate safety with upstream beta3"
```

---

### Task 7: Update Documentation for the Standalone Build and Safety Boundary

**Files:**
- Modify: `docs/building.md`
- Modify: `README.md`
- Modify: `CHANGELOG.md`
- Test: `python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_single_can_docs_are_present -v`

- [ ] **Step 1: Add build docs**

In `docs/building.md`, add this section after the existing PlatformIO build targets list:

```markdown
## Waveshare 单 CAN 独立版

`waveshare_single_can_standalone` 是微雪 ESP32-S3-RS485-CAN 的长期维护入口。它使用 ESP32-S3 内置 TWAI 单路 CAN，不初始化 MCP2515，也不暴露 CAN2、Bus2、Service Mode、Stalk/highbeam 或灯光特技执行入口。

Build:

```bash
pio run -e waveshare_single_can_standalone
```

Upload:

```bash
pio run -e waveshare_single_can_standalone -t upload --upload-port /dev/cu.usbmodem21101
```

首次接车前先连接 Dashboard，确认 `/status.capabilities.singleCan=true`、`/status.capabilities.can2Available=false`，并确认 FSD 注入处于关闭状态。
```

- [ ] **Step 2: Add README entry**

In `README.md`, add this subsection under Quick Start or hardware specs:

```markdown
### Waveshare 单 CAN 独立版

微雪 ESP32-S3-RS485-CAN 请使用 `waveshare_single_can_standalone` 环境。该版本只使用单路 TWAI CAN（GPIO15 TX / GPIO16 RX），保留 FSD 激活、速度策略、驾驶风格、网络设置、OTA 升级和单 CAN 诊断；CAN2、MCP2515、Bus2、Service Mode、Stalk/highbeam、灯光特技入口会按 capabilities 隐藏或不注册。

```bash
pio run -e waveshare_single_can_standalone
```

AP Injection Gate 安全策略对齐 `ev-open-can-tools v3.0.2-beta.3`：Legacy `0x3EE mux0` 在 gate 开启时必须等待 AP active 连续 2 秒；unknown/invalid/SNA gear 默认 fail-closed。该策略降低 2026.8.3.6 相关误注入风险，但不承诺全车型直接兼容。
```

- [ ] **Step 3: Add changelog entry**

At the top of `CHANGELOG.md`, add:

```markdown
## Unreleased

- Added `waveshare_single_can_standalone` profile for Waveshare ESP32-S3-RS485-CAN single-CAN deployments.
- Added Dashboard capabilities for single-CAN UI/API pruning: CAN2, Bus2, Service Mode, Stalk/highbeam, and lighting-stunt surfaces are hidden or unregistered in standalone builds.
- Added AP Injection Gate safety parity with upstream `ev-open-can-tools v3.0.2-beta.3`: Legacy `0x3EE mux0` requires AP active for 2 seconds when the gate is enabled, and live unknown gear fails closed.
```

- [ ] **Step 4: Run docs contract test**

Run:

```bash
python3 -m unittest test.test_dashboard_api_contract.DashboardApiContractTests.test_single_can_docs_are_present -v
```

Expected: PASS.

- [ ] **Step 5: Commit docs**

```bash
git add docs/building.md README.md CHANGELOG.md
git commit -m "docs: document waveshare single can standalone build"
```

---

### Task 8: Run Full Verification Before Firmware Asset Work

**Files:**
- No source changes expected.
- Read outputs from commands.

- [ ] **Step 1: Run Python dashboard contract tests**

Run:

```bash
python3 -m unittest test.test_dashboard_api_contract -v
```

Expected: PASS.

- [ ] **Step 2: Verify dashboard header is current**

Run:

```bash
python3 scripts/minify_dashboard.py --check
```

Expected: PASS with no generated-header drift.

- [ ] **Step 3: Run native helper tests**

Run:

```bash
pio test -e native_helpers -f test_native_helpers
```

Expected: PASS.

- [ ] **Step 4: Run AP gate tests**

Run:

```bash
pio test -e native_injection_after_ap -f test_native_injection_after_ap
```

Expected: PASS.

- [ ] **Step 5: Run dashboard native tests for normal dashboard**

Run:

```bash
pio test -e native_dashboard -f test_native_dashboard
```

Expected: PASS.

- [ ] **Step 6: Run dashboard native tests for standalone dashboard**

Run:

```bash
pio test -e native_single_can_dashboard -f test_native_dashboard
```

Expected: PASS.

- [ ] **Step 7: Build Waveshare standalone firmware**

Run:

```bash
pio run -e waveshare_single_can_standalone
```

Expected: SUCCESS. The compile line must include `DASH_SINGLE_CAN_STANDALONE=1`, `DASH_INITIAL_HW_MODE=3`, `DRIVER_TWAI`, `TWAI_TX_PIN=GPIO_NUM_15`, and `TWAI_RX_PIN=GPIO_NUM_16`.

- [ ] **Step 8: Smoke build existing dual CAN firmware**

Run:

```bash
pio run -e lilygo_t2can_dual
```

Expected: SUCCESS. This guards against breaking the existing dual-CAN product while adding standalone gates.

- [ ] **Step 9: Handle verification failures without hiding them**

If any verification command failed, do not make a broad cleanup commit. Return to the task that introduced the failing behavior, make the smallest fix there, rerun that task's test, and commit using that task's commit message pattern. If every verification command passed, leave the working tree unchanged and continue to Task 9.

---

### Task 9: Generate Firmware Assets and Record Delivery Evidence

**Files:**
- Create: `firmware-assets/waveshare-single-can-standalone_${STAMP}/` from the shell commands in this task.
- Modify: `firmware-assets/LILYGO-T-2CAN_latest_*` only if the existing release script intentionally updates them for this product line.
- Modify: `CHANGELOG.md` if asset checksums are recorded there.
- Test: checksum verification.

- [ ] **Step 1: Inspect existing asset script names**

Run:

```bash
find scripts -maxdepth 1 -type f | sort | grep -E 'asset|firmware|release|merge|sha|flash'
```

Expected: locate the existing asset generation script used by this repository. If multiple scripts match, inspect their help output before running anything.

- [ ] **Step 2: Generate the standalone firmware binary through PlatformIO**

Run:

```bash
pio run -e waveshare_single_can_standalone
```

Expected: `.pio/build/waveshare_single_can_standalone/firmware.bin` exists.

- [ ] **Step 3: Create a dated asset directory**

Run:

```bash
STAMP=$(date +%Y%m%d-%H%M)
ASSET_DIR="firmware-assets/waveshare-single-can-standalone_${STAMP}"
mkdir -p "$ASSET_DIR"
cp .pio/build/waveshare_single_can_standalone/firmware.bin "$ASSET_DIR/firmware-waveshare-single-can-standalone-ota.bin"
shasum -a 256 "$ASSET_DIR/firmware-waveshare-single-can-standalone-ota.bin" | tee "$ASSET_DIR/SHA256SUMS"
```

Expected: `SHA256SUMS` contains one SHA256 line for the OTA app binary.

- [ ] **Step 4: Generate full-flash only if the existing repo script supports it**

If the existing script supports merged/full-flash images for ESP32-S3 16MB, run that script with `waveshare_single_can_standalone` as the env. The output file name must contain `waveshare-single-can-standalone-full-flash-16mb.bin`.

If no script supports this env yet, create a follow-up task instead of hand-writing unknown flash offsets. Do not invent `esptool.py merge_bin` offsets without checking the current project script.

- [ ] **Step 5: Verify checksums**

Run:

```bash
shasum -a 256 -c "$ASSET_DIR/SHA256SUMS"
```

Expected: `OK` for every listed asset.

- [ ] **Step 6: Commit assets only after checksum verification**

```bash
git add "$ASSET_DIR"
git commit -m "build: add waveshare single can standalone firmware asset"
```

If assets are too large or the project policy says not to commit them, skip the commit and record the asset path and SHA256 in the final report.

---

### Task 10: Final Review and Handoff

**Files:**
- Modify: `memory.md` in project root if implementation decisions differ from the spec.
- Read: `git status`, test outputs.

- [ ] **Step 1: Check final working tree**

Run:

```bash
git status --short
```

Expected: only intentional uncommitted files, or clean working tree if all changes were committed.

- [ ] **Step 2: Review commits on the feature branch**

Run:

```bash
git log --oneline --decorate -10
```

Expected: commits show test-first progression for standalone contracts, env, capabilities, UI pruning, AP gate parity, docs, and assets if assets were committed.

- [ ] **Step 3: Summarize verification evidence**

Prepare a final report with these exact items:

```markdown
## Waveshare Single CAN Standalone Verification

- Branch: feature/waveshare-single-can-standalone
- Env: waveshare_single_can_standalone
- Dashboard capabilities: singleCan=true, can2Available=false
- Python contract tests: PASS
- minify_dashboard.py --check: PASS
- native_helpers: PASS
- native_injection_after_ap: PASS
- native_dashboard: PASS
- native_single_can_dashboard: PASS
- pio run -e waveshare_single_can_standalone: PASS
- pio run -e lilygo_t2can_dual: PASS
- Firmware assets: include the exact asset directory path and the complete `SHA256SUMS` output block below this list.
- Safety note: AP gate aligns with ev-open-can-tools v3.0.2-beta.3; 2026.8.3.6 full-model compatibility is not claimed.
```

Immediately below the report list, paste the literal output of `cat "$ASSET_DIR/SHA256SUMS"`. If no assets were generated, state `Firmware assets: not generated in this session` and omit the checksum block.

- [ ] **Step 4: Update project memory if implementation changed the design**

If the implementation chose a different env name, AP gate behavior, asset location, or testing boundary than the spec, update `/Users/ziwind/my-vibe-project/memory.md` with a short note under recent updates.

- [ ] **Step 5: Stop before merge or push**

Do not merge, push, or flash hardware unless Jordan explicitly asks for that next step.

---

## Self-Review Notes

- **Spec coverage:** Tasks cover the standalone profile, capabilities, UI/API pruning, AP gate beta.3 parity, retained FSD/speed/drive/network/OTA behavior, tests, docs, assets, and final verification evidence.
- **Placeholder scan:** The implementation steps use exact env names, file paths, commands, and code snippets. The only final-report SHA256 values are explicitly replaced by actual command output during Task 10.
- **Type consistency:** The plan consistently uses `waveshare_single_can_standalone`, `DASH_SINGLE_CAN_STANDALONE`, `DASH_INITIAL_HW_MODE`, `/status.capabilities`, `legacyFsdActivationAllowed`, and `FsdGateBlockReason::LegacyFsdSettle`.
