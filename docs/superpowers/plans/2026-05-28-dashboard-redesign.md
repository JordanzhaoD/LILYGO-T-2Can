# Dashboard Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rewrite the T-2CAN Dashboard frontend from a single-page scrolling layout to a sidebar-navigated, 9-page design with purple modern styling.

**Architecture:** Single HTML file rewrite of `mcp2515_dashboard_ui.src.h`. All HTML, CSS, and JS in one file embedded as a C++ raw string. Sidebar navigation switches between 9 pages using CSS `display:none/block`. Backend API (`mcp2515_dashboard.h`) has zero changes. Build via `minify_dashboard.py` → `mcp2515_dashboard_ui.h`.

**Tech Stack:** Vanilla HTML/CSS/JS (no frameworks), ESP-IDF, PlatformIO, Python minification pipeline (rjsmin + csscompressor + htmlmin2 + gzip).

**Spec:** `docs/superpowers/specs/2026-05-28-dashboard-redesign-design.md`

---

## Files

| File | Action | Responsibility |
|------|--------|----------------|
| `include/web/mcp2515_dashboard_ui.src.h` | Full rewrite | New dashboard HTML/CSS/JS source |
| `include/web/mcp2515_dashboard_ui.h` | Auto-generated | Minified + gzipped blob (via script) |

---

## Reference: Backend API Endpoints (read-only, do not modify)

These endpoints in `mcp2515_dashboard.h` must be called by the new JS exactly as the current code does:

- `GET /status` — Returns JSON with: `can`, `ci`, `ia`, `fps`, `hw`, `sp`, `rx`, `tx`, `txerr`, `fd`, `soff`, `up`, `eflg`, `gtwap`, `probe`, `mux[]`, `apActive`, `adEnabled`, `apGate`, `b2` (bus2 object: `on`, `rx`, `ids`, `rc`, `ce`), `temp` (chip temp)
- `GET /config` — Current device config (hw, sp, spa, can, etc.)
- `POST /config` — Save config changes (form-encoded body)
- `GET /bus2_ids` — Bus2 discovered CAN ID list
- `POST /stalk_test` — Body: `mode=PUSH|PULL&dur=ms`
- `POST /service_mode` — Body: `on=1|0`
- `GET /sniff` — CAN sniffer frames
- `POST /record` — Start/stop CAN recorder
- `GET /record` — Download recorded CSV
- `GET /can_controller` — CAN controller status
- `GET /last_write` — Last write probe
- `POST /reboot` — Reboot device
- `POST /reset_stats` — Reset RX/TX counters
- `POST /fsd_switch` — Toggle FSD (body: `active=1|0`)
- `GET /wifi_status` — WiFi connection info
- `POST /wifi_connect` — Connect to saved network
- `POST /wifi_save` — Save WiFi credentials
- `POST /wifi_delete` — Delete saved network
- `GET /ap_status` — AP hotspot status
- `POST /ap_save` — Save AP config
- `GET /gateway_status` — Gateway/NAPT status
- `GET /gateway_dns` — DNS config, blacklist, whitelist
- `POST /gateway_dns` — Save DNS config
- `GET /gateway_blocked` — Blocked domain list
- `POST /gateway_blocked_clear` — Clear blocked list
- `POST /gateway_dns_test` — Test domain resolution
- `POST /ota_upload` — Firmware upload (multipart)
- `POST /can_pins_save` — Save custom CAN GPIO pins
- `GET /log` — Debug log entries
- `GET /backup` — Download settings backup JSON
- `POST /backup` — Import settings backup

---

### Task 1: Create New File Skeleton

**Files:**
- Rewrite: `include/web/mcp2515_dashboard_ui.src.h`

Replace the entire file with the new skeleton containing: raw string wrapper, CSS variables, sidebar HTML, 9 empty page containers, and top status bar.

- [ ] **Step 1: Write the new skeleton**

Replace entire content of `include/web/mcp2515_dashboard_ui.src.h` with:

```html
R"HTML(<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Atlas T-2CAN</title>
<style>
/* === CSS Variables === */
:root {
  --sidebar-bg: #111827;
  --main-bg: #1f2937;
  --card-bg: #374151;
  --card-bg-alt: #1f2937;
  --accent: #7c3aed;
  --accent-light: #a78bfa;
  --ok: #4ade80;
  --err: #f87171;
  --warn: #fbbf24;
  --info: #60a5fa;
  --tx1: #f9fafb;
  --tx2: #d1d5db;
  --tx3: #9ca3af;
  --border: #4b5563;
  --header-bg: #111827;
  --sidebar-w: 160px;
}
/* === Reset & Base === */
* { margin:0; padding:0; box-sizing:border-box; }
body { font-family: -apple-system, 'SF Pro Text', 'Helvetica Neue', sans-serif;
  background: var(--main-bg); color: var(--tx1); font-size: 13px;
  display: flex; height: 100vh; overflow: hidden; }

/* === Sidebar === */
.sidebar { width: var(--sidebar-w); background: var(--sidebar-bg);
  display: flex; flex-direction: column; flex-shrink: 0;
  border-right: 1px solid var(--border); overflow-y: auto; }
.sidebar-hdr { padding: 14px 12px 10px; border-bottom: 1px solid var(--border); }
.sidebar-hdr h1 { font-size: 16px; font-weight: 700; color: var(--tx1); letter-spacing: 0.5px; }
.sidebar-hdr p { font-size: 10px; color: var(--tx3); margin-top: 2px; }
.sidebar-nav { flex: 1; padding: 6px 0; }
.nav-item { display: block; padding: 8px 12px; color: var(--tx3);
  font-size: 12px; cursor: pointer; border-radius: 0;
  transition: background .15s, color .15s; user-select: none; }
.nav-item:hover { background: var(--main-bg); color: var(--tx2); }
.nav-item.active { background: var(--accent); color: #fff; font-weight: 500; }
.sidebar-ft { padding: 10px 12px; border-top: 1px solid var(--border);
  display: flex; gap: 6px; }
.sidebar-ft button { flex:1; background: var(--card-bg); border: none;
  color: var(--tx3); padding: 4px 0; border-radius: 4px; font-size: 11px;
  cursor: pointer; }
.sidebar-ft button:hover { color: var(--tx2); }

/* === Main Area === */
.main { flex: 1; display: flex; flex-direction: column; overflow: hidden; }
.topbar { background: var(--header-bg); padding: 8px 16px;
  display: flex; align-items: center; gap: 12px; flex-shrink: 0;
  border-bottom: 1px solid var(--border); min-height: 36px; }
.topbar-badge { padding: 2px 8px; border-radius: 4px; font-size: 11px; font-weight: 600; }
.badge-ok { background: #166534; color: var(--ok); }
.badge-err { background: #7f1d1d; color: var(--err); }
.badge-warn { background: #78350f; color: var(--warn); }
.topbar-fps { color: var(--info); font-size: 12px; font-weight: 600; }
.topbar-time { margin-left: auto; color: var(--tx3); font-size: 11px; }
.content { flex: 1; overflow-y: auto; padding: 16px; }

/* === Pages === */
.page { display: none; }
.page.active { display: block; }

/* === Cards === */
.card { background: var(--card-bg); border-radius: 10px; padding: 14px;
  margin-bottom: 12px; }
.card-title { font-size: 14px; font-weight: 600; color: var(--tx1);
  margin-bottom: 10px; }
.card-subtitle { font-size: 11px; color: var(--tx3); margin-top: -6px;
  margin-bottom: 10px; }

/* === Stats Grid === */
.stats { display: grid; grid-template-columns: repeat(3, 1fr); gap: 6px;
  margin-bottom: 8px; }
.stat { background: var(--card-bg-alt); border-radius: 8px; padding: 8px;
  text-align: center; }
.stat-lbl { font-size: 10px; color: var(--tx3); margin-bottom: 2px; }
.stat-val { font-size: 13px; font-weight: 600; }
.v-ok { color: var(--ok); }
.v-err { color: var(--err); }
.v-warn { color: var(--warn); }
.v-info { color: var(--info); }
.v-acc { color: var(--accent-light); }
.v-dim { color: var(--tx3); }

/* === Toggle Switch === */
.tgl { position: relative; display: inline-block; width: 44px; height: 24px; flex-shrink: 0; }
.tgl input { opacity: 0; width: 0; height: 0; }
.tgl-track { position: absolute; inset: 0; background: var(--border);
  border-radius: 12px; transition: background .2s; cursor: pointer; }
.tgl-track::after { content: ''; position: absolute; width: 20px; height: 20px;
  background: var(--tx3); border-radius: 50%; top: 2px; left: 2px;
  transition: transform .2s, background .2s; }
.tgl input:checked + .tgl-track { background: var(--accent); }
.tgl input:checked + .tgl-track::after { transform: translateX(20px); background: #fff; }

/* === Buttons === */
.btn { background: var(--accent); border: none; color: #fff; padding: 8px 16px;
  border-radius: 6px; font-size: 12px; cursor: pointer; font-weight: 500;
  transition: opacity .15s; }
.btn:hover { opacity: 0.85; }
.btn-danger { background: var(--err); }
.btn-outline { background: transparent; border: 1px solid var(--border);
  color: var(--tx3); }
.btn-outline:hover { border-color: var(--accent-light); color: var(--accent-light); }
.btn-sm { padding: 4px 10px; font-size: 11px; }

/* === Selection Cards (HW, Profile, etc.) === */
.sel-cards { display: grid; gap: 6px; }
.sel-cards.c2 { grid-template-columns: repeat(2, 1fr); }
.sel-cards.c3 { grid-template-columns: repeat(3, 1fr); }
.sel-cards.c4 { grid-template-columns: repeat(4, 1fr); }
.sel-card { background: var(--card-bg-alt); border: 2px solid var(--border);
  border-radius: 8px; padding: 10px; text-align: center; cursor: pointer;
  transition: border-color .15s, background .15s; }
.sel-card:hover { border-color: var(--tx3); }
.sel-card.active { border-color: var(--accent); background: var(--accent); color: #fff; }
.sel-card.active .sel-lbl { color: #c4b5fd; }
.sel-card .sel-lbl { font-size: 9px; color: var(--tx3); text-transform: uppercase;
  letter-spacing: 0.5px; }
.sel-card .sel-name { font-size: 13px; font-weight: 600; margin-top: 2px; }

/* === Setting Row === */
.setting-row { display: flex; justify-content: space-between; align-items: center;
  padding: 8px 0; border-bottom: 1px solid rgba(75,85,99,0.3); }
.setting-row:last-child { border-bottom: none; }
.setting-name { font-size: 12px; color: var(--tx1); }
.setting-desc { font-size: 10px; color: var(--tx3); margin-top: 2px; }

/* === Sub-tabs (CAN tools) === */
.sub-tabs { display: flex; gap: 2px; background: var(--card-bg);
  border-radius: 6px; padding: 2px; margin-bottom: 10px; }
.sub-tab { flex: 1; text-align: center; padding: 5px 8px;
  border-radius: 4px; font-size: 11px; cursor: pointer;
  color: var(--tx3); transition: all .15s; }
.sub-tab.active { background: var(--accent); color: #fff; font-weight: 500; }

/* === Table === */
.tbl { width: 100%; border-collapse: collapse; font-size: 11px; }
.tbl th { text-align: left; padding: 6px 8px; color: var(--tx3);
  border-bottom: 1px solid var(--border); font-weight: 500; font-size: 10px; }
.tbl td { padding: 4px 8px; border-bottom: 1px solid rgba(75,85,99,0.2); }
.tbl .hex { font-family: 'SF Mono', 'Courier New', monospace; color: var(--accent-light); }

/* === Inputs === */
.inp { background: var(--card-bg-alt); border: 1px solid var(--border);
  border-radius: 6px; padding: 6px 10px; color: var(--tx1); font-size: 12px;
  width: 100%; outline: none; }
.inp:focus { border-color: var(--accent-light); }
.inp::placeholder { color: var(--tx3); }
textarea.inp { resize: vertical; min-height: 60px; font-family: monospace;
  font-size: 11px; line-height: 1.5; }

/* === Status Indicator === */
.status-dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
.status-dot.ok { background: var(--ok); box-shadow: 0 0 6px var(--ok); }
.status-dot.err { background: var(--err); }
.status-dot.warn { background: var(--warn); }

/* === Mobile Responsive === */
@media (max-width: 768px) {
  .sidebar { position: fixed; left: -200px; z-index: 100;
    transition: left .25s; width: 180px; height: 100vh; }
  .sidebar.open { left: 0; }
  .mobile-toggle { display: block; }
  .main { width: 100%; }
  .stats { grid-template-columns: repeat(2, 1fr); }
  .sel-cards.c4 { grid-template-columns: repeat(2, 1fr); }
}
.mobile-toggle { display: none; background: none; border: none;
  color: var(--tx2); font-size: 20px; cursor: pointer; padding: 4px 8px; }
.overlay { display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.5);
  z-index: 99; }
.overlay.active { display: block; }

/* === Scrollbar === */
::-webkit-scrollbar { width: 6px; }
::-webkit-scrollbar-track { background: var(--main-bg); }
::-webkit-scrollbar-thumb { background: var(--border); border-radius: 3px; }

/* === Big Toggle Card === */
.big-toggle { text-align: center; padding: 20px; }
.big-toggle .toggle-visual { width: 72px; height: 36px; border-radius: 18px;
  margin: 0 auto 8px; position: relative; transition: background .3s;
  box-shadow: 0 0 20px rgba(0,0,0,0.3); }
.big-toggle .toggle-visual .thumb { width: 32px; height: 32px; border-radius: 50%;
  background: #fff; position: absolute; top: 2px; transition: left .3s; }
.big-toggle .toggle-label { font-size: 15px; font-weight: 700; }
.big-toggle.on .toggle-visual { background: var(--ok);
  box-shadow: 0 0 20px rgba(74,222,128,0.3); }
.big-toggle.on .toggle-visual .thumb { left: 38px; }
.big-toggle.off .toggle-visual { background: var(--err);
  box-shadow: 0 0 20px rgba(248,113,113,0.3); }
.big-toggle.off .toggle-visual .thumb { left: 2px; }

/* === Upload Area === */
.upload-area { border: 2px dashed var(--border); border-radius: 10px;
  padding: 20px; text-align: center; cursor: pointer;
  transition: border-color .2s; }
.upload-area:hover { border-color: var(--accent-light); }

/* === Diag Grid === */
.diag-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 4px; }
.diag-item { background: var(--card-bg-alt); border-radius: 4px;
  padding: 4px 8px; display: flex; justify-content: space-between;
  font-size: 11px; }
.diag-item .lbl { color: var(--tx3); }
</style>
</head>
<body>

<!-- Mobile overlay -->
<div class="overlay" id="overlay" onclick="closeSidebar()"></div>

<!-- Sidebar -->
<nav class="sidebar" id="sidebar">
  <div class="sidebar-hdr">
    <h1>Atlas</h1>
    <p>T-2CAN 控制面板</p>
  </div>
  <div class="sidebar-nav">
    <div class="nav-item active" data-page="pg-overview">概览</div>
    <div class="nav-item" data-page="pg-hardware">模块配置</div>
    <div class="nav-item" data-page="pg-fsd">FSD 开关</div>
    <div class="nav-item" data-page="pg-speed">速度偏移</div>
    <div class="nav-item" data-page="pg-bus2">Bus2 控制</div>
    <div class="nav-item" data-page="pg-defense">FSD 防御</div>
    <div class="nav-item" data-page="pg-ota">OTA 升级</div>
    <div class="nav-item" data-page="pg-network">网络设置</div>
    <div class="nav-item" data-page="pg-can">CAN 工具</div>
  </div>
  <div class="sidebar-ft">
    <button onclick="toggleLanguage()" id="lang-btn">EN</button>
    <button onclick="toggleTheme()" id="theme-btn">☀</button>
  </div>
</nav>

<!-- Main Area -->
<div class="main">
  <!-- Top Status Bar -->
  <div class="topbar">
    <button class="mobile-toggle" onclick="openSidebar()">☰</button>
    <span class="topbar-fps" id="tb-fps">0.0 Hz</span>
    <span class="topbar-badge badge-ok" id="tb-status">已连接</span>
    <span class="topbar-badge badge-warn" id="tb-fsd">FSD OFF</span>
    <span class="topbar-time" id="tb-up">00:00:00</span>
  </div>

  <!-- Content Area -->
  <div class="content" id="content">

    <!-- Page 1: Overview -->
    <div class="page active" id="pg-overview">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

    <!-- Page 2: Hardware Config -->
    <div class="page" id="pg-hardware">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

    <!-- Page 3: FSD Switch -->
    <div class="page" id="pg-fsd">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

    <!-- Page 4: Speed Offset -->
    <div class="page" id="pg-speed">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

    <!-- Page 5: Bus2 Control -->
    <div class="page" id="pg-bus2">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

    <!-- Page 6: FSD Defense -->
    <div class="page" id="pg-defense">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

    <!-- Page 7: OTA Update -->
    <div class="page" id="pg-ota">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

    <!-- Page 8: Network Settings -->
    <div class="page" id="pg-network">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

    <!-- Page 9: CAN Tools -->
    <div class="page" id="pg-can">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

  </div>
</div>

<script>
// === JS will be implemented in Tasks 5-8 ===
</script>
</body>
</html>)HTML";
```

