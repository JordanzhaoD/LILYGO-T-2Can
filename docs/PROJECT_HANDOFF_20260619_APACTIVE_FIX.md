# 项目交接文档 — 微雪单 CAN 独立版 EPAS nag（apActive 修复轮）

> 生成时间：2026-06-19  
> 适用：把本文档整段粘贴进新的 AI 聊天窗口，新 AI 即可无缝接手。  
> 生成时项目状态：`feature/ui-v11-redesign` @ `2f75109`，apActive 根因已修复并烧录，**待 Jordan 实车验收**。

---

## 1. 项目基本信息

- **项目名称**：LILYGO-T-2Can-firmware · **微雪单 CAN 独立版**（内部代号 Atlas FSD）
- **项目目标**：在 Tesla FSD/AP 行驶时，抑制方向盘「nag」（车要求驾驶员轻转方向盘证明在关注），让驾驶更连贯。
- **最终解决什么问题**：通过 ESP32 向车辆 CAN 总线注入 0x370 (EPAS3P_sysStatus) 帧，让 EPAS 控制器**推断「手在方向盘上」**，从而减少/消除 nag 提示。是 Jordan 私人车辆的驾驶辅助便利功能（已授权，非恶意）。
- **当前开发阶段**：**第 5 轮实车调试**。前 4 轮方案（均值回归扭矩 / B1 固定扭矩 / B2 固定 handsOn / Mode-C 状态机）在 2026.14.x 实车上均未消除 nag；第 5 轮定位到第一性根因（apActive 门控失效）并已修复烧录，**等 Jordan 上车验收**。
- **技术栈**：
  - 语言：C++17（固件）+ Python（契约测试）
  - 框架：ESP-IDF 6.0.1，经 PlatformIO（`platformio/espressif32@7.0.0`）构建
  - 库：ArduinoJson @ ^7
  - 测试：Unity（native C++ 单测）+ pytest（Python 源码契约测试）
- **运行环境**：
  - 硬件：Waveshare ESP32-S3-RS485-CAN（ESP32-S3，16MB flash，单 TWAI CAN @ 500kbps）
  - 目标板 MAC：`28:84:85:49:93:fc`（ESP32-S3 rev v0.2，XMC 16MB quad flash）
  - 串口：`/dev/cu.usbmodem21101`（历史值，烧录前重新确认）
  - WiFi：板上 AP 模式 `Atlas-FSD` @ `100.100.1.1`（手机/电脑连此 WiFi 访问 Dashboard）
- **主要依赖**：见 `platformio.ini`（ArduinoJson 是唯一外部 lib；其余 ESP-IDF 自带）
- **项目所在目录**：`/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware`
- **当前分支 / 版本状态**：
  - 分支：`feature/ui-v11-redesign`（**所有 EPAS nag 工作都在此分支**）
  - HEAD：`2f75109` = `v1.0-atlas-single-can-52-g2f75109`（设备自报版本串）
  - 本地 tag：`v1.0-atlas-single-can`（打在更早的 main merge commit `f3732c2` 上）
  - **未 push 任何 remote**（见下方约束）
  - 工作区有 1 个未提交改动：`docs/superpowers/specs/2026-06-19-upstream-epas-nag-modec-align-design.md`（apActive 修订注记，docs-only，可单独 commit 或丢弃）
- **是否有线上部署**：**无**。嵌入式固件，烧录到物理板即「部署」。
- **是否涉及数据库**：**无**。持久化只用 SPIFFS（文件系统，存 `/rec.csv`、`/epas_nag_trace.csv`、插件配置）和 NVS（存开关状态）。
- **是否涉及第三方 API**：**无**。纯 CAN 总线 + 本地 HTTP/WebSocket 服务，不联网。
- **是否涉及敏感配置或密钥**：
  - 无 API key / Token / 密码（项目本身不需要）。
  - **安全敏感**：CAN 总线注入涉及车辆控制系统。仅限 Jordan 授权的私人车辆。EPAS nag 默认 OFF，需手动开 Dashboard 防御页开关才启用。
  - WiFi 密码若有：`[REDACTED]`（AP `Atlas-FSD` 的密码在 NVS/配置，本文不写明文）。

---

## 2. 当前项目进度（按模块）

> **最重要的一句**：前 4 轮调「扭矩值 / handsOn level」全失败，是因为它们都在 `nextDemandTorque` **内部**调参，而内部根本进不去——卡在门口的 `apActive` 门控（第 5 轮发现）。第 5 轮已删该门控。**新 AI 不要再回去调扭矩/handsOn 固定值**（上游 v2.16-beta.8 已证伪）。

