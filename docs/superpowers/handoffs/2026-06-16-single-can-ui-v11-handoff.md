# 项目交接文档

## 1. 项目基本信息

- **项目名称：**  
  `LILYGO-T-2Can-firmware` / 单 CAN ATLAS 固件 UI V1.1 重新设计

- **项目目标：**  
  为微雪 `Waveshare ESP32-S3-RS485-CAN` 单 CAN standalone 固件重新设计 Dashboard Web UI：修复 4 个具体 UI 缺陷，并把所有主要页面统一到「驾驶舱玻璃拟态」设计语言。

- **这个项目最终要解决什么问题：**
  1. 手机端底部导航缺少完整模块入口。
  2. 单 CAN 版本驾驶中心仍显示无意义的 CAN2 指标。
  3. 插件管理 / AP 注入安全 UI 风格与驾驶中心割裂，像英文工具面板。
  4. 首页底部有 3 个低价值静态卡片：`维修模式 0x339`、`方向盘免打扰 0x3C2`、`Legacy 功能支持矩阵`。
  5. 车机版 + 手机版整体视觉不统一，需要统一到「驾驶状态中心 / 车辆态势」那套风格。

- **当前开发阶段：**  
  UI V1.1 实施中。计划共 14 个任务，当前已完成 Task 1–5，下一步应从 **Task 6：手机底部导航 5 标签** 继续。

- **技术栈：**
  - ESP32-S3 固件
  - PlatformIO
  - ESP-IDF
  - C++17
  - 嵌入式 Web UI：HTML / CSS / JavaScript 写在 C++ header 的 raw string 中
  - Python `unittest` 做 UI 契约测试
  - ArduinoJson `bblanchon/ArduinoJson@^7`

- **运行环境：**
  - 本地 macOS：Darwin 25.5.0
  - PlatformIO / ESP-IDF
  - 目标硬件：Waveshare ESP32-S3-RS485-CAN，16MB Flash
  - 主构建环境：`waveshare_single_can_standalone`
  - 双 CAN 回归环境：`lilygo_t2can_dual`

- **主要依赖：**
  - `platformio/espressif32@7.0.0`
  - `framework-espidf @ 4.60001.0`
  - `bblanchon/ArduinoJson@^7`
  - Python 3，用于契约测试和脚本
  - `scripts/platformio_sync_profile.py`
  - `scripts/update_ota_build_timestamp.py`
  - `scripts/minify_dashboard.py`

- **项目所在目录：**
  - 当前实际工作目录：`/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware`
  - 注意：会话 Primary working directory 是 `/Users/ziwind/my-vibe-project`，但当前开发对象在子目录 `LILYGO-T-2Can-firmware`。

- **当前分支或版本状态：**
  - 当前分支：`feature/ui-v11-redesign`
  - 工作树 tracked changes：写入本交接文档后会新增本文档文件；写入前 UI 代码 tracked changes 为无。
  - 最近提交：
    - `8cf9974 fix(ui): 插件列表render标签中文化+已装数量pill接线（补全spec全中文）`
    - `1209a5b feat(ui): 插件管理重设计为驾驶舱风格（功能零增减，全中文）`
    - `a0a4903 feat(ui): AP 注入安全块重设计为驾驶舱风格（状态提升+Gate进度条，安全线不变）`
    - `a69b52e fix(ui): AP激活纳入settling态，hint按状态显示（对齐spec：settling=Gate已就绪）`
    - `a4ce131 feat(ui): 驾驶中心 CAN2 指标格 → AP 激活状态`
    - `f1115c7 feat(ui): 删除首页底部冗余3块（维修模式/方向盘免打扰/Legacy矩阵）`
    - `a7763df feat(ui): 设计系统基底 — ds token + .card 驾驶舱玻璃化`
    - `3d4bfe5 docs: 单CAN UI V1.1 重新设计实施计划（14任务，TDD契约驱动）`
    - `c591746 docs: 单CAN UI V1.1 重新设计 spec（5问题修复 + 驾驶舱设计系统统一）`
    - `f3732c2 Merge branch 'feature/waveshare-single-can-standalone' into main`

- **是否有线上部署：**  
  不确定。当前上下文是本地固件开发与构建，未涉及线上 Web 部署。固件曾在 V1.0 阶段烧录过微雪板，但当前 UI V1.1 尚未执行 Task 14 资产生成 + 烧录。

- **是否涉及数据库：**  
  未发现传统数据库。固件可能使用 NVS / SPIFFS 等嵌入式存储保存配置和插件，但当前 UI 任务没有改底层存储。

- **是否涉及第三方 API：**  
  当前 UI 改造不涉及第三方 API。插件管理会使用固件本地 API：`/plugins/status`、`/plugins/install_url`、`/plugins/install_json`、`/plugins/upload`、`/plugins/toggle`、`/plugins/priority`、`/plugins/remove`、`/plugins/replay_count`、`/plugins/rule_test`。AP / 配置相关通过本地 `/config` 等接口，不是第三方 API。

- **是否涉及敏感配置或密钥：**  
  当前检查未发现 `.env` 文件。没有输出任何 API Key、Token、密码、私钥、Cookie。如果后续发现敏感值，必须统一写成 `[REDACTED]`，不要贴真实值。

---

## 2. 当前项目进度

### 模块 A：UI V1.1 设计规范与实施计划

- **状态：已完成**
- **相关文件：**
  - `docs/superpowers/specs/2026-06-16-single-can-ui-redesign-design.md`
  - `docs/superpowers/plans/2026-06-16-single-can-ui-redesign.md`
- **核心逻辑：**
  - Spec 定义 5 个 UI 问题和统一设计系统方向。
  - Plan 定义 14 个 TDD 任务。
  - 实施方法选择：Subagent-Driven Development。
- **已验证：**
  - Spec 已提交：`c591746`
  - Plan 已提交：`3d4bfe5`
- **当前问题：**
  - Plan 中部分旧行号已经因 Task 1–5 改动而漂移，新 AI 必须用内容定位，不要盲信行号。
- **下一步：**
  - 从 Task 6 继续执行。

### 模块 B：Task 1 — 设计系统基底

- **状态：已完成，已验证**
- **相关文件：**
  - `include/web/mcp2515_dashboard_ui.src.h`
  - `test/test_ui_v11_redesign.py`
- **核心逻辑：**
  - 在 `:root` 里新增设计系统 token：`--ds-cyan`、`--ds-indigo`、`--ds-accent-grad`、`--ds-ok`、`--ds-warn`、`--ds-err`、`--ds-card-bg`、`--ds-card-stroke`、`--ds-radius`。
  - 将 `.card` 从旧灰色工具风改为深色玻璃风，使用 `--ds-card-bg` / `--ds-card-stroke` / `--ds-radius` / `backdrop-filter: var(--cockpit-card-blur)`。
