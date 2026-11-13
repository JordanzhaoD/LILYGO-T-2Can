# Alert Telemetry（告警取证）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 降级日自动取证仪器——五源常驻哨兵 + severity 门控自动抓拍 + NVS 事件日志 + 防护页告警卡。

**Architecture:** 纯核心头文件 `include/dash_alert_watch.h`（仿 `dash_ban_forge.h`：无堆/无 Serial/inline 联结）承载检测（哨兵/去重/severity/监听环/抓拍状态机）；车辆接线层（`src/main.cpp` + `include/web/mcp2515_dashboard.h`）承载反应（NVS/黑盒开闸锁/JSON/UI）。回灌经新引擎面 `dashRecV2SubmitRecord`（保留原始 tsUs）。

**Tech Stack:** ESP32-P4 / ESP-IDF(arduino) / PlatformIO（native 测试 env + p4_3can 构建）/ pytest 源码契约测试。

**Spec:** `docs/superpowers/specs/2026-11-13-alert-telemetry-design.md`（已批准，含 §8 评审裁决——本计划的每条裁决都从那里来）

## Global Constraints

- 零总线发送：本特性任何代码路径不得调用任何 TX 函数；CanC 保持 LISTEN_ONLY（spec §2 反目标）
- fail-closed：偏好默认开（纯被动），但异常路径不得留半开状态（闸开着没人关、状态机卡 CAPTURING）（spec §3）
- NVS 键名 ≤15 字符：只用 `alertEvLog` `alertBoot` `alert_watch`（spec §3，静默拒写坑）
- 黑盒锁参数域 pre 10-600s / post 5-300s（`dashRecV2LockRequest` 自带强制的单一真源）
- 帧记录 18B 布局冻结：核心用自己的 `DashAlertRec`（static_assert 18），接线层 memcpy 对接 `RecRingFrame`（核心不得 include dash_recorder_v2.h）
- commit 纯英文；ESP 构建必走 `scripts/pio-build.sh`；ui.h 纪律=源码 commit → p4_3can 构建 → ui.h 单独 commit；门禁顺序 native → ESP build → pytest
- 自动抓拍会话必须容量感知自清理（spec §8④）；post 窗上限 60s（spec §8④）

## Review Focus

测试没覆盖但最可能咬人的五类（每条已钉进 owning task）：

1. **0x368 载荷洪水**：降级日每帧偏离模板——不去重则事件日志 2.4s 刷满（Task 1 钉：distinct 载荷去重测试）
2. **0x346 跨重启伪上升沿**：告警锁存期板子重启→RAM 位图清空→每 boot 一条重事件（Task 1 钉：空位图+置位帧=事件的边沿测试；Task 5 钉：`alertBoot` bootSeq 递增并烙进每条事件）
3. **黑盒容量耗尽**：多事件降级日 3×64 块>144 块→拒写（Task 5 钉：驱逐后仍不足→降级 log-only + 事件记失败原因）
4. **FLUSH 时闸未开/中途关闸**：回灌提交落到关着的闸上必须安全丢弃计数，不得崩溃或绕过闸（Task 4 钉：gate-closed 提交=丢弃+计数）
5. **用户已手动武装串流时触发**：不得二次开闸/不得回灌污染手动会话的时间轴（Task 3 钉：armed-in-progress→跳过 FLUSH 直接补锁）

---

### Task 1: 核心检测——哨兵、事件模型、severity、去重

**Files:**
- Create: `include/dash_alert_watch.h`
- Create: `platformio.ini`（新增 `[env:native_alert_watch]`，仿 `native_ban_forge` 块）
- Create: `test/test_native_alert_watch_main.cpp`
- Test: `test/test_alert_watch_contract.py`（pytest 源码契约，仿 `test_ban_forge_contract.py`）

**Interfaces:**
- Produces（后续任务依赖的精确签名）:
```cpp
struct DashAlertEvent { uint32_t bootSeq, tMs; uint16_t src; uint8_t code, sev, data[8]; bool captureFailed; };
// src 枚举: kAlertSrcTripwire=1, Matrix346=2, Log526=3, Log5A5=4, Log524=5
// sev: kSevLog=0, kSevCapture=1
bool dashAlertWatchOnFrame(uint32_t id, uint8_t dlc, const uint8_t *data, uint32_t nowMs); // 返回=有新事件
bool dashAlertWatchOnTripwire(const uint8_t deviant[8], uint32_t nowMs);
const DashAlertEvent *dashAlertWatchLastEvent();
uint8_t dashAlertWatchClassify(uint32_t id, uint8_t code); // 查表: sev
const char *dashAlertWatchName(uint16_t src, uint8_t code); // 解码名（w/a 共享 64 条表 + PM 专表，从 catalog.json 机械生成）
```
- 常量：监听集 14 ID 表 `kAlertWatchIds[]`（0x368,0x346,0x526,0x5A5,0x524,0x3EE,0x398,0x2F1,0x126,0x356,0x3C5,0x396,0x33A,0x728）

