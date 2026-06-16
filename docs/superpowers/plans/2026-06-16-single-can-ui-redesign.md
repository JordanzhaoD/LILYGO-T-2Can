# 单 CAN ATLAS 固件 UI V1.1 重新设计 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复单 CAN standalone UI 的 4 个缺陷（手机导航 / CAN2 块 / 插件&AP注入风格 / 首页底部冗余块），并把全部板块统一到驾驶舱玻璃拟态风格。

**Architecture:** 单文件 UI 改造——`include/web/mcp2515_dashboard_ui.src.h`（3779 行，CSS + HTML + JS 同文件，经 gzip 存储进固件）。策略=设计系统优先：先抽取/统一一套驾驶舱 token + 组件基底（Task 1），再逐板块套用。现有两套设计并存——`.card`（灰 `--card-bg #374151` 工具风）与 `.cockpit-*`（深色驾驶舱）；统一方向是把工具风板块拉齐到驾驶舱。

**Tech Stack:** ESP-IDF C++（PlatformIO 构建）、嵌入式 HTML/CSS/JS 原始字符串、Python unittest 契约测试（grep UI 源字符串）、esptool 烧录。

**基线:** `main` HEAD `f3732c2`（tag `v1.0-atlas-single-can`）。**安全红线:** AP 注入 fail-closed / 2s gate / AP 断开清零是服务端 `handlers.h` 强制，任何 UI 改动不得触碰。

**Spec:** `docs/superpowers/specs/2026-06-16-single-can-ui-redesign-design.md`

---

## File Structure

唯一修改文件：

- **Modify:** `include/web/mcp2515_dashboard_ui.src.h` — UI 源（CSS `:root` 在 16-50 行；`.card` 108；cockpit 组件 356-399；驾驶中心 559-770；硬件页 800-871；各 page 见下；移动 nav JS 3646-3720；状态更新 JS 2075-2096 / 2695-2716）

契约测试：

- **Modify/Create:** `test/test_ui_v11_redesign.py` — 新增契约测试，读 `src.h`（源）断言结构字符串

各 page 行号（standalone 可见 8 个）：
- pg-overview 559（驾驶状态中心，基准，不改结构）
- pg-hardware 800（含插件管理 856-871）
- pg-drive 875（驾驶风格）
- pg-speed 929（速度策略）
- pg-defense 1215（FSD 防护）
- pg-ota 1304（OTA 升级）
- pg-network 1371（网络设置）
- pg-can 1532（CAN 诊断）
- 隐藏（data-single-hide）：pg-bus2 1018 / pg-strobe 1137 / pg-shift 1657

---

## 契约测试基础设施

所有任务的契约测试遵循同一模式。新建 `test/test_ui_v11_redesign.py`，读 **`src.h`**（源文件，总有真内容；不依赖 stamped 的 `ui.h`）：

```python
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
```

运行命令：`PYTHONPATH=test python3 -m unittest test_ui_v11_redesign -v`

> 注：仓库 `test/` 无 `__init__.py`，必须用 `PYTHONPATH=test`。契约测试 grep `src.h`（明文源），不碰 `firmware.bin`（UI 是 gzip 存储，明文搜不到）。

---

## Task 1: 设计系统基底（CSS token + 组件统一）

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:16-50`（`:root`）、`:108`（`.card`）
- Test: `test/test_ui_v11_redesign.py`（新增 `DesignSystemTests` 类）

- [ ] **Step 1: 写失败契约测试**

```python
class DesignSystemTests(UiV11RedesignTests):
    def test_ds_tokens_defined(self):
        for tok in ["--ds-cyan", "--ds-indigo", "--ds-accent-grad"]:
            self.assert_present(tok + ":")

    def test_card_is_cockpit_glass(self):
        # .card 重定义为驾驶舱玻璃风：深色 + backdrop-filter
        self.assertRegex(SRC, r"\.card\s*\{[^}]*backdrop-filter")
        self.assert_present("--ds-accent-grad")
```

- [ ] **Step 2: 运行测试，确认失败**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.DesignSystemTests -v`
Expected: FAIL（`--ds-cyan` 未定义）

- [ ] **Step 3: 在 `:root`（16-50 行 `--cockpit-card-blur` 之后）新增设计系统 token**

```css
  --cockpit-card-blur: blur(22px) saturate(1.16);
  /* === 设计系统基底（V1.1，全板块统一） === */
  --ds-cyan: #0ea5e9;
  --ds-indigo: #6366f1;
  --ds-accent-grad: linear-gradient(135deg, #0ea5e9, #6366f1);
  --ds-ok: #34d399;
  --ds-warn: #f59e0b;
  --ds-err: #f87171;
  --ds-card-bg: rgba(15,23,42,.58);
  --ds-card-stroke: rgba(148,163,184,.20);
  --ds-radius: 18px;
```

- [ ] **Step 4: 重定义 `.card`（108 行）为驾驶舱玻璃风**

把 `108` 行的 `.card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 14px; padding: 22px; ...}` 替换为：

```css
.card { background: var(--ds-card-bg); border: 1px solid var(--ds-card-stroke); border-radius: var(--ds-radius); padding: 20px; backdrop-filter: var(--cockpit-card-blur); box-shadow: 0 8px 28px rgba(0,0,0,.22); margin-bottom: 14px; }
```

