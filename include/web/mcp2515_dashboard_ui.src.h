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
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
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
  display: flex; height: 100vh; overflow: hidden;
  -webkit-text-size-adjust: 100%; -webkit-overflow-scrolling: touch; }

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
.content { flex: 1; overflow-y: auto; overflow-x: hidden; padding: 16px;
  -webkit-overflow-scrolling: touch; }

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
.tbl { width: 100%; border-collapse: collapse; font-size: 11px; table-layout: fixed; }
.tbl th { text-align: left; padding: 6px 8px; color: var(--tx3);
  border-bottom: 1px solid var(--border); font-weight: 500; font-size: 10px; }
.tbl td { padding: 4px 8px; border-bottom: 1px solid rgba(75,85,99,0.2);
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.tbl .hex { font-family: 'SF Mono', 'Courier New', monospace; color: var(--accent-light); }
.tbl-wrap { overflow-x: auto; -webkit-overflow-scrolling: touch; }
select.inp { font-size: 16px; }

/* === Inputs === */
.inp { background: var(--card-bg-alt); border: 1px solid var(--border);
  border-radius: 6px; padding: 6px 10px; color: var(--tx1); font-size: 16px;
  width: 100%; outline: none; max-width: 100%; box-sizing: border-box; }
.inp:focus { border-color: var(--accent-light); }
.inp::placeholder { color: var(--tx3); }
textarea.inp { resize: vertical; min-height: 60px; font-family: monospace;
  font-size: 11px; line-height: 1.5; }

/* === Status Indicator === */
.status-dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; }
.status-dot.ok { background: var(--ok); box-shadow: 0 0 6px var(--ok); }
.status-dot.err { background: var(--err); }
.status-dot.warn { background: var(--warn); }

/* === Mobile Bottom Tab Bar === */
.mob-tabs { display: none; position: fixed; bottom: 0; left: 0; right: 0;
  z-index: 200; background: var(--sidebar-bg); border-top: 1px solid var(--border);
  padding: 2px 0; padding-bottom: env(safe-area-inset-bottom, 0px);
  flex-shrink: 0; }
.mob-tab { display: flex; flex-direction: column; align-items: center;
  justify-content: center; padding: 4px 0; color: var(--tx3);
  font-size: 9px; cursor: pointer; flex: 1; -webkit-tap-highlight-color: transparent; }
.mob-tab.active { color: var(--accent-light); }
.mob-tab .mob-icon { font-size: 18px; line-height: 1; margin-bottom: 1px; }
.mob-more-panel { display: none; position: fixed; bottom: 52px; left: 0; right: 0;
  z-index: 210; background: var(--sidebar-bg); border-top: 1px solid var(--border);
  border-radius: 12px 12px 0 0; padding: 12px 16px; max-height: 60vh; overflow-y: auto; }
.mob-more-panel.open { display: block; }
.mob-more-item { display: block; padding: 10px 0; color: var(--tx2);
  font-size: 13px; border-bottom: 1px solid rgba(75,85,99,0.3); cursor: pointer; }
.mob-more-item:last-child { border-bottom: none; }
.mob-more-item.active { color: var(--accent-light); }
.mob-more-close { position: absolute; top: 8px; right: 12px; color: var(--tx3);
  font-size: 18px; cursor: pointer; }

/* === Mobile Responsive === */
@media (max-width: 768px) {
  .sidebar { display: none !important; }
  .overlay { display: none !important; }
  .mobile-toggle { display: none !important; }
  .mob-tabs { display: flex; }
  .main { width: 100%; padding-bottom: 52px; }
  .topbar { padding: 6px 10px; gap: 6px; min-height: auto; }
  .topbar-fps { font-size: 11px; }
  .topbar-badge { font-size: 10px; padding: 1px 6px; }
  .topbar-time { font-size: 10px; margin-left: auto; }
  .content { padding: 10px 10px; }
  .card { padding: 10px 8px; margin-bottom: 8px; border-radius: 8px; }
  .card-title { font-size: 13px; margin-bottom: 8px; }
  .stats { grid-template-columns: repeat(2, 1fr); gap: 4px; }
  .stat { padding: 6px 4px; }
  .stat-lbl { font-size: 9px; }
  .stat-val { font-size: 12px; }
  .sel-cards.c2, .sel-cards.c3, .sel-cards.c4 { grid-template-columns: repeat(2, 1fr); gap: 4px; }
  .sel-card { padding: 8px 4px; }
  .setting-row { flex-wrap: wrap; gap: 4px; padding: 6px 0; }
  .setting-name { font-size: 12px; }
  .tbl { font-size: 10px; }
  .tbl th { padding: 4px 4px; font-size: 9px; }
  .tbl td { padding: 3px 4px; font-size: 10px; }
  .btn { padding: 8px 12px; font-size: 12px; }
  .btn-sm { padding: 5px 8px; font-size: 10px; }
  .big-toggle { padding: 14px 8px; }
  .big-toggle .toggle-visual { width: 60px; height: 30px; }
  .big-toggle .toggle-visual .thumb { width: 26px; height: 26px; }
  .big-toggle.on .toggle-visual .thumb { left: 32px; }
  .big-toggle .toggle-label { font-size: 14px; }
  .sub-tabs { gap: 1px; padding: 2px; }
  .sub-tab { padding: 4px 4px; font-size: 10px; }
  .diag-grid { grid-template-columns: 1fr 1fr; gap: 3px; }
  .diag-item { padding: 3px 6px; font-size: 10px; }
  .upload-area { padding: 14px 8px; }
}
.mobile-toggle { display: none; background: none; border: none;
  color: var(--tx2); font-size: 20px; cursor: pointer; padding: 4px 8px; }
.overlay { display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.5);
  z-index: 150; }
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
<!-- HW Version Selection -->
<div class="card">
  <div class="card-title">硬件版本</div>
  <div class="card-subtitle">选择您的自动驾驶硬件版本</div>
  <div class="sel-cards c2" id="hw-cards">
    <div class="sel-card active" onclick="setHW(-1)">
      <div class="sel-lbl">推荐</div>
      <div class="sel-name">Auto</div>
    </div>
    <div class="sel-card" onclick="setHW(0)">
      <div class="sel-lbl">旧版</div>
      <div class="sel-name">Legacy</div>
    </div>
    <div class="sel-card" onclick="setHW(1)">
      <div class="sel-lbl">第三代</div>
      <div class="sel-name">HW3</div>
    </div>
    <div class="sel-card" onclick="setHW(2)">
      <div class="sel-lbl">第四代</div>
      <div class="sel-name">HW4</div>
    </div>
  </div>
</div>

<!-- Profile Selection -->
<div class="card">
  <div class="card-title">速度配置</div>
  <div class="card-subtitle">选择速度偏移方案</div>
  <div class="sel-cards c3" id="profile-cards">
    <div class="sel-card" onclick="setProfile(0)">
      <div class="sel-name">Chill</div>
    </div>
    <div class="sel-card active" onclick="setProfile(1)">
      <div class="sel-name">Normal</div>
    </div>
    <div class="sel-card" onclick="setProfile(2)">
      <div class="sel-name">Hurry</div>
    </div>
  </div>
</div>

<!-- AP Restore -->
<div class="card">
  <div class="setting-row">
    <div>
      <div class="setting-name">AP 状态恢复</div>
      <div class="setting-desc">重启后恢复上次 AP 配置</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="hw-ap-restore" onchange="saveConfig()">
      <div class="tgl-track"></div>
    </label>
  </div>
</div>
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
<!-- HW3 Custom Speed -->
<div class="card">
  <div class="card-title">HW3 自定义速度</div>
  <div class="setting-row">
    <div>
      <div class="setting-name">启用自定义速度</div>
      <div class="setting-desc">覆盖默认速度映射表</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="hw3-ct-tgl" onchange="saveHw3Speed()">
      <div class="tgl-track"></div>
    </label>
  </div>
</div>

<!-- Speed Bucket Table -->
<div class="card">
  <div class="card-title">速度映射表</div>
  <table class="tbl">
    <thead><tr><th>实际速度</th><th>目标速度</th></tr></thead>
    <tbody>
      <tr><td>30</td><td><input class="inp" type="number" id="hw3-b30" value="45" onchange="saveHw3Speed()" style="width:80px"></td></tr>
      <tr><td>40</td><td><input class="inp" type="number" id="hw3-b40" value="60" onchange="saveHw3Speed()" style="width:80px"></td></tr>
      <tr><td>50</td><td><input class="inp" type="number" id="hw3-b50" value="75" onchange="saveHw3Speed()" style="width:80px"></td></tr>
      <tr><td>60</td><td><input class="inp" type="number" id="hw3-b60" value="90" onchange="saveHw3Speed()" style="width:80px"></td></tr>
      <tr><td>70</td><td><input class="inp" type="number" id="hw3-b70" value="105" onchange="saveHw3Speed()" style="width:80px"></td></tr>
    </tbody>
  </table>
