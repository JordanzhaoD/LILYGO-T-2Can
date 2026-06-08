# Atlas T-2CAN UI Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the approved driving-first Atlas T-2CAN dashboard redesign in the firmware-embedded UI, with precise Chinese copy as the default visible UI language.

**Architecture:** Keep the current ESP32 dashboard architecture: one firmware-embedded HTML/CSS/JS source in `include/web/mcp2515_dashboard_ui.src.h`, generated/minified into `include/web/mcp2515_dashboard_ui.h` by `scripts/minify_dashboard.py`, and served by existing backend routes in `include/web/mcp2515_dashboard.h`. This plan changes frontend structure, CSS, copy, and client-side interactions only; backend API behavior remains unchanged.

**Tech Stack:** Vanilla HTML/CSS/JavaScript inside a C++ raw string, Python `unittest` contract tests, `scripts/webui_simulator.py` for local preview, `scripts/minify_dashboard.py --check`, PlatformIO native tests and firmware build.

---

## Design decisions locked in

Jordan approved the high-fidelity concept at:

- `designs/atlas-t2can-ui-redesign/Atlas T-2CAN UI Redesign.html`
- `designs/atlas-t2can-ui-redesign/DESIGN_NOTES.md`

Implement these approved directions without changing the concept:

1. **车机首页 / 驾驶状态中心**
   - Large FSD injection state.
   - Large hold-to-enable primary control.
   - Always-visible CAN/FSD defense/HW/drive/speed/runtime summary.
   - Car-cockpit feeling for horizontal tablet/car display.

2. **手机首页 / 现场遥控器**
   - Native-app-style card hierarchy.
   - One obvious primary action.
   - Low information density.
   - Dangerous actions moved away from first-touch surfaces.

3. **CAN 诊断 / 工程模式**
   - Keep engineering density.
   - Add clear KPI rail, filter, segmented tools, and error-first rows.
   - Do not force this density onto home/mobile screens.

4. **精确汉化**
   - Default visible UI copy must be precise Chinese.
   - Keep technical acronyms when they are the standard name: `FSD`, `CAN`, `CAN1`, `CAN2`, `HW3`, `HW4`, `NVS`, `OTA`, `API`, `TX`, `RX`, `EFLG`, `DBC`, `GPIO`.
   - Replace awkward product copy:
     - `破解模式` → `硬件模式` or `激活模式`
     - `FSD 智能破解模式` → `FSD 硬件模式`
     - `FSD防封` → `FSD 防护`
     - `Drive Console`, `Cockpit Home`, `Remote Card`, `Engineering Mode`, `Hold to Enable` must not appear in the default Chinese UI.
   - English translations may remain in the language map for the existing EN toggle, but the default rendered markup and new Chinese source strings must be Chinese.

## File structure

Modify these files:

- `include/web/mcp2515_dashboard_ui.src.h`
  - Main implementation file. Add design tokens, cockpit layout CSS, mobile remote CSS, diagnostic layout CSS, updated overview/mobile/CAN markup, and small JS helpers.

- `include/web/mcp2515_dashboard_ui.h`
  - Generated file. Regenerate from `mcp2515_dashboard_ui.src.h` using `scripts/minify_dashboard.py`.

- `test/test_dashboard_api_contract.py`
  - Add regression tests for the redesign tokens, precise Chinese default copy, required IDs, and preserved backend contracts.

- `docs/dashboard.md`
  - Update the user-facing dashboard description after implementation so documentation matches the new UI.

Do not modify these files unless a test proves they are required:

- `include/web/mcp2515_dashboard.h`
- `include/handlers.h`
- `src/main.cpp`
- API endpoints
- NVS keys

---

### Task 1: Add UI redesign contract tests first

**Files:**
- Modify: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add tests for the approved UI direction and precise Chinese copy**

Append the following methods inside `class DashboardApiContractTests(unittest.TestCase):`, after `test_ui_explains_legacy_real_wiring_states` and before `test_release_metadata_and_lilygo_ci_are_wired`.

```python
    def test_atlas_redesign_chinese_default_copy_is_precise(self) -> None:
        """Driving-first redesign must use precise Chinese for default visible UI copy."""
        required_copy = [
            "驾驶状态中心",
            "FSD 注入",
            "长按 2 秒启用 FSD 注入",
            "长按 2 秒关闭 FSD 注入",
            "硬件模式",
            "驾驶风格",
            "速度策略",
            "FSD 防护",
            "现场遥控",
            "连接到设备热点",
            "长按启用",
            "长按关闭",
            "工程模式",
            "错误优先",
            "帧筛选",
            "实时帧",
            "记录器",
            "控制器",
            "调试",
            "最近写入",
        ]
        for token in required_copy:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

        forbidden_default_copy = [
            "Cockpit Home",
            "Remote Card",
            "Engineering Mode",
            "Drive Console",
            "Hold to Enable",
            "Hold to Disable",
            "PRIMARY",
            "破解模式",
            "FSD 智能破解模式",
            "防封",
        ]
        for token in forbidden_default_copy:
            with self.subTest(token=token):
                self.assertNotIn(token, self.ui)

    def test_atlas_redesign_cockpit_home_structure_is_present(self) -> None:
        """Overview page must become the driving-first cockpit home while preserving existing IDs."""
        overview_page = re.search(r'id="pg-overview".*?id="pg-hardware"', self.ui, re.S)
        self.assertIsNotNone(overview_page)
        body = overview_page.group(0)
        for token in [
            'class="cockpit-home"',
            'class="cockpit-primary"',
            'id="fsd-toggle"',
            'id="fsd-label"',
            'id="ov-master-tgl"',
            'id="ov-fsd-tgl"',
            'id="fsd-boot-tgl"',
            'id="s-can"',
            'id="s-can2"',
            'id="s-fps"',
            'id="s-temp"',
            'id="s-ver"',
            'id="ov-hw"',
            'id="m-hw"',
            'id="m-drive"',
            'id="m-speed"',
            'id="m-defense"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_atlas_redesign_mobile_remote_structure_is_present(self) -> None:
        """Mobile home must be a remote-control surface, not a compressed desktop stack."""
        for token in [
            'class="mobile-remote"',
            'class="mobile-primary-card"',
            'id="m-fsd-tgl"',
            'id="m-can"',
            'id="m-can2"',
            'id="m-fsd"',
            'id="m-fps"',
            'id="m-alert"',
            '现场遥控',
            '底部导航',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_atlas_redesign_can_diagnostics_structure_is_present(self) -> None:
        """CAN tools must expose engineering-mode hierarchy and preserve existing tabs/IDs."""
        can_page = re.search(r'id="pg-can".*?id="pg-shift"', self.ui, re.S)
        self.assertIsNotNone(can_page)
        body = can_page.group(0)
        for token in [
            'class="diag-shell"',
            'class="diag-kpi-rail"',
            'id="sniff-filter"',
            'id="sniff-pause"',
            'id="sniff-rows"',
            'id="can-recorder"',
            'id="can-controller"',
            'id="can-debug"',
            'id="ctrl-eflg"',
            'id="ctrl-rxerr"',
            'id="ctrl-txerr"',
            'id="lw-injected"',
            'id="lw-match"',
            '错误优先',
            '帧筛选',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)
```