### 模块 A：EPAS nag 核心引擎
- **状态**：第 5 轮刚修复 apActive 门控，**待实车验收**
- **相关文件**：`include/dash_epas_nag.h`（353 行）、`include/can_helpers.h`（165 行，AP 解析）、`include/handlers.h`（995 行，CarManagerBase）
- **核心逻辑**：回声 0x370 帧（counter+1 + demand-state 扭矩 + 从扭矩派生的 handsOnLevel），按 das hands-on 等级差异化注入（Mode-C 状态机）
- **当前问题**：apActive 根因已修；**残留第二层隐患**——Jordan 车 0x399 byte5 全程=`0xC4`→das 恒=1，打通后只走 `s1_hold`（极轻 500ms +0Nm），若 nag 仍在需查 das 字段映射
- **下一步**：等 Jordan 实车验证 `echo>0` + `demand.gate` 切换 + nag 是否减弱

### 模块 B：诊断可观测性（P1 诊断层）
- **状态**：已完成且验证有效（第 2 轮 7e2c728 引入，本轮依赖它定位根因）
- **相关文件**：`include/dash_epas_nag_trace.h`、`include/web/mcp2515_dashboard.h`（`/status.epasNagDiag` JSON、串口 `epas_nag`/`epas_nag_trace` 命令、防御页 chip）
- **核心逻辑**：实时 `/status.epasNagDiag`（echo/suppress/lastGate/real370/echo370/demand...）+ `EpasNagTraceRing` 自动录 SPIFFS `/epas_nag_trace.csv`（10Hz + 边沿，flush 延后 loop 不阻塞 100Hz 热路径）
- **当前问题**：无
- **下一步**：无（本轮靠它拿铁证）

### 模块 C：Mode-C demand-state 状态机（扭矩内核）
- **状态**：已完成（第 4 轮 49c4125），第 5 轮只改了入口门控（删 `!apActive`）
- **相关文件**：`include/dash_epas_nag.h::nextDemandTorque()`
- **核心逻辑**：das==1→500ms hold；das==2→2000ms delay+mild walk；das==3/4/5/0xFF→1000ms delay+0→2.1Nm ramp+1500ms cycle；handsOnLevel 从扭矩幅值推导；0x129 转角喂方向（反向助力）
- **当前问题**：见模块 A 第二层隐患
- **下一步**：实车若 das=1 太弱，再决定是改 s1 逻辑还是查 das 字段

### 模块 D：单 CAN standalone 产物裁剪
- **状态**：已完成（2026-06-15/16，10+14 任务，V1.0 tag）
- **相关文件**：`platformio.ini::[env:waveshare_single_can_standalone]`、`include/dash_capabilities.h`、`include/web/mcp2515_dashboard.h::dashCapabilitiesJson()`
- **核心逻辑**：`DASH_SINGLE_CAN_STANDALONE=1` 宏裁掉 CAN2/MCP2515/灯光，`/status.capabilities` 驱动 UI/API 裁剪
- **当前问题**：无
- **下一步**：无

### 模块 E：FSD unlock / AP Gate（其他功能，非 nag）
- **状态**：已完成且实车工作正常
- **相关文件**：`include/handlers.h`（legacy/HW3/HW4 handler）、`include/web/mcp2515_dashboard.h`（injection gate）
- **核心逻辑**：0x3EE mux0/mux1 注入激活 FSD；AP Injection Gate 开启时须 AP active 连续 2s + fail-closed gear
- **当前问题**：FSD 正常，**但依赖 `APActive` 的逻辑都有潜伏 bug**（`isDASAutopilotActive` 不认 status=6）——见已知问题
- **下一步**：独立 bug，待 Jordan 确认 status=6 语义后单独修

### 已证明不可行 / 已废弃方案（新 AI 不要重复尝试）
- ❌ **均值回归 ~0Nm 扭矩**（beta.7 思路）：车读到「手离开」特征，无效（第 2 轮 echoTorque=1908 实测）
- ❌ **B1 固定 +1.80Nm 扭矩**（raw 2230）：扭矩对但 handsOn=0，车不信「手离开+扭矩」矛盾（第 3 轮 CSV 证实）
- ❌ **B2 固定 handsOnLevel=1**（`outData[4]|=0x40`）：上游 v2.16-beta.8 明确证伪固定 handsOn，无效（第 4 轮）
- ❌ **apActive 门控**（本轮删掉）：对 byte0=6 的车失效，导致 echo=0
- ✅ **Mode-C demand-state + handsOnLevel 从扭矩派生**：上游 beta.8 思路，本轮保留，只删了它外面那层失效的 apActive 门

---

## 3. 文件结构说明

### 入口与构建
- **`src/main.cpp`**（806 行）
  - 作用：固件入口，初始化 WiFi/CAN/Dashboard/串口命令，启动 FreeRTOS 任务
  - 当前状态：稳定
  - 注意：改动少，主要逻辑在 header-only 模块
