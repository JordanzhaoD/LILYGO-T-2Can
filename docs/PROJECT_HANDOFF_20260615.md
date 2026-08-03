# 项目交接文档 — LILYGO T-2CAN 固件 · 微雪单 CAN 独立版

> 生成时间：2026-06-15  
> 生成时 HEAD：`46e1ce9`（feature/waveshare-single-can-standalone 分支）  
> 用途：把此文档粘贴进新的 AI 聊天窗口，新 AI 应能无缝接手继续开发。  
> 不确定的信息已标注「不确定」，未编造。

---

## 1. 项目基本信息

- **项目名称**：LILYGO T-2CAN — Tesla Model Y FSD CAN Dashboard 固件；当前开发的是其**微雪单 CAN 独立版分支**。
- **项目目标**：为「微雪 ESP32-S3-RS485-CAN」单 CAN 硬件产出一个**长期可维护、与上游低成本同步**的独立固件产物，同时**不破坏**双 CAN 主项目（LILYGO T-2CAN）。
- **这个项目最终要解决什么问题**：
  - 特斯拉 Model Y FSD（Full Self-Driving）功能在 CAN 总线上的解锁/控制（车速、AP/Autopilot 注入、Summon 等）。
  - 通过板载 WiFi 热点提供浏览器 Web 控制面板（无需手机 App）。
  - 本次分支新增：**官方 JSON 插件引擎**（用户可安装/管理 CAN 帧规则插件）、**AP Gate 增强**（延迟注入 / AP-AUTO-RESTORE / 注入来源标识）、**单 CAN UI 裁剪**（隐藏 CAN2/灯光/自动换挡等双 CAN 专属表面）。
- **当前开发阶段**：正在执行 **2026-06-15 的 14 任务实施计划**（UI + 插件引擎 + 审计），**Task 1–8 已完成并提交，Task 9 进行中（mobile UI 对齐，尚未提交），Task 10–14 待做**。
- **技术栈**：
  - 语言：C++17（固件）、Python 3（测试脚本）、JavaScript/HTML/CSS（内嵌 Web UI）。
  - 框架：**ESP-IDF**（`framework = espidf`，`framework-espidf @ 4.60001.0`），通过 PlatformIO 构建。
  - 构建/测试：PlatformIO Core（`pio`）、Python `unittest`。
- **运行环境**：嵌入式，ESP32-S3。开发机 macOS（Darwin 25.5.0，zsh）。
- **主要依赖**：ESP-IDF 组件（TWAI、NVS、SPIFFS、esp_http_server、WiFi、OTA）；详见 `dependencies.lock` 与 `managed_components/`。无 npm/pip 运行时依赖（Python 测试只用标准库 `unittest`）。
- **项目所在目录**：`/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware`
- **当前分支或版本状态**：
  - 分支：`feature/waveshare-single-can-standalone`
  - VERSION：`4.1.0`
  - **未 push、未 merge 到 main、未烧录硬件**（硬性约束，见下）。
- **是否有线上部署**：否。这是设备端固件，无云端。设备运行时提供本地 WiFi AP Web 面板。
- **是否涉及数据库**：否。持久化用 ESP32 NVS（键值存储）+ SPIFFS（插件 JSON 文件）。
- **是否涉及第三方 API**：无云 API。Web UI 可由用户粘贴插件 JSON 的 **URL**（如 raw.githubusercontent.com）让设备去 fetch，但那是设备运行时行为，非构建期依赖。
- **是否涉及敏感配置或密钥**：
  - 源码中**无硬编码密钥/API Key/Token**。
  - 运行时敏感项：WiFi AP 的 SSID/密码（用户在设备 Web 面板配置，存 NVS）—— **[REDACTED 概念，由用户在板上设置，源码无值]**。
  - OTA 更新可能涉及签名校验机制（**不确定**，未深入）。

---

## 2. 当前项目进度

### 重要背景：两组任务计划（不要混淆）

仓库里有**两份**微雪单 CAN 计划：

1. **`2026-06-14-waveshare-single-can-standalone.md`（10 任务）** —— 已**全部完成**。这是**基础层**：建立了 `waveshare_single_can_standalone` 构建入口、`/status.capabilities` 能力开关（`singleCan=true`/`can2Available=false`…）、AP Gate 对齐上游 `ev-open-can-tools v3.0.2-beta.3`（Legacy `0x3EE mux0` 须 AP active 连续 2 秒、gear fail-closed）。
2. **`2026-06-15-waveshare-single-can-ui-plugin-audit.md`（14 任务，当前）** —— **Task 1–8 完成，Task 9 进行中**。这是在基础层之上做 UI 对齐 + 官方插件引擎 + 审计。

**当前执行的是第 2 份（2026-06-15 的 14 任务）。** 详见 `docs/superpowers/plans/2026-06-15-waveshare-single-can-ui-plugin-audit.md`（2623 行）。

### 14 任务进度总表

| Task | 内容 | 状态 | Commit |
|------|------|------|--------|
| 1 | RED 契约测试（standalone AP/plugin/mobile） | ✅ 完成 | `1eeb396` |
| 2 | RED native 插件引擎测试 + PlatformIO env | ✅ 完成 | `a8e2f3e` |
| 3 | 插件模型 + 解析器 + status 序列化 | ✅ 完成 | `383bc72` |
| 4 | 插件执行 + 优先级冲突 + 安全门控 + GTW2047 重放 | ✅ 完成 | `29fd9cb` |
| 5 | AP 延迟 + auto-restore 后端配置/status | ✅ 完成 | `68d9d72` + `0d7f557` |
| 6 | 插件引擎接入 dashboard 路由 + SPIFFS 存储 | ✅ 完成 | `a71a1bf` + `442afff` |
| 7 | 插件引擎接入 CAN 帧循环（避免重复 FSD 注入） | ✅ 完成 | `1a29627` |
| 8 | desktop UI AP 控件 + 官方插件管理器 | ✅ 完成 | `46e1ce9` |
| 9 | mobile UI 对齐 AP/plugin | 🔄 **进行中（代码未提交）** | — |
| 10 | 清理 standalone 可见的 CAN2/T-2CAN/自动换挡表面 | ⏳ 待做 | — |
| 11 | 加固插件存储持久化（SPIFFS 文件格式） | ⏳ 待做 | — |
| 12 | 官方插件 + 单 CAN AP 文档 | ⏳ 待做 | — |
| 13 | 全面审计与验证 | ⏳ 待做 | — |
| 14 | 最终评审交接 + memory 更新 | ⏳ 待做 | — |