- [ ] **Step 2: Run the new tests and verify they fail**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'atlas_redesign' -q
```

Expected: FAIL. The failure should report missing tokens such as `驾驶状态中心`, `class="cockpit-home"`, or `class="diag-shell"`.

- [ ] **Step 3: Commit the failing tests**

```bash
git add test/test_dashboard_api_contract.py
git commit -m "test: capture atlas dashboard redesign contract"
```

---

### Task 2: Add design tokens and layout CSS

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:15-380`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add focused CSS tokens after the existing `:root` block**

In `include/web/mcp2515_dashboard_ui.src.h`, after the existing `--sidebar-w: 220px;` line, add these tokens inside `:root`:

```css
  --cockpit-bg: #05070b;
  --cockpit-panel: rgba(15,23,42,0.82);
  --cockpit-panel-strong: #101827;
  --cockpit-soft: rgba(30,41,59,0.62);
  --cockpit-stroke: rgba(148,163,184,0.20);
  --cockpit-stroke-strong: rgba(148,163,184,0.36);
  --cockpit-muted: #8d9bb4;
  --cockpit-dim: #66748c;
  --cockpit-cyan: #38bdf8;
  --cockpit-radius-xl: 28px;
  --cockpit-radius-lg: 20px;
  --cockpit-shadow: 0 24px 64px rgba(0,0,0,0.35);
```

- [ ] **Step 2: Add cockpit/home/mobile/diagnostic CSS before `/* === Scrollbar === */`**

Insert this CSS block before the existing `/* === Scrollbar === */` comment:

```css
/* === Atlas driving-first redesign === */
.cockpit-home { display: grid; gap: 16px; }
.cockpit-status-bar { display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 10px; }
.cockpit-title small { display: block; color: var(--cockpit-muted); font-size: 11px; font-weight: 800; letter-spacing: .12em; text-transform: uppercase; }
.cockpit-title h2 { margin: 4px 0 0; font-size: 30px; line-height: 1; letter-spacing: -0.04em; }
.cockpit-chip-row { display: flex; flex-wrap: wrap; gap: 8px; }
.cockpit-chip { border: 1px solid var(--cockpit-stroke); border-radius: 999px; padding: 8px 12px; background: rgba(15,23,42,.62); color: var(--tx3); font-size: 12px; font-weight: 800; }
.cockpit-chip.ok { color: var(--ok); border-color: rgba(74,222,128,.35); background: rgba(74,222,128,.08); }
.cockpit-chip.warn { color: var(--warn); border-color: rgba(251,191,36,.35); background: rgba(251,191,36,.08); }
.cockpit-grid { display: grid; grid-template-columns: minmax(0, 1.12fr) minmax(320px, .88fr); gap: 16px; }
.cockpit-primary { min-height: 340px; border: 1px solid var(--cockpit-stroke); border-radius: var(--cockpit-radius-xl); padding: 24px; background: radial-gradient(circle at 85% 0%, rgba(124,58,237,.18), transparent 32%), linear-gradient(145deg, rgba(15,23,42,.92), rgba(8,13,24,.78)); box-shadow: var(--cockpit-shadow); display: flex; flex-direction: column; justify-content: space-between; overflow: hidden; }
.cockpit-state-label { color: var(--cockpit-muted); font-size: 12px; font-weight: 900; letter-spacing: .14em; text-transform: uppercase; }
.cockpit-state { font-size: clamp(74px, 11vw, 132px); line-height: .82; font-weight: 900; letter-spacing: -.09em; margin: 16px 0 8px; color: var(--err); text-shadow: 0 0 26px rgba(248,113,113,.18); }
.cockpit-state.on { color: var(--ok); text-shadow: 0 0 30px rgba(74,222,128,.22); }
.cockpit-state-desc { color: var(--tx2); font-size: 14px; line-height: 1.55; max-width: 520px; }
.cockpit-hold { width: 100%; min-height: 68px; margin-top: 18px; border: none; border-radius: 22px; background: linear-gradient(135deg, #991b1b, #ef4444); color: white; font-size: 16px; font-weight: 900; letter-spacing: .04em; cursor: pointer; box-shadow: 0 16px 36px rgba(239,68,68,.20); }
.cockpit-hold.on { background: linear-gradient(135deg, #047857, #22c55e); box-shadow: 0 16px 36px rgba(34,197,94,.20); }
.cockpit-metrics { display: grid; grid-template-columns: repeat(4, minmax(0,1fr)); gap: 10px; }
.cockpit-metric { min-height: 92px; border: 1px solid var(--cockpit-stroke); border-radius: var(--cockpit-radius-lg); background: rgba(15,23,42,.58); padding: 14px; }
.cockpit-metric .lbl { color: var(--cockpit-muted); font-size: 10px; font-weight: 900; letter-spacing: .08em; text-transform: uppercase; }
.cockpit-metric .val { margin-top: 8px; color: var(--tx1); font-size: 24px; font-weight: 900; letter-spacing: -0.04em; overflow-wrap: anywhere; }
.cockpit-metric .hint { margin-top: 4px; color: var(--cockpit-dim); font-size: 11px; }
.cockpit-vehicle { border: 1px solid var(--cockpit-stroke); border-radius: var(--cockpit-radius-xl); background: linear-gradient(145deg, rgba(15,23,42,.68), rgba(8,13,24,.86)); padding: 20px; box-shadow: var(--cockpit-shadow); }
.cockpit-mode-grid { display: grid; grid-template-columns: repeat(3, minmax(0,1fr)); gap: 10px; margin-top: 14px; }
.cockpit-mode { border: 1px solid var(--cockpit-stroke); border-radius: 18px; background: rgba(15,23,42,.55); padding: 14px; }
.cockpit-mode.active { border-color: rgba(124,58,237,.58); background: rgba(124,58,237,.16); }
.cockpit-mode strong { display: block; margin-top: 5px; font-size: 16px; }
.cockpit-mode span { display: block; margin-top: 4px; color: var(--tx3); font-size: 11px; line-height: 1.35; }
.cockpit-warning { border: 1px solid rgba(251,191,36,.35); border-radius: 18px; background: rgba(251,191,36,.08); padding: 14px; color: var(--tx2); font-size: 13px; line-height: 1.45; }
.mobile-remote { display: none; }
.mobile-primary-card { border-radius: 26px; padding: 20px; color: #fff; background: radial-gradient(circle at 82% 0%, rgba(96,165,250,.32), transparent 34%), linear-gradient(145deg, #111827, #312e81); box-shadow: 0 18px 42px rgba(49,46,129,.28); }
.mobile-primary-card .mobile-state { font-size: 58px; line-height: .9; font-weight: 900; letter-spacing: -.08em; margin: 12px 0 4px; color: #fecaca; }
.mobile-primary-card .mobile-state.on { color: #bbf7d0; }
.mobile-primary-card p { color: #cbd5e1; font-size: 13px; line-height: 1.45; margin: 0; }
.mobile-hold { width: 100%; min-height: 54px; margin-top: 14px; border: none; border-radius: 18px; background: #ef4444; color: white; font-weight: 900; }
.mobile-hold.on { background: #16a34a; }
.mobile-remote-grid { display: grid; grid-template-columns: repeat(2, minmax(0,1fr)); gap: 10px; margin-top: 12px; }
.mobile-remote-card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 18px; padding: 12px; }
.mobile-remote-card .lbl { color: var(--tx3); font-size: 10px; font-weight: 900; letter-spacing: .08em; text-transform: uppercase; }
.mobile-remote-card .val { margin-top: 7px; font-size: 20px; font-weight: 900; overflow-wrap: anywhere; }
.mobile-alert-card { margin-top: 12px; border: 1px solid rgba(251,191,36,.35); border-radius: 18px; background: rgba(251,191,36,.08); padding: 12px; color: var(--tx2); font-size: 12px; line-height: 1.45; }
.diag-shell { display: grid; grid-template-columns: 260px minmax(0,1fr); gap: 14px; }
.diag-kpi-rail { display: grid; gap: 10px; }
.diag-kpi { border: 1px solid var(--border); border-radius: 16px; background: var(--card-bg); padding: 13px; }
.diag-kpi .lbl { color: var(--tx3); font-size: 10px; font-weight: 900; letter-spacing: .08em; text-transform: uppercase; }
.diag-kpi .val { margin-top: 7px; font-size: 22px; font-weight: 900; overflow-wrap: anywhere; }
.diag-main-panel { min-width: 0; }
.diag-toolbar { display: flex; gap: 8px; margin-bottom: 10px; }
.diag-toolbar .inp { flex: 1; }
.diag-table-shell { max-height: 360px; overflow: auto; border: 1px solid var(--border); border-radius: 16px; }
.diag-table-shell .tbl th { position: sticky; top: 0; background: var(--card-bg); z-index: 1; }
.diag-row-warn { background: rgba(251,191,36,.08); }
.diag-row-error { background: rgba(248,113,113,.10); }
@media (max-width: 900px) {
  .cockpit-grid { grid-template-columns: 1fr; }
  .cockpit-metrics { grid-template-columns: repeat(2, minmax(0,1fr)); }
  .diag-shell { grid-template-columns: 1fr; }
}
@media (max-width: 768px) {
  .desktop-home { display: none; }
  .mobile-remote { display: block; }
  .cockpit-title h2 { font-size: 22px; }
  .cockpit-status-bar { align-items: flex-start; }
  .cockpit-chip-row { width: 100%; }
  .cockpit-primary { min-height: 0; padding: 18px; border-radius: 22px; }
  .cockpit-state { font-size: 62px; }
  .cockpit-hold { min-height: 56px; border-radius: 18px; font-size: 14px; }
  .cockpit-mode-grid { grid-template-columns: 1fr; }
  .diag-toolbar { flex-direction: column; }
}
```

- [ ] **Step 3: Run the redesign tests and verify CSS tokens are still failing only because markup is missing**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'atlas_redesign' -q
```

Expected: still FAIL, but no syntax errors while reading `mcp2515_dashboard_ui.src.h`.

- [ ] **Step 4: Commit CSS foundation**

```bash
git add include/web/mcp2515_dashboard_ui.src.h
git commit -m "feat: add atlas cockpit design tokens"
```

---

### Task 3: Replace Overview with driving-first cockpit home and mobile remote

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:432-726`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Replace only the body of `pg-overview`**

In `include/web/mcp2515_dashboard_ui.src.h`, replace everything from:

```html
    <!-- Page 1: Overview -->
    <div class="page active" id="pg-overview">
```

through the closing `</div>` immediately before:

```html
    <!-- Page 2: Hardware Config -->
```

with this HTML. It preserves the IDs used by existing JavaScript and tests.