- [ ] **Step 1: 写失败测试**（native main）：四场景——(a) 0x526 帧 b0=0x90(a144)→事件 sev=capture；(b) 0x526 b0=0x3E(a062)→capture，b0=0x0A→log-only；(c) 0x346 首帧 `00..00`→无事件，次帧 b7bit5 置位→w062 事件，重播同帧→无新事件；(d) 同一 deviant 载荷喂 OnTripwire 100 次→恰 1 事件，换载荷→第 2 事件。契约测试钉：头文件含 `static_assert(sizeof(DashAlertRec) == 18)`、无 `Serial`、无 `heap_caps`。
- [ ] **Step 2: 跑 native 确认失败**：`scripts/pio-build.sh test -e native_alert_watch` → 编译失败（头文件不存在）
- [ ] **Step 3: 实现核心头**：哨兵=ID 查表（14 ID 编译期 switch/表）+0x346 RAM 位图对比记上升沿+日志帧码提取；severity 表（CAPTURE 集=spec §5.2 清单硬编码）；tripwire 载荷去重（最近 distinct 载哈希集合，容量 8 滚动）；事件环 RAM 24 条
- [ ] **Step 4: 跑 native + 契约确认绿**
- [ ] **Step 5: Commit** `feat(alert): core sentinels, severity gate, payload dedupe`

### Task 2: 核心监听环与回灌迭代器

**Files:**
- Modify: `include/dash_alert_watch.h`
- Modify: `test/test_native_alert_watch_main.cpp`、`test/test_alert_watch_contract.py`

**Interfaces:**
```cpp
void dashAlertWatchRingInit(uint8_t *buf, size_t cap);      // 接线层给 PSRAM
bool dashAlertWatchRingPush(uint32_t id, uint8_t dlc, const uint8_t *data, uint32_t tsUs); // 命中监听集才入环
size_t dashAlertWatchRingCount();
bool dashAlertWatchRingPopOldest(DashAlertRec *out);        // 时间序出环（回灌用）
```

- [ ] **Step 1: 失败测试**：非监听 ID 不入环；监听 ID 入环且满环覆盖最旧；PopOldest 按入序返回；DashAlertRec 18B
- [ ] **Step 2: RED**（同 Task 1 命令）
- [ ] **Step 3: 实现**：环形数组+写指针，无动态分配，buf 为空时 Push 直接丢（安全退化）
- [ ] **Step 4: GREEN + Commit** `feat(alert): watch-set ring with ordered drain`

### Task 3: 核心抓拍状态机（poll/action 协议）

**Files:**
- Modify: `include/dash_alert_watch.h`、两个测试文件

**Interfaces:**
```cpp
enum class DashAlertAction : uint8_t { None, ArmFlush, Lock };
struct DashAlertPoll { DashAlertAction action; uint32_t preMs, postMs; uint8_t busMask; };
DashAlertPoll dashAlertWatchPoll(uint32_t nowMs);
void dashAlertWatchNotifyCaptureBegin(bool ok, const char *reason); // ok=false→事件标 captureFailed+reason,回 IDLE
void dashAlertWatchNotifyLockDone(bool ok);
void dashAlertWatchSetExternalRecording(bool active);  // 用户手动武装中
```
状态机（spec §5.2 修订版）：IDLE --capture事件--> PENDING_FLUSH（poll 返 ArmFlush, pre=300000, post=30000, mask=0x05）--begin(ok)--> CAPTURING --post 窗到（CAPTURE 级再触发 +15s，上限 60s）--> poll 返 Lock --done--> COOLDOWN 60s --> IDLE。externalRecording=true 时跳过 ArmFlush 直接进 CAPTURING（不回灌）。

- [ ] **Step 1: 失败测试**：完整迁移链（注入 capture 事件→poll=ArmFlush→notify(ok)→时间快进 30s→poll=Lock→notify→COOLDOWN→快进 60s→IDLE）；再触发延长（30s+15s≤60s 上限钉死）；externalRecording 路径不返 ArmFlush；notify(false)→IDLE+事件 captureFailed=true；LOG 级事件永不触发 ArmFlush
- [ ] **Step 2: RED** → **Step 3: 实现**（纯状态+时间比较，时间只经 nowMs 入参）→ **Step 4: GREEN**
- [ ] **Step 5: Commit** `feat(alert): capture state machine with severity gate and capacity degrade`

### Task 4: 引擎时间戳提交面 `dashRecV2SubmitRecord`

**Files:**
- Modify: `include/web/dash_recorder_v2.h`（`dashRecV2Submit` 旁新增，复用其全部闸门/落盘路径）
- Test: `test/test_blackbox_recorder.py`（新增契约）

**Interfaces:**
```cpp
static void dashRecV2SubmitRecord(const RecRingFrame &r); // tsUs 用 r.tsUs（回灌保留原时间）
```

- [ ] **Step 1: 失败契约测试**（test_blackbox_recorder.py 新增，钉四条）：存在 `dashRecV2SubmitRecord(const RecRingFrame &r)`；热路径首检与 `dashRecV2Submit` 同闸（`recBBEnabled && recBBStreamArmed`——关闸=零工作零闪存）；不重算 tsUs（钉 `r.tsUs` 直用、无 `esp_timer_get_time`）；staging 背压路径复用（同 drop+overflow 计数）
- [ ] **Step 2: RED**（pytest 该文件）
- [ ] **Step 3: 实现**（体=拷贝 dashRecV2Submit 去掉 CanFrame→RecRingFrame 组装与打戳，10 行内）
- [ ] **Step 4: GREEN + Commit** `feat(recorder): stamped submit for forensic ring backfill`