</div>

<!-- High Speed Section -->
<div class="card">
  <div class="card-title">高速映射</div>
  <table class="tbl">
    <thead><tr><th>实际速度</th><th>目标速度</th></tr></thead>
    <tbody>
      <tr><td>80</td><td><input class="inp" type="number" id="hw3-b80" value="90" onchange="saveHw3Speed()" style="width:80px"></td></tr>
      <tr><td>100</td><td><input class="inp" type="number" id="hw3-b100" value="110" onchange="saveHw3Speed()" style="width:80px"></td></tr>
      <tr><td>120</td><td><input class="inp" type="number" id="hw3-b120" value="130" onchange="saveHw3Speed()" style="width:80px"></td></tr>
    </tbody>
  </table>
</div>

<!-- Encoding + Stats -->
<div class="card">
  <div class="setting-row">
    <div>
      <div class="setting-name">速度编码方式</div>
      <div class="setting-desc">选择 CAN 总线速度编码</div>
    </div>
    <select class="inp" id="hw3-enc" onchange="saveHw3Speed()" style="width:120px">
      <option value="0">默认</option>
      <option value="1">编码 A</option>
      <option value="2">编码 B</option>
    </select>
  </div>
</div>

<!-- Realtime Stats -->
<div class="card">
  <div class="card-title">实时数据</div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl">Fused</span><span class="v-acc" id="sp-fused">--</span></div>
    <div class="diag-item"><span class="lbl">Stock Offset</span><span class="v-dim" id="sp-stock">--</span></div>
    <div class="diag-item"><span class="lbl">Raw Write</span><span class="v-dim" id="sp-raw">--</span></div>
    <div class="diag-item"><span class="lbl">Speed</span><span class="v-dim" id="sp-speed">--</span></div>
  </div>
</div>
    </div>

    <!-- Page 5: Bus2 Control -->
    <div class="page" id="pg-bus2">
<!-- Bus2 Status -->
<div class="stats">
  <div class="stat"><div class="stat-lbl">Bus2 状态</div><div class="stat-val v-dim" id="b2-status">Offline</div></div>
  <div class="stat"><div class="stat-lbl">Bus2 RX</div><div class="stat-val v-info" id="b2-rx">0</div></div>
  <div class="stat"><div class="stat-lbl">已发现 ID</div><div class="stat-val v-acc" id="b2-ids">0</div></div>
</div>

<!-- Bus2 ID Table -->
<div class="card">
  <div class="card-title">Bus2 CAN ID 列表 <span style="color:var(--tx3);font-size:11px;font-weight:400" id="b2-count">(0)</span></div>
  <div style="max-height:200px;overflow-y:auto">
    <table class="tbl">
      <thead><tr><th>ID</th><th>DLC</th><th>数据</th><th>计数</th></tr></thead>
      <tbody id="b2-rows"></tbody>
    </table>
  </div>
</div>

<!-- Stalk Test -->
<div class="card">
  <div class="card-title">灯光注入测试</div>
  <div class="card-subtitle">模拟方向盘拨杆操作</div>
  <div style="display:flex;gap:8px;margin-bottom:10px">
    <button class="btn btn-outline" onclick="stalkTest('PULL')" style="flex:1">PULL (闪光)</button>
    <button class="btn btn-outline" onclick="stalkTest('PUSH')" style="flex:1">PUSH (远光)</button>
  </div>
  <div class="setting-row">
    <div class="setting-name">持续时间 (ms)</div>
    <input class="inp" type="number" id="stalk-dur" value="500" style="width:100px">
  </div>
  <div class="setting-row">
    <div class="setting-name">状态</div>
    <div class="setting-desc" id="stalk-status">空闲</div>
  </div>
</div>

<!-- Service Mode -->
<div class="card">
  <div class="setting-row">
    <div>
      <div class="setting-name">Service Mode</div>
      <div class="setting-desc">启用 0x339 持续注入 Bus2</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="svc-mode-tgl" onchange="toggleServiceMode()">
      <div class="tgl-track"></div>
    </label>
  </div>
</div>
    </div>

    <!-- Page 6: FSD Defense -->
    <div class="page" id="pg-defense">
<!-- Slew Toggle -->
<div class="card">
  <div class="card-title">偏移速率保护</div>
  <div class="card-subtitle">防止速度偏移突变被检测</div>
  <div class="setting-row">
    <div>
      <div class="setting-name">启用 slew rate 限制</div>
      <div class="setting-desc">限制偏移值下降速率</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="hw3-slew-tgl" onchange="saveHw3Slew()">
      <div class="tgl-track"></div>
    </label>
  </div>
</div>

<!-- Slew Config -->
<div class="card">
  <div class="card-title">保护参数</div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl">最大下降速率</span><span class="v-acc" id="def-rate">--</span></div>
    <div class="diag-item"><span class="lbl">最小保持偏移</span><span class="v-acc" id="def-min">--</span></div>
    <div class="diag-item"><span class="lbl">触发次数</span><span class="v-warn" id="def-cnt">0</span></div>
    <div class="diag-item"><span class="lbl">当前偏移</span><span class="v-dim" id="def-cur">--</span></div>
  </div>
</div>

<!-- Protection Status -->
<div class="card">
  <div class="setting-row">
    <div style="display:flex;align-items:center;gap:8px">
      <span class="status-dot err" id="def-dot"></span>
      <div class="setting-name" id="def-status">保护未启用</div>
    </div>
  </div>
</div>
    </div>

    <!-- Page 7: OTA Update -->
    <div class="page" id="pg-ota">
<!-- Version Info -->
<div class="card">
  <div class="card-title">固件信息</div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl">版本</span><span class="v-acc" id="ota-ver">--</span></div>
    <div class="diag-item"><span class="lbl">构建时间</span><span class="v-dim" id="ota-build">--</span></div>
    <div class="diag-item"><span class="lbl">Flash 占用</span><span class="v-dim" id="ota-flash">--</span></div>
    <div class="diag-item"><span class="lbl">SDK</span><span class="v-dim" id="ota-sdk">--</span></div>
  </div>
</div>

<!-- Upload Area -->
<div class="card">
  <div class="card-title">固件上传</div>
  <div class="upload-area" id="ota-drop" onclick="$('ota-file').click()">
    <div style="font-size:24px;color:var(--tx3);margin-bottom:4px">↑</div>
    <div style="color:var(--tx1);font-weight:500">选择固件文件</div>
    <div style="color:var(--tx3);font-size:11px">支持 .bin 格式，拖放或点击选择</div>
  </div>
  <input type="file" id="ota-file" accept=".bin" style="display:none" onchange="uploadFirmware()">
  <div style="margin-top:10px">
    <button class="btn" id="ota-btn" onclick="uploadFirmware()" style="width:100%">开始上传</button>
  </div>
  <div id="ota-progress" style="display:none;margin-top:8px">
    <div style="background:var(--card-bg-alt);border-radius:4px;height:8px;overflow:hidden">
      <div id="ota-bar" style="background:var(--accent);height:100%;width:0%;transition:width .3s"></div>
    </div>
    <div style="text-align:center;color:var(--tx3);font-size:11px;margin-top:4px" id="ota-pct">0%</div>
  </div>
</div>
    </div>

    <!-- Page 8: Network Settings -->
    <div class="page" id="pg-network">
<!-- WiFi Hotspot -->
<div class="card">
  <div class="card-title">WiFi 热点</div>
  <div class="setting-row">
    <div class="setting-name">SSID</div>
    <div class="v-dim" id="ap-ssid">--</div>
  </div>
  <div class="setting-row">
    <div class="setting-name">连接设备</div>
    <div class="v-ok" id="ap-clients">0</div>
  </div>
  <div class="setting-row">
    <div class="setting-name">WiFi Mode</div>
    <div class="v-acc" id="ap-mode">--</div>
  </div>
</div>