- **验证：**
  - 契约测试通过。
  - `pio run -e waveshare_single_can_standalone` 已验证 SUCCESS。
  - 已执行 `git restore include/web/mcp2515_dashboard_ui.h` 去除构建 stamping。
- **提交：**
  - `a7763df feat(ui): 设计系统基底 — ds token + .card 驾驶舱玻璃化`
- **当前问题：** 无。
- **下一步：** 后续页面继续复用这些 token 和 class。

### 模块 C：Task 2 — 删除首页底部 3 个冗余块

- **状态：已完成，已验证**
- **相关文件：**
  - `include/web/mcp2515_dashboard_ui.src.h`
  - `test/test_ui_v11_redesign.py`
- **删除内容：**
  - `维修模式 0x339`
  - `方向盘免打扰 0x3C2`
  - `Legacy 功能支持矩阵`
- **保留内容：**
  - `Legacy CAN-A 诊断`
  - `AP 注入安全`
- **核心逻辑：**
  - 这 3 块是纯静态卡片，无 id、无 JS、无后端 API，删除零副作用。
- **验证：**
  - grep 确认 3 个标题出现次数为 0。
  - grep 确认相邻保留块仍存在。
  - `test_ui_v11_redesign` 当时 4/4 OK。
  - 构建 SUCCESS。
- **提交：**
  - `f1115c7 feat(ui): 删除首页底部冗余3块（维修模式/方向盘免打扰/Legacy矩阵）`
- **当前问题：** 无。
- **下一步：** 无。

### 模块 D：Task 3 — 驾驶中心 CAN2 → AP 激活

- **状态：已完成，已验证**
- **相关文件：**
  - `include/web/mcp2515_dashboard_ui.src.h`
  - `test/test_ui_v11_redesign.py`
- **核心逻辑：**
  - 将驾驶状态中心的 CAN2 指标格替换为 AP 激活指标：`ov-ap-metric`、`ov-ap`、`ov-ap-hint`。
  - 新指标使用现有 AP 注入状态字段：真实字段是 `d.apInjectionState`，不是计划里假设的 `d.apActive`。
  - 当前语义：
    - `settling` 或 `injecting` → 显示 `激活`
    - `settling` → hint：`Gate {apDelayMs || 2000}ms · 已就绪`
    - `injecting` → hint：`正在注入`
    - 其它状态 → `等待` / `门控未触发`
- **为什么这么改：**
  - spec 的「Gate {delay}ms · 已就绪」更符合 `settling` 状态。一开始实现者只把 `injecting` 算激活，审查后认为 `settling` 也应算 AP 激活，于是补了 `a69b52e` 修复。
- **验证：**
  - 真实调研：`renderApInjectionState` 使用 `d.apInjectionState`；`setText` / `setCls` 都 guard null。
  - 契约测试通过。
  - 构建 SUCCESS。
- **提交：**
  - `a4ce131 feat(ui): 驾驶中心 CAN2 指标格 → AP 激活状态`
  - `a69b52e fix(ui): AP激活纳入settling态，hint按状态显示（对齐spec：settling=Gate已就绪）`
- **当前问题：**
  - 审查曾提出 `pollCAN2` 在双 CAN 下仍可能更新 `ov-can` 显示 CAN2 文案。控制器判断：这是双 CAN 回归场景合理行为，单 CAN 下 `pollCAN2` 被 `!isSingleCan()` 守卫不运行，不修。
  - `m-can2` 在单 CAN 隐藏；双 CAN 由 `pollCAN2` 更新。
- **下一步：** 无。

### 模块 E：Task 4 — AP 注入安全块重设计

- **状态：已完成，已严格审查，已验证**
- **相关文件：**
  - `include/web/mcp2515_dashboard_ui.src.h`
  - `test/test_ui_v11_redesign.py`
- **核心逻辑：**
  - 将 AP 注入安全块从旧 `grid c4` 工具风，改成驾驶舱风格：`card cockpit-card`、`ck-head`、`ck-chip`、`state-panel`、`gate-bar`、`src-row`、`ctl-row`、`safety-strip`。
  - 新增显示 id：`ap-state-panel`、`ap-core-state-big`、`ap-gate-bar`、`ap-gate-fill`。
  - 原始 id 全保留：`ap-core-card`、`ap-core-state-pill`、`ap-core-gate-tgl`、`ap-delay-select`、`ap-auto-restore-tgl`、`injection-source`、`ap-core-state-detail`。
  - 三个控件仍然调用 `saveApGateControls()`。
- **安全逻辑：**
  - `saveApGateControls` 函数体未改。
  - 后端字段未改：`apg`、`ap_delay_ms`、`ap_auto_restore`。
  - 未改任何 C++。
  - 未碰 `handlers.h`。
  - fail-closed / 2s gate / AP 断开清零仍由服务端强制。
- **验证：**
  - 专项审查确认：7 个原 id 各唯一存在；4 个新 id 存在；gate / auto restore 仍是 checkbox；delay 仍是 select；option 仍是 `0/1000/2000/3000`；diff 只动 `src.h` + test，无 C++。
  - `test_ui_v11_redesign` 当时 10/10 OK。
  - 单 CAN 构建 SUCCESS。
  - 双 CAN 环境 `lilygo_t2can_dual` 也构建 SUCCESS。
- **提交：**
  - `a0a4903 feat(ui): AP 注入安全块重设计为驾驶舱风格（状态提升+Gate进度条，安全线不变）`
- **当前问题：** 无安全顾虑。
- **下一步：** 后续页面可复用 Task 4 的 CSS 类。

### 模块 F：Task 5 — 插件管理重设计

- **状态：已完成，已验证**
- **相关文件：**
  - `include/web/mcp2515_dashboard_ui.src.h`
  - `test/test_ui_v11_redesign.py`
- **核心逻辑：**
  - 桌面端 `plugins-card` 改为驾驶舱风格：🧩 插件管理、URL 安装、上传文件、粘贴 JSON、已安装插件、GTW2047 重放倍率、规则测试器。
  - 新增 / 复用样式：`.install-grid`、`.install-cell`、`.mock-input`、`.ds-btn`、`.ds-btn-muted`、`.ds-select`、`.plugin-list`、`.collapse`。
  - 桌面原 id 全保留：`plugins-card`、`plugin-url`、`plugin-file`、`plugin-json`、`plugin-list`、`plugin-replay-count`、`plugin-editor-json`、`plugin-rule-test-result`。
  - 桌面 onclick / onchange 全保留：`installPluginUrl()`、`uploadPluginJson()`、`installPluginJson()`、`setPluginReplayCount(this.value)`、`runPluginRuleTest()`。
  - 移动端插件页也中文化并保留：`m-plugin-url`、`m-plugin-json`、`m-plugin-list`、`m-plugin-replay-count`、`installMobilePluginUrl()`、`installMobilePluginJson()`、`setPluginReplayCount(this.value)`。
  - 注意：移动端原本没有文件上传入口，本任务保持功能零增减，没有新增移动上传。