### 按模块拆解

#### 模块 A：插件引擎（Task 2–7、11）—— 本次开发的核心新功能
- **状态**：核心已完成（Task 2–7），持久化加固待做（Task 11）。
- **相关文件**：
  - `include/dash_plugin_engine.h`（header-only 引擎：模型/解析/执行/优先级/安全门/GTW2047 重放/emit_periodic 缓存/JSON status）
  - `include/web/mcp2515_dashboard.h`（路由 `/plugins/*` + SPIFFS 存储 + `dashPluginContext()` + `dashPluginOwnsFsdActivation()`）
  - `include/handlers.h`（`pluginOwnsFsdActivation` 回调 + `builtInFsdInjectionAllowed()`）
  - `include/app.h`（`appLoop` 里 `applyToFrame` + `tickPeriodic`）
  - `test/test_native_plugin_engine/test_plugin_engine.cpp`（23 个 native 测试，全过）
- **核心逻辑**：用户通过 Web UI 安装 JSON 插件（URL/粘贴/上传），引擎按规则（CAN id + mux + send/ops）在帧循环中执行；插件可拥有 FSD 帧（1006/1021/2047）时抑制 built-in 注入避免重复；GTW2047 帧支持 1–5x 重放并自动推进 counter/checksum。
- **关键常量**：`kDashPluginMaxPlugins=8`、`kDashPluginMaxRules=16`、`kDashPluginMaxOps=16`、`kDashPluginMaxJsonBytes=8192`、replay 1–5。
- **当前问题**：
  - Task 6 SPIFFS 持久化是**简化存根**（`dashSavePluginState`/`dashLoadPluginState`），上传缓冲区有限、JSON 转义、load 未完整实现 —— **Task 11 会加固**。
  - `dashPluginContext()` **必须保持无副作用**（见模块 D 安全门），Task 7 已修复。

#### 模块 B：AP Gate 增强（Task 5、8、9）—— 延迟注入 / AP-AUTO-RESTORE / 注入来源
- **状态**：后端完成（Task 5），desktop UI 完成（Task 8），mobile UI 进行中（Task 9）。
- **相关文件**：`include/web/mcp2515_dashboard.h`（`dashClampApDelayMs`、`dashApInjectionStateName`、`/config` 的 `ap_delay_ms`/`ap_auto_restore`、`/status` 的 `apDelayMs`/`apInjectionState`/`apAutoRestore`/`injectionSource`）；`include/platform/espidf_runtime.h` + `src/espidf_runtime.cpp`（`Preferences::putUInt/getUInt`）。
- **核心逻辑**：AP 延迟状态机 `blocked/waiting_ap/settling/injecting`；`legacyFsdRequiredStableMs` 可配（0–3000ms，默认 2000）。
- **当前问题**：无。已验证（native_injection_after_ap 10/10 + contract test）。

#### 模块 C：单 CAN UI 裁剪（Task 8 desktop 已做、Task 9 mobile 进行中、Task 10 清理待做）
- **状态**：desktop AP/插件卡片已加（Task 8）；mobile 对齐进行中（Task 9）；CAN2/灯光/自动换挡表面清理待做（Task 10）。
- **相关文件**：`include/web/mcp2515_dashboard_ui.src.h`（UI 源，3567 行）、`include/web/mcp2515_dashboard_ui.h`（minified 生成物）、`scripts/minify_dashboard.py`。
- **核心逻辑**：用 `singleCan` 能力开关 + `applyProductMode()` + `data-product-name`/`data-single-hide`/`data-dual-hide`/`data-single-text` 属性，运行时按 `CAP.singleCan` 切换 dual/standalone 表面。
- **当前问题（Task 8 的已知偏差，已审查通过）**：
  - `ap-core-card` 用 `ap-core-*` ID 前缀（避免与现有 line ~838 的 `ap-gate-tgl`/`saveApGate()` 冲突）；`ap-delay-select`/`ap-auto-restore-tgl` 保持原样（契约测试要求确切 ID）。
  - Task 8 收窄了 `test_ota_upload_matches_espidf_raw_upload_handler`：允许 `/plugins/upload` 用 `FormData`（multipart），仍禁止 OTA `/update` 用 FormData（保持 raw octet-stream）。**这是正确演进，非削弱**。

#### 模块 D：双 CAN 主项目保护（贯穿所有任务的硬约束）
- **状态**：持续验证中（每个 Task 都编译 `lilygo_t2can_dual` 确认不破坏）。
- **核心逻辑**：`DASH_PLUGIN_ENGINE` 仅在 `waveshare_single_can_standalone` 定义（platformio.ini:204）。双 CAN 不定义它 → `dashPluginOwnsFsdActivation()` 走 `#else` 返回 false → `builtInFsdInjectionAllowed()` 永远 true → FSD 注入不被抑制 → 双 CAN 行为完全不变。**CAN2/MCP2515/Service/Stalk/Lighting/Burst 源码保留隔离，不删**。
- **当前问题**：无。Task 1–8 每步都验证 dual 编译 SUCCESS。