<!-- WiFi Internet -->
<div class="card">
  <div class="card-title">WiFi 联网</div>
  <div class="setting-row">
    <div class="setting-name">状态</div>
    <div class="v-warn" id="wifi-status">未配置</div>
  </div>
  <div id="wifi-slots" style="margin-top:8px"></div>
  <div style="margin-top:8px;display:flex;gap:6px">
    <button class="btn btn-sm" onclick="scanWifi()">扫描网络</button>
    <button class="btn btn-sm btn-outline" onclick="editWifiSlot(-1)">手动添加</button>
  </div>
  <div id="wifi-form" style="display:none;margin-top:10px">
    <div class="setting-row"><div class="setting-name">SSID</div><input class="inp" id="wf-ssid" style="width:160px"></div>
    <div class="setting-row"><div class="setting-name">密码</div><input class="inp" type="password" id="wf-pass" style="width:160px"></div>
    <div style="display:flex;gap:6px;margin-top:6px">
      <button class="btn btn-sm" onclick="saveWifi()">保存</button>
      <button class="btn btn-sm btn-outline" onclick="clearWifiForm()">取消</button>
    </div>
  </div>
</div>

<!-- STA-AP Gateway -->
<div class="card">
  <div class="setting-row">
    <div class="setting-name">STA-AP 网关 (NAT)</div>
    <label class="tgl">
      <input type="checkbox" id="gw-nat-tgl" onchange="saveGateway()">
      <div class="tgl-track"></div>
    </label>
  </div>
  <div class="diag-grid" style="margin-top:6px">
    <div class="diag-item"><span class="lbl">AP</span><span class="v-dim" id="gw-ap">--</span></div>
    <div class="diag-item"><span class="lbl">STA</span><span class="v-dim" id="gw-sta">--</span></div>
    <div class="diag-item"><span class="lbl">NAT</span><span class="v-dim" id="gw-nat-st">--</span></div>
    <div class="diag-item"><span class="lbl">DNS</span><span class="v-dim" id="gw-dns-st">--</span></div>
    <div class="diag-item"><span class="lbl">Slow</span><span class="v-dim" id="gw-slow">0</span></div>
    <div class="diag-item"><span class="lbl">Pending</span><span class="v-dim" id="gw-pend">0</span></div>
    <div class="diag-item"><span class="lbl">Upstream</span><span class="v-dim" id="gw-upstream">--</span></div>
    <div class="diag-item"><span class="lbl">Clients</span><span class="v-dim" id="gw-clients">0</span></div>
  </div>
  <div class="setting-row" style="margin-top:6px">
    <div>
      <div class="setting-name">网络性能模式</div>
      <div class="setting-desc">转发流量时降低 WebUI 轮询</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="gw-perf-tgl" onchange="saveGateway()">
      <div class="tgl-track"></div>
    </label>
  </div>
</div>

<!-- Upstream DNS -->
<div class="card">
  <div class="card-title">上游 DNS</div>
  <div class="sel-cards c4" id="dns-upstream">
    <div class="sel-card active" onclick="setDnsUpstream('auto')"><div class="sel-name">Auto</div></div>
    <div class="sel-card" onclick="setDnsUpstream('223.5.5.5')"><div class="sel-name">Ali</div></div>
    <div class="sel-card" onclick="setDnsUpstream('119.29.29.29')"><div class="sel-name">Tencent</div></div>
    <div class="sel-card" onclick="setDnsUpstream('custom')"><div class="sel-name">Custom</div></div>
  </div>
  <div id="dns-custom-row" style="display:none;margin-top:6px">
    <input class="inp" id="dns-custom-ip" placeholder="输入 DNS IP 地址">
  </div>
  <div class="sel-cards c2" style="margin-top:6px" id="dns-profile">
    <div class="sel-card" onclick="setDnsProfile('conservative')"><div class="sel-name">保守模式</div></div>
    <div class="sel-card active" onclick="setDnsProfile('aggressive')"><div class="sel-name">激进模式</div></div>
  </div>
</div>

<!-- DNS Filter Rules -->
<div class="card">
  <div class="card-title">DNS 过滤规则</div>
  <div style="margin-bottom:8px">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px">
      <span style="color:var(--err);font-weight:500;font-size:11px">黑名单</span>
      <span style="color:var(--tx3);font-size:10px" id="dns-bl-cnt">0 域名</span>
    </div>
    <textarea class="inp" id="dns-blacklist" rows="3" placeholder="每行一个域名"></textarea>
  </div>
  <div style="margin-bottom:8px">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px">
      <span style="color:var(--ok);font-weight:500;font-size:11px">白名单</span>
      <span style="color:var(--tx3);font-size:10px" id="dns-wl-cnt">0 域名</span>
    </div>
    <textarea class="inp" id="dns-whitelist" rows="3" placeholder="每行一个域名"></textarea>
  </div>
  <div style="display:flex;gap:6px;margin-bottom:8px">
    <button class="btn btn-sm" onclick="saveGatewayDns()">保存规则</button>
    <button class="btn btn-sm btn-outline" onclick="loadGatewayDns()">刷新</button>
  </div>
  <div style="display:flex;gap:6px;margin-bottom:8px">
    <input class="inp" id="dns-test-input" placeholder="输入域名测试..." style="flex:1">
    <button class="btn btn-sm" onclick="testGatewayDns()">测试</button>
  </div>
  <div style="display:flex;justify-content:space-between;align-items:center">
    <span style="color:var(--tx3);font-size:10px">已拦截: <span id="dns-blocked-cnt">0</span> 条</span>
    <div style="display:flex;gap:4px">
      <button class="btn btn-sm btn-outline" onclick="loadGatewayBlocked()">刷新</button>
      <button class="btn btn-sm btn-outline" onclick="clearGatewayBlocked()">清空</button>
    </div>
  </div>
</div>
    </div>

    <!-- Page 9: CAN Tools -->
    <div class="page" id="pg-can">
<!-- CAN Pin Config -->
<div class="card">
  <div class="card-title">CAN 引脚配置</div>
  <div class="card-subtitle">Bus1 TWAI + Bus2 MCP2515 SPI</div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl">Bus1 TX</span><span class="v-acc">GPIO 7</span></div>
    <div class="diag-item"><span class="lbl">Bus1 RX</span><span class="v-acc">GPIO 6</span></div>
    <div class="diag-item"><span class="lbl">Bus2 CS</span><span class="v-acc" id="can-cs">GPIO 10</span></div>
    <div class="diag-item"><span class="lbl">SPI SCK</span><span class="v-acc" id="can-sck">GPIO 12</span></div>
    <div class="diag-item"><span class="lbl">SPI MISO</span><span class="v-acc" id="can-miso">GPIO 13</span></div>
    <div class="diag-item"><span class="lbl">SPI MOSI</span><span class="v-acc" id="can-mosi">GPIO 11</span></div>
    <div class="diag-item"><span class="lbl">MCP RST</span><span class="v-acc" id="can-rst">GPIO 9</span></div>
  </div>
</div>

<!-- Sub-tabs -->
<div class="sub-tabs">
  <div class="sub-tab active" onclick="switchCanTab('sniffer')">Sniffer</div>
  <div class="sub-tab" onclick="switchCanTab('recorder')">Recorder</div>
  <div class="sub-tab" onclick="switchCanTab('controller')">Controller</div>
  <div class="sub-tab" onclick="switchCanTab('debug')">Debug</div>
</div>

<!-- Sniffer -->
<div id="can-sniffer">
  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px">
      <span style="font-weight:600">CAN Sniffer</span>
      <span style="color:var(--tx3);font-size:11px" id="sniff-count">0 frames</span>
    </div>
    <div style="display:flex;gap:6px;margin-bottom:8px">
      <input class="inp" id="sniff-filter" placeholder="Filter: ID or name..." style="flex:1">
      <button class="btn btn-sm btn-outline" id="sniff-pause" onclick="toggleSniffPause()">⏸</button>
    </div>
    <div style="max-height:250px;overflow-y:auto">
      <table class="tbl">
        <thead><tr><th>ID</th><th>Dir</th><th>数据</th><th>Age</th></tr></thead>
        <tbody id="sniff-rows"></tbody>
      </table>
    </div>
  </div>
</div>

