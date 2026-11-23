# TX 分路 v2（offsetOnly 路由 + bit27 nagKill）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 0x3EE 总开关只关激活注入：速度偏移改走既有 offsetOnly 通道（beta02 田野写入器），DND 加事件内自愈，bit27 nagKill 作为并行免打扰路径（默认关）。

**Architecture:** master-off 分支内独立精简速度通道（复用 computeLegacySpeedOffset + dashWriteLegacyOffsetTo3eeMux0，不 fall-through 主路径）；DND 引擎加 disarmedAtMs_ 时间戳与自愈；新 nagKill 引擎 RX 驱动骑载 + 与 DND 互认让路。

**Tech Stack:** ESP-IDF (pio espidf), Unity native tests, Arduino web server。

**Spec:** `docs/TX-SPLIT-RIDE-0X3EE-DESIGN-20261123.md`

## Global Constraints

- 滚轮 DND 配对引擎本体不动（只加自愈+互认回调）
- 开关开（默认）= beta02 逐字节行为
- bit27：hands∈[3,5] 单发、永不在 hands≥6 动作、NVS 键 `nk27` 默认 off
- 0x2F8 骑载整体移除（diag 字段 ride2f8TxOk/rideBlockedReason 一并删）
- 版本 `6.1.0-beta.05`（点号式）；VERSION bump 后必须 touch CMakeLists.txt
- ESP 构建：`IDF_MAINTAINER=1 pio run -e lilygo_t2can_dual`（beta02 基线无 wrapper）

## Review Focus

1. master-off+偏移开的帧必须与开关开的 offsetOnly 输出逐字节一致（字段映射错=偏移失效）→ Task 2 奇偶钉
2. DND 自愈跑在 wasWarning 计算前，且健康引擎的普通警告沿不得 resetRuntime（否则清掉刚开的 episode）→ Task 3 顺序钉+回归钉
3. bit27 与 DND 同开时 counter 竞争：bit27 发射拍 DND 必须零帧 → Task 6 让路钉
4. bit27 帧回来（own-echo）不得再触发克隆（链式风暴）→ Task 5 echo 钉
5. ci 改纯源后 canActive=false+开关=on 必须显示开（老板自车板 canActive 曾被旧总开关清 0）→ Task 4 纯度钉

---

### Task 1: 移除 0x2F8 骑载 + 迁移旧钉

**Files:**
- Modify: `include/handlers.h:73-74`（diag 字段）、`:805-845`（760 处理器 ride 块）
- Modify: `include/web/mcp2515_dashboard.h:2647-2648`（JSON）
- Modify: `test/test_native_legacy/test_legacy_handler.cpp:1223-1250,1336`

**Interfaces:**
- Produces: `legacySpeedDiag` 无 ride2f8TxOk/rideBlockedReason；`test_3ee_master_off_rides_offset_on_native_2f8` 删除

- [ ] **Step 1: 重写 master-off 钉为占位**

把 `test_3ee_master_off_stops_all_0x3ee_writes`（:1213）改名 `test_3ee_master_off_mux1_zero_and_diag`：断言 master-off+偏移 Off 时喂 mux1 → 零 send、`legacySpeedDiag.blockedReason=="fsd3eeMasterOff"`。删除 `test_3ee_master_off_rides_offset_on_native_2f8` 整个函数与 RUN_TEST 行。

- [ ] **Step 2: 跑测试确认编译失败**

Run: `pio test -e native_legacy`
Expected: FAIL（ride2f8TxOk 字段还在引用/编译错或断言失败）

- [ ] **Step 3: 删 handlers.h 的 ride 字段与 760 ride 块**

删 `:73-74` 两字段；删 760 处理器内整个 A3 mirror-ride if 块（`fsd3eeInjectAllowed && !fsd3eeInjectAllowed() &&` 起至块尾），保留 760 的只读 GPS 诊断部分。

- [ ] **Step 4: 删 dashboard JSON 的 ride 字段**

`mcp2515_dashboard.h:2647-2648` 附近删 `"ride2f8TxOk"`/`"rideBlockedReason"` 输出。

- [ ] **Step 5: 跑 native_legacy 全绿**

Run: `pio test -e native_legacy`
Expected: 67/67 PASS（68-2 钉迁移+1 新占位=67）

- [ ] **Step 6: Commit**

`git commit -m "refactor(t2can): remove the dead 0x2F8 mirror-ride (no host frame on X197)"`

### Task 2: master-off 的 offsetOnly 路由

**Files:**
- Modify: `include/handlers.h:899-907`（master-off 分支）
- Test: `test/test_native_legacy/test_legacy_handler.cpp`