> 说明：`.card` 现用灰 `--card-bg`，改为深色玻璃（`--ds-card-bg`）。所有用 `.card` 的板块（设备信息、核心开关、功耗等）自动升级为驾驶舱风。layout（padding/margin）保持接近，仅换皮。

- [ ] **Step 5: 运行测试，确认通过**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.DesignSystemTests -v`
Expected: PASS

- [ ] **Step 6: 双构建回归（确保 CSS 改动不破坏编译）**

Run: `pio run -e waveshare_single_can_standalone 2>&1 | tail -3`
Expected: `SUCCESS`

- [ ] **Step 7: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h test/test_ui_v11_redesign.py
git commit -m "feat(ui): 设计系统基底 — ds token + .card 驾驶舱玻璃化"
```

---

## Task 2: 问题 4 — 删除首页底部 3 块

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:739-757`（删除 3 个 `.card`）
- Test: `test/test_ui_v11_redesign.py`

- [ ] **Step 1: 写失败契约测试**

```python
class RemoveBottomBlocksTests(UiV11RedesignTests):
    def test_removed_titles_absent(self):
        for t in ["维修模式 0x339", "方向盘免打扰 0x3C2", "Legacy 功能支持矩阵"]:
            self.assert_absent(t)

    def test_retained_neighbors_present(self):
        # 相邻保留块
        self.assert_present("Legacy CAN-A 诊断")
        self.assert_present("AP 注入安全")
```

- [ ] **Step 2: 运行，确认失败**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.RemoveBottomBlocksTests -v`
Expected: FAIL（标题仍存在）

- [ ] **Step 3: 删除 739-757 行的 3 个 `.card`**

删除这 3 块（每块含 `<div class="card">...</div>`）：

```html
        <div class="card">
          <div class="card-title">维修模式 0x339</div>
          <div class="setting-desc" style="margin-bottom:8px">突发发送仅证明模块 TX，车辆响应未知。</div>
        </div>

        <div class="card">
          <div class="card-title">方向盘免打扰 0x3C2</div>
          <div class="setting-desc" style="margin-bottom:8px">等待原生 0x3C2 帧后再发送，避免盲发合成帧。</div>
        </div>

        <div class="card">
          <div class="card-title">Legacy 功能支持矩阵</div>
          <div class="stats">
            <div class="stat"><div class="stat-lbl">仿生转向</div><div class="stat-val" style="color:var(--tx3)">不支持</div></div>
            <div class="stat"><div class="stat-lbl">Ban Shield</div><div class="stat-val" style="color:var(--tx3)">不支持</div></div>
            <div class="stat"><div class="stat-lbl">MPP 自定义</div><div class="stat-val" style="color:var(--tx3)">未接线</div></div>
            <div class="stat"><div class="stat-lbl">换挡辅助</div><div class="stat-val" style="color:var(--tx3)">未实现</div></div>
          </div>
        </div>
```

> 这 3 块无 `id`、无 JS 引用、无后端 API，删除零副作用。删除后 737（Legacy CAN-A 诊断）直接接 759（AP 注入安全）。

- [ ] **Step 4: 运行，确认通过**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.RemoveBottomBlocksTests -v`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h test/test_ui_v11_redesign.py
git commit -m "feat(ui): 删除首页底部冗余3块（维修模式/方向盘免打扰/Legacy矩阵）"
```

---

## Task 3: 问题 2 — 驾驶中心 CAN2 → AP 激活

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:596`（cockpit-metric）、JS `:2082`/`:2708-2709`（s-can2 残留引用）、新增 AP 激活更新逻辑
- Test: `test/test_ui_v11_redesign.py`

- [ ] **Step 1: 写失败契约测试**

```python
class Can2ToApTests(UiV11RedesignTests):
    def test_ap_metric_replaces_can2(self):
        self.assert_id_present("ov-ap")          # 新 AP 激活格
        self.assert_present("AP 激活")
        # 原 CAN2 metric 的 id 不应再作为 CAN2 指标出现
        self.assert_absent('<div class="lbl">CAN2</div><div class="val" id="s-can2">')

    def test_ap_update_js_present(self):
        self.assert_present("setText('ov-ap'")
```

- [ ] **Step 2: 运行，确认失败**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.Can2ToApTests -v`
Expected: FAIL（`ov-ap` 不存在）

- [ ] **Step 3: 替换 596 行的 CAN2 cockpit-metric**

原（596 行）：
```html
              <div class="cockpit-metric"><div class="lbl">CAN2</div><div class="val" id="s-can2">Idle</div><div class="hint">--</div></div>
```
替换为：
```html
              <div class="cockpit-metric" id="ov-ap-metric"><div class="lbl">AP 激活</div><div class="val" id="ov-ap">等待</div><div class="hint" id="ov-ap-hint">门控未触发</div></div>
```

- [ ] **Step 4: 处理 s-can2 残留 JS 引用**

