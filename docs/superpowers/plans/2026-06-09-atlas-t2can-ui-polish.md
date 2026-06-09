# Atlas T-2CAN UI Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the production Atlas T-2CAN dashboard visibly closer to the approved high-fidelity design while preserving all firmware behavior, routes, DOM ids, and safety constraints.

**Architecture:** Keep the dashboard as a single embedded HTML/CSS/JS source in `include/web/mcp2515_dashboard_ui.src.h`, regenerate `include/web/mcp2515_dashboard_ui.h`, and update only frontend markup/styles/tests/docs. Use contract tests to lock the visual structure and preserved ids before changing implementation.

**Tech Stack:** Vanilla HTML/CSS/JavaScript in C++ raw string, Python pytest contract tests, `scripts/minify_dashboard.py`, PlatformIO native tests and firmware build, Playwright/browser simulator for visual verification.

---

## File structure

- Modify: `include/web/mcp2515_dashboard_ui.src.h`
  - Owns all dashboard source HTML/CSS/JS.
  - Add visual-polish tokens, cockpit vehicle visual, mobile remote refinements, and CAN diagnostics visual hierarchy.

- Modify: `include/web/mcp2515_dashboard_ui.h`
  - Generated gzip header from the source UI.
  - Regenerate with `python3 scripts/minify_dashboard.py` after source changes.

- Modify: `test/test_dashboard_api_contract.py`
  - Add regression tests for polish markers, preserved ids, and design-alignment structures.

- Modify: `scripts/webui_simulator.py`
  - Add simulator response for `/gear_assist_status` so browser verification has a clean console.

- Modify: `docs/dashboard.md`
  - Update dashboard guide with the polish details and verification notes.

- Create/modify: screenshot artifacts only for local verification if needed; do not commit temporary Playwright screenshots unless explicitly requested.

---

### Task 1: Add UI polish contract tests

**Files:**
- Modify: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add tests for cockpit, mobile, diagnostics polish markers**

Append these methods inside `class DashboardApiContractTests(unittest.TestCase):`, near the existing Atlas redesign tests:

```python
    def test_atlas_polish_cockpit_visual_depth_is_present(self) -> None:
        """Cockpit home should feel like a car display without losing production ids."""
        overview_page = re.search(r'id="pg-overview".*?id="pg-hardware"', self.ui, re.S)
        self.assertIsNotNone(overview_page)
        body = overview_page.group(0)
        for token in [
            'class="cockpit-shell"',
            'class="cockpit-ambient"',
            'class="cockpit-vehicle-visual"',
            'class="vehicle-lane"',
            'class="vehicle-silhouette"',
            'class="cockpit-safety-strip"',
            'id="ov-drive-current"',
            'id="tb-fsd-local"',
            '驾驶状态中心',
            '车辆态势',
            '安全优先',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_atlas_polish_mobile_remote_app_surface_is_present(self) -> None:
        """Mobile home should use a native remote-card visual hierarchy."""
        for token in [
            'class="mobile-app-shell"',
            'class="mobile-primary-card"',
            'class="mobile-remote-grid"',
            'class="mobile-risk-note"',
            '现场遥控',
            '长按启用',
            '危险操作已收纳',
            '底部导航',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)

    def test_atlas_polish_can_engineering_console_is_present(self) -> None:
        """CAN diagnostics should expose a deliberate engineering-console hierarchy."""
        can_page = re.search(r'id="pg-can".*?id="pg-shift"', self.ui, re.S)
        self.assertIsNotNone(can_page)
        body = can_page.group(0)
        for token in [
            'class="diag-shell diag-console"',
            'class="diag-timeline"',
            'class="diag-kpi-rail"',
            'class="diag-table-shell"',
            'class="diag-priority-row"',
            '工程模式',
            '错误优先',
            '发现 ID',
            'LIVE TIMELINE',
            'DLC',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, body)

    def test_webui_simulator_serves_gear_assist_status(self) -> None:
        """Local browser verification should not produce a 404 for gear assist status."""
        simulator = (ROOT / "scripts" / "webui_simulator.py").read_text(encoding="utf-8")
        self.assertIn('"/gear_assist_status"', simulator)
        self.assertIn('gear_assist_status', simulator)
```

