# 移除 EPAS-Faithful Nag 注入引擎 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 物理删除 `DashEpasNagEngine`（向 0x370 注入篡改帧、已在实车触发车身控制故障的引擎）及其全部测试/UI/诊断，并还原 bit-19 老耦合，让固件对"假 0x370 注入"这类隐患从代码层面不再可能。

**Architecture:** 三段式提交，每段都编译通过 + 测试全绿。① 还原 `epasNagActive` 耦合（恢复新引擎加入前的老 bit-19 baseline）+ 删除所有 epas_nag 测试/env；② 用一条"反向守卫测试"驱动（先红后绿）删除引擎本体 + dashboard 胶水 + 前端 UI；③ 生成安全事故禁令文档。最后构建/烧录/smoke 验证危险符号彻底消失。

**Tech Stack:** C++17 / ESP-IDF 6.0.1（PlatformIO `espressif32@7.0.0`）、Unity（native 单测）、pytest（源码契约测试）、Python minify（`scripts/minify_dashboard.py`，依赖 `csscompressor/htmlmin/rjsmin`）、esptool（烧录）。

**Spec:** `docs/superpowers/specs/2026-06-19-remove-epas-nag-design.md`
**Branch:** `fix/remove-dangerous-epas-nag`（已基于 `feature/ui-v11-redesign@2f75109`）
**Repo root for all commands:** `/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware`（Bash cwd 会重置，每个命令都 `cd` 进去或用绝对路径）

---

## 文件结构（改动地图）

| 文件 | 责任 | 动作 |
|---|---|---|
| `include/handlers.h` | CAN handler 基类/子类、老 NagHandler、bit-19 baseline、FSD | 改 3 处（删字段 + 还原 2 处耦合） |
| `include/web/mcp2515_dashboard.h` | HTTP/WS 服务器 + CAN 分发 + dashboard 胶水（7213 行） | 摘除全部 nag 胶水（~15 函数 + 全局 + JSON + 串口 + 路由） |
| `include/web/mcp2515_dashboard_ui.src.h` | dashboard HTML 源（可读） | 删 6 处 nag 块 |
| `include/web/mcp2515_dashboard_ui.h` | minify 产物（PROGMEM） | 由 minify 重新生成，构建后去 stamp |
| `include/dash_epas_nag.h` | 引擎本体 | 🗑️ 删 |
| `include/dash_epas_nag_trace.h` | 诊断录制 ring | 🗑️ 删 |
| `platformio.ini` | 构建配置 | 删 `[env:native_epas_nag]` 块 |
| `test/test_native_epas_nag/` | 引擎单测（46 个） | 🗑️ 删目录 |
| `test/test_epas_nag_*.py`（5 个） | 源码契约测试 | 🗑️ 删 |
| `test/test_no_epas_nag_contract.py` | **新增**反向守卫 | ✨ 建 |
| `docs/EPAS-NAG-REMOVAL-INCIDENT.md` | 安全禁令记录 | ✨ 建 |

**绝不碰**：老 `NagHandler`、`nagKillerRuntime`、HW3/HW4 的 `setBit(frame,19,false)` 清除动作本身、`setBit(frame,47,true)`、`APActive`/`isDASAutopilotActive`、`injectionAllowed`、0x3EE FSD 块、`dashDefenseEnabled`、`canActive`、handler 过滤器 `ids[]`（含 880/923，因此 `test_native_hw4` 的 880/923 断言无需改）、`dash_tx_evidence.h`、`shared_types.h`。

---

## Task 1: 基线确认（建立安全网，不提交）

**Files:** 无改动

- [ ] **Step 1: 确认在正确分支**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && git branch --show-current`
Expected: `fix/remove-dangerous-epas-nag`

- [ ] **Step 2: 跑相关 native 测试，确认基线全绿**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
pio test -e native_nag -e native_hw3 -e native_hw4 -e native_legacy -e native_dashboard -e native_helpers -e native_injection_after_ap
```
Expected: 每个 env `N Tests 0 Failures 0 Ignored`（注：`native_hw4::test_hw4_checkAD_blocks...` 与 `native_wheel_dnd::test_volume_sequence...` 是**预存失败**、与 EPAS 无关，可忽略；其余全绿）