(a) `2082` 行（`setText('ov-can', ...)`）——该行用 `$('s-can2')` 有 `&&` guard，s-can2 删除后返回 null 走 `'CAN2 --'` 分支，**不崩溃**。但单 CAN 不应显示 CAN2，改为只显 CAN1：

```javascript
  setText('ov-can', d.can ? 'CAN1 Online' : 'CAN1 Offline');
```

(b) `2087-2089` 行（`can2Online`/`can2Text`/`m-can2`）——有 guard，且 `m-can2` 是 `data-single-hide`（单 CAN 隐藏）。**保留不动**（安全，无副作用）。

(c) `2708-2709` 行（在 `pollCAN2` 内更新 `s-can2`）——单 CAN 不调 `pollCAN2`，但删除 `s-can2` 后这两行 `$()` 为 null。`setText`/`setCls` 内部须 guard null。**删除这两行**（2710-2711 的 `m-can2` 同理保留，guard）：

删除 2708-2709：
```javascript
  setCls('s-can2','stat-val '+(d.count>0?'v-ok':'v-dim'));
  setText('s-can2',d.count>0?'Online':'Idle');
```

- [ ] **Step 5: 新增 AP 激活更新逻辑**

在状态更新函数（含 `2082` 行 `setText('ov-can',...)` 的同一函数，`setText('ov-hw',hwText)` 2080 附近）插入：

```javascript
  // AP 激活状态（问题2：替换 CAN2）
  var apActive = !!(d.apActive);
  setText('ov-ap', apActive ? '激活' : '等待');
  var apMetric = $('ov-ap-metric');
  if (apMetric) apMetric.style.borderColor = apActive ? 'rgba(52,211,153,.45)' : '';
  var apHint = $('ov-ap-hint');
  if (apHint) apHint.textContent = apActive ? ('Gate ' + (S.apDelay !== undefined ? S.apDelay : 2000) + 'ms · 已就绪') : '门控未触发';
```

> `d.apActive` 来自 `/status`（AP 注入安全块同源字段）；`S.apDelay` 是延迟注入设定（ms）。若 `/status` 字段名不同，执行时按实际 `applyStatus` 解析的字段调整（核查 `applyCapabilities`/状态解析函数里的 AP 字段名）。

- [ ] **Step 6: 运行，确认通过**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.Can2ToApTests -v`
Expected: PASS

- [ ] **Step 7: 双构建回归**

Run: `pio run -e waveshare_single_can_standalone 2>&1 | tail -3` → SUCCESS

- [ ] **Step 8: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h test/test_ui_v11_redesign.py
git commit -m "feat(ui): 驾驶中心 CAN2 指标格 → AP 激活状态"
```

---

## Task 4: 问题 3b — AP 注入安全块重设计

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:759-768`（`ap-core-card`）、关联 CSS、JS Gate 进度
- Test: `test/test_ui_v11_redesign.py`

- [ ] **Step 1: 写失败契约测试**

```python
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
```

- [ ] **Step 2: 运行，确认失败**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.ApInjectionRedesignTests -v`
Expected: FAIL

- [ ] **Step 3: 替换 `ap-core-card`（759-768 行）为驾驶舱结构**

原（759-768）是 `card-h` + `grid c4` + `diag-line`。替换为（保留所有原 `id`：`ap-core-state-pill`/`ap-core-gate-tgl`/`ap-delay-select`/`ap-auto-restore-tgl`/`injection-source`/`ap-core-state-detail`）：

```html
        <div class="card cockpit-card" id="ap-core-card">
          <div class="ck-head"><h3><span>🛡</span>AP 注入安全</h3><span class="ck-chip" id="ap-core-state-pill">等待 AP</span></div>
          <div class="state-panel" id="ap-state-panel">
            <div class="sp-label">AP 注入状态</div>
            <div class="sp-val" id="ap-core-state-big">等待</div>
            <div class="sp-desc" id="ap-core-state-detail">门控未触发</div>
            <div class="gate-bar" id="ap-gate-bar"><i id="ap-gate-fill"></i></div>
          </div>
          <div class="src-row"><span>注入来源</span><b id="injection-source">Disabled</b></div>
          <div class="ctl-section">注入门控（服务端强制）</div>
          <div class="ctl-row"><div><div class="cn">AP 门控</div><div class="cd">要求 Parked / AP / Summon 后才注入</div></div>
            <label class="tgl"><input id="ap-core-gate-tgl" type="checkbox" onchange="saveApGateControls()"><div class="tgl-track"></div></label></div>
          <div class="ctl-row"><div><div class="cn">延迟注入</div><div class="cd">AP 激活后等待再注入（推荐 2000ms）</div></div>
            <label class="field"><select id="ap-delay-select" onchange="saveApGateControls()"><option value="0">0 ms</option><option value="1000">1000 ms</option><option value="2000">2000 ms</option><option value="3000">3000 ms</option></select></label></div>
          <div class="ctl-row"><div><div class="cn">AP 自动恢复</div><div class="cd">重启后恢复上次 AP 配置</div></div>
            <label class="tgl"><input id="ap-auto-restore-tgl" type="checkbox" onchange="saveApGateControls()"><div class="tgl-track"></div></label></div>
          <div class="safety-strip">⚠️ <b>Fail-closed（不变）：</b>未知 / 无效 / SNA 档位默认禁止注入；AP 断开立即清零 Gate 计时。此策略由服务端 C++ 强制（handlers.h），客户端 UI 无法绕过。</div>
        </div>
```