### Task 5: 车辆接线——钩子、NVS、抓拍执行、端点

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`（CanC 派发块 ban forge 后、/status、/alert_events、defense_config、loop 轮询点若在此）
- Modify: `src/main.cpp`（PSRAM 256KB 环分配 + poll 执行器，或按现有 loop 结构就近）
- Test: `test/test_alert_watch_contract.py`（扩充）

**Interfaces:**
- Consumes: Task 1-4 全部；`dashRecV2LockRequest`、`dashRecV2SetStreamArmed`、`recV2RemoveSession`、`dashRecV2HasWall`
- Produces: `/alert_events` GET（JSON：最近 24 事件+bootSeq+解码名）；/status 新块 `alertWatch`；defense_config 键 `alert_watch`（默认 1）

抓拍执行器（poll 返回 ArmFlush 时）：① 空闲块=未保护块数×64KB，需求=环字节数+60s×33KB/s 估算；② 逐个 `recV2RemoveSession` 驱逐**自动会话**（meta 带 `"auto":true` 标记——锁定后补写该字段或用 seq 登记表）直到放得下；③ `dashRecV2SetStreamArmed(true)`；④ `PopOldest`→memcpy→`dashRecV2SubmitRecord` 循环回灌；⑤ 通知 core begin(true)。Lock 动作：`dashRecV2LockRequest(300000, 30000, 0x05, err)`→done(ok)→`dashRecV2SetStreamArmed(false)`（仅当本次是我们开的闸）。

- [ ] **Step 1: 失败契约测试**（源码 token 级）：CanC 派发块内 ban forge 块之后调用 `dashAlertWatchOnFrame`；tripwire 计数差分喂 `dashAlertWatchOnTripwire`；NVS 只出现 `alertEvLog`/`alertBoot`/`alert_watch` 三个键；存在 `/alert_events` 路由；`alertWatch` JSON 块在 /status；驱逐路径含 `recV2RemoveSession` 且有 `"auto"` 标记；ArmFlush 执行器含容量估算与"仍不足→notify(false)"
- [ ] **Step 2: RED** → **Step 3: 实现接线**（环 malloc MALLOC_CAP_SPIRAM 256KB 失败→特性自禁用+dashLog，fail-closed）
- [ ] **Step 4: GREEN**（pytest）
- [ ] **Step 5: Commit** `feat(alert): vehicle wiring — hook, NVS events, auto-capture executor, endpoints`

### Task 6: 防护页告警卡 + UI

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`（防护页 ban forge 卡后新增"告警取证"卡 + i18n 词条）
- Modify: `include/web/mcp2515_dashboard_ui.h`（构建再生，单独 commit）
- Test: `test/test_alert_watch_contract.py`（ui.src token 契约）

卡片内容（spec §5.4）：状态行（监听帧数/环占用/最近事件/抓拍状态）、0x346 锁存位图（解码名≤8）、事件列表（10 条：时间+源+名+hex）、总开关（默认开）、事件 JSON 导出链接+提示"抓拍会话在录制会话列表下载"。

- [ ] **Step 1: 失败契约**：ui.src.h 含 `id="alw-"` 前缀元素集（`alw-status/alw-matrix/alw-events/alw-tgl`）、i18n 双语词条、fetch `/alert_events`、`set('st-` 轮询接入既有 status 轮询
- [ ] **Step 2: RED** → **Step 3: 实现 UI**（复用防护页卡片样式与轮询函数）
- [ ] **Step 4: GREEN** → **Step 5: 源码 Commit** `feat(alert): defense-page alert forensics card` → **Step 6: pio 构建**（`scripts/pio-build.sh run -e p4_3can`）→ **Step 7: ui.h 单独 Commit** `chore(ui): regenerate dashboard blob (alert card)`

### Task 7: 台架端到端验证 + 门禁收口

**Files:**
- Create: `docs/alert-watch-bench-2026-11-14.md`（台架步骤与预期输出）

- [ ] **Step 1: 门禁全跑**：native 全 25+1 env → `pio-build.sh run -e p4_3can` → 全量 pytest
- [ ] **Step 2: 台架重放**（需板 2 + 串口）：`rv2_frame 3 0x526 8 90 00 20 00 00 00 00 00`（a144 配置告警，sess_124 原始帧）→ 预期：事件入列、自动开闸、~30s 后锁定、会话可下载、`/alert_events` 可见；再重放 `3E 00 19 ...`（a062）；再重放一条 LOG 级码→只记事件不开闸
- [ ] **Step 3: 记录台架结果入 docs + Commit** `test(alert): bench replay of sess_124 alert frames, gates green`
- [ ] **Step 4: 汇报 Jordan**（台架过=可 OTA 板 2 装车，田野静默一周验零误报）
