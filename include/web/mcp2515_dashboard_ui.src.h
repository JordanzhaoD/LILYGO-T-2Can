#pragma once
#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#else
#include <Arduino.h>
#endif

static const char DASH_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
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
<!-- FSD Quick Toggle -->
<div class="card">
  <div class="card-title">FSD 注入</div>
  <div class="setting-row">
    <div>
      <div class="setting-name">FSD 开关</div>
      <div class="setting-desc" id="ov-fsd-desc">点击切换开关状态</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="ov-fsd-tgl" onchange="toggleFsd()">
      <div class="tgl-track"></div>
    </label>
  </div>
</div>

<!-- Stats Grid -->
<div class="stats">
  <div class="stat"><div class="stat-lbl">CAN Bus</div><div class="stat-val v-dim" id="s-can">Offline</div></div>
  <div class="stat"><div class="stat-lbl">RX</div><div class="stat-val v-info" id="s-rx">0</div></div>
  <div class="stat"><div class="stat-lbl">TX</div><div class="stat-val v-info" id="s-tx">0</div></div>
  <div class="stat"><div class="stat-lbl">帧率</div><div class="stat-val v-info" id="s-fps">0.0 Hz</div></div>
  <div class="stat"><div class="stat-lbl">硬件版本</div><div class="stat-val v-acc" id="s-hw">--</div></div>
  <div class="stat"><div class="stat-lbl">速度偏移</div><div class="stat-val v-dim" id="s-soff">0</div></div>
</div>

<!-- Secondary Stats -->
<div class="stats">
  <div class="stat"><div class="stat-lbl">芯片温度</div><div class="stat-val v-dim" id="s-temp">--</div></div>
  <div class="stat"><div class="stat-lbl">TX Errors</div><div class="stat-val v-dim" id="s-txerr">0</div></div>
  <div class="stat"><div class="stat-lbl">跟随距离</div><div class="stat-val v-dim" id="s-fd">--</div></div>
</div>
    </div>

    <!-- Page 2: Hardware Config -->
    <div class="page" id="pg-hardware">
      <p style="color:var(--tx3)">Loading...</p>
    </div>

    <!-- Page 3: FSD Switch -->
    <div class="page" id="pg-fsd">
<div class="card">
  <div class="big-toggle off" id="fsd-toggle" onclick="toggleFsd()">
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
      <input type="checkbox" id="fsd-boot-tgl" onchange="saveConfig()">
      <div class="tgl-track"></div>
    </label>
  </div>
</div>

<div class="card">
  <div class="card-title">紧急控制</div>
  <div style="display:flex;gap:8px">
    <button class="btn btn-danger" onclick="fetch('/reboot')" style="flex:1">重启设备</button>
    <button class="btn" onclick="fetch('/reset_stats')" style="flex:1">重置计数</button>
  </div>
</div>
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
// JS will be implemented in Tasks 6-8
</script>
</body>
</html>)HTML";