- **`platformio.ini`**（357 行）
  - 作用：PlatformIO 构建配置，定义 ~10 个 env（各种硬件）
  - **本项目只用 `[env:waveshare_single_can_standalone]`（L178）**：board=esp32s3box，16MB，`DASH_SINGLE_CAN_STANDALONE=1`，`DASH_INITIAL_HW_MODE=3`，TWAI GPIO15/16
  - 注意：`default_envs` 不含 waveshare_single_can_standalone，构建/测试要显式 `-e waveshare_single_can_standalone`
- **`partitions_16mb_ota_4096k_nvs64.csv`**
  - 作用：自定义分区表。nvs@0x9000 / otadata@0x19000 / app0@**0x20000**(4MB) / app1@0x420000
  - 注意：**app 在 0x20000（不是默认 0x10000）**——这就是为什么不能用 `pio run -t upload`（会用 0x10000 + 2MB 模板，砖）

### 核心业务逻辑（EPAS nag）
- **`include/dash_epas_nag.h`**（353 行）⭐ 本次修复核心
  - 作用：`DashEpasNagEngine` —— EPAS nag 注入引擎（纯逻辑，无 millis/random，可单测）
  - 当前状态：第 5 轮刚改（删 `!apActive` 门控）
  - 最重要的函数：
    - `nextDemandTorque(das, nowMs, outTorque, outLevel)` — Mode-C 状态机，决定每帧注入什么扭矩/level
    - `tryBuildEcho(inData, dlc, nowMs, outData)` — 收到真实 0x370 时，决定透传还是注入；**echo/suppress 计数在这里++**
    - `shouldInject(handsOn, nowMs)` — 外层 G1-G6 门控（enabled/canActive/defense/handsOn/das/blackout）
    - `handsLevelFromRaw(raw)` — 从扭矩幅值推 handsOnLevel（≥200→2, ≥100→1）
  - 注意：**改这里务必同步 `test/test_native_epas_nag/test_epas_nag.cpp`**（46 个单测）。`demand_idle` 现在只由 `das∈{0,8,15}` 触发（不再有 `!apActive`）
- **`include/can_helpers.h`**（165 行）⭐ 根因所在
  - 作用：CAN 帧字段解析的纯函数
  - 最重要的函数：
    - `readDASAutopilotStatus(frame)` = `frame.data[0] & 0x0F`（0x399 byte0 低4位）
    - `isDASAutopilotActive(status)` = `status >= 3 && status <= 5`（**只认 {3,4,5}，不认 6 —— 潜伏 bug**）
    - `readGTWAutopilot(frame)` = `(data[5]>>2)&0x07`
  - 注意：改 `isDASAutopilotActive` 影响面大（FSD injection gate 等多处用 `APActive`），不要轻改
- **`include/handlers.h`**（995 行）
  - 作用：`CarManagerBase` 基类 + legacy/HW3/HW4 子类，CAN 帧分发与状态维护
  - 最重要的：`APActive` 字段（L359/541/853 三处 = `isDASAutopilotActive(readDASAutopilotStatus(frame))`）、`dasHandsOnState`、FSD 注入逻辑
  - 注意：`dashHandler` 全局指针（`mcp2515_dashboard.h:114`）指向当前 handler
- **`include/dash_epas_nag_trace.h`**（98 行）
  - 作用：`EpasNagTraceRing` —— 诊断录制 ring buffer（kCap=2000，~52B/entry）
  - 注意：flush 到 SPIFFS 延后到 loop（不在 100Hz onFrame）

### Web / CAN 服务器（巨型文件）
- **`include/web/mcp2515_dashboard.h`**（**7213 行**）⚠️ 改动要小心
  - 作用：HTTP/WebSocket 服务器 + CAN 帧分发 + Dashboard HTML + 几乎所有胶水逻辑
  - 最重要的函数/段：
    - `dashEpasNagOnFrame()`（L829）— 每帧回调，调 `tryBuildEcho`、发 echo、录 trace
    - `noteApState((bool)dashHandler->APActive, now)`（L840）— 喂 apActive 给引擎（注意：引擎已不门控它，但还记录用于 blackout 边沿）
    - `onDasGate`/`onSteeringAngle` 接线（L692/L842-850）
    - `/status` JSON 生成（含 `epasNagDiag.demand` 嵌套对象）
    - 串口命令 `epas_nag`/`epas_nag_trace`（L~5245）
  - 注意：**构建会 stamp build-id 到 `mcp2515_dashboard_ui.h`，构建后必须 `git restore include/web/mcp2515_dashboard_ui.h`**
- **`include/web/mcp2515_dashboard_ui.h`** / `mcp2515_dashboard_ui.src.h`
  - 作用：gzip 存储的 Dashboard HTML（minify 生成）
  - 注意：**永远不要 commit 对它的改动**（build stamp，构建后 restore）

### 测试文件
- **`test/test_native_epas_nag/test_epas_nag.cpp`**（589 行）⭐
  - 作用：`DashEpasNagEngine` 的 46 个 Unity 单测
  - 最重要的：`makeDemandEngine(dasState)` helper、`test_demand_injects_when_apactive_false_but_das_strong` / `..._das_light`（第 5 轮回归测试）
  - 注意：改 `dash_epas_nag.h` 必须同步这里；`pio test -e native_epas_nag` 跑