<!-- Recorder -->
<div id="can-recorder" style="display:none">
  <div class="card">
    <div class="card-title">CAN Recorder</div>
    <div style="display:flex;gap:8px;margin-bottom:10px">
      <button class="btn btn-sm" id="rec-start" onclick="startRec()">开始录制</button>
      <button class="btn btn-sm btn-outline" id="rec-stop" onclick="stopRec()" disabled>停止</button>
      <button class="btn btn-sm btn-outline" id="rec-dl" onclick="downloadRec()" disabled>下载 CSV</button>
    </div>
    <div class="setting-row">
      <div class="setting-name">录制帧数限制</div>
      <input class="inp" type="number" id="rec-limit" value="1000" style="width:100px">
    </div>
    <div class="setting-row">
      <div class="setting-name">状态</div>
      <div class="setting-desc" id="rec-status">空闲</div>
    </div>
  </div>
</div>

<!-- Controller -->
<div id="can-controller" style="display:none">
  <div class="card">
    <div class="card-title">CAN Controller 状态</div>
    <div class="diag-grid">
      <div class="diag-item"><span class="lbl">EFLG</span><span class="v-dim" id="ctrl-eflg">0x00</span></div>
      <div class="diag-item"><span class="lbl">RX Errors</span><span class="v-dim" id="ctrl-rxerr">0</span></div>
      <div class="diag-item"><span class="lbl">TX Errors</span><span class="v-dim" id="ctrl-txerr">0</span></div>
      <div class="diag-item"><span class="lbl">Mode</span><span class="v-dim" id="ctrl-mode">--</span></div>
    </div>
    <div id="ctrl-mux" style="margin-top:8px"></div>
  </div>
</div>

<!-- Debug -->
<div id="can-debug" style="display:none">
  <div class="card">
    <div class="card-title">Debug</div>
    <div class="setting-row">
      <div class="setting-name">Debug 日志</div>
      <label class="tgl">
        <input type="checkbox" id="debug-tgl" onchange="toggleCanDebug()">
        <div class="tgl-track"></div>
      </label>
    </div>
  </div>
  <div class="card">
    <div class="card-title">Last Write Check</div>
    <div class="diag-grid">
      <div class="diag-item"><span class="lbl">Injected</span><span class="v-acc" id="lw-injected">--</span></div>
      <div class="diag-item"><span class="lbl">Bus</span><span class="v-dim" id="lw-bus">--</span></div>
      <div class="diag-item"><span class="lbl">Match</span><span class="v-dim" id="lw-match">--</span></div>
      <div class="diag-item"><span class="lbl">Age</span><span class="v-dim" id="lw-age">--</span></div>
    </div>
  </div>
</div>
    </div>

  </div>
</div>

<script>
// ═══════════════════════════════════════════════════════════
// Atlas T-2CAN Dashboard — Complete JavaScript
// ═══════════════════════════════════════════════════════════

// ── Constants & State ──────────────────────────────────────
var HW_NAMES = ['Legacy','HW3','HW4'];
var HW_LABELS = {
  'Legacy': {zh:'旧版',en:'Legacy'},
  'HW3': {zh:'第三代',en:'HW3'},
  'HW4': {zh:'第四代',en:'HW4'}
};
var SP_NAMES = ['Chill','Normal','Hurry'];
var S = {hw:-1,ci:false,sp:1,spa:true,can:false,ia:false,
         hw3OffsetSlew:false,hw3SlewRate:0,hw3CustomSpeed:false,
         hw3CustomTarget:[45,60,75,90,105],
         hw3HighSpeedEnable:false,
         hw3HighSpeedTarget:[90,110,130],
         hw3WireEncoding:0,legacyMppOverride:false,
         legacyMppCustomEnable:false,
         legacyMppHighSpeedEnable:false,
         legacyMppCustomTarget:[45,60,75,90,105],
         legacyMppHighSpeedTarget:[90,110,130]};
var lang = 'zh';
var dark = true;
var pollTimer = null;
var sniffPaused = false;
var sniffFrames = [];
var recActive = false;
var canTab = 'sniffer';

// ── Utilities ──────────────────────────────────────────────
function $(id){return document.getElementById(id)}
function setText(id,txt){var e=$(id);if(e)e.textContent=txt}
function setHtml(id,html){var e=$(id);if(e)e.innerHTML=html}
function setCls(id,cls){var e=$(id);if(e)e.className=cls}
function escHtml(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')}
function toHex(n){return '0x'+('0000'+n.toString(16).toUpperCase()).slice(-4)}
function fmtUp(sec){
  var h=Math.floor(sec/3600),m=Math.floor(sec%3600/60),s=sec%60;
  return (h<10?'0':'')+h+':'+(m<10?'0':'')+m+':'+(s<10?'0':'')+s;
}

// ── I18N ───────────────────────────────────────────────────
var I18N={
  // Sidebar
  '概览':'Overview','模块配置':'Hardware','FSD 开关':'FSD Switch',
  '速度偏移':'Speed Offset','Bus2 控制':'Bus2 Control',
  'FSD 防御':'FSD Defense','OTA 升级':'OTA Update',
  '网络设置':'Network','CAN 工具':'CAN Tools',
  // Top bar
  '已连接':'Connected','未连接':'Disconnected',
  // Overview
  'FSD 注入':'FSD Injection','FSD 开关':'FSD Toggle',
  '点击切换开关状态':'Tap to toggle','CAN Bus':'CAN Bus',
  '帧率':'FPS','硬件版本':'Hardware','速度偏移':'Offset',
  '芯片温度':'Chip Temp','TX Errors':'TX Err','跟随距离':'Follow Dist',
  // Hardware
  '硬件版本':'Hardware Version','选择您的自动驾驶硬件版本':'Select your AD hardware version',
  '推荐':'Recommended','旧版':'Legacy','第三代':'Gen 3','第四代':'Gen 4',
  '速度配置':'Speed Profile','选择速度偏移方案':'Select speed offset scheme',
  'AP 状态恢复':'AP State Restore','重启后恢复上次 AP 配置':'Restore AP config after reboot',
  // FSD page
  'FSD 注入控制':'FSD Injection Control',
  '启用后设备将注入 CAN 帧到车辆总线':'Injects CAN frames to vehicle bus when enabled',
  '已关闭':'OFF','已开启':'ON',
  '开机自动启用':'Auto-enable on boot','设备重启后自动开启 FSD 注入':'Auto-enable FSD after reboot',
  '紧急控制':'Emergency','重启设备':'Reboot','重置计数':'Reset Stats',
  // Speed
  'HW3 自定义速度':'HW3 Custom Speed','启用自定义速度':'Enable custom speed',
  '覆盖默认速度映射表':'Override default speed mapping',
  '速度映射表':'Speed Mapping','实际速度':'Actual','目标速度':'Target',
  '高速映射':'High Speed Mapping',
  '速度编码方式':'Speed Encoding','选择 CAN 总线速度编码':'Select CAN speed encoding',
  '默认':'Default','编码 A':'Enc A','编码 B':'Enc B',
  '实时数据':'Realtime Data',
  // Bus2
  'Bus2 状态':'Bus2 Status','Bus2 RX':'Bus2 RX','已发现 ID':'Found IDs',
  'Bus2 CAN ID 列表':'Bus2 CAN IDs',
  '灯光注入测试':'Lighting Test','模拟方向盘拨杆操作':'Simulate stalk operation',
  '持续时间 (ms)':'Duration (ms)','状态':'Status','空闲':'Idle',
  '启用 0x339 持续注入 Bus2':'Enable 0x339 continuous Bus2 injection',
  // Defense
  '偏移速率保护':'Slew Rate Protection','防止速度偏移突变被检测':'Prevent sudden offset detection',
  '启用 slew rate 限制':'Enable slew rate limit','限制偏移值下降速率':'Limit offset drop rate',
  '保护参数':'Protection Params',
  '最大下降速率':'Max Drop Rate','最小保持偏移':'Min Hold Offset',
  '触发次数':'Triggers','当前偏移':'Current Offset',
  '保护未启用':'Protection OFF','保护已启用':'Protection ON',
  // OTA
  '固件信息':'Firmware Info','版本':'Version','构建时间':'Build Time',
  'Flash 占用':'Flash Used',
  '固件上传':'Firmware Upload','选择固件文件':'Select firmware',
  '支持 .bin 格式，拖放或点击选择':'.bin files, drag-drop or click',
  '开始上传':'Start Upload','上传中...':'Uploading...',
  // Network
  'WiFi 热点':'WiFi Hotspot','SSID':'SSID','连接设备':'Clients',
  'WiFi 联网':'WiFi Internet','未配置':'Not configured',
  '扫描网络':'Scan','手动添加':'Add Manually','密码':'Password',
  '保存':'Save','取消':'Cancel',
  'STA-AP 网关 (NAT)':'STA-AP Gateway (NAT)',
  '网络性能模式':'Perf Mode','转发流量时降低 WebUI 轮询':'Reduce polling during forwarding',
  '上游 DNS':'Upstream DNS',
  '保守模式':'Conservative','激进模式':'Aggressive',
  'DNS 过滤规则':'DNS Filter Rules',
  '黑名单':'Blacklist','白名单':'Whitelist','域名':'domains',
  '保存规则':'Save Rules','刷新':'Refresh',
  '输入域名测试...':'Enter domain to test...','测试':'Test',
  '已拦截':'Blocked','条':'items','清空':'Clear',
  // CAN Tools
  'CAN 引脚配置':'CAN Pin Config',
  'CAN Sniffer':'CAN Sniffer',
  'CAN Recorder':'CAN Recorder','开始录制':'Start','停止':'Stop','下载 CSV':'Download CSV',
  '录制帧数限制':'Frame Limit',
  'CAN Controller 状态':'CAN Controller Status',
  'Debug 日志':'Debug Log',
  'Last Write Check':'Last Write Check'
};
function T(zh){return lang==='en'&&I18N[zh]?I18N[zh]:zh}
function applyI18n(){
  var navs=document.querySelectorAll('.nav-item');
  var zhTexts=['概览','模块配置','FSD 开关','速度偏移','Bus2 控制','FSD 防御','OTA 升级','网络设置','CAN 工具'];
  for(var i=0;i<navs.length;i++){
    navs[i].textContent=lang==='zh'?zhTexts[i]:I18N[zhTexts[i]];
  }
  $('lang-btn').textContent=lang==='zh'?'EN':'中';
}

// ── Theme ──────────────────────────────────────────────────
function toggleTheme(){
  dark=!dark;
  var r=document.documentElement.style;
  if(dark){
    r.setProperty('--sidebar-bg','#111827');
    r.setProperty('--main-bg','#1f2937');
    r.setProperty('--card-bg','#374151');
    r.setProperty('--card-bg-alt','#1f2937');
    r.setProperty('--tx1','#f9fafb');r.setProperty('--tx2','#d1d5db');
    r.setProperty('--tx3','#9ca3af');r.setProperty('--border','#4b5563');
    r.setProperty('--header-bg','#111827');
    $('theme-btn').textContent='☀';
  }else{
    r.setProperty('--sidebar-bg','#f3f4f6');
    r.setProperty('--main-bg','#e5e7eb');
    r.setProperty('--card-bg','#ffffff');
    r.setProperty('--card-bg-alt','#f9fafb');
    r.setProperty('--tx1','#111827');r.setProperty('--tx2','#374151');
    r.setProperty('--tx3','#6b7280');r.setProperty('--border','#d1d5db');
    r.setProperty('--header-bg','#f3f4f6');
    $('theme-btn').textContent='🌙';
  }
}

// ── Language ───────────────────────────────────────────────
function toggleLanguage(){
  lang=lang==='zh'?'en':'zh';
  applyI18n();
  updateFsdToggle(S.ci);
}

// ── Navigation ─────────────────────────────────────────────
function showPage(pageId){
  var pages=document.querySelectorAll('.page');
  for(var i=0;i<pages.length;i++)pages[i].classList.remove('active');
  var p=$(pageId);if(p)p.classList.add('active');
  var navs=document.querySelectorAll('.nav-item');
  for(var i=0;i<navs.length;i++){
    navs[i].classList.toggle('active',navs[i].getAttribute('data-page')===pageId);
  }
  closeSidebar();
  if(typeof updateMobTabs==='function')updateMobTabs(pageId);
  if(pageId==='pg-can')pollCanTab();
  if(pageId==='pg-bus2')pollBus2();
  if(pageId==='pg-network'){pollWifiStatus();pollGatewayStatus();loadGatewayDns();}
}
function openSidebar(){$('sidebar').classList.add('open');$('overlay').classList.add('active')}
function closeSidebar(){$('sidebar').classList.remove('open');$('overlay').classList.remove('active')}

// ── Fetch Helper ───────────────────────────────────────────
async function fetchJson(url){
  try{
    var r=await fetch(url);
    if(!r.ok)throw new Error(r.status);
    return await r.json();
  }catch(e){return null}
}
async function postForm(url,data){
  try{
    var body=[];
    for(var k in data)body.push(encodeURIComponent(k)+'='+encodeURIComponent(data[k]));
    await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.join('&')});
  }catch(e){}
}