- **后续补丁：**
  - 初版完成后发现 render 函数里还有运行时生成英文：`Enabled`、`Priority`、`Remove`、`Detail`、`IDs`、`Rules`、`No plugins installed. New installs start disabled.`。
  - 已补充中文化：`启用`、`优先级`、`删除`、`详情`、`目标ID`、`规则`、`暂无插件。新安装默认禁用。`
  - 已将 `plugins-count-pill` 从固定 `已装 0 个` 接线到实际安装数量：在 `renderPluginsStatus(j)` 中更新 `已装 {installed.length} 个`。
- **验证：**
  - 桌面 id 各 1 次。
  - 移动端 id 各 1 次。
  - 关键英文契约清零。
  - 后续手动 grep 确认 render 英文标签清零。
  - `test_ui_v11_redesign` 13/13 OK。
  - `pio run -e waveshare_single_can_standalone` SUCCESS。
- **提交：**
  - `1209a5b feat(ui): 插件管理重设计为驾驶舱风格（功能零增减，全中文）`
  - `8cf9974 fix(ui): 插件列表render标签中文化+已装数量pill接线（补全spec全中文）`
- **当前问题：**
  - 未做专项 subagent 审查，但控制器已做 id / onclick / 移动端 / 英文残留 / 契约 / 构建核验。
- **下一步：**
  - Task 6 手机底部导航会影响插件入口，注意插件已归入硬件页。

### 模块 G：Task 6 — 手机底部导航 5 标签

- **状态：未开始**
- **相关文件：**
  - `include/web/mcp2515_dashboard_ui.src.h`
  - `test/test_ui_v11_redesign.py`
- **目标：**
  - 将单 CAN 手机底部导航改为：驾驶、硬件、速度、网络、更多。
  - 「更多」面板收纳：驾驶风格、FSD 防护、OTA 升级、CAN 诊断。
- **注意：**
  - 这是高风险任务，因为之前有过 `fix: restore dual can mobile navigation wiring`，说明手机导航容易破坏双 CAN 回归。
  - 必须保留 / 不破坏双 CAN 移动导航。
  - 必须用实际环境名：单 CAN=`waveshare_single_can_standalone`，双 CAN=`lilygo_t2can_dual`。
  - Plan 里写过 `lilygo_t2can`，但实际没有这个 env。
- **下一步：**
  - 新 AI 应从 Task 6 继续，建议严格派 subagent + spec review + code quality review。

### 模块 H：Task 7–12 — 全板块套驾驶舱基底

- **状态：未开始**
- **涉及页面：**
  - Task 7：驾驶风格页 `pg-drive`
  - Task 8：速度策略页 `pg-speed`
  - Task 9：FSD 防护页 `pg-defense`
  - Task 10：OTA 升级页 `pg-ota`
  - Task 11：网络设置页 `pg-network`
  - Task 12：CAN 诊断页 `pg-can`
- **核心目标：**
  - 不改功能。
  - 只把页面头部、卡片、表单、按钮、提示风格统一到 Task 1 / Task 4 / Task 5 的驾驶舱基底。
- **下一步：**
  - Task 6 后逐个推进。
  - 每个任务都要先 audit 当前真实 HTML，因为行号已漂移。

### 模块 I：Task 13 — 全量验证

- **状态：未开始**
- **目标：**
  - 全契约测试
  - 单 / 双 CAN 构建
  - minify check
  - UI 字符串审计
  - 整体 diff review
  - 去 stamping
- **注意：**
  - 当前任务还没到最终验证阶段。
- **下一步：**
  - Task 7–12 完成后执行。

### 模块 J：Task 14 — 资产生成 + 烧录

- **状态：未开始，需 Jordan 明确授权**
- **重要约束：**
  - 只有 Jordan 明确说「生成资产并烧录」才执行。
  - 绝对不要擅自烧录。
  - 绝对不要使用 `pio run -t upload`。
  - 烧录必须使用 `esptool.py` 显式偏移。
- **下一步：**
  - 在 Task 13 完成并得到用户明确授权后再做。

---

## 3. 文件结构说明

### 核心 UI 文件

- `include/web/mcp2515_dashboard_ui.src.h`
  - **作用：** Web Dashboard UI 的真实源文件，包含 HTML / CSS / JS。当前最近检查为 3840 行。
  - **当前状态：** Task 1–5 已修改并提交。
  - **里面最重要的内容：**
    - `:root` CSS token
    - `.card`
    - `.cockpit-card`
    - `.ck-head`
    - `.state-panel`
    - `.install-grid`
    - `pg-overview`
    - `pg-hardware`
    - `plugins-card`
    - `ap-core-card`
    - 移动端 `mob-*` 页面和底部导航 JS
    - JS 函数：`poll`、`pollCAN2`、`renderApInjectionState`、`saveApGateControls`、`loadPlugins`、`renderPluginsStatus`、`renderMobilePlugins`、`showMobilePage`、`updateMobTabs`、`toggleMobMore`
  - **新 AI 修改时要注意：**
    - 这是 C++ header 里的 raw string，clang LSP 诊断可能误报。
    - 不要相信旧行号，用 grep 搜 id / 文案定位。
    - 构建后会生成 / stamp `include/web/mcp2515_dashboard_ui.h`，必须 `git restore include/web/mcp2515_dashboard_ui.h`。
    - UI 字符串最终 gzip 进固件，`firmware.bin` 明文搜不到 UI。

- `include/web/mcp2515_dashboard_ui.h`
  - **作用：** 构建时由脚本生成 / 同步的 UI header。
  - **当前状态：** committed 版本应保持 manual/stub 状态。构建后会变脏，需要 restore。
  - **新 AI 修改时要注意：** 不要手动提交构建 stamp。每次 `pio run` 后执行 `git restore include/web/mcp2515_dashboard_ui.h`。

- `include/web/mcp2515_dashboard.h`
  - **作用：** Dashboard Web 相关 header。
  - **当前状态：** 当前 UI V1.1 任务未直接修改。
  - **注意：** 不确定其具体函数，不要无必要修改。