#### 模块 E：测试（贯穿全程）
- **状态**：native 测试全过（plugin_engine 23/23、injection_after_ap 10/10）；契约测试除 Task 9/10 的 3 个预期 RED 外全过。
- **预期 RED（未来任务，非回归）**：
  - `test_mobile_single_can_ui_matches_ap_and_plugin_capabilities`（Task 9）
  - `test_single_can_product_ui_hides_dual_can_surfaces`（Task 10）
  - `test_generated_dashboard_header_contains_task1_ui_contract_tokens`（mob-plugin-page token，Task 9）

### 哪些方案已证明不可行 / 不要让新 AI 重复尝试
1. **不要给子代理指定 `model` 参数**：会触发 `502 unknown provider for model gpt-5.5`。省略 `model` 继承主循环模型（当前 GLM-5.2）即可。
2. **不要用 `putULong`/arduino 的 `Preferences`**：项目在 `espidf_runtime.h` 有**自定义 Preferences 类**（非 arduino-esp32 的）。Task 5 起初用 `putULong` 失败，根因是自定义类只有 `putBool/putUChar/putChar/putString`。已用 nvs_set_u32 加 `putUInt/getUInt` 修复（commit `442afff`）。
3. **不要信 `compile_commands.json` 核对 `-D` 宏**：ESP-IDF 经 CMake 注入 build_flags，compile_commands.json 不含。核对宏是否生效要 `strings firmware.bin`（如查 `/lighting_config` 是否被 `#if !defined(DASH_SINGLE_CAN_STANDALONE)` 守卫掉）。
4. **不要用 `test.test_*` dotted 路径跑测试**：`test/` 无 `__init__.py`，必须 `PYTHONPATH=test python3 -m unittest test_dashboard_api_contract...`。
5. **Python 单引号里 `'\"x\"'` 不是反斜杠引号**：会解析成纯引号。匹配 C++ 字面量 `\"apDelayMs\"` 必须用原始字符串 `r'\"apDelayMs\"'`（Task 5 踩过，commit `0d7f557`）。
6. **不要删 CAN2/灯光/自动换挡源码**：只隔离/隐藏，双 CAN 仍要用。

---

## 3. 文件结构说明

### 顶层
- `platformio.ini`（351 行）—— 所有构建环境定义。**最重要**。关键 env 见第 6 节。
- `src/main.cpp` —— PlatformIO 入口。根据 `DRIVER_T2CAN_DUAL` 走双 CAN，否则单 CAN。include `app.h`。
- `src/espidf_runtime.cpp` —— 自定义 ESP-IDF 平台抽象实现（Preferences/String/WebServer/SPIFFS/WiFi/HTTPClient/Update 等）。
- `VERSION` —— `4.1.0`
- `README.md` —— 项目说明（中英）。说明这是 Tesla Model Y FSD CAN Dashboard，双 CAN（CAN1 TWAI 做 FSD/速度，CAN2 MCP2515 做维修/灯光）。
- `sdkconfig.*` —— ESP-IDF 各 env 的 sdkconfig（`sdkconfig.waveshare_single_can_standalone`、`sdkconfig.lilygo_t2can_dual` 等）。
- `platformio_profile.h` —— 共享构建设置（被 main.cpp include）。
- `partitions_16mb_ota_4096k_nvs64.csv` —— 16MB flash 分区表。

### include/（核心逻辑，多为 header-only）
- **`include/app.h`** —— **CAN 主循环**。`appLoop<Driver>()` template：读帧 → `h->handleMessage(frame)` → **（Task 7）插件 `applyToFrame`** → `dashPostProcessFrame` → 循环末尾 **（Task 7）`tickPeriodic`**。
  - 改这里要注意：`applyToFrame`/`tickPeriodic` 在 `#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_PLUGIN_ENGINE)` 守卫内；`original` 帧变量在该守卫外定义（更宽的 `ESP32_DASHBOARD && !NATIVE_BUILD`）。
- **`include/handlers.h`** —— **FSD 注入逻辑**。`CarManagerBase`（基类，含回调 `legacyFsdActivationAllowed`、Task 7 的 `pluginOwnsFsdActivation` + `builtInFsdInjectionAllowed()`）+ `LegacyHandler`/`HW3Handler`/`HW4Handler`。
  - 改这里要注意：**双 CAN 共享此类**。任何新回调必须默认 nullptr，使双 CAN 行为不变。FSD 发送路径（Legacy mux0/mux1 ~line 406/455、HW3 ~589/602/611、HW4 ~935/947/957）都已加 `builtInFsdInjectionAllowed()` 守卫。
- **`include/dash_plugin_engine.h`** —— **插件引擎**（header-only）。`DashPluginEngine`（`installJson`/`applyToFrame`/`tickPeriodic`/`hasEnabledRuleFor`/`remove`/`statusJson` 等）+ 数据结构 `DashPlugin`/`DashPluginRule`/`DashPluginContext`/`DashPluginApplyResult`。
  - 改这里要注意：`plugins_` 是 `std::vector`（用 `push_back`）；`statusJson()` 返回 `DashPluginStatusString`（公共 std::string 子类）。
- **`include/dash_capabilities.h`** —— 能力开关定义（`singleCan`/`can2Available`/`lightingBusSupported` 等）。
- **`include/dash_fsd_diag.h`** —— FSD 诊断结构（`LegacyFsdDiag`、`FsdSkipReason`、`FsdHealthState`、`FsdGateBlockReason`）。
- 其他功能头：`dash_bionic_steer.h`、`dash_fog_light.h`、`dash_hw3_speed.h`、`dash_legacy_speed.h`、`dash_ota_guard.h`、`dash_power_mgmt.h`、`dash_twai_diag.h`、`dash_tx_evidence.h`、`dash_wheel_dnd.h`、`log_buffer.h`、`lwip_hooks.h`、`can_frame_types.h`、`can_helpers.h`、`shared_types.h`。