// ── Polling ────────────────────────────────────────────────
async function poll(){
  var d=await fetchJson('/status');
  if(!d){
    setCls('tb-status','topbar-badge badge-err');
    setText('tb-status',T('未连接'));
    return;
  }
  S.hw=d.hw;S.ci=d.ci;S.sp=d.sp;S.spa=d.spAuto;S.can=d.can;S.ia=d.ia;
  S.hw3OffsetSlew=d.hw3OffsetSlew;S.hw3SlewRate=d.hw3SlewRate;
  S.hw3CustomSpeed=d.hw3CustomSpeed;
  S.hw3CustomTarget=d.hw3CustomTarget||[45,60,75,90,105];
  S.hw3HighSpeedEnable=d.hw3HighSpeedEnable;
  S.hw3HighSpeedTarget=d.hw3HighSpeedTarget||[90,110,130];
  S.hw3WireEncoding=d.hw3WireEncoding;
  S.legacyMppOverride=d.legacyMppOverride;
  S.legacyMppCustomEnable=d.legacyMppCustomEnable;
  S.legacyMppHighSpeedEnable=d.legacyMppHighSpeedEnable;
  S.legacyMppCustomTarget=d.legacyMppCustomTarget||[45,60,75,90,105];
  S.legacyMppHighSpeedTarget=d.legacyMppHighSpeedTarget||[90,110,130];

  // Top bar
  setText('tb-fps',d.fps.toFixed(1)+' Hz');
  setCls('tb-status','topbar-badge '+(d.can?'badge-ok':'badge-err'));
  setText('tb-status',d.can?T('已连接'):T('未连接'));
  setCls('tb-fsd','topbar-badge '+(d.ia?'badge-ok':'badge-warn'));
  setText('tb-fsd',d.ia?'FSD ON':'FSD OFF');
  setText('tb-up',fmtUp(d.up||0));

  // Overview
  var ovTgl=$('ov-fsd-tgl');
  if(ovTgl)ovTgl.checked=!!d.ci;
  setCls('s-can','stat-val '+(d.can?'v-ok':'v-err'));
  setText('s-can',d.can?'Online':'Offline');
  setText('s-rx',d.rx||0);
  setText('s-tx',d.tx||0);
  setText('s-fps',d.fps.toFixed(1)+' Hz');
  setText('s-hw',d.hw>=0&&d.hw<3?HW_NAMES[d.hw]:'Auto');
  setText('s-soff',d.soff||0);
  // Temp from /system_status is separate; use eflg field as proxy
  setText('s-txerr',d.txerr||0);
  setText('s-fd',d.fd||0);

  // FSD page toggle
  updateFsdToggle(d.ci);

  // Boot toggle
  var bt=$('fsd-boot-tgl');
  if(bt)bt.checked=!!d.ci;

  // HW page
  updateHwCards(d.hw);
  updateProfileCards(d.sp);

  // AP restore
  var apR=$('hw-ap-restore');
  if(apR)apR.checked=!!d.apAutoRestore;

  // Speed page
  updateSpeedPage(d);

  // Defense page
  updateDefensePage(d);

  // CAN controller sub-page
  updateCanController(d);
}

function updateFsdToggle(ci){
  var tgl=$('fsd-toggle');
  if(!tgl)return;
  if(ci){tgl.className='big-toggle on';setText('fsd-label',T('已开启'))}
  else{tgl.className='big-toggle off';setText('fsd-label',T('已关闭'))}
}

function updateHwCards(hw){
  var cards=$('hw-cards');
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  var map=[-1,0,1,2];
  for(var i=0;i<items.length;i++){
    items[i].classList.toggle('active',map[i]===hw);
  }
}

function updateProfileCards(sp){
  var cards=$('profile-cards');
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  for(var i=0;i<items.length;i++){
    items[i].classList.toggle('active',i===sp);
  }
}