- **`test/test_epas_nag_*.py`**（5 个契约测试）
  - `test_epas_nag_config_contract.py` / `_filters_contract.py` / `_wiring_contract.py` / `_trace_contract.py` / `_trace_persistence_contract.py`
  - 作用：扫 C++ 源码，断言配置/过滤器/接线/trace 符号存在（防回归）
  - 注意：源码里 JSON 串是转义的（`\"key\"`），契约测试匹配要用转义形 `'\\"key\\"'`
- **其他 native 测试**：`test_native_hw3` / `test_native_hw4` / `test_native_legacy` / `test_native_dashboard` / `test_native_helpers` / `test_native_injection_after_ap` 等（`pio test -e <name>` 逐个跑）

### 部署相关
- **`scripts/platformio_sync_profile.py`** — pre 构建脚本，给 ui.h 盖 build-id（导致 dirty 的元凶）
- **`scripts/update_ota_build_timestamp.py`** — pre 构建脚本，OTA 时间戳
- **`firmware-assets/waveshare-single-can-standalone_20260619-apactive-fix/`** — 本次资产（full_merged 等，见第 6 节）

---

## 4. 核心逻辑说明（新 AI 理解「为什么这样写」）

### 数据流：一帧 CAN 到一次注入决策

```
车辆 CAN 总线 (0x370 @100Hz)
    ↓ TWAI 收帧
mcp2515_dashboard.h::dashEpasNagOnFrame(frame)   [L829]
    ├─ if (!dashEpasNagEnabled) 早退           [G2 开关]
    ├─ noteRuntime(enabled, canActive, defense) [刷新 G1/G2/G3]
    ├─ noteApState(dashHandler->APActive, now)  [L840 —— 注意：引擎已不门控，仅记录]
    ├─ if (0x399/0x39B) → onDasGate()           [解析 das hands-on 等级 → byte5 bits[5:2]]
    └─ if (0x370) → tryBuildEcho()              [L861]
         ├─ 记 lastRealTorque/lastRealCnt
         ├─ shouldInject(handsOn) → G1-G6 外层门控
         ├─ nextDemandTorque(das, now) → Mode-C 状态机  [★ 第5轮删了 !apActive]
         │     ├─ das∈{0,8,15} → demand_idle (不注入)
         │     ├─ das==1 → s1_hold (500ms 后停)
         │     ├─ das==2 → s2_delay→s2_mild walk
         │     └─ das==3/4/5/0xFF → strong_delay→strong_ramp→strong_hold
         ├─ [注入] 写 outData (扭矩/level/counter+1/checksum) → echoCount++
         └─ [不注入] suppressCount++, lastGate=<原因>
    ↓ 若注入
dashDriver->send(echo)  [发回总线]
    ↓ tx.recordAttempt/recordResult [TX 证据]
```

### 为什么是「回声」而不是「强制标志」
Tesla EPAS 会**交叉校验**：单纯置 handsOnLevel=1（B2 试过）+ 扭矩，车检测到「声称手在但扭矩特征像手离开」的矛盾，不信。所以上游 beta.8 的思路是：**回声真实 0x370 帧**（counter+1 保持活），注入的扭矩按 DAS 提醒等级**动态差异化**，handsOnLevel 从扭矩幅值**派生**（不硬置），让回声帧整体看起来像「人在轻微转向」。

### 容易出 bug 的地方（已踩过的坑）
1. **入口门控用一个失效信号**（本轮根因）：`!apActive` 依赖 `isDASAutopilotActive(byte0∈{3,4,5})`，对 byte0=6 的车恒 false。**教训：das 等级本身才是可靠的「AP engaged + 请求 hands-on」信号，不要再加独立 AP 门控。**
2. **测试锁定了 bug**：旧 `test_demand_ap_inactive_blocks` 断言「apActive=false 时不注入」，把错误前提固化成预期行为，导致 bug 漏网 4 轮。**教训：现有测试通过 ≠ 正确，要看测试的前提是否成立。**
3. **USB-CDC 串口打开触发 `rst:0x15` 复位**：会清空 RAM 里的 trace ring。读历史 trace 要靠 SPIFFS 回退（已实现 `epas_nag_trace` ring 空时读 `/epas_nag_trace.csv`）。
4. **trace flush 死锁**：早期 stop 检测在 onFrame 内（车断开无帧→永不触发→flush 不发生→断电丢）。已改：loop 里用 `canOnline`（真总线信号）检测 stop + 周期 5s flush。
5. **SPIFFS 写大文件阻塞**：REC_CAP=8000 + 满 SPIFFS 导致 `/rec_stop` 超时。已改 REC_CAP=2000 + save 前 `SPIFFS.remove`。
6. **版本串缓存**：`git describe` 烤进 firmware，`rm -rf .pio/build/<env>` 清缓存重建才刷新。
7. **Bash 工具 cwd 每次重置**到 `/Users/ziwind/my-vibe-project`，repo 文件必须用绝对路径或 `cd repo && ...`。