### include/web/（Web 服务 + UI）
- **`include/web/mcp2515_dashboard.h`** —— **Web 服务 + dashboard 业务**（~6700+ 行，最大文件）。`WebServer` 路由、`/status`、`/config`、`/plugins/*`、handler pool（`handlerPool[3]` ~line 6503）、`dashApplyRuntimeState`、`dashPluginContext()`、`dashPluginOwnsFsdActivation()`、AP 延迟状态机 `dashApInjectionStateName()`、安全门 `dashApInjectionAllowed()` 等。
  - 改这里要注意：`dashPluginContext()`（~line 969）**必须无副作用**——用只读的 `legacyFsdLastAllowed`（~line 178），**不要**调用有副作用的 `dashLegacyFsdActivationAllowed(millis())`（会重置 settle timer，破坏 legacy FSD 稳定逻辑）。
- **`include/web/mcp2515_dashboard_ui.src.h`** —— **UI 源**（3567 行，可读）。HTML+CSS+JS 内嵌。Task 8 加了产品名/`applyProductMode`/ap-core-card/plugins-card/插件 JS。
- **`include/web/mcp2515_dashboard_ui.h`** —— **minified UI**（由 `scripts/minify_dashboard.py` 从 src.h 生成）。**不要手编**，改 src.h 后跑 minify。
- `include/web/dash_gateway.h` —— 网关相关。

### include/platform/
- **`include/platform/espidf_runtime.h`** —— 自定义 ESP-IDF 平台抽象声明（`Preferences` 类含 Task 5 加的 `putUInt/getUInt`、`String`、`WebServer`、`SPIFFSClass`、`WiFiClass`、`HTTPClient`、`UpdateClass` 等）。

### test/
- **`test/test_dashboard_api_contract.py`** —— **UI/后端契约测试**（Python unittest，读 ui.src.h 文本 + 后端 JSON 字段断言）。`setUpClass` 读 `DASH_HTML`（src.h 原始文本，含 C++ 转义）。
  - 改这里要注意：token 转义陷阱（见第 7 节）；测试用 `--single-can` profile 区分。
- `test/test_native_plugin_engine/test_plugin_engine.cpp` —— 插件引擎 native C++ 测试（23 个，全过）。
- `test/test_native_injection_after_ap/test_injection_after_ap.cpp` —— injection-after-AP 测试（10 个，全过）。
- 其他 native 测试目录：`test_native_legacy`/`test_native_hw3`/`test_native_hw4`/`test_native_nag`/`test_native_helpers`/`test_native_dashboard`/`test_native_twai`/`test_native_bionic_steer`/`test_native_wheel_dnd`/`test_native_log_buffer`/`test_native_mcp2515_recovery`/`test_native_bypass_tlssc_requirement`。

### scripts/
- **`scripts/minify_dashboard.py`** —— UI minifier（src.h → ui.h）。支持 `--check`（验证 ui.h 是最新 minify 结果）。
- `scripts/platformio_set_profile.py` / `platformio_sync_profile.py` / `platformio_native_env.py` —— PlatformIO profile/env 管理。
- `scripts/update_ota_build_timestamp.py`、`scripts/check_release_metadata.py`、`scripts/webui_simulator.py`、`scripts/ev_can_analyzer.py`、`scripts/crack_stalk_crc.py`。

### docs/
- `docs/superpowers/plans/2026-06-15-waveshare-single-can-ui-plugin-audit.md` —— **当前 14 任务计划**（2623 行）。
- `docs/superpowers/plans/2026-06-14-waveshare-single-can-standalone.md` —— 上一个 10 任务计划（已完成）。
- `docs/superpowers/specs/2026-06-15-waveshare-single-can-ui-plugin-audit-design.md` —— 当前计划的设计规格。
- `docs/superpowers/specs/2026-06-14-waveshare-single-can-standalone-design.md` —— 基础层设计。
- 其余 plans/specs 是历史 FSD/dashboard 迭代。

### 固件资产（与本次开发无关，勿动）
- `firmware-assets/`（带连字符）—— 上游/历史固件资产。
- `firmware_assets/`（带下划线）—— 双 CAN 固件资产（git status 显示有未跟踪/已修改的二进制，**与单 CAN 14 任务无关**）。
- `legacy-arduino/`、`managed_components/`、`designs/`、`dist/`、`BaAlpha/`（顶层有，**不确定用途**）。

---

## 4. 核心逻辑说明

### 整体运行流程（设备上电后）
1. `main.cpp` → 根据编译 env 选择 driver（TWAI / MCP2515 / 双 CAN）→ `appSetup()` 初始化 handler + CAN driver + WiFi AP + WebServer。
2. 主循环 `appLoop()`：
   - 读 CAN 帧（`appDriver->read(frame)`）。
   - `h->handleMessage(frame)` —— handler 判断帧类型，若是 FSD 激活帧（1006/1021/2047），按安全门控决定是否注入：
     - **安全门 fail-close 顺序**：`canActive` → `otaAllowed`（OTA 进行中禁注入）→ `apGateAllowed`（AP Gate）→ `fsdMasterEnabled`。任一不满足即 `GateBlocked`，blockedBy token 记录 `can_active`/`ota`/`ap_gate`/`fsd_master`。
     - `builtInFsdInjectionAllowed()` —— 若插件拥有 FSD 帧（`pluginOwnsFsdActivation` 回调返回 true），抑制 built-in 注入（避免重复）。
   - **（Task 7）** 插件引擎 `applyToFrame(original, dashPluginContext(), driver)` —— 按插件规则修改/发送帧；若发送了更新 `lastInjectMs`。
   - `dashPostProcessFrame(original, driver)` —— 后处理。
   - 循环末尾 **（Task 7）** `tickPeriodic(nowMs, ctx, driver)` —— 插件周期性 emit（带缓存）。
3. WebServer（esp_http_server）并行处理 HTTP：`/status`（JSON 状态）、`/config`（设置）、`/plugins/*`（插件 CRUD）、`/update`（OTA）等。UI（ui.h 内嵌 HTML）由浏览器加载，`poll()` 周期 fetch `/status` 渲染。