```html
    <!-- Page 1: Overview -->
    <div class="page active" id="pg-overview">
      <div class="desktop-home cockpit-home">
        <div class="cockpit-status-bar">
          <div class="cockpit-title">
            <small>ATLAS T-2CAN</small>
            <h2>驾驶状态中心</h2>
          </div>
          <div class="cockpit-chip-row">
            <span class="cockpit-chip" id="st-module-ui">UI 待同步</span>
            <span class="cockpit-chip" id="st-module-nvs">NVS 读取中</span>
            <span class="cockpit-chip" id="st-module-run">运行状态待检测</span>
          </div>
        </div>

        <div class="cockpit-grid">
          <div class="cockpit-home-main">
            <div class="cockpit-primary">
              <div>
                <div class="cockpit-state-label">FSD 注入</div>
                <div class="cockpit-state" id="fsd-label">已关闭</div>
                <div class="cockpit-state-desc" id="ov-fsd-desc">模块总开关关闭；启用前请确认车辆处于安全状态。</div>
              </div>
              <button class="cockpit-hold" id="fsd-toggle" onclick="toggleFsd()">长按 2 秒启用 FSD 注入</button>
            </div>

            <div class="cockpit-metrics">
              <div class="cockpit-metric"><div class="lbl">硬件模式</div><div class="val" id="ov-hw">Auto</div><div class="hint">自动识别</div></div>
              <div class="cockpit-metric"><div class="lbl">驾驶风格</div><div class="val" id="m-drive">Normal</div><div class="hint">均衡响应</div></div>
              <div class="cockpit-metric"><div class="lbl">速度策略</div><div class="val" id="m-speed">auto</div><div class="hint">自动偏移</div></div>
              <div class="cockpit-metric"><div class="lbl">运行时间</div><div class="val" id="ov-up">00:00:00</div><div class="hint">设备在线</div></div>
            </div>

            <div class="cockpit-metrics">
              <div class="cockpit-metric"><div class="lbl">CAN1</div><div class="val" id="s-can">Offline</div><div class="hint" id="m-can">--</div></div>
              <div class="cockpit-metric"><div class="lbl">CAN2</div><div class="val" id="s-can2">Idle</div><div class="hint" id="m-can2">--</div></div>
              <div class="cockpit-metric"><div class="lbl">帧率</div><div class="val" id="s-fps">0.0 Hz</div><div class="hint" id="m-fps">0.0 Hz</div></div>
              <div class="cockpit-metric"><div class="lbl">芯片温度</div><div class="val" id="s-temp">--</div><div class="hint">高于 60°C 标红</div></div>
            </div>
          </div>

          <div class="cockpit-vehicle">
            <div class="cockpit-status-bar">
              <div class="cockpit-title">
                <small>车辆与防护</small>
                <h2>现场状态</h2>
              </div>
              <span class="cockpit-chip warn" id="tb-fsd-local">FSD 防护</span>
            </div>
            <div class="cockpit-mode-grid">
              <div class="cockpit-mode active"><div>🎯</div><strong id="drive-current">Normal</strong><span>当前驾驶风格</span></div>
              <div class="cockpit-mode"><div>🛡️</div><strong id="m-defense">防护中</strong><span>FSD 防护状态</span></div>
              <div class="cockpit-mode"><div>↗</div><strong id="s-soff">0</strong><span>速度偏移</span></div>
            </div>
            <div class="cockpit-mode-grid">
              <div class="cockpit-mode"><div>⚙</div><strong id="s-hw">--</strong><span>实际硬件模式</span></div>
              <div class="cockpit-mode"><div>📦</div><strong id="s-ver">--</strong><span>固件版本</span></div>
              <div class="cockpit-mode"><div>📡</div><strong id="m-rxtx">0 / 0</strong><span>RX / TX</span></div>
            </div>
            <div class="cockpit-warning" id="m-alert">暂无异常。实验功能请在停车和安全环境下验证。</div>
          </div>
        </div>

        <div class="card">
          <div class="card-title">核心开关</div>
          <div class="setting-row">
            <div>
              <div class="setting-name">FSD-V14 模式 <span class="exp-badge">实验</span></div>
              <div class="setting-desc">独立切换 MAX/V14 实验 profile，不影响模块总开关。</div>
            </div>
            <label class="tgl"><input type="checkbox" id="ov-fsd-tgl" onchange="toggleV14Mode()"><div class="tgl-track"></div></label>
          </div>
          <div class="setting-row">
            <div>
              <div class="setting-name">模块总开关</div>
              <div class="setting-desc">控制当前 FSD 注入模块启停；状态会同步保存。</div>
            </div>
            <label class="tgl"><input type="checkbox" id="ov-master-tgl" onchange="toggleFsd()"><div class="tgl-track"></div></label>
          </div>
          <div class="setting-row">
            <div>
              <div class="setting-name">开机自动启用</div>
              <div class="setting-desc">设备重启后按保存值恢复 FSD 注入。</div>
            </div>
            <label class="tgl"><input type="checkbox" id="fsd-boot-tgl" onchange="saveConfig()"><div class="tgl-track"></div></label>
          </div>
        </div>

        <div class="card">
          <div class="card-title">设备信息</div>
          <div class="stats">
            <div class="stat"><div class="stat-lbl">FSD-V14</div><div class="stat-val v-dim" id="ov-v14">待机</div></div>
            <div class="stat"><div class="stat-lbl">CAN1 RX/TX</div><div class="stat-val v-info" id="s-rx">0/0</div></div>
            <div class="stat"><div class="stat-lbl">TX Errors</div><div class="stat-val v-dim" id="s-txerr">0</div></div>
            <div class="stat"><div class="stat-lbl">跟随距离</div><div class="stat-val v-dim" id="s-fd">--</div></div>
            <div class="stat"><div class="stat-lbl">车辆 OTA</div><div class="stat-val v-acc" id="s-vota">正常</div></div>
            <div class="stat"><div class="stat-lbl">OTA 确认</div><div class="stat-val v-dim" id="s-vota-cnt">0</div></div>
          </div>
        </div>

        <div class="card">
          <div class="card-title">功耗管理</div>
          <div class="setting-row">
            <div><div class="setting-name">自动关机</div><div class="setting-desc">5 分钟无 CAN 数据自动休眠。</div></div>
            <label class="tgl"><input type="checkbox" id="ov-auto-shutdown" onchange="toggleAutoShutdown(this.checked)"><div class="tgl-track"></div></label>
          </div>
          <div class="setting-row">
            <div><div class="setting-name">WiFi 自动关闭</div><div class="setting-desc">5 分钟无操作关闭网络中转。</div></div>
            <label class="tgl"><input type="checkbox" id="ov-wifi-auto-off" onchange="toggleWifiAutoOff(this.checked)"><div class="tgl-track"></div></label>
          </div>
        </div>
      </div>

      <div class="mobile-remote">
        <div class="cockpit-status-bar">
          <div class="cockpit-title">
            <small>ATLAS REMOTE</small>
            <h2>现场遥控</h2>
          </div>
          <span class="cockpit-chip ok">连接到设备热点</span>
        </div>
        <div class="mobile-primary-card">
          <div class="cockpit-state-label">FSD 注入</div>
          <div class="mobile-state" id="m-fsd">OFF</div>
          <p>普通点击不会触发危险动作；请长按确认后再启用模块。</p>
          <button class="mobile-hold" onclick="toggleFsd()">长按启用</button>
        </div>
        <div class="mobile-remote-grid">
          <div class="mobile-remote-card"><div class="lbl">CAN1</div><div class="val" id="m-can">--</div></div>
          <div class="mobile-remote-card"><div class="lbl">CAN2</div><div class="val" id="m-can2">--</div></div>
          <div class="mobile-remote-card"><div class="lbl">硬件模式</div><div class="val" id="m-hw">Auto</div></div>
          <div class="mobile-remote-card"><div class="lbl">帧率</div><div class="val" id="m-fps">0.0 Hz</div></div>
        </div>
        <div class="mobile-alert-card">底部导航保留常用入口；重启、爆闪、维修模式等危险操作移入 More 和二级页面。</div>
      </div>
    </div>
```