- `include/web/dash_gateway.h`
  - **作用：** Dashboard gateway 相关 header。
  - **当前状态：** 当前 UI V1.1 任务未修改。
  - **注意：** 不确定其具体函数，不要无必要修改。

### 测试文件

- `test/test_ui_v11_redesign.py`
  - **作用：** UI V1.1 新增契约测试。直接读取 `include/web/mcp2515_dashboard_ui.src.h` 明文源并断言字符串 / id / 结构存在或不存在。
  - **当前状态：** 已创建并持续增加测试。目前包含：`UiV11RedesignTests`、`DesignSystemTests`、`RemoveBottomBlocksTests`、`Can2ToApTests`、`ApInjectionRedesignTests`、`PluginRedesignTests`。
  - **最重要的函数 / 方法：** `assert_present`、`assert_absent`、`assert_id_present`。
  - **新 AI 修改时要注意：**
    - 仓库 `test/` 没有 `__init__.py`，运行必须用 `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign -v`。
    - 新任务继续往该文件追加 test class。
    - 不要让测试只验证 mock，要 grep 真实 `src.h`。

- `test/test_dashboard_api_contract.py`
  - **作用：** Dashboard API 契约测试。
  - **当前状态：** 当前任务未修改。
  - **注意：** Task 13 全量验证时需要一起跑 discover。

- `test/test_wifi_settings_regression.py`
  - **作用：** WiFi 设置回归测试。
  - **当前状态：** 当前任务未修改。
  - **注意：** Task 11 网络设置页时要注意不要破坏这些契约。

### 构建配置

- `platformio.ini`
  - **作用：** PlatformIO 构建环境配置。
  - **当前状态：** 当前任务未修改。
  - **关键 env：**
    - `waveshare_single_can_standalone`：单 CAN standalone 主目标。build flags 包括 `-DDRIVER_TWAI`、`-DESP32_DASHBOARD`、`-DDASH_SINGLE_CAN_STANDALONE=1`、`-DDASH_INITIAL_HW_MODE=3`、`-DDASH_PLUGIN_ENGINE=1`。
    - `lilygo_t2can_dual`：双 CAN 回归目标。
  - **主要依赖：** `platformio/espressif32@7.0.0`、`framework-espidf @ 4.60001.0`、`bblanchon/ArduinoJson@^7`。
  - **新 AI 修改时要注意：** Plan 中提到的 `lilygo_t2can` env 实际不存在，应使用 `lilygo_t2can_dual`。不要改构建配置，除非任务明确要求。

### 固件入口 / 核心逻辑

- `src/main.cpp`
  - **作用：** 固件主入口。
  - **当前状态：** 当前 UI V1.1 任务未修改。
  - **注意：** 不要为 UI 视觉任务改这里。

- `src/espidf_runtime.cpp`
  - **作用：** ESP-IDF runtime 相关逻辑。
  - **当前状态：** 当前任务未修改。
  - **注意：** 不确定具体细节，不要无必要修改。

- `src/CMakeLists.txt`
  - **作用：** ESP-IDF / PlatformIO 构建入口之一。
  - **当前状态：** 当前任务未修改。

- `src/idf_component.yml`
  - **作用：** ESP-IDF component 配置。
  - **当前状态：** 当前任务未修改。

### include 下的重要业务 header

以下文件存在，但当前 UI V1.1 前 5 个任务未修改。新 AI 不应无必要改动。

- `include/app.h`：固件 app 层定义，不确定具体内容。
- `include/can_frame_types.h`：CAN frame 类型定义。
- `include/can_helpers.h`：CAN helper。
- `include/dash_capabilities.h`：Dashboard capabilities。
- `include/dash_fsd_diag.h`：FSD 诊断相关。
- `include/dash_hw3_speed.h`：HW3 速度相关。
- `include/dash_legacy_speed.h`：Legacy 速度相关。
- `include/dash_ota_guard.h`：OTA guard。
- `include/dash_bionic_steer.h`：仿生转向相关。
- `include/dash_fog_light.h`：雾灯 / 灯光相关。
- **注意：** 当前任务是 UI 视觉改造。除非计划明确要求，否则不要动 C++ 业务逻辑文件。

### 脚本

- `scripts/platformio_sync_profile.py`
  - **作用：** PlatformIO pre script，同步 profile / UI header，构建时会导致 `include/web/mcp2515_dashboard_ui.h` stamping。
  - **注意：** 构建后要 restore `ui.h`。

- `scripts/update_ota_build_timestamp.py`
  - **作用：** 更新 OTA build timestamp。
  - **注意：** 当前 UI 开发不用改。

- `scripts/minify_dashboard.py`
  - **作用：** Dashboard UI minify / check 脚本。
  - **当前状态：** Task 13 要跑：`python3 scripts/minify_dashboard.py --check`。

- `scripts/webui_simulator.py`
  - **作用：** Web UI simulator，不确定当前是否用于本 UI 测试。
  - **注意：** 当前任务未使用。

### 文档

- `docs/superpowers/specs/2026-06-16-single-can-ui-redesign-design.md`
  - **作用：** 本轮 UI V1.1 设计 spec。
  - **当前状态：** 已确认并提交。
  - **新 AI 注意：** 这是需求源头，继续开发前应读。

- `docs/superpowers/plans/2026-06-16-single-can-ui-redesign.md`
  - **作用：** 本轮 UI V1.1 14 任务实施计划。
  - **当前状态：** 已确认并提交。
  - **新 AI 注意：** 任务 1–5 已完成；从 Task 6 继续；行号已经漂移，必须用内容定位。

---

## 4. 核心逻辑说明

### UI 是怎么进入固件的？

这个项目的 Dashboard UI 不是普通前端项目，没有独立 React/Vite 构建。UI 被写在 `include/web/mcp2515_dashboard_ui.src.h`。这个文件是 C++ header，但里面主要是一个 raw string，包含 HTML / CSS / JavaScript。构建时脚本会处理它，生成或同步 `include/web/mcp2515_dashboard_ui.h`。最终 UI 以 gzip 形式进固件。

因此：搜 `firmware.bin` 看不到 UI 明文；应该检查 `src.h` 明文源；构建后 `ui.h` 会变化，但不要提交构建 stamp。

### 用户 / 请求从哪里进入？

用户通过设备 Dashboard Web UI 进入。UI 页面通过本地 HTTP API 与固件交互。关键 JS 逻辑在 `mcp2515_dashboard_ui.src.h` 中：