function updateSpeedPage(d){
  var ctTgl=$('hw3-ct-tgl');
  if(ctTgl)ctTgl.checked=!!d.hw3CustomSpeed;
  // Fill bucket inputs
  var buckets=[30,40,50,60,70];
  for(var i=0;i<5;i++){
    var inp=$('hw3-b'+buckets[i]);
    if(inp&&d.hw3CustomTarget)inp.value=d.hw3CustomTarget[i];
  }
  var hsb=[80,100,120];
  for(var i=0;i<3;i++){
    var inp=$('hw3-b'+hsb[i]);
    if(inp&&d.hw3HighSpeedTarget)inp.value=d.hw3HighSpeedTarget[i];
  }
  var enc=$('hw3-enc');
  if(enc)enc.value=String(d.hw3WireEncoding||0);
  // Realtime stats
  setText('sp-fused',d.fusedSpeedLimitKph?d.fusedSpeedLimitKph+' kph':'--');
  setText('sp-stock',d.hw3StockOffset!==undefined?d.hw3StockOffset+' kph':'--');
  setText('sp-raw',d.fusedSpeedLimitRaw!==undefined?d.fusedSpeedLimitRaw:'--');
  setText('sp-speed',d.soff!==undefined?d.soff:'--');
}

function updateDefensePage(d){
  var tgl=$('hw3-slew-tgl');
  if(tgl)tgl.checked=!!d.hw3OffsetSlew;
  setText('def-rate',d.hw3SlewRate!==undefined?d.hw3SlewRate+'%/s':'--');
  setText('def-min',d.hw3OffsetTarget!==undefined?d.hw3OffsetTarget:'--');
  setText('def-cnt',d.hw3SlewCount||0);
  setText('def-cur',d.hw3OffsetLast!==undefined?d.hw3OffsetLast:'--');
  var dot=$('def-dot');
  var statusEl=$('def-status');
  if(d.hw3OffsetSlew){
    if(dot)dot.className='status-dot ok';
    setText('def-status',T('保护已启用'));
  }else{
    if(dot)dot.className='status-dot err';
    setText('def-status',T('保护未启用'));
  }
}

// ── FSD Toggle ─────────────────────────────────────────────
async function toggleFsd(){
  var next=S.ci?0:1;
  await postForm('/config',{can:next?'1':'0',force:next?'1':'0'});
  S.ci=!!next;
  var ovTgl=$('ov-fsd-tgl');
  if(ovTgl)ovTgl.checked=S.ci;
  updateFsdToggle(S.ci);
}

// ── HW Selection ───────────────────────────────────────────
async function setHW(hw){
  await postForm('/config',{hw:String(hw)});
  S.hw=hw;
  updateHwCards(hw);
}

// ── Profile Selection ──────────────────────────────────────
async function setProfile(sp){
  await postForm('/config',{sp:String(sp),spa:'0'});
  S.sp=sp;S.spa=false;
  updateProfileCards(sp);
}

// ── Save Config (generic toggle) ───────────────────────────
async function saveConfig(){
  var data={};
  var apR=$('hw-ap-restore');
  if(apR)data.apRestore=apR.checked?'1':'0';
  var bt=$('fsd-boot-tgl');
  if(bt&&bt.checked)data.can='1';
  else if(bt)data.can='0';
  await postForm('/config',data);
}

// ── HW3 Speed Save ─────────────────────────────────────────
async function saveHw3Speed(){
  var data={};
  var ctTgl=$('hw3-ct-tgl');
  if(ctTgl)data.hw3CustomSpeed=ctTgl.checked?'1':'0';
  var buckets=[30,40,50,60,70];
  for(var i=0;i<5;i++){
    var inp=$('hw3-b'+buckets[i]);
    if(inp)data['hw3CustomT'+i]=inp.value;
  }
  var hsb=[80,100,120];
  for(var i=0;i<3;i++){
    var inp=$('hw3-b'+hsb[i]);
    if(inp)data['hw3HighTarget'+i]=inp.value;
  }
  var enc=$('hw3-enc');
  if(enc)data.hw3WireEncoding=enc.value;
  await postForm('/config',data);
}

// ── HW3 Slew Save ──────────────────────────────────────────
async function saveHw3Slew(){
  var tgl=$('hw3-slew-tgl');
  var data={};
  if(tgl)data.hw3OffsetSlew=tgl.checked?'1':'0';
  await postForm('/config',data);
}

// ── Bus2 ───────────────────────────────────────────────────
async function pollBus2(){
  var d=await fetchJson('/bus2_ids');
  if(!d)return;
  setText('b2-ids',d.count||0);
  setText('b2-count','('+d.count+')');
  var rows='';
  if(d.ids){
    for(var i=0;i<d.ids.length;i++){
      var id=d.ids[i];
      rows+='<tr><td class="hex">'+escHtml(id.id)+'</td><td>'+id.dlc+'</td>'
        +'<td class="hex">'+escHtml(id.data)+'</td><td>'+id.count+'</td></tr>';
    }
  }
  setHtml('b2-rows',rows);
}

async function stalkTest(mode){
  var dur=$('stalk-dur');
  var durVal=dur?dur.value:'500';
  setText('stalk-status',T('测试中...')||'Testing...');
  try{
    await fetch('/stalk_test?mode='+(mode==='PULL'?'flash':'highbeam')+'&dur='+durVal);
    setText('stalk-status',T('空闲'));
  }catch(e){setText('stalk-status','Error')}
}

async function toggleServiceMode(){
  var tgl=$('svc-mode-tgl');
  await postForm('/service_mode',{on:tgl&&tgl.checked?'1':'0'});
}

// ── OTA Upload ─────────────────────────────────────────────
function uploadFirmware(){
  var fileInput=$('ota-file');
  if(!fileInput||!fileInput.files||fileInput.files.length===0)return;
  var file=fileInput.files[0];
  var prog=$('ota-progress');
  var bar=$('ota-bar');
  var pct=$('ota-pct');
  var btn=$('ota-btn');
  if(prog)prog.style.display='block';
  if(btn)btn.disabled=true;

  var xhr=new XMLHttpRequest();
  xhr.open('POST','/update',true);
  xhr.setRequestHeader('Authorization','Basic '+btoa('admin:admin'));

  xhr.upload.onprogress=function(e){
    if(e.lengthComputable){
      var p=Math.round(e.loaded/e.total*100);
      if(bar)bar.style.width=p+'%';
      if(pct)setText('ota-pct',p+'%');
    }
  };
  xhr.onload=function(){
    if(btn)btn.disabled=false;
    if(bar)bar.style.width='100%';
    if(pct)setText('ota-pct',xhr.status===200?'OK!':'Error');
  };
  xhr.onerror=function(){
    if(btn)btn.disabled=false;
    if(pct)setText('ota-pct','Error');
  };
  var fd=new FormData();
  fd.append('file',file);
  xhr.send(fd);
}

// OTA drag-drop setup
function setupOtaDrop(){
  var drop=$('ota-drop');
  if(!drop)return;
  drop.addEventListener('dragover',function(e){e.preventDefault();drop.style.borderColor='var(--accent-light)'});
  drop.addEventListener('dragleave',function(){drop.style.borderColor='var(--border)'});
  drop.addEventListener('drop',function(e){
    e.preventDefault();drop.style.borderColor='var(--border)';
    var fi=$('ota-file');
    if(e.dataTransfer.files.length>0&&fi){
      fi.files=e.dataTransfer.files;
      uploadFirmware();
    }
  });
}

// ── Firmware Info (from /system_status) ─────────────────────
async function loadFirmwareInfo(){
  var d=await fetchJson('/system_status');
  if(!d)return;
  setText('ota-ver',d.firmware||'--');
  setText('ota-build',d.idf||'--');
  var appUsed=d.app_used?Math.round(d.app_used/1024)+'KB':'--';
  var appTotal=d.app_size?Math.round(d.app_size/1024)+'KB':'--';
  setText('ota-flash',appUsed+' / '+appTotal);
  setText('ota-sdk',d.target||'--');
}

// ── WiFi ───────────────────────────────────────────────────
async function pollWifiStatus(){
  var d=await fetchJson('/wifi_status');
  if(!d)return;
  var st=$('wifi-status');
  if(st){
    if(d.connected){st.textContent=d.ssid+' ('+d.ip+')';st.className='v-ok'}
    else if(d.connecting){st.textContent=T('连接中...')||'Connecting...';st.className='v-warn'}
    else{st.textContent=T('未配置');st.className='v-warn'}
  }
  // Load networks
  var net=await fetchJson('/wifi_networks');
  if(net)renderWifiSlots(net);
  // AP status
  var ap=await fetchJson('/ap_status');
  if(ap){
    setText('ap-ssid',ap.ssid||'--');
    setText('ap-clients',ap.clients||0);
    setText('ap-mode',ap.mode||'--');
  }
}

