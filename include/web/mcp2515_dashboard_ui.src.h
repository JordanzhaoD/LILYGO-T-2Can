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
  --sidebar-w: 220px;
}
/* === Reset & Base === */
* { margin:0; padding:0; box-sizing:border-box; }
body { font-family: -apple-system, 'SF Pro Text', 'Helvetica Neue', sans-serif;
  background: var(--main-bg); color: var(--tx1); font-size: 15px;
  display: flex; height: 100vh; overflow: hidden;
  -webkit-text-size-adjust: 100%; -webkit-overflow-scrolling: touch; }

/* === Sidebar === */
.sidebar { width: var(--sidebar-w); background: var(--sidebar-bg);
  display: flex; flex-direction: column; flex-shrink: 0;
  border-right: 1px solid var(--border); overflow-y: auto; }
.sidebar-hdr { padding: 22px 24px 18px; border-bottom: 1px solid var(--border); }
.sidebar-hdr h1 { font-size: 24px; font-weight: 800; color: var(--accent); letter-spacing: 0; }
.sidebar-hdr p { font-size: 11px; color: var(--tx3); margin-top: 2px; }
.sidebar-nav { flex: 1; padding: 16px 10px; }
.nav-item { display: flex; align-items: center; gap: 14px; padding: 16px 18px; color: var(--tx3);
  font-size: 19px; cursor: pointer; border-radius: 12px;
  transition: background .15s, color .15s; user-select: none; font-weight: 600; }
.nav-item .nav-icon { font-size: 24px; line-height: 1; flex-shrink: 0; }
.nav-item:hover { background: var(--main-bg); color: var(--tx2); }
.nav-item.active { background: rgba(124,58,237,0.18); color: var(--accent-light); font-weight: 800; font-size: 20px; }
.sidebar-ft { padding: 10px 12px; border-top: 1px solid var(--border);
  display: flex; gap: 6px; }
.sidebar-ft button { flex:1; background: var(--card-bg); border: none;
  color: var(--tx3); padding: 4px 0; border-radius: 4px; font-size: 11px;
  cursor: pointer; }
.sidebar-ft button:hover { color: var(--tx2); }

/* === Main Area === */
.main { flex: 1; display: flex; flex-direction: column; overflow: hidden; }
.topbar { background: var(--header-bg); padding: 10px 16px;
  display: flex; align-items: center; gap: 12px; flex-shrink: 0;
  border-bottom: 1px solid var(--border); min-height: 44px; }
.topbar-badge { padding: 7px 16px; border-radius: 999px; font-size: 13px; font-weight: 800; border: 1px solid var(--border); background: var(--card-bg-alt); }
.badge-ok { color: var(--tx1); }
.badge-err { color: var(--err); }
.toast{position:fixed;bottom:60px;left:50%;transform:translateX(-50%);background:#7f1d1d;color:#fff;padding:8px 18px;border-radius:8px;font-size:13px;z-index:999;opacity:0;transition:opacity .3s;pointer-events:none}
.toast.ok{background:#166534}
.toast.show{opacity:1}
.badge-warn { color: var(--tx1); }
.topbar-fps { color: var(--tx1); font-size: 14px; font-weight: 800; padding: 7px 16px; border-radius: 999px; background: var(--card-bg-alt); border: 1px solid var(--border); }
.topbar-time { margin-left: auto; color: var(--tx3); font-size: 12px; }
.topbar-exp { color: var(--warn); font-size: 12px; font-weight: 800; padding: 7px 12px; border-radius: 999px; background: rgba(251,191,36,0.12); border: 1px solid rgba(251,191,36,0.35); }
.mobile-theme-toggle { display: none; border: 1px solid var(--border); background: var(--card-bg-alt); color: var(--tx1); border-radius: 999px; min-width: 76px; height: 32px; padding: 0 10px; font-size: 12px; font-weight: 800; white-space: nowrap; }
.topbar-dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block;
  flex-shrink: 0; }
.topbar-dot.ok { background: var(--ok); box-shadow: 0 0 6px var(--ok); }
.topbar-dot.err { background: var(--err); box-shadow: 0 0 6px var(--err); }
.topbar-dot.warn { background: var(--warn); box-shadow: 0 0 6px var(--warn); }
.content { flex: 1; overflow-y: auto; overflow-x: hidden; padding: 22px;
  -webkit-overflow-scrolling: touch; }

/* === Pages === */
.page { display: none; }
.page.active { display: block; }

/* === Cards === */
.card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 14px; padding: 22px;
  margin-bottom: 20px; }
.page-title { font-size: 26px; font-weight: 800; margin: 4px 0 20px; padding-bottom: 16px; border-bottom: 1px solid var(--border); color: var(--tx1); }
.card-title { font-size: 18px; font-weight: 800; color: var(--tx1);
  margin-bottom: 10px; }
.card-subtitle { font-size: 12px; color: var(--tx3); margin-top: -6px;
  margin-bottom: 10px; }
.quick-actions { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-bottom: 14px; }
.qa-btn { background: var(--card-bg); border: 2px solid var(--border); border-radius: 12px;
  padding: 14px 8px; cursor: pointer; text-align: center; color: var(--tx1);
  transition: all .2s; min-height: 80px; display: flex; flex-direction: column;
  align-items: center; justify-content: center; gap: 4px; }
.qa-btn:hover { border-color: var(--accent); transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(0,0,0,0.3); }
.qa-btn:active { transform: translateY(0); }
.qa-btn.qa-fsd.active { border-color: var(--ok); background: rgba(74,222,128,0.1); }
.qa-btn.qa-danger { border-color: rgba(248,113,113,0.3); }
.qa-btn.qa-danger:hover { border-color: var(--err); }
.qa-icon { font-size: 24px; line-height: 1; }
.qa-label { font-size: 13px; font-weight: 700; }
.qa-status { font-size: 10px; color: var(--tx3); font-weight: 500; }
.exp-badge { display: inline-block; margin-left: 8px; padding: 2px 8px; border-radius: 999px; background: rgba(251,191,36,0.12); border: 1px solid rgba(251,191,36,0.35); color: var(--warn); font-size: 11px; font-weight: 800; vertical-align: middle; }
.status-triplet { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin: 10px 0 16px; }
.status-chip { background: var(--card-bg-alt); border: 1px solid var(--border); border-radius: 10px; padding: 10px; min-height: 54px; }
.status-chip .lbl { color: var(--tx3); font-size: 11px; margin-bottom: 4px; }
.status-chip .val { color: var(--tx1); font-size: 14px; font-weight: 800; overflow-wrap: anywhere; }
.s-ok { color: var(--ok) !important; }
.s-warn { color: var(--warn) !important; }
.s-err { color: var(--err) !important; }

/* === Stats Grid === */
.stats { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px;
  margin-bottom: 4px; }
.stat { background: var(--card-bg); border: 1px solid var(--border); border-radius: 8px; padding: 12px;
  text-align: center; box-shadow: 0 1px 3px rgba(0,0,0,0.2); }
.stat-lbl { font-size: 11px; color: var(--tx3); margin-bottom: 2px; font-weight: 500; }
.stat-val { font-size: 16px; font-weight: 700; }
.v-ok { color: var(--ok); }
.v-err { color: var(--err); }
.v-warn { color: var(--warn); }
.v-info { color: var(--info); }
.v-acc { color: var(--accent-light); }
.v-dim { color: var(--tx3); }

/* === Toggle Switch === */
.tgl { position: relative; display: inline-block; width: 52px; height: 28px; flex-shrink: 0; }
.tgl input { opacity: 0; width: 0; height: 0; }
.tgl-track { position: absolute; inset: 0; background: var(--border);
  border-radius: 12px; transition: background .2s; cursor: pointer; }
.tgl-track::after { content: ''; position: absolute; width: 24px; height: 24px;
  background: var(--tx3); border-radius: 50%; top: 2px; left: 2px;
  transition: transform .2s, background .2s; }