- [ ] **Step 2: Run tests and verify they fail**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'atlas_polish or gear_assist_status' -q
```

Expected: FAIL with missing tokens such as `cockpit-shell`, `mobile-app-shell`, `diag-console`, or `/gear_assist_status`.

- [ ] **Step 3: Commit failing tests**

```bash
git add test/test_dashboard_api_contract.py
git commit -m "test: capture atlas dashboard polish contract"
```

---

### Task 2: Add shared visual polish tokens and cockpit shell

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add CSS tokens**

In `include/web/mcp2515_dashboard_ui.src.h`, inside the existing `:root` block after `--cockpit-shadow`, add:

```css
  --cockpit-glow-purple: 0 0 42px rgba(124,58,237,0.22);
  --cockpit-glow-cyan: 0 0 34px rgba(56,189,248,0.18);
  --cockpit-glow-danger: 0 0 34px rgba(248,113,113,0.22);
  --cockpit-glow-ok: 0 0 34px rgba(74,222,128,0.20);
  --cockpit-card-blur: blur(22px) saturate(1.16);
```

- [ ] **Step 2: Add cockpit shell CSS**

Replace the current cockpit CSS block starting at `.cockpit-home {` through `.cockpit-warning { ... }` with this focused block:

```css
.cockpit-shell { position: relative; overflow: hidden; border: 1px solid var(--cockpit-stroke); border-radius: 30px; padding: 18px; background: radial-gradient(circle at 14% 0%, rgba(124,58,237,.20), transparent 34%), radial-gradient(circle at 88% 12%, rgba(56,189,248,.14), transparent 32%), linear-gradient(145deg, rgba(2,6,23,.84), rgba(8,13,24,.64)); box-shadow: var(--cockpit-shadow); }
.cockpit-ambient { position: absolute; inset: 0; pointer-events: none; background-image: linear-gradient(rgba(148,163,184,.045) 1px, transparent 1px), linear-gradient(90deg, rgba(148,163,184,.035) 1px, transparent 1px); background-size: 44px 44px; mask-image: radial-gradient(circle at center, black, transparent 74%); opacity: .82; }
.cockpit-home { position: relative; z-index: 1; display: grid; gap: 16px; }
.cockpit-status-bar { display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 10px; }
.cockpit-title small { display: block; color: var(--cockpit-muted); font-size: 11px; font-weight: 900; letter-spacing: .14em; text-transform: uppercase; }
.cockpit-title h2 { margin: 4px 0 0; font-size: 32px; line-height: .98; letter-spacing: -0.05em; }
.cockpit-chip-row { display: flex; flex-wrap: wrap; gap: 8px; justify-content: flex-end; }
.cockpit-chip { border: 1px solid var(--cockpit-stroke); border-radius: 999px; padding: 8px 12px; background: rgba(15,23,42,.62); color: var(--tx3); font-size: 12px; font-weight: 850; backdrop-filter: var(--cockpit-card-blur); }
.cockpit-chip.ok { color: var(--ok); border-color: rgba(74,222,128,.35); background: rgba(74,222,128,.09); }
.cockpit-chip.warn { color: var(--warn); border-color: rgba(251,191,36,.35); background: rgba(251,191,36,.09); }
.cockpit-grid { display: grid; grid-template-columns: minmax(0, 1.05fr) minmax(360px, .95fr); gap: 16px; }
.cockpit-home-main { display: grid; gap: 12px; }
.cockpit-primary { min-height: 360px; border: 1px solid var(--cockpit-stroke-strong); border-radius: var(--cockpit-radius-xl); padding: 24px; background: radial-gradient(circle at 86% 0%, rgba(124,58,237,.24), transparent 34%), linear-gradient(145deg, rgba(15,23,42,.90), rgba(8,13,24,.70)); box-shadow: var(--cockpit-shadow), var(--cockpit-glow-purple); display: grid; grid-template-rows: 1fr auto; gap: 18px; overflow: hidden; backdrop-filter: var(--cockpit-card-blur); }
.cockpit-state-panel { min-height: 230px; display: flex; flex-direction: column; align-items: center; justify-content: center; text-align: center; }
.cockpit-state-label { color: var(--cockpit-muted); font-size: 12px; font-weight: 950; letter-spacing: .16em; text-transform: uppercase; }
.cockpit-state { width: 100%; font-size: clamp(86px, 12vw, 152px); line-height: .78; font-weight: 950; letter-spacing: -.075em; margin: 18px 0 12px; color: var(--err); text-align: center; text-shadow: var(--cockpit-glow-danger); }
.cockpit-state.on { color: var(--ok); text-shadow: var(--cockpit-glow-ok); }
.cockpit-state-desc { color: var(--tx2); font-size: 14px; line-height: 1.55; max-width: 540px; margin: 0 auto; }
.cockpit-hold { width: 100%; min-height: 72px; margin-top: 18px; border: none; border-radius: 24px; background: linear-gradient(135deg, #991b1b, #ef4444); color: white; font-size: 16px; font-weight: 950; letter-spacing: .045em; cursor: pointer; box-shadow: 0 18px 42px rgba(239,68,68,.24); }
.cockpit-hold.on { background: linear-gradient(135deg, #047857, #22c55e); box-shadow: 0 18px 42px rgba(34,197,94,.22); }
.cockpit-metrics { display: grid; grid-template-columns: repeat(4, minmax(0,1fr)); gap: 10px; }
.cockpit-metric { min-height: 96px; border: 1px solid var(--cockpit-stroke); border-radius: var(--cockpit-radius-lg); background: rgba(15,23,42,.58); padding: 14px; backdrop-filter: var(--cockpit-card-blur); }
.cockpit-metric .lbl { color: var(--cockpit-muted); font-size: 10px; font-weight: 900; letter-spacing: .09em; text-transform: uppercase; }
.cockpit-metric .val { margin-top: 8px; color: var(--tx1); font-size: 24px; font-weight: 900; letter-spacing: -0.045em; overflow-wrap: anywhere; }
.cockpit-metric .hint { margin-top: 4px; color: var(--cockpit-dim); font-size: 11px; }
.cockpit-vehicle { border: 1px solid var(--cockpit-stroke); border-radius: var(--cockpit-radius-xl); background: linear-gradient(145deg, rgba(15,23,42,.60), rgba(8,13,24,.82)); padding: 20px; box-shadow: var(--cockpit-shadow), var(--cockpit-glow-cyan); backdrop-filter: var(--cockpit-card-blur); }
.cockpit-vehicle-visual { position: relative; min-height: 220px; display: grid; place-items: center; margin: 14px 0; border-radius: 24px; overflow: hidden; background: radial-gradient(circle at 50% 45%, rgba(56,189,248,.17), transparent 38%), linear-gradient(180deg, rgba(2,6,23,.30), rgba(2,6,23,.72)); }
.vehicle-lane { position: absolute; inset: 0; background: linear-gradient(90deg, transparent 18%, rgba(56,189,248,.20) 50%, transparent 82%); clip-path: polygon(43% 0, 57% 0, 82% 100%, 18% 100%); opacity: .72; }
.vehicle-silhouette { position: relative; width: 46%; min-width: 180px; height: 132px; border-radius: 42% 42% 28% 28%; background: linear-gradient(145deg, rgba(226,232,240,.94), rgba(100,116,139,.22)); box-shadow: 0 24px 70px rgba(56,189,248,.18); }
.vehicle-silhouette::before { content: ''; position: absolute; inset: 18px 32px 54px; border-radius: 999px 999px 18px 18px; background: linear-gradient(90deg, rgba(15,23,42,.86), rgba(56,189,248,.22)); }
.vehicle-silhouette::after { content: ''; position: absolute; left: 18%; right: 18%; bottom: 18px; height: 10px; border-radius: 999px; background: rgba(56,189,248,.72); box-shadow: 0 0 24px rgba(56,189,248,.45); }
.cockpit-mode-grid { display: grid; grid-template-columns: repeat(3, minmax(0,1fr)); gap: 10px; margin-top: 14px; }
.cockpit-mode { border: 1px solid var(--cockpit-stroke); border-radius: 18px; background: rgba(15,23,42,.55); padding: 14px; }
.cockpit-mode.active { border-color: rgba(124,58,237,.58); background: rgba(124,58,237,.16); }
.cockpit-mode strong { display: block; margin-top: 5px; font-size: 16px; }
.cockpit-mode span { display: block; margin-top: 4px; color: var(--tx3); font-size: 11px; line-height: 1.35; }
.cockpit-safety-strip, .cockpit-warning { border: 1px solid rgba(251,191,36,.35); border-radius: 18px; background: rgba(251,191,36,.08); padding: 14px; color: var(--tx2); font-size: 13px; line-height: 1.45; }
```

- [ ] **Step 3: Wrap overview markup in shell**

Inside `pg-overview`, replace:

```html
        <div class="cockpit-home">
```

with:

```html
        <div class="cockpit-shell">
          <div class="cockpit-ambient"></div>
          <div class="cockpit-home">
```

Add the matching closing `</div>` immediately after the existing cockpit home block ends and before the legacy lower production sections begin.

- [ ] **Step 4: Run focused test**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'atlas_polish_cockpit' -q
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h test/test_dashboard_api_contract.py
git commit -m "feat: deepen atlas cockpit visual shell"
```

---

### Task 3: Add cockpit vehicle visual and safety strip

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add vehicle visual markup**

Inside the existing `<div class="cockpit-vehicle">`, after the header/status bar and before the first `cockpit-mode-grid`, insert:

```html
            <div class="cockpit-vehicle-visual" aria-label="车辆态势视觉">
              <div class="vehicle-lane"></div>
              <div class="vehicle-silhouette"></div>
            </div>
```

- [ ] **Step 2: Rename right panel title copy**

Change the right panel title from:

```html
<h2>现场状态</h2>
```

to:

```html
<h2>车辆态势</h2>
```

- [ ] **Step 3: Add safety strip markup**

Replace the existing warning line:

```html
            <div class="cockpit-warning" id="m-alert">暂无异常。实验功能请在停车和安全环境下验证。</div>
```

with:

```html
            <div class="cockpit-safety-strip" id="m-alert">安全优先：暂无异常。实验功能请在停车和安全环境下验证。</div>
```

- [ ] **Step 4: Run focused test**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'atlas_polish_cockpit' -q
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h
git commit -m "feat: add cockpit vehicle visual hierarchy"
```

---

### Task 4: Polish mobile remote surface

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Replace mobile CSS block**

Replace the current mobile remote CSS lines `.mobile-remote { display: none; }` through `.mobile-remote-card .val { ... }` with:

```css
.mobile-remote { display: none; }
.mobile-app-shell { border-radius: 28px; padding: 16px; background: linear-gradient(180deg, #f8fafc, #e9eef7); color: #111827; box-shadow: 0 18px 50px rgba(0,0,0,.22); }
.mobile-app-shell .cockpit-title small { color: #64748b; }
.mobile-app-shell .cockpit-title h2 { color: #0f172a; }
.mobile-primary-card { margin-top: 14px; border-radius: 30px; padding: 22px; color: white; background: radial-gradient(circle at 86% 0%, rgba(56,189,248,.34), transparent 34%), linear-gradient(145deg, #111827, #312e81); box-shadow: 0 18px 42px rgba(49,46,129,.28); }
.mobile-primary-card .cockpit-state-label { color: #c4b5fd; }
.mobile-primary-card .cockpit-state { font-size: 74px; color: #ffb4bd; text-shadow: 0 0 24px rgba(255,180,189,.24); }
.mobile-primary-card .cockpit-state.on { color: #adffc8; text-shadow: 0 0 24px rgba(173,255,200,.26); }
.mobile-primary-card .cockpit-state-desc { color: #cbd5e1; }
.mobile-primary-card .cockpit-hold { min-height: 58px; border-radius: 22px; }
.mobile-remote-grid { display: grid; grid-template-columns: repeat(2, minmax(0,1fr)); gap: 10px; margin-top: 12px; }
.mobile-remote-card { background: white; border: 1px solid #e2e8f0; border-radius: 22px; padding: 14px; min-height: 86px; }
.mobile-remote-card .lbl { color: #64748b; font-size: 10px; font-weight: 900; letter-spacing: .08em; text-transform: uppercase; }
.mobile-remote-card .val { margin-top: 7px; color: #0f172a; font-size: 21px; font-weight: 900; overflow-wrap: anywhere; }
.mobile-risk-note { margin-top: 12px; border-radius: 22px; padding: 14px; background: #fff7ed; border: 1px solid #fed7aa; color: #9a3412; font-size: 13px; line-height: 1.45; }
```

- [ ] **Step 2: Wrap mobile remote markup in app shell**

Replace:

```html
      <div class="mobile-remote">
```

with:

```html
      <div class="mobile-remote mobile-app-shell">
```

- [ ] **Step 3: Mark primary mobile card**

Replace the mobile primary card opening:

```html
        <div class="cockpit-primary">
```

inside `mobile-remote` with:

```html
        <div class="cockpit-primary mobile-primary-card">
```

- [ ] **Step 4: Add risk note**

After the mobile remote grid, add:

```html
        <div class="mobile-risk-note">危险操作已收纳到底部导航的“更多”和对应二级页面；首页只保留现场安全遥控。</div>
```

- [ ] **Step 5: Tune mobile bottom tab labels only if current labels differ**

Keep existing page mapping. Ensure the visible mobile bottom tabs include concise Chinese labels:

```html
状态
模式
速度
网络
更多
```

Do not change the `data-page` attributes.

- [ ] **Step 6: Run focused test**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'atlas_polish_mobile' -q
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h
git commit -m "feat: polish atlas mobile remote surface"
```

---

### Task 5: Polish CAN diagnostics engineering console

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add diagnostics CSS**

After the existing diagnostics CSS block, add or replace with:

```css
.diag-console { align-items: start; }
.diag-kpi-rail { display: grid; gap: 10px; }
.diag-kpi-rail .diag-kpi { min-height: 92px; }
.diag-table-shell { border: 1px solid var(--cockpit-stroke); border-radius: 20px; overflow: hidden; background: rgba(2,6,23,.42); }
.diag-priority-row { display: grid; grid-template-columns: 96px 62px 54px minmax(220px, 1fr) 76px; gap: 10px; align-items: center; padding: 10px 12px; border-bottom: 1px solid rgba(148,163,184,.11); font-family: 'SF Mono', 'Courier New', monospace; font-size: 12px; }
.diag-priority-row.header { background: rgba(15,23,42,.86); color: var(--tx3); font-family: -apple-system, 'SF Pro Text', 'Helvetica Neue', sans-serif; font-size: 10px; font-weight: 900; letter-spacing: .09em; text-transform: uppercase; }
.diag-priority-row.warn { background: rgba(251,191,36,.08); }
.diag-priority-row.err { background: rgba(248,113,113,.09); }
.diag-timeline { margin-top: 14px; border: 1px solid var(--cockpit-stroke); border-radius: 18px; padding: 14px; background: rgba(15,23,42,.48); }
.diag-timeline .label { color: var(--cockpit-muted); font-size: 10px; font-weight: 900; letter-spacing: .10em; text-transform: uppercase; }
.diag-bars { display: grid; gap: 8px; margin-top: 10px; }
.diag-bar { height: 9px; border-radius: 999px; overflow: hidden; background: rgba(15,23,42,.90); }
.diag-bar span { display: block; height: 100%; border-radius: inherit; background: linear-gradient(90deg, var(--accent), var(--info)); }
```

- [ ] **Step 2: Add diag-console class**

Replace:

```html
<div class="diag-shell">
```

inside `pg-can` with:

```html
<div class="diag-shell diag-console">
```

- [ ] **Step 3: Add timeline card to KPI rail**

Inside `diag-kpi-rail`, after the existing KPI cards, add:

```html
  <div class="diag-timeline">
    <div class="label">LIVE TIMELINE</div>
    <div class="diag-bars">
      <div class="diag-bar"><span style="width:82%"></span></div>
      <div class="diag-bar"><span style="width:54%"></span></div>
      <div class="diag-bar"><span style="width:28%"></span></div>
    </div>
  </div>
```

- [ ] **Step 4: Add discovered ID KPI if missing**

If `发现 ID` is missing from the KPI rail, add:

```html
  <div class="diag-kpi"><small>发现 ID</small><b id="diag-id-count">--</b></div>
```

- [ ] **Step 5: Add table shell marker and DLC header**

Wrap the live rows container area in:

```html
<div class="diag-table-shell">
  <div class="diag-priority-row header"><span>ID</span><span>DIR</span><span>DLC</span><span>DATA / EVENT</span><span>AGE</span></div>
  <div id="sniff-rows"></div>
</div>
```

Keep `id="sniff-rows"` exactly once.

- [ ] **Step 6: Update render function for row class**

In the JavaScript function that renders sniffer rows into `sniff-rows`, ensure each row uses `diag-priority-row`. Use `DLC` value from frame data when available, otherwise display `8` for existing 8-byte rows and `--` for events.

The row template should contain this shape:

```javascript
return '<div class="diag-priority-row '+rowCls+'"><span>'+id+'</span><span>'+dir+'</span><span>'+dlc+'</span><span>'+data+'</span><span>'+age+'</span></div>';
```

- [ ] **Step 7: Run focused test**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'atlas_polish_can' -q
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h
git commit -m "feat: polish atlas can diagnostics console"
```

---

### Task 6: Fix simulator console 404

**Files:**
- Modify: `scripts/webui_simulator.py`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add simulator method**

Inside `class SimState`, add:

```python
    def gear_assist_status(self) -> dict[str, Any]:
        return {
            "ok": True,
            "enabled": False,
            "armed": False,
            "state": "standby",
            "lastAction": "none",
            "reason": "simulator",
        }
```

- [ ] **Step 2: Add GET route**

Inside the HTTP GET handler route map, add:

```python
            "/gear_assist_status": self.state.gear_assist_status(),
```

- [ ] **Step 3: Run focused test**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py \
  -k 'gear_assist_status' -q
```

Expected: PASS.

- [ ] **Step 4: Commit**

```bash
git add scripts/webui_simulator.py test/test_dashboard_api_contract.py
git commit -m "test: serve gear assist status in dashboard simulator"
```

---

### Task 7: Regenerate dashboard header and update docs

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.h`
- Modify: `docs/dashboard.md`

- [ ] **Step 1: Regenerate generated UI header**

Run:

```bash
python3 scripts/minify_dashboard.py
```

Expected: command completes and updates `include/web/mcp2515_dashboard_ui.h`.

- [ ] **Step 2: Update dashboard docs**

In `docs/dashboard.md`, update the `Dashboard Surfaces` section to mention:

```markdown
- **驾驶状态 / 驾驶状态中心**: now uses a cockpit shell, large FSD state, vehicle attitude visual, safety strip, and grouped runtime metrics.
- **现场遥控**: uses a native-app-style mobile card with one primary long-press FSD action and secondary risky actions kept out of the first screen.
- **CAN 诊断**: uses an engineering-console hierarchy with KPI rail, frame filter, error-priority table, live timeline, recorder, controller, and debug surfaces.
```

- [ ] **Step 3: Commit generated header and docs**

```bash
git add include/web/mcp2515_dashboard_ui.h docs/dashboard.md
git commit -m "docs: describe atlas dashboard polish surfaces"
```

---

### Task 8: Full verification and visual check

**Files:**
- No code changes unless verification finds failures.

- [ ] **Step 1: Run minify check**

```bash
python3 scripts/minify_dashboard.py --check
```

Expected: PASS / no diff required.

- [ ] **Step 2: Run dashboard contract tests**

```bash
python3 -m pytest test/test_dashboard_api_contract.py -q
```

Expected: all tests pass.

- [ ] **Step 3: Run native dashboard tests**

```bash
pio test -e native_dashboard
```

Expected: all tests pass.

- [ ] **Step 4: Build firmware**

```bash
pio run -e lilygo_t2can_dual
```

Expected: SUCCESS.

- [ ] **Step 5: Start simulator for screenshots**

```bash
python3 scripts/webui_simulator.py --host 127.0.0.1 --port 61734
```

Expected: dashboard available at `http://127.0.0.1:61734/`.

- [ ] **Step 6: Capture browser verification surfaces**

Use Playwright or browser manually to inspect:

- Desktop cockpit home at 1440x900.
- Mobile remote at 390x844.
- Desktop CAN diagnostics at 1440x900.

Expected:

- Cockpit shows shell, ambient grid, dominant FSD state, vehicle visual, and safety strip.
- Mobile shows native-card surface, low information density, bottom tabs, and risk note.
- CAN diagnostics shows engineering console, KPI rail, timeline, filter, tabs, and DLC table header.

- [ ] **Step 7: Check browser console**

Expected: no console errors from missing simulator endpoints.

- [ ] **Step 8: Final commit if verification required fixes**

If any verification fix was needed:

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h scripts/webui_simulator.py docs/dashboard.md test/test_dashboard_api_contract.py
git commit -m "fix: complete atlas dashboard polish verification"
```

If no fixes were needed, do not create an empty commit.