- [ ] **Step 4: 在 CSS（cockpit 组件段，399 行 `.mobile-risk-note` 之后）新增 AP 卡样式**

```css
.cockpit-card { background: radial-gradient(120% 60% at 50% 0%, rgba(56,189,248,.08), transparent 60%), linear-gradient(160deg,#0f172a,#0b1220); border:1px solid var(--ds-card-stroke); }
.ck-head { display:flex; align-items:center; justify-content:space-between; margin-bottom:14px; }
.ck-head h3 { margin:0; font-size:17px; color:var(--tx1); display:flex; align-items:center; gap:9px; }
.ck-chip { font-size:10.5px; padding:4px 11px; border-radius:999px; background:rgba(148,163,184,.15); color:var(--tx2); border:1px solid var(--ds-card-stroke); }
.ck-chip.ok { background:rgba(52,211,153,.14); color:#6ee7b7; border-color:rgba(52,211,153,.3); }
.state-panel { background:rgba(255,255,255,.04); border:1px solid var(--ds-card-stroke); border-radius:13px; padding:15px; text-align:center; margin-bottom:14px; }
.state-panel.active { border-color:rgba(52,211,153,.4); background:rgba(52,211,153,.06); }
.sp-label { font-size:10px; color:var(--cockpit-muted); letter-spacing:.06em; }
.sp-val { font-size:24px; font-weight:700; color:var(--tx3); margin:6px 0; }
.state-panel.active .sp-val { color:#34d399; }
.sp-desc { font-size:10.5px; color:var(--cockpit-dim); }
.gate-bar { height:5px; background:rgba(52,211,153,.15); border-radius:999px; margin-top:9px; overflow:hidden; }
.gate-bar i { display:block; height:100%; width:0; background:linear-gradient(90deg,#34d399,#10b981); border-radius:999px; transition:width .2s; }
.src-row { display:flex; align-items:center; justify-content:space-between; background:rgba(0,0,0,.2); border:1px solid var(--ds-card-stroke); border-radius:10px; padding:10px 14px; margin-bottom:14px; font-size:12px; }
.src-row b { color:var(--tx1); }
.ctl-section { font-size:11px; color:var(--ds-cyan); letter-spacing:.06em; margin:6px 0 10px; }
.ctl-row { display:flex; align-items:center; justify-content:space-between; background:rgba(255,255,255,.03); border:1px solid var(--ds-card-stroke); border-radius:11px; padding:11px 15px; margin-bottom:9px; }
.ctl-row .cn { font-size:13px; color:var(--tx1); }
.ctl-row .cd { font-size:10.5px; color:var(--cockpit-dim); margin-top:2px; }
.safety-strip { margin-top:14px; background:rgba(245,158,11,.08); border:1px solid rgba(245,158,11,.25); border-radius:10px; padding:11px 15px; font-size:11.5px; color:#fcd34d; line-height:1.6; }
```

- [ ] **Step 5: Gate 进度条 JS**

在 AP 状态更新处（搜 `ap-core-state-pill` 的更新逻辑，通常在状态轮询函数）添加 Gate 进度渲染。若 `ap-core-state-pill` 更新逻辑里能取到 AP active settle 计时，则：

```javascript
  var apBig = $('ap-core-state-big');
  var apPanel = document.getElementById('ap-state-panel');
  var apFill = $('ap-gate-fill');
  if (apBig) apBig.textContent = d.apActive ? '激活' : '等待';
  if (apPanel) apPanel.classList.toggle('active', !!d.apActive);
  if (apFill && d.apActive && d.apGateProgress !== undefined) apFill.style.width = (d.apGateProgress * 100) + '%';
```

> 若 `/status` 无 `apGateProgress` 字段，进度条按 `apActive` 二态显示（激活=满，未激活=空），不强制依赖新后端字段（YAGNI）。

- [ ] **Step 6: 运行，确认通过**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.ApInjectionRedesignTests -v` → PASS

- [ ] **Step 7: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h test/test_ui_v11_redesign.py
git commit -m "feat(ui): AP 注入安全块重设计为驾驶舱风格（状态提升+Gate进度条，安全线不变）"
```

---

## Task 5: 问题 3a — 插件管理重设计

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:856-871`（`plugins-card`）、移动端 `:3741-3746`
- Test: `test/test_ui_v11_redesign.py`

- [ ] **Step 1: 写失败契约测试**

```python
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
```

- [ ] **Step 2: 运行，确认失败**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.PluginRedesignTests -v` → FAIL

- [ ] **Step 3: 替换 `plugins-card`（856-871）为驾驶舱风格**

保留所有原 `id`（`plugin-url`/`plugin-file`/`plugin-json`/`plugin-list`/`plugin-replay-count`/`plugin-editor-json`/`plugin-rule-test-result`）和 `onclick`（`installPluginUrl`/`uploadPluginJson`/`installPluginJson`/`setPluginReplayCount`/`runPluginRuleTest`）：