- 状态轮询：`poll`、`pollCAN2`
- AP 注入状态渲染：`renderApInjectionState`
- AP gate 配置保存：`saveApGateControls`
- 插件状态：`loadPlugins`、`renderPluginsStatus`、`renderMobilePlugins`
- 移动端页面切换：`showMobilePage`、`updateMobTabs`、`toggleMobMore`

### 数据经过哪些步骤处理？

以插件管理为例：

1. UI 调 `loadPlugins()`。
2. fetch `/plugins/status`。
3. JSON 进入 `PLUGINS`。
4. 调 `renderPluginsStatus(j)` 更新桌面插件列表，调 `renderMobilePlugins(j)` 更新移动端插件列表。
5. 用户点击 URL 安装 / JSON 安装 / 上传 / 开关 / 删除 / 倍率，对应调用 `installPluginUrl()`、`installPluginJson()`、`uploadPluginJson()`、`togglePlugin(name, checked)`、`removePlugin(name)`、`setPluginReplayCount(count)`。

Task 5 已把这些用户可见文案中文化，并保留原函数与 API。

以 AP 注入安全为例：

1. 后端状态里有 `d.apInjectionState`。
2. `renderApInjectionState(d)` 更新 `ap-core-state-pill`、`ap-core-state-detail`、`injection-source` 以及新增 `ap-core-state-big`、`ap-state-panel`、`ap-gate-fill`。
3. 用户改门控 / 延迟 / 自动恢复时调用 `saveApGateControls()`。
4. `saveApGateControls()` 发 `/config` payload：`apg`、`ap_delay_ms`、`ap_auto_restore`。
5. 服务端 C++ 强制安全逻辑，UI 不能绕过。

### 哪些地方容易出 bug？

1. **行号漂移**：Plan 中行号已不准。新 AI 必须 grep id / 文案定位。
2. **`mcp2515_dashboard_ui.src.h` 被 clang LSP 误报**：真实依据是 `pio run`。
3. **`include/web/mcp2515_dashboard_ui.h` stamping**：构建后必须 restore，不要提交。
4. **手机导航**：过去有 commit `c569c21 fix: restore dual can mobile navigation wiring`。Task 6 修改手机导航时特别容易破坏双 CAN，必须单 / 双 CAN 构建回归。
5. **AP 注入安全**：UI 只能展示和保存配置，不能碰服务端注入逻辑。
6. **插件管理 id / onclick**：插件逻辑依赖固定 id，换皮时必须保留。

### 哪些逻辑已经改过？

- `.card` 已玻璃化。
- AP 注入安全卡已重写。
- 插件管理卡已重写。
- AP 激活格已替代 CAN2。
- 插件 render 英文已中文化。
- `plugins-count-pill` 已接线真实数量。

### 为什么要这样设计？

Jordan 的产品哲学是「简约」：预测 → 单点击破 → All In。这轮 UI 设计采用「设计系统优先」：先抽取驾驶中心的视觉基因（深色背景、玻璃卡片、状态胶囊、图标 + 主值 + 说明），再逐页面套用。保持功能零增减，避免过度工程。所有安全逻辑留在服务端，不让 UI 变成安全边界。

### 有没有临时方案或技术债？

- `mcp2515_dashboard_ui.src.h` 是 3840 行单文件，非常大，维护成本高。但本轮计划明确不拆文件。
- `plugins-count-pill` 已接线，但只是显示已装数量，没有更多状态分类。
- Task 7–12 只是视觉套基底，不应顺手做功能重构。
- UI 契约测试主要是字符串 grep，不是浏览器 E2E。优点是快，缺点是无法验证真实布局。

---

## 5. 环境变量与配置

### `.env`

- 当前检查结果：未发现 `.env` 文件。
- Bash 检查 `.env*` 时 zsh 报 `no matches found`，结论是当前目录无 `.env`。
- 因此当前固件开发不依赖 `.env`。

### PlatformIO 配置

- `platformio.ini`
  - **用途：** 构建配置。
  - **是否必填：** 是。
  - **关键配置：** `platform = platformio/espressif32@7.0.0`、`framework = espidf`、`framework-espidf @ 4.60001.0`、`lib_deps = bblanchon/ArduinoJson@^7`。
  - **缺失会导致：** 无法构建固件。

### 关键 build flags

- `DASH_SINGLE_CAN_STANDALONE=1`
  - **用途：** 单 CAN standalone 产品 profile。
  - **是否必填：** 对 `waveshare_single_can_standalone` 是。
  - **缺失会导致：** UI / runtime 可能不按单 CAN 模式隐藏 CAN2 相关内容。

- `DASH_PLUGIN_ENGINE=1`
  - **用途：** 启用插件引擎。
  - **是否必填：** 对当前插件管理 UI 相关功能是。
  - **缺失会导致：** 插件 UI 可能对应不到后端能力。

- `DASH_STA_AP_GATEWAY`
  - **用途：** STA/AP 网关相关。
  - **是否必填：** 对当前目标 profile 是。
  - **缺失影响：** 不确定，可能影响网络/AP 行为。

- `ESP_IDF_LWIP_HOOK_FILENAME="lwip_hooks.h"`
  - **用途：** lwIP hook。
  - **缺失影响：** 不确定，属于固件网络构建配置。

### API / 密钥

- 当前上下文未涉及 API Key、Token、密码、私钥、Cookie。
- 如果后续发现，文档和聊天输出中必须写 `[REDACTED]`。

### 数据库配置

- 未发现数据库连接配置。
- 不涉及传统数据库。

### 代理配置

- 当前 UI V1.1 任务未涉及代理配置。

### 部署配置

- 固件构建通过 PlatformIO。
- 烧录在 Task 14 才会做，且需用户明确授权。

---

## 6. 启动、运行、测试方式

### 0. 进入项目目录

已验证：

```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware
```

### 1. 查看当前分支

已验证：

```bash
git branch --show-current
```

应看到：

```text
feature/ui-v11-redesign
```

### 2. 安装依赖

未在本次会话重新验证安装过程。推测需要：

```bash
pip install platformio
```

以及 PlatformIO 自动拉取 `platformio/espressif32@7.0.0` 和 ArduinoJson。若依赖已存在，直接构建即可。

### 3. 运行 UI 契约测试

已验证：

```bash
PYTHONPATH=test python3 -m unittest test_ui_v11_redesign -v
```

当前应看到：

```text
Ran 13 tests
OK
```

### 4. 构建单 CAN 固件

已多次验证：

```bash
pio run -e waveshare_single_can_standalone
```

成功时应看到类似：

```text
waveshare_single_can_standalone  SUCCESS
========================= 1 succeeded in ... =========================
```

### 5. 构建双 CAN 回归环境

已验证环境存在：