- [ ] **Step 3: 跑现有 epas_nag 契约测试，确认当前"还在"（红/绿都行，只是基线快照）**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
python3 -m pytest test/test_epas_nag_config_contract.py test/test_epas_nag_filters_contract.py test/test_epas_nag_trace_contract.py test/test_epas_nag_trace_persistence_contract.py test/test_epas_nag_wiring_contract.py -q
```
Expected: `5 passed`（这些测试断言 nag 符号**存在**——下架后会被删除）

> 不提交。此任务只是确认"动手前一切如预期"，作为后续每步比对的基线。

---

## Task 2 (Commit A): 还原 epasNagActive 耦合 + 删除所有 epas_nag 测试/env

> 为什么这两个动作绑一个 commit：还原耦合会让 `test_epas_nag_wiring_contract.py`（断言 `!(bool)epasNagActive` 出现 ≥2 次）立刻失败，所以必须同步删掉它；其余 4 个契约测试 + native env + 单测目录一并删除（它们都在下架范围，早删不破坏编译）。

**Files:**
- Modify: `include/handlers.h:73,605,956-957`
- Modify: `include/web/mcp2515_dashboard.h:1542`
- Delete: `test/test_native_epas_nag/`（目录）
- Delete: `test/test_epas_nag_config_contract.py`、`test/test_epas_nag_filters_contract.py`、`test/test_epas_nag_trace_contract.py`、`test/test_epas_nag_trace_persistence_contract.py`、`test/test_epas_nag_wiring_contract.py`
- Modify: `platformio.ini`（删 `[env:native_epas_nag]` 块）

- [ ] **Step 1: 还原 HW3 mux-1 耦合（去掉 `&& !(bool)epasNagActive`）**

Edit `include/handlers.h`，把：
```cpp
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed() && !(bool)epasNagActive)
```
改为：
```cpp
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
```
（同时把上一行注释 `// ── Mux 1: Nag suppression (skipped when EPAS-faithful nag active — D7) ──` 改为 `// ── Mux 1: Nag suppression (bit-19 clear, legacy baseline) ──`）

- [ ] **Step 2: 还原 HW4 mux-1 耦合（拆掉 `if(!epasNagActive)` 包裹）**

Edit `include/handlers.h`，把 L950-957 这段：
```cpp
            // Mux 1: FSD-ready signal (bit 47) + nag suppression (bit 19).
            // bit-47 FSD-ready is independent of the nag engine; only the bit-19
            // nag clear is skipped when EPAS-faithful nag is active (D7).
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
            {
                setBit(frame, 47, true);
                if (!(bool)epasNagActive)                 // D7: bit-19 nag skipped when faithful active
                    setBit(frame, 19, false);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(1, true);
            }
```
改为：
```cpp
            // Mux 1: FSD-ready signal (bit 47) + nag suppression (bit 19).
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
            {
                setBit(frame, 47, true);
                setBit(frame, 19, false);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(1, true);
            }
```

- [ ] **Step 3: 删除 CarManagerBase 的 epasNagActive 字段**

Edit `include/handlers.h`，删除这一整行：
```cpp
    Shared<bool> epasNagActive{false};  // EPAS-faithful nag engine active (D7): when true, skip bit19 nag
```

- [ ] **Step 4: 删除 dashboard 里对该字段的赋值行**

Edit `include/web/mcp2515_dashboard.h`，删除这一整行：
```cpp
        dashHandler->epasNagActive = dashEpasNagEnabled && canActive && dashDefenseEnabled;
```

- [ ] **Step 5: 删除全部 epas_nag 测试 + 单测目录**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
git rm -r test/test_native_epas_nag && \
git rm test/test_epas_nag_config_contract.py test/test_epas_nag_filters_contract.py test/test_epas_nag_trace_contract.py test/test_epas_nag_trace_persistence_contract.py test/test_epas_nag_wiring_contract.py
```
Expected: 6 个文件/目录被删除

- [ ] **Step 6: 删除 platformio.ini 的 `[env:native_epas_nag]` 块**

Edit `platformio.ini`，删除整个块（约 L353-357，从 `[env:native_epas_nag]` 到下一个 `[env:...]` 或文件尾之间的全部内容，含 `platform=`/`extra_scripts`/`build_flags`/`test_filter = test_native_epas_nag` 等）。删除后确认文件里不再有 `native_epas_nag` 字样：
Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && grep -n native_epas_nag platformio.ini`
Expected: 无输出