```html
<div class="card cockpit-card" id="plugins-card">
  <div class="ck-head"><h3><span>🧩</span>插件管理</h3><span class="ck-chip ok" id="plugins-count-pill">已装 0 个</span></div>
  <div class="ctl-section">添加插件</div>
  <div class="install-grid">
    <div class="install-cell"><div class="ii">🔗</div><div class="it">URL 安装</div>
      <input id="plugin-url" class="mock-input" placeholder="https://.../plugin.json">
      <button class="ds-btn" onclick="installPluginUrl()">安装</button></div>
    <div class="install-cell"><div class="ii">📤</div><div class="it">上传文件</div>
      <input id="plugin-file" type="file" accept=".json,application/json" onchange="uploadPluginJson()" class="mock-input">
      <span class="ds-btn-muted">选择 .json</span></div>
    <div class="install-cell"><div class="ii">📋</div><div class="it">粘贴 JSON</div>
      <textarea id="plugin-json" rows="3" class="mock-input" placeholder='{"name":"...","rules":[]}'></textarea>
      <button class="ds-btn" onclick="installPluginJson()">解析安装</button></div>
  </div>
  <div class="ctl-section">已安装插件</div>
  <div id="plugin-list" class="plugin-list"></div>
  <div class="ctl-section">GTW2047 重放倍率</div>
  <select id="plugin-replay-count" onchange="setPluginReplayCount(this.value)" class="ds-select"><option value="1">1×</option><option value="2">2×</option><option value="3">3×</option><option value="4">4×</option><option value="5">5×</option></select>
  <div class="hint">仅重复已启用插件生成的 0x7FF 修改帧</div>
  <details class="collapse"><summary>规则测试器</summary><textarea id="plugin-editor-json" rows="6" class="mock-input"></textarea><button class="ds-btn" onclick="runPluginRuleTest()">规则测试</button><pre id="plugin-rule-test-result"></pre></details>
</div>
```

- [ ] **Step 4: 在 CSS 新增插件卡样式（AP 卡样式段之后）**

```css
.install-grid { display:grid; grid-template-columns:repeat(3,1fr); gap:10px; margin-bottom:14px; }
.install-cell { background:rgba(255,255,255,.04); border:1px solid var(--ds-card-stroke); border-radius:12px; padding:13px 12px; text-align:center; }
.install-cell .ii { font-size:22px; margin-bottom:6px; }
.install-cell .it { font-size:11.5px; color:var(--tx1); margin-bottom:6px; font-weight:600; }
.mock-input { background:rgba(0,0,0,.25); border:1px solid var(--ds-card-stroke); border-radius:7px; padding:6px 8px; font-size:11px; color:var(--tx2); width:100%; margin-bottom:6px; box-sizing:border-box; }
.ds-btn { display:inline-block; background:var(--ds-accent-grad); color:#fff; font-size:11px; padding:5px 12px; border-radius:7px; border:none; cursor:pointer; }
.ds-btn-muted { font-size:10px; color:var(--cockpit-dim); }
.ds-select { background:rgba(0,0,0,.25); border:1px solid var(--ds-card-stroke); border-radius:7px; color:var(--tx1); padding:6px; }
.plugin-list { display:grid; gap:8px; }
.collapse { margin-top:14px; background:rgba(255,255,255,.03); border:1px solid var(--ds-card-stroke); border-radius:10px; padding:10px 14px; font-size:11.5px; color:var(--cockpit-dim); }
```

- [ ] **Step 5: 移动端插件页（3741-3746）同步中文化 + 驾驶舱类**

把 `mob-plugin-page` 内英文（`Plugins`/`Install URL`/`Paste JSON`/`Installed`/`GTW2047 Replay Count`）改为中文，套用 `.install-cell`/`.ds-btn` 类。保留 id（`m-plugin-url`/`m-plugin-json`/`m-plugin-list`/`m-plugin-replay-count`）和 onclick。

- [ ] **Step 6: 运行，确认通过**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.PluginRedesignTests -v` → PASS

- [ ] **Step 7: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h test/test_ui_v11_redesign.py
git commit -m "feat(ui): 插件管理重设计为驾驶舱风格（功能零增减，全中文）"
```

---

## Task 6: 问题 1 — 手机底部导航 5 标签

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:3749-3754`（`#mob-tabs`）、`:3764-3777`（`#mob-more` 解除 single-hide）、JS `:3652-3668`（map）
- Test: `test/test_ui_v11_redesign.py`

- [ ] **Step 1: 写失败契约测试**

```python
class MobileNavTests(UiV11RedesignTests):
    def test_five_tabs(self):
        for label in ["驾驶", "硬件", "速度", "网络", "更多"]:
            # 5 个 mob-tab（standalone 版，在 data-dual-hide 内）
            self.assert_present('data-mobile-page="' + {"驾驶":"driving","硬件":"hardware","速度":"speed","网络":"network","更多":"more"}[label] + '"')

    def test_more_panel_items(self):
        self.assert_present("驾驶风格")
        self.assert_present("FSD 防护")
        self.assert_present("OTA 升级")
        self.assert_present("CAN 诊断")

    def test_showmobile_map_updated(self):
        # 新 map 含 hardware/speed
        self.assert_present("hardware:'pg-hardware'")
        self.assert_present("speed:'pg-speed'")
```

