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
// JS will be implemented in Tasks 6-8
</script>
</body>
</html>)HTML";