### AP Gate 状态机（`dashApInjectionStateName()`）
- `blocked` → `waiting_ap`（等 AP active）→ `settling`（AP active 稳定计时中，须连续 `legacyFsdRequiredStableMs` 默认 2000ms）→ `injecting`（稳定后允许注入）。
- 任一环节断开（AP disengage / gear unknown/invalid/SNA）→ 回 `blocked` 并清 timer（fail-closed）。
- **设计原因**：对齐上游 `ev-open-can-tools v3.0.2-beta.3` 的 Legacy `0x3EE mux0` 安全语义，避免 AP 未真正 active 就注入导致危险。

### 插件引擎执行流（Task 3–7）
1. 安装：`installJson(json, enabledAfterInstall)` → 解析 JSON → 校验（name/rules/id/mux/ops）→ 存入 `plugins_`（≤8）。
2. 执行（每帧）：`applyToFrame` 按优先级排序 enabled+compatible 插件 → 匹配 rule（id+mux）→ 执行 ops（setBit/clearBit/setValue/checksum 等）→ 发送；GTW2047 按 `replayCount_`（1–5）重放并推进 counter/checksum。
3. 周期：`tickPeriodic` 处理 `send: periodic` 规则（带缓存，避免重复计算）。
4. 安全：`dashPluginContext()` 镜像 dashboard 注入门（canActive + OTA guard + AP gate + FSD settle），插件只在注入被允许时才 act。
5. 重复避免：插件若声明 1006/1021/2047 规则（`hasEnabledRuleFor`），`dashPluginOwnsFsdActivation()` 返回 true → built-in 注入被抑制。

### 为什么这样设计
- **header-only + 自定义平台抽象**：让同一套逻辑能在 ESP32（espidf）和 native（x86 测试）双编译，`NATIVE_BUILD` 宏切换。
- **能力开关而非删代码**：单 CAN 独立版不删 CAN2/灯光源码（双 CAN 还要用），而是用 `singleCan` 能力开关在 UI/API 层裁剪。降低与上游同步成本。
- **插件引擎仅 standalone**：`DASH_PLUGIN_ENGINE` 只在 standalone 定义，双 CAN 完全不受影响（callback nullptr → 不抑制）。

### 容易出 bug 的地方
1. **`dashPluginContext()` 副作用**（Task 7 已修，但易回退）：每帧调用，必须无副作用。别用 `dashLegacyFsdActivationAllowed()`（重置 timer），用只读 `legacyFsdLastAllowed`。
2. **UI ID 冲突**：Task 8 的 `ap-gate-tgl` 与现存控件冲突，已用 `ap-core-*` 前缀。新加 UI 控件前先 grep 现有 ID。
3. **token 转义**：契约测试断言 C++ 字面量 `\"key\"` 时，Python 要用原始字符串 `r'\"key\"'`。
4. **minify 漂移**：改 src.h 后必须重新 minify，否则 `--check` 失败、ui.h 与 src.h 不一致。
5. **双 CAN 回归**：任何 `handlers.h`/`CarManagerBase` 改动都要编译 `lilygo_t2can_dual` 确认。

### 临时方案 / 技术债
- Task 6 的 SPIFFS 插件持久化是**简化存根**（`dashSavePluginState`/`dashLoadPluginState` 未完整）—— **Task 11 加固**。
- `mcp2515_dashboard.h` ~6700 行单文件，偏大（重构机会，但本次不动）。

---

## 5. 环境变量与配置

### 构建/编译期（PlatformIO，非 ENV 变量）
本项目是嵌入式固件，**不使用传统 `.env` 环境变量**。配置通过：
- `platformio.ini` 的 `build_flags`（编译期宏）。
- `sdkconfig.*`（ESP-IDF 配置）。
- 运行时 NVS（设备 Web 面板配置）。

### 关键编译期宏（`-D` flags）
- `DASH_SINGLE_CAN_STANDALONE=1` —— 单 CAN 独立版标志（仅 standalone env）。
- `DASH_PLUGIN_ENGINE=1` —— 启用插件引擎（仅 standalone env，line 204）。
- `DASH_DEFAULT_HW` / `DASH_INITIAL_HW_MODE` —— 默认硬件模式（standalone=3=Auto）。
- `ESP32_DASHBOARD` —— 启用 dashboard Web 功能（所有 ESP32 env）。
- `NATIVE_BUILD` —— native 测试构建（禁硬件、启用 x86 测试桩）。
- `DRIVER_T2CAN_DUAL` —— 双 CAN（TWAI + MCP2515）。
- `DRIVER_TWAI` —— 用 ESP32 TWAI 驱动。
- 业务开关：`ENHANCED_AUTOPILOT`、`ISA_SPEED_CHIME_SUPPRESS`、`EMERGENCY_VEHICLE_DETECTION`、`NAG_KILLER`、`INJECTION_AFTER_AP` 等。

### 运行时配置（设备 NVS，非源码）
- WiFi AP SSID / 密码 —— **[REDACTED，用户在 Web 面板设置]**。
- `ap_gate`、`ap_dly`（AP 延迟 ms）、`ap_rst`（AP-AUTO-RESTORE）、`boot_can`、`hw`、各种 speed/defense 开关。

### 配置文件检查结果
- `.env` / `.env.example`：**本项目（LILYGO-T-2Can-firmware）内不存在**。（注：父目录 `my-vibe-project` 有 `.env.example`，属另一个 Docker/Traefik 项目，与本固件无关。）
- `config`/`settings`：无（用 NVS + sdkconfig）。
- 数据库连接配置：无。
- 代理配置：无。
- 部署配置：无云部署。

---

## 6. 启动、运行、测试方式