**Interfaces:**
- Consumes: `computeLegacySpeedOffset(nowMs)`、`dashWriteLegacyOffsetTo3eeMux0(data,offset,pct)`（dash_legacy_smart_offset.h:70）、`legacySpeedBus.accept/ambiguous`、`abortGuard`、`builtInFsdInjectionAllowed()`、`jitterInhibit3ee`
- Produces: master-off+mux0+偏移开 → 恰一帧 offsetOnly 发送；diag `offsetOnlyTxOk++`；`blockedReason` 流转 gateBlocked/abortGuard/jitterProcedure/none

- [ ] **Step 1: 写失败测试（三钉）**

`test_3ee_master_off_rides_offset_via_offsetonly`：开关关+Manual 偏移+喂原生 mux0 → 恰一帧 send；断言字节：`b3`（偏移 kph+30 进 bits1-6|0x80）、`b5|=0x03`、`b7`=百分比、bit46（b5 bit6）与原生相同（中立）。
`test_3ee_master_off_offset_parity_with_switch_on`：同输入开关开跑主路径，捕获 send 帧，与开关关的输出逐字节相等。
`test_3ee_master_off_no_offset_zero_tx`：开关关+偏移关 → mux0/mux1 全零 send。

- [ ] **Step 2: 跑测试确认失败**

Run: `pio test -e native_legacy`
Expected: 新钉 FAIL（当前 master-off 一刀切 return）

- [ ] **Step 3: 实现 master-off 精简速度通道**

master-off 分支重构为：
```
if (fsd3eeInjectAllowed && !fsd3eeInjectAllowed()) {
    if (readMuxID(frame) != 0) return;              // mux1 零发送
    if (!legacySpeedOffsetRequested()) { legacySpeedDiag.blockedReason="fsd3eeMasterOff"; return; }
    // gate 链（顺序照主路径）：abortGuard → jitter → legacySpeedBus.accept → injectionAllowed → builtInFsdInjectionAllowed
    // 每个失败写对应 blockedReason 后 return
    uint8_t off = computeLegacySpeedOffset(nowMs);
    if (off == 0) return;
    CanFrame ride = frame;  dashWriteLegacyOffsetTo3eeMux0(ride.data, off, legacySpeedDiag.outputOffsetPct);
    // 诊断四联（mux0Rx/Tx Byte3/5/7 照主路径）+ send + ok? offsetOnlyTxOk++ : offsetOnlyTxFail++
}
```
不更新 fsdTriggered/ADEnabled/mux0 激活 diag/jitterObserve3ee（beta.04 行为保持）。

- [ ] **Step 4: 跑 native_legacy 全绿**

Run: `pio test -e native_legacy`
Expected: 70/70 PASS

- [ ] **Step 5: Commit**

`git commit -m "feat(t2can): master-off routes mux0 speed to the offsetOnly channel (field-verified writer)"`

### Task 3: DND 事件内自愈

**Files:**
- Modify: `include/dash_legacy_wheel_045_dnd.h`（引擎）
- Test: `test/test_native_legacy_wheel_045_dnd/test_legacy_wheel_045_dnd.cpp`

**Interfaces:**
- Produces: `void healTick(uint32_t nowMs)`（800ms 自愈+3 次/30s 冷却）；`onDasStatus` 开头调用；新 diag 字段 `disarmHeals`、`disarmCooldownUntilMs`

- [ ] **Step 1: 写失败测试（四钉）**

`test_disarm_heals_after_800ms`：两连 TX 失败 disarm → 800ms 后喂原生 0x45+tick → 引擎复活且发出配对首帧。
`test_disarm_heal_capped_three_per_episode`：同 episode 第 4 次 disarm 后不再自愈。
`test_disarm_cooldown_30s_then_edge_revives`：3 次后进 30s 冷却；冷却内新沿忽略；期满后警告新沿复活。
`test_healthy_engine_edge_no_reset`（回归）：健康引擎普通警告沿 → 状态机正常进 episode，`diag.pairsThisEpisode` 不被清。

- [ ] **Step 2: 跑测试确认失败**

Run: `pio test -e native_legacy_wheel_045_dnd`
Expected: 新钉 FAIL（无 healTick）

- [ ] **Step 3: 实现自愈**