```bash
pio run -e lilygo_t2can_dual
```

Task 4 时已验证 SUCCESS。注意：Plan 中部分位置写 `lilygo_t2can`，这是错误 / 过时环境名。实际应使用 `lilygo_t2can_dual`。

### 6. 构建后清理 stamping

每次构建后必须执行：

```bash
git restore include/web/mcp2515_dashboard_ui.h
```

原因：构建脚本会改 `ui.h`，但不应提交。

### 7. minify check

Task 13 才需要。未在当前已完成阶段验证。

计划命令：

```bash
python3 scripts/minify_dashboard.py --check
```

### 8. 全量测试

Task 13 才需要。计划命令：

```bash
PYTHONPATH=test python3 -m unittest discover -s test -v
```

当前尚未在本轮完整跑 discover，已跑的是 `test_ui_v11_redesign`。

### 9. 烧录

当前不要做。Task 14 需要 Jordan 明确授权。

禁止：

```bash
pio run -t upload
```

正确方向是使用 `esptool.py` 显式偏移，但当前不要执行。

### 常见启动 / 构建失败原因

1. **clang LSP 报错**：`Arduino.h file not found`、raw string 里的 `#` 被当 preprocessing directive。原因是 LSP 不理解 PlatformIO / ESP-IDF include path 或 raw string。处理：看 `pio run`，不要被 LSP 误导。
2. **`test/` 找不到模块**：`ModuleNotFoundError` 或测试发现失败。原因：`test/` 没有 `__init__.py`。处理：必须加 `PYTHONPATH=test`。
3. **构建后 git 有 `mcp2515_dashboard_ui.h` 修改**：构建 stamping。处理：`git restore include/web/mcp2515_dashboard_ui.h`。
4. **使用错误双 CAN env**：PlatformIO 找不到 `lilygo_t2can`。有效 env：`lilygo_t2can_dual`。

---

## 7. 已知问题与坑

### 问题：clang LSP 对 `mcp2515_dashboard_ui.src.h` 大量误报

- **表现：** `Arduino.h file not found`、`Invalid preprocessing directive`、`Unexpected character <U+25A3>`、raw string 内 HTML/CSS/emoji 被 clang 当 C++ 解析。
- **可能原因：** 该文件是 C++ header + raw string，LSP 没有 ESP-IDF/PlatformIO 完整上下文。
- **已尝试方案：** 不把 LSP 诊断作为判断依据。
- **有效方案：** 用 `pio run` 验证。
- **无效方案：** 试图按 clang 报错修改 raw string 内容，这会误伤 UI。
- **相关文件：** `include/web/mcp2515_dashboard_ui.src.h`
- **新 AI 下一步该怎么查：** 如果看到这些 diagnostics，先忽略。跑 `pio run -e waveshare_single_can_standalone`。

### 问题：构建会污染 `mcp2515_dashboard_ui.h`

- **表现：** `git status` 显示 `include/web/mcp2515_dashboard_ui.h` 被修改。
- **可能原因：** `scripts/platformio_sync_profile.py` / 构建流程会 stamp 该文件。
- **已尝试方案：** 构建后 restore。
- **有效方案：** `git restore include/web/mcp2515_dashboard_ui.h`
- **无效方案：** 提交 `ui.h` stamp。
- **相关文件：** `include/web/mcp2515_dashboard_ui.h`、`scripts/platformio_sync_profile.py`
- **新 AI 下一步该怎么查：** 每次构建后都跑 `git status --short`。

### 问题：Plan 里的行号已漂移

- **表现：** Plan 写某段在 759 行，但实际已经不是。
- **可能原因：** Task 1–5 已增删多处。
- **有效方案：** grep id / 文案定位，例如 `id="mob-tabs"`、`showMobilePage`、`mob-more`、`pg-drive`、`pg-speed`。
- **无效方案：** 直接按行号 edit。
- **相关文件：** `docs/superpowers/plans/2026-06-16-single-can-ui-redesign.md`、`include/web/mcp2515_dashboard_ui.src.h`
- **新 AI 下一步该怎么查：** 先 `grep -n`，再 Read 小范围上下文。

### 问题：手机导航容易破坏双 CAN

- **表现：** 曾有历史 commit：`c569c21 fix: restore dual can mobile navigation wiring`
- **可能原因：** 单 CAN / 双 CAN 有不同 mobile tab / hide 逻辑。
- **有效方案：** Task 6 必须先读当前 mobile nav 全部结构，确认 `data-single-hide` / `data-dual-hide` 语义，再改；单 / 双 CAN 都构建。
- **相关文件：** `include/web/mcp2515_dashboard_ui.src.h`
- **新 AI 下一步该怎么查：** `grep -n "mob-tabs\|showMobilePage\|updateMobTabs\|mob-more" include/web/mcp2515_dashboard_ui.src.h`

### 问题：AP 注入安全不能被 UI 弱化

- **表现：** 用户特别强调 fail-closed / 2s gate / AP 断开清零。
- **可能原因：** 这是车辆控制相关安全逻辑。
- **有效方案：** UI 只显示和保存配置。不改 C++ handler。不改 `saveApGateControls` 函数体。
- **相关文件：** `include/web/mcp2515_dashboard_ui.src.h`；`handlers.h` 或相关 C++ 文件（当前任务未查具体路径）。
- **新 AI 下一步该怎么查：** 若涉及 AP 注入，先 grep `saveApGateControls` 和 `apInjectionState`。不要碰服务端注入路径。

### 问题：`.env` 不存在

- **表现：** `ls .env*` 无匹配。
- **原因：** 当前固件项目不依赖 `.env`。
- **有效方案：** 不要编造环境变量。
- **相关文件：** 无。
- **新 AI 下一步该怎么查：** 如果任务涉及 WiFi / API / secret，再单独检查配置文件。

### 问题：Task 5 初版仍有 render 英文标签

- **表现：** DOM 模板已中文化，但 `renderPluginsStatus` / `renderMobilePlugins` 生成的字符串仍有英文。
- **有效方案：** 已在 `8cf9974` 修复。
- **相关文件：** `include/web/mcp2515_dashboard_ui.src.h`
- **新 AI 下一步该怎么查：** 如果继续中文化其它模块，不要只看静态 HTML，也要看 JS render 字符串。

### 问题：`plugins-count-pill` 初版固定显示 0

- **表现：** `plugins-count-pill` 静态 `已装 0 个`。
- **有效方案：** 已在 `renderPluginsStatus(j)` 中接线为实际数量。
- **相关文件：** `include/web/mcp2515_dashboard_ui.src.h`
- **新 AI 下一步该怎么查：** grep `plugins-count-pill`。