### 临时方案 / 技术债
- `apActive` 成员字段现在被 `nextDemandTorque` 弃用（只 `noteApState` 写、不再读作门控），但仍被 G6 blackout 的 `prevApActive` 边沿检测用。保留成员，未删（最小改动）。
- `isDASAutopilotActive` 不认 status=6 是**独立潜伏 bug**（影响 FSD injection gate 等多处），未修（影响面大，待 Jordan 确认 6 的语义）。
- 2 个预存测试失败（非 EPAS 引入）：`native_hw4 test_hw4_checkAD_blocks_mux0_and_mux2_send`、`native_wheel_dnd test_volume_sequence_writes_correct_steps_to_data2`——留 backlog。

---

## 5. 环境变量与配置

> **本项目无 `.env` / 无数据库连接 / 无第三方 API key。** 固件配置在 `platformio.ini`（编译期 `-D` 宏）+ NVS（运行期开关）。

### 编译期宏（`platformio.ini` `[env:waveshare_single_can_standalone]`）
- `DASH_SINGLE_CAN_STANDALONE=1` — 启用单 CAN 裁剪（必填，决定整个产物形态）
- `DASH_INITIAL_HW_MODE=3` — 初始硬件模式（3=某 HW gen，不确定具体含义，沿用）
- `TWAI_TX_PIN=GPIO_NUM_15` / `TWAI_RX_PIN=GPIO_NUM_16` — CAN 总线引脚（Waveshare 板硬连线）
- `REC_CAP=2000`（不确定是否在此 env 显式设；第 3 轮从 8000 改 2000，可能在代码默认）
- 缺失影响：宏错会导致编译出错误产物或砖

### 运行期配置（NVS + Dashboard，非环境变量）
- `dashEpasNagEnabled` — EPAS nag 总开关（**默认 false**，Dashboard 防御页 D7/D8 toggle）
- `dashDefenseEnabled` — 防御总开关（G1）
- `canActive` — CAN 注入开关（G2，接车后开）
- `nvsIsaChimeSuppress` / `nvsBanShieldEnable` / `dashCheckNagDisabled` 等 — 其他功能开关
- WiFi AP SSID `Atlas-FSD` @ `100.100.1.1`，密码 `[REDACTED]`（在 NVS/配置）

### 配置文件检查
- `.env` / `.env.example`：**firmware repo 内无**（primary 目录 `/Users/ziwind/my-vibe-project/.env.example` 是别的项目，与本项目无关）
- `config/` / `settings/`：无（配置散在 NVS + Dashboard）
- 代理配置：无
- 部署配置：`partitions_16mb_ota_4096k_nvs64.csv`（分区表）

> 不确定项：`DASH_INITIAL_HW_MODE=3` 的精确语义（沿用既有配置，未深究）；`REC_CAP` 是否在 waveshare env 显式设置（代码默认 2000，不确定 env 覆盖）。

---

## 6. 启动、运行、测试方式

> 所有命令在 repo 根 `/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware` 执行。Bash 工具 cwd 会重置，用 `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && ...`。

### 安装依赖
- `pio`（PlatformIO CLI）—— 已装。`pio pkg install` 一般不需要（lib_deps 自动拉）。
- Python 测试：`pyserial`（串口/smoke）、`pytest`（契约测试）—— 已装。

### 测试（**改代码后必须先跑这些**）
```bash
# EPAS nag 原生单测（最重要，改 dash_epas_nag.h 必跑）—— 已验证
pio test -e native_epas_nag

# 5 个 EPAS python 契约（扫源码防回归）—— 已验证
python3 -m pytest test/test_epas_nag_config_contract.py test/test_epas_nag_filters_contract.py test/test_epas_nag_trace_contract.py test/test_epas_nag_trace_persistence_contract.py test/test_epas_nag_wiring_contract.py -q

# 其他 native env（按需）—— 已验证可用
pio test -e native_hw3 -e native_hw4 -e native_legacy -e native_dashboard -e native_helpers -e native_injection_after_ap
```
- 启动成功看到：`XX Tests 0 Failures 0 Ignored` / pytest `N passed`
- 注意：`pio test`（无 -e）会对所有板载 env 跑 → 大量 ERRORED 误报，**必须显式 -e**