- [ ] **Step 2: Run overview structure test and verify it passes or fails only on JS copy**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'cockpit_home_structure or mobile_remote_structure or chinese_default_copy' -q
```

Expected: `cockpit_home_structure` and `mobile_remote_structure` PASS. `chinese_default_copy` may still FAIL until Task 6 updates JS button labels.

- [ ] **Step 3: Commit overview/mobile markup**

```bash
git add include/web/mcp2515_dashboard_ui.src.h
git commit -m "feat: redesign dashboard overview as cockpit home"
```

---

### Task 4: Rename awkward default Chinese copy across navigation and hardware page

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:389-406`
- Modify: `include/web/mcp2515_dashboard_ui.src.h:728-758`
- Modify: `include/web/mcp2515_dashboard_ui.src.h:1589-1700` language map area if present
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Update visible navigation labels**

In the sidebar navigation, keep the same `data-page` attributes and change only labels:

```html
<div class="nav-item active" data-page="pg-overview"><span class="nav-icon">▣</span>驾驶状态</div>
<div class="nav-item" data-page="pg-hardware"><span class="nav-icon">◇</span>硬件模式</div>
<div class="nav-item" data-page="pg-drive"><span class="nav-icon">◉</span>驾驶风格</div>
<div class="nav-item" data-page="pg-speed"><span class="nav-icon">↗</span>速度策略</div>
<div class="nav-item" data-page="pg-bus2"><span class="nav-icon">✦</span>CAN2 控制</div>
<div class="nav-item" data-page="pg-strobe"><span class="nav-icon">⚡</span>灯光特技</div>
<div class="nav-item" data-page="pg-defense"><span class="nav-icon">◈</span>FSD 防护</div>
<div class="nav-item" data-page="pg-ota"><span class="nav-icon">⇧</span>OTA 升级</div>
<div class="nav-item" data-page="pg-network"><span class="nav-icon">◎</span>网络设置</div>
<div class="nav-item" data-page="pg-can"><span class="nav-icon">⌘</span>CAN 诊断</div>
<div class="nav-item" data-page="pg-shift"><span class="nav-icon">⚙</span>自动换挡</div>
```

- [ ] **Step 2: Update hardware page labels without changing function calls**

Replace the hardware page title/card text with:

```html
<div class="page-title">硬件模式</div>
```

and:

```html
<div class="card-title">FSD 硬件模式</div>
<div class="card-subtitle">选择车辆自动驾驶硬件版本；Auto 会使用固件现有探测逻辑。</div>
```

- [ ] **Step 3: Update the i18n Chinese navigation list and English map together**

Find the existing navigation translation tokens tested by `test_batch_c_sidebar_i18n_and_version_display_are_stable`. Update that test in Task 5, but first update the source to include both Chinese and English labels.

The Chinese list must be exactly:

```javascript
'驾驶状态','硬件模式','驾驶风格','速度策略','CAN2 控制','灯光特技','FSD 防护','OTA 升级','网络设置','CAN 诊断','自动换挡'
```

The English map must include these pairs:

```javascript
'驾驶状态':'Drive Status',
'硬件模式':'Hardware Mode',
'驾驶风格':'Drive Style',
'速度策略':'Speed Strategy',
'CAN2 控制':'CAN2 Control',
'灯光特技':'Light Show',
'FSD 防护':'FSD Guard',
'OTA 升级':'OTA Update',
'网络设置':'Network',
'CAN 诊断':'CAN Diagnostics',
'自动换挡':'Auto Shift'
```

Do not remove existing backend IDs or page IDs.

- [ ] **Step 4: Run Chinese copy test and expect only test file mismatch for old i18n assertion**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'chinese_default_copy or sidebar_i18n' -q
```

Expected: `chinese_default_copy` should get closer to passing; `sidebar_i18n` may fail because the existing test still expects the old list.

- [ ] **Step 5: Commit copy cleanup**

```bash
git add include/web/mcp2515_dashboard_ui.src.h
git commit -m "feat: refine dashboard Chinese copy"
```

---

### Task 5: Update existing i18n contract for the new precise Chinese labels

**Files:**
- Modify: `test/test_dashboard_api_contract.py:1280-1286`

- [ ] **Step 1: Replace the old sidebar i18n assertions**

Change `test_batch_c_sidebar_i18n_and_version_display_are_stable` to this exact body:

```python
    def test_batch_c_sidebar_i18n_and_version_display_are_stable(self) -> None:
        """New UI pages must not break nav translations, and overview version should use /system_status firmware."""
        for token in [
            "'灯光特技':'Light Show'",
            "'自动换挡':'Auto Shift'",
            "'驾驶状态':'Drive Status'",
            "'硬件模式':'Hardware Mode'",
            "'驾驶风格':'Drive Style'",
            "'速度策略':'Speed Strategy'",
            "'CAN 诊断':'CAN Diagnostics'",
            "'FSD 防护':'FSD Guard'",
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)
        self.assertIn("'驾驶状态','硬件模式','驾驶风格','速度策略','CAN2 控制','灯光特技','FSD 防护','OTA 升级','网络设置','CAN 诊断','自动换挡'", self.ui)
        self.assertIn("d&&(d.firmware||d.version)", self.ui)
        self.assertIn("setText('s-ver',d.firmware||d.version);", self.ui)