- [ ] **Step 7: 确认没有遗留对 epasNagActive 的引用**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && grep -rn "epasNagActive" include/ src/`
Expected: 无输出（字段、2 处读、1 处写都已清除）

- [ ] **Step 8: 跑 native 测试，确认还原耦合没误伤老路径**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
pio test -e native_nag -e native_hw3 -e native_hw4
```
Expected: 全绿（`native_hw4::test_hw4_nag_suppression_clears_bit19_sets_bit47` 仍通过——bit-19 清除与 bit-47 置位现在无条件执行；老 `NagHandler` 测试不受影响）

- [ ] **Step 9: 提交 Commit A**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
git add include/handlers.h include/web/mcp2515_dashboard.h platformio.ini && \
git commit -m "fix(epas-nag): revert epasNagActive coupling to unconditional bit-19 baseline

The new DashEpasNagEngine gated the legacy bit-19 clear behind !epasNagActive
(defer to the engine when it was active). Removing the engine reverts both
HW3/HW4 mux-1 blocks to unconditional bit-19 clear — the exact pre-engine
behavior. Legacy NagHandler / FSD-ready / bit-47 untouched.

Also drops the epas_nag test suite (46 native tests + 5 contract tests) and
the [env:native_epas_nag] build env — they asserted symbols about to be
removed in the next commit. Engine code itself is removed next."
```
Expected: commit 成功，工作区只剩后续要改的 `mcp2515_dashboard.h` 等

---

## Task 3 (Commit B): 反向守卫（红）→ 删引擎 + dashboard 胶水 + UI（绿）

> TDD 适配下架场景：先写一条"断言 nag 符号不存在"的守卫测试 → 此时仍红（符号还在）→ 删除引擎本体 + dashboard 全部胶水 + 前端 UI → 守卫转绿。引擎文件与 dashboard 的引用必须同 commit 删除（否则编译断）。

**Files:**
- Create: `test/test_no_epas_nag_contract.py`
- Delete: `include/dash_epas_nag.h`、`include/dash_epas_nag_trace.h`
- Modify: `include/web/mcp2515_dashboard.h`（摘除全部 nag 胶水）
- Modify: `include/web/mcp2515_dashboard_ui.src.h`（删 6 块）→ 重新 minify 生成 `.h`

### 3a. 写反向守卫测试（红）

- [ ] **Step 1: 创建守卫测试**

Create `test/test_no_epas_nag_contract.py`：
```python
#!/usr/bin/env python3
"""Guard rail: EPAS-faithful nag injection must stay REMOVED.

DashEpasNagEngine injected spoofed 0x370 (EPAS own-status) frames and caused a
real-vehicle body-control fault (2026-06-19). It was physically removed. This
test prevents re-introducing that hazard class by asserting the forbidden
symbols are absent from compiled source.

Run: python3 -m pytest test/test_no_epas_nag_contract.py -q
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Symbols that exist ONLY for the removed DashEpasNagEngine path.
# Legacy NagHandler / nagKillerRuntime / bit-19 baseline do NOT match these.
FORBIDDEN = [
    "DashEpasNag",       # engine struct + diag
    "dashEpasNag",       # global instance / wiring
    "epasNagDiag",       # /status JSON object
    "tryBuildEcho",      # engine echo builder
    "nextDemandTorque",  # engine demand-state machine
    "EpasNagTraceRing",  # trace recorder
    "dash_epas_nag",     # include filename
    "def_epnag",         # NVS key
]

# Scanned COMPILED source only (never test/ — this file itself contains the words).
SCAN_DIRS = [ROOT / "include", ROOT / "src"]
SCAN_FILES = [ROOT / "platformio.ini"]
SRC_EXTS = (".h", ".hpp", ".c", ".cpp", ".ino")


def _source_text():
    chunks = []
    for d in SCAN_DIRS:
        for p in d.rglob("*"):
            if p.is_file() and p.suffix in SRC_EXTS:
                chunks.append(p.read_text(errors="ignore"))
    for f in SCAN_FILES:
        if f.exists():
            chunks.append(f.read_text(errors="ignore"))
    return "\n".join(chunks)


def test_no_epas_nag_symbols_in_compiled_source():
    src = _source_text()
    hits = {sym: src.count(sym) for sym in FORBIDDEN if sym in src}
    assert not hits, (
        "EPAS-faithful nag symbols must stay REMOVED (2026-06-19 safety incident). "
        f"Found: {hits}. See docs/EPAS-NAG-REMOVAL-INCIDENT.md — do NOT re-add "
        "0x370 echo injection; it faults the power-steering ECU."
    )


def test_no_native_epas_nag_env():
    ini = (ROOT / "platformio.ini").read_text()
    assert "[env:native_epas_nag]" not in ini, "native_epas_nag env must stay removed"
```

- [ ] **Step 2: 跑守卫测试，确认当前为红（符号还在）**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python3 -m pytest test/test_no_epas_nag_contract.py -q`
Expected: **FAIL**，报 `Found: {'DashEpasNag': N, 'dashEpasNag': N, ...}`（这是预期的红——它正是我们要消灭的状态）

### 3b. 删引擎本体

- [ ] **Step 3: 删两个引擎头文件**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
git rm include/dash_epas_nag.h include/dash_epas_nag_trace.h
```
Expected: 2 个文件删除

### 3c. 摘除 mcp2515_dashboard.h 全部 nag 胶水

> ⚠️ **关键技巧：从行号大到小、自下而上删除**，这样早删的不影响晚删的行号。下面按"从下往上"顺序列出。每删一块后不必单独编译，全部删完再编译一次。每块用其函数签名/唯一锚点定位（行号是导航参考，实际以锚点文本为准）。

- [ ] **Step 4: 删 loop 里的 `dashEpasNagTracePoll();` 调用（~L7188）**

Edit `include/web/mcp2515_dashboard.h`：定位主 loop 中单独一行的 `    dashEpasNagTracePoll();`，删除该行。

- [ ] **Step 5: 删 `/epas_nag_trace` 路由注册（~L7118-7120）**

删除形如 `    server.on("/epas_nag_trace", HTTP_GET, handleEpasNagTraceDownload);` 等 3 行路由注册（`/epas_nag_trace` 的 GET/start/stop）。

- [ ] **Step 6: 删串口命令分派 + help（~L5632-5635 分派，~L5488-5489 help）**

删除 `else if (strcmp(start, "epas_nag") == 0) ...` 与 `else if (strcmp(start, "epas_nag_trace") == 0) ...` 两个分派分支；删除 help 文本里 `epas_nag` / `epas_nag_trace` 两行。

- [ ] **Step 7: 删两个串口打印函数（~L5414-5482）**

删除整个 `dashSerialPrintEpasNagStatus()` 与 `dashSerialPrintEpasNagTrace()` 函数定义。

- [ ] **Step 8: 删 3 个 trace HTTP handler（~L3998-4026）**

删除整个 `handleEpasNagTraceDownload()`、`handleEpasNagTraceStart()`、`handleEpasNagTraceStop()` 函数。

- [ ] **Step 9: 删防御 POST 解析里的 epas_nag（~L3332 操作数 + ~L3374-3375 handler）**

删除 `|| server.hasArg("epas_nag")` 这一个 `||` 操作数；删除 `if (server.hasArg("epas_nag")) dashEpasNagEnabled = dashArgTruthy(...);` 整个 if 块。

- [ ] **Step 10: 删防御配置 JSON 的 `"epas_nag"`（~L3317-3318）**

删除 `j += ",\"epas_nag\":";` 及其紧随的值拼接行。

- [ ] **Step 11: 删 `/status` 的 epasNag + epasNagDiag JSON（~L2716-2755）**

删除 `j += ",\"epasNag\":";` 行 + 整个 `epasNagDiag` 对象构造块（`j += ",\"epasNagDiag\":{...}"` 到其闭合）。

- [ ] **Step 12: 删 NVS load 里的 def_epnag（~L1802）**

删除 `dashEpasNagEnabled = prefs.getBool("def_epnag", false);` 行。

- [ ] **Step 13: 删 NVS save 里的 def_epnag（~L1599）**

删除 `prefs.putBool("def_epnag", dashEpasNagEnabled);` 行。

- [ ] **Step 14: 删 `dashApplyRuntimeState` 里的运行态赋值已被 Task2 Step4 删除——此处仅确认无残留**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && grep -n "dashEpasNagEnabled\|dashEpasNagCtrl" include/web/mcp2515_dashboard.h`
Expected: 仅剩全局声明 + 引擎接线（下一步删）；赋值行应已无

- [ ] **Step 15: 删 `dashRecordApRestoreFrame` 0x129 块里的 onSteeringAngle 喂入（~L692）**

删除单独一行 `        dashEpasNagCtrl.onSteeringAngle(...);`（保留 0x129 解析宿主 `steerAngleX10` 等不变）。

- [ ] **Step 16: 删 `mcpDashOnFrame` 里的 `dashEpasNagOnFrame(f);` 调用（~L1053）**

删除单独一行 `    dashEpasNagOnFrame(f);`。

- [ ] **Step 17: 删 trace poll/flush/stop/sample 函数（~L1016-1033、969-978、920-967）**

删除整个 `dashEpasNagTracePoll()`、`dashEpasNagTraceFlush()`、`dashEpasNagTraceStop()`、`dashEpasNagTraceSample()` 函数。

- [ ] **Step 18: 删 `epasNagGateFromName()` + `dashEpasNagSnapshotTec()`（~L907-918、893-905）**

删除这两个函数。

- [ ] **Step 19: 删 `dashEpasNagOnFrame()` 整个函数（~L820-891）**

删除整个 `dashEpasNagOnFrame()` 函数（每帧回调，含 noteRuntime/noteApState/onDasGate/tryBuildEcho/send echo/traceSample）。

- [ ] **Step 20: 删 trace 全局变量 + fwd-decl（~L202-222）**

删除 `dashEpasNagTrace`、`dashEpasNagLastEntry`、`dashEpasNagLastSampleMs`、`dashEpasNagConsecFail`、`dashEpasNagLastTxFail`、`dashEpasNagLastSwTxErr`、`dashEpasNagLastApActive`、`dashEpasNagLastGateStr`、`dashEpasNagTracePendingFlush`、`dashEpasNagLastFlushMs`、`dashEpasNagTraceManual` 及 `dashEpasNagTraceSample/Flush/Stop/Poll` 的前向声明。

- [ ] **Step 21: 删引擎实例 + 总开关全局（~L199-200）**

删除 `static bool dashEpasNagEnabled` 与 `static DashEpasNagEngine dashEpasNagCtrl` 两行。

- [ ] **Step 22: 删两个 include（文件顶部 ~L49-50）**

删除 `#include "dash_epas_nag.h"` 与 `#include "dash_epas_nag_trace.h"`。**保留** `#include "dash_tx_evidence.h"`（轮 DND 共用）。

- [ ] **Step 23: 编译验证（native）**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
pio test -e native_dashboard -e native_hw3 -e native_hw4 -e native_legacy -e native_helpers -e native_injection_after_ap
```
Expected: 全绿（编译通过证明无残留引用）。若报 `not declared` 之类，按报错定位漏删的引用补删。

- [ ] **Step 24: grep 确认 dashboard 里 nag 胶水已净**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && grep -in "epasNag\|dashEpasNag\|epas_nag" include/web/mcp2515_dashboard.h`
Expected: 无输出

### 3d. 删前端 UI（.src.h）+ 重新 minify

- [ ] **Step 25: 删 `.src.h` 的 6 处 nag 块（按定位关键词逐处删）**

Edit `include/web/mcp2515_dashboard_ui.src.h`，删除以下 6 处（用关键词定位，删除其所在的完整 HTML 元素 / JS 语句）：
1. **开关行**：搜 `def-epnag-tgl`（标签含"EPAS 方向盘 nag"），删其整个 `<div class=setting-row>...</div>`
2. **状态三联行**：搜 `def-epas-nag-row`，删其整个 `<div class=status-triplet ...>...</div>`（含 `def-epas-echo/tx/tq`）
3. **JS 渲染块**：搜 `var ng=d.epasNagDiag` 或 `d.epasNagDiag`，删该渲染语句块（含 `setText('def-epas-...')` 调用）
4. **JS 开关回显**：搜 `def-epnag-tgl` 在 JS 里的 `epnag.checked=!!d.epas_nag`，删该语句（及关联的 `var epnag=$('def-epnag-tgl')` 若仅为此用）
5. **JS 防御摘要**：搜 `d.epas_nag`，删防御激活判断里的 `||d.epas_nag` 操作数
6. **JS POST 字段**：搜 `epas_nag:` 在 POST body 拼接处，删该字段（如 `epas_nag:epnag&&epnag.checked?'1':'0'`）；以及防御摘要数组里的 `'EPAS nag'` 项（若有）

- [ ] **Step 26: 确认 minify 依赖在**

Run: `python3 -c "import csscompressor, htmlmin, rjsmin; print('ok')"`
Expected: `ok`。若报 `ModuleNotFoundError`，安装：`pip3 install csscompressor htmlmin rjsmin`

- [ ] **Step 27: 重新 minify 生成 `.h`**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python3 scripts/minify_dashboard.py`
Expected: 无报错，`include/web/mcp2515_dashboard_ui.h` 被重写

- [ ] **Step 28: 确认 `.h` 里 nag 已净**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && grep -ic "epas_nag\|epasNag\|def-epnag\|def-epas-nag" include/web/mcp2515_dashboard_ui.h`
Expected: `0`

- [ ] **Step 29: 跑反向守卫测试——应转绿**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python3 -m pytest test/test_no_epas_nag_contract.py -q`
Expected: **PASS**（`2 passed`）——这是 TDD 的"绿"时刻，证明危险符号已从编译源消失

- [ ] **Step 30: 提交 Commit B**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
git add -A include/ test/test_no_epas_nag_contract.py && \
git commit -m "fix(epas-nag): physically remove DashEpasNagEngine injection (safety)

Real-vehicle incident 2026-06-19: the engine injected spoofed 0x370 (EPAS
own-status) frames with synthesized torque + derived handsOnLevel, which
contradicted real driver input and faulted the power-steering / body-control
ECU (disabled body control, severe fault, only resolved by unplugging module +
car reboot). Root cause is inherent to the approach — echoing EPAS's own status
with falsified content — not a tuning bug.

Removes: dash_epas_nag.h, dash_epas_nag_trace.h, all dashboard glue (~15
functions + globals + /status.epasNagDiag + serial cmds + /epas_nag_trace
routes + NVS def_epnag + POST parse), and 6 dashboard-UI nag blocks (regenerated
via minify_dashboard.py).

Adds test/test_no_epas_nag_contract.py as a permanent guard rail asserting the
forbidden symbols stay absent from compiled source. Legacy NagHandler / bit-19
baseline / FSD / defense untouched and still green."
```
Expected: commit 成功

---

## Task 4: 构建固件 + 去 stamp

**Files:** `.pio/build/waveshare_single_can_standalone/*`（产物，不提交）

- [ ] **Step 1: 清版本串缓存 + 构建**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
rm -rf .pio/build/waveshare_single_can_standalone && \
pio run -e waveshare_single_can_standalone
```
Expected: `[SUCCESS] Took NN seconds` + `RAM: ~40% Flash: <29.4%`（Flash 应比之前下降，因为删了一大块代码 + UI）

- [ ] **Step 2: 去 build-id stamp（必做，否则 dirty）**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && git restore include/web/mcp2515_dashboard_ui.h`
Expected: 无输出；`git status` 里 `mcp2515_dashboard_ui.h` 不再显示为 modified（除非有别的改动）

- [ ] **Step 3: 确认工作区干净（除预期）**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && git status --short`
Expected: 仅显示历史遗留的 untracked（handoffs/firmware-assets 等），**无** tracked 文件 dirty

---

## Task 5: 危险消失铁证 + 全 native 测试扫尾

**Files:** 无改动

- [ ] **Step 1: grep 全仓库编译源——零活代码命中**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
grep -rin "epasNag\|DashEpasNag\|epas_nag\|epasNagDiag\|nextDemandTorque\|tryBuildEcho\|EpasNagTraceRing\|def_epnag" include/ src/ platformio.ini
```
Expected: **无输出**（危险代码已从所有编译路径消失）

- [ ] **Step 2: 全 native 测试一遍**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
pio test -e native_nag -e native_hw3 -e native_hw4 -e native_legacy -e native_dashboard -e native_helpers -e native_injection_after_ap && \
python3 -m pytest test/test_no_epas_nag_contract.py -q
```
Expected: native 全绿（预存的 hw4/wheel_dnd 2 个失败可忽略）；守卫测试 `1 passed` 之类全绿

- [ ] **Step 3: 验收清单逐项确认**

确认：
- [ ] grep 零命中（Step 1）
- [ ] 老 NagHandler + FSD/防御单测全绿（Step 2）
- [ ] 构建 SUCCESS（Task 4 Step 1）
- [ ] `.pio/build/.../firmware.bin` 存在

---

## Task 6: 生成资产 + 烧录 + smoke（需 Jordan 授权/操作板子）

> ⚠️ 这是写物理硬件。烧录前 `ls /dev/cu.usbmodem*` 重新确认端口。**只用 esptool，绝不用 `pio run -t upload`（会砖）。**

**Files:** `firmware-assets/waveshare-single-can-standalone_<date>-nag-removed/*`（bin 不提交）

- [ ] **Step 1: 确认板子 + 端口**

Run: `ls /dev/cu.usbmodem*`
Expected: 一个设备，如 `/dev/cu.usbmodem21101`

- [ ] **Step 2: flash_id 确认是目标板**

Run: `PORT=/dev/cu.usbmodem21101 && esptool.py --chip esp32s3 --port $PORT --baud 921600 flash_id`
Expected: `MAC: 28:84:85:49:93:fc`、`Detected flash size: 16MB`

- [ ] **Step 3: 生成 full_merged 资产**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
mkdir -p firmware-assets/waveshare-single-can-standalone_20260619-nag-removed && \
esptool.py --chip esp32s3 merge_bin \
  --output firmware-assets/waveshare-single-can-standalone_20260619-nag-removed/waveshare-single-can-standalone_full_merged_20260619-nag-removed.bin \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 .pio/build/waveshare_single_can_standalone/bootloader.bin \
  0x8000 .pio/build/waveshare_single_can_standalone/partitions.bin \
  0x19000 .pio/build/waveshare_single_can_standalone/ota_data_initial.bin \
  0x20000 .pio/build/waveshare_single_can_standalone/firmware.bin
```
Expected: 无报错；首字节为 `0xe9`（`xxd ... | head -1`）

- [ ] **Step 4: 擦除 + 烧录 full_merged @0x0**

Run:
```bash
PORT=/dev/cu.usbmodem21101 && \
esptool.py --chip esp32s3 --port $PORT --baud 921600 erase_flash && \
esptool.py --chip esp32s3 --port $PORT --baud 921600 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 firmware-assets/waveshare-single-can-standalone_20260619-nag-removed/waveshare-single-can-standalone_full_merged_20260619-nag-removed.bin
```
Expected: `Hash of data verified`

- [ ] **Step 5: smoke 抓启动日志（pyserial 显式 DTR/RTS 复位）**

用 pyserial 打开串口会触发 `rst:0x15`，须用显式 DTR/RTS 序列抓启动日志（参考既有 `/tmp/smoke_*.py` 模式）。关键断言：
- 出现 `ev-open-can-tools ready`
- **无 panic / guru meditation**
- **无 `epas_nag enabled=...` 日志行**（功能已消失的铁证）
- `App version:` 版本串正常

> 若不便自动抓，至少 `screen $PORT 115200` 手动复位一次，肉眼确认上述 4 点。

- [ ] **Step 6: 资产 BUILD_INFO 记录（可选，bin 不 commit）**

在资产目录写一个 `BUILD_INFO.md` 记录：分支 `fix/remove-dangerous-epas-nag`、HEAD、构建时间、SHA256、smoke 结论。bin 文件本身**不 commit**（仓库约定）。

---

## Task 7 (Commit C): 安全事故禁令文档

**Files:** Create `docs/EPAS-NAG-REMOVAL-INCIDENT.md`

- [ ] **Step 1: 创建禁令文档**

Create `docs/EPAS-NAG-REMOVAL-INCIDENT.md`：
````markdown
# ⛔ EPAS-Faithful Nag 注入 — 下架事故记录（禁止复辟）

**日期**: 2026-06-19
**状态**: 已物理移除（commit 见 git log `fix(epas-nag): physically remove DashEpasNagEngine`）
**下架依据**: `docs/superpowers/specs/2026-06-19-remove-epas-nag-design.md`

## 事故

第 5 轮 apActive 修复（`2f75109`）打通 `DashEpasNagEngine` 的 0x370 注入路径后，实车启用即出现：**车身控制系统禁用、车辆异常、严重故障提示**，持续到拔掉模块、重启车机才恢复。诊断页实测 `echo>0`、`demand.gate` 在 s1_hold/s2_mild/strong_* 切换——故障时模块正在主动注入篡改的 0x370 帧。

## 根因

`0x370` 是 **EPAS（电子助力转向）控制器自身的状态报文**。引擎回声 0x370（counter+1 + 合成扭矩 + 从扭矩派生的 handsOnLevel）= 冒充助力转向 ECU 广播假状态。EPAS 交叉校验发现 counter 序列冲突 + 扭矩语义矛盾 → 助力转向/车身控制进入故障保护。这是**原理性**隐患，不是调参问题：任何向 0x370 注入篡改帧都可能让转向故障，属 life-critical。

## ⛔ 禁令

**禁止任何形式的"回声/伪造 0x370（EPAS 状态报文）注入"**——固定扭矩、状态机扭矩、或 counter+1 之外的任何篡改都不行。永久守卫测试 `test/test_no_epas_nag_contract.py` 会阻止这类符号重新进入编译源。

## 保留项（为何不一起删）

- **老 `NagHandler` + bit-19 baseline**：counter+1 轻度回声，实测未出事，Jordan 2026-06-19 授权保留。它与本事故的"篡改扭矩/伪造 handsOn"引擎是不同代码路径。
- **FSD unlock / AP Gate / 防御系统 / 轮 DND**：与 0x370 注入无关，全部保留。

## 教训

1. 向安全 ECU 的**自身状态报文**注入篡改帧，等于和该 ECU 对着干，随时可能让它故障。
2. 诊断 `echo>0` 是"注入正在发生"的证据，不是"功能正常"的证据——本次正是 echo 打通的瞬间出事。
3. 涉及行车安全的功能，默认 OFF 不够，必须**物理删除**才能杜绝误启用。
````

- [ ] **Step 2: 提交 Commit C**

Run:
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && \
git add docs/EPAS-NAG-REMOVAL-INCIDENT.md && \
git commit -m "docs: record EPAS-faithful nag removal incident + reimplement ban"
```
Expected: commit 成功

- [ ] **Step 3: 在两份旧 spec 顶部加"已下架"标注（不删，留历史）**

分别在 `docs/superpowers/specs/2026-06-17-epas-faithful-nag-unified-design.md` 与 `docs/superpowers/specs/2026-06-19-upstream-epas-nag-modec-align-design.md` 顶部标题下加一行：
```markdown
> ⛔ **已下架（2026-06-19）**：本 spec 描述的引擎因实车安全事故被物理移除。见 `docs/EPAS-NAG-REMOVAL-INCIDENT.md` 与 `docs/superpowers/specs/2026-06-19-remove-epas-nag-design.md`。请勿据此复辟。
```
Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && git add docs/superpowers/specs/2026-06-17-epas-faithful-nag-unified-design.md docs/superpowers/specs/2026-06-19-upstream-epas-nag-modec-align-design.md && git commit --amend --no-edit`
Expected: 并入 Commit C

---

## 完成标准（全部满足）

- [ ] `grep` 编译源零 nag 命中（Task 5 Step 1）
- [ ] 守卫测试 `test_no_epas_nag_contract.py` 常绿
- [ ] 老 NagHandler + FSD/防御 native 全绿
- [ ] 构建 SUCCESS + Flash 下降
- [ ] full_merged 烧录 Hash verified + smoke 无 panic + **无 `epas_nag` 日志**
- [ ] Dashboard 防御页不再有"EPAS nag"开关
- [ ] 3 个 commit（A/B/C）各自编译通过、测试全绿
- [ ] 事故禁令文档落库

## 回滚点

- 每个任务结束跑一次 native 测试，红即停下定位，不累积
- Commit A/B/C 各自可 `git revert`（A 还原耦合、B 还原引擎、C 删文档）
- 整支可 `git switch feature/ui-v11-redesign` 回到下架前状态（2f75109）