### 构建（**只能用 pio run，绝不能用 -t upload**）
```bash
# 清缓存刷新版本串（commit 后必须）—— 已验证
rm -rf .pio/build/waveshare_single_can_standalone

# 构建 —— 已验证（Took ~33s）
pio run -e waveshare_single_can_standalone

# 构建后去 build-id stamp —— 已验证必须做
git restore include/web/mcp2515_dashboard_ui.h
```
- 成功看到：`[SUCCESS] Took NN seconds` + `RAM: 40.1% Flash: 29.4%`
- 产物：`.pio/build/waveshare_single_can_standalone/{bootloader.bin, partitions.bin, ota_data_initial.bin, firmware.bin}`

### 烧录（**⚠️ 只能用 esptool，pio run -t upload 会砖**）
```bash
PORT=/dev/cu.usbmodem21101   # 烧录前 ls /dev/cu.usbmodem* 重新确认

# 1. 确认板子 —— 已验证
esptool.py --chip esp32s3 --port $PORT --baud 921600 flash_id
#   预期 MAC 28:84:85:49:93:fc, Detected flash size 16MB

# 2. 擦除（清 SPIFFS + 干净状态）—— 已验证
esptool.py --chip esp32s3 --port $PORT --baud 921600 erase_flash

# 3. 烧录 full_merged @ 0x0 —— 已验证（绝不分区单独烧 app）
esptool.py --chip esp32s3 --port $PORT --baud 921600 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 firmware-assets/waveshare-single-can-standalone_20260619-apactive-fix/waveshare-single-can-standalone_full_merged_20260619-apactive-fix.bin
#   预期 "Hash of data verified"
```
- **为什么 esptool 而非 pio upload**：自定义分区 app@0x20000/16MB，pio upload 用 app@0x10000/2MB 模板，冲突会砖。
- **为什么 dio**：板 eFuse 是 quad，但 bootloader header 编 dio，沿用（已验证可启动）。

### 生成资产（merge_bin）
```bash
# homebrew esptool 用 merge_bin（下划线，非 merge-bin）—— 已验证
esptool.py --chip esp32s3 merge_bin --output full_merged.bin \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 .pio/build/waveshare_single_can_standalone/bootloader.bin \
  0x8000 .pio/build/waveshare_single_can_standalone/partitions.bin \
  0x19000 .pio/build/waveshare_single_can_standalone/ota_data_initial.bin \
  0x20000 .pio/build/waveshare_single_can_standalone/firmware.bin
# full_merged 首字节应为 0xe9
```

### Smoke 验证（pyserial 抓启动日志）
- 打开串口会触发 `rst:0x15` 复位，用显式 DTR/RTS 序列抓启动日志（参考 `/tmp/smoke_apactive.py` 或记忆）
- 成功看到：`App version: v1.0-atlas-single-can-52-g2f751`、`ev-open-can-tools ready`、无 panic、`epas_nag enabled=0`

### 常见启动/构建失败
- 版本串陈旧 → `rm -rf .pio/build/<env>` 重建
- `pio run -e <env> --target build` 语法错 → 正确是 `pio run -e <env>`（无 --target）
- `File` 类无 `available()`/`readStringUntil()`（Arduino-ESP32）→ 只能用 `readString()`
- clangd 报 `unity.h not found` / `uint8_t unknown` / `constexpr unknown` → **IDE 误报，忽略，仅以 pio test/pio run 为准**

---

## 7. 已知问题与坑

### 问题：EPAS nag 实车完全无效（echo=0）—— 第 5 轮根因 ✅ 已修
- **表现**：实车 `echo=0 / suppress=3353 / Tx ok=0 / lastGate=demand_idle`，nag 不抑制，升级红色接管
- **根因**：`nextDemandTorque` 的 `!apActive` 门控；`apActive=isDASAutopilotActive(0x399 byte0∈{3,4,5})`，Jordan 车 byte0 全程=6 → 恒 false
- **有效方案**：删 `!apActive` 门控（commit `2f75109`），让 das 等级自决
- **无效方案**：B1 调扭矩、B2 调 handsOn、Mode-C 调状态机参数（都在内部，进不去）
- **相关文件**：`include/dash_epas_nag.h:158`、`include/can_helpers.h:70-78`、`include/handlers.h:359/541/853`
- **下一步**：等 Jordan 实车验证 echo>0

### 问题：das 恒=1，打通 apActive 后可能仍太弱 —— 第二层隐患 ⚠️ 未修
- **表现**：Jordan 车 0x399 byte5 全程=`0xC4` → das=1，只走 s1_hold（500ms +0Nm level 0）
- **可能原因**：das 字段 byte5 bits[5:2] 对 Jordan 车/固件可能不准，真实等级在 byte0/3/4 变化位
- **已尝试**：无（本轮只修第一层，故意不猜两层）
- **下一步**：若 echo>0 但 nag 仍在，拉 `/epas_nag_trace.csv` 看 das 实际值 + 各 byte 变化，定位真实 hands-on 等级字段