引擎加 `disarmedAtMs_`、`healsThisEpisode_`、`kHealDelayMs=800`、`kMaxHealsPerEpisode=3`、`kDisarmCooldownMs=30000`。
`healTick`：`autoDisarmed_ && now-disarmedAtMs_>=800 && healsThisEpisode_<3 && now>=cooldownUntilMs_` → `resetRuntime()`（清 autoDisarmed_/failures）+ `healsThisEpisode_++`。disarm 处（:473-476）记 `disarmedAtMs_=now`、`healsThisEpisode_>=3` 时设 `cooldownUntilMs_=now+30000`。
**调用点=onDasStatus 开头（wasWarning 计算前）**；episode 开始处（:283 `warningActive_ && !wasWarning`）重置 `healsThisEpisode_=0`。

- [ ] **Step 4: 跑 DND 套件全绿（含既有 18 钉零回归）**

Run: `pio test -e native_legacy_wheel_045_dnd`
Expected: 22/22 PASS

- [ ] **Step 5: Commit**

`git commit -m "feat(t2can): wheel DND in-event disarm self-heal (800ms, 3x cap, 30s cooldown)"`

### Task 4: ci 单一真源

**Files:**
- Modify: `include/web/mcp2515_dashboard.h:3049-3056`
- Test: `test/test_native_dashboard/`（新增钉）或 py 合约

**Interfaces:**
- Produces: `/status ci` == `dashFsd3eeInject`（纯）；topbar `can` 不变

- [ ] **Step 1: 写失败测试**

native_dashboard 钉：构造 `canActive=false, dashFsd3eeInject=true` → status JSON `ci==true`；`canActive=true, dashFsd3eeInject=false` → `ci==false`。

- [ ] **Step 2: 确认失败 → 改实现**

`j += (canActive && dashFsd3eeInject) ? "true" : "false";` → `j += dashFsd3eeInject ? "true" : "false";` 注释更新（移除 beta02 的 canActive 耦合说明）。

- [ ] **Step 3: 全绿后 Commit**

Run: `pio test -e native_dashboard`
`git commit -m "fix(t2can): /status ci is the pure fsd_3ee_inject single source"`

### Task 5: bit27 nagKill 引擎（纯 header + 测试）

**Files:**
- Create: `include/dash_legacy_nag_kill_045.h`
- Create: `test/test_native_nag_kill/test_nag_kill.cpp` + platformio.ini env `native_nag_kill`

**Interfaces:**
- Produces: `class DashLegacyNagKill045`：
  - `void configure(bool enabled)` / `void setRuntimeAllowed(bool, reason)`
  - `void onDasStatus(uint8_t apState, uint8_t handsState, uint32_t nowMs)`（hands∈[3,5] 置标志；≥6 硬关）
  - `bool onNative045(const uint8_t data[8], uint32_t nowMs, CanFrame *out)`（标志在→克隆：bit27（byte3|=0x08）+counter（b6 高半字节）+1+CRC8-J1850→out 返回 true；单发清标志）
  - `void recordTxResult(bool ok)`（两连败 disarm+800ms 自愈同 Task 3 参数）
  - `static bool isCloneFrame(const uint8_t data[8])`（bit27 位检测）
  - `bool cloneInFlight(uint32_t nowMs)`（最近 20ms 发过克隆）
  - `struct Diag { enabled, cloned, disarmed, heals, blockedReason... } diag`

- [ ] **Step 1: 写全套失败测试**

钉：①hs=3+原生帧→恰一帧（位断言 byte3 bit3=1、counter+1、CRC 正确——用既有 CRC8 J1850 真值函数）②hs=2/hs=6→零 ③单发（同沿第二原生帧零）④echo：isCloneFrame 帧→onNative045 返回 false ⑤nativeFresh 150ms 外→零 ⑥两连败 disarm→800ms 自愈 ⑦默认 configure(false) 全零 ⑧diag 计数

- [ ] **Step 2: 确认失败 → 实现引擎**

结构照 `dash_legacy_wheel_045_dnd.h` 骨架（Diag/configure/状态机精简版）。CRC8 复用仓库既有 `crc8_j1850`（can_helpers.h / 单 CAN v1.19 移植件——grep 确认符号名）。

- [ ] **Step 3: 新 env 跑绿**

Run: `pio test -e native_nag_kill`
Expected: 全 PASS

- [ ] **Step 4: Commit**

`git commit -m "feat(t2can): bit27 nagKill ride engine (single-shot per warning edge, [3,5] window)"`

### Task 6: bit27 集成接线 + DND 互认让路

**Files:**
- Modify: `src/main.cpp`（RX 分发 + tick）
- Modify: `include/dash_legacy_wheel_045_dnd.h`（sibling 回调）
- Modify: `include/web/mcp2515_dashboard.h`（NVS nk27 + /defense_config `nag_kill` + 状态绑定）
- Test: `test/test_native_legacy_wheel_045_dnd/`（互认钉）+ `test/test_native_legacy/`（集成钉）