### 问题：不要 push

- **表现：** 记忆中明确：origin/main 是 hqs 独立线，历史无共同祖先；不推送。
- **有效方案：** 不要 `git push`。只本地 commit。
- **相关文件：** Git repo。
- **新 AI 下一步该怎么查：** 除非 Jordan 明确要求，否则不要 push。

### 问题：不要用 `pio run -t upload`

- **表现：** 历史经验：flash_args 模板错误。
- **有效方案：** 烧录用 `esptool.py` 显式偏移。
- **相关文件：** `platformio.ini`
- **新 AI 下一步该怎么查：** Task 14 才考虑烧录，且需授权。

---

## 8. 最近修改记录

### 修改 1：新增 UI V1.1 spec

- **修改原因：** 明确 5 个 UI 问题和设计系统方向。
- **修改文件：** `docs/superpowers/specs/2026-06-16-single-can-ui-redesign-design.md`
- **改了什么：** 定义手机导航、CAN2→AP、插件/AP 注入风格、删 3 块、全板块统一。
- **为什么这么改：** 先设计再实施，避免 UI 方向反复。
- **是否验证：** 用户已确认。
- **可能影响：** 作为后续实现依据。

### 修改 2：新增 UI V1.1 14 任务实施计划

- **修改原因：** 把设计拆成可执行 TDD 任务。
- **修改文件：** `docs/superpowers/plans/2026-06-16-single-can-ui-redesign.md`
- **改了什么：** 定义 Task 1–14。
- **为什么这么改：** 用 Subagent-Driven Development 分任务实施和审查。
- **是否验证：** 用户选择 Subagent-Driven。
- **可能影响：** 新 AI 应继续按此计划执行。

### 修改 3：Task 1 设计系统基底

- **修改原因：** 为全板块统一提供 token 和基础 `.card` 风格。
- **修改文件：** `include/web/mcp2515_dashboard_ui.src.h`、`test/test_ui_v11_redesign.py`
- **改了什么：** 新增 `--ds-*` token；`.card` 改深色玻璃；新建测试基类。
- **为什么这么改：** 设计系统优先。
- **是否验证：** 测试 + 构建已验证。
- **可能影响：** 所有 `.card` 都会更暗、更玻璃化。

### 修改 4：Task 2 删除首页底部 3 块

- **修改原因：** 3 个静态块低价值且不适合单 CAN UI。
- **修改文件：** `include/web/mcp2515_dashboard_ui.src.h`、`test/test_ui_v11_redesign.py`
- **改了什么：** 删除维修模式 0x339、方向盘免打扰 0x3C2、Legacy 功能支持矩阵。
- **为什么这么改：** 简化首页，减少噪声。
- **是否验证：** grep + 测试 + 构建。
- **可能影响：** 首页更短，AP 注入安全更靠上。

### 修改 5：Task 3 驾驶中心 CAN2 → AP 激活

- **修改原因：** 单 CAN 没有 CAN2，显示 AP 激活更有意义。
- **修改文件：** `include/web/mcp2515_dashboard_ui.src.h`、`test/test_ui_v11_redesign.py`
- **改了什么：** CAN2 metric 换成 AP 激活 metric；新增 AP 状态更新逻辑；使用真实字段 `d.apInjectionState`。
- **为什么这么改：** 与 AP 注入安全块同源。
- **是否验证：** 测试 + 构建。
- **可能影响：** 驾驶状态中心更适合单 CAN。双 CAN CAN2 逻辑保留在双 CAN 路径。

### 修改 6：Task 3 语义修复：settling 也算 AP 激活

- **修改原因：** spec 的 Gate 已就绪语义更符合 `settling`。
- **修改文件：** `include/web/mcp2515_dashboard_ui.src.h`
- **改了什么：** `apActive = injecting || settling`；hint 分状态显示：settling=Gate delay 已就绪，injecting=正在注入。
- **为什么这么改：** 避免 Gate 计时中显示等待。
- **是否验证：** 测试 + 构建。
- **可能影响：** AP 激活显示更早变绿。

### 修改 7：Task 4 AP 注入安全块重设计

- **修改原因：** 原 UI 是工具网格，与驾驶中心风格割裂。
- **修改文件：** `include/web/mcp2515_dashboard_ui.src.h`、`test/test_ui_v11_redesign.py`
- **改了什么：** 重写 `ap-core-card` 结构；新增状态主区、Gate 进度条、安全条；保留原 id / onchange / 后端字段。
- **为什么这么改：** 提升可读性，同时不改变安全逻辑。
- **是否验证：** 契约 + 单 / 双构建 + 专项安全审查。
- **可能影响：** AP 设置视觉显著改变，但功能不变。

### 修改 8：Task 5 插件管理重设计

- **修改原因：** 原插件管理是英文工具风，需统一驾驶舱风格。
- **修改文件：** `include/web/mcp2515_dashboard_ui.src.h`、`test/test_ui_v11_redesign.py`
- **改了什么：** 桌面插件卡改成驾驶舱风格；移动插件页中文化；新增相关 CSS。
- **为什么这么改：** 功能零增减，只换皮。
- **是否验证：** 契约 + 构建。
- **可能影响：** 插件页面视觉大改。原 id / onclick 保留。

### 修改 9：Task 5 render 标签中文化 + pill 接线

- **修改原因：** 初版静态 HTML 中文化后，JS render 字符串仍有英文；`plugins-count-pill` 固定 0。
- **修改文件：** `include/web/mcp2515_dashboard_ui.src.h`
- **改了什么：** `Enabled`→`启用`、`Priority`→`优先级`、`Remove`→`删除`、`Detail`→`详情`、`IDs`→`目标ID`、`Rules`→`规则`、`No plugins installed...`→`暂无插件。新安装默认禁用。`；`plugins-count-pill` 显示真实安装数量。
- **为什么这么改：** 补全 spec 的「全中文」。
- **是否验证：** grep + 契约 + 构建。
- **可能影响：** 插件列表 render 文案改变。

---

## 9. 下一步开发计划

### 下一步最优先做什么？

#### Task 6：手机底部导航 5 标签