> 平台：macOS + zsh + PlatformIO Core（`pio`）+ Python 3。  
> 工作目录：`/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware`

### 安装依赖
- PlatformIO：`pip install platformio`（**不确定是否已装**，若 `pio` 命令存在则已装）。
- Python 测试：仅用标准库 `unittest`，无需 pip install。
- ESP-IDF 组件：`pio` 首次构建自动拉取（`managed_components/`）。

### 测试命令（已验证可用）

**native 插件引擎测试**（Task 2–7 验证用）：
```bash
pio test -e native_plugin_engine          # 期望 23/23 PASS
pio test -e native_injection_after_ap     # 期望 10/10 PASS
```

**Python 契约测试**（UI/后端断言，**必须 PYTHONPATH=test**）：
```bash
PYTHONPATH=test python3 -m unittest test_dashboard_api_contract 2>&1 | tail -20
# 或单测：
PYTHONPATH=test python3 -m unittest \
  test_dashboard_api_contract.DashboardApiContractTests.test_single_can_driving_status_exposes_ap_gate_delay_and_restore \
  test_dashboard_api_contract.DashboardApiContractTests.test_official_plugin_manager_contract_is_present
```

### 构建命令（编译，不烧录）

**单 CAN 独立版**（本次开发目标）：
```bash
pio run -e waveshare_single_can_standalone   # ~20s，期望 SUCCESS
```

**双 CAN 主项目**（每次改动后必须验证不破坏）：
```bash
pio run -e lilygo_t2can_dual                  # ~40s，期望 SUCCESS
```

### UI minify（改 src.h 后必须跑）
```bash
python3 scripts/minify_dashboard.py           # 生成 ui.h
python3 scripts/minify_dashboard.py --check    # 验证 ui.h 是最新（返回 0 = OK）
```

### 完整一次验证（Task 完成前必跑）
```bash
pio test -e native_plugin_engine 2>&1 | tail -4
pio test -e native_injection_after_ap 2>&1 | tail -4
pio run -e waveshare_single_can_standalone 2>&1 | tail -4
pio run -e lilygo_t2can_dual 2>&1 | tail -4
python3 scripts/minify_dashboard.py --check
```

### 烧录（**当前计划禁止！待 Task 14 后由 Jordan 决定**）
```bash
# 仅记录命令格式，现在不要执行：
pio run -e waveshare_single_can_standalone -t upload   # 烧录
# 或用 esptool.py 逐分区烧录 merged.bin
```

### 常见启动/构建失败原因
1. `pio test` 报找不到模块 → 漏了 `PYTHONPATH=test`。
2. `putUInt`/`getUInt` 编译错 → 已修复（commit `442afff`），若再现说明回退了平台层。
3. `minify --check` 失败 → 改了 src.h 没 re-minify。
4. 双 CAN 编译失败 → 改了 `handlers.h`/共享代码破坏了双 CAN 路径。
5. 契约测试 token 不匹配 → C++ 转义 `\"` vs Python 字符串（用 `r'...'` 原始字符串）。
6. `pio` 太慢/卡 → ESP-IDF 首次构建慢，正常。

### 启动成功后应该看到
- `pio run` 末尾 `[SUCCESS]` + Flash/RAM 占用百分比。
- `pio test` 末尾 `N test cases: N succeeded`。
- 设备烧录后（未来）：串口 `Serial.println(readyMsg)` + WiFi AP 起来，浏览器连 AP 打开 Web 面板。

---

## 7. 已知问题与坑

### 问题：子代理调用 502 unknown provider for model gpt-5.5
- **表现**：调度 subagent 时若指定 `model` 参数，报 502。
- **原因**：模型 ID 解析失败。
- **有效方案**：**省略 `model` 参数**，继承主循环模型（GLM-5.2）。
- **相关**：所有 Agent/Task 调用。

### 问题：自定义 Preferences 缺 putUInt/getUInt（Task 5，已修）
- **表现**：`prefs.putUInt` 编译失败。
- **原因**：`espidf_runtime.h` 的自定义 `Preferences` 类只有 putBool/putUChar/putChar/putString。
- **有效方案**：加 `putUInt`/`getUInt`（`nvs_set_u32`/`nvs_get_u32`），commit `442afff`。
- **不要重试**：putULong 也不行，必须加 putUInt。

### 问题：dashPluginContext() 副作用破坏 legacy FSD settle（Task 7，已修）
- **表现**：若 `dashPluginContext()` 调用 `dashLegacyFsdActivationAllowed(millis())`，每帧重置 settle timer，AP 永远无法稳定 2 秒。
- **原因**：该函数有副作用（重置 `legacyFsdApActiveSinceMs`、写 `legacyFsdLastAllowed`）。
- **有效方案**：改为只读 `dashApInjectionAllowed() && legacyFsdLastAllowed`。
- **相关**：`mcp2515_dashboard.h` ~line 974。

### 问题：UI ID 冲突 ap-gate-tgl（Task 8，已修）
- **表现**：计划 Step 3 的 ap-core-card 用 `ap-gate-tgl`，与现存控件（~line 838）重复，getElementById 只返回第一个。
- **有效方案**：ap-core-card 用 `ap-core-*` 前缀；保留旧 `ap-gate-tgl` 给双 CAN。

### 问题：契约测试 token 转义（Task 5，已修）
- **表现**：`'"apDelayMs"'` 不匹配 C++ `\"apDelayMs\"`。
- **原因**：Python 单引号 `'\"x\"'` 解析成纯引号，丢反斜杠。
- **有效方案**：原始字符串 `r'\"apDelayMs\"'`。commit `0d7f557`。

### 问题：ESP-IDF build_flags 不在 compile_commands.json（基础层踩过）
- **表现**：核对 `-D` 宏时 compile_commands.json 找不到。
- **有效方案**：`strings firmware.bin | grep <符号>` 核对（如 `/lighting_config` 是否被守卫掉）。