```

- [ ] **Step 2: Run i18n tests**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'sidebar_i18n or chinese_default_copy' -q
```

Expected: PASS if Task 4 is complete; otherwise fail only on tokens not yet updated.

- [ ] **Step 3: Commit test update**

```bash
git add test/test_dashboard_api_contract.py
git commit -m "test: update dashboard i18n contract"
```

---

### Task 6: Update JavaScript labels and FSD state rendering

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:1583-3400`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add a helper for FSD visual state**

In the JavaScript section, near existing helper functions such as `setText`, add:

```javascript
function setFsdVisualState(on){
  var stateText=on?'已启用':'已关闭';
  var mobileText=on?'ON':'OFF';
  setText('fsd-label',stateText);
  setText('m-fsd',mobileText);
  var desc=on?'FSD 注入模块已启用；请持续关注 CAN 状态和防护状态。':'模块总开关关闭；启用前请确认车辆处于安全状态。';
  setText('ov-fsd-desc',desc);
  var btn=$('fsd-toggle');
  if(btn){btn.textContent=on?'长按 2 秒关闭 FSD 注入':'长按 2 秒启用 FSD 注入';btn.classList.toggle('on',!!on);}
  var mobileBtn=document.querySelector('.mobile-hold');
  if(mobileBtn){mobileBtn.textContent=on?'长按关闭':'长按启用';mobileBtn.classList.toggle('on',!!on);}
  var state=$('fsd-label');
  if(state){state.classList.toggle('on',!!on);}
  var mobileState=$('m-fsd');
  if(mobileState){mobileState.classList.toggle('on',!!on);}
}
```

- [ ] **Step 2: Call helper from status polling**

In the existing status polling/update function that processes `/status`, find where `canActive`, `ia`, `m-fsd`, `fsd-label`, or `ov-master-tgl` are updated. After `S.can` / `S.ia` is assigned, call:

```javascript
setFsdVisualState(!!(d.canActive||d.ia||d.injectionActive));
```

If the status object uses different field names locally, use the existing boolean that currently drives `id="fsd-label"` and `id="m-fsd"`.

- [ ] **Step 3: Ensure `toggleFsd()` keeps UI responsive**

Inside `toggleFsd()`, after a successful `postForm(...)` and before/after reloading status, add optimistic visual update using the current checkbox value:

```javascript
var master=$('ov-master-tgl');
if(master)setFsdVisualState(!!master.checked);
```

Do not remove the existing backend call or refresh call.

- [ ] **Step 4: Replace old English/prototype strings in source**

Search for these strings in `mcp2515_dashboard_ui.src.h` and remove or translate them if present:

```text
Cockpit Home
Remote Card
Engineering Mode
Drive Console
Hold to Enable
Hold to Disable
PRIMARY
FSD 智能破解模式
破解模式
防封
```

Allowed: English translations in the i18n map from Task 4, and technical acronyms such as `CAN`, `FSD`, `OTA`, `NVS`, `TX`, `RX`.

- [ ] **Step 5: Run redesign copy tests**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'atlas_redesign_chinese_default_copy or cockpit_home_structure or mobile_remote_structure' -q
```

Expected: PASS.

- [ ] **Step 6: Commit JS and copy updates**

```bash
git add include/web/mcp2515_dashboard_ui.src.h
git commit -m "feat: wire redesigned FSD visual state"
```

---

### Task 7: Redesign CAN tools page as Engineering Mode while preserving IDs

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:1442-1544`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Replace the top of `pg-can` through the sub-tabs**

Inside `<div class="page" id="pg-can">`, replace the page title, CAN pin card, and sub-tabs area with this structure. Keep the existing `can-sniffer`, `can-recorder`, `can-controller`, and `can-debug` containers below it.

```html
<div class="page-title">CAN 诊断</div>
<div class="diag-shell">
  <aside class="diag-kpi-rail">
    <div class="diag-kpi"><div class="lbl">CAN1 RX/TX</div><div class="val" id="diag-rxtx">0 / 0</div></div>
    <div class="diag-kpi"><div class="lbl">CAN2 EFLG</div><div class="val" id="diag-eflg">0x00</div></div>
    <div class="diag-kpi"><div class="lbl">TX Errors</div><div class="val" id="diag-txerr">0</div></div>
    <div class="diag-kpi"><div class="lbl">最近写入</div><div class="val" id="diag-last-write">--</div></div>
    <div class="card">
      <div class="card-title">CAN 引脚</div>
      <div class="diag-grid">
        <div class="diag-item"><span class="lbl">CAN1 TX</span><span class="v-acc">GPIO 7</span></div>
        <div class="diag-item"><span class="lbl">CAN1 RX</span><span class="v-acc">GPIO 6</span></div>
        <div class="diag-item"><span class="lbl">CAN2 CS</span><span class="v-acc" id="can-cs">GPIO 10</span></div>
        <div class="diag-item"><span class="lbl">SPI SCK</span><span class="v-acc" id="can-sck">GPIO 12</span></div>
        <div class="diag-item"><span class="lbl">SPI MISO</span><span class="v-acc" id="can-miso">GPIO 13</span></div>
        <div class="diag-item"><span class="lbl">SPI MOSI</span><span class="v-acc" id="can-mosi">GPIO 11</span></div>
        <div class="diag-item"><span class="lbl">MCP RST</span><span class="v-acc" id="can-rst">GPIO 9</span></div>
      </div>
    </div>
  </aside>

  <section class="diag-main-panel">
    <div class="card">
      <div class="cockpit-status-bar">
        <div class="cockpit-title"><small>工程模式</small><h2>错误优先的实时帧</h2></div>
        <div class="cockpit-chip-row"><span class="cockpit-chip ok">实时</span><span class="cockpit-chip warn">错误优先</span><span class="cockpit-chip">DBC 可切换</span></div>
      </div>
      <div class="diag-toolbar">
        <input class="inp" id="sniff-filter" placeholder="帧筛选：输入 ID、名称、TXERR 或 mux...">
        <button class="btn btn-sm btn-outline" id="sniff-pause" onclick="toggleSniffPause()">暂停</button>
      </div>
      <div class="sub-tabs">
        <div class="sub-tab active" onclick="switchCanTab('sniffer')">实时帧</div>
        <div class="sub-tab" onclick="switchCanTab('recorder')">记录器</div>
        <div class="sub-tab" onclick="switchCanTab('controller')">控制器</div>
        <div class="sub-tab" onclick="switchCanTab('debug')">调试</div>
      </div>