- [ ] **Step 2: 运行，确认失败**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.MobileNavTests -v` → FAIL

- [ ] **Step 3: 重写 `#mob-tabs`（3749-3754）为 5 标签**

```html
<div class="mob-tabs" id="mob-tabs">
  <button class="mob-tab active" data-mobile-page="driving" onclick="showMobilePage('driving')"><span class="mob-icon">▣</span>驾驶</button>
  <button class="mob-tab" data-mobile-page="hardware" onclick="showMobilePage('hardware')"><span class="mob-icon">◇</span>硬件</button>
  <button class="mob-tab" data-mobile-page="speed" onclick="showMobilePage('speed')"><span class="mob-icon">↗</span>速度</button>
  <button class="mob-tab" data-mobile-page="network" onclick="showMobilePage('network')"><span class="mob-icon">◎</span>网络</button>
  <button class="mob-tab" data-mobile-page="more" onclick="toggleMobMore()"><span class="mob-icon">⋯</span>更多</button>
</div>
```

- [ ] **Step 4: 新增 standalone 版「更多」面板（在 `#mob-tabs` 之后，`data-dual-hide` 内）**

复用现有 `toggleMobMore()`（切换 `#mob-more.open`）。新增一个 standalone 专用面板（解除 `data-single-hide`）。在 3754 行 `</div>`（`#mob-tabs` 结束）后插入：

```html
<div class="mob-more-panel" id="mob-more-standalone">
  <div class="mob-more-close" onclick="toggleMobMoreStandalone()">✕</div>
  <div class="mob-more-item" onclick="showMobilePage('drivingstyle')">◉ 驾驶风格</div>
  <div class="mob-more-item" onclick="showPage('pg-defense')">◈ FSD 防护</div>
  <div class="mob-more-item" onclick="showPage('pg-ota')">⇧ OTA 升级</div>
  <div class="mob-more-item" onclick="showPage('pg-can')">⌘ CAN 诊断</div>
</div>
```

- [ ] **Step 5: 更新 JS（3652-3668）map + 新增 toggle**

把 `updateMobTabs`（3652）和 `showMobilePage`（3660）的 `map` 改为：

```javascript
function toggleMobMoreStandalone(){
  var p=$('mob-more-standalone');
  if(p)p.classList.toggle('open');
}
function updateMobTabs(pageId){
  var map={driving:'pg-overview',hardware:'pg-hardware',speed:'pg-speed',network:'pg-network'};
  var tabs=document.querySelectorAll('.mob-tab[data-mobile-page]');
  for(var i=0;i<tabs.length;i++){
    var mob=tabs[i].getAttribute('data-mobile-page');
    tabs[i].classList.toggle('active',mob===pageId||map[mob]===pageId);
  }
}
function showMobilePage(name){
  var map={driving:'pg-overview',hardware:'pg-hardware',speed:'pg-speed',network:'pg-network',drivingstyle:'pg-drive'};
  var panels=document.querySelectorAll('[data-mobile-page-panel]');
  for(var i=0;i<panels.length;i++)panels[i].classList.toggle('active',panels[i].getAttribute('data-mobile-page-panel')===name);
  showPage(map[name]||'pg-overview');
  updateMobTabs(name);
  if(name==='network')pollWifiStatus();
}
```

> 原 map 的 `home/plugins` 删除。`plugins` 内容已在硬件页（Task 5）。初始页 `showMobilePage('driving')`（3687 行 `showMobilePage('home')` 改为 `'driving'`）。

- [ ] **Step 6: 处理 mobile-page panels（3732-3747）**

现有 `mob-home-page`/`mob-driving-page`/`mob-plugin-page`/`mob-network-page`。新导航用桌面 page（pg-overview 等），这些独立 panel 大部分可移除（内容融入桌面页）。**保留 `mob-network-page`（若 network 面板有独立内容）**，移除 `mob-home-page`（空）、`mob-plugin-page`（插件已在硬件页）。`mob-driving-page`（AP 注入）内容移入硬件页或保留为 pg-hardware 内的 AP 区。

> 执行时核查每个 panel 是否被引用；未被引用的移除，避免空 panel 残留。

- [ ] **Step 7: 运行，确认通过**

Run: `PYTHONPATH=test python3 -m unittest test_ui_v11_redesign.MobileNavTests -v` → PASS

- [ ] **Step 8: 双构建回归（手机导航改 JS 易引入跨任务回归，参照 Task 14 教训）**

Run: `pio run -e waveshare_single_can_standalone -e lilygo_t2can 2>&1 | tail -5` → 两个均 SUCCESS