### 问题：`isDASAutopilotActive` 不认 status=6 —— 独立潜伏 bug ⚠️ 未修
- **表现**：`can_helpers.h:77` `status>=3 && status<=5`，byte0=6 判 false
- **影响**：所有依赖 `dashHandler->APActive` 的逻辑（不止 EPAS nag，还有 FSD injection gate L2337 等）
- **下一步**：Jordan 连板贴 `/status` 的 `APActive`/`GTW_autopilot` 确认 6 的语义后，单独修（影响面大，不混进 EPAS）

### 问题：USB-CDC 打开触发复位丢 trace 现场 ✅ 已缓解
- **表现**：pyserial 打开 → `rst:0x15` → RAM trace ring 清空
- **有效方案**：ring 空时回退读 SPIFFS `/epas_nag_trace.csv`（`epas_nag_trace` 命令）；loop 周期 5s flush 保数据
- **下一步**：无

### 问题：trace flush 死锁（断电丢）✅ 已修
- **根因**：stop 检测在 onFrame 内，车断开无帧→永不触发
- **有效方案**：loop 用 `canOnline` 检测 stop + 周期 flush

### 问题：/rec 录制无法结束/下载 ✅ 已修（第 3 轮）
- **根因**：SPIFFS 86% 满 + REC_CAP=8000 → save 大文件阻塞 /rec_stop 超时
- **有效方案**：REC_CAP 2000 + save 前 SPIFFS.remove + UI 乐观反馈 + erase_flash 清累积

### 问题：2 个预存测试失败（非 EPAS，留 backlog）
- `native_hw4::test_hw4_checkAD_blocks_mux0_and_mux2_send`（handlers.h:937 HW4 mux0 无视 checkAD）
- `native_wheel_dnd::test_volume_sequence_writes_correct_steps_to_data2`
- **下一步**：与 EPAS 无关，暂不碰

### 问题：origin/main 是协作者 hqs 的不相关历史线 ⚠️ 永久约束
- **表现**：`origin/main`（JordanzhaoD/LILYGO-T-2Can）是 hqs17017301634-max 的 4 提交独立线，与本地无共同祖先
- **约束**：**绝不能 force push**（会删 hqs 4 提交）；普通 push 会被拒。当前策略：本地保留，不 push。
- **下一步**：若要发布，需 Jordan 在 {force push 覆盖 / --allow-unrelated-histories 合并 / push 新分支} 三选一明确

---

## 8. 最近修改记录（时间线，新→旧）

### 修改 1：删 apActive 门控（第 5 轮，HEAD `2f75109`）✅ 已验证已烧录
- **原因**：第 4 轮 modec 资产实车 echo=0，systematic-debugging 定位根因
- **文件**：`include/dash_epas_nag.h`（删 `!apActive ||`，L158）、`test/test_native_epas_nag/test_epas_nag.cpp`（删 `test_demand_ap_inactive_blocks`，加 2 回归测试）
- **改了什么**：`nextDemandTorque` 入口从 `if(!apActive||das==0||8||15)` 改为 `if(das==0||8||15)`
- **为什么**：apActive 对 byte0=6 的车失效，das 等级才是可靠信号
- **验证**：native 46/46、python 35/35、构建 SUCCESS、烧录 Hash verified、smoke 版本 52-g2f75109 干净启动
- **影响**：EPAS nag 注入路径打通；apActive 成员弃用但保留；不影响其他模块

### 修改 2：Mode-C demand-state 状态机（第 4 轮，`49c4125`，10 commit 链）
- **原因**：B2 实车无效，对齐上游 flipper-tesla-fsd v2.16-beta.8
- **文件**：`include/dash_epas_nag.h`（整体重写扭矩内核）、`include/web/mcp2515_dashboard.h`（0x129 接线 + `/status.epasNagDiag.demand`）
- **改了什么**：固定扭矩→按 das 等级差异化状态机；固定 handsOn→从扭矩派生
- **验证**：native 43/43、python 35/35、构建 SUCCESS、已烧录（但实车 echo=0 → 引出第 5 轮）

### 修改 3：B2 handsOn=1（第 3 轮，`6537e91`）❌ 实车无效（上游已证伪）
### 修改 4：B1 固定 +1.80Nm（第 2 轮，`a420943`）❌ 实车无效（扭矩对但 handsOn 矛盾）
### 修改 5：P1 诊断层（第 1 轮，`7e2c728`）✅ 有效（本轮依赖它定位）
- 引入 `/status.epasNagDiag` + `EpasNagTraceRing` + 串口命令

### 用户确认过的需求（贯穿）
- nag 抑制要「faithful」（回声推断），不要可检测的强制标志
- EPAS nag 默认 OFF，bit-19 baseline 保留不动
- 不删 `NagHandler`（双 CAN bus4 仍可能用）
- 资产 bin 不 commit git（仓库约定，8 个历史资产全 untracked）

### 废弃方案
- 均值回归 / B1 / B2 / apActive 门控（见第 2 节「不可行」清单）

---

## 9. 下一步开发计划