- **目标：** 将单 CAN 手机底部导航改为驾驶、硬件、速度、网络、更多；更多面板包含驾驶风格、FSD 防护、OTA 升级、CAN 诊断。
- **原因：** 这是用户最早提出的问题之一。当前手机端模块入口不完整。插件管理已经合并进硬件页风格，适合接入新的「硬件」tab。
- **涉及文件：** `include/web/mcp2515_dashboard_ui.src.h`、`test/test_ui_v11_redesign.py`
- **具体步骤：**
  1. 在 `test/test_ui_v11_redesign.py` 增加 `MobileNavTests`。
  2. 先跑红：`PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.MobileNavTests -v`
  3. grep 当前 mobile nav：`grep -n "mob-tabs\|showMobilePage\|updateMobTabs\|mob-more\|data-mobile-page" include/web/mcp2515_dashboard_ui.src.h`
  4. 读完整 mobile nav HTML + JS。
  5. 重写单 CAN `#mob-tabs` 为 5 标签。
  6. 新增或改造 standalone 更多面板。
  7. 更新 `showMobilePage` / `updateMobTabs` map：`driving -> pg-overview`、`hardware -> pg-hardware`、`speed -> pg-speed`、`network -> pg-network`、`drivingstyle -> pg-drive`。
  8. 注意隐藏项：CAN2、灯光特技、自动换挡不应出现在单 CAN 更多面板。
  9. 跑 `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign -v`。
  10. 双构建：`pio run -e waveshare_single_can_standalone`、`pio run -e lilygo_t2can_dual`、`git restore include/web/mcp2515_dashboard_ui.h`。
  11. 提交。
- **验收标准：**
  - 契约测试新增通过。
  - 全部 UI V1.1 契约通过。
  - 单 CAN / 双 CAN 构建通过。
  - 单 CAN 手机底部有 5 个入口。
  - 更多面板有 4 个剩余模块。
  - 双 CAN mobile nav 不被破坏。

### 第二优先级：Task 7–8 驾驶风格页与速度策略页套驾驶舱基底

- **目标：** Task 7：驾驶风格页统一到驾驶舱风格。Task 8：速度策略页统一到驾驶舱风格。
- **原因：** 这两个页面是用户高频可见配置页。适合在 Task 6 后继续推进视觉统一。
- **具体步骤：**
  1. grep `pg-drive` 和 `pg-speed`。
  2. Read 各自完整页面。
  3. 先加契约测试：标题存在；关键 id / class 仍存在；`ck-head` / `cockpit-card` 等新类存在。
  4. 套用视觉类。
  5. 不改功能函数。
  6. 测试 + 构建 + commit。

### 第三优先级：Task 9–12 剩余页面统一

- **目标：** FSD 防护页、OTA 升级页、网络设置页、CAN 诊断页统一到驾驶舱风格。
- **原因：** 完成问题 5「整套车机版 + 手机版风格统一」。为最终 Task 13 验证做准备。
- **具体步骤：**
  1. 每页单独任务。
  2. 每页先 audit 当前 DOM / id / onclick。
  3. 每页加契约测试。
  4. 只换皮，不改功能。
  5. 每页单独 commit。
  6. 注意网络设置页可能受 `test_wifi_settings_regression.py` 影响。
  7. 注意 CAN 诊断页已经部分 cockpit 风，不要过度改。

### 之后：Task 13 全量验证

- **目标：** 确认全 UI V1.1 改动没有破坏构建和契约。
- **具体步骤：**
  ```bash
  PYTHONPATH=test python3 -m unittest discover -s test -v
  pio run -e waveshare_single_can_standalone -e lilygo_t2can_dual
  python3 scripts/minify_dashboard.py --check
  git restore include/web/mcp2515_dashboard_ui.h
  git diff main -- include/web/mcp2515_dashboard_ui.src.h | head -200
  ```
- **注意：** 如果 discover 里旧测试因 UI 文案变化失败，要判断是测试该更新还是功能真的坏了。

### 最后：Task 14 资产生成 + 烧录

- **不要现在做。**
- **必须等待 Jordan 明确授权。**
- **不要使用：** `pio run -t upload`
- **必须使用 esptool 显式偏移。**

## 哪些任务不要现在做？

1. 不要烧录。Task 14 需要明确授权。
2. 不要 push。记忆中明确不要推远程，origin/main 历史复杂。
3. 不要重构 C++ 后端。本轮是 UI 视觉改造。AP 安全逻辑不碰。
4. 不要拆 `mcp2515_dashboard_ui.src.h`。文件很大但计划没有拆分任务，拆文件会扩大风险。
5. 不要新增功能。插件 / AP 注入 / 网络 / OTA 都只是换皮。Jordan 强调简约，避免过度工程。

## 哪些功能容易过度开发？

1. 插件管理：不要加新的插件市场、搜索、分类、统计。当前只需已有功能换皮。
2. AP 注入安全：不要让 UI 自己决定安全状态。不要加入客户端绕过逻辑。
3. 手机更多面板：不要把所有隐藏项都塞进去。单 CAN 不应该出现 CAN2 / 灯光 / 换挡隐藏项。
4. 全板块统一：不要为了美化改业务逻辑。每个页面只套视觉基底，保留 id / onclick / API。

## 哪些地方要先验证再写代码？

1. Task 6 手机导航：先完整读当前 mobile nav 结构。特别确认 `data-single-hide` / `data-dual-hide`。再改。
2. 任何 id / onclick 相关改动：先 grep 当前真实 id / onclick。改后确认唯一存在。
3. AP 状态字段：永远不要猜 `d.apActive`。当前真实字段是 `d.apInjectionState`。
4. 构建环境：不要用不存在的 `lilygo_t2can`。用 `lilygo_t2can_dual`。
5. 测试命令：`PYTHONPATH=test` 必须带。不带可能失败。

---

## 额外交接提醒

- 当前任务列表状态：
  - #34 T1 完成
  - #35 T2 完成
  - #36 T3 完成
  - #37 T4 完成
  - #38 T5 完成
  - #39 T6 pending
  - #40–#45 T7–T12 pending
  - #46 T13 pending
  - #47 T14 pending，需授权

- 当前最佳继续方式：
  - 使用 `superpowers:subagent-driven-development`
  - 从 Task 6 开始
  - Task 6 是高风险导航任务，建议派实现 subagent + spec reviewer + code quality reviewer，不要控制器直接大改。

- 当前已验证命令：
  ```bash
  PYTHONPATH=test python3 -m unittest test_ui_v11_redesign -v
  pio run -e waveshare_single_can_standalone
  pio run -e lilygo_t2can_dual
  git restore include/web/mcp2515_dashboard_ui.h
  ```

- 当前不确定信息：
  - 是否有线上部署：不确定。
  - 是否有真实数据库：未发现，推测没有传统数据库。
  - `scripts/webui_simulator.py` 是否能用于当前 UI V1.1 视觉验证：不确定，本轮未使用。
  - Task 14 烧录端口当前值：不确定。Plan 示例里有 `/dev/cu.usbmodem21101`，但实际烧录前必须重新确认设备端口。