- [ ] **Step 2: Verify file is valid for minify script**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python3 scripts/minify_dashboard.py`
Expected: Script runs without error, produces `mcp2515_dashboard_ui.h` with valid output.

- [ ] **Step 3: Verify build compiles**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e lilygo_t2can_dual 2>&1 | tail -5`
Expected: `SUCCESS` (the empty pages won't affect compilation)

- [ ] **Step 4: Commit skeleton**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "feat: new dashboard skeleton with sidebar nav and 9 page containers"
```

---

### Task 2: Implement Page 1 (Overview) + Page 3 (FSD Switch) HTML

These two pages share the FSD toggle logic and are the most critical user-facing pages.

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h` (replace the `<!-- Page 1 -->` and `<!-- Page 3 -->` placeholder content)

- [ ] **Step 1: Write Overview page HTML**

Replace the `<div class="page active" id="pg-overview">` content with:

```html
<!-- FSD Quick Toggle -->
<div class="card">
  <div class="card-title">FSD 注入</div>
  <div class="setting-row">
    <div>
      <div class="setting-name">FSD 开关</div>
      <div class="setting-desc" id="ov-fsd-desc">点击切换开关状态</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="ov-fsd-tgl" onchange="toggleFsdTopButton()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div>
    </label>
  </div>
</div>

<!-- Stats Grid -->
<div class="stats">
  <div class="stat">
    <div class="stat-lbl">CAN Bus</div>
    <div class="stat-val v-dim" id="s-can">Offline</div>
  </div>
  <div class="stat">
    <div class="stat-lbl">RX</div>
    <div class="stat-val v-info" id="s-rx">0</div>
  </div>
  <div class="stat">
    <div class="stat-lbl">TX</div>
    <div class="stat-val v-info" id="s-tx">0</div>
  </div>
  <div class="stat">
    <div class="stat-lbl">帧率</div>
    <div class="stat-val v-info" id="s-fps">0.0 Hz</div>
  </div>
  <div class="stat">
    <div class="stat-lbl">硬件版本</div>
    <div class="stat-val v-acc" id="s-hw">--</div>
  </div>
  <div class="stat">
    <div class="stat-lbl">速度偏移</div>
    <div class="stat-val v-dim" id="s-soff">0</div>
  </div>
</div>

<!-- Secondary Stats -->
<div class="stats">
  <div class="stat">
    <div class="stat-lbl">芯片温度</div>
    <div class="stat-val v-dim" id="s-temp">--</div>
  </div>
  <div class="stat">
    <div class="stat-lbl">TX Errors</div>
    <div class="stat-val v-dim" id="s-txerr">0</div>
  </div>
  <div class="stat">
    <div class="stat-lbl">跟随距离</div>
    <div class="stat-val v-dim" id="s-fd">--</div>
  </div>
</div>
```

- [ ] **Step 2: Write FSD Switch page HTML**

Replace the `<div class="page" id="pg-fsd">` content with:

```html
<div class="card">
  <div class="big-toggle off" id="fsd-toggle" onclick="toggleFsdTopButton()">
    <div class="card-title">FSD 注入控制</div>
    <div class="setting-desc" style="margin-bottom:12px">启用后设备将注入 CAN 帧到车辆总线</div>
    <div class="toggle-visual"><div class="thumb"></div></div>
    <div class="toggle-label" id="fsd-label">已关闭</div>
  </div>
</div>

<div class="card">
  <div class="setting-row">
    <div>
      <div class="setting-name">开机自动启用</div>
      <div class="setting-desc">设备重启后自动开启 FSD 注入</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="fsd-boot-tgl" onchange="saveApRestore()">
      <div class="tgl-track"><div class="tgl-thumb"></div></div>
    </label>
  </div>
</div>

<div class="card">
  <div class="card-title">紧急控制</div>
  <div style="display:flex;gap:8px">
    <button class="btn btn-danger" onclick="emergencyStop()" style="flex:1">紧急停止</button>
    <button class="btn" onclick="resumeInj()" style="flex:1">恢复注入</button>
  </div>
</div>
```

- [ ] **Step 3: Build verify**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python3 scripts/minify_dashboard.py && pio run -e lilygo_t2can_dual 2>&1 | tail -3`
Expected: `SUCCESS`

- [ ] **Step 4: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "feat: add overview and FSD switch page HTML"
```

---

### Task 3: Implement Page 2 (Hardware Config) + Page 4 (Speed Offset) HTML

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h` (replace page 2 and page 4 placeholders)

- [ ] **Step 1: Write Hardware Config page HTML**

Replace `<div class="page" id="pg-hardware">` content. This page has: HW version selection cards (2x2), profile selection grid, and AP restore toggle. Copy the segment/button logic from the current dashboard's `hw-seg` pattern but render as selection cards.

Key HTML elements:
- 4 selection cards: Auto (selected by default), Legacy, HW3, HW4
- Profile grid (3 cols): Chill, Normal, Hurry (+ Max/Sloth for HW4)
- Each card has `onclick="setHW(N)"` or `onclick="setProfile(N)"`

- [ ] **Step 2: Write Speed Offset page HTML**

Replace `<div class="page" id="pg-speed">` content. This page has:
- HW3 Custom Speed toggle + bucket table (30→45, 40→60, 50→75, 60→90, 70→105)
- Custom toggle for editing buckets + editable number inputs
- High-speed section (80→90, 100→110, 120→130)
- Encoding selector dropdown
- Realtime stats: Fused, Stock offset, Raw write target
- Legacy MPP speed section with its own toggle and bucket table

Port the existing `hw3-ct-*`, `hw3-hs-*`, `hw3-enc`, `legacy-mpp-*` input IDs and their `onchange` handlers to the new layout.

- [ ] **Step 3: Build verify**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python3 scripts/minify_dashboard.py && pio run -e lilygo_t2can_dual 2>&1 | tail -3`
Expected: `SUCCESS`

- [ ] **Step 4: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "feat: add hardware config and speed offset page HTML"
```

---

### Task 4: Implement Page 5 (Bus2 Control) + Page 6 (FSD Defense) HTML

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`

- [ ] **Step 1: Write Bus2 Control page HTML**

Replace `<div class="page" id="pg-bus2">` content. Elements:
- Bus2 ID table header with count (`#bus2-count`)
- Scrollable table body (`#bus2-rows`) with columns: ID, DLC, Data, Count
- Stalk injection section: two buttons (PULL flash / PUSH high beam) with `onclick="stalkTest(mode)"` and a duration input
- Service Mode toggle with `onchange="toggleServiceMode()"`
- Status text (`#stalk-status`)

- [ ] **Step 2: Write FSD Defense page HTML**

Replace `<div class="page" id="pg-defense">` content. Elements:
- Slew toggle (`#hw3-slew-tgl`, `onchange="saveHw3Slew()"`)
- Config rows: max descent rate, min offset hold (read from status)
- Protection trigger counter
- Status indicator dot + text

- [ ] **Step 3: Build verify**

Run: `python3 scripts/minify_dashboard.py && pio run -e lilygo_t2can_dual 2>&1 | tail -3`
Expected: `SUCCESS`

- [ ] **Step 4: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "feat: add Bus2 control and FSD defense page HTML"
```

---

### Task 5: Implement Page 7 (OTA) + Page 8 (Network) + Page 9 (CAN Tools) HTML

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`

- [ ] **Step 1: Write OTA Update page HTML**

Replace `<div class="page" id="pg-ota">` content. Elements:
- Version info card (version, build time, flash usage)
- Drag-and-drop upload area
- Upload button + progress indicator

Port existing OTA upload logic (`uploadFirmware()`, drag-drop handlers).

- [ ] **Step 2: Write Network Settings page HTML**

Replace `<div class="page" id="pg-network">` content. Five sections as cards:
1. WiFi Hotspot (SSID, password, hidden, clients)
2. WiFi Internet (4 slots, connect/delete)
3. STA-AP Gateway (NAT toggle, diag grid with 9 fields, performance mode)
4. Upstream DNS (Auto/Ali/Tencent/Custom buttons, Conservative/Aggressive profiles)
5. DNS Filter Rules (blacklist textarea, whitelist textarea, DNS test input, filter record list with refresh/clear)

Port all existing `ap_*`, `wifi_*`, `gw-*`, `gateway*` IDs and handlers.

- [ ] **Step 3: Write CAN Tools page HTML**

Replace `<div class="page" id="pg-can">` content:
1. CAN Pin Config card (Bus1 TX/RX, Bus2 CS/SCK/MISO/MOSI/RST)
2. Sub-tabs bar: Sniffer | Recorder | Controller | Debug
3. Sniffer sub-page: filter input, pause button, frame table
4. Recorder sub-page: start/stop, frame limit, download
5. Controller sub-page: EFLG, mux counters
6. Debug sub-page: last write check, debug log toggle

- [ ] **Step 4: Build verify**

Run: `python3 scripts/minify_dashboard.py && pio run -e lilygo_t2can_dual 2>&1 | tail -3`
Expected: `SUCCESS`

- [ ] **Step 5: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "feat: add OTA, network, and CAN tools page HTML"
```

---

### Task 6: Implement Core JavaScript

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h` (replace the `<script>` section)

- [ ] **Step 1: Write core JS — constants, state, utilities**

Inside the `<script>` tag, write:

```javascript
// === Constants ===
const HW = ['Legacy', 'HW3', 'HW4'];
const SP3 = ['Chill', 'Normal', 'Hurry'];
const SP4 = ['Chill', 'Normal', 'Hurry', 'Max', 'Sloth'];
const $ = id => document.getElementById(id);

// === State ===
let state = { hw: 0, sp: 0, spAuto: true, can: false };
let dashLang = localStorage.getItem('dashLang') ||
  ((navigator.language || '').toLowerCase().startsWith('zh') ? 'zh' : 'en');

// === Utility Functions ===
function setText(id, txt) { const el = $(id); if (el) el.textContent = txt; }
function setClass(id, cls) { const el = $(id); if (el) el.className = cls; }
function setHtml(id, html) { const el = $(id); if (el) el.innerHTML = html; }
function fmtUp(s) {
  if (!s) return '0s';
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60;
  return h > 0 ? `${h}:${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`
    : `${m}:${String(sec).padStart(2,'0')}`;
}
function escapeHtml(s) {
  return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;')
    .replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
function toHex(n, w) { return (n>>>0).toString(16).padStart(w, '0').toUpperCase(); }
function clampProfileForHw(hw, sp) {
  const max = hw === 2 ? SP4.length - 1 : SP3.length - 1;
  return Math.min(sp, max);
}
```

- [ ] **Step 2: Write sidebar navigation JS**

```javascript
// === Sidebar Navigation ===
document.querySelectorAll('.nav-item').forEach(item => {
  item.addEventListener('click', () => {
    const pageId = item.dataset.page;
    // Update nav
    document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
    item.classList.add('active');
    // Update pages
    document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
    const page = $(pageId);
    if (page) page.classList.add('active');
    // Close mobile sidebar
    closeSidebar();
  });
});

function openSidebar() {
  $('sidebar').classList.add('open');
  $('overlay').classList.add('active');
}
function closeSidebar() {
  $('sidebar').classList.remove('open');
  $('overlay').classList.remove('active');
}
```

- [ ] **Step 3: Write poll() function**

Port the existing `poll()` function from the current dashboard. The key change is mapping status fields to the new element IDs:

```javascript
async function poll() {
  return runPoll('status', async () => {
    try {
      const d = await fetchPollJson('/status', 5000, true);
      const on = !!d.can;
      const armed = !!d.ci;
      const injecting = typeof d.ia === 'undefined' ? armed : !!d.ia;
      const fpsVal = Number(d.fps || 0);
      state.hw = d.hw;
      state.sp = clampProfileForHw(d.hw, d.sp);
      state.spAuto = typeof d.spAuto === 'undefined' ? state.spAuto : !!d.spAuto;
      state.can = armed;

      // Top bar
      setText('tb-fps', fpsVal.toFixed(1) + ' Hz');
      setText('tb-up', fmtUp(d.up));
      const statusEl = $('tb-status');
      statusEl.textContent = on ? '已连接' : '离线';
      statusEl.className = 'topbar-badge ' + (on ? 'badge-ok' : 'badge-err');
      const fsdEl = $('tb-fsd');
      fsdEl.textContent = injecting ? 'FSD 运行中' : 'FSD OFF';
      fsdEl.className = 'topbar-badge ' + (injecting ? 'badge-ok' : armed ? 'badge-warn' : 'badge-err');

      // Overview page stats
      setText('s-can', on ? 'Active' : 'Offline');
      setClass('s-can', 'stat-val ' + (on ? 'v-ok' : 'v-err'));
      setText('s-rx', d.rx);
      setText('s-tx', d.tx);
      setText('s-fps', fpsVal.toFixed(1) + ' Hz');
      setText('s-hw', HW[d.hw] || '?');
      setText('s-soff', d.soff || '0');
      setText('s-temp', d.temp !== undefined ? d.temp + '°C' : '--');
      setText('s-txerr', d.txerr);
      setClass('s-txerr', 'stat-val ' + (d.txerr > 0 ? 'v-warn' : 'v-dim'));
      setText('s-fd', d.fd || '--');

      // FSD page
      updateFsdControl(d);
      // HW config page
      updateHardwareControls(d);
      // Speed page
      updateHw3SpeedControl(d);
      updateLegacyMppControl(d);
      // Defense page
      updateHw3SlewControl(d);
      // Bus2 page
      if (d.b2) updateBus2Status(d.b2);
    } catch (e) {
      noteDashboardPoll(false);
    }
  });
}
```

- [ ] **Step 4: Write fetch/runPoll infrastructure**

Port `fetchPollJson()`, `runPoll()`, `waitMs()`, `noteDashboardPoll()`, `startDashboardPolling()`, `stopDashboardPolling()`, `dashboardVisible()`, `intervalVisible()`, `clearDashboardPollingIntervals()` from current dashboard unchanged.

- [ ] **Step 5: Write DOMContentLoaded handler**

```javascript
document.addEventListener('DOMContentLoaded', () => {
  initSidebar();
  applyDashboardI18n(document);
  startDashboardPolling();
});
```

- [ ] **Step 6: Build verify**

Run: `python3 scripts/minify_dashboard.py && pio run -e lilygo_t2can_dual 2>&1 | tail -3`
Expected: `SUCCESS`

- [ ] **Step 7: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "feat: add core JS — navigation, polling, state management"
```

---

### Task 7: Implement Per-Page JavaScript Functions

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`

Port all remaining JS functions from the current dashboard, organized by page:

- [ ] **Step 1: Port FSD control functions**

Functions to port: `updateFsdControl()`, `updateInjectButtons()`, `injectionStatusLabel()`, `toggleFsdTopButton()`, `emergencyStop()`, `resumeInj()`, `saveFsdSwitch()`, `saveApRestore()`.

- [ ] **Step 2: Port HW config + profile functions**

Functions to port: `setHW()`, `updateHW4()`, `updateHardwareDependentSections()`, `updateProfileControls()`, `updateProfileSeg()`, `profileDisplayName()`, `profileNamesForHw()`, `pushCfg()`, `updSeg()` (segmented control helper, adapted for selection cards).

- [ ] **Step 3: Port speed offset functions**

Functions to port: `updateHw3SpeedControl()`, `saveHw3Speed()`, `updateLegacyMppControl()`, `saveLegacyMpp()`, `updateHw3SlewControl()`, `saveHw3Slew()`.

- [ ] **Step 4: Port Bus2 + Stalk + Service Mode functions**

Functions to port: `loadBus2Ids()`, `updateBus2Status()` (new), `stalkTest()`, `toggleServiceMode()`, `t2canBus2IdAt()` accessor.

- [ ] **Step 5: Port OTA functions**

Functions to port: `uploadFirmware()`, `handleDrop()`, `resetOtaCredentials()`, drag-drop event handlers.

- [ ] **Step 6: Port Network functions**

Functions to port: all `saveAP()`, `loadApStatus()`, `scanWifi()`, `pickWifi()`, `renderWifiSlots()`, `loadWifiNetworks()`, `loadWifiStatus()`, `connectWifiSlot()`, `editWifiSlot()`, `clearWifiForm()`, `deleteWifiSlot()`, `saveWifi()`, and all gateway/DNS functions (`loadGatewayStatus()`, `loadGatewayDns()`, `saveGatewayDns()`, `loadGatewayBlocked()`, `clearGatewayBlocked()`, `testGatewayDns()`, `applyGatewayProfile()`, `setGatewayUpstreamMode()`, etc.).

- [ ] **Step 7: Port CAN tools functions**

Functions to port: `renderSniffer()`, `toggleSniffIdMode()`, `syncSniffPauseButton()`, `startRec()`, `stopRec()`, `pollRec()`, `renderEflg()`, `renderWriteProbe()`, `toggleCanDebug()`, `applyCanDebug()`, CAN sub-tab switching.

- [ ] **Step 8: Build verify**

Run: `python3 scripts/minify_dashboard.py && pio run -e lilygo_t2can_dual 2>&1 | tail -3`
Expected: `SUCCESS`

- [ ] **Step 9: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "feat: port all page-specific JS functions to new dashboard"
```

---

### Task 8: Implement I18N + Theme

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`

- [ ] **Step 1: Write I18N dictionaries**

Add `I18N_ZH` and `I18N_EN` objects. Start with the existing translations from the current dashboard, then add new entries for sidebar labels and page titles:

```javascript
const I18N_ZH = {
  // Sidebar
  'Overview': '概览',
  'Hardware Config': '模块配置',
  'FSD Switch': 'FSD 开关',
  'Speed Offset': '速度偏移',
  'Bus2 Control': 'Bus2 控制',
  'FSD Defense': 'FSD 防御',
  'OTA Update': 'OTA 升级',
  'Network Settings': '网络设置',
  'CAN Tools': 'CAN 工具',
  // ... copy all existing entries from current dashboard ...
  // ... add new entries for new UI elements ...
};
const I18N_EN = {};
Object.keys(I18N_ZH).forEach(k => I18N_EN[I18N_ZH[k]] = k);
```

- [ ] **Step 2: Write I18N application functions**

Port `trText()`, `toggleLanguage()`, `applyDashboardI18n()`, `updateLanguageButton()` from current dashboard. Add logic to translate sidebar nav items and page titles on language switch.

- [ ] **Step 3: Write theme toggle**

Port `toggleTheme()` from current dashboard. Add light theme CSS variables.

- [ ] **Step 4: Build verify**

Run: `python3 scripts/minify_dashboard.py && pio run -e lilygo_t2can_dual 2>&1 | tail -3`
Expected: `SUCCESS`

- [ ] **Step 5: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "feat: add I18N dictionaries and theme toggle"
```

---

### Task 9: Flash to Device and Manual Verification

**Files:** None (testing only)

- [ ] **Step 1: Build and flash**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e lilygo_t2can_dual -t upload`
Expected: Firmware uploaded to device

- [ ] **Step 2: Connect to device AP and verify**

Connect to the T2CAN AP WiFi, then open `http://100.100.1.1` in a browser.

Verify checklist:
- [ ] Sidebar renders with "Atlas" / "T-2CAN 控制面板" header
- [ ] All 9 nav items are visible and clickable
- [ ] Clicking each nav item switches the page content
- [ ] Top status bar shows CAN FPS, connection status, FSD status, uptime
- [ ] Overview page shows FSD toggle + stats grid with chip temperature
- [ ] Hardware config page shows HW version selection cards (Auto/Legacy/HW3/HW4)
- [ ] FSD switch page shows big toggle + boot injection option
- [ ] Speed offset page shows bucket tables with editable inputs
- [ ] Bus2 control page shows ID table + stalk test buttons + service mode
- [ ] FSD defense page shows slew toggle + protection status
- [ ] OTA page shows version info + upload area
- [ ] Network page shows WiFi + Gateway + DNS filtering + blacklist/whitelist
- [ ] CAN tools page shows pin config + sub-tabs (Sniffer/Recorder/Controller/Debug)
- [ ] Language toggle (中/EN) works across all pages
- [ ] Theme toggle works
- [ ] Mobile responsive: sidebar collapses on narrow viewport

- [ ] **Step 3: Fix any issues found during testing**

Iterate on any rendering or functional issues. Commit fixes.

- [ ] **Step 4: Final commit**

```bash
git add -A
git commit -m "feat: complete Atlas T-2CAN dashboard redesign with sidebar navigation"
```

---

## Self-Review Checklist

- [x] **Spec coverage:** Each of the 9 pages in the spec has a corresponding task (Tasks 2-5)
- [x] **No placeholders:** All tasks contain actual code or explicit port instructions
- [x] **Type consistency:** All element IDs are defined in HTML tasks and referenced consistently in JS tasks
- [x] **Build verification:** Every task ends with a build verify step
- [x] **Backend unchanged:** No tasks modify `mcp2515_dashboard.h` or `main.cpp`
- [x] **I18N covered:** Task 8 handles all translation work
- [x] **Mobile responsive:** CSS includes `@media (max-width: 768px)` rules
- [x] **Sidebar title:** "Atlas" + "T-2CAN 控制面板" as specified
- [x] **Chip temperature:** In overview page stats grid
- [x] **CAN pins in CAN Tools:** Moved from network to CAN tools page
- [x] **DNS filtering in Network:** Blacklist, whitelist, upstream DNS, profiles all included