function renderWifiSlots(net){
  var container=$('wifi-slots');
  if(!container)return;
  var html='';
  if(net.networks){
    for(var i=0;i<net.networks.length;i++){
      var n=net.networks[i];
      var active=net.active===n.idx;
      html+='<div class="setting-row"><div class="setting-name"'
        +(active?' style="color:var(--ok)"':'')
        +'>'+escHtml(n.ssid)+'</div>'
        +'<div style="display:flex;gap:4px">'
        +'<button class="btn btn-sm btn-outline" onclick="connectWifi('+n.idx+')">'+(active?'✓':'')+'</button>'
        +'<button class="btn btn-sm btn-outline" onclick="editWifiSlot('+n.idx+')">'+T('编辑')+'</button>'
        +'<button class="btn btn-sm btn-outline" style="color:var(--err)" onclick="deleteWifi('+n.idx+')">✕</button>'
        +'</div></div>';
    }
  }
  container.innerHTML=html;
}

var editingSlot=-1;
function editWifiSlot(idx){
  editingSlot=idx;
  var form=$('wifi-form');
  if(form)form.style.display='block';
  if(idx>=0){
    var ssid=$('wf-ssid');
    // We don't have the password; just leave blank
    if(ssid)ssid.value='';
  }
}
function clearWifiForm(){
  var form=$('wifi-form');if(form)form.style.display='none';
  var ssid=$('wf-ssid');if(ssid)ssid.value='';
  var pass=$('wf-pass');if(pass)pass.value='';
}

async function saveWifi(){
  var ssid=$('wf-ssid');var pass=$('wf-pass');
  if(!ssid||!ssid.value)return;
  var data={ssid:ssid.value,pass:pass?pass.value:'',idx:String(editingSlot>=0?editingSlot:-1)};
  await postForm('/wifi_config',data);
  clearWifiForm();
  pollWifiStatus();
}

async function scanWifi(){
  var d=await fetchJson('/wifi_scan?force=1');
  if(!d||!d.networks)return;
  // Show scan results in slots area
  var container=$('wifi-slots');
  if(!container)return;
  var html='<div style="margin-bottom:6px;color:var(--tx3);font-size:11px">'+T('扫描结果')+': '+d.networks.length+'</div>';
  for(var i=0;i<d.networks.length;i++){
    var n=d.networks[i];
    html+='<div class="setting-row" style="cursor:pointer" onclick="pickScanResult(\''+escHtml(n.ssid)+'\')">'
      +'<div class="setting-name">'+escHtml(n.ssid)+'</div>'
      +'<div class="v-dim" style="font-size:11px">'+n.rssi+' dBm</div></div>';
  }
  container.innerHTML=html;
}

function pickScanResult(ssid){
  var wf=$('wf-ssid');if(wf)wf.value=ssid;
  var form=$('wifi-form');if(form)form.style.display='block';
  editingSlot=-1;
}

async function connectWifi(idx){await postForm('/wifi_connect',{idx:String(idx)});pollWifiStatus()}
async function deleteWifi(idx){await postForm('/wifi_delete',{idx:String(idx)});pollWifiStatus()}

// ── Gateway Status ─────────────────────────────────────────
async function pollGatewayStatus(){
  var d=await fetchJson('/gateway_status');
  if(!d)return;
  setText('gw-ap',d.ap_ip||'--');
  setText('gw-sta',d.sta_connected?d.sta_ip:'--');
  setText('gw-nat-st',d.nat?'ON':'OFF');
  setText('gw-dns-st',d.dns_bind_ok?'OK':'--');
  setText('gw-slow',d.dns_slow_500ms||0);
  setText('gw-pend',d.dns_pending||0);
  setText('gw-upstream',d.upstream_dns||'--');
  setText('gw-clients',d.ap_clients||0);
  // NAT toggle
  var natTgl=$('gw-nat-tgl');
  if(natTgl)natTgl.checked=!!d.nat;
}

async function saveGateway(){
  var natTgl=$('gw-nat-tgl');
  // Gateway save is handled via gateway_dns endpoint for DNS rules
  // NAT toggle requires gateway config
  if(natTgl)await postForm('/gateway_dns',{enabled:natTgl.checked?'1':'0'});
  pollGatewayStatus();
}

// ── DNS Config ─────────────────────────────────────────────
async function loadGatewayDns(){
  var d=await fetchJson('/gateway_dns');
  if(!d)return;
  var bl=$('dns-blacklist');if(bl)bl.value=d.blacklist||'';
  var wl=$('dns-whitelist');if(wl)wl.value=d.whitelist||'';
  setText('dns-bl-cnt',(d.black_count||0)+' '+T('域名'));
  setText('dns-wl-cnt',(d.white_count||0)+' '+T('域名'));
  // Upstream DNS mode
  updateDnsUpstreamCards(d.upstream_mode||0);
  // Custom IP
  var customRow=$('dns-custom-row');
  var customIp=$('dns-custom-ip');
  if(d.upstream_mode===3&&customRow){customRow.style.display='block';if(customIp)customIp.value=d.upstream_custom||''}
  else if(customRow){customRow.style.display='none'}
}

function updateDnsUpstreamCards(mode){
  var cards=$('dns-upstream');
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  var modes=[0,1,2,3];
  for(var i=0;i<items.length;i++){
    items[i].classList.toggle('active',modes[i]===mode);
  }
}

async function setDnsUpstream(val){
  var mode=0;
  if(val==='auto')mode=0;
  else if(val==='223.5.5.5')mode=1;
  else if(val==='119.29.29.29')mode=2;
  else mode=3;
  updateDnsUpstreamCards(mode);
  var customRow=$('dns-custom-row');
  if(mode===3&&customRow)customRow.style.display='block';
  else if(customRow)customRow.style.display='none';
}

async function setDnsProfile(profile){
  var cards=$('dns-profile');
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  items[0].classList.toggle('active',profile==='conservative');
  items[1].classList.toggle('active',profile==='aggressive');
}

async function saveGatewayDns(){
  var data={enabled:'1'};
  var bl=$('dns-blacklist');if(bl)data.blacklist=bl.value;
  var wl=$('dns-whitelist');if(wl)data.whitelist=wl.value;
  var cards=$('dns-upstream');
  if(cards){
    var items=cards.querySelectorAll('.sel-card');
    for(var i=0;i<items.length;i++){
      if(items[i].classList.contains('active')){
        data.upstream_mode=String(i);
        break;
      }
    }
  }
  var customIp=$('dns-custom-ip');
  if(customIp)data.upstream_custom=customIp.value;
  await postForm('/gateway_dns',data);
  loadGatewayDns();
}

async function testGatewayDns(){
  var inp=$('dns-test-input');if(!inp||!inp.value)return;
  var d=await fetchJson('/gateway_dns_test?domain='+encodeURIComponent(inp.value));
  if(d){
    var result=d.action||d.decision||'--';
    alert('DNS Test: '+inp.value+' -> '+result);
  }
}

async function loadGatewayBlocked(){
  var d=await fetchJson('/gateway_blocked');
  var count=Array.isArray(d)?d.length:0;
  setText('dns-blocked-cnt',count);
}

async function clearGatewayBlocked(){
  await postForm('/gateway_blocked_clear',{});
  setText('dns-blocked-cnt','0');
}

// ── CAN Tools ──────────────────────────────────────────────
function switchCanTab(tab){
  canTab=tab;
  var tabs=document.querySelectorAll('.sub-tab');
  var tabIds=['can-sniffer','can-recorder','can-controller','can-debug'];
  var tabNames=['sniffer','recorder','controller','debug'];
  for(var i=0;i<tabs.length;i++)tabs[i].classList.toggle('active',tabNames[i]===tab);
  for(var i=0;i<tabIds.length;i++){
    var el=$(tabIds[i]);
    if(el)el.style.display=tabNames[i]===tab?'block':'none';
  }
  if(tab==='sniffer')pollSniffer();
  if(tab==='controller')pollCanController();
  if(tab==='debug')pollLastWrite();
}

async function pollCanTab(){switchCanTab(canTab)}

async function pollSniffer(){
  var d=await fetchJson('/frames');
  if(!d||!d.frames)return;
  sniffFrames=d.frames;
  renderSniffer();
}