### 问题：strobe loader 旧契约回归（基础层 Task 5，已修）
- **表现**：UI 加 `&&CAP.lightingBusSupported` 守卫后，strobe loader 契约测试失败。
- **有效方案**：更新断言（commit `31f4866`，基础层计划）。

### 未修但已知的设计债
- Task 6 SPIFFS 插件持久化是简化存根 → Task 11 加固。
- `mcp2515_dashboard.h` 单文件 ~6700 行偏大（未规划重构）。

### 偶发/不确定
- minify 输出可能有微小字节级漂移（子代理报告 re-minify 后稳定，但 git status 当前显示 ui.h 有未提交改动，**来源不确定**，可能是 minify 漂移或 Task 9 预备改动）。

---

## 8. 最近修改记录

### 修改：Task 8 desktop UI AP + 插件管理器（`46e1ce9`）
- **原因**：实现 desktop 端 AP 核心控件 + 官方 JSON 插件管理器。
- **文件**：`mcp2515_dashboard_ui.src.h`、`mcp2515_dashboard_ui.h`、`test_dashboard_api_contract.py`。
- **改了什么**：加 `applyProductMode()`/产品名节点/`ap-core-card`（AP门控+延迟+restore+注入来源）/`plugins-card`（install URL/JSON/upload、enable/priority/remove、GTW2047 replay）/插件 JS 函数（loadPlugins/installPluginUrl/.../renderPluginsStatus/esc）。
- **为什么这么改**：独立版需要可见的 AP 增强控件 + 插件管理 UI。
- **验证**：minify --check PASS、2 个 contract test PASS、standalone + dual 编译 SUCCESS。
- **偏差**：ap-core-* ID 前缀避免冲突；收窄 test_ota_upload 允许 /plugins/upload 用 FormData。
- **可能影响**：standalone 桌面端新 UI；双 CAN 不受影响（新控件独立 ID）。

### 修改：Task 7 插件引擎接入 CAN 循环（`1a29627`）
- **原因**：插件能在帧循环执行，且拥有 FSD 帧时抑制 built-in 避免重复。
- **文件**：`app.h`（applyToFrame + tickPeriodic）、`handlers.h`（pluginOwnsFsdActivation + builtInFsdInjectionAllowed + 守卫）、`mcp2515_dashboard.h`（dashPluginContext 重构为只读 + dashPluginOwnsFsdActivation + handler pool 赋值 + injectionSource status）、`dash_plugin_engine.h`（hasEnabledRuleFor）。
- **关键**：`dashPluginContext()` 改用只读 `legacyFsdLastAllowed`（无副作用）。
- **验证**：native_plugin_engine 23/23 + native_injection_after_ap 10/10 + standalone/dual 编译 SUCCESS。
- **风险点**：双 CAN 安全靠 callback nullptr + 宏未定义，已验证。

### 修改：Task 6 插件路由 + SPIFFS（`a71a1bf` + `442afff`）
- **原因**：插件 CRUD API + 持久化。
- **文件**：`mcp2515_dashboard.h`（9 个 `/plugins/*` 路由 + SPIFFS 存根）、`espidf_runtime.h/.cpp`（putUInt/getUInt 平台修复）。
- **注意**：SPIFFS 持久化是简化存根，Task 11 加固。

### 修改：Task 5 AP 延迟/restore 后端（`68d9d72` + `0d7f557`）
- **原因**：AP 延迟注入（0–3000ms）+ AP-AUTO-RESTORE 可配。
- **文件**：`mcp2515_dashboard.h`（`dashClampApDelayMs`、`/config`、`/status`）、`test_dashboard_api_contract.py`（token 转义修复）。

### 修改：Task 3–4 插件引擎核心（`383bc72` + `29fd9cb`）
- **原因**：插件模型/解析/执行/优先级/安全门/GTW2047 重放。
- **文件**：`dash_plugin_engine.h`。

### 基础层（2026-06-14 的 10 任务，已完成，HEAD `c9743df`）
- 已完成的单 CAN 独立版基础：`waveshare_single_can_standalone` env、能力开关、AP Gate beta.3 parity。详见 `docs/superpowers/plans/2026-06-14-waveshare-single-can-standalone.md` 与 memory `lilygo-waveshare-single-can-standalone-20260615.md`。

---

## 9. 下一步开发计划

### 计划方法论（重要）
- 用 **Subagent-Driven Development**（superpowers）：每任务一个 fresh 实现者子代理 → 规格合规审查 → 代码质量审查 → 修复循环 → 标记完成 → 下一任务。任务间不暂停。
- **子代理调用切记省略 `model` 参数**（否则 502）。
- 每个任务完成后：跑 native 测试 + standalone 编译 + **dual 编译**（验证不破坏双 CAN）+ 契约测试，然后 commit。
- 硬约束：**不 push、不 merge、不 flash**。