```

Then keep the existing tab bodies below this section. Close the new `<section>` and `<div class="diag-shell">` after the existing `can-debug` block.

- [ ] **Step 2: Update the Sniffer card markup but keep IDs**

Inside `id="can-sniffer"`, replace the outer sniffer card with:

```html
<div id="can-sniffer">
  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px">
      <span style="font-weight:800">实时帧</span>
      <span style="color:var(--tx3);font-size:11px" id="sniff-count">0 frames</span>
    </div>
    <div class="diag-table-shell">
      <table class="tbl">
        <thead><tr><th>ID</th><th>Dir</th><th>数据</th><th>Age</th></tr></thead>
        <tbody id="sniff-rows"></tbody>
      </table>
    </div>
  </div>
</div>
```

Do not duplicate `id="sniff-filter"` or `id="sniff-pause"`; those moved to the toolbar.

- [ ] **Step 3: Update recorder/controller/debug labels to Chinese**

Within the existing blocks, change visible labels only:

```html
<div class="card-title">CAN 记录器</div>
<button class="btn btn-sm" id="rec-start" onclick="startRec()">开始记录</button>
<button class="btn btn-sm btn-outline" id="rec-stop" onclick="stopRec()" disabled>停止</button>
<button class="btn btn-sm btn-outline" id="rec-dl" onclick="downloadRec()" disabled>下载 CSV</button>
```

```html
<div class="card-title">CAN 控制器状态</div>
```

```html
<div class="card-title">调试</div>
<div class="setting-name">调试日志</div>
<div class="card-title">最近写入检查</div>
```

- [ ] **Step 4: Run CAN diagnostics contract test**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'can_diagnostics_structure or destructive_buttons or post_helper' -q
```

Expected: PASS.

- [ ] **Step 5: Commit CAN diagnostics redesign**

```bash
git add include/web/mcp2515_dashboard_ui.src.h
git commit -m "feat: redesign CAN tools as diagnostics mode"
```

---

### Task 8: Update status polling to populate new diagnostic summary IDs

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:1864-2200` status polling functions
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add safe population calls after existing status fields update**

In the existing `/status` polling function, after it updates `s-rx`, `s-txerr`, `s-can2`, or equivalent status values, add:

```javascript
setText('diag-rxtx',((d.rxCount||d.rx||0)+' / '+(d.txCount||d.tx||0)));
setText('diag-txerr',(d.txErr||d.txerr||d.can2TxErr||0));
setText('diag-eflg',(d.can2Eflg!==undefined?('0x'+Number(d.can2Eflg).toString(16).padStart(2,'0').toUpperCase()):'0x00'));
```

If the existing status object uses different exact field names, keep the existing field names and preserve the same fallback behavior.

- [ ] **Step 2: Update last-write check population**

Where `lw-injected`, `lw-bus`, `lw-match`, or `lw-age` are populated, add:

```javascript
var lwMatchText=($('lw-match')&&$('lw-match').textContent)?$('lw-match').textContent:'--';
setText('diag-last-write',lwMatchText);
```

- [ ] **Step 3: Add contract tokens for new JS population**

In `test_atlas_redesign_can_diagnostics_structure_is_present`, add these required tokens:

```python
            "diag-rxtx",
            "diag-eflg",
            "diag-txerr",
            "diag-last-write",
```

- [ ] **Step 4: Run CAN diagnostics tests**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'can_diagnostics_structure' -q
```

Expected: PASS.

- [ ] **Step 5: Commit diagnostic summary wiring**

```bash
git add include/web/mcp2515_dashboard_ui.src.h test/test_dashboard_api_contract.py
git commit -m "feat: populate diagnostics summary rail"
```

---

### Task 9: Regenerate minified dashboard header and run contract suite

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Regenerate dashboard header**

Run from repo root:

```bash
python3 scripts/minify_dashboard.py
```

Expected: command exits 0 and rewrites `include/web/mcp2515_dashboard_ui.h`.

- [ ] **Step 2: Verify generated header is current**

Run:

```bash
python3 scripts/minify_dashboard.py --check
```

Expected: PASS / exit 0. If it reports drift, rerun Step 1 and repeat Step 2.

- [ ] **Step 3: Run the full dashboard contract test file**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py -q
```

Expected: all tests PASS. Current baseline around this project may be ~99 tests; use the actual output as truth.

- [ ] **Step 4: Commit generated header and tests**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h test/test_dashboard_api_contract.py
git commit -m "feat: regenerate redesigned dashboard UI"
```

---

### Task 10: Preview in simulator and verify desktop/mobile manually

**Files:**
- No source changes expected unless verification reveals issues.
- Uses: `scripts/webui_simulator.py`

- [ ] **Step 1: Start the Web UI simulator**

Run:

```bash
python3 scripts/webui_simulator.py --host 127.0.0.1 --port 8765
```

If the script does not support flags, run:

```bash
python3 scripts/webui_simulator.py
```

Expected: local HTTP server starts and serves the dashboard source from `include/web/mcp2515_dashboard_ui.src.h`.

- [ ] **Step 2: Open desktop viewport**

Use Playwright or a browser to open:

```text
http://127.0.0.1:8765/
```

Verify:

- The default page is `驾驶状态`.
- The first screen shows `驾驶状态中心`.
- No default-visible `破解模式`, `防封`, `Hold to Enable`, `Cockpit Home`, `Remote Card`, or `Engineering Mode` appears.
- `FSD 注入` main state is visually dominant.
- `硬件模式`, `驾驶风格`, `速度策略`, `CAN1`, `CAN2`, `帧率`, `芯片温度` are visible.