- [ ] **Step 9: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h test/test_ui_v11_redesign.py
git commit -m "feat(ui): 手机底部导航 5 标签（驾驶/硬件/速度/网络/更多）"
```

---

## Task 7-12: 问题 5 — 全板块套驾驶舱基底

这 6 个任务模式一致：每个 page 先 audit 现状（识别工具风元素），再套用 Task 1/4 定义的 `.cockpit-card`/`.ck-head`/`.ctl-row`/`.ds-btn` 等基底类，配色用 `--ds-*`/`--cockpit-*` 变量。每个任务的套用清单基于该 page 的已知结构。

**通用套用规则（每板块适用）：**
1. `page-title` 行 → 包进 `<div class="ck-head"><h3>...标题...</h3><span class="ck-chip">...状态...</span></div>` 结构（标题前可加 emoji）
2. 板块内 `.card` → 加 `cockpit-card` 类（Task 1 已让 `.card` 玻璃化，此处只补图标/胶囊）
3. 数值展示 `.stats`/`.stat` → 可保留（Task 1 的 `.card` 已深色化），关键数值考虑 `.cockpit-metric` 三件套
4. 配色统一用 `--ds-cyan`/`--ds-accent-grad`/`--ds-ok` 等

每个任务契约测试：验证板块标题 + 关键 id 仍 PRESENT（套用不破坏功能）+ 新增 `cockpit-card`/`ck-head` 类 PRESENT。

### Task 7: 驾驶风格页（pg-drive 875）

- [ ] **Step 1: Audit** — 读 875-928 行，记录 `.drive-grid`/`.drive-card`/`page-title` 结构
- [ ] **Step 2: 套用** — `page-title` 改 `ck-head`（标题 ◉ 驾驶风格 + 状态胶囊）；`.drive-card` 配色用 `--ds-accent-grad` active 态
- [ ] **Step 3: 契约** — `test_drive_page_cockpit`：`assert_present('驾驶风格')`、`assert_present('drive-grid')`、新增 `cockpit-card`/`ck-head` 出现
- [ ] **Step 4: 运行 + commit**：`git commit -m "feat(ui): 驾驶风格页套驾驶舱基底"`

### Task 8: 速度策略页（pg-speed 929）

- [ ] **Step 1: Audit** — 读 929-1017 行
- [ ] **Step 2: 套用** — `page-title` → `ck-head`（↗ 速度策略）；速度卡片 `.sel-cards` 已有，配色统一 `--ds-*`
- [ ] **Step 3-4: 契约 + commit** — `assert_present('速度策略')`、`sel-cards` 仍 PRESENT；commit `feat(ui): 速度策略页套驾驶舱基底`

### Task 9: FSD 防护页（pg-defense 1215）

- [ ] **Step 1: Audit** — 读 1215-1303 行
- [ ] **Step 2: 套用** — `page-title`（◈ FSD 防护）→ `ck-head` + 警告胶囊；防护开关用 `.ctl-row`；安全提示用 `.safety-strip`
- [ ] **Step 3-4: 契约 + commit** — 关键防护 id PRESENT；commit `feat(ui): FSD防护页套驾驶舱基底`

### Task 10: OTA 升级页（pg-ota 1304）

- [ ] **Step 1: Audit** — 读 1304-1370 行
- [ ] **Step 2: 套用** — `page-title`（⇧ OTA 升级）→ `ck-head`；拖拽区/进度条用 `--ds-accent-grad`；按钮 `.ds-btn`
- [ ] **Step 3-4: 契约 + commit** — OTA id（如 `ota-drop`/`ota-progress`）PRESENT；commit `feat(ui): OTA升级页套驾驶舱基底`

### Task 11: 网络设置页（pg-network 1371）

- [ ] **Step 1: Audit** — 读 1371-1531 行（含 WiFi/AP 配置）
- [ ] **Step 2: 套用** — `page-title`（◎ 网络设置）→ `ck-head`；表单字段 `.ctl-row`；WiFi 列表卡片 `.cockpit-card`
- [ ] **Step 3-4: 契约 + commit** — WiFi id（`wifi-status`/`wf-ssid` 等，参照 test_wifi_settings_regression）PRESENT；commit `feat(ui): 网络设置页套驾驶舱基底`

### Task 12: CAN 诊断页（pg-can 1532）

- [ ] **Step 1: Audit** — 读 1532-1656 行（已大量用 `.diag-*`/`--cockpit-stroke`，部分已驾驶舱风）
- [ ] **Step 2: 套用** — `page-title`（⌘ CAN 诊断）→ `ck-head`；`.diag-kpi` 已深色，统一 `--ds-*` 强调色；表格 header 已 cockpit 风，仅补 `ck-head`
- [ ] **Step 3-4: 契约 + commit** — 诊断 id（`sniffer`/`can-status` 等）PRESENT；commit `feat(ui): CAN诊断页套驾驶舱基底`

---

## Task 13: 全量验证

**Files:** 无修改（验证 only）

- [ ] **Step 1: 全契约测试**

Run: `PYTHONPATH=test python3 -m unittest discover -s test -v 2>&1 | tail -15`
Expected: 全 OK（含新 `test_ui_v11_redesign` + 既有 `test_dashboard_api_contract`/`test_wifi_settings_regression` 等）

> 若既有契约因 UI 字符串变更回归（参照 Task 5/14 教训：UI 守卫变更使旧断言失败），更新该断言。

- [ ] **Step 2: 双构建**

Run: `pio run -e waveshare_single_can_standalone -e lilygo_t2can 2>&1 | tail -5`
Expected: 两个 SUCCESS（standalone + dual 回归）

- [ ] **Step 3: minify check**

Run: `python3 scripts/minify_dashboard.py --check 2>&1 | tail -3`
Expected: EXIT 0

- [ ] **Step 4: gzip 审计（UI 字符串 PRESENT/ABSENT）**

UI 是 gzip 存储（`DASH_HTML_GZ`），`firmware.bin` 明文搜不到。解压后核查：

```bash
python3 - <<'PY'
import gzip, re
from pathlib import Path
# 定位 DASH_HTML_GZ（在编译后的 ui.h，需先 sync）。或直接核查 src.h 明文（源）。
src = Path("include/web/mcp2515_dashboard_ui.src.h").read_text()
for must_present in ["AP 激活", "插件管理", "🛡", "🧩", "Fail-closed"]:
    assert must_present in src, f"MISSING {must_present}"