.tgl input:checked + .tgl-track { background: var(--accent); }
.tgl input:checked + .tgl-track::after { transform: translateX(24px); background: #fff; }

/* === Buttons === */
.btn { background: var(--accent); border: none; color: #fff; padding: 12px 20px;
  border-radius: 8px; font-size: 14px; cursor: pointer; font-weight: 600;
  transition: opacity .15s; min-height: 44px; }
.btn:hover { opacity: 0.85; }
.btn-danger { background: #dc2626; }
.btn-outline { background: transparent; border: 1px solid var(--border);
  color: var(--tx3); }
.btn-outline:hover { border-color: var(--accent-light); color: var(--accent-light); }
.btn-sm { padding: 8px 14px; font-size: 12px; min-height: 36px; }

/* === Selection Cards (HW, Profile, etc.) === */
.sel-cards { display: grid; gap: 16px; }
.sel-cards.c2 { grid-template-columns: repeat(2, 1fr); }
.sel-cards.c3 { grid-template-columns: repeat(3, 1fr); }
.sel-cards.c4 { grid-template-columns: repeat(4, 1fr); }
.sel-card { background: var(--card-bg-alt); border: 2px solid var(--border);
  border-radius: 12px; padding: 24px 14px; text-align: center; cursor: pointer;
  transition: border-color .15s, background .15s; }
.sel-card:hover { border-color: var(--tx3); }
.sel-card.active { border-color: var(--accent-light); background: rgba(124,58,237,0.18); color: var(--accent-light); }
.sel-card.active .sel-lbl { color: var(--accent-light); }
.sel-card .sel-lbl { font-size: 10px; color: var(--tx3); text-transform: uppercase;
  letter-spacing: 0.5px; }
.sel-card .sel-name { font-size: 20px; font-weight: 800; margin-top: 2px; }

/* === Setting Row === */
.setting-row { display: flex; justify-content: space-between; align-items: center;
  padding: 12px 0; border-bottom: 1px solid rgba(75,85,99,0.3); }
.setting-row:last-child { border-bottom: none; }
.setting-name { font-size: 14px; color: var(--tx1); font-weight: 600; }
.setting-desc { font-size: 12px; color: var(--tx3); margin-top: 2px; }

/* === Sub-tabs (CAN tools) === */
.sub-tabs { display: flex; gap: 2px; background: var(--card-bg);
  border-radius: 6px; padding: 2px; margin-bottom: 10px; }
.sub-tab { flex: 1; text-align: center; padding: 8px 12px;
  border-radius: 4px; font-size: 13px; cursor: pointer;
  color: var(--tx3); transition: all .15s; font-weight: 600; }
.sub-tab.active { background: var(--accent); color: #fff; font-weight: 700; }

/* === Table === */
.tbl { width: 100%; border-collapse: collapse; font-size: 13px; table-layout: fixed; }
.tbl th { text-align: left; padding: 8px 10px; color: var(--tx3);
  border-bottom: 1px solid var(--border); font-weight: 500; font-size: 11px; }
.tbl td { padding: 6px 10px; border-bottom: 1px solid rgba(75,85,99,0.2);
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.tbl .hex { font-family: 'SF Mono', 'Courier New', monospace; color: var(--accent-light); }
.tbl-wrap { overflow-x: auto; -webkit-overflow-scrolling: touch; }
select.inp { font-size: 16px; }

/* === Inputs === */
.inp { background: var(--card-bg-alt); border: 1px solid var(--border);
  border-radius: 8px; padding: 10px 14px; color: var(--tx1); font-size: 16px;
  width: 100%; outline: none; max-width: 100%; box-sizing: border-box; }
.inp:focus { border-color: var(--accent-light); }
.inp::placeholder { color: var(--tx3); }
textarea.inp { resize: vertical; min-height: 60px; font-family: monospace;
  font-size: 11px; line-height: 1.5; }

/* === Status Indicator === */
.status-dot { width: 10px; height: 10px; border-radius: 50%; display: inline-block; }
.status-dot.ok { background: var(--ok); box-shadow: 0 0 6px var(--ok); }
.status-dot.err { background: var(--err); }
.status-dot.warn { background: var(--warn); }

/* === Mobile Bottom Tab Bar === */
.mob-tabs { display: none; position: fixed; bottom: 0; left: 0; right: 0;
  z-index: 200; background: var(--sidebar-bg); border-top: 1px solid var(--border);
  padding: 2px 0; padding-bottom: env(safe-area-inset-bottom, 0px);
  flex-shrink: 0; }
.mob-tab { display: flex; flex-direction: column; align-items: center;
  justify-content: center; padding: 6px 0; color: var(--tx3);
  font-size: 10px; cursor: pointer; flex: 1; -webkit-tap-highlight-color: transparent; font-weight: 600; }
.mob-tab.active { color: var(--accent-light); }
.mob-tab .mob-icon { font-size: 22px; line-height: 1; margin-bottom: 1px; }
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
  body { font-size: 14px; }
  .sidebar { display: none !important; }
  .overlay { display: none !important; }
  .mobile-toggle { display: none !important; }
  .mobile-theme-toggle { display: inline-flex; align-items: center; justify-content: center; order: 7; box-shadow: 0 1px 4px rgba(0,0,0,0.18); }
  .mob-tabs { display: flex; min-height: 68px; padding: 6px 0 calc(6px + env(safe-area-inset-bottom, 0px)); }
  .mob-tab { min-height: 58px; padding: 7px 2px; font-size: 12px; gap: 2px; }
  .mob-tab .mob-icon { font-size: 29px; margin-bottom: 2px; }
  .mob-more-panel { bottom: 72px; padding: 16px 18px; }
  .mob-more-item { padding: 14px 0; font-size: 15px; }
  .mob-more-close { font-size: 22px; }
  .main { width: 100%; padding-bottom: 74px; }
  .topbar { padding: 7px 8px; gap: 5px; min-height: 0; flex-wrap: wrap; align-content: center; }
  .topbar-dot { width: 7px; height: 7px; }
  .topbar-fps { font-size: 11px; padding: 5px 8px; max-width: 72px; overflow: hidden; white-space: nowrap; }
  .topbar-badge { font-size: 11px; padding: 5px 8px; white-space: nowrap; }
  .topbar-exp { order: 9; flex: 1 0 100%; text-align: center; font-size: 11px; padding: 4px 8px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .topbar-time { order: 8; font-size: 11px; margin-left: auto; padding-right: 2px; white-space: nowrap; }
  .content { padding: 8px 10px 12px; }
  .card { padding: 12px 10px; margin-bottom: 10px; border-radius: 8px; }
  .card-title { font-size: 15px; margin-bottom: 8px; }
  .stats { grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 6px; }
  .stat { padding: 9px 6px; min-width: 0; }
  .stat-lbl { font-size: 10px; }
  .stat-val { font-size: 15px; overflow-wrap: anywhere; }
  .sel-cards.c2, .sel-cards.c3, .sel-cards.c4 { grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 6px; }
  .sel-card { padding: 13px 7px; min-width: 0; }
  .sel-card .sel-name { font-size: 17px; overflow-wrap: anywhere; }
  .setting-row { flex-wrap: wrap; gap: 4px; padding: 10px 0; }
  .setting-name { font-size: 13px; }
  .tbl { font-size: 11px; }
  .tbl th { padding: 6px 6px; font-size: 10px; }
  .tbl td { padding: 4px 6px; font-size: 11px; }
  .btn { padding: 10px 16px; font-size: 13px; min-height: 42px; }
  .btn-sm { padding: 6px 10px; font-size: 11px; min-height: 34px; }
  .big-toggle { padding: 18px 10px; }
  .big-toggle .toggle-visual { width: 70px; height: 36px; }
  .big-toggle .toggle-visual .thumb { width: 32px; height: 32px; }
  .big-toggle.on .toggle-visual .thumb { left: 36px; }
  .big-toggle .toggle-label { font-size: 16px; }
  .sub-tabs { gap: 1px; padding: 2px; }
  .sub-tab { padding: 6px 8px; font-size: 12px; }
  .diag-grid { grid-template-columns: 1fr 1fr; gap: 3px; }
  .diag-item { padding: 4px 6px; font-size: 11px; }
  .upload-area { padding: 14px 8px; }
  .quick-actions { gap: 6px; }
  .qa-btn { padding: 10px 4px; min-height: 68px; }
  .qa-icon { font-size: 20px; }
  .qa-label { font-size: 12px; }
  .page-title { font-size: 20px; margin: 2px 0 10px; padding-bottom: 8px; }
  .status-triplet { grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 6px; margin: 6px 0 10px; }
  .status-triplet .status-chip { min-height: 46px; padding: 8px; }
  .status-triplet .status-chip:nth-child(3) { grid-column: 1 / -1; }
  .status-chip .lbl { font-size: 10px; margin-bottom: 3px; }
  .status-chip .val { font-size: 13px; }
  .diag-grid { grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 5px; }
  .diag-item { min-width: 0; align-items: center; }
  .diag-item span:last-child { overflow-wrap: anywhere; text-align: right; }
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
.big-toggle { text-align: center; padding: 24px; }
.big-toggle .toggle-visual { width: 80px; height: 40px; border-radius: 20px;
  margin: 0 auto 8px; position: relative; transition: background .3s;
  box-shadow: 0 0 20px rgba(0,0,0,0.3); }
.big-toggle .toggle-visual .thumb { width: 36px; height: 36px; border-radius: 50%;
  background: #fff; position: absolute; top: 2px; transition: left .3s; }
.big-toggle .toggle-label { font-size: 18px; font-weight: 700; }
.big-toggle.on .toggle-visual { animation: pulse-ok 2s ease-in-out infinite; }
@keyframes pulse-ok {
  0%,100% { box-shadow: 0 0 10px rgba(74,222,128,0.3); }
  50% { box-shadow: 0 0 25px rgba(74,222,128,0.6); }
}
.big-toggle.off .toggle-visual { animation: pulse-err 2.5s ease-in-out infinite; }
@keyframes pulse-err {
  0%,100% { box-shadow: 0 0 8px rgba(248,113,113,0.2); }
  50% { box-shadow: 0 0 18px rgba(248,113,113,0.4); }
}
.big-toggle.on .toggle-visual { background: var(--ok);
  box-shadow: 0 0 20px rgba(74,222,128,0.3); }
.big-toggle.on .toggle-visual .thumb { left: 42px; }
.big-toggle.off .toggle-visual { background: var(--err);
  box-shadow: 0 0 20px rgba(248,113,113,0.3); }
.big-toggle.off .toggle-visual .thumb { left: 2px; }

/* === Card Hover (desktop) === */
@media (min-width: 769px) {
  .card { transition: transform .2s, box-shadow .2s; }
  .card:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(0,0,0,0.25); }
}

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
.hero-status { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; margin-bottom: 20px; }
.feature-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 14px; }
.mode-note { margin-top: 10px; color: var(--tx3); font-size: 12px; line-height: 1.45; }
.mobile-home { display: none; }
@media (max-width: 768px) {
  .hero-status, .feature-grid { grid-template-columns: 1fr 1fr; }
  .nav-item { font-size: 16px; }
  .nav-item.active { font-size: 17px; }
  .mobile-home { display: block; }
  #pg-overview > .hero-status,
  #pg-overview > .quick-actions,
  #pg-overview > .card { display: none; }
  #pg-overview .page-title { margin-bottom: 8px; }
  #pg-overview > .status-triplet { grid-template-columns: repeat(2, minmax(0, 1fr)); }
  #pg-overview > .status-triplet .status-chip:nth-child(3) { grid-column: 1 / -1; }
}
</style>
</head>
<body>
<div class="toast" id="toast"></div>

<!-- Mobile overlay -->
<div class="overlay" id="overlay" onclick="closeSidebar()"></div>

<!-- Sidebar -->
<nav class="sidebar" id="sidebar">
  <div class="sidebar-hdr">
    <h1>Atlas</h1>
    <p>T-2CAN 控制面板</p>
  </div>
  <div class="sidebar-nav">
    <div class="nav-item active" data-page="pg-overview"><span class="nav-icon">▣</span>模块配置</div>
    <div class="nav-item" data-page="pg-hardware"><span class="nav-icon">◇</span>激活模式</div>
    <div class="nav-item" data-page="pg-drive"><span class="nav-icon">◉</span>驾驶模式</div>
    <div class="nav-item" data-page="pg-speed"><span class="nav-icon">↗</span>速度偏移</div>
    <div class="nav-item" data-page="pg-bus2"><span class="nav-icon">✦</span>CAN2控制</div>
    <div class="nav-item" data-page="pg-defense"><span class="nav-icon">◈</span>FSD防御</div>
    <div class="nav-item" data-page="pg-ota"><span class="nav-icon">⇧</span>OTA升级</div>
    <div class="nav-item" data-page="pg-network"><span class="nav-icon">◎</span>网络设置</div>
    <div class="nav-item" data-page="pg-can"><span class="nav-icon">⌘</span>CAN工具</div>
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
    <span class="topbar-dot ok" id="tb-dot-status"></span>
    <span class="topbar-badge badge-ok" id="tb-status">已连接</span>
    <span class="topbar-dot warn" id="tb-dot-fsd"></span>
    <span class="topbar-badge badge-warn" id="tb-fsd">FSD防封保护</span>
    <span class="topbar-exp" id="tb-exp">实验项 0</span>
    <button class="mobile-theme-toggle" onclick="toggleTheme()" id="mobile-theme-btn">☀ 白天</button>
    <span class="topbar-time" id="tb-up">00:00:00</span>
  </div>

  <!-- Content Area -->
  <div class="content" id="content">

    <!-- Page 1: Overview -->
    <div class="page active" id="pg-overview">
<div class="page-title">模块配置</div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl">UI 显示状态</div><div class="val" id="st-module-ui">待同步</div></div>
  <div class="status-chip"><div class="lbl">NVS 持久化状态</div><div class="val" id="st-module-nvs">读取中</div></div>
  <div class="status-chip"><div class="lbl">实际 CAN/网络运行状态</div><div class="val" id="st-module-run">待检测</div></div>
</div>
<div class="hero-status">
  <div class="stat"><div class="stat-lbl">FSD-V14 <span class="exp-badge">实验</span></div><div class="stat-val v-dim" id="ov-v14">待机</div></div>
  <div class="stat"><div class="stat-lbl">HW Auto</div><div class="stat-val v-acc" id="ov-hw">Auto</div></div>
  <div class="stat"><div class="stat-lbl">CAN1 / CAN2</div><div class="stat-val v-dim" id="ov-can">--</div></div>
  <div class="stat"><div class="stat-lbl">运行时间</div><div class="stat-val v-info" id="ov-up">00:00:00</div></div>
</div>
<div class="mobile-home">
  <div class="card">
    <div class="card-title">状态</div>
    <div class="stats">
      <div class="stat"><div class="stat-lbl">CAN1</div><div class="stat-val v-dim" id="m-can">--</div></div>
      <div class="stat"><div class="stat-lbl">CAN2</div><div class="stat-val v-dim" id="m-can2">--</div></div>
      <div class="stat"><div class="stat-lbl">FSD</div><div class="stat-val v-dim" id="m-fsd">OFF</div></div>
      <div class="stat"><div class="stat-lbl">帧率</div><div class="stat-val v-info" id="m-fps">0.0 Hz</div></div>
      <div class="stat"><div class="stat-lbl">Rx / Tx</div><div class="stat-val v-info" id="m-rxtx">0 / 0</div></div>
      <div class="stat"><div class="stat-lbl">运行</div><div class="stat-val v-info" id="m-up">00:00:00</div></div>
    </div>
  </div>
  <div class="card">
    <div class="setting-row">
      <div>
        <div class="setting-name">FSD 总开关</div>
        <div class="setting-desc">开启前需要二次确认</div>
      </div>
      <label class="tgl">
        <input type="checkbox" id="m-fsd-tgl" onchange="toggleFsd()">
        <div class="tgl-track"></div>
      </label>
    </div>
  </div>
  <div class="card">
    <div class="card-title">当前模式</div>
    <div class="diag-grid">
      <div class="diag-item"><span class="lbl">破解</span><span class="v-acc" id="m-hw">--</span></div>
      <div class="diag-item"><span class="lbl">驾驶</span><span class="v-acc" id="m-drive">--</span></div>
      <div class="diag-item"><span class="lbl">速度</span><span class="v-dim" id="m-speed">--</span></div>
      <div class="diag-item"><span class="lbl">防御</span><span class="v-dim" id="m-defense">--</span></div>
    </div>
  </div>
  <div class="card">
    <div class="card-title">异常告警</div>
    <div class="setting-desc" id="m-alert">暂无异常</div>
  </div>
  <div class="card">
    <div class="card-title">未验证 / 实验功能</div>
    <div class="setting-desc">自动换挡、灯光连续爆闪、MAX/V14 模式、AP/EAP 免打扰、FSD 注入增强项。建议优先使用 Auto/HW3/Normal/auto 速度策略。</div>
  </div>
</div>
<!-- Quick Actions -->
<div class="quick-actions">
  <button class="qa-btn qa-fsd" id="qa-fsd" onclick="toggleFsd()">
    <div class="qa-icon" id="qa-fsd-icon">⚡</div>
    <div class="qa-label">FSD 注入</div>
    <div class="qa-status" id="qa-fsd-st">OFF</div>
  </button>
  <button class="qa-btn" onclick="resetStats()">
    <div class="qa-icon">🔄</div>
    <div class="qa-label">重置计数</div>
    <div class="qa-status">RX/TX</div>
  </button>
  <button class="qa-btn qa-danger" onclick="rebootDevice()">
    <div class="qa-icon">🔁</div>
    <div class="qa-label">重启设备</div>
    <div class="qa-status">Reboot</div>
  </button>
</div>
<!-- FSD Quick Toggle -->
<div class="card">
  <div class="card-title">核心激活程序</div>
  <div class="setting-row">
    <div>
      <div class="setting-name">FSD-V14 模式</div>
      <div class="setting-desc" id="ov-fsd-desc">独立切换 MAX/V14 实验 profile，不影响模块总开关</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="ov-fsd-tgl" onchange="toggleV14Mode()">
      <div class="tgl-track"></div>
    </label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name">模块总开关</div>
      <div class="setting-desc">控制当前 FSD 注入模块启停</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="ov-master-tgl" onchange="toggleFsd()">
      <div class="tgl-track"></div>
    </label>
  </div>
</div>

<!-- Stats Grid -->
<div class="card">
<div class="card-title">系统状态</div>
<div class="stats">
  <div class="stat"><div class="stat-lbl">CAN1</div><div class="stat-val v-dim" id="s-can">Offline</div></div>
  <div class="stat"><div class="stat-lbl">CAN2</div><div class="stat-val v-dim" id="s-can2">Idle</div></div>
  <div class="stat"><div class="stat-lbl">CAN1 RX/TX</div><div class="stat-val v-info" id="s-rx">0/0</div></div>
  <div class="stat"><div class="stat-lbl">帧率</div><div class="stat-val v-info" id="s-fps">0.0 Hz</div></div>
  <div class="stat"><div class="stat-lbl">硬件版本</div><div class="stat-val v-acc" id="s-hw">--</div></div>
  <div class="stat"><div class="stat-lbl">速度偏移</div><div class="stat-val v-dim" id="s-soff">0</div></div>
</div>
</div>

<!-- Secondary Stats -->
<div class="card">
<div class="card-title">设备信息</div>
<div class="stats">
  <div class="stat"><div class="stat-lbl">芯片温度</div><div class="stat-val v-dim" id="s-temp">--</div></div>
  <div class="stat"><div class="stat-lbl">TX Errors</div><div class="stat-val v-dim" id="s-txerr">0</div></div>
  <div class="stat"><div class="stat-lbl">跟随距离</div><div class="stat-val v-dim" id="s-fd">--</div></div>
</div>
</div>

	<!-- Phase 1: 车辆OTA状态 -->
	<div class="card">
	<div class="card-title">车辆 OTA 状态</div>
	<div class="stats">
	  <div class="stat"><div class="stat-lbl">OTA 状态</div><div class="stat-val v-acc" id="s-vota">正常</div></div>
	  <div class="stat"><div class="stat-lbl">OTA 确认次数</div><div class="stat-val v-dim" id="s-vota-cnt">0</div></div>
	</div>
	</div>

	<!-- Phase 1: 功耗管理 -->
	<div class="card">
	<div class="card-title">功耗管理</div>
	  <div class="setting-row">
	    <div>
	      <div class="setting-name">自动关机</div>
	      <div class="setting-desc">5分钟无CAN数据自动休眠</div>
	    </div>
	    <label class="tgl">
	      <input type="checkbox" id="ov-auto-shutdown" onchange="toggleAutoShutdown(this.checked)">
	      <div class="tgl-track"></div>
	    </label>
	  </div>
	  <div class="setting-row">
	    <div>
	      <div class="setting-name">WiFi 自动关闭</div>
	      <div class="setting-desc">5分钟无操作关闭网络中转</div>
	    </div>
	    <label class="tgl">
	      <input type="checkbox" id="ov-wifi-auto-off" onchange="toggleWifiAutoOff(this.checked)">
	      <div class="tgl-track"></div>
	    </label>
	  </div>
	</div>
    </div>

    <!-- Page 2: Hardware Config -->
    <div class="page" id="pg-hardware">
<div class="page-title">破解模式</div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl">UI 显示状态</div><div class="val" id="st-hw-ui">Auto</div></div>
  <div class="status-chip"><div class="lbl">NVS 持久化状态</div><div class="val" id="st-hw-nvs">--</div></div>
  <div class="status-chip"><div class="lbl">实际 CAN 运行状态</div><div class="val" id="st-hw-run">--</div></div>
</div>
<!-- HW Version Selection -->
<div class="card">
  <div class="card-title">FSD 智能破解模式</div>
  <div class="card-subtitle">选择车辆自动驾驶硬件版本，Auto 会交给固件现有探测逻辑</div>
  <div class="sel-cards c4" id="hw-cards">
    <div class="sel-card active" onclick="setHW(3)">
      <div class="sel-lbl">推荐</div>
      <div class="sel-name">Auto</div>
    </div>
    <div class="sel-card" onclick="setHW(0)">
      <div class="sel-lbl">旧版</div>
      <div class="sel-name">Legacy</div>
    </div>
    <div class="sel-card" onclick="setHW(1)">
      <div class="sel-lbl">第三代</div>
      <div class="sel-name">HW3.0</div>
    </div>
    <div class="sel-card" onclick="setHW(2)">
      <div class="sel-lbl">第四代</div>
      <div class="sel-name">HW4.0</div>
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

    <!-- Page 3: Drive Mode -->
    <div class="page" id="pg-drive">
<div class="page-title">驾驶模式</div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl">UI 显示状态</div><div class="val" id="st-drive-ui">Normal</div></div>
  <div class="status-chip"><div class="lbl">NVS 持久化状态</div><div class="val" id="st-drive-nvs">--</div></div>
  <div class="status-chip"><div class="lbl">实际 CAN 运行状态</div><div class="val" id="st-drive-run">--</div></div>
</div>
<div class="card">
  <div class="card-title">驾驶风格</div>
  <div class="card-subtitle">映射到 /drive_profile，保留 Auto/Sloth/Chill/Normal/Hurry/MAX 六档</div>
  <div class="sel-cards c3" id="drive-cards">
    <div class="sel-card" onclick="setDriveMode('auto')"><div class="sel-lbl">推荐</div><div class="sel-name">Auto</div></div>
    <div class="sel-card" onclick="setDriveMode('sloth')"><div class="sel-lbl">低速</div><div class="sel-name">Sloth</div></div>
    <div class="sel-card" onclick="setDriveMode('chill')"><div class="sel-lbl">舒适</div><div class="sel-name">Chill</div></div>
    <div class="sel-card active" onclick="setDriveMode('normal')"><div class="sel-lbl">标准</div><div class="sel-name">Normal</div></div>
    <div class="sel-card" onclick="setDriveMode('hurry')"><div class="sel-lbl">积极</div><div class="sel-name">Hurry</div></div>
    <div class="sel-card" onclick="setDriveMode('max')"><div class="sel-lbl">最大</div><div class="sel-name">MAX <span class="exp-badge">实验</span></div></div>
  </div>
  <div class="mode-note">当前选择：<span id="drive-current">Normal</span></div>
</div>
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
    <button class="btn btn-danger" onclick="rebootDevice()" style="flex:1">重启设备</button>
    <button class="btn" onclick="resetStats()" style="flex:1">重置计数</button>
  </div>
</div>
    </div>

    <!-- Page 4: Speed Offset -->
    <div class="page" id="pg-speed">
<div class="page-title">速度偏移</div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl">UI 显示状态</div><div class="val" id="st-speed-ui">待同步</div></div>
  <div class="status-chip"><div class="lbl">NVS 持久化状态</div><div class="val" id="st-speed-nvs">读取中</div></div>
  <div class="status-chip"><div class="lbl">实际 CAN/网络运行状态</div><div class="val" id="st-speed-run">待检测</div></div>
</div>
<div class="card">
  <div class="card-title">速度偏移模式</div>
  <div class="card-subtitle">选择固定百分比 / 自动偏移 / 自定义偏移，配置会同步到 /speed_strategy 与 /speed_custom</div>
  <div class="sel-cards c3" id="speed-mode-tabs" style="margin-bottom:12px">
    <div class="sel-card" onclick="showSpeedMode('fixed')"><div class="sel-lbl">模式</div><div class="sel-name">固定百分比</div></div>
    <div class="sel-card active" onclick="showSpeedMode('auto')"><div class="sel-lbl">模式</div><div class="sel-name">自动偏移</div></div>
    <div class="sel-card" onclick="showSpeedMode('custom')"><div class="sel-lbl">模式</div><div class="sel-name">自定义偏移</div></div>
  </div>
  <div class="mode-note">当前策略：<span id="speed-current">auto</span></div>
</div>

<!-- Realtime Offset -->
<div class="card">
  <div class="card-title">实时偏移</div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl">速度限制 speedLimit</span><span class="v-info" id="sp-limit">--</span></div>
    <div class="diag-item"><span class="lbl">实际偏移 actOffset</span><span class="v-acc" id="sp-act-offset">--</span></div>
    <div class="diag-item"><span class="lbl">生效模式</span><span class="v-dim" id="sp-active-mode">--</span></div>
    <div class="diag-item"><span class="lbl">Wire Encoding</span><span class="v-dim" id="sp-wire">--</span></div>
    <div class="diag-item"><span class="lbl">Fused Raw</span><span class="v-dim" id="sp-raw">--</span></div>
    <div class="diag-item"><span class="lbl">Stock Offset</span><span class="v-dim" id="sp-stock">--</span></div>
  </div>
</div>

<div class="card" id="speed-panel-fixed">
  <div class="card-title">固定百分比</div>
  <div class="card-subtitle">所有速度限制统一使用同一个偏移百分比</div>
  <div class="sel-cards c3" id="speed-fixed-pct" style="margin-bottom:10px">
    <div class="sel-card" onclick="setSpeedFixedPct(0)"><div class="sel-lbl">固定</div><div class="sel-name">0%</div></div>
    <div class="sel-card" onclick="setSpeedFixedPct(10)"><div class="sel-lbl">固定</div><div class="sel-name">10%</div></div>
    <div class="sel-card" onclick="setSpeedFixedPct(20)"><div class="sel-lbl">固定</div><div class="sel-name">20%</div></div>
    <div class="sel-card active" onclick="setSpeedFixedPct(30)"><div class="sel-lbl">固定</div><div class="sel-name">30%</div></div>
    <div class="sel-card" onclick="setSpeedFixedPct(40)"><div class="sel-lbl">固定</div><div class="sel-name">40%</div></div>
    <div class="sel-card" onclick="setSpeedFixedPct(50)"><div class="sel-lbl">固定</div><div class="sel-name">50%</div></div>
  </div>
  <div class="mode-note">保存时会 POST /speed_strategy=fixed，并通过 /speed_custom 写入 manualPct。</div>
</div>

<div class="card" id="speed-panel-auto" style="display:none">
  <div class="card-title">自动偏移算法</div>
  <div class="card-subtitle">固件按当前 speedLimit 自动选择偏移比例，适合日常使用</div>
  <table class="tbl">
    <thead><tr><th>速度限制</th><th>自动偏移</th></tr></thead>
    <tbody>
      <tr><td>≤ 40 km/h</td><td>+50%，封顶 60</td></tr>
      <tr><td>≤ 60 km/h</td><td>+50%，封顶 90</td></tr>
      <tr><td>≤ 90 km/h</td><td>+30%，封顶 117</td></tr>
      <tr><td>≤ 110 km/h</td><td>+20%，封顶 132</td></tr>
      <tr><td>> 110 km/h</td><td>+10%，封顶 132</td></tr>
    </tbody>
  </table>
  <button class="btn" onclick="setSpeedStrategy('auto')" style="margin-top:12px;width:100%">启用自动偏移</button>
</div>

<div class="card" id="speed-panel-custom" style="display:none">
  <div class="card-title">自定义 4 区间偏移</div>
  <div class="card-subtitle">填写每个速度区间的偏移百分比，范围 0-50%</div>
  <div class="setting-row"><div><div class="setting-name">0-50 km/h</div><div class="setting-desc">低速区间 cp1</div></div><input class="inp" type="number" min="0" max="50" id="speed-cp1" value="30" style="width:86px"></div>
  <div class="setting-row"><div><div class="setting-name">51-70 km/h</div><div class="setting-desc">城市快速路 cp2</div></div><input class="inp" type="number" min="0" max="50" id="speed-cp2" value="20" style="width:86px"></div>
  <div class="setting-row"><div><div class="setting-name">71-100 km/h</div><div class="setting-desc">高速巡航 cp3</div></div><input class="inp" type="number" min="0" max="50" id="speed-cp3" value="10" style="width:86px"></div>
  <div class="setting-row"><div><div class="setting-name">101+ km/h</div><div class="setting-desc">高限速区间 cp4</div></div><input class="inp" type="number" min="0" max="50" id="speed-cp4" value="10" style="width:86px"></div>
  <button class="btn" onclick="saveSpeedCustom()" style="width:100%;margin-top:10px">保存自定义偏移</button>
</div>

<!-- Encoding -->

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
    </div>

    <!-- Page 5: CAN2 Control -->
    <div class="page" id="pg-bus2">
<div class="page-title">CAN2 控制 <span class="exp-badge">未验证/实验</span></div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl">UI 显示状态</div><div class="val" id="st-light-ui">待同步</div></div>
  <div class="status-chip"><div class="lbl">NVS 持久化状态</div><div class="val" id="st-light-nvs">读取中</div></div>
  <div class="status-chip"><div class="lbl">实际 CAN/网络运行状态</div><div class="val" id="st-light-run">待检测</div></div>
</div>
<!-- CAN2 Status -->
<div class="stats">
  <div class="stat"><div class="stat-lbl">CAN2 状态</div><div class="stat-val v-dim" id="b2-status">Offline</div></div>
  <div class="stat"><div class="stat-lbl">CAN2 RX</div><div class="stat-val v-info" id="b2-rx">0</div></div>
  <div class="stat"><div class="stat-lbl">已发现 ID</div><div class="stat-val v-acc" id="b2-ids">0</div></div>
</div>

<!-- CAN2 ID Table -->
<div class="card">
  <div class="card-title">CAN2 ID 列表 <span style="color:var(--tx3);font-size:11px;font-weight:400" id="b2-count">(0)</span></div>
  <div style="max-height:200px;overflow-y:auto">
    <table class="tbl">
      <thead><tr><th>ID</th><th>DLC</th><th>数据</th><th>计数</th></tr></thead>
      <tbody id="b2-rows"></tbody>
    </table>
  </div>
</div>

<!-- Stalk Test -->
<div class="card">
  <div class="card-title">CAN2 控制</div>
  <div class="card-subtitle">映射到 /lighting_config，并用 CAN2 stalk_test 执行爆闪序列</div>
  <div class="setting-row">
    <div>
      <div class="setting-name">爆闪开关 <span class="exp-badge">实验</span></div>
      <div class="setting-desc">启用后按配置执行多次拨杆注入</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="light-enabled-tgl" onchange="saveLightingConfig()">
      <div class="tgl-track"></div>
    </label>
  </div>
  <div class="sel-cards c4" id="light-preset" style="margin-bottom:10px">
    <div class="sel-card active" onclick="setLightPreset(3)"><div class="sel-lbl">次数</div><div class="sel-name">3</div></div>
    <div class="sel-card" onclick="setLightPreset(5)"><div class="sel-lbl">次数</div><div class="sel-name">5</div></div>
    <div class="sel-card" onclick="setLightPreset(7)"><div class="sel-lbl">次数</div><div class="sel-name">7</div></div>
    <div class="sel-card" onclick="setLightPreset(10)"><div class="sel-lbl">次数</div><div class="sel-name">10</div></div>
  </div>
  <div class="sel-cards c3" id="light-frequency" style="margin-bottom:10px">
    <div class="sel-card" onclick="setLightFrequency('slow')"><div class="sel-lbl">频率</div><div class="sel-name">slow</div></div>
    <div class="sel-card active" onclick="setLightFrequency('medium')"><div class="sel-lbl">频率</div><div class="sel-name">medium</div></div>
    <div class="sel-card" onclick="setLightFrequency('fast')"><div class="sel-lbl">频率</div><div class="sel-name">fast</div></div>
  </div>
  <div class="sel-cards c3" id="rear-fog-strategy" style="margin-bottom:10px">
    <div class="sel-card active" onclick="setRearFogStrategy('off')"><div class="sel-lbl">后雾灯</div><div class="sel-name">off</div></div>
    <div class="sel-card" onclick="setRearFogStrategy('strobe')"><div class="sel-lbl">后雾灯</div><div class="sel-name">strobe</div></div>
    <div class="sel-card" onclick="setRearFogStrategy('continuous')"><div class="sel-lbl">后雾灯</div><div class="sel-name">continuous <span class="exp-badge">实验</span></div></div>
  </div>
  <div style="display:flex;gap:8px;margin-bottom:10px">
    <button class="btn btn-outline" onclick="stalkTest('PULL')" style="flex:1">PULL (闪光)</button>
    <button class="btn btn-outline" onclick="stalkTest('PUSH')" style="flex:1">PUSH (远光)</button>
  </div>
  <div style="display:flex;gap:8px;margin-bottom:10px">
    <button class="btn" onclick="strobeTest('PULL')" style="flex:1">执行爆闪</button>
    <button class="btn btn-outline" onclick="strobeTest('PUSH')" style="flex:1">远光序列</button>
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
      <div class="setting-desc">启用 0x339 持续注入 CAN2</div>
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
<div class="page-title">FSD防御 <span class="exp-badge">部分实验</span></div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl">UI 显示状态</div><div class="val" id="st-defense-ui">待同步</div></div>
  <div class="status-chip"><div class="lbl">NVS 持久化状态</div><div class="val" id="st-defense-nvs">读取中</div></div>
  <div class="status-chip"><div class="lbl">实际 CAN/网络运行状态</div><div class="val" id="st-defense-run">待检测</div></div>
</div>
<!-- Slew Toggle -->
<div class="card">
  <div class="card-title">FSD 防封保护</div>
  <div class="card-subtitle">保留现有 slew rate 保护接口，扩展为截图同款防御页</div>
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
  <div class="setting-row">
    <div>
      <div class="setting-name">仿生方向盘 <span class="exp-badge">实验</span></div>
      <div class="setting-desc">保存到防御配置，后续接入方向盘仿真逻辑</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-bionic-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name">声音警告抑制</div>
      <div class="setting-desc">映射到 handler isaChimeSuppress</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-sound-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name">速度免打扰 <span class="exp-badge">实验</span></div>
      <div class="setting-desc">保存到防御配置，避免速度策略频繁扰动</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-speed-nd-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name">AP/EAP 兼容 <span class="exp-badge">未验证</span></div>
      <div class="setting-desc">保留 AP/EAP 兼容策略位</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-apeap-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
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
<div class="page-title">OTA升级</div>
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
<div class="page-title">网络设置</div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl">UI 显示状态</div><div class="val" id="st-net-ui">待同步</div></div>
  <div class="status-chip"><div class="lbl">NVS 持久化状态</div><div class="val" id="st-net-nvs">读取中</div></div>
  <div class="status-chip"><div class="lbl">实际 CAN/网络运行状态</div><div class="val" id="st-net-run">待检测</div></div>
</div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl">DNS UI 显示状态</div><div class="val" id="st-dns-ui">待同步</div></div>
  <div class="status-chip"><div class="lbl">DNS NVS 持久化状态</div><div class="val" id="st-dns-nvs">读取中</div></div>
  <div class="status-chip"><div class="lbl">DNS 实际运行状态</div><div class="val" id="st-dns-run">待检测</div></div>
</div>
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
  <div class="setting-row">
    <div class="setting-name">AP SSID</div>
    <input class="inp" id="ap-ssid-input" style="width:180px" placeholder="热点名称">
  </div>
  <div class="setting-row">
    <div class="setting-name">AP 密码</div>
    <input class="inp" type="password" id="ap-pass-input" style="width:180px" placeholder="8-64 位">
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name">隐藏热点</div>
      <div class="setting-desc">保存后重启生效</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="ap-hidden-tgl">
      <div class="tgl-track"></div>
    </label>
  </div>
  <div style="display:flex;gap:6px;margin-top:8px">
    <button class="btn btn-sm" onclick="saveHotspot(false)">保存热点</button>
    <button class="btn btn-sm btn-outline" onclick="saveHotspot(true)">保存并重启</button>
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
    <button class="btn btn-sm btn-outline" onclick="testRelayWifi()">测试连接</button>
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
  <div class="card-title">黑名单 / DNS 过滤规则</div>
  <div class="card-subtitle">保守模式使用完整 12 条规则，激进模式使用最小 9 条规则</div>
  <div style="margin-bottom:8px">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px">
      <span style="color:var(--err);font-weight:500;font-size:11px">黑名单</span>
      <span style="color:var(--tx3);font-size:10px" id="dns-bl-cnt">0 域名</span>
    </div>
    <textarea class="inp" id="dns-blacklist" rows="6" placeholder="每行一个域名"></textarea>
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

    <!-- Page 10: CAN Tools -->
    <div class="page" id="pg-can">
<div class="page-title">CAN工具</div>
<!-- CAN Pin Config -->
<div class="card">
  <div class="card-title">CAN 引脚配置</div>
  <div class="card-subtitle">CAN1 TWAI + CAN2 MCP2515 SPI</div>
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
         driveProfile:3,
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
var pollMs = 1000;
var pollTick = null;
var sniffPaused = false;
var sniffFrames = [];
var recActive = false;
var canTab = 'sniffer';
var lightPreset = 3;
var lightFrequency = 'medium';
var rearFogStrategy = 'off';
var DNS_PROFILES = {
  conservative: {
    blacklist: 'api-prd.vn.cloud.tesla.cn\nhermes-x2-api.prd.vn.cloud.tesla.cn\nsignaling.vn.cloud.tesla.cn\nhermes-prd.vn.cloud.tesla.cn\nhermes-stream-prd.vn.cloud.tesla.cn\ntelemetry-prd.vn.cloud.tesla.cn\ntelemetry.tesla.cn\napigateway-x2-trigger.tesla.cn\nfleet-api.prd.cn.vn.cloud.tesla.cn\nfirmware.tesla.cn\nlog.tesla.cn\nvehicle-files.prd.cnn1.vn.cloud.tesla.cn',
    whitelist: 'connman.vn.cloud.tesla.cn\nnav-prd-maps.tesla.cn\nmaps-cn-prd.go.tesla.services\nmedia-server-me.tesla.cn'
  },
  aggressive: {
    blacklist: 'telemetry.tesla.cn\ntelemetry-prd.vn.cloud.tesla.cn\nhermes-prd.vn.cloud.tesla.cn\nfleet-api.prd.cn.vn.cloud.tesla.cn\nfirmware.tesla.cn\nlog.tesla.cn\nvehicle-files.prd.cnn1.vn.cloud.tesla.cn\nhermes-x2-api.prd.vn.cloud.tesla.cn\nsignaling.vn.cloud.tesla.cn',
    whitelist: 'connman.vn.cloud.tesla.cn\nnav-prd-maps.tesla.cn\nmaps-cn-prd.go.tesla.services\nmedia-server-me.tesla.cn'
  }
};
var OLD_DNS_BLACKLIST = 'tesla.cn\ntesla.com\nteslamotors.com\ntesla.services';

// ── Utilities ──────────────────────────────────────────────
function $(id){return document.getElementById(id)}
function setText(id,txt){var e=$(id);if(e)e.textContent=txt}
function setHtml(id,html){var e=$(id);if(e)e.innerHTML=html}
function setCls(id,cls){var e=$(id);if(e)e.className=cls}
function escHtml(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')}
function escAttr(s){return String(s).replace(/\\/g,'\\\\').replace(/'/g,"\\'").replace(/"/g,'\\"').replace(/</g,'\\x3c').replace(/>/g,'\\x3e')}
function showToast(msg,ok){var t=$('toast');if(t){t.textContent=msg;t.classList.toggle('ok',!!ok);t.classList.add('show')}}
function hideToast(){var t=$('toast');if(t){t.classList.remove('show');t.classList.remove('ok')}}
function toHex(n){return '0x'+('0000'+n.toString(16).toUpperCase()).slice(-4)}
function normLines(s){return String(s||'').split(/\r?\n/).map(function(x){return x.trim().toLowerCase()}).filter(Boolean).join('\n')}
function countLines(s){return normLines(s).split('\n').filter(Boolean).length}
function setStatusTriplet(prefix,ui,nvs,run,state){
  setText('st-'+prefix+'-ui',ui||'--');
  setText('st-'+prefix+'-nvs',nvs||'--');
  setText('st-'+prefix+'-run',run||'--');
  var cls=state==='err'?'s-err':(state==='warn'?'s-warn':'s-ok');
  ['ui','nvs','run'].forEach(function(part){
    var el=$('st-'+prefix+'-'+part);
    if(el)el.className='val '+cls;
  });
}
function hwLabel(hw){return hw>=0&&hw<3?HW_NAMES[hw]:'Auto'}
function driveLabel(sp,spa){return spa?'Auto':(sp===0?'Chill':(sp===2?'Hurry':'Normal'))}
function experimentSummary(){
  var flags=[];
  var drive=$('drive-current');
  if(drive&&drive.textContent==='MAX')flags.push('MAX/V14');
  var light=$('light-enabled-tgl');
  if(light&&light.checked)flags.push('灯光爆闪');
  var apeap=$('def-apeap-tgl');
  if(apeap&&apeap.checked)flags.push('AP/EAP');
  var bionic=$('def-bionic-tgl');
  if(bionic&&bionic.checked)flags.push('FSD增强');
  return flags.length?('实验: '+flags.join('/')):'实验项需实车验证';
}
function fmtUp(sec){
  var h=Math.floor(sec/3600),m=Math.floor(sec%3600/60),s=sec%60;
  return (h<10?'0':'')+h+':'+(m<10?'0':'')+m+':'+(s<10?'0':'')+s;
}

// ── I18N ───────────────────────────────────────────────────
var I18N={
  // Sidebar
  '模块配置':'Module','激活模式':'Activation Mode','驾驶模式':'Drive Mode',
  '速度偏移':'Speed Offset','CAN2控制':'CAN2 Control','FSD防御':'FSD Defense',
  'OTA升级':'OTA Update',
  '网络设置':'Network','CAN工具':'CAN Tools',
  // Top bar
  '已连接':'Connected','未连接':'Disconnected','连接丢失':'Connection Lost',
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
  // CAN2
  'CAN2 状态':'CAN2 Status','CAN2 RX':'CAN2 RX','已发现 ID':'Found IDs',
  'CAN2 ID 列表':'CAN2 CAN IDs',
  '灯光注入测试':'Lighting Test','模拟方向盘拨杆操作':'Simulate stalk operation',
  '持续时间 (ms)':'Duration (ms)','状态':'Status','空闲':'Idle',
  '启用 0x339 持续注入 CAN2':'Enable 0x339 continuous CAN2 injection',
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
  var zhTexts=['模块配置','激活模式','驾驶模式','速度偏移','CAN2控制','FSD防御','OTA升级','网络设置','CAN工具'];
  for(var i=0;i<navs.length;i++){
    var icon=navs[i].querySelector('.nav-icon');
    var iconHtml=icon?icon.outerHTML:'';
    navs[i].innerHTML=iconHtml+(lang==='zh'?zhTexts[i]:I18N[zhTexts[i]]);
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
    if($('theme-btn'))$('theme-btn').textContent='☀';
    if($('mobile-theme-btn'))$('mobile-theme-btn').textContent='☀ 白天';
  }else{
    r.setProperty('--sidebar-bg','#ffffff');
    r.setProperty('--main-bg','#f4f4f8');
    r.setProperty('--card-bg','#ffffff');
    r.setProperty('--card-bg-alt','#f3f4f8');
    r.setProperty('--tx1','#1f2329');r.setProperty('--tx2','#4b5563');
    r.setProperty('--tx3','#71717a');r.setProperty('--border','#d8dbe2');
    r.setProperty('--header-bg','#ffffff');
    if($('theme-btn'))$('theme-btn').textContent='🌙';
    if($('mobile-theme-btn'))$('mobile-theme-btn').textContent='🌙 夜间';
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
  if(pageId==='pg-drive')loadDriveProfile();
  if(pageId==='pg-bus2')pollCAN2();
  if(pageId==='pg-bus2')loadLightingConfig();
  if(pageId==='pg-speed')loadSpeedStrategy();
  if(pageId==='pg-defense')loadDefenseConfig();
  if(pageId==='pg-network'){pollWifiStatus();pollGatewayStatus();loadGatewayDns();loadGatewayBlocked();}
}
function openSidebar(){$('sidebar').classList.add('open');$('overlay').classList.add('active')}
function closeSidebar(){$('sidebar').classList.remove('open');$('overlay').classList.remove('active')}

// ── Fetch Helper ───────────────────────────────────────────
var _fetchErrCount=0;
async function fetchJson(url){
  try{
    var r=await fetch(url);
    if(!r.ok)throw new Error(r.status);
    var d=await r.json();
    if(_fetchErrCount>0){_fetchErrCount=0;hideToast()}
    return d;
  }catch(e){_fetchErrCount++;if(_fetchErrCount>=3)showToast(T('连接丢失')+' ('+_fetchErrCount+')');return null}
}
async function postForm(url,data){
  try{
    var body=[];
    for(var k in data)body.push(encodeURIComponent(k)+'='+encodeURIComponent(data[k]));
    var r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body.join('&')});
    var txt=await r.text();
    var payload=null;
    if(txt){
      try{payload=JSON.parse(txt)}catch(e){}
    }
    if(!r.ok){
      var msg=payload&&(payload.error||payload.msg)?(payload.error||payload.msg):('HTTP '+r.status);
      throw new Error(msg);
    }
    hideToast();
    return payload||{ok:true,text:txt};
  }catch(e){
    showToast((e&&e.message)?e.message:T('请求失败'));
    throw e;
  }
}

// ── Polling ────────────────────────────────────────────────
async function poll(){
  var d=await fetchJson('/status');
  if(!d){
    setCls('tb-status','topbar-badge badge-err');
    setText('tb-status',T('未连接'));
    return;
  }
  S.hw=d.hw;S.ci=d.ci;S.sp=d.sp;S.spa=d.spAuto;S.can=d.can;S.ia=d.ia;S.driveProfile=d.driveProfile!==undefined?d.driveProfile:S.driveProfile;
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
  setCls('tb-dot-status','topbar-dot '+(d.can?'ok':'err'));
  setCls('tb-fsd','topbar-badge '+(d.ia?'badge-ok':'badge-warn'));
  setText('tb-fsd',d.ia?'FSD ON':'FSD OFF');
  setCls('tb-dot-fsd','topbar-dot '+(d.ia?'ok':'warn'));
  setText('tb-exp',experimentSummary());
  setText('tb-up',fmtUp(d.up||0));

  // Overview
  var ovTgl=$('ov-fsd-tgl');
  if(ovTgl)ovTgl.checked=(S.driveProfile===5);
  var masterTgl=$('ov-master-tgl');
  if(masterTgl)masterTgl.checked=!!d.ci;
  var mFsdTgl=$('m-fsd-tgl');
  if(mFsdTgl)mFsdTgl.checked=!!d.ci;
  setCls('ov-v14','stat-val '+(S.driveProfile===5?'v-warn':'v-dim'));
  setText('ov-v14',S.driveProfile===5?'MAX/V14':'待机');
  setText('ov-hw',hwLabel(d.hw));
  setCls('ov-can','stat-val '+(d.can?'v-ok':'v-err'));
  setText('ov-can',(d.can?'CAN1 Online':'CAN1 Offline')+' / '+($('s-can2')&&$('s-can2').textContent?$('s-can2').textContent:'CAN2 --'));
  setText('ov-up',fmtUp(d.up||0));
  setCls('m-can','stat-val '+(d.can?'v-ok':'v-err'));
  setText('m-can',d.can?'Online':'Offline');
  setCls('m-can2','stat-val '+(($('s-can2')&&$('s-can2').textContent==='Online')?'v-ok':'v-dim'));
  setText('m-can2',($('s-can2')&&$('s-can2').textContent)?$('s-can2').textContent:'--');
  setCls('m-fsd','stat-val '+(d.ci?'v-ok':'v-dim'));
  setText('m-fsd',d.ci?'ON':'OFF');
  setText('m-fps',d.fps.toFixed(1)+' Hz');
  setText('m-rxtx',(d.rx||0)+' / '+(d.tx||0));
  setText('m-up',fmtUp(d.up||0));
  setText('m-hw',hwLabel(d.hw));
  setText('m-drive',driveLabel(d.sp,d.spAuto));
  setText('m-speed',d.soff!==undefined?d.soff:'--');
  setText('m-defense',d.hw3OffsetSlew?'ON':'OFF');
  // Phase 1: OTA + 功耗管理状态
  setCls('s-vota','stat-val '+(d.vehicleOta?'v-err':'v-ok'));
  setText('s-vota',d.vehicleOta?'OTA 进行中':'正常');
  var otaCnt=$('s-vota-cnt');
  if(otaCnt) fetchJson('/vehicle_ota_status').then(function(o){if(o)setText('s-vota-cnt',o.otaConfirmCount||0)}).catch(function(){});
  var ashut=$('ov-auto-shutdown');if(ashut)ashut.checked=!!d.autoShutdown;
  var woff=$('ov-wifi-auto-off');if(woff)woff.checked=!!d.wifiAutoOff;
  var alertText=[];
  if(!d.can)alertText.push('CAN 离线');
  if(d.txerr)alertText.push('TX Err '+d.txerr);
  if(d.apGate)alertText.push('AP Gate 等待');
  setText('m-alert',alertText.length?alertText.join(' / '):'暂无异常');
  setCls('s-can','stat-val '+(d.can?'v-ok':'v-err'));
  setText('s-can',d.can?'Online':'Offline');
  setText('s-rx',(d.rx||0)+'/'+(d.tx||0));
  setText('s-tx',d.tx||0);
  setText('s-fps',d.fps.toFixed(1)+' Hz');
  setText('s-hw',hwLabel(d.hw));
  setText('s-soff',d.soff||0);
  // Temp from /system_status is separate; use eflg field as proxy
  setText('s-txerr',d.txerr||0);
  setText('s-fd',d.fd||0);
  setStatusTriplet('module',d.ci?'FSD ON':'FSD OFF','启动保存: '+(d.ci?'ON':'OFF'),d.can?'CAN Online':'CAN Offline',d.can?'ok':'err');
  setStatusTriplet('hw',hwLabel(d.hw),'mode_hw: '+hwLabel(d.hw),(d.can?'CAN运行 / ':'CAN离线 / ')+hwLabel(d.hw),d.can?'ok':'warn');
  setStatusTriplet('speed','偏移 '+(d.soff!==undefined?d.soff:'--'),driveLabel(d.sp,d.spAuto),d.fusedSpeedLimitKph?('Fused '+d.fusedSpeedLimitKph+' kph'):'CAN未给出速度',d.can?'ok':'warn');
  setStatusTriplet('defense',d.hw3OffsetSlew?'防御 ON':'防御 OFF','slew '+(d.hw3OffsetSlew?'ON':'OFF'),'触发 '+(d.hw3SlewCount||0)+' / offset '+(d.hw3OffsetLast!==undefined?d.hw3OffsetLast:'--'),d.hw3OffsetSlew?'ok':'warn');

  // FSD page toggle
  updateFsdToggle(d.ci);

  // Quick action FSD button
  var qaBtn=$('qa-fsd');var qaSt=$('qa-fsd-st');
  if(qaBtn){qaBtn.classList.toggle('active',!!d.ci)}
  if(qaSt){qaSt.textContent=d.ci?'ON':'OFF';qaSt.style.color=d.ci?'var(--ok)':'var(--err)'}

  // Boot toggle
  var bt=$('fsd-boot-tgl');
  if(bt)bt.checked=!!d.ci;

  // HW page
  updateHwCards(d.hw);
  updateProfileCards(d.sp);
  updateDriveCards(d.sp,d.spAuto);

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
  if(cards){
    var items=cards.querySelectorAll('.sel-card');
    var map=[3,0,1,2];
    for(var i=0;i<items.length;i++){
      items[i].classList.toggle('active',map[i]===hw);
    }
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

function updateDriveCards(sp,spa){
  var cards=$('drive-cards');
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  var active=spa?'auto':(sp===0?'chill':(sp===2?'hurry':'normal'));
  updateDriveCardsByMode(active);
}

function updateDriveCardsByMode(active){
  var cards=$('drive-cards');
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  var modes=['auto','sloth','chill','normal','hurry','max'];
  for(var i=0;i<items.length;i++)items[i].classList.toggle('active',modes[i]===active);
  var label={auto:'Auto',sloth:'Sloth',chill:'Chill',normal:'Normal',hurry:'Hurry',max:'MAX'}[active]||'Normal';
  setText('drive-current',label);
}

async function loadDriveProfile(){
  var d=await fetchJson('/drive_profile');
  if(!d)return;
  var mode=String(d.profile||'Normal').toLowerCase();
  S.driveProfile=d.value!==undefined?d.value:S.driveProfile;
  updateDriveCardsByMode(mode);
  var v14=$('ov-fsd-tgl');if(v14)v14.checked=(mode==='max');
  setStatusTriplet('drive',mode,mode,mode==='max'?'MAX/V14 需实车验证':'配置已同步',mode==='max'?'warn':'ok');
}

var speedStrategyState='auto';
var speedManualPct=30;

function speedPctVal(id,def){
  var el=$(id);
  var v=el?parseInt(el.value,10):def;
  if(isNaN(v))v=def;
  if(v<0)v=0;
  if(v>50)v=50;
  if(el)el.value=String(v);
  return v;
}

function updateSpeedFixedCards(pct){
  var cards=$('speed-fixed-pct');
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  var values=[0,10,20,30,40,50];
  for(var i=0;i<items.length;i++)items[i].classList.toggle('active',values[i]===pct);
}

function showSpeedMode(strategy){
  speedStrategyState=strategy||'auto';
  var tabs=$('speed-mode-tabs');
  if(tabs){
    var items=tabs.querySelectorAll('.sel-card');
    var values=['fixed','auto','custom'];
    for(var i=0;i<items.length;i++)items[i].classList.toggle('active',values[i]===speedStrategyState);
  }
  var fixed=$('speed-panel-fixed'),auto=$('speed-panel-auto'),custom=$('speed-panel-custom');
  if(fixed)fixed.style.display=speedStrategyState==='fixed'?'block':'none';
  if(auto)auto.style.display=speedStrategyState==='auto'?'block':'none';
  if(custom)custom.style.display=speedStrategyState==='custom'?'block':'none';
  setText('speed-current',speedStrategyState);
}

function updateSpeedPage(d){
  var enc=$('hw3-enc');
  if(enc)enc.value=String(d.hw3WireEncoding||0);
  var speedLimit=(d.speedLimit!==undefined&&d.speedLimit>0)?d.speedLimit:(d.fusedSpeedLimitKph||0);
  setText('sp-limit',speedLimit?speedLimit+' kph':'--');
  setText('sp-act-offset',d.actOffset!==undefined?d.actOffset+' kph':'--');
  setText('sp-active-mode',speedStrategyState);
  setText('sp-wire',d.hw3WireEncoding!==undefined?d.hw3WireEncoding:'--');
  setText('sp-stock',d.hw3StockOffset!==undefined?d.hw3StockOffset+' kph':'--');
  setText('sp-raw',d.fusedSpeedLimitRaw!==undefined?d.fusedSpeedLimitRaw:'--');
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

async function loadDefenseConfig(){
  var d=await fetchJson('/defense_config');
  if(!d)return;
  var tgl=$('hw3-slew-tgl');
  if(tgl)tgl.checked=!!d.enabled;
  var bio=$('def-bionic-tgl');if(bio)bio.checked=!!d.bionic_steering;
  var sound=$('def-sound-tgl');if(sound)sound.checked=!!d.sound_warning_suppression;
  var nd=$('def-speed-nd-tgl');if(nd)nd.checked=!!d.speed_no_disturb;
  var apeap=$('def-apeap-tgl');if(apeap)apeap.checked=!!d.ap_eap_compatible;
  setText('def-status',d.enabled?T('保护已启用'):T('保护未启用'));
  var dot=$('def-dot');if(dot)dot.className='status-dot '+(d.enabled?'ok':'err');
  var exp=(d.bionic_steering||d.speed_no_disturb||d.ap_eap_compatible);
  setStatusTriplet('defense',d.enabled?'防御 ON':'防御 OFF',
    'NVS '+(d.enabled?'ON':'OFF')+(exp?' / 含实验项':''),
    exp?'实验项需实车验证':'等待 /status 运行确认',
    exp?'warn':(d.enabled?'ok':'warn'));
  setText('tb-exp',experimentSummary());
}

// ── FSD Toggle ─────────────────────────────────────────────
async function toggleFsd(){
  var next=S.ci?0:1;
  if(next&&!confirm('确认开启 FSD 注入？')){poll();return}
  try{await postForm('/config',{can:next?'1':'0',force:next?'1':'0'});}
  catch(e){return}
  S.ci=!!next;
  var masterTgl=$('ov-master-tgl');
  if(masterTgl)masterTgl.checked=S.ci;
  var mFsdTgl=$('m-fsd-tgl');
  if(mFsdTgl)mFsdTgl.checked=S.ci;
  updateFsdToggle(S.ci);
  poll();
}

async function toggleV14Mode(){
  var t=$('ov-fsd-tgl');
  var enable=!!(t&&t.checked);
  if(enable&&!confirm('确认开启 MAX/V14 实验模式？')){if(t)t.checked=false;return}
  try{await postForm('/drive_profile',{profile:enable?'max':'normal'});}
  catch(e){if(t)t.checked=!enable;return}
  S.driveProfile=enable?5:3;
  updateDriveCardsByMode(enable?'max':'normal');
  setCls('ov-v14','stat-val '+(enable?'v-warn':'v-dim'));
  setText('ov-v14',enable?'MAX/V14':'待机');
  setText('tb-exp',experimentSummary());
}

async function resetStats(){
  try{
    await postForm('/reset_stats',{});
    showToast(T('已保存')||'OK',true);
    setTimeout(function(){hideToast();poll()},500);
  }catch(e){}
}

async function rebootDevice(){
  if(!confirm(T('确认重启设备？')||'Reboot device?'))return;
  try{
    await postForm('/reboot',{});
    showToast(T('重启设备')||'Rebooting...');
  }catch(e){}
}

// ── HW Selection ───────────────────────────────────────────
async function setHW(hw){
  try{await postForm('/mode_hw',{value:String(hw)});}
  catch(e){return}
  S.hw=hw;
  updateHwCards(hw);
  setStatusTriplet('hw',hwLabel(hw),'mode_hw: '+hwLabel(hw),'等待 /status 确认','warn');
}

// ── Profile Selection ──────────────────────────────────────
async function setProfile(sp){
  var names=['Chill','Normal','Hurry'];
  try{await postForm('/drive_profile',{profile:names[sp]||'Normal'});}
  catch(e){return}
  S.sp=sp;S.spa=false;
  updateProfileCards(sp);
  updateDriveCards(sp,false);
}

async function setDriveMode(mode){
  try{await postForm('/drive_profile',{profile:mode});}
  catch(e){return}
  S.spa=mode==='auto';
  S.sp=(mode==='sloth'||mode==='chill')?0:((mode==='hurry'||mode==='max')?2:1);
  S.driveProfile={auto:0,sloth:1,chill:2,normal:3,hurry:4,max:5}[mode]||3;
  updateDriveCardsByMode(mode);
  updateProfileCards(S.sp);
  var v14=$('ov-fsd-tgl');if(v14)v14.checked=(mode==='max');
  setStatusTriplet('drive',mode,mode,mode==='max'?'MAX/V14 需实车验证':'等待 /status 确认',mode==='max'?'warn':'ok');
}

function updateSpeedStrategyCards(strategy){
  showSpeedMode(strategy||'auto');
}

function applySpeedCustom(d){
  if(!d)return;
  if(d.manualPct!==undefined)speedManualPct=parseInt(d.manualPct,10)||0;
  updateSpeedFixedCards(speedManualPct);
  var vals=d.customPct||[d.cp1,d.cp2,d.cp3,d.cp4];
  for(var i=0;i<4;i++){
    var el=$('speed-cp'+(i+1));
    if(el&&vals&&vals[i]!==undefined)el.value=vals[i];
  }
}

async function loadSpeedStrategy(){
  var strategyResp=await fetchJson('/speed_strategy');
  var customResp=await fetchJson('/speed_custom');
  if(customResp)applySpeedCustom(customResp);
  var strategy=(strategyResp&&strategyResp.strategy)||'auto';
  showSpeedMode(strategy);
  setStatusTriplet('speed',strategy,strategy,'等待 /status 速度确认',strategy==='custom'?'warn':'ok');
}

async function setSpeedStrategy(strategy){
  showSpeedMode(strategy);
  try{await postForm('/speed_strategy',{strategy:strategy});showToast(T('已保存')||'Saved',true)}
  catch(e){loadSpeedStrategy();return}
  setStatusTriplet('speed',strategy,strategy,'等待 /status 速度确认',strategy==='custom'?'warn':'ok');
}

async function setSpeedFixedPct(pct){
  speedManualPct=pct;
  showSpeedMode('fixed');
  updateSpeedFixedCards(pct);
  try{
    await postForm('/speed_strategy',{strategy:'fixed'});
    await postForm('/speed_custom',{manualPct:String(pct)});
    showToast(T('已保存')||'Saved',true);
  }catch(e){loadSpeedStrategy();return}
  setStatusTriplet('speed','fixed','manualPct '+pct+'%','等待 /status 速度确认','ok');
}

async function saveSpeedCustom(){
  var data={
    cp1:String(speedPctVal('speed-cp1',30)),
    cp2:String(speedPctVal('speed-cp2',20)),
    cp3:String(speedPctVal('speed-cp3',10)),
    cp4:String(speedPctVal('speed-cp4',10))
  };
  showSpeedMode('custom');
  try{
    await postForm('/speed_strategy',{strategy:'custom'});
    var d=await postForm('/speed_custom',data);
    applySpeedCustom(d);
    showToast(T('已保存')||'Saved',true);
  }catch(e){loadSpeedStrategy();return}
  setStatusTriplet('speed','custom','cp1-cp4 已保存','等待 /status 速度确认','warn');
}

// ── Save Config (generic toggle) ───────────────────────────
async function saveConfig(){
  var data={};
  var apR=$('hw-ap-restore');
  if(apR)data.apRestore=apR.checked?'1':'0';
  var bt=$('fsd-boot-tgl');
  if(bt&&bt.checked)data.can='1';
  else if(bt)data.can='0';
  try{await postForm('/config',data);}catch(e){}
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
  try{await postForm('/config',data);}catch(e){}
}

// ── HW3 Slew Save ──────────────────────────────────────────
async function saveHw3Slew(){
  saveDefenseConfig();
}

async function saveDefenseConfig(){
  var tgl=$('hw3-slew-tgl');
  var bio=$('def-bionic-tgl');
  var sound=$('def-sound-tgl');
  var nd=$('def-speed-nd-tgl');
  var apeap=$('def-apeap-tgl');
  var data={
    enabled:tgl&&tgl.checked?'1':'0',
    bionic_steering:bio&&bio.checked?'1':'0',
    sound_warning_suppression:sound&&sound.checked?'1':'0',
    speed_no_disturb:nd&&nd.checked?'1':'0',
    ap_eap_compatible:apeap&&apeap.checked?'1':'0'
  };
  try{await postForm('/defense_config',data);}
  catch(e){loadDefenseConfig()}
  loadDefenseConfig();
}

// ── CAN2 ───────────────────────────────────────────────────
// Phase 1: 功耗管理 toggle
async function toggleAutoShutdown(on){
  await postForm('/power_mgmt','autoShutdown='+on);
  poll();
}
async function toggleWifiAutoOff(on){
  await postForm('/power_mgmt','wifiAutoOff='+on);
  poll();
}

async function pollCAN2(){
  var d=await fetchJson('/bus2_ids');
  if(!d)return;
  setCls('b2-status','stat-val '+(d.count>0?'v-ok':'v-dim'));
  setText('b2-status',d.count>0?'Online':'Idle');
  setText('b2-rx',d.rx_total||0);
  setText('b2-ids',d.count||0);
  setText('b2-count','('+d.count+')');
  setCls('s-can2','stat-val '+(d.count>0?'v-ok':'v-dim'));
  setText('s-can2',d.count>0?'Online':'Idle');
  setCls('m-can2','stat-val '+(d.count>0?'v-ok':'v-dim'));
  setText('m-can2',d.count>0?'Online':'Idle');
  var sCan=$('s-can');
  setText('ov-can',(sCan&&sCan.textContent==='Online'?'CAN1 Online':'CAN1 Offline')+' / '+(d.count>0?'CAN2 Online':'CAN2 Idle'));
  var svc=$('svc-mode-tgl');
  if(svc)svc.checked=!!d.service_mode;
  setStatusTriplet('light',
    ($('light-enabled-tgl')&&$('light-enabled-tgl').checked)?'爆闪 ON':'爆闪 OFF',
    '规则 '+(lightPreset||3)+'x '+(lightFrequency||'medium'),
    (d.count>0?'CAN2 Online':'CAN2 Idle')+' / RX '+(d.rx_total||0),
    d.count>0?'warn':'warn');
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

function setLightPreset(count){
  lightPreset=count;
  var cards=$('light-preset');
  if(cards){
    var items=cards.querySelectorAll('.sel-card');
    var values=[3,5,7,10];
    for(var i=0;i<items.length;i++)items[i].classList.toggle('active',values[i]===count);
  }
  var dur=$('stalk-dur');
  if(dur)dur.value=count>=7?'300':(count>=5?'400':'500');
  saveLightingConfig();
}

function updateLightPreset(count){
  var cards=$('light-preset');
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  var values=[3,5,7,10];
  for(var i=0;i<items.length;i++)items[i].classList.toggle('active',values[i]===count);
}

function updateLightOptionCards(id,active,values){
  var cards=$(id);
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  for(var i=0;i<items.length;i++)items[i].classList.toggle('active',values[i]===active);
}

async function loadLightingConfig(){
  var d=await fetchJson('/lighting_config');
  if(!d)return;
  lightPreset=d.count||3;
  lightFrequency=d.frequency||'medium';
  rearFogStrategy=d.rear_fog_strategy||'off';
  updateLightPreset(lightPreset);
  updateLightOptionCards('light-frequency',lightFrequency,['slow','medium','fast']);
  updateLightOptionCards('rear-fog-strategy',rearFogStrategy,['off','strobe','continuous']);
  var t=$('light-enabled-tgl');if(t)t.checked=!!d.enabled;
  setStatusTriplet('light',d.enabled?'爆闪 ON':'爆闪 OFF',
    (d.count||3)+'x '+(d.frequency||'medium')+' / fog '+rearFogStrategy,
    '等待 CAN2 运行确认',
    d.enabled?'warn':'ok');
  setText('tb-exp',experimentSummary());
}

async function saveLightingConfig(){
  var t=$('light-enabled-tgl');
  try{await postForm('/lighting_config',{enabled:t&&t.checked?'1':'0',count:String(lightPreset),frequency:lightFrequency,rear_fog_strategy:rearFogStrategy});}
  catch(e){}
  loadLightingConfig();
}

function setLightFrequency(freq){
  lightFrequency=freq;
  updateLightOptionCards('light-frequency',freq,['slow','medium','fast']);
  saveLightingConfig();
}

function setRearFogStrategy(strategy){
  rearFogStrategy=strategy;
  updateLightOptionCards('rear-fog-strategy',strategy,['off','strobe','continuous']);
  saveLightingConfig();
}

function lightDelayMs(){
  return lightFrequency==='fast'?180:(lightFrequency==='slow'?650:350);
}

async function strobeTest(mode){
  if(!confirm('确认执行灯光注入序列？'))return;
  var t=$('light-enabled-tgl');
  if(t&&!t.checked){t.checked=true;await saveLightingConfig()}
  var count=Math.max(1,Math.min(10,lightPreset||3));
  setText('stalk-status','Strobe 0/'+count);
  for(var i=0;i<count;i++){
    await stalkTest(mode,true);
    setText('stalk-status','Strobe '+(i+1)+'/'+count);
    if(i<count-1)await new Promise(function(resolve){setTimeout(resolve,lightDelayMs())});
  }
  setText('stalk-status',T('空闲')+' ('+count+'x)');
}

async function stalkTest(mode,skipConfirm){
  if(!skipConfirm&&!confirm('确认执行灯光注入？'))return;
  var dur=$('stalk-dur');
  var durVal=dur?Math.max(100,Math.min(3000,parseInt(dur.value||'500',10)||500)):500;
  if(dur)dur.value=durVal;
  setText('stalk-status',T('测试中...')||'Testing...');
  try{
    var r=await fetch('/stalk_test?mode='+(mode==='PULL'?'flash':'highbeam')+'&dur='+durVal);
    if(!r.ok)throw new Error('HTTP '+r.status);
    var res=await r.json();
    if(!res.ok)throw new Error(res.error||'stalk test failed');
    setText('stalk-status',T('空闲')+' ('+res.duration_ms+'ms)');
  }catch(e){setText('stalk-status','Error');showToast((e&&e.message)?e.message:T('请求失败'))}
}

async function toggleServiceMode(){
  var tgl=$('svc-mode-tgl');
  try{await postForm('/service_mode',{on:tgl&&tgl.checked?'1':'0'});}
  catch(e){if(tgl)tgl.checked=!tgl.checked}
}

// ── OTA Upload ─────────────────────────────────────────────
async function uploadFirmware(){
  var fileInput=$('ota-file');
  if(!fileInput||!fileInput.files||fileInput.files.length===0)return;
  if(!confirm('确认上传 OTA 固件？上传完成可能会重启设备。'))return;
  var file=fileInput.files[0];
  var prog=$('ota-progress');
  var bar=$('ota-bar');
  var pct=$('ota-pct');
  var btn=$('ota-btn');
  if(prog)prog.style.display='block';
  if(btn)btn.disabled=true;

  var xhr=new XMLHttpRequest();
  xhr.open('POST','/update',true);
  // Fetch OTA credentials from backend (AP-local only) instead of hardcoding.
  try{var c=await(await fetch('/ota_creds')).json();xhr.setRequestHeader('Authorization','Basic '+btoa(c.u+':'+c.p))}catch(e){}
  xhr.setRequestHeader('Content-Type','application/octet-stream');
  xhr.setRequestHeader('X-File-Name',encodeURIComponent(file.name||'firmware.bin'));

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
  xhr.send(file);
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
  setStatusTriplet('net',
    d.connected?('STA '+d.ssid):(d.connecting?'STA 连接中':'STA 未配置'),
    '等待热点配置读取',
    d.connected?('STA '+d.ip):'STA Offline',
    d.connected?'ok':'warn');
  // Load networks
  var net=await fetchJson('/wifi_networks');
  if(net)renderWifiSlots(net);
  // AP status
  var ap=await fetchJson('/ap_status');
  if(ap){
    setText('ap-ssid',ap.ssid||'--');
    setText('ap-clients',ap.clients||0);
    setText('ap-mode',ap.mode||((ap.channel_auto?'AP+STA':'AP')+' CH'+(ap.channel||'--')));
    var apSsid=$('ap-ssid-input');if(apSsid&&!apSsid.value)apSsid.value=ap.ssid||'';
    var apHidden=$('ap-hidden-tgl');if(apHidden)apHidden.checked=!!ap.hidden;
    setStatusTriplet('net',
      (d.connected?('STA '+d.ssid):'STA Offline')+' / AP '+(ap.ssid||'--'),
      'AP '+(ap.ssid||'--')+(ap.hidden?' hidden':''),
      (d.connected?('STA '+d.ip):'STA Offline')+' / clients '+(ap.clients||0),
      d.connected||ap.clients>0?'ok':'warn');
  }
  var hc=await fetchJson('/hotspot_config');
  if(hc){
    var hs=$('ap-ssid-input');if(hs&&!hs.value)hs.value=hc.ssid||'';
    var ht=$('ap-hidden-tgl');if(ht)ht.checked=!!hc.hidden;
    setText('st-net-nvs','AP '+(hc.ssid||'--')+(hc.hidden?' hidden':''));
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
  try{await postForm('/wifi_config',data);}
  catch(e){return}
  clearWifiForm();
  setText('wifi-status',T('连接中...')||'Connecting...');
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
    html+='<div class="setting-row" style="cursor:pointer" onclick="pickScanResult(\''+escAttr(n.ssid)+'\')">'
      +'<div class="setting-name">'+escHtml(n.ssid)+'</div>'
      +'<div class="v-dim" style="font-size:11px">'+n.rssi+' dBm</div></div>';
  }
  container.innerHTML=html;
}

async function testRelayWifi(){
  var ssid=$('wf-ssid');var pass=$('wf-pass');
  if(!ssid||!ssid.value.trim()){
    showToast('请输入 SSID');
    setStatusTriplet('net','测试未发送','NVS 未变更','请输入中转 WiFi SSID','warn');
    return;
  }
  var data={ssid:ssid?ssid.value:'',pass:pass?pass.value:''};
  try{
    var r=await postForm('/relay_wifi_test',data);
    showToast((r&&r.connected)?'WiFi OK':'WiFi test ready',true);
  }catch(e){}
}

async function saveHotspot(reboot){
  if(reboot&&!confirm('确认保存网络配置并重启设备？'))return;
  var ssid=$('ap-ssid-input');var pass=$('ap-pass-input');var hidden=$('ap-hidden-tgl');
  if(!ssid||!ssid.value)return;
  var data={ssid:ssid.value,pass:pass?pass.value:'',hidden:hidden&&hidden.checked?'1':'0',save_reboot:reboot?'1':'0'};
  try{await postForm('/hotspot_config',data);showToast(reboot?T('重启设备'):T('已保存'),true)}
  catch(e){}
}

function pickScanResult(ssid){
  var wf=$('wf-ssid');if(wf)wf.value=ssid;
  var form=$('wifi-form');if(form)form.style.display='block';
  editingSlot=-1;
}

async function connectWifi(idx){
  try{await postForm('/wifi_connect',{idx:String(idx)});}
  catch(e){return}
  setText('wifi-status',T('连接中...')||'Connecting...');
  setTimeout(pollWifiStatus,300);
}
async function deleteWifi(idx){
  try{await postForm('/wifi_delete',{idx:String(idx)});}
  catch(e){return}
  pollWifiStatus();
}

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
  setText('st-dns-run',(d.dns_bind_ok?'DNS OK':'DNS --')+' / NAT '+(d.nat?'ON':'OFF')+' / block '+(d.dns_blocked||0));
  var dnsRun=$('st-dns-run');if(dnsRun)dnsRun.className='val '+(d.dns_bind_ok?'s-ok':'s-warn');
  setText('st-net-run',(d.sta_connected?('STA '+d.sta_ip):'STA Offline')+' / clients '+(d.ap_clients||0));
}

async function saveGateway(){
  var natTgl=$('gw-nat-tgl');
  var perfTgl=$('gw-perf-tgl');
  if(natTgl){
    try{await postForm('/gateway_dns',{enabled:natTgl.checked?'1':'0'});}
    catch(e){natTgl.checked=!natTgl.checked;return}
  }
  // Perf mode: reduce polling from 1s to 3s when forwarding
  if(typeof restartPoll==='function')restartPoll(perfTgl&&perfTgl.checked?3000:1000);
  pollGatewayStatus();
}

// ── DNS Config ─────────────────────────────────────────────
async function loadGatewayDns(){
  var d=await fetchJson('/dns_rules');
  if(!d)return;
  var bl=$('dns-blacklist');if(bl)bl.value=d.blacklist||'';
  var wl=$('dns-whitelist');if(wl)wl.value=d.whitelist||'';
  setText('dns-bl-cnt',(d.black_count||0)+' '+T('域名'));
  setText('dns-wl-cnt',(d.white_count||0)+' '+T('域名'));
  // Upstream DNS mode
  updateDnsUpstreamCards(d.upstream_mode||0);
  var profile=detectDnsProfile(d.blacklist,d.whitelist);
  updateDnsProfileCards(profile);
  var counts=(d.black_count||0)+' 黑 / '+(d.white_count||0)+' 白';
  setStatusTriplet('dns',profile||'自定义规则',counts,'等待 DNS 运行确认','ok');
  if(!normLines(d.blacklist)||normLines(d.blacklist)===normLines(OLD_DNS_BLACKLIST)){
    applyDnsProfile('conservative');
    updateDnsProfileCards('conservative');
    setStatusTriplet('dns','保守模式','已自动填入 12 条规则','请点击保存规则写入 NVS','warn');
  }
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
  applyDnsProfile(profile);
  updateDnsProfileCards(profile);
  try{await saveGatewayDns();showToast(T('已保存')||'Saved',true)}catch(e){}
}

function updateDnsProfileCards(profile){
  var cards=$('dns-profile');
  if(!cards)return;
  var items=cards.querySelectorAll('.sel-card');
  items[0].classList.toggle('active',profile==='conservative');
  items[1].classList.toggle('active',profile==='aggressive');
}

function detectDnsProfile(blacklist,whitelist){
  var b=normLines(blacklist),w=normLines(whitelist);
  if(b===normLines(DNS_PROFILES.conservative.blacklist)&&w===normLines(DNS_PROFILES.conservative.whitelist))return 'conservative';
  if(b===normLines(DNS_PROFILES.aggressive.blacklist)&&w===normLines(DNS_PROFILES.aggressive.whitelist))return 'aggressive';
  return '';
}

function applyDnsProfile(profile){
  var p=DNS_PROFILES[profile];
  if(!p)return;
  var bl=$('dns-blacklist');
  var wl=$('dns-whitelist');
  if(bl)bl.value=p.blacklist;
  if(wl)wl.value=p.whitelist;
  setText('dns-bl-cnt',countLines(p.blacklist)+' '+T('域名'));
  setText('dns-wl-cnt',countLines(p.whitelist)+' '+T('域名'));
  setStatusTriplet('dns',profile,countLines(p.blacklist)+' 黑 / '+countLines(p.whitelist)+' 白','规则已应用，等待保存','warn');
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
  try{await postForm('/dns_rules',data);}catch(e){return}
  setStatusTriplet('dns','规则已保存','已写入 NVS','等待 DNS 运行确认','ok');
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
  try{await postForm('/gateway_blocked_clear',{});}catch(e){return}
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
  try{await postForm('/rec_start',{});}catch(e){return}
  recActive=true;
  $('rec-start').disabled=true;$('rec-stop').disabled=false;$('rec-dl').disabled=true;
  setText('rec-status',T('录制中')||'Recording...');
}
async function stopRec(){
  try{await postForm('/rec_stop',{});}catch(e){return}
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
  if(tgl){
    try{await postForm('/logging',{eprn:tgl.checked?'1':'0'});}
    catch(e){tgl.checked=!tgl.checked}
  }
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
  // CAN2 MCP2515 pins from API response, fallback to compile-time defaults
  setText('can-cs',d.cs!=null?'GPIO '+d.cs:'GPIO 10');
  setText('can-sck',d.sck!=null?'GPIO '+d.sck:'GPIO 12');
  setText('can-miso',d.miso!=null?'GPIO '+d.miso:'GPIO 13');
  setText('can-mosi',d.mosi!=null?'GPIO '+d.mosi:'GPIO 11');
  setText('can-rst',d.rst!=null?'GPIO '+d.rst:'GPIO 9');
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
  try{await postForm('/ap_config',data);}catch(e){}
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
  pollTick=function(){
    poll();loadTemp();
    var activePage=document.querySelector('.page.active');
    if(activePage){
      var pid=activePage.id;
      if(pid==='pg-can'){if(canTab==='sniffer'&&!sniffPaused)pollSniffer();else if(canTab==='debug')pollLastWrite()}
      if(pid==='pg-bus2')pollCAN2();
    }
  };
  pollTimer=setInterval(pollTick,pollMs);

  // Visibility handling
  document.addEventListener('visibilitychange',function(){
    if(document.hidden){
      if(pollTimer){clearInterval(pollTimer);pollTimer=null}
    }else{
      if(!pollTimer){poll();pollTimer=setInterval(pollTick,pollMs)}
    }
  });
});

function restartPoll(ms){
  pollMs=ms;
  if(pollTimer&&pollTick){
    clearInterval(pollTimer);
    pollTimer=setInterval(pollTick,pollMs);
  }
}
</script>
<!-- Mobile Bottom Tab Bar -->
<div class="mob-tabs" id="mob-tabs">
  <div class="mob-tab active" data-page="pg-overview"><div class="mob-icon">▣</div><div>状态</div></div>
  <div class="mob-tab" data-page="pg-drive"><div class="mob-icon">◉</div><div>模式</div></div>
  <div class="mob-tab" data-page="pg-speed"><div class="mob-icon">↗</div><div>速度</div></div>
  <div class="mob-tab" data-page="pg-network"><div class="mob-icon">◎</div><div>网络</div></div>
  <div class="mob-tab" onclick="toggleMobMore()"><div class="mob-icon">···</div><div>更多</div></div>
</div>
<!-- Mobile More Menu -->
<div class="mob-more-panel" id="mob-more">
  <div class="mob-more-close" onclick="toggleMobMore()">✕</div>
  <div class="mob-more-item" data-page="pg-overview">▣ 状态</div>
  <div class="mob-more-item" data-page="pg-hardware">◇ 激活模式</div>
  <div class="mob-more-item" data-page="pg-drive">◉ 驾驶模式</div>
  <div class="mob-more-item" data-page="pg-speed">↗ 速度偏移</div>
  <div class="mob-more-item" data-page="pg-ota">⇧ OTA升级</div>
  <div class="mob-more-item" data-page="pg-bus2">✦ CAN2控制</div>
  <div class="mob-more-item" data-page="pg-network">◎ 网络设置</div>
  <div class="mob-more-item" data-page="pg-defense">◈ FSD防御</div>
  <div class="mob-more-item" data-page="pg-can">⌘ CAN工具</div>
</div>
</body>
</html>)HTML";