- [ ] **Step 3: Verify mobile viewport**

Set viewport to iPhone-like size:

```text
390 x 844
```

Verify:

- No horizontal overflow.
- `现场遥控` appears on the overview page.
- Main action says `长按启用` or `长按关闭`.
- Bottom tabs remain usable.
- Dense diagnostic tables are not shown on the mobile home surface.

- [ ] **Step 4: Verify CAN diagnostics viewport**

Navigate to `CAN 诊断`.

Verify:

- Page title is `CAN 诊断`.
- `工程模式`, `错误优先`, `帧筛选`, `实时帧`, `记录器`, `控制器`, `调试` are visible.
- Existing sub-tabs switch without JavaScript errors.
- Typing in `帧筛选` still filters rows or at least does not break the existing sniffer flow.

- [ ] **Step 5: Check browser console**

Expected: 0 JavaScript errors. A missing favicon warning is acceptable only if no real JS/runtime error appears.

- [ ] **Step 6: Stop the simulator**

Use `Ctrl-C` in the simulator terminal, or stop the background task if launched through Claude Code.

- [ ] **Step 7: Fix issues found in preview and repeat Steps 1-5**

If preview finds a real issue, fix `include/web/mcp2515_dashboard_ui.src.h`, regenerate `include/web/mcp2515_dashboard_ui.h`, rerun:

```bash
python3 scripts/minify_dashboard.py --check
python3 -m pytest test/test_dashboard_api_contract.py -q
```

Then repeat manual preview.

---

### Task 11: Run native/build verification

**Files:**
- No source changes expected unless tests fail.

- [ ] **Step 1: Run native dashboard tests**

Run:

```bash
pio test -e native_dashboard
```

Expected: PASS.

- [ ] **Step 2: Run related native safety tests**

Run:

```bash
pio test -e native_legacy
pio test -e native_helpers
```

Expected: PASS for both. These should not be affected by UI-only changes; failures mean the working tree has unrelated breakage or UI changes accidentally touched shared code.

- [ ] **Step 3: Build LILYGO firmware**

Run:

```bash
pio run -e lilygo_t2can_dual
```

Expected: `SUCCESS` and firmware builds with regenerated UI header.

- [ ] **Step 4: Commit verification-only fixes if needed**

If any fix was required:

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h test/test_dashboard_api_contract.py
git commit -m "fix: resolve dashboard redesign verification issues"
```

If no fix was required, do not create an empty commit.

---

### Task 12: Update documentation and final verification

**Files:**
- Modify: `docs/dashboard.md`
- Optional: update `CHANGELOG.md` only if project convention requires user-facing UI changes to be listed immediately.

- [ ] **Step 1: Update `docs/dashboard.md` overview section**

Replace the first dashboard feature section after the image with:

```markdown
## Dashboard Surfaces

The dashboard is organized around three usage surfaces:

- **驾驶状态**: the default driving-first home page. It shows the FSD injection state, CAN health, hardware mode, drive style, speed strategy, runtime, firmware version, and safety warnings without exposing dense debug tables.
- **现场遥控**: the mobile layout for hotspot use from a phone. It keeps the primary FSD injection action large, moves risky operations away from the first screen, and keeps status cards short enough for one-handed use.
- **CAN 诊断**: the engineering mode for live frames, recording, controller status, debug logs, and last-write checks. This is where high-density CAN data belongs.
```

Keep the rest of the document unless it contradicts the new UI.

- [ ] **Step 2: Add a note about precise Chinese copy**

Under `## Persistence Notes`, add:

```markdown
## UI Copy Notes

The default dashboard UI uses Chinese labels for all user-facing controls. Technical acronyms such as FSD, CAN, HW, NVS, OTA, TX/RX, EFLG, DBC, and GPIO are intentionally kept in English because they are standard protocol or firmware terms.
```

- [ ] **Step 3: Run doc-neutral tests again**

Run:

```bash
python3 scripts/minify_dashboard.py --check
python3 -m pytest test/test_dashboard_api_contract.py -q
pio test -e native_dashboard
pio run -e lilygo_t2can_dual
```

Expected: all commands PASS.

- [ ] **Step 4: Commit docs**

```bash
git add docs/dashboard.md
git commit -m "docs: describe redesigned dashboard surfaces"
```

- [ ] **Step 5: Record final status**

Run:

```bash
git status --short
```

Expected: either clean working tree, or only intentionally untracked design artifacts if the team chooses not to commit `designs/`.

---

## Self-review

### Spec coverage

- Approved hybrid direction is covered by Tasks 2, 3, and 7.
- Precise Chinese default UI copy is covered by Tasks 1, 4, 5, and 6.
- Existing backend API preservation is covered by Tasks 3, 7, 8, 9, and existing route/ID-preservation tests.
- Mobile-specific experience is covered by Tasks 2, 3, and 10.
- CAN engineering density is covered by Tasks 7, 8, and 10.
- Generated firmware header and build verification are covered by Tasks 9 and 11.
- Documentation is covered by Task 12.

### Placeholder scan

No unresolved placeholder markers or vague future-work instructions remain. Each task includes exact files, exact commands, expected outcomes, and code snippets for the required changes.

### Type/name consistency

The plan consistently uses existing IDs that current JavaScript/tests rely on: `fsd-toggle`, `fsd-label`, `ov-master-tgl`, `ov-fsd-tgl`, `fsd-boot-tgl`, `m-fsd`, `m-can`, `m-can2`, `m-hw`, `m-drive`, `m-speed`, `m-defense`, `sniff-filter`, `sniff-pause`, `sniff-rows`, `can-recorder`, `can-controller`, `can-debug`, `lw-*`. New helper IDs use the `diag-*` prefix and are introduced in Tasks 7-8 before being asserted in tests.

## Execution options

1. **Subagent-Driven (recommended)** — Use `superpowers:subagent-driven-development`. Dispatch a fresh subagent per task, review between tasks, and keep commits small.
2. **Inline Execution** — Use `superpowers:executing-plans`. Execute tasks in this session with checkpoints after major task groups.