### 🔴 最高优先级：实车验收 apActive 修复
- **目标**：确认第 5 轮修复在 Jordan 车上真的打通了注入路径
- **原因**：这是当前唯一阻塞——修复已烧录但未经实车验证
- **涉及文件**：无代码改动，纯验收
- **具体步骤（Jordan 做，非 AI）**：
  1. 板子接车 CAN 总线
  2. 手机连 `Atlas-FSD` WiFi，开 Dashboard 防御页
  3. 开防御总开关 + EPAS nag 子开关
  4. FSD/AP 行驶复现 nag
  5. 看 `/status.epasNagDiag`：`echo` 应 >0（不再 0）、`demand.gate` 应在 s1_hold/s2_mild/strong_* 切换
  6. 拉 `/epas_nag_trace.csv` 发 AI 分析
- **验收标准**：echo > 0（证明根因修对）+ demand.gate 不再恒 demand_idle

### 🟡 第二优先级：若 nag 仍在，定位第二层（das 字段映射）
- **目标**：搞清 Jordan 车 0x399 的真实 hands-on 等级在哪个 byte/bit
- **原因**：byte5 全程 `0xC4`（das=1）可疑，真实等级可能在变化的 byte0/3/4
- **具体步骤**：
  1. 拿 Jordan 的 trace CSV + 原始 CAN 录制（如 `can_recording (3).csv`）
  2. 对照 nag 触发/升级时刻，看 0x399 哪个 byte/bit 在变化
  3. 查 Tesla 0x399 community 逆向资料确认字段
  4. 若字段错，改 `onDasGate`（dash_epas_nag.h:241）+ 同步测试
- **验收标准**：das 等级能正确反映车的真实 nag 强度（strong nag 时 das=3/4/5）

### 🟢 第三优先级：修 `isDASAutopilotActive` status=6（独立 bug）
- **目标**：让 `APActive` 对 byte0=6 的车也正确
- **原因**：影响 FSD injection gate 等多处，是潜伏 bug
- **具体步骤**：
  1. Jordan 连板贴 `/status` 的 `APActive` / `GTW_autopilot` 值，确认 6=FSD engaged？
  2. 若是，`can_helpers.h:77` 改 `status >= 3 && status <= 6`（或 `>= 3`）
  3. **评估影响面**（FSD injection gate 等所有用 APActive 的地方），加测试
- **验收标准**：byte0=6 时 APActive=true，且 FSD/nag 行为不回归

### ❌ 现在不要做的事
- **不要回去调扭矩固定值 / handsOn 固定值**（B1/B2 已证伪，上游 beta.8 否定）
- **不要重新加 apActive 门控**（刚删的根因）
- **不要 `pio run -t upload`**（会砖）
- **不要 force push / push origin/main**（hqs 不相关历史）
- **不要在 onFrame 里 flush SPIFFS**（阻塞 100Hz 热路径）
- **不要 commit `mcp2515_dashboard_ui.h`**（build stamp，构建后 restore）

### ⚠️ 容易过度开发的地方
- 不要为「可能的车 type」加一堆配置开关（Jordan 只有一台车，YAGNI）
- 不要重构 `mcp2515_dashboard.h`（7213 行，风险高，无收益）
- 不要加「可视化 trace 分析」等花哨功能（先让 nag 真的消失）

### 🔬 要先验证再写代码的地方
- 任何 das 字段改动 → 先拿 trace CSV 证实真实字段位置
- 任何扭矩值改动 → 先确认能进入 `nextDemandTorque` 内部（apActive 已不门控，但 das∈{0,8,15} 仍排除）
- 任何 `isDASAutopilotActive` 改动 → 先评估 FSD injection gate 等影响面

---

## 附：关键约束速查（新 AI 务必遵守）

1. **不 push 任何 remote**（origin/main 是 hqs 不相关线，绝不 force push）
2. **只用 `esptool.py write_flash 0x0 full_merged.bin`** 烧录，**绝不用 `pio run -t upload`**（砖）
3. **EPAS nag 默认 OFF**（`dashEpasNagEnabled=false`）；bit-19 baseline 默认开不动；不删 `NagHandler`
4. **构建后 `git restore include/web/mcp2515_dashboard_ui.h`**（去 build-id stamp）
5. **commit 后 `rm -rf .pio/build/waveshare_single_can_standalone`**（刷新版本串）
6. **Bash 工具 cwd 每次重置**，用绝对路径或 `cd repo && ...`
7. **`File` 类只有 `readString()`**（无 available/readStringUntil）
8. **忽略 clangd 诊断**（unity.h/uint8_t/constexpr 误报），仅以 `pio test`/`pio run` 为准
9. **资产 bin 不 commit git**（仓库约定）
10. **改 `dash_epas_nag.h` 必须同步 `test_epas_nag.cpp`** + 跑 `pio test -e native_epas_nag` + 5 个 python 契约