**Interfaces:**
- Consumes: Task 5 引擎全套；Task 3 DND healTick
- Produces: 全局 `dashNagKillCtrl`；DND 引擎 `void setSiblingCloneFilter(bool (*fn)(const uint8_t*))`；t2canNagKillTick()

- [ ] **Step 1: 写失败测试**

DND 侧：`test_dnd_ignores_bit27_sibling`——onCan1Frame 喂 bit27=1 帧 → nativeFresh 不刷新、不触发配对。
让路：`test_dnd_yields_when_nagkill_in_flight`——nagKill.cloneInFlight() 为真时 DND tick 零帧（此钉在集成层：main.cpp 无 native 桩，落在 DND 引擎新增 `setYieldGate(bool (*fn)(uint32_t))` 回调上钉）。

- [ ] **Step 2: 实现互认**

DND `onCan1Frame` 的原生帧判定处插 `if (siblingCloneFilter_ && siblingCloneFilter_(data)) { 记 echo 统计; return; }`；`nextFrame` 入口插 yieldGate 检查。

- [ ] **Step 3: main.cpp 接线**

0x45 RX 分发处（DND onCan1Frame 旁）：先 `isCloneFrame` 自检（own-echo 静默），再喂 nagKill.onNative045→true 则 driver.send+recordTxResult；onDasStatus 分发处同时喂 nagKill；tick 链加 `t2canNagKillTick()`（heal 自愈）。绑定 DND 的 siblingCloneFilter=nagKill.isCloneFrame、yieldGate=nagKill.cloneInFlight。

- [ ] **Step 4: dashboard 绑定**

NVS `nk27` 读入（默认 false）→ `dashNagKillEnabled`；`/defense_config` 增 `nag_kill` 参数；`/status` 增 `nagKillDiag` JSON 段（enabled/cloned/disarmed/heals/blockedReason）+ `legacySmartSpeed` 增 `offsetOnlyTxOk`/`offsetOnlyTxFail` 暴露（已计数未上 JSON）。

- [ ] **Step 5: 全 native 套件绿 + ESP 构建绿**

Run: `pio test -e native_legacy -e native_legacy_wheel_045_dnd -e native_nag_kill -e native_dashboard && IDF_MAINTAINER=1 pio run -e lilygo_t2can_dual`

- [ ] **Step 6: Commit**

`git commit -m "feat(t2can): wire nagKill into the CAN task with DND sibling/yield coordination"`

### Task 7: UI（开关+文案+i18n）

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`（defense 页 DND 上方新行）+ 重新 minify 生成 ui.h

**Interfaces:**
- Consumes: `/defense_config nag_kill`、`/status nagKillDiag`

- [ ] **Step 1: 加开关行（「Legacy 左滚轮免打扰」上方）**

`nag-kill-tgl`，`onchange="saveDefenseConfig()"`；回填读 `d.nag_kill`（undefined→false）；实验徽标；描述：「警告沿单帧骑载：克隆下一个原生 0x045 置 bit27+续 counter+重算 CRC；与滚轮免打扰并行可单开；实验功能默认关」。0x3EE 开关描述同步改（去 0x2F8 字样→offsetOnly 语义+jitter 连带说明）。i18n 英文对译。

- [ ] **Step 2: minify + blob 钉**

`/usr/bin/python3 scripts/minify_dashboard.py`；断言 ui.h 内 `nag-kill-tgl` 在 `def-dnd-vol-tgl` 之前、旧 `0x2F8` 字样为零。

- [ ] **Step 3: dashboard 套件绿 + Commit**

`git commit -m "feat(t2can): nagKill toggle above wheel DND + offsetOnly copy fixes"`

### Task 8: 发版 6.1.0-beta.05

- [ ] **Step 1: VERSION=`6.1.0-beta.05` + touch CMakeLists.txt + CHANGELOG**
- [ ] **Step 2: 全门禁**（native 全家 + `IDF_MAINTAINER=1 pio run -e lilygo_t2can_dual`）
- [ ] **Step 3: commit 源码 → 重构建（uiBuildId 戳 hash）→ commit 戳**
- [ ] **Step 4: 资产目录**（ota_app/full_merged/bootloader/partitions/ota_data + SHA256SUMS + 手写 README：版本系列说明/TX 分路 v2/nagKill 实验披露/OTA 命令 POST /update + X-File-Name/田野清单四步）
- [ ] **Step 5: 镜像自检**（zlib 解压 gzip blob 断言：版本号/uiBuildId/nag-kill-tgl 位置/无 0x2F8 字样）
- [ ] **Step 6: latest 三指针 + push + 记忆更新**