for must_absent in ["维修模式 0x339", "Official JSON plugin manager", "Install from URL"]:
    assert must_absent not in src, f"应删除 {must_absent} 仍存在"
print("审计通过")
PY
```

- [ ] **Step 5: 最终整体 diff review（抓跨任务回归）**

Run: `git diff main -- include/web/mcp2515_dashboard_ui.src.h | head -200`
重点核查：双 CAN 移动导航（`#mob-tabs-dual`）未被 Task 6 破坏（Task 14 教训）；AP safety 相关 handler 引用未改。

- [ ] **Step 6: 去 stamping + commit 验证标记**

```bash
git restore include/web/mcp2515_dashboard_ui.h  # committed ui.h 保持 "manual"
git status --short  # 确认仅 src.h + test 改动
```

---

## Task 14: 资产生成 + 烧录（需 Jordan 明确授权）

> **前置：** 此任务仅在 Jordan 明确说「生成资产并烧录」时执行。否则停在 Task 13。

- [ ] **Step 1: 干净重建（刷新 ESP-IDF 版本标签，参照 20260616-0157 教训）**

```bash
rm -rf .pio/build/waveshare_single_can_standalone
pio run -e waveshare_single_can_standalone 2>&1 | tail -3  # SUCCESS
git restore include/web/mcp2515_dashboard_ui.h  # 去 stamping
```

- [ ] **Step 2: 生成资产包**

```bash
TS=$(date +%Y%m%d-%H%M)
mkdir -p firmware-assets/waveshare-single-can-standalone_ui-v11_$TS
# 复制 bootloader/partitions/ota_data（与 20260616-0157 同，分区表未变）+ 新 ota_app + full_merged
# 用 esptool merge_bin（下划线标志）：
esptool.py --chip esp32s3 merge_bin --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 bootloader.bin 0x8000 partitions.bin 0x19000 ota_data_initial.bin 0x20000 firmware.bin \
  -o firmware-assets/.../full_merged.bin
# 生成 BUILD_INFO.md + SHA256SUMS.txt（参照 20260616-0157 模板）
```

- [ ] **Step 3: 烧录（esptool 显式偏移，勿用 pio upload）**

```bash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem21101 --baud 921600 erase_flash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem21101 --baud 921600 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 full_merged.bin
```

- [ ] **Step 4: 运行时验证（串口 diag）**

确认 `[BOOT] canActive=NO`、`injection_active=OFF`、`APActive=no`（安全状态）。UI 改动不改变这些服务端状态。

- [ ] **Step 5: 连 AP WiFi 进 Dashboard 核查**

确认产品名 `Atlas Single CAN`、`/status.capabilities.singleCan=true`、手机底部 5 标签、驾驶中心 AP 激活格、插件/AP 注入驾驶舱风格、首页底部 3 块已删。

---

## Self-Review

**1. Spec coverage:**
- 问题 1（手机导航）→ Task 6 ✓
- 问题 2（CAN2→AP）→ Task 3 ✓
- 问题 3a（插件）→ Task 5 ✓
- 问题 3b（AP 注入）→ Task 4 ✓
- 问题 4（删 3 块）→ Task 2 ✓
- 问题 5（全板块统一）→ Task 1（基底）+ Task 7-12（套用）✓
- 设计系统基底 → Task 1 ✓
- 非目标（不改后端/安全/功能）→ 各任务均保留原 id/onclick，Task 4 明示安全线不变 ✓
- 验证 → Task 13 ✓

**2. Placeholder scan:** 无 TBD/TODO。Task 7-12 的「Audit」步骤是真实读代码动作（给出行号），套用规则具体（类名 + 变量），非占位符。Task 3/4 的 JS 字段名（`apActive`/`apGateProgress`）标注「按 /status 实际字段调整」——执行时核查，非占位符。

**3. Type consistency:** `ov-ap`/`ov-ap-hint`/`ap-state-panel`/`ap-gate-bar`/`mob-more-standalone` 等 id 在定义任务和契约测试中一致。`showMobilePage` map 在 Task 6 定义后各处引用一致。`saveApGateControls`/`installPluginUrl` 等原 onclick 全程保留未改名。

**4. 跨任务回归风险:** Task 6（手机导航）改 JS 易破坏双 CAN 移动 UX（Task 14 教训）——Task 6 Step 8 双构建 + Task 13 Step 5 整体 diff review 覆盖。