function renderSniffer(){
  var rows=$('sniff-rows');if(!rows)return;
  var filter=$('sniff-filter');
  var f=filter?filter.value.toLowerCase():'';
  var count=0;
  var html='';
  for(var i=sniffFrames.length-1;i>=0;i--){
    var fr=sniffFrames[i];
    var idStr=toHex(fr.id);
    var name=fr.name||'';
    if(f&&idStr.toLowerCase().indexOf(f)<0&&name.toLowerCase().indexOf(f)<0)continue;
    count++;
    var dataStr='';
    if(fr.data){for(var j=0;j<fr.data.length;j++){if(j)dataStr+=' ';dataStr+=('0'+fr.data[j].toString(16).toUpperCase()).slice(-2)}}
    html+='<tr><td class="hex">'+idStr+(name?' <span style="color:var(--tx3);font-size:10px">'+escHtml(name)+'</span>':'')
      +'</td><td>RX</td><td class="hex">'+dataStr+'</td><td>--</td></tr>';
  }
  rows.innerHTML=html;
  setText('sniff-count',count+' frames');
}

function toggleSniffPause(){
  sniffPaused=!sniffPaused;
  var btn=$('sniff-pause');
  if(btn)btn.textContent=sniffPaused?'▶':'⏸';
}

// Recorder
async function startRec(){
  await postForm('/rec_start',{});
  recActive=true;
  $('rec-start').disabled=true;$('rec-stop').disabled=false;$('rec-dl').disabled=true;
  setText('rec-status',T('录制中')||'Recording...');
}
async function stopRec(){
  await postForm('/rec_stop',{});
  recActive=false;
  $('rec-start').disabled=false;$('rec-stop').disabled=true;$('rec-dl').disabled=false;
  setText('rec-status',T('已保存')||'Saved');
}
function downloadRec(){window.location.href='/rec_download'}

// Controller
async function pollCanController(){
  // Use /status for controller info
  var d=await fetchJson('/status');
  if(!d)return;
  setText('ctrl-eflg',toHex(d.eflg||0));
  setText('ctrl-mode',d.can?'Normal':'Offline');
  // Mux stats
  var mux=$('ctrl-mux');
  if(mux&&d.mux){
    var html='';
    for(var i=0;i<d.mux.length;i++){
      html+='<div class="diag-item"><span class="lbl">Mux '+i+'</span>'
        +'<span class="v-dim">RX:'+d.mux[i].rx+' TX:'+d.mux[i].tx+' Err:'+d.mux[i].err+'</span></div>';
    }
    mux.innerHTML=html;
  }
}
function updateCanController(d){
  setText('ctrl-eflg',toHex(d.eflg||0));
  setText('ctrl-rxerr',d.txerr||0);
  setText('ctrl-txerr',d.txerr||0);
  setText('ctrl-mode',d.can?'Normal':'Offline');
}

// Debug
async function toggleCanDebug(){
  var tgl=$('debug-tgl');
  if(tgl)await postForm('/logging',{eprn:tgl.checked?'1':'0'});
}

async function pollLastWrite(){
  var d=await fetchJson('/status');
  if(!d||!d.probe)return;
  var p=d.probe;
  setText('lw-injected',p.active?'Yes':'No');
  setText('lw-bus',p.id?toHex(p.id):'--');
  var stateNames=['Idle','Pending','Match','Different','Failed'];
  setText('lw-match',stateNames[p.state]||'--');
  setText('lw-age',p.txa?p.txa+'ms':'--');
}

// ── CAN Pins ───────────────────────────────────────────────
async function loadCanPins(){
  var d=await fetchJson('/can_pins');
  if(!d)return;
  setText('can-cs','GPIO 10');
  setText('can-sck','GPIO 12');
  setText('can-miso','GPIO 13');
  setText('can-mosi','GPIO 11');
  setText('can-rst','GPIO 9');
}

// ── Temp from system_status ────────────────────────────────
async function loadTemp(){
  var d=await fetchJson('/system_status');
  if(d&&d.temp_c!==undefined&&d.temp_c!==null){
    setText('s-temp',d.temp_c+'°C');
  }
}

// ── AP Config ──────────────────────────────────────────────
async function saveApConfig(){
  var ssid=$('ap-ssid-input');
  var pass=$('ap-pass-input');
  var data={};
  if(ssid)data.ssid=ssid.value;
  if(pass)data.pass=pass.value;
  await postForm('/ap_config',data);
}

// ── Init ───────────────────────────────────────────────────
// ── Mobile Tab Bar ──────────────────────────────────────────
function toggleMobMore(){
  var p=$('mob-more');
  if(p)p.classList.toggle('open');
}
function updateMobTabs(pageId){
  var tabs=document.querySelectorAll('.mob-tab[data-page]');
  for(var i=0;i<tabs.length;i++){
    tabs[i].classList.toggle('active',tabs[i].getAttribute('data-page')===pageId);
  }
  var items=document.querySelectorAll('.mob-more-item');
  for(var i=0;i<items.length;i++){
    items[i].classList.toggle('active',items[i].getAttribute('data-page')===pageId);
  }
}

document.addEventListener('DOMContentLoaded',function(){
  // Desktop sidebar nav
  var navs=document.querySelectorAll('.nav-item');
  for(var i=0;i<navs.length;i++){
    navs[i].addEventListener('click',function(){
      showPage(this.getAttribute('data-page'));
    });
  }

  // Mobile bottom tab bar
  var mtabs=document.querySelectorAll('.mob-tab[data-page]');
  for(var i=0;i<mtabs.length;i++){
    mtabs[i].addEventListener('click',function(){
      var pid=this.getAttribute('data-page');
      showPage(pid);
      updateMobTabs(pid);
    });
  }
  // Mobile more menu items
  var mitems=document.querySelectorAll('.mob-more-item');
  for(var i=0;i<mitems.length;i++){
    mitems[i].addEventListener('click',function(){
      var pid=this.getAttribute('data-page');
      showPage(pid);
      updateMobTabs(pid);
      toggleMobMore();
    });
  }

  // Initial page
  showPage('pg-overview');

  // OTA drag-drop
  setupOtaDrop();

  // Load initial data
  loadFirmwareInfo();
  loadCanPins();

  // Start polling
  poll();
  pollTimer=setInterval(function(){
    poll();
    loadTemp();
    // Conditional sub-page polling
    var activePage=document.querySelector('.page.active');
    if(activePage){
      var pid=activePage.id;
      if(pid==='pg-can'){
        if(canTab==='sniffer'&&!sniffPaused)pollSniffer();
        else if(canTab==='debug')pollLastWrite();
      }
      if(pid==='pg-bus2')pollBus2();
    }
  },1000);

  // Visibility handling
  document.addEventListener('visibilitychange',function(){
    if(document.hidden){
      if(pollTimer){clearInterval(pollTimer);pollTimer=null}
    }else{
      if(!pollTimer){
        poll();
        pollTimer=setInterval(function(){poll();loadTemp()},1000);
      }
    }
  });
});
</script>
<!-- Mobile Bottom Tab Bar -->
<div class="mob-tabs" id="mob-tabs">
  <div class="mob-tab active" data-page="pg-overview"><div class="mob-icon">📊</div><div>概览</div></div>
  <div class="mob-tab" data-page="pg-fsd"><div class="mob-icon">⚡</div><div>FSD</div></div>
  <div class="mob-tab" data-page="pg-bus2"><div class="mob-icon">🔌</div><div>Bus2</div></div>
  <div class="mob-tab" data-page="pg-network"><div class="mob-icon">📶</div><div>网络</div></div>
  <div class="mob-tab" onclick="toggleMobMore()"><div class="mob-icon">···</div><div>更多</div></div>
</div>
<!-- Mobile More Menu -->
<div class="mob-more-panel" id="mob-more">
  <div class="mob-more-close" onclick="toggleMobMore()">✕</div>
  <div class="mob-more-item" data-page="pg-overview">📊 概览</div>
  <div class="mob-more-item" data-page="pg-hardware">🔧 模块配置</div>
  <div class="mob-more-item" data-page="pg-fsd">⚡ FSD 开关</div>
  <div class="mob-more-item" data-page="pg-speed">🚀 速度偏移</div>
  <div class="mob-more-item" data-page="pg-bus2">🔌 Bus2 控制</div>
  <div class="mob-more-item" data-page="pg-defense">🛡 FSD 防御</div>
  <div class="mob-more-item" data-page="pg-ota">📦 OTA 升级</div>
  <div class="mob-more-item" data-page="pg-network">📶 网络设置</div>
  <div class="mob-more-item" data-page="pg-can">🔧 CAN 工具</div>
</div>
</body>
</html>)HTML";