### 下一步最优先做什么：Task 9 — mobile UI 对齐 AP/plugin
- **目标**：让 mobile 端 UI 也有 AP 状态卡片 + 插件管理，对齐 desktop（Task 8）。
- **原因**：契约测试 `test_mobile_single_can_ui_matches_ap_and_plugin_capabilities` 当前 RED（缺 mob-plugin-page/m-ap-state/m-ap-gate-tgl/m-ap-delay-select/m-ap-auto-restore-tgl/m-plugin-list/m-plugin-json/m-plugin-replay-count/data-mobile-page="plugins" 等 token）。
- **涉及文件**：`include/web/mcp2515_dashboard_ui.src.h`（改）、`mcp2515_dashboard_ui.h`（minify 生成）、`test/test_dashboard_api_contract.py`（可能已含 RED 测试，让之转 GREEN）。
- **具体步骤**（计划 2017–2113 行）：
  1. 替换 mobile more menu 条目：移除 standalone 可见的 CAN2/strobe/shift，加 home/driving/plugins/network tab（`data-mobile-page="plugins"`）。
  2. 加 mobile AP section（`#mob-driving-page`，含 `m-ap-state`/`m-ap-detail`/`m-ap-gate-tgl`/`m-ap-delay-select`/`m-ap-auto-restore-tgl`）。
  3. 加 mobile plugin section（`#mob-plugin-page`，含 `m-plugin-url`/`m-plugin-json`/`m-plugin-list`/`m-plugin-replay-count`）。
  4. 加 mobile JS：`showMobilePage`/`syncMobileApControls`/`renderMobileApState`/`installMobilePluginUrl`/`installMobilePluginJson`/`renderMobilePlugins`。注意 `syncMobileApControls` 引用 `ap-gate-tgl`/`ap-delay-select`/`ap-auto-restore-tgl`（desktop 控件），与 Task 8 的 `ap-core-*` 前缀有出入 —— **需核对**：Task 8 桌面控件 ID 实际是 `ap-core-gate-tgl` 等，而计划 Task 9 的 `syncMobileApControls` 写的是 `ap-gate-tgl`。需让 mobile 同步到桌面真实 ID（`ap-core-*`）或确认契约测试只查 mobile 自己的 ID。**这是 Task 9 的关键核对点**。
  5. poll() 里调用 `renderMobileApState(d)` + `renderMobilePlugins(j)`（Task 8 已放 `renderMobileApState(d)` 占位调用，需补实现）。
  6. minify + 跑 `test_mobile_single_can_ui_matches_ap_and_plugin_capabilities`，commit `feat: align mobile single can ap plugin ui`。
- **验收标准**：`test_mobile_single_can_ui_matches_ap_and_plugin_capabilities` PASS；minify --check PASS；standalone + dual 编译 SUCCESS。

### 第二优先级：Task 10 — 清理 standalone 可见双 CAN 表面
- **目标**：standalone 模式隐藏 CAN2/灯光/自动换挡导航与页面（不删源码，用 `data-single-hide` + 能力守卫）。
- **原因**：`test_single_can_product_ui_hides_dual_can_surfaces` 当前 RED。
- **步骤**（计划 2117–2203 行）：给 pg-bus2/pg-strobe/pg-shift 加 `data-single-hide="1"` + `data-cap`；standalone overview 标签用 `data-single-text`；隐藏 `m-can2` mobile card；路由调用 JS 加 `!isSingleCan()` 守卫；`showPage` 加 hidden 页重定向。
- **验收**：`test_single_can_product_ui_hides_dual_can_surfaces` + `test_single_can_ui_uses_capabilities_to_hide_can2_and_lighting` PASS；dual 编译/行为不变。

### 第三优先级：Task 11 — 加固插件存储持久化
- **目标**：把 Task 6 的 SPIFFS 简化存根升级为完整 export/import 文件格式（重启后插件不丢）。
- **步骤**（计划 2207+ 行）：`DashPluginEngine` 加 `exportConfigJson()`/`importConfigJson()`；`mcp2515_dashboard.h` 的 `dashSavePluginState`/`dashLoadPluginState` 完整实现；加 native 测试。
- **验收**：native_plugin_engine 测试含持久化用例 PASS。

### 之后：Task 12（文档）、Task 13（全面审计验证）、Task 14（最终评审 + memory 更新）

### 哪些任务不要现在做
- **不要 push / merge / flash** —— 等 Task 14 + Jordan 明确决定。
- **不要重构 `mcp2515_dashboard.h` 大文件** —— 超出本次计划范围。
- **不要删 CAN2/灯光/自动换挡源码** —— 双 CAN 仍要用，只隔离。
- **不要在双 CAN env 加 `DASH_PLUGIN_ENGINE`** —— 会改变双 CAN 行为。

### 哪些功能容易过度开发
- 插件引擎：常量已限定（8 插件/16 规则/16 ops/8192 bytes），不要无限扩展。
- UI：保持简约（产品哲学「能更简单吗」），不要堆功能。
- mobile UI：对齐 desktop 即可，不要做全新的 mobile 设计。

### 哪些地方要先验证再写代码
- 任何 `handlers.h`/`CarManagerBase` 改动 → 先确认 dual 编译不破。
- 任何 src.h UI 改动 → 改完立即 minify + `--check` + 跑相关契约测试。
- 任何 `dashPluginContext()` 相关 → 确认无副作用（用 `legacyFsdLastAllowed` 只读）。
- 子代理调度 → 省略 `model` 参数。

---

## 附：当前 working tree 状态（生成时）

```
分支: feature/waveshare-single-can-standalone（未 push，无 upstream 跟踪显示）
HEAD: 46e1ce9 (Task 8)

未提交改动:
 M include/web/mcp2515_dashboard_ui.h        # minify 漂移或 Task 9 预备（来源不确定）
 M firmware-assets/LILYGO-T-2CAN_latest_*    # 双 CAN 二进制资产，与单 CAN 14 任务无关
?? docs/superpowers/specs/2026-06-02-*.md     # 未跟踪设计文档
?? docs/superpowers/specs/2026-06-09-*.md
?? firmware-assets/v4.1.0_*/                  # 历史资产目录
```

Task 9 尚未开始编码（无 src.h 的 mobile 改动提交）。从 Task 9 继续。

---

## 附：关键命令速查

```bash
# 进入项目
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware

# 测试
pio test -e native_plugin_engine
pio test -e native_injection_after_ap
PYTHONPATH=test python3 -m unittest test_dashboard_api_contract 2>&1 | tail -20

# 编译（单 CAN + 双 CAN 必须都 SUCCESS）
pio run -e waveshare_single_can_standalone
pio run -e lilygo_t2can_dual

# UI minify
python3 scripts/minify_dashboard.py
python3 scripts/minify_dashboard.py --check

# 读当前计划
# docs/superpowers/plans/2026-06-15-waveshare-single-can-ui-plugin-audit.md
```
