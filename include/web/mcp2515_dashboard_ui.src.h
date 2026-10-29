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
<title data-product-name data-single-text="Atlas Single CAN">Atlas T-2CAN</title>
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
  --font-mono: 'JetBrains Mono','IBM Plex Mono','SF Mono',ui-monospace,Menlo,Consolas,'Courier New',monospace;
  --border-soft: rgba(75,85,99,0.3);
  --border-soft-2: rgba(75,85,99,0.2);
  --sep-line: rgba(255,255,255,0.08);
  --ok-ink: #34d399;
  --safety-note-ink: #fcd34d;
  --cockpit-bg: #05070b;
  --cockpit-panel: rgba(15,23,42,0.82);
  --cockpit-panel-strong: #101827;
  --cockpit-soft: rgba(15,23,42,.58);
  --cockpit-stroke: rgba(148,163,184,0.20);
  --cockpit-stroke-strong: rgba(148,163,184,0.36);
  --cockpit-muted: #8d9bb4;
  --cockpit-dim: #66748c;
  --cockpit-cyan: #38bdf8;
  --cockpit-radius-xl: 28px;
  --cockpit-radius-lg: 20px;
  --cockpit-shadow: 0 24px 64px rgba(0,0,0,0.35);
  --cockpit-glow-purple: 0 0 42px rgba(124,58,237,0.22);
  --cockpit-glow-cyan: 0 0 34px rgba(56,189,248,0.18);
  --cockpit-glow-danger: 0 0 34px rgba(248,113,113,0.22);
  --cockpit-glow-ok: 0 0 34px rgba(74,222,128,0.20);
  --cockpit-card-blur: blur(22px) saturate(1.16);
  --cockpit-grid-line: rgba(148,163,184,0.045);
  --cockpit-primary-bg: radial-gradient(circle at 86% 0%, rgba(124,58,237,.24), transparent 34%), linear-gradient(145deg, rgba(15,23,42,.90), rgba(8,13,24,.70));
  --cockpit-vehicle-bg: linear-gradient(145deg, rgba(15,23,42,.60), rgba(8,13,24,.82));
  --cockpit-viz-bg: radial-gradient(circle at 50% 45%, rgba(56,189,248,.17), transparent 38%), linear-gradient(180deg, rgba(2,6,23,.30), rgba(2,6,23,.72));
  --diag-bar-bg: rgba(15,23,42,.90);
  --tgl-track: rgba(148,163,184,.3);
  --accent-stroke-soft: rgba(124,58,237,.58);
  --accent-bg-soft: rgba(124,58,237,.16);
  --ck-chip-bg: rgba(148,163,184,.15);
  --ck-card-bg: radial-gradient(120% 60% at 50% 0%, rgba(56,189,248,.08), transparent 60%), linear-gradient(160deg,#0f172a,#0b1220);
  --row-glass: rgba(255,255,255,.04);
  --row-glass-2: rgba(255,255,255,.03);
  --row-sink: rgba(0,0,0,.2);
  --cockpit-shell-bg: radial-gradient(circle at 14% 0%, rgba(124,58,237,.20), transparent 34%), radial-gradient(circle at 88% 12%, rgba(56,189,248,.14), transparent 32%), linear-gradient(145deg, rgba(2,6,23,.84), rgba(8,13,24,.64));
  --diag-shell-bg: rgba(2,6,23,.42);
  --diag-head-bg: rgba(15,23,42,.86);
  --diag-timeline-bg: rgba(15,23,42,.48);
  /* === 设计系统基底（V1.1，全板块统一） === */
  --ds-cyan: #0ea5e9;
  --ds-indigo: #6366f1;
  --ds-accent-grad: linear-gradient(135deg,#0ea5e9,#6366f1);
  --ds-ok: #34d399;
  --ds-warn: #f59e0b;
  --ds-err: #f87171;
  --ds-card-bg: rgba(15,23,42,.58);
  --ds-card-stroke: rgba(148,163,184,.20);
  --ds-radius: 18px;
}
html.light {
  --sidebar-bg: #F4F4F0;
  --main-bg: #E9E7E0;
  --card-bg: #F7F6F2;
  --card-bg-alt: #EFEDE7;
  --accent: #7c3aed;
  --accent-light: #8b5cf6;
  --ok: #15803D;
  --err: #B91C1C;
  --warn: #B45309;
  --info: #1D4ED8;
  --tx1: #141412;
  --tx2: #44443E;
  --tx3: #6E6E66;
  --border: #CFCDC5;
  --header-bg: #F4F4F0;
  --border-soft: rgba(17,17,17,0.16);
  --border-soft-2: rgba(17,17,17,0.10);
  --sep-line: rgba(17,17,17,0.14);
  --ok-ink: #0E9F6E;
  --safety-note-ink: #92600A;
  --cockpit-bg: #EAE8E3;
  --cockpit-panel: #F7F6F2;
  --cockpit-panel-strong: #EFEDE7;
  --cockpit-soft: #F1EFE9;
  --cockpit-stroke: rgba(17,17,17,0.20);
  --cockpit-stroke-strong: rgba(17,17,17,0.45);
  --cockpit-muted: #5D5D55;
  --cockpit-dim: #7C7C73;
  --cockpit-cyan: #0369A1;
  --cockpit-shadow: 0 12px 32px rgba(17,17,17,0.10);
  --cockpit-glow-purple: none;
  --cockpit-glow-cyan: none;
  --cockpit-glow-danger: none;
  --cockpit-glow-ok: none;
  --cockpit-card-blur: none;
  --cockpit-grid-line: rgba(17,17,17,0.07);
  --cockpit-primary-bg: linear-gradient(145deg, #FBFAF7, #EFEEE8);
  --cockpit-vehicle-bg: linear-gradient(145deg, #F4F4F0, #E9E7E0);
  --cockpit-viz-bg: radial-gradient(circle at 50% 45%, rgba(3,105,161,.14), transparent 38%), linear-gradient(180deg, #EFEDE7, #E4E1D9);
  --diag-bar-bg: #E5E2DA;
  --tgl-track: rgba(17,17,17,0.16);
  --accent-stroke-soft: rgba(124,58,237,0.45);
  --accent-bg-soft: rgba(124,58,237,0.10);
  --ck-chip-bg: rgba(17,17,17,0.07);
  --ck-card-bg: #F7F6F2;
  --row-glass: rgba(17,17,17,0.04);
  --row-glass-2: rgba(17,17,17,0.03);
  --row-sink: rgba(17,17,17,0.05);
  --cockpit-shell-bg: linear-gradient(145deg, #FBFAF7, #EFEEE8);
  --diag-shell-bg: #F1EFE9;
  --diag-head-bg: #E9E7E0;
  --diag-timeline-bg: #F4F4F0;
  --ds-cyan: #0369A1;
  --ds-indigo: #4338CA;
  --ds-accent-grad: linear-gradient(135deg,#0ea5e9,#6366f1);
  --ds-ok: #0E9F6E;
  --ds-warn: #B45309;
  --ds-err: #B91C1C;
  --ds-card-bg: #F7F6F2;
  --ds-card-stroke: rgba(17,17,17,0.18);
}

/* === Reset & Base === */
.val, .stat-val, .diag-item, .tabular { font-family: var(--font-mono); }
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
  font-size: 19px; cursor: pointer; border-radius:12px;
  transition: background .15s, color .15s; user-select: none; font-weight: 600; }
.nav-item .nav-icon { font-size: 24px; line-height: 1; flex-shrink: 0; }
.nav-item:hover { background: var(--main-bg); color: var(--tx2); }
.nav-item.active { background:rgba(124,58,237,0.18); color: var(--accent-light); font-weight: 800; font-size: 20px; }
.sidebar-ft { padding: 10px 12px; border-top: 1px solid var(--border);
  display: flex; gap: 6px; }
.sidebar-ft button { flex:1; background: var(--card-bg); border: none;
  color: var(--tx3); padding: 4px 0; border-radius:4px; font-size: 11px;
  cursor: pointer; }
.sidebar-ft button:hover { color: var(--tx2); }

/* === Main Area === */
.main { flex: 1; display: flex; flex-direction: column; overflow: hidden; }
.topbar { background: var(--header-bg); padding: 10px 16px;
  display: flex; align-items: center; gap: 12px; flex-shrink: 0;
  border-bottom: 1px solid var(--border); min-height: 44px; }
.topbar-badge { padding: 7px 16px; border-radius:999px; font-size: 13px; font-weight: 800; border: 1px solid var(--border); background: var(--card-bg-alt); }
.badge-ok { color: var(--tx1); }
.badge-err { color: var(--err); }
.toast{position:fixed;bottom:60px;left:50%;transform:translateX(-50%);background:#7f1d1d;color:#fff;padding:8px 18px;border-radius:8px;font-size:13px;z-index:999;opacity:0;transition:opacity .3s;pointer-events:none}
.toast.ok{background:#166534}
.toast.show{opacity:1}
.badge-warn { color: var(--tx1); }
.topbar-fps { color: var(--tx1); font-size: 14px; font-weight: 800; padding: 7px 16px; border-radius:999px; background: var(--card-bg-alt); border: 1px solid var(--border); }
.topbar-time { margin-left: auto; color: var(--tx3); font-size: 12px; }
.topbar-exp { color: var(--warn); font-size: 12px; font-weight: 800; padding: 7px 12px; border-radius:999px; background: rgba(251,191,36,0.12); border: 1px solid rgba(251,191,36,0.35); }
.mobile-theme-toggle { display: none; border: 1px solid var(--border); background: var(--card-bg-alt); color: var(--tx1); border-radius:999px; min-width: 76px; height: 32px; padding: 0 10px; font-size: 12px; font-weight: 800; white-space: nowrap; }
.topbar-dot { width: 8px; height: 8px; border-radius:50%; display: inline-block;
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
.card { background: var(--ds-card-bg); border: 1px solid var(--ds-card-stroke); border-radius:var(--ds-radius); padding: 20px; backdrop-filter: var(--cockpit-card-blur); box-shadow: 0 8px 28px rgba(0,0,0,.22); margin-bottom: 14px; }
.page-title { font-size: 26px; font-weight: 800; margin: 4px 0 20px; padding-bottom: 16px; border-bottom: 1px solid var(--border); color: var(--tx1); }
.card-title { font-size: 18px; font-weight: 800; color: var(--tx1);
  margin-bottom: 10px; }
.card-subtitle { font-size: 12px; color: var(--tx3); margin-top: -6px;
  margin-bottom: 10px; }
.quick-actions { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-bottom: 14px; }
.qa-btn { background: var(--card-bg); border: 2px solid var(--border); border-radius:12px;
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
.exp-badge { display: inline-block; margin-left: 8px; padding: 2px 8px; border-radius:999px; background: rgba(251,191,36,0.12); border: 1px solid rgba(251,191,36,0.35); color: var(--warn); font-size: 11px; font-weight: 800; vertical-align: middle; }
.status-triplet { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin: 10px 0 16px; }
.status-chip { background: var(--card-bg-alt); border: 1px solid var(--border); border-radius:10px; padding: 10px; min-height: 54px; }
.status-chip .lbl { color: var(--tx3); font-size: 11px; margin-bottom: 4px; }
.status-chip .val { color: var(--tx1); font-size: 14px; font-weight: 800; overflow-wrap: anywhere; }
.s-ok { color: var(--ok) !important; }
.s-warn { color: var(--warn) !important; }
.s-err { color: var(--err) !important; }

/* === Stats Grid === */
.stats { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px;
  margin-bottom: 4px; }
.stat { background: var(--card-bg); border: 1px solid var(--border); border-radius:8px; padding: 12px;
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
  border-radius:12px; transition: background .2s; cursor: pointer; }
.tgl-track::after { content: ''; position: absolute; width: 24px; height: 24px;
  background: var(--tx3); border-radius:50%; top: 2px; left: 2px;
  transition: transform .2s, background .2s; }
.tgl input:checked + .tgl-track { background: var(--accent); }
.tgl input:checked + .tgl-track::after { transform: translateX(24px); background: #fff; }

/* === Buttons === */
.btn { background: var(--accent); border: none; color: #fff; padding: 12px 20px;
  border-radius:8px; font-size: 14px; cursor: pointer; font-weight: 600;
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
  border-radius:12px; padding: 24px 14px; text-align: center; cursor: pointer;
  transition: border-color .15s, background .15s; }
.sel-card:hover { border-color: var(--tx3); }
.sel-card.active { border-color: var(--accent-light); background:rgba(124,58,237,0.18); color: var(--accent-light); }
.sel-card.active .sel-lbl { color: var(--accent-light); }
.sel-card .sel-lbl { font-size: 10px; color: var(--tx3); text-transform: uppercase;
  letter-spacing: 0.5px; }
.sel-card .sel-name { font-size: 20px; font-weight: 800; margin-top: 2px; }

/* === Drive Mode Cards (Phase 5A) === */
.drive-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; margin: 8px 0; }
.drive-card { background: var(--card-bg-alt); border: 2px solid var(--border); border-radius:12px;
  padding: 16px 10px; text-align: center; cursor: pointer; transition: all 0.2s; }
.drive-card:hover { border-color: var(--tx3); transform: translateY(-2px); }
.drive-card.active { border-color:#38bdf8; background:linear-gradient(160deg, rgba(14,165,233,.20), rgba(99,102,241,.16)); box-shadow:0 6px 16px rgba(14,165,233,.18); }
.drive-card .drive-icon { font-size: 28px; margin-bottom: 6px; }
.drive-card .drive-name { font-size: 16px; font-weight: 700; color: var(--tx1); }
.drive-card .drive-desc { font-size: 11px; color: var(--tx3); margin-top: 4px; line-height: 1.3; }
@media(max-width:768px) {
  .drive-grid { grid-template-columns: repeat(2, 1fr); gap: 8px; }
  .drive-card { padding: 12px 6px; }
  .drive-card .drive-icon { font-size: 22px; }
  .drive-card .drive-name { font-size: 14px; }
  .drive-card .drive-desc { font-size: 10px; }
}

/* === Setting Row === */
.setting-row { display: flex; justify-content: space-between; align-items: center;
  padding: 12px 0; border-bottom: 1px solid var(--border-soft); }
.setting-row:last-child { border-bottom: none; }
.setting-name { font-size: 14px; color: var(--tx1); font-weight: 600; }
.setting-desc { font-size: 12px; color: var(--tx3); margin-top: 2px; }

/* === Sub-tabs (CAN tools) === */
.sub-tabs { display: flex; gap: 2px; background: var(--card-bg);
  border-radius:6px; padding: 2px; margin-bottom: 10px; }
.sub-tab { flex: 1; text-align: center; padding: 8px 12px;
  border-radius:4px; font-size: 13px; cursor: pointer;
  color: var(--tx3); transition: all .15s; font-weight: 600; }
.sub-tab.active { background: var(--accent); color: #fff; font-weight: 700; }

/* === Table === */
.tbl { width: 100%; border-collapse: collapse; font-size: 13px; table-layout: fixed; }
.tbl th { text-align: left; padding: 8px 10px; color: var(--tx3);
  border-bottom: 1px solid var(--border); font-weight: 500; font-size: 11px; }
.tbl td { padding: 6px 10px; border-bottom: 1px solid var(--border-soft-2);
  overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.tbl .hex { font-family: 'SF Mono', 'Courier New', monospace; color: var(--accent-light); }
.tbl-wrap { overflow-x: auto; -webkit-overflow-scrolling: touch; }
select.inp { font-size: 16px; }

/* === Inputs === */
.inp { background: var(--card-bg-alt); border: 1px solid var(--border);
  border-radius:8px; padding: 10px 14px; color: var(--tx1); font-size: 16px;
  width: 100%; outline: none; max-width: 100%; box-sizing: border-box; }
.inp:focus { border-color: var(--accent-light); }
.inp::placeholder { color: var(--tx3); }
textarea.inp { resize: vertical; min-height: 60px; font-family: monospace;
  font-size: 11px; line-height: 1.5; }

/* === Status Indicator === */
.status-dot { width: 10px; height: 10px; border-radius:50%; display: inline-block; }
.status-dot.ok { background: var(--ok); box-shadow: 0 0 6px var(--ok); }
.status-dot.err { background: var(--err); }
.status-dot.warn { background: var(--warn); }

/* === Mobile Bottom Tab Bar === */
.mob-tabs { display: none; position: fixed; bottom: 0; left: 0; right: 0;
  z-index: 200; background: var(--sidebar-bg); border-top: 1px solid var(--border);
  padding: 2px 0; padding-bottom: env(safe-area-inset-bottom, 0px);
  flex-shrink: 0; }
.mob-tab { display: flex; flex-direction: column; align-items: center;
  justify-content: center; padding: 6px 0; color: var(--tx3); background: transparent; border: 0;
  font-size: 10px; cursor: pointer; flex: 1; -webkit-tap-highlight-color: transparent; font-weight: 600; }
.mob-tab.active { color: var(--accent-light); }
.mob-tab .mob-icon { font-size: 22px; line-height: 1; margin-bottom: 1px; }
.mob-more-panel { display: none; position: fixed; bottom: 52px; left: 0; right: 0;
  z-index: 210; background: var(--sidebar-bg); border-top: 1px solid var(--border);
  border-radius:12px 12px 0 0; padding: 12px 16px; max-height: 60vh; overflow-y: auto; }
.mob-more-panel.open { display: block; }
.mob-more-item { display: block; padding: 10px 0; color: var(--tx2);
  font-size: 13px; border-bottom: 1px solid var(--border-soft); cursor: pointer; }
.mob-more-item:last-child { border-bottom: none; }
.mob-more-item.active { color: var(--accent-light); }
.mob-more-close { position: absolute; top: 8px; right: 12px; color: var(--tx3);
  font-size: 18px; cursor: pointer; }
.mobile-page { display: none; }
.mobile-card { background: var(--card-bg); border: 1px solid var(--border); border-radius:14px;
  padding: 14px; margin: 10px 12px; color: var(--tx1); }
.mobile-card h3 { font-size: 16px; margin: 0 0 10px; }
.mobile-card label { display: flex; align-items: center; justify-content: space-between; gap: 10px; margin: 10px 0; }
.mobile-card input, .mobile-card select, .mobile-card textarea { background: var(--card-bg-alt); border: 1px solid var(--border);
  color: var(--tx1); border-radius:10px; padding: 10px; max-width: 100%; }
.mobile-card textarea { width: 100%; display: block; margin-bottom: 10px; }
.mobile-card button { border: 0; border-radius:10px; padding: 10px 14px; background: var(--accent); color: #fff; font-weight: 800; }

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
  .mobile-page.active:not(#mob-home-page) { display: block; position: fixed; top: 54px; bottom: 74px; left: 0; right: 0;
    z-index: 160; background: var(--main-bg); overflow-y: auto; padding: 8px 0; }
  .topbar { padding: 7px 8px; gap: 5px; min-height: 0; flex-wrap: wrap; align-content: center; }
  .topbar-dot { width: 7px; height: 7px; }
  .topbar-fps { font-size: 11px; padding: 5px 8px; max-width: 72px; overflow: hidden; white-space: nowrap; }
  .topbar-badge { font-size: 11px; padding: 5px 8px; white-space: nowrap; }
  .topbar-exp { order: 9; flex: 1 0 100%; text-align: center; font-size: 11px; padding: 4px 8px; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .topbar-time { order: 8; font-size: 11px; margin-left: auto; padding-right: 2px; white-space: nowrap; }
  .content { padding: 8px 10px 12px; }
  .card { padding: 12px 10px; margin-bottom: 10px; border-radius:8px; }
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

/* === Atlas driving-first redesign === */
.cockpit-shell { position: relative; overflow: hidden; border: 1px solid var(--cockpit-stroke); border-radius:30px; padding: 18px; background: var(--cockpit-shell-bg); box-shadow: var(--cockpit-shadow); }
.cockpit-ambient { position: absolute; inset: 0; pointer-events: none; background-image: linear-gradient(var(--cockpit-grid-line) 1px, transparent 1px), linear-gradient(90deg, var(--cockpit-grid-line) 1px, transparent 1px); background-size: 44px 44px; mask-image: radial-gradient(circle at center, black, transparent 74%); opacity: .82; }
.cockpit-home { position: relative; z-index: 1; display: grid; gap: 16px; }
.cockpit-status-bar { display: flex; flex-wrap: wrap; align-items: center; justify-content: space-between; gap: 10px; }
.cockpit-title small { display: block; color: var(--cockpit-muted); font-size: 11px; font-weight: 900; letter-spacing: .14em; text-transform: uppercase; }
.cockpit-title h2 { margin: 4px 0 0; font-size: 32px; line-height: .98; letter-spacing: -0.05em; }
.cockpit-chip-row { display: flex; flex-wrap: wrap; gap: 8px; justify-content: flex-end; }
.cockpit-chip { border: 1px solid var(--cockpit-stroke); border-radius:999px; padding: 8px 12px; background: var(--cockpit-soft); color: var(--tx3); font-size: 12px; font-weight: 850; backdrop-filter: var(--cockpit-card-blur); }
.cockpit-chip.ok { color: var(--ok); border-color: rgba(74,222,128,.35); background: rgba(74,222,128,.09); }
.cockpit-chip.warn { color: var(--warn); border-color: rgba(251,191,36,.35); background: rgba(251,191,36,.09); }
.cockpit-grid { display: grid; grid-template-columns: minmax(0, 1.05fr) minmax(360px, .95fr); gap: 16px; }
.cockpit-home-main { display: grid; gap: 12px; }
.cockpit-primary { min-height: 360px; border: 1px solid var(--cockpit-stroke-strong); border-radius:var(--cockpit-radius-xl); padding: 24px; background: var(--cockpit-primary-bg); box-shadow: var(--cockpit-shadow), var(--cockpit-glow-purple); display: grid; grid-template-rows: 1fr auto; gap: 18px; overflow: hidden; backdrop-filter: var(--cockpit-card-blur); }
.cockpit-state-panel { min-height: 230px; display: flex; flex-direction: column; align-items: center; justify-content: center; text-align: center; }
.cockpit-state-label { color: var(--cockpit-muted); font-size: 12px; font-weight: 950; letter-spacing: .16em; text-transform: uppercase; }
.cockpit-state { width: 100%; font-size: clamp(86px, 12vw, 152px); line-height: .78; font-weight: 950; letter-spacing: -.075em; margin: 18px 0 12px; color: var(--err); text-align: center; text-shadow: var(--cockpit-glow-danger); }
.cockpit-state.on { color: var(--ok); text-shadow: var(--cockpit-glow-ok); }
.cockpit-state-desc { color: var(--tx2); font-size: 14px; line-height: 1.55; max-width: 540px; margin: 0 auto; }
.cockpit-hold { width: 100%; min-height: 72px; margin-top: 18px; border: none; border-radius:24px; background: linear-gradient(135deg, #991b1b, #ef4444); color: white; font-size: 16px; font-weight: 950; letter-spacing: .045em; cursor: pointer; box-shadow: 0 18px 42px rgba(239,68,68,.24); }
.cockpit-hold.on { background: linear-gradient(135deg, #047857, #22c55e); box-shadow: 0 18px 42px rgba(34,197,94,.22); }
.cockpit-metrics { display: grid; grid-template-columns: repeat(4, minmax(0,1fr)); gap: 10px; }
.cockpit-metrics.m5 { grid-template-columns: repeat(5, minmax(0,1fr)); }
.cockpit-metric { min-height: 96px; border: 1px solid var(--cockpit-stroke); border-radius:var(--cockpit-radius-lg); background: var(--cockpit-soft); padding: 14px; backdrop-filter: var(--cockpit-card-blur); }
.cockpit-metric .lbl { color: var(--cockpit-muted); font-size: 10px; font-weight: 900; letter-spacing: .09em; text-transform: uppercase; }
.cockpit-metric .val { margin-top: 8px; color: var(--tx1); font-size: 24px; font-weight: 900; letter-spacing: -0.045em; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.cockpit-metric .val.vf-md { font-size: 20px; }
.cockpit-metric .val.vf-sm { font-size: 17px; }
.cockpit-metric .val.vf-xs { font-size: 14px; }
.cockpit-metrics.m5 .val { font-size: 18px; }
.cockpit-metrics.m5 .val.vf-md { font-size: 16px; }
.cockpit-metrics.m5 .val.vf-sm { font-size: 14px; }
.cockpit-metrics.m5 .val.vf-xs { font-size: 12px; }
.cockpit-metric .val.v-ok { color: var(--ok); }
.cockpit-metric .val.v-err { color: var(--err); }
.cockpit-metric .val.v-warn { color: var(--warn); }
.cockpit-metric .val.v-info { color: var(--info); }
.cockpit-metric .val.v-acc { color: var(--accent-light); }
.cockpit-metric .val.v-dim { color: var(--tx3); }
.cockpit-metric .hint { margin-top: 4px; color: var(--cockpit-dim); font-size: 11px; }
.cockpit-vehicle { border: 1px solid var(--cockpit-stroke); border-radius:var(--cockpit-radius-xl); background: var(--cockpit-vehicle-bg); padding: 20px; box-shadow: var(--cockpit-shadow), var(--cockpit-glow-cyan); backdrop-filter: var(--cockpit-card-blur); }
.cockpit-vehicle-visual { position: relative; min-height: 220px; display: grid; place-items: center; margin: 14px 0; border-radius:24px; overflow: hidden; background: var(--cockpit-viz-bg); }
.vehicle-lane { position: absolute; inset: 0; background: linear-gradient(90deg, transparent 18%, rgba(56,189,248,.20) 50%, transparent 82%); clip-path: polygon(43% 0, 57% 0, 82% 100%, 18% 100%); opacity: .72; }
.vehicle-silhouette { position: relative; width: 46%; min-width: 180px; height: 132px; border-radius: 42% 42% 28% 28%; background: linear-gradient(145deg, rgba(226,232,240,.94), rgba(100,116,139,.22)); box-shadow: 0 24px 70px rgba(56,189,248,.18); }
.vehicle-silhouette::before { content: ''; position: absolute; inset: 18px 32px 54px; border-radius: 999px 999px 18px 18px; background: linear-gradient(90deg, rgba(15,23,42,.86), rgba(56,189,248,.22)); }
.vehicle-silhouette::after { content: ''; position: absolute; left: 18%; right: 18%; bottom: 18px; height: 10px; border-radius: 999px; background: rgba(56,189,248,.72); box-shadow: 0 0 24px rgba(56,189,248,.45); }
.cockpit-mode-grid { display: grid; grid-template-columns: repeat(3, minmax(0,1fr)); gap: 10px; margin-top: 14px; }
.cockpit-mode { border: 1px solid var(--cockpit-stroke); border-radius:18px; background: var(--cockpit-soft); padding: 14px; }
.cockpit-mode.active { border-color: var(--accent-stroke-soft); background: var(--accent-bg-soft); }
.cockpit-mode strong { display: block; margin-top: 5px; font-size: 16px; }
.cockpit-mode span { display: block; margin-top: 4px; color: var(--tx3); font-size: 11px; line-height: 1.35; }
.cockpit-safety-strip, .cockpit-warning { border: 1px solid rgba(251,191,36,.35); border-radius:18px; background: rgba(251,191,36,.08); padding: 14px; color: var(--tx2); font-size: 13px; line-height: 1.45; }
.mobile-remote { display: none; }
.mobile-app-shell { border-radius:28px; padding: 16px; background: linear-gradient(180deg, #f8fafc, #e9eef7); color: #111827; box-shadow: 0 18px 50px rgba(0,0,0,.22); }
.mobile-app-shell .cockpit-title small { color: #64748b; }
.mobile-app-shell .cockpit-title h2 { color: #0f172a; }
.mobile-app-shell .cockpit-chip { background: #fff; color: #334155; border-color: #e2e8f0; }
.mobile-primary-card { margin-top: 14px; border-radius:30px; padding: 22px; color: white; background: radial-gradient(circle at 86% 0%, rgba(56,189,248,.34), transparent 34%), linear-gradient(145deg, #111827, #312e81); box-shadow: 0 18px 42px rgba(49,46,129,.28); }
.mobile-primary-card .cockpit-state-label { color: #c4b5fd; }
.mobile-primary-card .mobile-state { font-size: 74px; line-height: .86; font-weight: 950; letter-spacing: -.1em; margin: 14px 0 4px; color: #ffb4bd; text-shadow: 0 0 24px rgba(255,180,189,.24); }
.mobile-primary-card .mobile-state.on { color: #adffc8; text-shadow: 0 0 24px rgba(173,255,200,.26); }
.mobile-primary-card p { color: #cbd5e1; font-size: 13px; line-height: 1.45; margin: 0; }
.mobile-hold { width: 100%; min-height: 58px; margin-top: 14px; border: none; border-radius:22px; background: #ef4444; color: white; font-weight: 950; letter-spacing: .04em; box-shadow: 0 12px 26px rgba(239,68,68,.25); }
.mobile-hold.on { background: #16a34a; box-shadow: 0 12px 26px rgba(22,163,74,.24); }
.mobile-remote-grid { display: grid; grid-template-columns: repeat(2, minmax(0,1fr)); gap: 10px; margin-top: 12px; }
.mobile-remote-card { background: white; border: 1px solid #e2e8f0; border-radius:22px; padding: 14px; min-height: 86px; }
.mobile-remote-card .lbl { color: #64748b; font-size: 10px; font-weight: 900; letter-spacing: .08em; text-transform: uppercase; }
.mobile-remote-card .val { margin-top: 7px; color: #0f172a; font-size: 21px; font-weight: 900; overflow-wrap: anywhere; }
.mobile-risk-note, .mobile-alert-card { margin-top: 12px; border-radius:22px; padding: 14px; background: #fff7ed; border: 1px solid #fed7aa; color: #9a3412; font-size: 13px; line-height: 1.45; }
/* === AP 注入安全卡（驾驶舱风格，仅显示层，服务端强制不变） === */
.cockpit-card { background: var(--ck-card-bg); border:1px solid var(--ds-card-stroke); }
.ck-head { display:flex; align-items:center; justify-content:space-between; margin-bottom:14px; }
.ck-head h3 { margin:0; font-size:17px; color:var(--tx1); display:flex; align-items:center; gap:9px; }
.ck-chip { font-size:10.5px; padding:4px 11px; border-radius:999px; background:var(--ck-chip-bg); color:var(--tx2); border:1px solid var(--ds-card-stroke); }
.ck-chip.ok { background:rgba(52,211,153,.14); color:var(--ok-ink); border-color:rgba(52,211,153,.3); }
.state-panel { background:var(--row-glass); border:1px solid var(--ds-card-stroke); border-radius:13px; padding:15px; text-align:center; margin-bottom:14px; }
.state-panel.active { border-color:rgba(52,211,153,.4); background:rgba(52,211,153,.06); }
.sp-label { font-size:10px; color:var(--cockpit-muted); letter-spacing:.06em; }
.sp-val { font-size:24px; font-weight:700; color:var(--tx3); margin:6px 0; }
.state-panel.active .sp-val { color:var(--ok-ink); }
.sp-desc { font-size:10.5px; color:var(--cockpit-dim); }
.gate-bar { height:5px; background:rgba(52,211,153,.15); border-radius:999px; margin-top:9px; overflow:hidden; }
.gate-bar i { display:block; height:100%; width:0; background:linear-gradient(90deg,#34d399,#10b981); border-radius:999px; transition:width .2s; }
.src-row { display:flex; align-items:center; justify-content:space-between; background:var(--row-sink); border:1px solid var(--ds-card-stroke); border-radius:10px; padding:10px 14px; margin-bottom:14px; font-size:12px; }
.src-row b { color:var(--tx1); }
.ctl-section { font-size:11px; color:var(--tx3); letter-spacing:.06em; margin:6px 0 10px; }
.ctl-row { display:flex; align-items:center; justify-content:space-between; background:var(--row-glass-2); border:1px solid var(--ds-card-stroke); border-radius:11px; padding:11px 15px; margin-bottom:9px; }
.ctl-row .cn { font-size:13px; color:var(--tx1); }
.ctl-row .cd { font-size:10.5px; color:var(--cockpit-dim); margin-top:2px; }
.safety-strip { margin-top:14px; background:rgba(245,158,11,.08); border:1px solid rgba(245,158,11,.25); border-radius:10px; padding:11px 15px; font-size:11.5px; color:var(--safety-note-ink); line-height:1.6; }
/* === .tgl 开关（AP 门控/自动恢复，仅显示皮，input id/checked 不变） === */
.tgl { position:relative; display:inline-block; width:40px; height:22px; }
.tgl input { opacity:0; width:100%; height:100%; position:absolute; margin:0; cursor:pointer; }
.tgl-track { position:absolute; inset:0; background:var(--tgl-track); border-radius:999px; transition:.2s; }
.tgl input:checked + .tgl-track { background:linear-gradient(90deg,#0ea5e9,#6366f1); }
.tgl-track::after { content:""; position:absolute; top:2px; left:2px; width:18px; height:18px; border-radius:50%; background:#fff; transition:.2s; }
.tgl input:checked + .tgl-track::after { transform:translateX(18px); }
/* === 插件管理卡（Task 5，驾驶舱风格换皮） === */
.install-grid { display:grid; grid-template-columns:repeat(3,1fr); gap:10px; margin-bottom:14px; }
.install-cell { background:rgba(255,255,255,.04); border:1px solid var(--ds-card-stroke); border-radius:12px; padding:13px 12px; text-align:center; }
.install-cell .ii { font-size:22px; margin-bottom:6px; }
.install-cell .it { font-size:11.5px; color:var(--tx1); margin-bottom:6px; font-weight:600; }
.mock-input { background:rgba(0,0,0,.25); border:1px solid var(--ds-card-stroke); border-radius:7px; padding:6px 8px; font-size:11px; color:var(--tx2); width:100%; margin-bottom:6px; box-sizing:border-box; }
.ds-btn { display:inline-block; background:var(--ds-accent-grad); color:#fff; font-size:11px; padding:5px 12px; border-radius:7px; border:none; cursor:pointer; }
.ds-btn-muted { font-size:10px; color:var(--cockpit-dim); }
.ds-select { background:rgba(0,0,0,.25); border:1px solid var(--ds-card-stroke); border-radius:7px; color:var(--tx1); padding:6px; }
.plugin-list { display:grid; gap:8px; }
.collapse { margin-top:14px; background:var(--row-glass-2); border:1px solid var(--ds-card-stroke); border-radius:10px; padding:10px 14px; font-size:11.5px; color:var(--cockpit-dim); }
/* === 已安装插件列表项（renderPluginsStatus，套驾驶舱设计系统） === */
.plugin-item { background:var(--row-glass-2); border:1px solid var(--ds-card-stroke); border-radius:11px; padding:11px 15px; }
.plugin-item .pi-head { display:flex; align-items:center; justify-content:space-between; gap:10px; flex-wrap:wrap; }
.plugin-item .pi-name { font-weight:600; color:var(--tx1); font-size:13px; }
.plugin-item .pi-ver { color:var(--cockpit-dim); font-size:10.5px; }
.plugin-item .pi-controls { display:flex; align-items:center; gap:10px; }
.plugin-item .pi-prio { width:56px; margin-bottom:0; text-align:center; }
.plugin-item .pi-hint { font-size:10.5px; color:var(--cockpit-dim); margin-top:6px; }
.plugin-item .pi-warn { font-size:10.5px; color:var(--warn); margin-top:4px; }
.plugin-item .pi-details { margin-top:8px; }
.plugin-item .pi-details > pre { margin:6px 0 0; background:rgba(0,0,0,.25); border:1px solid var(--ds-card-stroke); border-radius:7px; padding:8px; font-size:10px; color:var(--tx2); overflow:auto; }
.ds-btn-danger { background:linear-gradient(135deg,#991b1b,#ef4444); }
.diag-shell { display: grid; grid-template-columns: 260px minmax(0,1fr); gap: 14px; }
.diag-console { align-items: start; }
.diag-kpi-rail { display: grid; gap: 10px; }
.diag-kpi { border: 1px solid var(--border); border-radius:16px; background: var(--card-bg); padding: 13px; }
.diag-kpi-rail .diag-kpi { min-height: 92px; }
.diag-kpi .lbl { color: var(--tx3); font-size: 10px; font-weight: 900; letter-spacing: .08em; text-transform: uppercase; }
.diag-kpi .val { margin-top: 7px; font-size: 22px; font-weight: 900; overflow-wrap: anywhere; }
.diag-main-panel { min-width: 0; }
.diag-toolbar { display: flex; gap: 8px; margin-bottom: 10px; }
.diag-toolbar .inp { flex: 1; }
.diag-table-shell { max-height: 390px; overflow: auto; border: 1px solid var(--cockpit-stroke); border-radius:20px; background: var(--diag-shell-bg); }
.diag-priority-row { display: grid; grid-template-columns: 96px 62px 54px minmax(220px, 1fr) 76px; gap: 10px; align-items: center; padding: 10px 12px; border-bottom: 1px solid rgba(148,163,184,.11); font-family: 'SF Mono', 'Courier New', monospace; font-size: 12px; }
.diag-priority-row.header { background: var(--diag-head-bg); color: var(--tx3); font-family: -apple-system, 'SF Pro Text', 'Helvetica Neue', sans-serif; font-size: 10px; font-weight: 900; letter-spacing: .09em; text-transform: uppercase; position: sticky; top: 0; z-index: 1; }
.diag-priority-row.warn { background: rgba(251,191,36,.08); }
.diag-priority-row.err { background: rgba(248,113,113,.09); }
.diag-timeline { margin-top: 14px; border: 1px solid var(--cockpit-stroke); border-radius:18px; padding: 14px; background: var(--diag-timeline-bg); }
.diag-timeline .label { color: var(--cockpit-muted); font-size: 10px; font-weight: 900; letter-spacing: .10em; text-transform: uppercase; }
.diag-bars { display: grid; gap: 8px; margin-top: 10px; }
.diag-bar { height: 9px; border-radius:999px; overflow: hidden; background: var(--diag-bar-bg); }
.diag-bar span { display: block; height: 100%; border-radius:inherit; background: linear-gradient(90deg, var(--accent), var(--info)); }
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
  .cockpit-primary { min-height: 0; padding: 18px; border-radius:22px; }
  .cockpit-state-panel { min-height: 160px; }
  .cockpit-state { font-size: 62px; }
  .cockpit-hold { min-height: 56px; border-radius:18px; font-size: 14px; }
  .cockpit-mode-grid { grid-template-columns: 1fr; }
  .diag-toolbar { flex-direction: column; }
  .diag-priority-row { grid-template-columns: 78px 46px 42px minmax(150px, 1fr) 54px; gap: 6px; font-size: 11px; padding: 9px 10px; }
}

/* === Scrollbar === */
::-webkit-scrollbar { width: 6px; }
::-webkit-scrollbar-track { background: var(--main-bg); }
::-webkit-scrollbar-thumb { background: var(--border); border-radius:3px; }

/* === Big Toggle Card === */
.big-toggle { text-align: center; padding: 24px; }
.big-toggle .toggle-visual { width: 80px; height: 40px; border-radius:20px;
  margin: 0 auto 8px; position: relative; transition: background .3s;
  box-shadow: 0 0 20px rgba(0,0,0,0.3); }
.big-toggle .toggle-visual .thumb { width: 36px; height: 36px; border-radius:50%;
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
.upload-area { border: 2px dashed var(--border); border-radius:10px;
  padding: 20px; text-align: center; cursor: pointer;
  transition: border-color .2s; }
.upload-area:hover { border-color: var(--accent-light); }

/* === Diag Grid === */
.diag-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 4px; }
.diag-item { background: var(--card-bg-alt); border-radius:4px;
  padding: 4px 8px; display: flex; justify-content: space-between;
  font-size: 11px; }
.diag-item .lbl { color: var(--tx3); }
.hero-status { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; margin-bottom: 20px; }
.feature-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 14px; }
.mode-note { margin-top: 10px; color: var(--tx3); font-size: 12px; line-height: 1.45; }
.cap-hidden{display:none!important}
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
    <h1 data-product-name data-single-text="Atlas Single CAN">Atlas T-2CAN</h1>
    <p data-i18n="控制面板">控制面板</p>
  </div>
  <div class="sidebar-nav">
    <div class="nav-item active" data-page="pg-overview"><span class="nav-icon">▣</span><span data-i18n="驾驶状态">驾驶状态</span></div>
    <div class="nav-item" data-page="pg-hardware"><span class="nav-icon">◇</span><span data-i18n="硬件模式">硬件模式</span></div>
    <div class="nav-item" data-page="pg-drive"><span class="nav-icon">◉</span><span data-i18n="驾驶风格">驾驶风格</span></div>
    <div class="nav-item" data-page="pg-speed"><span class="nav-icon">↗</span><span data-i18n="速度策略">速度策略</span></div>
    <div class="nav-item" data-page="pg-bus2" data-cap="can2" data-single-hide="1"><span class="nav-icon">✦</span><span data-i18n="CAN2 控制">CAN2 控制</span></div>
    <div class="nav-item" data-page="pg-defense"><span class="nav-icon">◈</span><span data-i18n="FSD 防护">FSD 防护</span></div>
    <div class="nav-item" data-page="pg-ota"><span class="nav-icon">⇧</span><span data-i18n="OTA 升级">OTA 升级</span></div>
    <div class="nav-item" data-page="pg-network"><span class="nav-icon">◎</span><span data-i18n="网络设置">网络设置</span></div>
    <div class="nav-item" data-page="pg-can"><span class="nav-icon">⌘</span><span data-i18n="CAN 诊断">CAN 诊断</span></div>
    <div class="nav-item" data-page="pg-shift" data-single-hide="1"><span class="nav-icon">⚙</span><span data-i18n="自动换挡">自动换挡</span></div>
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
    <span class="topbar-badge badge-ok" id="tb-status" data-i18n="已连接">已连接</span>
    <span class="topbar-dot warn" id="tb-dot-fsd"></span>
    <span class="topbar-badge badge-warn" id="tb-fsd" data-i18n="FSD 防护状态">FSD 防护状态</span>
    <span class="topbar-exp" id="tb-exp" data-i18n="实验项 0">实验项 0</span>
    <button class="mobile-theme-toggle" onclick="toggleTheme()" id="mobile-theme-btn" data-i18n="☀ 白天">☀ 白天</button>
    <span class="topbar-time" id="tb-up">00:00:00</span>
  </div>

  <!-- Content Area -->
  <div class="content" id="content">

    <!-- Page 1: Overview -->
    <div class="page active" id="pg-overview">
      <div class="desktop-home">
        <div class="cockpit-shell">
          <div class="cockpit-ambient"></div>
          <div class="cockpit-home">
        <div class="cockpit-status-bar">
          <div class="cockpit-title">
            <small data-product-name data-single-text="Atlas Single CAN">Atlas T-2CAN</small>
            <h2 data-i18n="驾驶状态中心">驾驶状态中心</h2>
          </div>
          <div class="cockpit-chip-row">
            <span class="cockpit-chip" id="st-module-ui" data-i18n="UI 待同步">UI 待同步</span>
            <span class="cockpit-chip" id="st-module-nvs" data-i18n="NVS 读取中">NVS 读取中</span>
            <span class="cockpit-chip" id="st-module-run" data-i18n="运行状态待检测">运行状态待检测</span>
          </div>
        </div>

        <div class="cockpit-grid">
          <div class="cockpit-home-main">
            <div class="cockpit-primary">
              <div class="cockpit-state-panel">
                <div class="cockpit-state-label" data-i18n="FSD 注入">FSD 注入</div>
                <div class="cockpit-state" id="fsd-label">OFF</div>
                <div class="cockpit-state-desc" id="ov-fsd-desc" data-i18n="模块总开关关闭；启用前请确认车辆处于安全状态。">模块总开关关闭；启用前请确认车辆处于安全状态。</div>
              </div>
              <button class="cockpit-hold" id="fsd-toggle" onclick="toggleFsd()" data-i18n="长按 2 秒启用 FSD 注入">长按 2 秒启用 FSD 注入</button>
            </div>

            <div class="cockpit-metrics">
              <div class="cockpit-metric"><div class="lbl" data-i18n="硬件模式">硬件模式</div><div class="val" id="ov-hw">Auto</div><div class="hint" data-i18n="自动识别">自动识别</div></div>
              <div class="cockpit-metric"><div class="lbl" data-i18n="驾驶风格">驾驶风格</div><div class="val" id="m-drive">Normal</div><div class="hint" data-i18n="均衡响应">均衡响应</div></div>
              <div class="cockpit-metric"><div class="lbl" data-i18n="速度策略">速度策略</div><div class="val" id="m-speed">auto</div><div class="hint" data-i18n="自动偏移">自动偏移</div></div>
              <div class="cockpit-metric"><div class="lbl" data-i18n="运行时间">运行时间</div><div class="val" id="ov-up">00:00:00</div><div class="hint" id="m-up" data-i18n="设备在线">设备在线</div></div>
            </div>

            <div class="cockpit-metrics" id="ov-can-row">
              <div class="cockpit-metric"><div class="lbl">CAN1</div><div class="val" id="s-can">Offline</div><div class="hint">--</div></div>
              <div class="cockpit-metric" data-single-hide="1" data-cap="can2"><div class="lbl">CAN2</div><div class="val" id="s-can2">Offline</div><div class="hint">CAN-B</div></div>
              <div class="cockpit-metric" id="ov-ap-metric"><div class="lbl" data-i18n="AP 激活">AP 激活</div><div class="val" id="ov-ap" data-i18n="等待">等待</div><div class="hint" id="ov-ap-hint" data-i18n="门控未触发">门控未触发</div></div>
              <div class="cockpit-metric"><div class="lbl" data-i18n="帧率">帧率</div><div class="val" id="s-fps">0.0 Hz</div><div class="hint">0.0 Hz</div></div>
              <div class="cockpit-metric"><div class="lbl" data-i18n="芯片温度">芯片温度</div><div class="val" id="s-temp">--</div><div class="hint" data-i18n="高于 60°C 标红">高于 60°C 标红</div></div>
            </div>
          </div>

          <div class="cockpit-vehicle">
            <div class="cockpit-status-bar">
              <div class="cockpit-title">
                <small data-i18n="车辆与防护">车辆与防护</small>
                <h2 data-i18n="车辆态势">车辆态势</h2>
              </div>
              <span class="cockpit-chip warn" id="tb-fsd-local" data-i18n="FSD 防护">FSD 防护</span>
            </div>
            <div class="cockpit-vehicle-visual" aria-label="车辆态势视觉">
              <div class="vehicle-lane"></div>
              <div class="vehicle-silhouette"></div>
            </div>
            <div class="cockpit-mode-grid">
              <div class="cockpit-mode active"><div>🎯</div><strong id="ov-drive-current">Normal</strong><span data-i18n="当前驾驶风格">当前驾驶风格</span></div>
              <div class="cockpit-mode"><div>🛡️</div><strong id="m-defense" data-i18n="防护中">防护中</strong><span data-i18n="FSD 防护状态">FSD 防护状态</span></div>
              <div class="cockpit-mode"><div>↗</div><strong id="s-soff">0</strong><span data-i18n="速度偏移">速度偏移</span></div>
            </div>
            <div class="cockpit-mode-grid">
              <div class="cockpit-mode"><div>⚙</div><strong id="s-hw">--</strong><span data-i18n="实际硬件模式">实际硬件模式</span></div>
              <div class="cockpit-mode"><div>📦</div><strong id="s-ver">--</strong><span data-i18n="固件版本">固件版本</span></div>
              <div class="cockpit-mode"><div>📡</div><strong id="m-rxtx">0 / 0</strong><span>RX / TX</span></div>
            </div>
            <div class="cockpit-safety-strip" id="m-alert" data-i18n="安全优先：暂无异常。实验功能请在停车和安全环境下验证。">安全优先：暂无异常。实验功能请在停车和安全环境下验证。</div>
          </div>
        </div>

        <div class="card">
          <div class="card-title" data-i18n="核心开关">核心开关</div>
          <div class="setting-row">
            <div>
              <div class="setting-name"><span data-i18n="FSD-V14 模式 ">FSD-V14 模式 </span><span class="exp-badge" data-i18n="实验">实验</span></div>
              <div class="setting-desc" data-i18n="独立切换 MAX/V14 实验 profile，不影响模块总开关。">独立切换 MAX/V14 实验 profile，不影响模块总开关。</div>
            </div>
            <label class="tgl"><input type="checkbox" id="ov-fsd-tgl" onchange="toggleV14Mode()"><div class="tgl-track"></div></label>
          </div>
          <div class="setting-row">
            <div>
              <div class="setting-name" data-i18n="模块总开关">模块总开关</div>
              <div class="setting-desc" data-i18n="控制当前 FSD 注入模块启停；状态会同步保存。">控制当前 FSD 注入模块启停；状态会同步保存。</div>
            </div>
            <label class="tgl"><input type="checkbox" id="ov-master-tgl" onchange="toggleFsd()"><div class="tgl-track"></div></label>
          </div>
          <div class="setting-row">
            <div>
              <div class="setting-name" data-i18n="开机自动启用">开机自动启用</div>
              <div class="setting-desc" data-i18n="设备重启后按保存值恢复 FSD 注入。">设备重启后按保存值恢复 FSD 注入。</div>
            </div>
            <label class="tgl"><input type="checkbox" id="fsd-boot-tgl" onchange="saveConfig()"><div class="tgl-track"></div></label>
          </div>
        </div>

        <div class="card">
          <div class="card-title" data-i18n="设备信息">设备信息</div>
          <div class="stats">
            <div class="stat"><div class="stat-lbl">FSD-V14</div><div class="stat-val v-dim" id="ov-v14" data-i18n="待机">待机</div></div>
            <div class="stat"><div class="stat-lbl">CAN1 RX/TX</div><div class="stat-val v-info" id="s-rx">0/0</div></div>
            <div class="stat"><div class="stat-lbl">TX Errors</div><div class="stat-val v-dim" id="s-txerr">0</div></div>
            <div class="stat"><div class="stat-lbl" data-i18n="跟随距离">跟随距离</div><div class="stat-val v-dim" id="s-fd">--</div></div>
            <div class="stat"><div class="stat-lbl" data-i18n="车辆 OTA">车辆 OTA</div><div class="stat-val v-acc" id="s-vota" data-i18n="正常">正常</div></div>
            <div class="stat"><div class="stat-lbl" data-i18n="OTA 确认">OTA 确认</div><div class="stat-val v-dim" id="s-vota-cnt">0</div></div>
          </div>
        </div>

        <div class="card">
          <div class="card-title" data-i18n="FSD 注入策略">FSD 注入策略</div>
          <div class="setting-desc" style="margin-bottom:10px" data-i18n="稳定模式仅注入 mux0 bit46，禁用 mux1；Tesla Controller 对齐模式用于受控 A/B 测试。">稳定模式仅注入 mux0 bit46，禁用 mux1；Tesla Controller 对齐模式用于受控 A/B 测试。</div>
          <div class="sel-cards c3" id="legacy-fsd-policy-cards" style="margin-bottom:12px">
            <div class="sel-card active" data-policy="stable" onclick="setLegacyFsdPolicy('stable')">
              <div class="sel-lbl" data-i18n="推荐">推荐</div>
              <div class="sel-name" data-i18n="稳定模式">稳定模式</div>
            </div>
            <div class="sel-card" data-policy="experimental" onclick="setLegacyFsdPolicy('experimental')">
              <div class="sel-lbl">A/B</div>
              <div class="sel-name" data-i18n="实验模式">实验模式</div>
            </div>
            <div class="sel-card" data-policy="legacy_tesla_parity" onclick="setLegacyFsdPolicy('legacy_tesla_parity')">
              <div class="sel-lbl" data-i18n="对齐">对齐</div>
              <div class="sel-name" data-i18n="Tesla Controller 对齐模式">Tesla Controller 对齐模式</div>
            </div>
          </div>
          <div class="stats">
            <div class="stat"><div class="stat-lbl" data-i18n="当前策略">当前策略</div><div class="stat-val v-acc" id="legacy-fsd-strategy" data-i18n="稳定">稳定</div></div>
            <div class="stat"><div class="stat-lbl">mux0</div><div class="stat-val v-dim" id="legacy-fsd-mux0">--</div></div>
            <div class="stat"><div class="stat-lbl">mux1</div><div class="stat-val v-dim" id="legacy-fsd-mux1">--</div></div>
            <div class="stat"><div class="stat-lbl" data-i18n="触发源">触发源</div><div class="stat-val v-dim" id="legacy-fsd-trigger">--</div></div>
          </div>
        </div>

        <div class="card">
          <div class="card-title" data-i18n="紧急控制">紧急控制</div>
          <div style="display:flex;gap:8px">
            <button class="btn btn-danger" onclick="rebootDevice()" style="flex:1" data-i18n="重启设备">重启设备</button>
            <button class="btn" onclick="resetStats()" style="flex:1" data-i18n="重置计数">重置计数</button>
          </div>
        </div>

          </div>
        </div>
      </div>

      <div class="mobile-remote">
        <div class="mobile-app-shell">
        <div class="cockpit-status-bar">
          <div class="cockpit-title">
            <small>ATLAS REMOTE</small>
            <h2 data-i18n="现场遥控">现场遥控</h2>
          </div>
          <span class="cockpit-chip ok" data-i18n="连接到设备热点">连接到设备热点</span>
        </div>
        <div class="mobile-primary-card">
          <div class="cockpit-state-label" data-i18n="FSD 注入">FSD 注入</div>
          <div class="mobile-state" id="m-fsd">OFF</div>
          <p data-i18n="普通点击不会触发危险动作；请长按确认后再启用模块。">普通点击不会触发危险动作；请长按确认后再启用模块。</p>
          <button id="m-fsd-tgl" class="mobile-hold" onclick="toggleFsd()" data-i18n="长按启用">长按启用</button>
        </div>
        <div class="mobile-remote-grid">
          <div class="mobile-remote-card"><div class="lbl">CAN1</div><div class="val" id="m-can">--</div></div>
          <div class="mobile-remote-card" data-single-hide="1" data-cap="can2"><div class="lbl">CAN2</div><div class="val" id="m-can2">--</div></div>
          <div class="mobile-remote-card"><div class="lbl" data-i18n="硬件模式">硬件模式</div><div class="val" id="m-hw">Auto</div></div>
          <div class="mobile-remote-card"><div class="lbl" data-i18n="帧率">帧率</div><div class="val" id="m-fps">0.0 Hz</div></div>
        </div>
        <div class="mobile-primary-card">
          <div class="cockpit-state-label" data-i18n="紧急控制">紧急控制</div>
          <div style="display:flex;gap:8px;margin-top:8px">
            <button class="btn btn-danger" onclick="rebootDevice()" style="flex:1" data-i18n="重启设备">重启设备</button>
            <button class="btn" onclick="resetStats()" style="flex:1" data-i18n="重置计数">重置计数</button>
          </div>
        </div>
        <div class="mobile-risk-note" data-i18n="危险操作需点击确认：重启设备会中断当前模块；重置计数会清零统计。">危险操作需点击确认：重启设备会中断当前模块；重置计数会清零统计。</div>
        </div>
      </div>
    </div>

    <!-- Page 2: Hardware Config -->
    <div class="page" id="pg-hardware">
<div class="page-title" data-i18n="硬件模式">硬件模式</div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl" data-i18n="UI 显示状态">UI 显示状态</div><div class="val" id="st-hw-ui">Auto</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="NVS 持久化状态">NVS 持久化状态</div><div class="val" id="st-hw-nvs">--</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="实际 CAN 运行状态">实际 CAN 运行状态</div><div class="val" id="st-hw-run">--</div></div>
</div>
<!-- HW Version Selection -->
<div class="card">
  <div class="card-title" data-i18n="FSD 硬件模式">FSD 硬件模式</div>
  <div class="card-subtitle" data-i18n="选择车辆自动驾驶硬件版本；Auto 会使用固件现有探测逻辑。">选择车辆自动驾驶硬件版本；Auto 会使用固件现有探测逻辑。</div>
  <div class="sel-cards c4" id="hw-cards">
    <div class="sel-card active" onclick="setHW(3)">
      <div class="sel-lbl" data-i18n="推荐">推荐</div>
      <div class="sel-name">Auto</div>
    </div>
    <div class="sel-card" onclick="setHW(0)">
      <div class="sel-lbl" data-i18n="旧版">旧版</div>
      <div class="sel-name">Legacy</div>
    </div>
    <div class="sel-card" onclick="setHW(1)">
      <div class="sel-lbl" data-i18n="第三代">第三代</div>
      <div class="sel-name">HW3.0</div>
    </div>
    <div class="sel-card" onclick="setHW(2)">
      <div class="sel-lbl" data-i18n="第四代">第四代</div>
      <div class="sel-name">HW4.0</div>
    </div>
  </div>
</div>

<!-- 插件管理（驾驶舱风格，全中文） -->
<div class="card cockpit-card" id="plugins-card">
  <div class="ck-head"><h3><span>🧩</span><span data-i18n="插件管理">插件管理</span></h3><span class="ck-chip ok" id="plugins-count-pill" data-i18n="已装 0 个">已装 0 个</span></div>
  <div class="ctl-section" data-i18n="添加插件">添加插件</div>
  <div class="install-grid">
    <div class="install-cell"><div class="ii">🔗</div><div class="it" data-i18n="URL 安装">URL 安装</div>
      <input id="plugin-url" class="mock-input" placeholder="https://.../plugin.json">
      <button class="ds-btn" onclick="installPluginUrl()" data-i18n="安装">安装</button></div>
    <div class="install-cell"><div class="ii">📤</div><div class="it" data-i18n="上传文件">上传文件</div>
      <input id="plugin-file" type="file" accept=".json,application/json" class="mock-input">
      <button class="ds-btn" onclick="uploadPluginJson()" data-i18n="上传">上传</button>
      <div id="plugin-upload-status" class="ds-btn-muted"></div></div>
    <div class="install-cell"><div class="ii">📋</div><div class="it" data-i18n="粘贴 JSON">粘贴 JSON</div>
      <textarea id="plugin-json" rows="3" class="mock-input" placeholder='{"name":"...","rules":[]}'></textarea>
      <button class="ds-btn" onclick="installPluginJson()" data-i18n="解析安装">解析安装</button></div>
  </div>
  <div class="ctl-section" data-i18n="已安装插件">已安装插件</div>
  <div id="plugin-list" class="plugin-list"></div>
  <div class="ctl-section" data-i18n="GTW2047 重放倍率">GTW2047 重放倍率</div>
  <select id="plugin-replay-count" onchange="setPluginReplayCount(this.value)" class="ds-select"><option value="1">1×</option><option value="2">2×</option><option value="3">3×</option><option value="4">4×</option><option value="5">5×</option></select>
  <div class="hint" data-i18n="仅重复已启用插件生成的 0x7FF 修改帧">仅重复已启用插件生成的 0x7FF 修改帧</div>
  <details class="collapse"><summary data-i18n="规则测试器">规则测试器</summary><textarea id="plugin-editor-json" rows="6" class="mock-input"></textarea><button class="ds-btn" onclick="runPluginRuleTest()" data-i18n="规则测试">规则测试</button><pre id="plugin-rule-test-result"></pre></details>
</div>
    </div>

    <!-- Page 3: Drive Mode -->
    <div class="page" id="pg-drive">
<div class="ck-head"><h3><span>◉</span><span data-i18n="驾驶风格">驾驶风格</span></h3><span class="ck-chip" id="st-drive-pill">Normal</span></div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl" data-i18n="UI 显示状态">UI 显示状态</div><div class="val" id="st-drive-ui">Normal</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="NVS 持久化状态">NVS 持久化状态</div><div class="val" id="st-drive-nvs">--</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="实际 CAN 运行状态">实际 CAN 运行状态</div><div class="val" id="st-drive-run">--</div></div>
</div>
<div class="card cockpit-card">
  <div class="card-subtitle" data-i18n="选择速度策略映射到 /drive_profile">选择速度策略映射到 /drive_profile</div>
  <div class="drive-grid" id="drive-cards">
    <div class="drive-card" onclick="setDriveMode('auto')">
      <div class="drive-icon">🤖</div>
      <div class="drive-name">Auto</div>
      <div class="drive-desc" data-i18n="系统自动选择最佳模式">系统自动选择最佳模式</div>
    </div>
    <div class="drive-card" onclick="setDriveMode('sloth')">
      <div class="drive-icon">🦥</div>
      <div class="drive-name">Sloth</div>
      <div class="drive-desc" data-i18n="最低速档，适合拥堵跟车">最低速档，适合拥堵跟车</div>
    </div>
    <div class="drive-card" onclick="setDriveMode('chill')">
      <div class="drive-icon">😌</div>
      <div class="drive-name">Chill</div>
      <div class="drive-desc" data-i18n="舒适驾驶，温和加速">舒适驾驶，温和加速</div>
    </div>
    <div class="drive-card active" onclick="setDriveMode('normal')">
      <div class="drive-icon">🎯</div>
      <div class="drive-name">Normal</div>
      <div class="drive-desc" data-i18n="标准模式，均衡性能">标准模式，均衡性能</div>
    </div>
    <div class="drive-card" onclick="setDriveMode('hurry')">
      <div class="drive-icon">🚀</div>
      <div class="drive-name">Hurry</div>
      <div class="drive-desc" data-i18n="积极提速，更快响应">积极提速，更快响应</div>
    </div>
    <div class="drive-card" onclick="setDriveMode('max')">
      <div class="drive-icon">⚡</div>
      <div class="drive-name">MAX</div>
      <div class="drive-desc"><span data-i18n="最大性能 ">最大性能 </span><span class="exp-badge" data-i18n="实验">实验</span></div>
    </div>
  </div>
  <div class="mode-note"><span data-i18n="当前选择：">当前选择：</span><span id="drive-current">Normal</span></div>
</div>
    </div>

    <!-- Page 4: Speed Offset -->
    <div class="page" id="pg-speed">
<div class="ck-head"><h3><span>◉</span><span data-i18n="速度策略">速度策略</span></h3><span class="ck-chip" id="st-speed-pill">auto</span></div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl" data-i18n="UI 显示状态">UI 显示状态</div><div class="val" id="st-speed-ui" data-i18n="待同步">待同步</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="NVS 持久化状态">NVS 持久化状态</div><div class="val" id="st-speed-nvs" data-i18n="读取中">读取中</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="实际 CAN/网络运行状态">实际 CAN/网络运行状态</div><div class="val" id="st-speed-run" data-i18n="待检测">待检测</div></div>
</div>
<div class="card">
  <div class="card-title" data-i18n="Legacy 智能速度偏移">Legacy 智能速度偏移</div>
  <div class="card-subtitle" data-i18n="独立四档：关闭 / 手动 / 自动 / 自定义。0x2F8 只读限速，0x3EE mux0 输出。">独立四档：关闭 / 手动 / 自动 / 自定义。0x2F8 只读限速，0x3EE mux0 输出。</div>
  <div class="sel-cards c4" id="legacy-speed-mode" style="margin:12px 0">
    <div class="sel-card" onclick="setLegacySpeedMode('off')"><div class="sel-lbl" data-i18n="模式">模式</div><div class="sel-name" data-i18n="关闭">关闭</div></div>
    <div class="sel-card" onclick="setLegacySpeedMode('manual')"><div class="sel-lbl" data-i18n="模式">模式</div><div class="sel-name" data-i18n="手动">手动</div></div>
    <div class="sel-card" onclick="setLegacySpeedMode('auto')"><div class="sel-lbl" data-i18n="模式">模式</div><div class="sel-name" data-i18n="自动">自动</div></div>
    <div class="sel-card" onclick="setLegacySpeedMode('custom')"><div class="sel-lbl" data-i18n="模式">模式</div><div class="sel-name" data-i18n="自定义">自定义</div></div>
  </div>
  <div class="setting-row" id="legacy-speed-manual-row">
    <div><div class="setting-name" data-i18n="手动偏移">手动偏移</div><div class="setting-desc" data-i18n="固定加值，范围 0-33 km/h">固定加值，范围 0-33 km/h</div></div>
    <input class="inp" type="number" min="0" max="33" id="legacy-speed-manual" value="0" onchange="saveLegacySpeedConfig()" style="width:86px">
  </div>
  <div id="legacy-speed-custom-panel">
    <div class="setting-row"><div><div class="setting-name">0-50 km/h</div><div class="setting-desc" data-i18n="低速区间">低速区间</div></div><input class="inp" type="number" min="0" max="63" id="legacy-speed-cp1" value="50" onchange="saveLegacySpeedConfig()" style="width:86px"></div>
    <div class="setting-row"><div><div class="setting-name">51-70 km/h</div><div class="setting-desc" data-i18n="城市快速路">城市快速路</div></div><input class="inp" type="number" min="0" max="63" id="legacy-speed-cp2" value="30" onchange="saveLegacySpeedConfig()" style="width:86px"></div>
    <div class="setting-row"><div><div class="setting-name">71-100 km/h</div><div class="setting-desc" data-i18n="高速巡航">高速巡航</div></div><input class="inp" type="number" min="0" max="63" id="legacy-speed-cp3" value="20" onchange="saveLegacySpeedConfig()" style="width:86px"></div>
    <div class="setting-row"><div><div class="setting-name">101+ km/h</div><div class="setting-desc" data-i18n="高限速区间">高限速区间</div></div><input class="inp" type="number" min="0" max="63" id="legacy-speed-cp4" value="10" onchange="saveLegacySpeedConfig()" style="width:86px"></div>
  </div>
  <div class="setting-row">
    <div><div class="setting-name" data-i18n="目标总线">目标总线</div><div class="setting-desc" data-i18n="Auto 遇到双通道镜像时停止写入">Auto 遇到双通道镜像时停止写入</div></div>
    <select class="inp" id="legacy-speed-bus" onchange="saveLegacySpeedConfig()" style="width:112px">
      <option value="Auto">Auto</option>
      <option value="CAN1">CAN1</option>
      <option value="CAN2">CAN2</option>
    </select>
  </div>
  <div class="setting-row">
    <div><div class="setting-name" data-i18n="下降缓冲">下降缓冲</div><div class="setting-desc" data-i18n="限速下降时平滑调整目标速度">限速下降时平滑调整目标速度</div></div>
    <label class="tgl"><input type="checkbox" id="legacy-speed-smooth" onchange="saveLegacySpeedConfig()" checked><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div><div class="setting-name" data-i18n="缓降速率">缓降速率</div><div class="setting-desc">1-20 km/h/s</div></div>
    <input class="inp" type="number" min="1" max="20" id="legacy-speed-rate" value="5" onchange="saveLegacySpeedConfig()" style="width:86px">
  </div>
  <div class="diag-grid" style="margin-top:10px">
    <div class="diag-item"><span class="lbl">0x2F8 RX</span><span class="v-dim" id="sp-legacy-rx2f8">0</span></div>
    <div class="diag-item"><span class="lbl">0x3EE RX</span><span class="v-dim" id="sp-legacy-rx3ee">0</span></div>
    <div class="diag-item"><span class="lbl">TX OK / Fail</span><span class="v-dim" id="sp-legacy-tx">0 / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="阻断原因">阻断原因</span><span class="v-dim" id="sp-legacy-block">--</span></div>
  </div>
</div>
<div class="card cockpit-card">
  <div class="card-title" data-i18n="速度策略模式">速度策略模式</div>
  <div class="card-subtitle" data-i18n="HW3/HW4：固定百分比 / 自动偏移 / 自定义偏移。Legacy 请使用上方独立四档控制。">HW3/HW4：固定百分比 / 自动偏移 / 自定义偏移。Legacy 请使用上方独立四档控制。</div>
  <div class="sel-cards c3" id="speed-mode-tabs" style="margin-bottom:12px">
    <div class="sel-card" onclick="showSpeedMode('fixed')"><div class="sel-lbl" data-i18n="模式">模式</div><div class="sel-name" data-i18n="固定百分比">固定百分比</div></div>
    <div class="sel-card active" onclick="showSpeedMode('auto')"><div class="sel-lbl" data-i18n="模式">模式</div><div class="sel-name" data-i18n="自动偏移">自动偏移</div></div>
    <div class="sel-card" onclick="showSpeedMode('custom')"><div class="sel-lbl" data-i18n="模式">模式</div><div class="sel-name" data-i18n="自定义偏移">自定义偏移</div></div>
  </div>
  <div class="mode-note"><span data-i18n="当前策略：">当前策略：</span><span id="speed-current">auto</span></div>
</div>

<!-- Realtime Offset -->
<div class="card">
  <div class="card-title" data-i18n="实时偏移">实时偏移</div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl" data-i18n="速度限制 speedLimit">速度限制 speedLimit</span><span class="v-info" id="sp-limit">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="实际偏移 actOffset">实际偏移 actOffset</span><span class="v-acc" id="sp-act-offset">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="生效模式">生效模式</span><span class="v-dim" id="sp-active-mode">--</span></div>
    <div class="diag-item"><span class="lbl">Wire Encoding</span><span class="v-dim" id="sp-wire">--</span></div>
    <div class="diag-item"><span class="lbl">Fused Raw</span><span class="v-dim" id="sp-raw">--</span></div>
    <div class="diag-item"><span class="lbl">Stock Offset</span><span class="v-dim" id="sp-stock">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="Legacy 限速来源">Legacy 限速来源</span><span class="v-info" id="sp-legacy-source">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="Legacy 目标速度">Legacy 目标速度</span><span class="v-acc" id="sp-legacy-target">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="Legacy 目标总线">Legacy 目标总线</span><span class="v-dim" id="sp-legacy-bus">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="Legacy 运行状态">Legacy 运行状态</span><span class="v-dim" id="sp-legacy-runtime">--</span></div>
  </div>
</div>

<div class="card" id="speed-panel-fixed">
  <div class="card-title" data-i18n="固定百分比">固定百分比</div>
  <div class="card-subtitle" data-i18n="所有速度限制统一使用同一个偏移百分比">所有速度限制统一使用同一个偏移百分比</div>
  <div class="sel-cards c3" id="speed-fixed-pct" style="margin-bottom:10px">
    <div class="sel-card" onclick="setSpeedFixedPct(0)"><div class="sel-lbl" data-i18n="固定">固定</div><div class="sel-name">0%</div></div>
    <div class="sel-card" onclick="setSpeedFixedPct(10)"><div class="sel-lbl" data-i18n="固定">固定</div><div class="sel-name">10%</div></div>
    <div class="sel-card" onclick="setSpeedFixedPct(20)"><div class="sel-lbl" data-i18n="固定">固定</div><div class="sel-name">20%</div></div>
    <div class="sel-card active" onclick="setSpeedFixedPct(30)"><div class="sel-lbl" data-i18n="固定">固定</div><div class="sel-name">30%</div></div>
    <div class="sel-card" onclick="setSpeedFixedPct(40)"><div class="sel-lbl" data-i18n="固定">固定</div><div class="sel-name">40%</div></div>
    <div class="sel-card" onclick="setSpeedFixedPct(50)"><div class="sel-lbl" data-i18n="固定">固定</div><div class="sel-name">50%</div></div>
  </div>
  <div class="mode-note" data-i18n="保存时会 POST /speed_strategy=fixed，并通过 /speed_custom 写入 manualPct。">保存时会 POST /speed_strategy=fixed，并通过 /speed_custom 写入 manualPct。</div>
</div>

<div class="card" id="speed-panel-auto" style="display:none">
  <div class="card-title" data-i18n="自动偏移算法">自动偏移算法</div>
  <div class="card-subtitle" data-i18n="固件按当前 speedLimit 自动选择偏移比例，适合日常使用">固件按当前 speedLimit 自动选择偏移比例，适合日常使用</div>
  <table class="tbl">
    <thead><tr><th data-i18n="速度限制">速度限制</th><th data-i18n="自动偏移">自动偏移</th></tr></thead>
    <tbody>
      <tr><td>≤ 40 km/h</td><td data-i18n="+50%，封顶 60">+50%，封顶 60</td></tr>
      <tr><td>≤ 60 km/h</td><td data-i18n="+50%，封顶 90">+50%，封顶 90</td></tr>
      <tr><td>≤ 90 km/h</td><td data-i18n="+30%，封顶 117">+30%，封顶 117</td></tr>
      <tr><td>≤ 110 km/h</td><td data-i18n="+20%，封顶 132">+20%，封顶 132</td></tr>
      <tr><td>> 110 km/h</td><td data-i18n="+10%，封顶 132">+10%，封顶 132</td></tr>
    </tbody>
  </table>
  <button class="btn" onclick="setSpeedStrategy('auto')" style="margin-top:12px;width:100%" data-i18n="启用自动偏移">启用自动偏移</button>
</div>

<div class="card" id="speed-panel-custom" style="display:none">
  <div class="card-title" data-i18n="自定义 4 区间偏移">自定义 4 区间偏移</div>
  <div class="card-subtitle" data-i18n="填写每个速度区间的偏移百分比，范围 0-50%">填写每个速度区间的偏移百分比，范围 0-50%</div>
  <div class="setting-row"><div><div class="setting-name">0-50 km/h</div><div class="setting-desc" data-i18n="低速区间 cp1">低速区间 cp1</div></div><input class="inp" type="number" min="0" max="50" id="speed-cp1" value="30" style="width:86px"></div>
  <div class="setting-row"><div><div class="setting-name">51-70 km/h</div><div class="setting-desc" data-i18n="城市快速路 cp2">城市快速路 cp2</div></div><input class="inp" type="number" min="0" max="50" id="speed-cp2" value="20" style="width:86px"></div>
  <div class="setting-row"><div><div class="setting-name">71-100 km/h</div><div class="setting-desc" data-i18n="高速巡航 cp3">高速巡航 cp3</div></div><input class="inp" type="number" min="0" max="50" id="speed-cp3" value="10" style="width:86px"></div>
  <div class="setting-row"><div><div class="setting-name">101+ km/h</div><div class="setting-desc" data-i18n="高限速区间 cp4">高限速区间 cp4</div></div><input class="inp" type="number" min="0" max="50" id="speed-cp4" value="10" style="width:86px"></div>
  <button class="btn" onclick="saveSpeedCustom()" style="width:100%;margin-top:10px" data-i18n="保存自定义偏移">保存自定义偏移</button>
</div>

<!-- Encoding -->

<div class="card">
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="速度编码方式">速度编码方式</div>
      <div class="setting-desc" data-i18n="选择 CAN 总线速度编码">选择 CAN 总线速度编码</div>
    </div>
    <select class="inp" id="hw3-enc" onchange="saveHw3Speed()" style="width:120px">
      <option value="0" data-i18n="默认">默认</option>
      <option value="1" data-i18n="编码 A">编码 A</option>
      <option value="2" data-i18n="编码 B">编码 B</option>
    </select>
  </div>
</div>
    </div>

    <!-- Page 5: CAN2 Control -->
    <div class="page" id="pg-bus2" data-cap="can2" data-single-hide="1">
<div class="page-title"><span data-i18n="CAN2 控制 ">CAN2 控制 </span><span class="exp-badge" data-i18n="未验证/实验">未验证/实验</span></div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl" data-i18n="UI 显示状态">UI 显示状态</div><div class="val" id="st-light-ui" data-i18n="待同步">待同步</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="NVS 持久化状态">NVS 持久化状态</div><div class="val" id="st-light-nvs" data-i18n="读取中">读取中</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="实际 CAN/网络运行状态">实际 CAN/网络运行状态</div><div class="val" id="st-light-run" data-i18n="待检测">待检测</div></div>
</div>
<!-- Service Mode（beta04 从页底移至顶部，并加状态显示） -->
<div class="card">
  <div class="setting-row">
    <div>
      <div class="setting-name">Service Mode</div>
      <div class="setting-desc" data-i18n="进入 Service 需在车机屏幕操作，本页仅显示状态">进入 Service 需在车机屏幕操作，本页仅显示状态</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="svc-mode-tgl" onchange="toggleServiceMode()">
      <div class="tgl-track"></div>
    </label>
  </div>
  <div class="diag-grid" style="margin-top:8px">
    <div class="diag-item"><span class="lbl" data-i18n="当前状态">当前状态</span><span class="v-dim" id="svc-mode-status" data-i18n="未激活">未激活</span></div>
  </div>
</div>

<!-- CAN2 Status -->
<div class="stats">
  <div class="stat"><div class="stat-lbl" data-i18n="CAN2 状态">CAN2 状态</div><div class="stat-val v-dim" id="b2-status">Offline</div></div>
  <div class="stat"><div class="stat-lbl">CAN2 RX</div><div class="stat-val v-info" id="b2-rx">0</div></div>
  <div class="stat"><div class="stat-lbl">CAN2 TX</div><div class="stat-val v-info" id="b2-tx">0</div></div>
  <div class="stat"><div class="stat-lbl">CAN2 TXErr</div><div class="stat-val v-dim" id="b2-txerr">0</div></div>
  <div class="stat"><div class="stat-lbl">CAN2 EFLG</div><div class="stat-val v-dim" id="b2-eflg">0x00</div></div>
  <div class="stat"><div class="stat-lbl" data-i18n="已发现 ID">已发现 ID</div><div class="stat-val v-acc" id="b2-ids">0</div></div>
</div>

<!-- CAN2 ID Table -->
<div class="card">
  <div class="card-title"><span data-i18n="CAN2 ID 列表 ">CAN2 ID 列表 </span><span style="color:var(--tx3);font-size:11px;font-weight:400" id="b2-count">(0)</span></div>
  <div style="max-height:200px;overflow-y:auto">
    <table class="tbl">
      <thead><tr><th>ID</th><th>DLC</th><th data-i18n="数据">数据</th><th data-i18n="计数">计数</th></tr></thead>
      <tbody id="b2-rows"></tbody>
    </table>
  </div>
</div>

<!-- Stalk Test -->
<div class="card">
  <div class="card-title" data-i18n="CAN2 控制">CAN2 控制</div>
  <div class="card-subtitle" data-i18n="映射到 /lighting_config，并用 CAN2 stalk_test 执行爆闪序列">映射到 /lighting_config，并用 CAN2 stalk_test 执行爆闪序列</div>
  <div class="setting-row">
    <div>
      <div class="setting-name"><span data-i18n="爆闪开关 ">爆闪开关 </span><span class="exp-badge" data-i18n="实验">实验</span></div>
      <div class="setting-desc" data-i18n="启用后按配置执行多次拨杆注入">启用后按配置执行多次拨杆注入</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="light-enabled-tgl" onchange="saveLightingConfig()">
      <div class="tgl-track"></div>
    </label>
  </div>
  <div class="sel-cards c4" id="light-preset" style="margin-bottom:10px">
    <div class="sel-card active" onclick="setLightPreset(3)"><div class="sel-lbl" data-i18n="次数">次数</div><div class="sel-name">3</div></div>
    <div class="sel-card" onclick="setLightPreset(5)"><div class="sel-lbl" data-i18n="次数">次数</div><div class="sel-name">5</div></div>
    <div class="sel-card" onclick="setLightPreset(7)"><div class="sel-lbl" data-i18n="次数">次数</div><div class="sel-name">7</div></div>
    <div class="sel-card" onclick="setLightPreset(10)"><div class="sel-lbl" data-i18n="次数">次数</div><div class="sel-name">10</div></div>
  </div>
  <div class="sel-cards c3" id="light-frequency" style="margin-bottom:10px">
    <div class="sel-card" onclick="setLightFrequency('slow')"><div class="sel-lbl" data-i18n="频率">频率</div><div class="sel-name">slow</div></div>
    <div class="sel-card active" onclick="setLightFrequency('medium')"><div class="sel-lbl" data-i18n="频率">频率</div><div class="sel-name">medium</div></div>
    <div class="sel-card" onclick="setLightFrequency('fast')"><div class="sel-lbl" data-i18n="频率">频率</div><div class="sel-name">fast</div></div>
  </div>
  <div style="display:flex;gap:8px;margin-bottom:10px">
    <button class="btn btn-outline" onclick="stalkTest('PULL')" style="flex:1" data-i18n="PULL (闪光)">PULL (闪光)</button>
    <button class="btn btn-outline" onclick="stalkTest('PUSH')" style="flex:1" data-i18n="PUSH (远光)">PUSH (远光)</button>
  </div>
  <div style="display:flex;gap:8px;margin-bottom:10px">
    <button class="btn" onclick="strobeTest('PULL')" style="flex:1" data-i18n="执行爆闪">执行爆闪</button>
    <button class="btn btn-outline" onclick="strobeTest('PUSH')" style="flex:1" data-i18n="远光序列">远光序列</button>
  </div>
  <div class="setting-row">
    <div class="setting-name" data-i18n="持续时间 (ms)">持续时间 (ms)</div>
    <input class="inp" type="number" id="stalk-dur" value="500" style="width:100px">
  </div>
  <div class="setting-row">
    <div class="setting-name" data-i18n="状态">状态</div>
    <div class="setting-desc" id="stalk-status" data-i18n="空闲">空闲</div>
  </div>
</div>

<!-- Double Pull Flash Burst -->
<div class="card">
  <div class="card-title"><span data-i18n="双拨爆闪 ">双拨爆闪 </span><span class="exp-badge" data-i18n="实验">实验</span></div>
  <div class="card-subtitle" data-i18n="2秒内两次真实 PULL 触发固件侧 0x249 PULL burst，默认关闭">2秒内两次真实 PULL 触发固件侧 0x249 PULL burst，默认关闭</div>
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="启用双拨触发">启用双拨触发</div>
      <div class="setting-desc" data-i18n="开启后，真实超车闪双拨才会自动执行预设爆闪">开启后，真实超车闪双拨才会自动执行预设爆闪</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="burst-enabled-tgl" onchange="toggleBurst(this)">
      <div class="tgl-track"></div>
    </label>
  </div>
  <div class="sel-cards c4" id="burst-preset" style="margin-bottom:10px">
    <div class="sel-card" onclick="setBurstPreset('A')"><div class="sel-lbl" data-i18n="A 短促">A 短促</div><div class="sel-name">2× 250ms</div></div>
    <div class="sel-card active" onclick="setBurstPreset('B')"><div class="sel-lbl" data-i18n="B 标准">B 标准</div><div class="sel-name">3× 180ms</div></div>
    <div class="sel-card" onclick="setBurstPreset('C')"><div class="sel-lbl" data-i18n="C 明显">C 明显</div><div class="sel-name">5× 150ms</div></div>
    <div class="sel-card" onclick="setBurstPreset('D')"><div class="sel-lbl" data-i18n="D 长闪">D 长闪</div><div class="sel-name">7× 200ms</div></div>
  </div>
  <div class="stats">
    <div class="stat"><div class="stat-lbl" data-i18n="双拨状态">双拨状态</div><div class="stat-val v-dim" id="burst-status" data-i18n="关闭">关闭</div></div>
    <div class="stat"><div class="stat-lbl" data-i18n="当前参数">当前参数</div><div class="stat-val v-info" id="burst-params">3× 180/180ms</div></div>
    <div class="stat"><div class="stat-lbl" data-i18n="剩余相位">剩余相位</div><div class="stat-val v-dim" id="burst-phases">0</div></div>
  </div>
</div>

    </div>

    <!-- Page 6: FSD Defense -->
    <div class="page" id="pg-defense">
<div class="ck-head"><h3><span>◉</span><span data-i18n="FSD 防护">FSD 防护</span></h3><span class="ck-chip" id="st-defense-pill" data-i18n="部分实验">部分实验</span></div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl" data-i18n="UI 显示状态">UI 显示状态</div><div class="val" id="st-defense-ui" data-i18n="待同步">待同步</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="NVS 持久化状态">NVS 持久化状态</div><div class="val" id="st-defense-nvs" data-i18n="读取中">读取中</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="实际 CAN/网络运行状态">实际 CAN/网络运行状态</div><div class="val" id="st-defense-run" data-i18n="待检测">待检测</div></div>
</div>
<!-- Independent defense master and feature toggles -->
<div class="card cockpit-card">
  <div class="card-title" data-i18n="FSD 防护状态">FSD 防护状态</div>
  <div class="card-subtitle" data-i18n="通用防御与左滚轮 DND 分别独立控制">通用防御与左滚轮 DND 分别独立控制</div>
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="FSD 防御总开关">FSD 防御总开关</div>
      <div class="setting-desc" data-i18n="仅联动 Ban Shield 通用防御；左滚轮免打扰与 8.3.6防甩机制不受此开关联动">仅联动 Ban Shield 通用防御；左滚轮免打扰与 8.3.6防甩机制不受此开关联动</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="def-master-tgl" onchange="saveDefenseConfig()">
      <div class="tgl-track"></div>
    </label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="AP 注入门控">AP 注入门控</div>
      <div class="setting-desc" data-i18n="Legacy 仅在 0x399 AP 活动后才允许注入（服务端强制，fail-closed）">Legacy 仅在 0x399 AP 活动后才允许注入（服务端强制，fail-closed）</div>
    </div>
    <label class="tgl"><input type="checkbox" id="ap-core-gate-tgl" onchange="saveApGateControls()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="AP 自动恢复">AP 自动恢复</div>
      <div class="setting-desc" data-i18n="重启后恢复上次 AP 配置">重启后恢复上次 AP 配置</div>
    </div>
    <label class="tgl"><input type="checkbox" id="ap-auto-restore-tgl" onchange="saveApGateControls()"><div class="tgl-track"></div></label>
  </div>
  <div id="ap-core-card" style="margin:10px 0 2px;border-top:1px solid rgba(255,255,255,.08);padding-top:10px">
    <div class="ck-head"><h3><span>🛡</span><span data-i18n="AP 注入安全">AP 注入安全</span></h3><span class="ck-chip" id="ap-core-state-pill" data-i18n="等待 AP">等待 AP</span></div>
    <div class="state-panel" id="ap-state-panel">
      <div class="sp-label" data-i18n="AP 注入状态">AP 注入状态</div>
      <div class="sp-val" id="ap-core-state-big" data-i18n="等待">等待</div>
      <div class="sp-desc" id="ap-core-state-detail" data-i18n="门控未触发">门控未触发</div>
      <div class="gate-bar" id="ap-gate-bar"><i id="ap-gate-fill"></i></div>
    </div>
    <div class="src-row"><span data-i18n="注入来源">注入来源</span><b id="injection-source">Disabled</b></div>
    <div class="ctl-section" data-i18n="注入门控（服务端强制）">注入门控（服务端强制）</div>
    <div class="safety-strip">⚠️ <b data-i18n="Fail-closed（不变）：">Fail-closed（不变）：</b><span data-i18n="未知 / 无效 / SNA 档位默认禁止注入；AP 断开立即清零 Gate 计时。此策略由服务端 C++ 强制（handlers.h），客户端 UI 无法绕过。">未知 / 无效 / SNA 档位默认禁止注入；AP 断开立即清零 Gate 计时。此策略由服务端 C++ 强制（handlers.h），客户端 UI 无法绕过。</span></div>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name"><span data-i18n="8.3.6防甩机制 ">8.3.6防甩机制 </span><span class="exp-badge" data-i18n="实验 · 未验证实车防甩">实验 · 未验证实车防甩</span></div>
      <div class="setting-desc" id="jitter-desc" data-i18n="0x399 事件驱动：AP 啮合（事件3）进入武装期并单发一次 bit46 解锁帧；检测到 FSD 车道捕捉（事件6）立即合成 CAN1 原生 0x045 取消（提前约 200ms 打断甩动窗），等 AP 可用（事件2）后自动重挂；重挂前补发一次 bit46 刷新以提高 FSD 落地率。失败无重试、无锁定——程序静默复位，人工再开一轮。机制逐字移植自单 CAN v1.19.2 实车冻结版。需先开启上方「AP 注入门控」——门控关闭时本开关仅记忆状态、机制不运行，Legacy 走原始直通路径。">0x399 事件驱动：AP 啮合（事件3）进入武装期并单发一次 bit46 解锁帧；检测到 FSD 车道捕捉（事件6）立即合成 CAN1 原生 0x045 取消（提前约 200ms 打断甩动窗），等 AP 可用（事件2）后自动重挂；重挂前补发一次 bit46 刷新以提高 FSD 落地率。失败无重试、无锁定——程序静默复位，人工再开一轮。机制逐字移植自单 CAN v1.19.2 实车冻结版。需先开启上方「AP 注入门控」——门控关闭时本开关仅记忆状态、机制不运行，Legacy 走原始直通路径。</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-jitter-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="diag-grid" style="margin:6px 0 4px">
    <div class="diag-item"><span class="lbl" data-i18n="生效状态">生效状态</span><span class="v-dim" id="jitter-effective" data-i18n="关闭">关闭</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="阶段 / 原因">阶段 / 原因</span><span class="v-dim" id="jitter-phase">-- / --</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="AP / 周期数">AP / 周期数</span><span class="v-dim" id="jitter-ap">-- / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="取消 / 重挂">取消 / 重挂</span><span class="v-dim" id="jitter-bursts">0 / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="FSD落地 / EAP落地">FSD落地 / EAP落地</span><span class="v-dim" id="jitter-landing">0 / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="bit46 / 刷新">bit46 / 刷新</span><span class="v-dim" id="jitter-frames">0 / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="TX 成/败">TX 成/败</span><span class="v-dim" id="jitter-tx">0 / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="转向中止/复位">转向中止/复位</span><span class="v-dim" id="jitter-steer">0 / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="超时/故障复位">超时/故障复位</span><span class="v-dim" id="jitter-resets">0 / 0</span></div>
  </div>
  <div class="diag-grid" style="margin:6px 0 4px">
    <div class="diag-item"><span class="lbl">Abort Guard</span><span class="v-dim" id="def-abort-state" data-i18n="关闭">关闭</span></div>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="声音警告抑制">声音警告抑制</div>
      <div class="setting-desc" data-i18n="独立 ISA 提示音策略，不联动滚轮或摄像头配置">独立 ISA 提示音策略，不联动滚轮或摄像头配置</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-sound-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name"><span data-i18n="限速覆盖 ">限速覆盖 </span><span class="exp-badge">HW4</span></div>
      <div class="setting-desc" data-i18n="0x39B 强写融合/视觉限速为 NONE">0x39B 强写融合/视觉限速为 NONE</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-isa-override-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name"><span data-i18n="Legacy 左滚轮免打扰 ">Legacy 左滚轮免打扰 </span><span class="exp-badge" data-i18n="实车路径">实车路径</span></div>
      <div class="setting-desc" data-i18n="0x399 告警触发；CAN1 原生 0x045 告警持续才反向平衡，清除后立即停止">0x399 告警触发；CAN1 原生 0x045 告警持续才反向平衡，清除后立即停止</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-dnd-vol-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="diag-grid" style="margin:6px 0 4px">
    <div class="diag-item"><span class="lbl" data-i18n="告警状态">告警状态</span><span class="v-dim" id="wheel-hands-state">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="原生 0x045">原生 0x045</span><span class="v-dim" id="wheel-native-3c2" data-i18n="等待">等待</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="状态/本周期帧/下周期方向">状态/本周期帧/下周期方向</span><span class="v-dim" id="wheel-sequence-state">idle / 0 / +1</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="正向帧/反向帧/交替完成">正向帧/反向帧/交替完成</span><span class="v-dim" id="wheel-action-type">0 / 0 / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="重复/冲突">重复/冲突</span><span class="v-dim" id="wheel-counter-state">0 / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="回声/镜像">回声/镜像</span><span class="v-dim" id="wheel-echo-state">0 / 0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="阻断原因">阻断原因</span><span class="v-dim" id="wheel-blocked-reason">disabled</span></div>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name"><span data-i18n="速度免打扰 ">速度免打扰 </span><span class="exp-badge" data-i18n="实验">实验</span></div>
      <div class="setting-desc" data-i18n="保存到防御配置，避免速度策略频繁扰动">保存到防御配置，避免速度策略频繁扰动</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-speed-nd-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name"><span data-i18n="AP/EAP 兼容 ">AP/EAP 兼容 </span><span class="exp-badge" data-i18n="未验证">未验证</span></div>
      <div class="setting-desc" data-i18n="保留 AP/EAP 兼容策略位">保留 AP/EAP 兼容策略位</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-apeap-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name">Abort Guard <span class="exp-badge" data-i18n="实验">实验</span></div>
      <div class="setting-desc" data-i18n="0x399 AP state 8/9 后锁止 Legacy 注入；仅状态小于 2 自动解除">0x399 AP state 8/9 后锁止 Legacy 注入；仅状态小于 2 自动解除</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-abort-guard-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name"><span data-i18n="忽略车辆 OTA 守卫 ">忽略车辆 OTA 守卫 </span><span class="exp-badge" data-i18n="诊断">诊断</span></div>
      <div class="setting-desc" data-i18n="OTA 守卫置位后若 0x318 停发会永久封锁注入（10 分钟后已自动解除）；开启=立即忽略守卫恢复注入，真 OTA 进行中有风险，仅诊断用">OTA 守卫置位后若 0x318 停发会永久封锁注入（10 分钟后已自动解除）；开启=立即忽略守卫恢复注入，真 OTA 进行中有风险，仅诊断用</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-ota-ovr-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
</div>

<!-- Slew Config -->
<div class="card">
  <div class="card-title" data-i18n="保护参数">保护参数</div>
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="启用 slew rate 限制">启用 slew rate 限制</div>
      <div class="setting-desc" data-i18n="仅限制速度偏移值下降速率，不控制 FSD 防御或 NAG">仅限制速度偏移值下降速率，不控制 FSD 防御或 NAG</div>
    </div>
    <label class="tgl"><input type="checkbox" id="hw3-slew-tgl" onchange="saveHw3Slew()"><div class="tgl-track"></div></label>
  </div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl" data-i18n="最大下降速率">最大下降速率</span><span class="v-acc" id="def-rate">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="最小保持偏移">最小保持偏移</span><span class="v-acc" id="def-min">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="触发次数">触发次数</span><span class="v-warn" id="def-cnt">0</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="当前偏移">当前偏移</span><span class="v-dim" id="def-cur">--</span></div>
  </div>
</div>

<!-- Protection Status -->
<div class="card">
  <div class="setting-row">
    <div style="display:flex;align-items:center;gap:8px">
      <span class="status-dot err" id="def-dot"></span>
      <div class="setting-name" id="def-status" data-i18n="保护未启用">保护未启用</div>
    </div>
  </div>
</div>
    </div>

    <!-- Page 7: OTA Update -->
    <div class="page" id="pg-ota">
<div class="ck-head"><h3><span>◉</span><span data-i18n="OTA 升级">OTA 升级</span></h3><span class="ck-chip" id="st-ota-pill">--</span></div>
<!-- Version Info -->
<div class="card cockpit-card">
  <div class="card-title" data-i18n="固件信息">固件信息</div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl" data-i18n="版本">版本</span><span class="v-acc" id="ota-ver">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="构建时间">构建时间</span><span class="v-dim" id="ota-build">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="Flash 占用">Flash 占用</span><span class="v-dim" id="ota-flash">--</span></div>
    <div class="diag-item"><span class="lbl">SDK</span><span class="v-dim" id="ota-sdk">--</span></div>
  </div>
</div>

<!-- Release Update -->
<div class="card">
  <div class="card-title"><span data-i18n="Release 在线更新 ">Release 在线更新 </span><span class="exp-badge">Batch C</span></div>
  <div class="card-subtitle" data-i18n="从 GitHub Release 检查与安装匹配当前硬件的 OTA 固件">从 GitHub Release 检查与安装匹配当前硬件的 OTA 固件</div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl" data-i18n="当前版本">当前版本</span><span class="v-acc" id="rel-current">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="最新版本">最新版本</span><span class="v-info" id="rel-latest">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="固件文件">固件文件</span><span class="v-dim" id="rel-artifact">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="更新状态">更新状态</span><span class="v-dim" id="rel-status" data-i18n="未检查">未检查</span></div>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="Beta 通道">Beta 通道</div>
      <div class="setting-desc" data-i18n="检查最新 prerelease，默认关闭">检查最新 prerelease，默认关闭</div>
    </div>
    <label class="tgl"><input type="checkbox" id="rel-beta-tgl" onchange="toggleUpdateBeta()"><div class="tgl-track"></div></label>
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="开机自动更新">开机自动更新</div>
      <div class="setting-desc" data-i18n="联网后自动检查并安装新版本，建议测试稳定后再启用">联网后自动检查并安装新版本，建议测试稳定后再启用</div>
    </div>
    <label class="tgl"><input type="checkbox" id="rel-auto-tgl" onchange="toggleAutoUpdate()"><div class="tgl-track"></div></label>
  </div>
  <div style="display:flex;gap:6px;margin-top:8px;flex-wrap:wrap">
    <button class="btn btn-sm" id="rel-check-btn" onclick="checkReleaseUpdate()" data-i18n="检查更新">检查更新</button>
    <button class="btn btn-sm btn-outline" id="rel-install-btn" onclick="installReleaseUpdate()" disabled data-i18n="安装更新">安装更新</button>
  </div>
  <div class="setting-desc" id="rel-msg" style="margin-top:8px" data-i18n="请先连接 WiFi 后检查 GitHub Release。">请先连接 WiFi 后检查 GitHub Release。</div>
</div>

<!-- Upload Area -->
<div class="card">
  <div class="card-title" data-i18n="固件上传">固件上传</div>
  <div class="upload-area" id="ota-drop" onclick="$('ota-file').click()">
    <div style="font-size:24px;color:var(--tx3);margin-bottom:4px">↑</div>
    <div style="color:var(--tx1);font-weight:500" data-i18n="选择固件文件">选择固件文件</div>
    <div style="color:var(--tx3);font-size:11px" data-i18n="支持 .bin 格式，拖放或点击选择">支持 .bin 格式，拖放或点击选择</div>
  </div>
  <input type="file" id="ota-file" accept=".bin" style="display:none" onchange="uploadFirmware()">
  <div style="margin-top:10px;display:flex;gap:6px;flex-wrap:wrap">
    <button class="btn" id="ota-btn" onclick="uploadFirmware()" style="flex:1" data-i18n="开始上传">开始上传</button>
    <button class="btn btn-outline" id="ota-reset-btn" onclick="resetOtaCredentials()" style="flex:1" data-i18n="重置凭据缓存">重置凭据缓存</button>
  </div>
  <div id="ota-progress" style="display:none;margin-top:8px">
    <div style="background:var(--card-bg-alt);border-radius:0;height:8px;overflow:hidden">
      <div id="ota-bar" style="background:var(--accent);height:100%;width:0%;transition:width .3s"></div>
    </div>
    <div style="text-align:center;color:var(--tx3);font-size:11px;margin-top:4px" id="ota-pct">0%</div>
  </div>
</div>
    </div>

    <!-- Page 8: Network Settings -->
    <div class="page" id="pg-network">
<div class="ck-head"><h3><span>◉</span><span data-i18n="网络设置">网络设置</span></h3><span class="ck-chip" id="st-net-pill" data-i18n="未配置">未配置</span></div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl" data-i18n="UI 显示状态">UI 显示状态</div><div class="val" id="st-net-ui" data-i18n="待同步">待同步</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="NVS 持久化状态">NVS 持久化状态</div><div class="val" id="st-net-nvs" data-i18n="读取中">读取中</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="实际 CAN/网络运行状态">实际 CAN/网络运行状态</div><div class="val" id="st-net-run" data-i18n="待检测">待检测</div></div>
</div>
<div class="status-triplet">
  <div class="status-chip"><div class="lbl" data-i18n="DNS UI 显示状态">DNS UI 显示状态</div><div class="val" id="st-dns-ui" data-i18n="待同步">待同步</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="DNS NVS 持久化状态">DNS NVS 持久化状态</div><div class="val" id="st-dns-nvs" data-i18n="读取中">读取中</div></div>
  <div class="status-chip"><div class="lbl" data-i18n="DNS 实际运行状态">DNS 实际运行状态</div><div class="val" id="st-dns-run" data-i18n="待检测">待检测</div></div>
</div>
<!-- WiFi Hotspot -->
<div class="card cockpit-card">
  <div class="card-title" data-i18n="WiFi 热点">WiFi 热点</div>
  <div class="setting-row">
    <div class="setting-name">SSID</div>
    <div class="v-dim" id="ap-ssid">--</div>
  </div>
  <div class="setting-row">
    <div class="setting-name" data-i18n="连接设备">连接设备</div>
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
    <div class="setting-name" data-i18n="AP 密码">AP 密码</div>
    <input class="inp" type="password" id="ap-pass-input" style="width:180px" placeholder="8-64 位">
  </div>
  <div class="setting-row">
    <div>
      <div class="setting-name" data-i18n="隐藏热点">隐藏热点</div>
      <div class="setting-desc" data-i18n="保存后重启生效">保存后重启生效</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="ap-hidden-tgl">
      <div class="tgl-track"></div>
    </label>
  </div>
  <div style="display:flex;gap:6px;margin-top:8px">
    <button class="btn btn-sm" onclick="saveHotspot(false)" data-i18n="保存热点">保存热点</button>
    <button class="btn btn-sm btn-outline" onclick="saveHotspot(true)" data-i18n="保存并重启">保存并重启</button>
  </div>
</div>

<!-- WiFi Internet -->
<div class="card">
  <div class="card-title" data-i18n="WiFi 联网">WiFi 联网</div>
  <div class="setting-row">
    <div class="setting-name" data-i18n="状态">状态</div>
    <div class="v-warn" id="wifi-status" data-i18n="未配置">未配置</div>
  </div>
  <div id="wifi-slots" style="margin-top:8px"></div>
  <div style="margin-top:8px;display:flex;gap:6px">
    <button class="btn btn-sm" onclick="scanWifi()" data-i18n="扫描网络">扫描网络</button>
    <button class="btn btn-sm btn-outline" onclick="editWifiSlot(-1)" data-i18n="手动添加">手动添加</button>
    <button class="btn btn-sm btn-outline" onclick="testRelayWifi()" data-i18n="测试连接">测试连接</button>
  </div>
  <div id="wifi-form" style="display:none;margin-top:10px">
    <div class="setting-row"><div class="setting-name">SSID</div><input class="inp" id="wf-ssid" style="width:160px"></div>
    <div class="setting-row"><div class="setting-name" data-i18n="密码">密码</div><input class="inp" type="password" id="wf-pass" style="width:160px"></div>
    <div style="display:flex;gap:6px;margin-top:6px">
      <button class="btn btn-sm" onclick="saveWifi()" data-i18n="保存">保存</button>
      <button class="btn btn-sm btn-outline" onclick="clearWifiForm()" data-i18n="取消">取消</button>
    </div>
  </div>
</div>

<!-- STA-AP Gateway -->
<div class="card">
  <div class="setting-row">
    <div class="setting-name" data-i18n="STA-AP 网关 (NAT)">STA-AP 网关 (NAT)</div>
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
      <div class="setting-name" data-i18n="网络性能模式">网络性能模式</div>
      <div class="setting-desc" data-i18n="转发流量时降低 WebUI 轮询">转发流量时降低 WebUI 轮询</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="gw-perf-tgl" onchange="saveGateway()">
      <div class="tgl-track"></div>
    </label>
  </div>
</div>

<!-- Upstream DNS -->
<div class="card">
  <div class="card-title" data-i18n="上游 DNS">上游 DNS</div>
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
    <div class="sel-card active" onclick="setDnsProfile('conservative')"><div class="sel-name" data-i18n="保守模式">保守模式</div></div>
    <div class="sel-card" onclick="setDnsProfile('aggressive')"><div class="sel-name" data-i18n="激进模式">激进模式</div></div>
  </div>
</div>

<!-- DNS Filter Rules -->
<div class="card">
  <div class="card-title" data-i18n="黑名单 / DNS 过滤规则">黑名单 / DNS 过滤规则</div>
  <div class="card-subtitle" data-i18n="保守模式使用完整 12 条规则，激进模式使用最小 9 条规则">保守模式使用完整 12 条规则，激进模式使用最小 9 条规则</div>
  <div style="margin-bottom:8px">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px">
      <span style="color:var(--err);font-weight:500;font-size:11px" data-i18n="黑名单">黑名单</span>
      <span style="color:var(--tx3);font-size:10px" id="dns-bl-cnt" data-i18n="0 域名">0 域名</span>
    </div>
    <textarea class="inp" id="dns-blacklist" rows="6" placeholder="每行一个域名"></textarea>
  </div>
  <div style="margin-bottom:8px">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:4px">
      <span style="color:var(--ok);font-weight:500;font-size:11px" data-i18n="白名单">白名单</span>
      <span style="color:var(--tx3);font-size:10px" id="dns-wl-cnt" data-i18n="0 域名">0 域名</span>
    </div>
    <textarea class="inp" id="dns-whitelist" rows="3" placeholder="每行一个域名"></textarea>
  </div>
  <div style="display:flex;gap:6px;margin-bottom:8px">
    <button class="btn btn-sm" onclick="saveGatewayDns()" data-i18n="保存规则">保存规则</button>
    <button class="btn btn-sm btn-outline" onclick="loadGatewayDns()" data-i18n="刷新">刷新</button>
  </div>
  <div style="display:flex;gap:6px;margin-bottom:8px">
    <input class="inp" id="dns-test-input" placeholder="输入域名测试..." style="flex:1">
    <button class="btn btn-sm" onclick="testGatewayDns()" data-i18n="测试">测试</button>
  </div>
  <div style="display:flex;justify-content:space-between;align-items:center">
    <span style="color:var(--tx3);font-size:10px"><span data-i18n="已拦截: ">已拦截: </span><span id="dns-blocked-cnt">0</span><span data-i18n=" 条"> 条</span></span>
    <div style="display:flex;gap:4px">
      <button class="btn btn-sm btn-outline" onclick="loadGatewayBlocked()" data-i18n="刷新">刷新</button>
      <button class="btn btn-sm btn-outline" onclick="clearGatewayBlocked()" data-i18n="清空">清空</button>
    </div>
  </div>
</div>

    </div>

    <!-- Page 10: CAN Tools -->
    <div class="page" id="pg-can">
<div class="ck-head"><h3><span>◉</span><span data-i18n="CAN 诊断">CAN 诊断</span></h3><span class="ck-chip" id="st-can-pill">0 / 0</span></div>
<!-- legacy contract marker: class="diag-shell" -->
<div class="diag-shell diag-console">
  <aside class="diag-kpi-rail">
    <div class="diag-kpi"><div class="lbl">CAN1 RX/TX</div><div class="val" id="diag-rxtx">0 / 0</div></div>
    <div class="diag-kpi"><div class="lbl">CAN2 EFLG</div><div class="val" id="diag-eflg">0x00</div></div>
    <div class="diag-kpi"><div class="lbl" data-i18n="发送错误">发送错误</div><div class="val" id="diag-txerr">0</div></div>
    <div class="diag-kpi"><div class="lbl" data-i18n="最近写入">最近写入</div><div class="val" id="diag-last-write">--</div></div>
    <div class="diag-kpi"><div class="lbl" data-i18n="发现 ID">发现 ID</div><div class="val" id="diag-id-count">--</div></div>
    <div class="diag-timeline">
      <div class="label">LIVE TIMELINE</div>
      <div class="diag-bars">
        <div class="diag-bar"><span style="width:82%"></span></div>
        <div class="diag-bar"><span style="width:54%"></span></div>
        <div class="diag-bar"><span style="width:28%"></span></div>
      </div>
    </div>
    <div class="card">
      <div class="card-title" data-i18n="CAN 引脚">CAN 引脚</div>
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
    <div class="card cockpit-card">
      <div class="cockpit-status-bar">
        <div class="cockpit-title"><small data-i18n="工程模式">工程模式</small><h2 data-i18n="错误优先的实时帧">错误优先的实时帧</h2></div>
        <div class="cockpit-chip-row"><span class="cockpit-chip ok" data-i18n="实时">实时</span><span class="cockpit-chip warn" data-i18n="错误优先">错误优先</span><span class="cockpit-chip" data-i18n="DBC 可切换">DBC 可切换</span></div>
      </div>
      <div class="diag-toolbar">
        <input class="inp" id="sniff-filter" placeholder="帧筛选：输入 ID、名称、TXERR 或 mux...">
        <button class="btn btn-sm btn-outline" id="sniff-pause" onclick="toggleSniffPause()" data-i18n="暂停">暂停</button>
      </div>
      <div class="sub-tabs">
        <div class="sub-tab active" onclick="switchCanTab('sniffer')" data-i18n="实时帧">实时帧</div>
        <div class="sub-tab" onclick="switchCanTab('recorder')" data-i18n="记录器">记录器</div>
        <div class="sub-tab" onclick="switchCanTab('controller')" data-i18n="控制器">控制器</div>
        <div class="sub-tab" onclick="switchCanTab('debug')" data-i18n="调试">调试</div>
      </div>
    </div>

<!-- Sniffer -->
<div id="can-sniffer">
  <div class="card">
    <div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:8px">
      <span style="font-weight:800" data-i18n="实时帧">实时帧</span>
      <span style="color:var(--tx3);font-size:11px" id="sniff-count" data-i18n="0 实时帧">0 实时帧</span>
    </div>
    <div class="diag-table-shell">
      <div class="diag-priority-row"><span>ID</span><span>DIR</span><span>DLC</span><span>DATA / EVENT</span><span>AGE</span></div>
      <div id="sniff-rows"></div>
    </div>
  </div>
</div>

<!-- Recorder -->
<div id="can-recorder" style="display:none">
  <div class="card">
    <div class="card-title" data-i18n="CAN 记录器">CAN 记录器</div>
    <div style="display:flex;gap:8px;margin-bottom:10px;flex-wrap:wrap">
      <button class="btn btn-sm" id="rec-start" onclick="startRec()" data-i18n="开始记录">开始记录</button>
      <button class="btn btn-sm btn-outline" id="rec-stop" onclick="stopRec()" disabled data-i18n="停止">停止</button>
      <button class="btn btn-sm btn-outline" id="rec-dl" onclick="downloadRec()" disabled data-i18n="下载 CSV">下载 CSV</button>
      <button class="btn btn-sm btn-outline" id="rec-meta" onclick="downloadRecMeta()" disabled data-i18n="下载诊断">下载诊断</button>
    </div>
    <div class="setting-row">
      <div><div class="setting-name" data-i18n="录制时长">录制时长</div><div class="setting-desc" data-i18n="后台流式写入，不阻塞 CAN">后台流式写入，不阻塞 CAN</div></div>
      <select class="inp" id="rec-duration" style="width:110px"><option value="60" data-i18n="60 秒">60 秒</option><option value="90" data-i18n="90 秒">90 秒</option><option value="120" selected data-i18n="120 秒">120 秒</option></select>
    </div>
    <div class="setting-row" style="align-items:flex-start">
      <div><div class="setting-name" data-i18n="CAN ID 过滤">CAN ID 过滤</div><div class="setting-desc" data-i18n="含 Model X CAN1 0x045 滚轮 RX/TX 及只读网关别名 0x238 / 0x292">含 Model X CAN1 0x045 滚轮 RX/TX 及只读网关别名 0x238 / 0x292</div></div>
      <input class="inp" id="rec-ids" value="045,118,129,186,238,292,370,389,399,3C2,3EE,3FD,47D,47F,488,5D9" style="width:min(420px,100%)">
    </div>
    <div class="setting-row">
      <div class="setting-name" data-i18n="状态">状态</div>
      <div class="setting-desc" id="rec-status" data-i18n="空闲">空闲</div>
    </div>
    <div class="diag-grid">
      <div class="diag-item"><span class="lbl" data-i18n="已写入">已写入</span><span class="v-dim" id="rec-count" data-i18n="0 帧">0 帧</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="丢帧">丢帧</span><span class="v-dim" id="rec-dropped">0</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="文件">文件</span><span class="v-dim" id="rec-bytes">0 KB</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="进度">进度</span><span class="v-dim" id="rec-progress" data-i18n="0 / 120 秒">0 / 120 秒</span></div>
      <div class="diag-item"><span class="lbl">CAN1 0x3C2 RX/TX/ERR</span><span class="v-dim" id="rec-3c2-can1">0 / 0 / 0</span></div>
      <div class="diag-item"><span class="lbl">CAN2 0x3C2 RX/TX/ERR</span><span class="v-dim" id="rec-3c2-can2">0 / 0 / 0</span></div>
      <div class="diag-item"><span class="lbl">mux1 / idle</span><span class="v-dim" id="rec-3c2-mux1">0 / 0</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="音量上 / 下">音量上 / 下</span><span class="v-dim" id="rec-3c2-volume">0 / 0</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="0x3C2 TX / 错误">0x3C2 TX / 错误</span><span class="v-dim" id="rec-3c2-tx">0 / 0</span></div>
      <div class="diag-item"><span class="lbl">CAN1 0x045 RX/TX/ERR</span><span class="v-dim" id="rec-045-can1">0 / 0 / 0</span></div>
      <div class="diag-item"><span class="lbl">CAN2 0x045 RX/TX/ERR</span><span class="v-dim" id="rec-045-can2">0 / 0 / 0</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="0x045 滚轮 上 / 下">0x045 滚轮 上 / 下</span><span class="v-dim" id="rec-045-updown">0 / 0</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="0x045 前 / 后">0x045 前 / 后</span><span class="v-dim" id="rec-045-fwdback">0 / 0</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="条件轮 上 / 下">条件轮 上 / 下</span><span class="v-dim" id="rec-045-cond">0 / 0</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="最后 Bus / Counter / CRC">最后 Bus / Counter / CRC</span><span class="v-dim" id="rec-045-last">-- / -- / --</span></div>
    </div>
    <div class="setting-desc" style="margin-top:10px" data-i18n="120 秒可靠录制需保留过滤。全量双 CAN 流量可能在时限前占满设备存储，状态会明确标记。">120 秒可靠录制需保留过滤。全量双 CAN 流量可能在时限前占满设备存储，状态会明确标记。</div>
  </div>
</div>

<!-- Controller -->
<div id="can-controller" style="display:none">
  <div class="card">
    <div class="card-title" data-i18n="CAN 控制器状态">CAN 控制器状态</div>
    <div class="diag-grid">
      <div class="diag-item"><span class="lbl">EFLG</span><span class="v-dim" id="ctrl-eflg">0x00</span></div>
      <div class="diag-item"><span class="lbl">RX Errors</span><span class="v-dim" id="ctrl-rxerr">0</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="发送错误">发送错误</span><span class="v-dim" id="ctrl-txerr">0</span></div>
      <div class="diag-item"><span class="lbl" data-i18n="模式">模式</span><span class="v-dim" id="ctrl-mode">--</span></div>
    </div>
    <div id="ctrl-mux" style="margin-top:8px"></div>
  </div>
</div>

<!-- Debug -->
<div id="can-debug" style="display:none">
  <div class="card">
    <div class="card-title" data-i18n="调试">调试</div>
    <div class="setting-row">
      <div class="setting-name" data-i18n="调试日志">调试日志</div>
      <label class="tgl">
        <input type="checkbox" id="debug-tgl" onchange="toggleCanDebug()">
        <div class="tgl-track"></div>
      </label>
    </div>
  </div>
  <div class="card">
    <div class="card-title" data-i18n="最近写入检查">最近写入检查</div>
    <div class="diag-grid">
      <div class="diag-item"><span class="lbl">Injected</span><span class="v-acc" id="lw-injected">--</span></div>
      <div class="diag-item"><span class="lbl">Bus</span><span class="v-dim" id="lw-bus">--</span></div>
      <div class="diag-item"><span class="lbl">Match</span><span class="v-dim" id="lw-match">--</span></div>
      <div class="diag-item"><span class="lbl">Age</span><span class="v-dim" id="lw-age">--</span></div>
    </div>
  </div>
</div>
  </section>
</div>
    </div>

    <!-- Page 10: Auto Shift (Phase 5A placeholder) -->
    <div class="page" id="pg-shift" data-single-hide="1">
<div class="page-title"><span data-i18n="自动换挡 ">自动换挡 </span><span class="exp-badge" data-i18n="储备功能">储备功能</span></div>
<div class="card" style="border-color:#f59e0b;background:rgba(245,158,11,0.06)">
  <div style="display:flex;align-items:center;gap:10px;margin-bottom:8px">
    <span style="font-size:24px">⚠️</span>
    <div>
      <div style="font-weight:700;color:#f59e0b" data-i18n="储备功能未开放">储备功能未开放</div>
      <div class="setting-desc" data-i18n="此页面为只读遥测展示，自动换挡功能将在后续版本中实装">此页面为只读遥测展示，自动换挡功能将在后续版本中实装</div>
    </div>
  </div>
</div>
<div class="card">
  <div class="card-title" data-i18n="车辆遥测（只读）">车辆遥测（只读）</div>
  <div class="card-subtitle" data-i18n="从 CAN 总线实时读取的车辆状态数据">从 CAN 总线实时读取的车辆状态数据</div>
  <div class="diag-grid">
    <div class="diag-item"><span class="lbl" data-i18n="当前车速">当前车速</span><span class="v-acc" id="shift-speed">-- km/h</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="当前挡位">当前挡位</span><span class="v-info" id="shift-gear">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="刹车状态">刹车状态</span><span class="v-dim" id="shift-brake">--</span></div>
    <div class="diag-item"><span class="lbl" data-i18n="FSD 状态">FSD 状态</span><span class="v-dim" id="shift-fsd">--</span></div>
  </div>
</div>
<div class="card">
  <div class="card-title" data-i18n="换挡策略预览">换挡策略预览</div>
  <div class="setting-desc" style="margin-bottom:8px" data-i18n="基于车速和驾驶模式的自动换挡策略（示意图）">基于车速和驾驶模式的自动换挡策略（示意图）</div>
  <table class="tbl">
    <thead><tr><th data-i18n="车速区间">车速区间</th><th>Auto</th><th>Chill</th><th>Hurry</th></tr></thead>
    <tbody>
      <tr><td>0-30 km/h</td><td data-i18n="标准">标准</td><td data-i18n="舒适">舒适</td><td data-i18n="积极">积极</td></tr>
      <tr><td>30-60 km/h</td><td data-i18n="标准">标准</td><td data-i18n="舒适">舒适</td><td data-i18n="积极">积极</td></tr>
      <tr><td>60-90 km/h</td><td data-i18n="标准">标准</td><td data-i18n="标准">标准</td><td data-i18n="积极">积极</td></tr>
      <tr><td>90+ km/h</td><td data-i18n="经济">经济</td><td data-i18n="标准">标准</td><td data-i18n="最大">最大</td></tr>
    </tbody>
  </table>
</div>
    </div>

<script>

// ═══════════════════════════════════════════════════════════
// Atlas T-2CAN Dashboard — Complete JavaScript
// ═══════════════════════════════════════════════════════════

// ── Constants & State ──────────────────────────────────────
var HW_NAMES = ['Legacy','HW3','HW4'];
var HW_LABELS = {
  'Legacy': {zh:T('旧版'),en:'Legacy'},
  'HW3': {zh:T('第三代'),en:'HW3'},
  'HW4': {zh:T('第四代'),en:'HW4'}
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
try { if (localStorage.getItem('tcanTheme') === 'light') dark = false; } catch(e) {}
document.documentElement.classList.toggle('light', !dark);
if(!dark){ if($('theme-btn'))$('theme-btn').textContent='🌙'; if($('mobile-theme-btn'))$('mobile-theme-btn').textContent=T('🌙 夜间'); }
var pollTimer = null;
var pollMs = 1000;
var pollTick = null;
var sniffPaused = false;
var sniffFrames = [];
var recActive = false;
var canTab = 'sniffer';
var lightPreset = 3;
var lightFrequency = 'medium';
var burstPreset = 'B';
var BURST_PRESETS = {
  A:{count:2,on_ms:250,off_ms:250,label:T('A 短促')},
  B:{count:3,on_ms:180,off_ms:180,label:T('B 标准')},
  C:{count:5,on_ms:150,off_ms:150,label:T('C 明显')},
  D:{count:7,on_ms:200,off_ms:200,label:T('D 长闪')}
};
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
function setFsdVisualState(on){
  var stateText=on?'ON':'OFF';
  var mobileText=stateText;
  setText('fsd-label',stateText);
  setText('m-fsd',mobileText);
  var desc=on?T('FSD 注入模块已启用；请持续关注 CAN 状态和防护状态。'):T('模块总开关关闭；启用前请确认车辆处于安全状态。');
  setText('ov-fsd-desc',desc);
  var btn=$('fsd-toggle');
  if(btn){btn.textContent=on?T('长按 2 秒关闭 FSD 注入'):T('长按 2 秒启用 FSD 注入');btn.classList.toggle('on',!!on);}
  var mobileBtn=document.querySelector('.mobile-hold');
  if(mobileBtn){mobileBtn.textContent=on?T('长按关闭'):T('长按启用');mobileBtn.classList.toggle('on',!!on);}
  var state=$('fsd-label');
  if(state){state.classList.toggle('on',!!on);}
  var mobileState=$('m-fsd');
  if(mobileState){mobileState.classList.toggle('on',!!on);}
}
function setHtml(id,html){var e=$(id);if(e)e.innerHTML=html}
function setCls(id,cls){var e=$(id);if(e)e.className=cls}
function escHtml(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')}
function esc(s){return String(s==null?'':s).replace(/[&<>]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;'}[c]})}
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
  if(light&&light.checked)flags.push(T('灯光爆闪'));
  var apeap=$('def-apeap-tgl');
  if(apeap&&apeap.checked)flags.push('AP/EAP');
  return flags.length?(T('实验: ')+flags.join('/')):T('实验项需实车验证');
}
function fmtUp(sec){
  var h=Math.floor(sec/3600),m=Math.floor(sec%3600/60),s=sec%60;
  return (h<10?'0':'')+h+':'+(m<10?'0':'')+m+':'+(s<10?'0':'')+s;
}

// ── I18N ───────────────────────────────────────────────────
var I18N={
  '驾驶状态':'Drive Status','硬件模式':'Hardware Mode','驾驶风格':'Drive Style',
  '速度策略':'Speed Strategy','CAN2 控制':'CAN2 Control','FSD 防护':'FSD Guard',
  'OTA 升级':'OTA Update','网络设置':'Network','CAN 诊断':'CAN Diagnostics',
  '自动换挡':'Auto Shift','已连接':'Connected','未连接':'Disconnected',
  '连接丢失':'Connection Lost','FSD 注入':'FSD Injection','FSD 开关':'FSD Toggle',
  '点击切换开关状态':'Tap to toggle','点「刷新」重试':'Tap Refresh to retry','CAN Bus':'CAN Bus','帧率':'FPS',
  '硬件版本':'Hardware Version','速度偏移':'Offset','芯片温度':'Chip Temp',
  'TX Errors':'TX Err','跟随距离':'Follow Dist','选择您的自动驾驶硬件版本':'Select your AD hardware version',
  '推荐':'Recommended','旧版':'Legacy','第三代':'Gen 3',
  '第四代':'Gen 4','速度配置':'Speed Profile','选择速度偏移方案':'Select speed offset scheme',
  'AP 状态恢复':'AP State Restore','重启后恢复上次 AP 配置':'Restore AP config after reboot','FSD 注入控制':'FSD Injection Control',
  '启用后设备将注入 CAN 帧到车辆总线':'Injects CAN frames to vehicle bus when enabled','已关闭':'OFF','已开启':'ON',
  '开机自动启用':'Auto-enable on boot','设备重启后自动开启 FSD 注入':'Auto-enable FSD after reboot','紧急控制':'Emergency',
  '重启设备':'Reboot','重置计数':'Reset Stats','HW3 自定义速度':'HW3 Custom Speed',
  '启用自定义速度':'Enable custom speed','覆盖默认速度映射表':'Override default speed mapping','速度映射表':'Speed Mapping',
  '实际速度':'Actual','目标速度':'Target','高速映射':'High Speed Mapping',
  '速度编码方式':'Speed Encoding','选择 CAN 总线速度编码':'Select CAN speed encoding','默认':'Default',
  '编码 A':'Enc A','编码 B':'Enc B','实时数据':'Realtime Data',
  'CAN2 状态':'CAN2 Status','CAN2 RX':'CAN2 RX','已发现 ID':'Found IDs',
  'CAN2 ID 列表':'CAN2 CAN IDs','灯光注入测试':'Lighting Test','模拟方向盘拨杆操作':'Simulate stalk operation',
  '持续时间 (ms)':'Duration (ms)','状态':'Status','空闲':'Idle',
  '启用 0x339 持续注入 CAN2':'Enable 0x339 continuous CAN2 injection','偏移速率保护':'Slew Rate Protection','防止速度偏移突变被检测':'Prevent sudden offset detection',
  '启用 slew rate 限制':'Enable slew rate limit','限制偏移值下降速率':'Limit offset drop rate','保护参数':'Protection Params',
  '最大下降速率':'Max Drop Rate','最小保持偏移':'Min Hold Offset','触发次数':'Triggers',
  '当前偏移':'Current Offset','保护未启用':'Protection OFF','保护已启用':'Protection ON',
  '固件信息':'Firmware Info','版本':'Version','构建时间':'Build Time',
  'Flash 占用':'Flash Used','Release 在线更新':'Release Online Update','当前版本':'Current',
  '最新版本':'Latest','固件文件':'Artifact','更新状态':'Update Status',
  'Beta 通道':'Beta Channel','开机自动更新':'Auto-update on boot','检查更新':'Check Update',
  '安装更新':'Install Update','固件上传':'Firmware Upload','选择固件文件':'Select firmware',
  '支持 .bin 格式，拖放或点击选择':'.bin files, drag-drop or click','开始上传':'Start Upload','上传中...':'Uploading...',
  'WiFi 热点':'WiFi Hotspot','SSID':'SSID','连接设备':'Clients',
  'WiFi 联网':'WiFi Internet','未配置':'Not configured','扫描网络':'Scan',
  '手动添加':'Add Manually','密码':'Password','保存':'Save',
  '取消':'Cancel','STA-AP 网关 (NAT)':'STA-AP Gateway (NAT)','网络性能模式':'Perf Mode',
  '转发流量时降低 WebUI 轮询':'Reduce polling during forwarding','上游 DNS':'Upstream DNS','保守模式':'Conservative',
  '激进模式':'Aggressive','DNS 过滤规则':'DNS Filter Rules','黑名单':'Blacklist',
  '白名单':'Whitelist','域名':'domains','保存规则':'Save Rules',
  '刷新':'Refresh','输入域名测试...':'Enter domain to test...','测试':'Test',
  '已拦截':'Blocked','条':'items','清空':'Clear',
  'CAN 引脚配置':'CAN Pin Config','实时帧':'Live Frames','记录器':'Recorder',
  '开始录制':'Start','停止':'Stop','下载 CSV':'Download CSV',
  '录制帧数限制':'Frame Limit','控制器状态':'Controller Status','调试':'Debug',
  '调试日志':'Debug Log','最近写入':'Last Write Check',' · 丢帧 ':' · drop ',
  ' · 规则: ':' · rules: ',' 个':' plugins',' 实时帧':' live frames',
  ' 对应的固件文件。':' firmware file.',' 已不低于 ':' already ≥ ',' 帧':' frames',
  ' 白':' W',' 秒':' s',' 黑 / ':' B / ',
  '+10%，封顶 132':'+10%, cap 132','+20%，封顶 132':'+20%, cap 132','+30%，封顶 117':'+30%, cap 117',
  '+50%，封顶 60':'+50%, cap 60','+50%，封顶 90':'+50%, cap 90','0 / 120 秒':'0 / 120 s',
  '0 域名':'0 domains','0 实时帧':'0 live frames','0 帧':'0 frames',
  '0x045 前 / 后':'0x045 Fwd / Rear','0x045 滚轮 上 / 下':'0x045 Wheel Up / Dn','0x399 AP state 8/9 后锁止 Legacy 注入；仅状态小于 2 自动解除':'0x399: Legacy injection locked after AP state 8/9; auto-unlocks only below state 2',
  '0x399 事件驱动：AP 啮合（事件3）进入武装期并单发一次 bit46 解锁帧；检测到 FSD 车道捕捉（事件6）立即合成 CAN1 原生 0x045 取消（提前约 200ms 打断甩动窗），等 AP 可用（事件2）后自动重挂；重挂前补发一次 bit46 刷新以提高 FSD 落地率。失败无重试、无锁定——程序静默复位，人工再开一轮。机制逐字移植自单 CAN v1.19.2 实车冻结版。需先开启上方「AP 注入门控」——门控关闭时本开关仅记忆状态、机制不运行，Legacy 走原始直通路径。':'0x399 event-driven: AP engagement (event 3) opens the arming window and fires a one-shot bit46 unlock frame; on FSD lane capture (event 6) a native CAN1 0x045 cancel is synthesized (~200 ms early, cutting the jerk window); auto re-request once AP is available again (event 2), preceded by a bit46 refresh to raise the FSD landing rate. No retry, no latching on failure — silent reset, start a new round manually. Ported verbatim from the single-CAN v1.19.2 field-frozen build. Requires the AP injection gate above — with the gate off this switch only remembers state and Legacy takes the direct path.','0x399 告警触发；CAN1 原生 0x045 告警持续才反向平衡，清除后立即停止':'0x399 alert-triggered; counter-balances only while the native CAN1 0x045 alert persists, stops right after it clears','0x39B 强写融合/视觉限速为 NONE':'0x39B force-writes fused/vision limit to NONE',
  '0x3C2 TX / 错误':'0x3C2 TX / Err','120 秒':'120 s','60 秒':'60 s',
  '90 秒':'90 s','120 秒可靠录制需保留过滤。全量双 CAN 流量可能在时限前占满设备存储，状态会明确标记。':'Reliable 120 s recording needs the filter. Full dual-CAN traffic may fill storage before the deadline; the status marks it clearly.','2秒内两次真实 PULL 触发固件侧 0x249 PULL burst，默认关闭':'Two real PULLs within 2 s trigger the firmware-side 0x249 PULL burst, default off',
  '8.3.6防甩机制':'8.3.6 Anti-Jerk','><summary>详情</summary><pre>':'><summary>Details</summary><pre>','>删除</button>':'>Delete</button>',
  '>暂无插件。新安装默认禁用。</div>':'>No plugins yet. New installs start disabled.</div>','>目标ID: ':'>Target ID: ','A 短促':'A Short',
  'AP / 周期数':'AP / Cycles','AP Gate 等待':'AP Gate Wait','AP 密码':'AP Password',
  'AP 故障·已复位':'AP Fault · Reset','AP 注入安全':'AP Injection Safety','AP 注入状态':'AP Injection Status',
  'AP 注入门控':'AP Injection Gate','AP 激活':'AP Active','AP 自动恢复':'AP Auto-Recovery',
  'AP 门控未开启':'AP gate off','AP/EAP 兼容':'AP/EAP Compat','Auto 遇到双通道镜像时停止写入':'Auto stops writing on dual-channel mirror',
  'B 标准':'B Standard','C 明显':'C Strong','D 长闪':'D Long',
  'CAN ID 过滤':'CAN ID Filter','CAN 引脚':'CAN Pins','CAN 控制器状态':'CAN Controller Status',
  'CAN 离线':'CAN Offline','CAN 记录器':'CAN Recorder','CAN未给出速度':'No speed from CAN',
  'CAN离线 / ':'CAN offline / ','CAN运行 / ':'CAN running / ','DBC 可切换':'DBC Switchable',
  'DNS NVS 持久化状态':'DNS NVS Persistence','DNS UI 显示状态':'DNS UI Display','DNS 实际运行状态':'DNS Runtime',
  'EAP 落地':'EAP Landed','FSD 注入模块已启用；请持续关注 CAN 状态和防护状态。':'FSD injection module enabled; keep watching CAN and guard status.','FSD 注入策略':'FSD Injection Policy',
  'FSD 状态':'FSD Status','FSD 硬件模式':'FSD Hardware Mode','FSD 落地':'FSD Landed',
  'FSD 防御总开关':'FSD Defense Master','FSD 防护状态':'FSD Guard Status','FSD-V14 模式':'FSD-V14 Mode',
  'FSD落地 / EAP落地':'FSD / EAP Landed','Fail-closed（不变）：':'Fail-closed (unchanged):','GTW2047 重放倍率':'GTW2047 Replay Ratio',
  'HW3/HW4：固定百分比 / 自动偏移 / 自定义偏移。Legacy 请使用上方独立四档控制。':'HW3/HW4: fixed percent / auto offset / custom offset. Legacy uses the dedicated 4-step control above.','Legacy 仅在 0x399 AP 活动后才允许注入（服务端强制，fail-closed）':'Legacy injection allowed only after 0x399 AP activity (server-enforced, fail-closed)','Legacy 左滚轮免打扰':'Legacy Left-Wheel DND',
  'Legacy 已就绪':'Legacy Ready','Legacy 智能速度偏移':'Legacy Smart Speed Offset','Legacy 目标总线':'Legacy Target Bus',
  'Legacy 目标速度':'Legacy Target Speed','Legacy 运行状态':'Legacy Status','Legacy 限速来源':'Legacy Limit Source',
  'MAX/V14 需实车验证':'MAX/V14 needs field validation','NVS 持久化状态':'NVS Persistence','NVS 未变更':'NVS Unchanged',
  'NVS 读取中':'NVS Loading','OTA 凭据缓存已清除':'OTA credential cache cleared','OTA 守卫置位后若 0x318 停发会永久封锁注入（10 分钟后已自动解除）；开启=立即忽略守卫恢复注入，真 OTA 进行中有风险，仅诊断用':'If 0x318 stops after the OTA guard sets, injection stays blocked (auto-released after 10 min); ON = ignore the guard now and restore injection — risky during a real OTA, diagnostics only',
  'OTA 已开始，设备将自动重启。':'OTA started, device will reboot.','OTA 已开始，设备将重启':'OTA started, device will reboot','OTA 确认':'OTA Confirm',
  'OTA 进行中':'OTA In Progress','P 档·已复位':'P Gear · Reset','PULL (闪光)':'PULL (Flash)',
  'PUSH (远光)':'PUSH (High Beam)','Release 中没有 ':'No match in release: ','STA 未配置':'STA Not Set',
  'STA 连接中':'STA Connecting','TX 成/败':'TX Ok/Err','Tesla Controller 对齐模式':'Tesla Controller Align Mode',
  'UI 待同步':'UI Sync Pending','UI 显示状态':'UI Display','URL 安装':'Install from URL',
  'bit46 / 刷新':'bit46 / Refresh','cp1-cp4 已保存':'cp1-cp4 saved','↗ 速度策略':'↗ Speed',
  '⇧ OTA 升级':'⇧ OTA','⌘ CAN 诊断':'⌘ CAN Diag','▣ 状态':'▣ Status',
  '◇ 硬件模式':'◇ Hardware','◈ FSD 防护':'◈ FSD Guard','◉ 驾驶风格':'◉ Drive Style',
  '◎ 网络设置':'◎ Network','☀ 白天':'☀ Day','⚙ 自动换挡':'⚙ Auto Shift',
  '⚠️ 未知 / 无效 / SNA 档位默认禁止注入；AP 断开立即清零 Gate 计时。此策略由服务端 C++ 强制（handlers.h），客户端 UI 无法绕过。':'⚠️ Unknown / invalid / SNA gears block injection by default; AP disconnect resets gate timing instantly. Enforced server-side in C++ (handlers.h) — the web UI cannot bypass it.','✦ CAN2 控制':'✦ CAN2','上传':'Upload',
  '上传文件':'Upload File','下载诊断':'Download Diag','下降缓冲':'Drop Buffer',
  '不可用 · AP 门控未开启':'Unavailable · AP gate off','丢帧':'Dropped','仅联动 Ban Shield 通用防御；左滚轮免打扰与 8.3.6防甩机制不受此开关联动':'Only links Ban Shield general defense; left-wheel DND and 8.3.6 anti-jerk stay independent',
  '仅重复已启用插件生成的 0x7FF 修改帧':'Only repeats 0x7FF modified frames produced by enabled plugins','仅限制速度偏移值下降速率，不控制 FSD 防御或 NAG':'Limits only the offset drop rate; does not control FSD defense or NAG','从 CAN 总线实时读取的车辆状态数据':'Live vehicle state read from the CAN bus',
  '从 GitHub Release 检查与安装匹配当前硬件的 OTA 固件':'Check and install OTA firmware for this hardware from GitHub Releases','优先级':'Priority','低速区间':'Low Range',
  '低速区间 cp1':'Low Range cp1','保存中...':'Saving...','保存到防御配置，避免速度策略频繁扰动':'Save into defense config to avoid speed-strategy churn',
  '保存后重启生效':'Reboot to apply after save','保存失败':'Save Failed','保存并重启':'Save & Reboot','加载失败':'Load Failed',
  '保存时会 POST /speed_strategy=fixed，并通过 /speed_custom 写入 manualPct。':'Saving POSTs /speed_strategy=fixed and writes manualPct via /speed_custom.','保存热点':'Save Hotspot','保存自定义偏移':'Save Custom Offset',
  '保守模式使用完整 12 条规则，激进模式使用最小 9 条规则':'Conservative mode uses the full 12 rules; aggressive uses the minimal 9','保留 AP/EAP 兼容策略位':'Keep AP/EAP compat policy bits','偏移 ':'Offset ',
  '停止失败':'Stop Failed','储备功能':'Reserved Feature','储备功能未开放':'Reserved feature, not yet available',
  '关闭':'Off','刹车状态':'Brake Status','剩余相位':'Phase Left','剩余':'Left',
  '危险操作需点击确认：重启设备会中断当前模块；重置计数会清零统计。':'Dangerous actions need confirmation: reboot interrupts modules; reset clears counters.','原生 0x045':'Native 0x045','双拨爆闪':'Double-Stalk Burst',
  '双拨状态':'Double-Stalk State','发现 ':'Found ','发现 ID':'Found IDs',
  '发现新版本':'New Version Found','发送错误':'Send Error','取消 / 重挂':'Cancel / Re-req',
  '取消/重挂':'Cancel/Re-req','取消已发':'Cancel Sent','取消生效':'Cancel Active',
  '后台流式写入，不阻塞 CAN':'Background streaming write, no CAN blocking','含 Model X CAN1 0x045 滚轮 RX/TX 及只读网关别名 0x238 / 0x292':'Model X CAN1 0x045 wheel RX/TX plus read-only gateway aliases 0x238 / 0x292','启动保存: ':'Boot save: ',
  '启用':'Enable','启用双拨触发':'Enable Double-Stalk Trigger','启用后按配置执行多次拨杆注入':'When enabled, runs multi-shot stalk injection per config',
  '启用自动偏移':'Enable Auto Offset','告警':'Alert','告警状态':'Alert Status',
  '回声/镜像':'Echo/Mirror','固件按当前 speedLimit 自动选择偏移比例，适合日常使用':'Firmware picks the offset ratio from the current speedLimit — good for daily use','固件版本':'Firmware Version',
  '固定':'Fixed','固定加值，范围 0-33 km/h':'Fixed add-on, 0-33 km/h','固定百分比':'Fixed Percent',
  '均衡响应':'Balanced Response','城市快速路':'Urban Expressway','城市快速路 cp2':'Urban Expressway cp2',
  '基于车速和驾驶模式的自动换挡策略（示意图）':'Speed- and style-based auto shift strategy (illustrative)','填写每个速度区间的偏移百分比，范围 0-50%':'Set offset percent per speed range, 0-50%','声音警告抑制':'Chime Suppression',
  '存储已满':'Storage Full','安全优先：暂无异常。实验功能请在停车和安全环境下验证。':'Safety first: no anomalies. Validate experimental features parked and safe.','安装':'Install',
  '安装中...':'Installing...','安装失败':'Install Failed','安装失败：':'Install failed: ',
  '安装成功':'Installed','实时':'Live','实时偏移':'Live Offset',
  '实车路径':'Field Path','实际 CAN 运行状态':'Actual CAN Runtime','实际 CAN/网络运行状态':'Actual CAN/Network Runtime',
  '实际偏移 actOffset':'Actual Offset actOffset','实际硬件模式':'Actual Hardware Mode','实验':'Exp',
  '实验 · 未验证实车防甩':'Exp · Field anti-jerk unverified','实验: ':'Exp: ','实验模式':'Experimental Mode',
  '实验项 0':'Exp Items 0','实验项需实车验证':'Experimental items need field validation','对齐':'Align',
  '就绪 / 阻断 ':'Ready / Blocked ','工程模式':'Engineering Mode','已保存':'Saved',
  '已写入':'Written','已写入 NVS':'Written to NVS','已安装插件':'Installed Plugins',
  '已开始安装':'Install Started','已拦截: ':'Blocked: ',' 条':' items',
  '已是最新版本':'Up to date','已自动填入 12 条规则':'12 rules auto-filled','已装 ':'Installed ',
  '已装 0 个':'Installed 0','已阻断':'Blocked','开启后，真实超车闪双拨才会自动执行预设爆闪':'When on, a real overtake double-stalk flash runs the preset burst',
  '开始记录':'Start Recording','当前参数':'Current Params','当前挡位':'Current Gear',
  '当前版本 ':'Current ','当前状态':'Current Status','当前硬件':'Current Hardware',
  '当前策略':'Current Policy','当前策略：':'Current policy: ','当前车速':'Current Speed',
  '当前选择：':'Current: ','当前驾驶风格':'Current Drive Style','录制中':'Recording',
  '录制时长':'Recording Length','待同步':'Sync Pending','待机':'Standby',
  '待检测':'Pending','忽略车辆 OTA 守卫':'Ignore Vehicle OTA Guard','成功':'OK',
  '失败':'fail','所有速度限制统一使用同一个偏移百分比':'One offset percent for every speed limit','手动':'Manual',
  '手动偏移':'Manual Offset','执行爆闪':'Run Burst','扫描结果':'Scan Results',
  '进入 Service 需在车机屏幕操作，本页仅显示状态':'Service Mode is enabled on the vehicle touchscreen; this page shows status only','换挡':'Shift','换挡策略预览':'Shift Strategy Preview',
  '控制':'Control','控制器':'Controller','控制当前 FSD 注入模块启停；状态会同步保存。':'Controls the FSD injection module on/off; state is saved.',
  '控制面板':'Control Panel','插件管理':'Plugin Manager','收尾中':'Finalizing',
  '收尾中...':'Finalizing...','数据':'Data','文件':'File',
  '无':'None','映射到 /lighting_config，并用 CAN2 stalk_test 执行爆闪序列':'Maps to /lighting_config; runs the burst via CAN2 stalk_test','普通点击不会触发危险动作；请长按确认后再启用模块。':'A normal tap does nothing dangerous; long-press to confirm before enabling.',
  '暂停':'Paused','暂无异常':'No Anomalies','更多':'More',
  '最低速档，适合拥堵跟车':'Lowest band, for stop-and-go','最后 Bus / Counter / CRC':'Last Bus / Counter / CRC','最大':'Max',
  '最大性能':'Max Performance','最近写入检查':'Last Write Check','未找到匹配固件':'No matching firmware',
  '未检查':'Not Checked','未激活':'Inactive','未生效':'Not Active',
  '未知':'Unknown','未踩':'Not Pressed','未验证':'Unverified',
  '未验证/实验':'Unverified/Exp','本行车段已站下':'Drive session ended','条件轮 上 / 下':'Cond Wheel Up / Dn',
  '标准':'Standard','标准模式，均衡性能':'Standard mode, balanced','核心开关':'Core Switch',
  '检查中...':'Checking...','检查失败':'Check Failed','检查最新 prerelease，默认关闭':'Check latest prerelease, default off',
  '模块总开关':'Module Master','模块总开关关闭；启用前请确认车辆处于安全状态。':'Module master off; confirm the vehicle is safe before enabling.','模式':'Mode',
  '次数':'Count','正向帧/反向帧/交替完成':'Fwd/Rev/Alternations','正在注入':'Injecting',
  '正在连接 GitHub Release，请稍候。':'Contacting GitHub Release, please wait.','正常':'Normal','此页面为只读遥测展示，自动换挡功能将在后续版本中实装':'Read-only telemetry; auto shift ships in a later release',
  '武装':'Armed','武装期(5s)':'Arm Window (5s)','注入来源':'Injection Source',
  '注入门控（服务端强制）':'Injection Gate (server-enforced)','测试中...':'Testing...','测试未发送':'Test Not Sent',
  '测试连接':'Test Connection','添加插件':'Add Plugin','激活':'Active',
  '灯光爆闪':'Light Burst','爆闪 OFF':'Burst OFF','爆闪 ON':'Burst ON',
  '爆闪开关':'Burst Switch','状态/本周期帧/下周期方向':'Status/Frames/Next Dir','独立 ISA 提示音策略，不联动滚轮或摄像头配置':'Standalone ISA chime policy; wheel and camera configs unaffected',
  '独立切换 MAX/V14 实验 profile，不影响模块总开关。':'Standalone MAX/V14 experimental profile; does not touch the module master.','独立四档：关闭 / 手动 / 自动 / 自定义。0x2F8 只读限速，0x3EE mux0 输出。':'Four modes: Off / Manual / Auto / Custom. 0x2F8 read-only limit, 0x3EE mux0 output.','现场遥控':'Remote Control',
  '生效 / 等待许可':'Active / Awaiting Permit','生效 / 运行':'Active / Running','生效模式':'Active Mode',
  '生效状态':'Active Status','监控':'Monitor','监控 AP 状态':'Watch AP Status',
  '目标总线':'Target Bus','硬件':'Hardware','确认上传 OTA 固件？上传完成可能会重启设备。':'Upload OTA firmware? Device may reboot after upload.',
  '确认保存网络配置并重启设备？':'Save network config and reboot?','确认安装 GitHub Release OTA 固件？设备会下载固件并自动重启。':'Install GitHub Release OTA firmware? Device downloads and reboots.','确认开启 FSD 注入？':'Enable FSD injection?',
  '确认开启 MAX/V14 实验模式？':'Enable MAX/V14 experimental mode?','确认执行灯光注入序列？':'Run lighting injection sequence?','确认执行灯光注入？':'Run lighting injection?',
  '确认重启设备？':'Reboot device?','积极':'Aggressive','积极提速，更快响应':'Faster ramp, quicker response',
  '稳定':'Stable','稳定模式':'Stable Mode','稳定模式仅注入 mux0 bit46，禁用 mux1；Tesla Controller 对齐模式用于受控 A/B 测试。':'Stable mode injects only mux0 bit46, mux1 disabled; Tesla Controller align mode is for controlled A/B tests.',
  '等待':'Waiting','等待 /status 确认':'Awaiting /status','等待 /status 速度确认':'Awaiting /status speed',
  '等待 0x399':'Waiting 0x399','等待 AP':'Waiting AP','等待 CAN1 0x045':'Waiting CAN1 0x045',
  '等待 CAN2 运行确认':'Awaiting CAN2 runtime','等待 DNS 运行确认':'Awaiting DNS runtime','等待中':'Waiting',
  '等待双拨':'Waiting Double-Stalk','等待条件':'Awaiting Condition','等待热点配置读取':'Awaiting hotspot config',
  '粘贴 JSON':'Paste JSON','系统自动选择最佳模式':'System picks the best mode','经济':'Eco',
  '缓降速率':'Slew Rate','编辑':'Edit','网络':'Network',
  '联网后自动检查并安装新版本，建议测试稳定后再启用':'Auto-check and install new versions when online; enable after testing','自动':'Auto','自动偏移':'Auto Offset',
  '自动偏移算法':'Auto Offset Algorithm','自动识别':'Auto Detect','自定义':'Custom',
  '自定义 4 区间偏移':'Custom 4-Range Offset','自定义偏移':'Custom Offset','自定义规则':'Custom Rules',
  '舒适':'Comfort','舒适驾驶，温和加速':'Comfortable, gentle acceleration','规则 ':'Rule ',
  '规则已保存':'Rules Saved','规则已应用，等待保存':'Rules applied, save pending','规则未读取':'Rules not loaded','规则测试':'Rule Test',
  '规则测试器':'Rule Tester','解析安装':'Parse & Install','触发源':'Trigger Source',
  '计数':'Count','许可丢失·已复位':'Permit Lost · Reset','设备信息':'Device Info',
  '设备在线':'Device Online','设备正在下载并写入 OTA 固件，完成后会自动重启。':'Device is downloading and flashing OTA firmware; it reboots when done.','设备重启后按保存值恢复 FSD 注入。':'FSD injection restores from the saved value after reboot.',
  '诊断':'Diagnostics','请先检查更新':'Check for updates first','请先连接 WiFi 后检查 GitHub Release。':'Connect WiFi before checking GitHub Release.',
  '请先选择 .json 文件':'Choose a .json file first','请检查设备连接':'Check device connection','请求失败':'Request Failed',
  '请点击保存规则写入 NVS':'Click Save Rules to write NVS','请输入 SSID':'Enter SSID','请输入中转 WiFi SSID':'Enter relay WiFi SSID',
  '读取中':'Loading','超时/故障复位':'Timeout/Fault Reset','超时·已复位':'Timeout · Reset',
  '踩下':'Pressed','车辆 OTA':'Vehicle OTA','车辆与防护':'Vehicle & Guard',
  '车辆态势':'Vehicle Status','车辆遥测（只读）':'Vehicle Telemetry (Read-only)','车速区间':'Speed Range',
  '转向>45°·中止突发':'Steer>45° · Abort Burst','转向>90°·全复位':'Steer>90° · Full Reset','转向中止/复位':'Steer Abort/Reset',
  '运行中':'Running','运行时间':'Uptime','运行状态待检测':'Status Pending',
  '进入':'Enter','进度':'Progress','远光序列':'High-Beam Sequence',
  '连接中':'Connecting','连接中...':'Connecting...','连接到设备热点':'Connect to device hotspot',
  '退出':'Exit','选择车辆自动驾驶硬件版本；Auto 会使用固件现有探测逻辑。':'Pick the vehicle AD hardware; Auto keeps the firmware probe logic.','选择速度策略映射到 /drive_profile':'Pick the speed policy mapped to /drive_profile',
  '通用防御与左滚轮 DND 分别独立控制':'General defense and left-wheel DND are controlled separately','速度':'Speed','速度免打扰':'Speed DND',
  '速度策略模式':'Speed Policy Mode','速度限制':'Speed Limit','速度限制 speedLimit':'Speed Limit speedLimit',
  '部分实验':'Partly Exp','配置已同步':'Config Synced','重复/冲突':'Dup/Conflict',
  '重挂已发':'Re-req Sent','重置凭据缓存':'Reset Cred Cache','锁止 / AP ':'Locked / AP ',
  '错误优先':'Errors First','错误优先的实时帧':'Live Frames, Errors First','长按 2 秒关闭 FSD 注入':'Hold 2 s to disable FSD injection',
  '长按 2 秒启用 FSD 注入':'Hold 2 s to enable FSD injection','长按关闭':'Hold to Disable','长按启用':'Hold to Enable',
  '门控未触发':'Gate Not Triggered','防御 OFF':'Defense OFF','防御 ON':'Defense ON',
  '防御关闭':'Defense Off','防御就绪':'Defense Ready','防护中':'Guarding',
  '阶段 / 原因':'Phase / Reason','阻断原因':'Block Reason','限速下降时平滑调整目标速度':'Smooths target speed when the limit drops',
  '限速覆盖':'Limit Override','隐藏热点':'Hide Hotspot','音量上 / 下':'Volume Up / Dn',
  '频率':'Frequency','驾驶':'Drive','驾驶状态中心':'Drive Status Center',
  '高于 60°C 标红':'Red above 60°C','高速巡航':'Highway Cruise','高速巡航 cp3':'Highway Cruise cp3',
  '高限速区间':'High Limit Range','高限速区间 cp4':'High Limit Range cp4','黑名单 / DNS 过滤规则':'Blacklist / DNS Filter Rules',
  '，点击“安装更新”后设备会下载并重启。':', click "Install Update" to download and reboot.','🌙 夜间':'🌙 Night','手部':'Hands',
  '删除':'Delete','暂无插件。新安装默认禁用。':'No plugins yet. New installs start disabled.','目标ID: ':'Target ID: ',
  '详情':'Details','8.3.6防甩机制 ':'8.3.6 Anti-Jerk ','AP/EAP 兼容 ':'AP/EAP Compat ',
  'CAN2 ID 列表 ':'CAN2 ID List ','CAN2 控制 ':'CAN2 Control ','FSD-V14 模式 ':'FSD-V14 Mode ',
  'Legacy 左滚轮免打扰 ':'Legacy Left-Wheel DND ','Release 在线更新 ':'Release Online Update ','双拨爆闪 ':'Double-Stalk Burst ',
  '忽略车辆 OTA 守卫 ':'Ignore Vehicle OTA Guard ','最大性能 ':'Max Performance ','爆闪开关 ':'Burst Switch ',
  '自动换挡 ':'Auto Shift ','速度免打扰 ':'Speed DND ','限速覆盖 ':'Limit Override ',
  '未知 / 无效 / SNA 档位默认禁止注入；AP 断开立即清零 Gate 计时。此策略由服务端 C++ 强制（handlers.h），客户端 UI 无法绕过。':'Unknown / invalid / SNA gears block injection by default; AP disconnect resets gate timing instantly. Enforced server-side in C++ (handlers.h) — the web UI cannot bypass it.',
};
function T(zh){return lang==='en'&&I18N[zh]?I18N[zh]:zh}
var CAP={singleCan:false,can2Available:true,lightingBusSupported:true,serviceModeSupported:true,stalkTestSupported:true,bus2SnifferSupported:true,fsdActivation:true,speedStrategy:true,driveProfile:true,networkSettings:true,otaUpdate:true,canDiagnostics:true};
var PRODUCT_SINGLE_CAN_NAME='Atlas Single CAN';
var PRODUCT_DUAL_CAN_NAME='Atlas T-2CAN';
function isSingleCan(){return !!CAP.singleCan}
function applyProductMode(){
  var name=isSingleCan()?PRODUCT_SINGLE_CAN_NAME:PRODUCT_DUAL_CAN_NAME;
  document.querySelectorAll('[data-product-name]').forEach(function(n){n.textContent=name});
  document.querySelectorAll('[data-single-hide]').forEach(function(n){n.classList.toggle('cap-hidden',isSingleCan())});
  document.querySelectorAll('[data-dual-hide]').forEach(function(n){n.classList.toggle('cap-hidden',!isSingleCan())});
  document.querySelectorAll('[data-single-text]').forEach(function(n){if(isSingleCan())n.textContent=n.getAttribute('data-single-text')});
}
function capEnabled(name){
  if(name==='can2')return !!CAP.can2Available;
  if(name==='lighting')return !!CAP.lightingBusSupported;
  return true;
}
function applyCapabilities(c){
  if(c){for(var k in c)CAP[k]=c[k]}
  var nodes=document.querySelectorAll('[data-cap]');
  for(var i=0;i<nodes.length;i++){
    var n=nodes[i];
    var show=capEnabled(n.getAttribute('data-cap'));
    n.classList.toggle('cap-hidden',!show);
  }
  var active=document.querySelector('.page.active');
  if(active&&active.getAttribute('data-cap')&&!capEnabled(active.getAttribute('data-cap'))){
    showPage('pg-overview');
  }
  // 5.0.0-beta04: CAN row widens to 5 columns only when CAN2 is present
  var canRow=$('ov-can-row');
  if(canRow)canRow.classList.toggle('m5',capEnabled('can2'));
  applyProductMode();
}
function applyI18n(){
  var nodes=document.querySelectorAll('[data-i18n]');
  for(var i=0;i<nodes.length;i++){
    nodes[i].textContent=T(nodes[i].getAttribute('data-i18n'));
  }
  $('lang-btn').textContent=lang==='zh'?'EN':'中';
}

// ── Theme ──────────────────────────────────────────────────
function toggleTheme(){
  dark=!dark;
  document.documentElement.classList.toggle('light',!dark);
  try{localStorage.setItem('tcanTheme',dark?'dark':'light');}catch(e){}
  if($('theme-btn'))$('theme-btn').textContent=dark?'☀':'🌙';
  if($('mobile-theme-btn'))$('mobile-theme-btn').textContent=dark?T('☀ 白天'):T('🌙 夜间');
}

// ── Language ───────────────────────────────────────────────
function toggleLanguage(){
  lang=lang==='zh'?'en':'zh';
  applyI18n();
  // 5.0.0-beta04: re-render from the cached /status payload plus light re-polls,
  // so a language switch no longer leaves stale Chinese until the next poll
  if(S.lastStatus)renderStatus(S.lastStatus);
  pollCAN2();loadSpeedStrategy();pollWifiStatus();loadPlugins();
}

// ── Navigation ─────────────────────────────────────────────
function showPage(pageId){
  var mobMore=$('mob-more');if(mobMore)mobMore.classList.remove('open');
  var _tgt=document.getElementById(pageId);
  if(_tgt&&_tgt.getAttribute('data-single-hide')&&isSingleCan())pageId='pg-overview';
  var pages=document.querySelectorAll('.page');
  for(var i=0;i<pages.length;i++)pages[i].classList.remove('active');
  var p=$(pageId);if(p)p.classList.add('active');
  var navs=document.querySelectorAll('.nav-item');
  for(var i=0;i<navs.length;i++){
    navs[i].classList.toggle('active',navs[i].getAttribute('data-page')===pageId);
  }
  closeSidebar();
  if(typeof updateMobTabs==='function')updateMobTabs(pageId);
  var mobDualTabs=document.querySelectorAll('#mob-tabs-dual .mob-tab[data-page]');
  for(var i=0;i<mobDualTabs.length;i++){
    mobDualTabs[i].classList.toggle('active',mobDualTabs[i].getAttribute('data-page')===pageId);
  }
  var mobMoreItems=document.querySelectorAll('#mob-more .mob-more-item[data-page]');
  for(var i=0;i<mobMoreItems.length;i++){
    mobMoreItems[i].classList.toggle('active',mobMoreItems[i].getAttribute('data-page')===pageId);
  }
  if(pageId==='pg-can')pollCanTab();
  if(pageId==='pg-drive')loadDriveProfile();
  if(pageId==='pg-bus2'&&CAP.can2Available&&!isSingleCan())pollCAN2();
  if(pageId==='pg-overview'&&CAP.can2Available&&!isSingleCan())pollCAN2();
  if(pageId==='pg-bus2'&&CAP.lightingBusSupported&&!isSingleCan())loadLightingConfig();
  if(pageId==='pg-bus2'&&CAP.can2Available&&!isSingleCan())loadBurstConfig();
  if(pageId==='pg-speed')loadSpeedStrategy();
  if(pageId==='pg-defense')loadDefenseConfig();
  if(pageId==='pg-shift'&&!isSingleCan())pollGearAssist();
  if(pageId==='pg-ota'){loadFirmwareInfo();loadOtaReleaseState();}
  if(pageId==='pg-network'){pollWifiStatus();pollGatewayStatus();loadGatewayDns();loadGatewayBlocked();}
}
function openSidebar(){$('sidebar').classList.add('open');$('overlay').classList.add('active')}
function closeSidebar(){$('sidebar').classList.remove('open');$('overlay').classList.remove('active')}

// ── Fetch Helper ───────────────────────────────────────────
var _fetchErrCount=0;
// 5.0.0-beta04: auto-fit cockpit metric font by value length (≤4→24px, 5-6→20, 7-8→17, >8→14)
function fitCockpitVals(){
  var els=document.querySelectorAll('.cockpit-metric .val');
  for(var i=0;i<els.length;i++){
    var n=(els[i].textContent||'').trim().length;
    els[i].classList.remove('vf-md','vf-sm','vf-xs');
    if(n>8)els[i].classList.add('vf-xs');
    else if(n>=7)els[i].classList.add('vf-sm');
    else if(n>=5)els[i].classList.add('vf-md');
  }
}
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
  S.lastStatus=d;
  renderStatus(d);
}
// 5.0.0-beta04: render pipeline split out of poll() so the language switch can
// re-render every page from the cached payload without waiting for the next poll
function renderStatus(d){
  var uptime=(d.uptime!==undefined)?d.uptime:(d.up||0);
  S.hw=d.hw;S.ci=d.ci;S.sp=d.sp;S.spa=d.spAuto;S.can=d.can;S.ia=d.ia;S.driveProfile=d.driveProfile!==undefined?d.driveProfile:S.driveProfile;
  applyCapabilities(d.capabilities||{});
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
  setText('tb-up',fmtUp(uptime));

  // Overview
  var ovTgl=$('ov-fsd-tgl');
  if(ovTgl)ovTgl.checked=(S.driveProfile===5);
  var masterTgl=$('ov-master-tgl');
  if(masterTgl)masterTgl.checked=!!d.ci;
  var bootTgl=$('fsd-boot-tgl');
  if(bootTgl)bootTgl.checked=!!(d.bootCan!==undefined?d.bootCan:d.ci);
  var mFsdTgl=$('m-fsd-tgl');
  if(mFsdTgl){
    if(mFsdTgl.type==='checkbox')mFsdTgl.checked=!!d.ci;
    else if(mFsdTgl.classList&&mFsdTgl.classList.contains('mobile-hold'))mFsdTgl.className='mobile-hold '+(d.ci?'on':'');
    else if(mFsdTgl.classList)mFsdTgl.classList.toggle('on',!!d.ci);
  }
  setCls('ov-v14','stat-val '+(S.driveProfile===5?'v-warn':'v-dim'));
  setText('ov-v14',S.driveProfile===5?'MAX/V14':T('待机'));
  var hwText=d.hwName||hwLabel(d.hw);
  setText('ov-hw',hwText);
  setCls('ov-can','stat-val '+(d.can?'v-ok':'v-err'));
  setText('ov-can', d.can ? 'CAN1 Online' : 'CAN1 Offline');
  // AP 激活状态（问题2：替换 CAN2，与 ap-core-state-pill 同源字段 d.apInjectionState）
  var apState = d.apInjectionState || 'blocked';
  var apActive = (apState === 'injecting' || apState === 'armed');
  setText('ov-ap', apActive ? T('激活') : T('等待'));
  var apMetric = $('ov-ap-metric');
  if (apMetric) apMetric.style.borderColor = apActive ? 'rgba(52,211,153,.45)' : '';
  var apHint = $('ov-ap-hint');
  if (apHint) apHint.textContent = (apState==='injecting' ? T('正在注入') : (apState==='armed' ? T('Legacy 已就绪') : T('门控未触发')));
  setText('ov-up',fmtUp(uptime));
  var mCan=$('m-can');
  if(mCan){mCan.className=(mCan.classList&&mCan.classList.contains('val')?'val ':'stat-val ')+(d.can?'v-ok':'v-err');mCan.textContent=d.can?'Online':'Offline';}
  var mCan2=$('m-can2');
  var can2Online=($('s-can2')&&$('s-can2').textContent==='Online');
  var can2Text=($('s-can2')&&$('s-can2').textContent)?$('s-can2').textContent:'--';
  if(mCan2){mCan2.className=(mCan2.classList&&mCan2.classList.contains('val')?'val ':'stat-val ')+(can2Online?'v-ok':'v-dim');mCan2.textContent=can2Text;}
  setFsdVisualState(!!d.ci);
  setText('m-fps',d.fps.toFixed(1)+' Hz');
  setText('m-rxtx',(d.rx||0)+' / '+(d.tx||0));
  setText('m-up',fmtUp(uptime));
  setText('m-hw',(d.hwName||hwLabel(d.hw)));
  setText('m-drive',driveModeLabel(driveModeFromProfile(d.driveProfile,d.driveProfileName)));
  setText('m-speed',d.soff!==undefined?d.soff:'--');
  setText('m-defense',d.hw3OffsetSlew?'ON':'OFF');
  // OTA status
  setCls('s-vota','stat-val '+(d.vehicleOta?'v-err':'v-ok'));
  setText('s-vota',d.vehicleOta?T('OTA 进行中'):T('正常'));
  var otaCnt=$('s-vota-cnt');
  if(otaCnt) fetchJson('/vehicle_ota_status').then(function(o){if(o)setText('s-vota-cnt',o.otaConfirmCount||0)}).catch(function(){});
  var alertText=[];
  if(!d.can)alertText.push(T('CAN 离线'));
  if(d.txerr)alertText.push('TX Err '+d.txerr);
  if(d.apGate)alertText.push(T('AP Gate 等待'));
  setText('m-alert',alertText.length?alertText.join(' / '):T('暂无异常'));
  var sCanEl=$('s-can');
  if(sCanEl){sCanEl.className=(sCanEl.classList.contains('val')?'val ':'stat-val ')+(d.can?'v-ok':'v-err');sCanEl.textContent=d.can?'Online':'Offline';}
  setText('s-rx',(d.rx||0)+'/'+(d.tx||0));
  setText('s-tx',d.tx||0);
  if(d.can2){
    setText('b2-tx',d.can2.tx||0);
    setText('b2-txerr',d.can2.txerr||0);
    setText('b2-eflg','0x'+toHex(d.can2.eflg||0,2));
  }
  setText('s-fps',d.fps.toFixed(1)+' Hz');
  setText('s-hw',(d.hwName||hwLabel(d.hw)));
  setText('s-soff',d.soff||0);
  // Temp from /system_status is separate; use eflg field as proxy
  setText('s-txerr',d.txerr||0);
  setText('diag-rxtx',((d.rxCount||d.rx||0)+' / '+(d.txCount||d.tx||0)));
  setText('st-can-pill',((d.rxCount||d.rx||0)+' / '+(d.txCount||d.tx||0)));
  setText('diag-txerr',(d.txErr||d.txerr||(d.can2&&d.can2.txerr)||d.can2TxErr||0));
  var diagEflg=(d.can2Eflg!==undefined)?d.can2Eflg:((d.can2&&d.can2.eflg!==undefined)?d.can2.eflg:undefined);
  var diagEflgNum=Number(diagEflg);
  var diagEflgValid=(typeof Number.isFinite==='function')?Number.isFinite(diagEflgNum):(typeof diagEflgNum==='number'&&isFinite(diagEflgNum));
  setText('diag-eflg',diagEflgValid?('0x'+diagEflgNum.toString(16).padStart(2,'0').toUpperCase()):'0x00');
  setText('s-fd',d.fd||0);
  setStatusTriplet('module',d.ci?'FSD ON':'FSD OFF',T('启动保存: ')+((d.bootCan!==undefined?d.bootCan:d.ci)?'ON':'OFF'),d.can?'CAN Online':'CAN Offline',d.can?'ok':'err');
  setStatusTriplet('hw',(d.hwName||hwLabel(d.hw)),'mode_hw: '+(d.hwName||hwLabel(d.hw)),(d.can?T('CAN运行 / '):T('CAN离线 / '))+(d.hwName||hwLabel(d.hw)),d.can?'ok':'warn');
  setStatusTriplet('speed',T('偏移 ')+(d.soff!==undefined?d.soff:'--'),driveLabel(d.sp,d.spAuto),d.fusedSpeedLimitKph?('Fused '+d.fusedSpeedLimitKph+' kph'):T('CAN未给出速度'),d.can?'ok':'warn');
  setStatusTriplet('defense',d.defenseEnabled?T('防御 ON'):T('防御 OFF'),'slew '+(d.hw3OffsetSlew?'ON':'OFF'),d.defenseEnabled?T('防御就绪'):T('防御关闭'),d.defenseEnabled?'ok':'warn');

  // FSD page toggle
  updateFsdToggle(d.ci);

  // Quick action FSD button
  var qaBtn=$('qa-fsd');var qaSt=$('qa-fsd-st');
  if(qaBtn){qaBtn.classList.toggle('active',!!d.ci)}
  if(qaSt){qaSt.textContent=d.ci?'ON':'OFF';qaSt.style.color=d.ci?'var(--ok)':'var(--err)'}

  // Boot toggle — show stored boot preference, not current session state
  var bt=$('fsd-boot-tgl');
  if(bt)bt.checked=!!(d.bootCan!==undefined?d.bootCan:d.ci);

  // HW page
  updateHwCards(d.hw);
  updateProfileCards(d.sp);
  updateDriveCards(d.sp,d.spAuto,d.driveProfile,d.driveProfileName);

  // Speed page
  updateSpeedPage(d);

  // Defense page
  updateDefensePage(d);

  // Legacy Real Wiring truth cards
  renderLegacyTruth(d);

  // CAN controller sub-page
  updateCanController(d);

  // AP injection core (standalone driving status)
  renderApInjectionState(d);

  // 5.0.0-beta04: keep cockpit metric values single-line at any length
  fitCockpitVals();
}

function updateFsdToggle(ci){
  var tgl=$('fsd-toggle');
  if(tgl){
    if(tgl.classList.contains('cockpit-hold')){
      tgl.classList.toggle('on',!!ci);
    }else if(ci){tgl.className='big-toggle on'}
    else{tgl.className='big-toggle off'}
  }
  setFsdVisualState(!!ci);
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

function driveModeFromProfile(profile,name){
  var modes=['auto','sloth','chill','normal','hurry','max'];
  if(profile!==undefined&&profile!==null&&profile>=0&&profile<modes.length)return modes[profile];
  var n=String(name||'').toLowerCase();
  return modes.indexOf(n)>=0?n:'normal';
}
function driveModeLabel(mode){
  return {auto:'Auto',sloth:'Sloth',chill:'Chill',normal:'Normal',hurry:'Hurry',max:'MAX'}[mode]||'Normal';
}
function updateDriveCards(sp,spa,profile,profileName){
  var cards=$('drive-cards');
  if(!cards)return;
  var active=(profile!==undefined&&profile!==null)?driveModeFromProfile(profile,profileName):(spa?'auto':(sp===0?'chill':(sp===2?'hurry':'normal')));
  updateDriveCardsByMode(active);
}

function updateDriveSurfaces(mode, statusText){
  var label=driveModeLabel(mode);
  setText('drive-current',label);
  setText('st-drive-pill',label);
  setText('ov-drive-current',label);
  setText('m-drive',label);
  setStatusTriplet('drive',label,label,statusText||(mode==='max'?T('MAX/V14 需实车验证'):T('配置已同步')),mode==='max'?'warn':'ok');
  var v14=$('ov-fsd-tgl');if(v14)v14.checked=(mode==='max');
  setCls('ov-v14','stat-val '+(mode==='max'?'v-warn':'v-dim'));
  setText('ov-v14',mode==='max'?'MAX/V14':T('待机'));
  setText('tb-exp',experimentSummary());
}

function updateDriveCardsByMode(active){
  var cards=$('drive-cards');
  var items=cards?cards.querySelectorAll('.drive-card'):[];
  var modes=['auto','sloth','chill','normal','hurry','max'];
  for(var i=0;i<items.length;i++)items[i].classList.toggle('active',modes[i]===active);
  updateDriveSurfaces(active);
}

async function loadDriveProfile(){
  var d=await fetchJson('/drive_profile');
  if(!d)return;
  var mode=driveModeFromProfile(d.value,d.profile);
  S.driveProfile=d.value!==undefined?d.value:S.driveProfile;
  updateDriveCardsByMode(mode);
  updateDriveSurfaces(mode,T('配置已同步'));
}

var speedStrategyState='auto';
var speedManualPct=30;
var legacySpeedModeState='off';

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
  setText('st-speed-pill',speedStrategyState);
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
  var ls=d.legacySmartSpeed||{};
  setText('sp-legacy-source',ls.limitSource&&ls.limitSource!=='none'?(ls.limitSource+' / '+(ls.limitKph||0)+' kph'):'--');
  setText('sp-legacy-target',ls.smoothedTargetKph?ls.smoothedTargetKph+' kph':'--');
  setText('sp-legacy-bus',(ls.busMode||'Auto')+' / '+(ls.targetBus||'AUTO'));
  setText('sp-legacy-runtime',ls.runtimeActive?T('运行中'):(ls.configured?T('等待条件'):T('未配置')));
  setText('sp-legacy-rx2f8',ls.rx2F8||0);
  setText('sp-legacy-rx3ee',ls.rx3EE||0);
  setText('sp-legacy-tx',(ls.txOk||0)+' / '+(ls.txFail||0));
  setText('sp-legacy-block',ls.blockedReason||'--');
  if(ls.legacyHandler){
    var run=ls.runtimeActive?('Legacy '+(ls.offsetKph||0)+' kph / '+(ls.targetBus||'AUTO')):('Legacy '+(ls.blockedReason||'waiting'));
    setStatusTriplet('speed',speedStrategyState,'Legacy '+(ls.mode||speedStrategyState),run,ls.runtimeActive?'ok':(ls.busAmbiguous?'err':'warn'));
  }
}

function updateDefensePage(d){
  var master=$('def-master-tgl');
  if(master)master.checked=!!d.defenseEnabled;
  var tgl=$('hw3-slew-tgl');
  if(tgl)tgl.checked=!!d.hw3OffsetSlew;
  setText('def-rate',d.hw3SlewRate!==undefined?d.hw3SlewRate+'%/s':'--');
  setText('def-min',d.hw3OffsetTarget!==undefined?d.hw3OffsetTarget:'--');
  setText('def-cnt',d.hw3SlewCount||0);
  setText('def-cur',d.hw3OffsetLast!==undefined?d.hw3OffsetLast:'--');
  var dot=$('def-dot');
  var statusEl=$('def-status');
  if(d.defenseEnabled){
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
  var master=$('def-master-tgl');
  if(master)master.checked=!!d.enabled;
  var tgl=$('hw3-slew-tgl');
  if(tgl)tgl.checked=!!d.slew_rate_enabled;
  var sound=$('def-sound-tgl');if(sound)sound.checked=!!d.sound_warning_suppression;
  var isaOvr=$('def-isa-override-tgl');if(isaOvr)isaOvr.checked=!!d.isa_override;
  var dndVol=$('def-dnd-vol-tgl');if(dndVol)dndVol.checked=!!d.dnd_volume;
  var nd=$('def-speed-nd-tgl');if(nd)nd.checked=!!d.speed_no_disturb;
  var dndSpd=$('def-dnd-spd-tgl');if(dndSpd)dndSpd.checked=!!d.dnd_speed;
  var apeap=$('def-apeap-tgl');if(apeap)apeap.checked=!!d.ap_eap_compatible;
  var abortTgl=$('def-abort-guard-tgl');if(abortTgl)abortTgl.checked=!!d.abort_guard;
  var otaOvr=$('def-ota-ovr-tgl');if(otaOvr)otaOvr.checked=!!d.ota_guard_override;
  var jtTgl=$('def-jitter-tgl');if(jtTgl)jtTgl.checked=!!d.jitter;
  var safety=d.legacyInjectionSafety||{};
  var abort=safety.abortGuard||{};
  // v1.19 8.3.6 JITTER 诊断：legacyInjectionSafety.jitter 对象（逐字移植自
  // 单 CAN v1.19.2 顶层 jitter；LittleGong 对齐精简版——一个阶段、一个原因、一组计数器）。
  var jt=safety.jitter||{};
  var jtReasonMap={off:T('关闭'),idle:T('待机'),monitoring:T('监控 AP 状态'),
    arming:T('武装期(5s)'),cancel:T('取消已发'),cancelSeen:T('取消生效'),reRequest:T('重挂已发'),
    reEngagedFsd:T('FSD 落地'),reEngagedEap:T('EAP 落地'),
    sessionCap:T('本行车段已站下'),parked:T('P 档·已复位'),
    apGateOff:T('AP 门控未开启'),permitLost:T('许可丢失·已复位'),
    timeout:T('超时·已复位'),apError:T('AP 故障·已复位'),
    steerAbort:T('转向>45°·中止突发'),steerReset:T('转向>90°·全复位')};
  var jtPhaseMap={inert:T('未生效'),idle:T('待机'),monitoring:T('监控'),arming:T('武装'),disengaging:T('取消/重挂')};
  setText('jitter-effective',jt.effective?(jt.permit?T('生效 / 运行'):T('生效 / 等待许可')):(jt.requested?T('不可用 · AP 门控未开启'):T('关闭')));
  setText('jitter-phase',(jtPhaseMap[jt.phase]||jt.phase||'--')+' / '+(jtReasonMap[jt.reason]||jt.reason||'--'));
  setText('jitter-ap',(jt.apState!=null?jt.apState:'--')+' / '+(jt.cycles||0));
  setText('jitter-bursts',(jt.cancels||0)+' / '+(jt.requests||0));
  // v1.19.2: landing split — FSD(state6)/EAP(state3) after our 0x42;
  // bit46 cell shows arming shots / pre-re-request refreshes.
  setText('jitter-landing',(jt.reengagedFsd||0)+' / '+(jt.reengagedEap||0));
  setText('jitter-frames',(jt.bit46Shots||0)+' / '+(jt.bit46Refreshes||0));
  setText('jitter-tx',(jt.txOk||0)+' / '+(jt.txFail||0));
  setText('jitter-steer',(jt.steerAborts||0)+' / '+(jt.steerResets||0));
  setText('jitter-resets',(jt.timeoutResets||0)+' / '+(jt.apErrorResets||0));
  setText('def-abort-state',abort.enabled?(abort.latched?(T('锁止 / AP ')+(abort.lastAbortState??'--')):(T('就绪 / 阻断 ')+(abort.blocks||0))):T('关闭'));
  setText('def-status',d.enabled?T('保护已启用'):T('保护未启用'));
  var dot=$('def-dot');if(dot)dot.className='status-dot '+(d.enabled?'ok':'err');
  var exp=(d.speed_no_disturb||d.ap_eap_compatible||d.dnd_volume||d.dnd_speed);
  setText('tb-exp',experimentSummary());
}

// ── FSD Toggle ─────────────────────────────────────────────
async function toggleFsd(){
  var next=S.ci?0:1;
  if(next&&!confirm(T('确认开启 FSD 注入？'))){poll();return}
  // 主开关同时更新当前会话和开机默认值，重启后保持一致。
  try{await postForm('/config',{can:next?'1':'0',force:next?'1':'0',bootCan:next?'1':'0'});}
  catch(e){return}
  S.ci=!!next;
  S.bootCan=!!next;
  var masterTgl=$('ov-master-tgl');
  if(masterTgl)masterTgl.checked=S.ci;
  var mFsdTgl=$('m-fsd-tgl');
  if(mFsdTgl){
    if(mFsdTgl.type==='checkbox')mFsdTgl.checked=S.ci;
    else if(mFsdTgl.classList&&mFsdTgl.classList.contains('mobile-hold'))mFsdTgl.className='mobile-hold '+(S.ci?'on':'');
    else if(mFsdTgl.classList)mFsdTgl.classList.toggle('on',S.ci);
  }
  var bootTgl=$('fsd-boot-tgl');
  if(bootTgl)bootTgl.checked=S.bootCan;
  updateFsdToggle(S.ci);
  if(masterTgl)setFsdVisualState(!!masterTgl.checked);
  poll();
}

async function toggleV14Mode(){
  var t=$('ov-fsd-tgl');
  var enable=!!(t&&t.checked);
  if(enable&&!confirm(T('确认开启 MAX/V14 实验模式？'))){if(t)t.checked=false;return}
  try{await postForm('/drive_profile',{profile:enable?'max':'normal'});}
  catch(e){if(t)t.checked=!enable;return}
  S.driveProfile=enable?5:3;
  updateDriveCardsByMode(enable?'max':'normal');
  setCls('ov-v14','stat-val '+(enable?'v-warn':'v-dim'));
  setText('ov-v14',enable?'MAX/V14':T('待机'));
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
  setStatusTriplet('hw',hwLabel(hw),'mode_hw: '+hwLabel(hw),T('等待 /status 确认'),'warn');
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
  var driveMap={auto:0,sloth:1,chill:2,normal:3,hurry:4,max:5};
  if(driveMap[mode]===undefined)mode='normal';
  updateDriveCardsByMode(mode);
  updateDriveSurfaces(mode,T('保存中...'));
  var saved;
  try{saved=await postForm('/drive_profile',{profile:mode});}
  catch(e){setStatusTriplet('drive',driveModeLabel(mode),T('保存失败'),T('请检查设备连接'),'err');return}
  var savedMode=saved&&saved.value!==undefined?driveModeFromProfile(saved.value,saved.profile):mode;
  S.spa=savedMode==='auto';
  S.sp=(savedMode==='sloth'||savedMode==='chill')?0:((savedMode==='hurry'||savedMode==='max')?2:1);
  S.driveProfile=driveMap[savedMode]!==undefined?driveMap[savedMode]:3;
  updateDriveCardsByMode(savedMode);
  updateProfileCards(S.sp);
  updateDriveSurfaces(savedMode,T('已保存'));
  setTimeout(poll,300);
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
  var legacyResp=await fetchJson('/legacy_speed_config');
  if(customResp)applySpeedCustom(customResp);
  if(legacyResp)applyLegacySpeedConfig(legacyResp);
  var strategy=(strategyResp&&strategyResp.strategy)||'auto';
  showSpeedMode(strategy);
  setStatusTriplet('speed',strategy,strategy,T('等待 /status 速度确认'),strategy==='custom'?'warn':'ok');
}

function legacySpeedVal(id,def,max){
  var el=$(id),v=el?parseInt(el.value,10):def;
  if(isNaN(v))v=def;
  if(v<0)v=0;
  if(v>max)v=max;
  if(el)el.value=String(v);
  return v;
}

function updateLegacySpeedMode(mode){
  legacySpeedModeState=mode||'off';
  var cards=$('legacy-speed-mode');
  if(cards){
    var items=cards.querySelectorAll('.sel-card'),values=['off','manual','auto','custom'];
    for(var i=0;i<items.length;i++)items[i].classList.toggle('active',values[i]===legacySpeedModeState);
  }
  var manual=$('legacy-speed-manual-row'),custom=$('legacy-speed-custom-panel');
  if(manual)manual.style.display=legacySpeedModeState==='manual'?'flex':'none';
  if(custom)custom.style.display=legacySpeedModeState==='custom'?'block':'none';
}

function applyLegacySpeedConfig(d){
  if(!d)return;
  var bus=$('legacy-speed-bus');if(bus)bus.value=d.bus||'Auto';
  var smooth=$('legacy-speed-smooth');if(smooth)smooth.checked=!!d.smooth_down;
  var rate=$('legacy-speed-rate');if(rate)rate.value=d.smooth_rate_kph_s||5;
  var manual=$('legacy-speed-manual');if(manual&&d.manual_offset_kph!==undefined)manual.value=d.manual_offset_kph;
  var vals=d.custom_pct||[];
  for(var i=0;i<4;i++){var el=$('legacy-speed-cp'+(i+1));if(el&&vals[i]!==undefined)el.value=vals[i];}
  updateLegacySpeedMode(d.mode||'off');
}

async function setLegacySpeedMode(mode){
  updateLegacySpeedMode(mode);
  await saveLegacySpeedConfig();
}

async function saveLegacySpeedConfig(){
  var bus=$('legacy-speed-bus'),smooth=$('legacy-speed-smooth'),rate=$('legacy-speed-rate');
  var data={
    mode:legacySpeedModeState,
    manual_offset_kph:String(legacySpeedVal('legacy-speed-manual',0,33)),
    cp1:String(legacySpeedVal('legacy-speed-cp1',50,63)),
    cp2:String(legacySpeedVal('legacy-speed-cp2',30,63)),
    cp3:String(legacySpeedVal('legacy-speed-cp3',20,63)),
    cp4:String(legacySpeedVal('legacy-speed-cp4',10,63)),
    bus:bus?bus.value:'Auto',
    smooth_down:smooth&&smooth.checked?'1':'0',
    smooth_rate_kph_s:String(rate&&rate.value?rate.value:5)
  };
  try{
    var d=await postForm('/legacy_speed_config',data);
    applyLegacySpeedConfig(d);
    showToast(T('已保存')||'Saved',true);
  }catch(e){loadSpeedStrategy();}
}

async function setSpeedStrategy(strategy){
  showSpeedMode(strategy);
  try{await postForm('/speed_strategy',{strategy:strategy});showToast(T('已保存')||'Saved',true)}
  catch(e){loadSpeedStrategy();return}
  setStatusTriplet('speed',strategy,strategy,T('等待 /status 速度确认'),strategy==='custom'?'warn':'ok');
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
  setStatusTriplet('speed','fixed','manualPct '+pct+'%',T('等待 /status 速度确认'),'ok');
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
  setStatusTriplet('speed','custom',T('cp1-cp4 已保存'),T('等待 /status 速度确认'),'warn');
}

// ── Save Config (generic toggle) ───────────────────────────
async function saveConfig(){
  var data={};
  var bt=$('fsd-boot-tgl');
  if(bt)data.bootCan=bt.checked?'1':'0';
  try{await postForm('/config',data);}catch(e){}
}
// ap-gate-tgl (hardware page) removed: the gate toggle now lives on the
// FSD-defense page as ap-core-gate-tgl, refreshed by renderApInjectionState().
// hw-ap-restore (hardware page) removed in 5.0-beta01: AP 状态恢复 is
// controlled from the FSD-defense page as ap-auto-restore-tgl.

// ── AP injection core controls (standalone driving status) ────────────
// AP-settle delay options removed in 4.5.0-beta06 (the 8.3.6 coordinator
// now owns injection timing; the legacy settle timer is gone).
async function saveApGateControls(){
  var gate=document.getElementById('ap-core-gate-tgl');
  var restore=document.getElementById('ap-auto-restore-tgl');
  try{
    await postForm('/config',{
      apg:gate&&gate.checked?'1':'0',
      ap_auto_restore:restore&&restore.checked?'1':'0'
    });
    showToast(T('已保存'));
  }catch(e){}
}
function renderApInjectionState(d){
  if(!d)return;
  var gate=(d.fsdDiag&&d.fsdDiag.gate)?d.fsdDiag.gate:{};
  var state=d.apInjectionState||'blocked';
  var label=state==='waiting_ap'?T('等待 AP'):state==='injecting'?T('正在注入'):state==='armed'?T('Legacy 已就绪'):T('已阻断');
  setText('ap-core-state-pill',label);
  // 驾驶舱状态主区 + Gate 进度条（仅显示层，不读新后端字段）
  var apActive = (state==='injecting'||state==='armed');
  var apBig = document.getElementById('ap-core-state-big');
  var apPanel = document.getElementById('ap-state-panel');
  var apFill = document.getElementById('ap-gate-fill');
  if (apBig) apBig.textContent = apActive ? T('激活') : T('等待');
  if (apPanel) apPanel.classList.toggle('active', apActive);
  if (apFill) apFill.style.width = (apActive ? '100%' : '0%');
  var reason=gate.lastBlockedBy||gate.gateReason||'';
  setText('ap-core-state-detail',label+(reason?' · '+reason:''));
  setText('injection-source',d.injectionSource||'Disabled');
  var apg=document.getElementById('ap-core-gate-tgl'); if(apg)apg.checked=!!d.apGateEnabled;
  var rst=document.getElementById('ap-auto-restore-tgl'); if(rst)rst.checked=!!d.apAutoRestore;
}
// ── 插件管理（JSON 插件） ──────────────────────────────────────
var PLUGINS={installed:[],replayCount:1};
function loadPlugins(){
  return fetch('/plugins/status').then(function(r){return r.json()}).then(function(j){PLUGINS=j;renderPluginsStatus(j)}).catch(function(){});
}
function installPluginUrl(){
  var v=document.getElementById('plugin-url').value;
  return postForm('/plugins/install_url',{url:v}).then(loadPlugins);
}
function installPluginJson(){
  var v=document.getElementById('plugin-json').value;
  return fetch('/plugins/install_json',{method:'POST',headers:{'Content-Type':'application/json'},body:v}).then(loadPlugins);
}
function uploadPluginJson(){
  var f=document.getElementById('plugin-file').files[0];
  var st=document.getElementById('plugin-upload-status');
  if(!f){if(st)st.textContent=T('请先选择 .json 文件');return;}
  if(st)st.textContent=T('上传中...');
  fetch('/plugins/upload',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:f}).then(function(r){
    return r.text().then(function(t){
      var msg='HTTP '+r.status;
      try{var j=JSON.parse(t);if(j&&j.message)msg=j.message;}catch(e){}
      if(r.ok){if(st)st.textContent=T('安装成功');return loadPlugins();}
      if(st)st.textContent=T('安装失败：')+(f&&f.name?f.name+' ':'')+msg;
    });
  }).catch(function(e){
    if(st)st.textContent=T('安装失败：')+(f&&f.name?f.name+' ':'')+(e&&e.message?e.message:e);
  });
}
function togglePlugin(name,on){return postForm('/plugins/toggle',{name:name,enabled:on?'1':'0'}).then(loadPlugins)}
function setPluginPriority(name,priority){return postForm('/plugins/priority',{name:name,priority:priority}).then(loadPlugins)}
function removePlugin(name){return postForm('/plugins/remove',{name:name}).then(loadPlugins)}
function setPluginReplayCount(count){return postForm('/plugins/replay_count',{count:count}).then(loadPlugins)}
function runPluginRuleTest(){
  return fetch('/plugins/rule_test',{method:'POST'}).then(function(r){return r.text()}).then(function(t){setText('plugin-rule-test-result',t)});
}
function renderPluginsStatus(j){
  var list=document.getElementById('plugin-list'); if(!list)return;
  var html=(j.installed||[]).map(function(p){
    var name=String(p.name||'');
    var ids=(p.targetIds||[]).join(', ');
    var conflicts=(p.conflicts||[]).join(' · ');
    return '<div class="plugin-item">'
      +'<div class="pi-head"><div><span class="pi-name">'+esc(name)+'</span> <small class="pi-ver">'+esc(p.version||'1.0')+'</small></div>'
      +'<div class="pi-controls">'
      +'<label class="tgl" title=T("'+T('启用')+'")><input type="checkbox" '+(p.enabled?'checked':'')+' onchange="togglePlugin(\''+escAttr(name)+'\',this.checked)"><div class="tgl-track"></div></label>'
      +'<input class="mock-input pi-prio" type="number" min="1" max="8" value="'+(p.priority||1)+'" onchange="setPluginPriority(\''+escAttr(name)+'\',this.value)" title=T("'+T('优先级')+'")>'
      +'<button class="ds-btn ds-btn-danger" onclick="removePlugin(\''+escAttr(name)+'\')">'+T('删除')+'</button>'
      +'</div></div>'
      +'<div class="pi-hint">'+T('目标ID: ')+esc(ids)+T(' · 规则: ')+(p.ruleCount||0)+'</div>'
      +(conflicts||p.lastError ? '<div class="pi-warn">'+esc(conflicts||p.lastError)+'</div>' : '')
      +'<details class="collapse pi-details"><summary>'+T('详情')+'</summary><pre>'+esc(JSON.stringify(p,null,2))+'</pre></details>'
      +'</div>';
  }).join('');
  list.innerHTML=html||'<div class="hint">'+T('暂无插件。新安装默认禁用。')+'</div>';
  var rc=document.getElementById('plugin-replay-count'); if(rc)rc.value=String(j.replayCount||1);
  var pill=document.getElementById('plugins-count-pill'); if(pill)pill.textContent=T('已装 ')+((j.installed||[]).length)+T(' 个');
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
  var tgl=$('hw3-slew-tgl');
  // 5.0.0-beta02: postForm 期望对象参数——字符串形式会让 for...in 遍历字符
  // 下标，发出 0=t&1=r… 乱码体，后端 hasArg 永远为 false（开关弹回）。
  try{await postForm('/config',{hw3OffsetSlew:(tgl&&tgl.checked?'1':'0')});}
  catch(e){}
  poll();
}

async function saveDefenseConfig(){
  var master=$('def-master-tgl');
  var sound=$('def-sound-tgl');
  var isaOvr=$('def-isa-override-tgl');
  var dndVol=$('def-dnd-vol-tgl');
  var nd=$('def-speed-nd-tgl');
  var dndSpd=$('def-dnd-spd-tgl');
  var apeap=$('def-apeap-tgl');
  var abortTgl=$('def-abort-guard-tgl');
  var otaOvr=$('def-ota-ovr-tgl');
  var jtTgl=$('def-jitter-tgl');
  var data={
    enabled:master&&master.checked?'1':'0',
    sound_warning_suppression:sound&&sound.checked?'1':'0',
    isa_override:isaOvr&&isaOvr.checked?'1':'0',
    dnd_volume:dndVol&&dndVol.checked?'1':'0',
    speed_no_disturb:nd&&nd.checked?'1':'0',
    ap_eap_compatible:apeap&&apeap.checked?'1':'0',
    abort_guard:abortTgl&&abortTgl.checked?'1':'0',
    ota_guard_override:otaOvr&&otaOvr.checked?'1':'0',
    jitter:jtTgl&&jtTgl.checked?'1':'0'
  };
  if(dndSpd)data.dnd_speed=dndSpd.checked?'1':'0';
  try{await postForm('/defense_config',data);}
  catch(e){loadDefenseConfig()}
  loadDefenseConfig();
}


async function pollCAN2(){
  var d=await fetchJson('/bus2_ids');
  if(!d)return;
  setCls('b2-status','stat-val '+(d.count>0?'v-ok':'v-dim'));
  setText('b2-status',d.count>0?'Online':'Idle');
  setText('b2-rx',d.rx_total||0);
  var c2=d.can2||{};
  setText('b2-tx',c2.tx||0);
  setText('b2-txerr',c2.txerr||0);
  setText('b2-eflg','0x'+toHex(c2.eflg||0,2));
  setCls('b2-txerr','stat-val '+((c2.txerr||0)>0?'v-warn':'v-dim'));
  setCls('b2-eflg','stat-val '+((c2.eflg||0)>0?'v-err':'v-dim'));
  setText('b2-ids',d.count||0);
  setText('b2-count','('+d.count+')');
  setCls('m-can2','stat-val '+(d.count>0?'v-ok':'v-dim'));
  setText('m-can2',d.count>0?'Online':'Idle');
  var sCan=$('s-can');
  setText('ov-can',(sCan&&sCan.textContent==='Online'?'CAN1 Online':'CAN1 Offline')+' / '+(d.count>0?'CAN2 Online':'CAN2 Idle'));
  var svc=$('svc-mode-tgl');
  if(svc)svc.checked=!!d.service_mode;
  setText('svc-mode-status',d.service_mode?T('激活'):T('未激活'));
  setCls('svc-mode-status',d.service_mode?'v-acc':'v-dim');
  var sCan2El=$('s-can2');
  if(sCan2El){sCan2El.className=(sCan2El.classList.contains('val')?'val ':'stat-val ')+(d.count>0?'v-ok':'v-dim');sCan2El.textContent=d.count>0?'Online':'Idle';}
  fitCockpitVals();
  loadBurstConfig();
  setStatusTriplet('light',
    ($('light-enabled-tgl')&&$('light-enabled-tgl').checked)?T('爆闪 ON'):T('爆闪 OFF'),
    T('规则 ')+(lightPreset||3)+'x '+(lightFrequency||'medium'),
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

function detectBurstPreset(d){
  for(var k in BURST_PRESETS){var p=BURST_PRESETS[k];if(p.count===d.count&&p.on_ms===d.on_ms&&p.off_ms===d.off_ms)return k}
  return burstPreset||'B';
}
function updateBurstPresetCards(){
  ['burst-preset'].forEach(function(id){
    var cards=$(id);if(!cards)return;
    var items=cards.querySelectorAll('.sel-card');
    var keys=['A','B','C','D'];
    for(var i=0;i<items.length;i++)items[i].classList.toggle('active',keys[i]===burstPreset);
  });
}
function updateBurstUi(d){
  if(!d)return;
  burstPreset=detectBurstPreset(d);
  updateBurstPresetCards();
  var enabled=!!d.enabled;
  var t=$('burst-enabled-tgl');if(t)t.checked=enabled;
  var running=!!d.running;
  var status=enabled?(running?T('运行中'):T('等待双拨')):T('关闭');
  setText('burst-status',status);
  setText('burst-params',(d.count||0)+'× '+(d.on_ms||0)+'/'+(d.off_ms||0)+'ms');
  setText('burst-phases',d.phases_left||0);
  setCls('burst-status','stat-val '+(running?'v-warn':(enabled?'v-ok':'v-dim')));
  setCls('burst-phases','stat-val '+((d.phases_left||0)>0?'v-warn':'v-dim'));
}
async function loadBurstConfig(){
  var d=await fetchJson('/burst');
  if(d)updateBurstUi(d);
}
async function saveBurstConfig(enabled){
  var p=BURST_PRESETS[burstPreset]||BURST_PRESETS.B;
  try{
    var d=await postForm('/burst',{enabled:enabled?'1':'0',count:String(p.count),on_ms:String(p.on_ms),off_ms:String(p.off_ms)});
    if(d)updateBurstUi(d);
  }catch(e){}
}
function toggleBurst(src){
  var t=$('burst-enabled-tgl');
  var enabled=src?!!src.checked:!!(t&&t.checked);
  if(t)t.checked=enabled;
  saveBurstConfig(enabled);
}
function setBurstPreset(id){
  if(!BURST_PRESETS[id])id='B';
  burstPreset=id;
  updateBurstPresetCards();
  var enabled=!!($('burst-enabled-tgl')&&$('burst-enabled-tgl').checked);
  saveBurstConfig(enabled);
}

async function loadLightingConfig(){
  var d=await fetchJson('/lighting_config');
  if(!d)return;
  lightPreset=d.count||3;
  lightFrequency=d.frequency||'medium';
  updateLightPreset(lightPreset);
  updateLightOptionCards('light-frequency',lightFrequency,['slow','medium','fast']);
  var t=$('light-enabled-tgl');if(t)t.checked=!!d.enabled;
  setStatusTriplet('light',d.enabled?T('爆闪 ON'):T('爆闪 OFF'),
    (d.count||3)+'x '+(d.frequency||'medium'),
    T('等待 CAN2 运行确认'),
    d.enabled?'warn':'ok');
  setText('tb-exp',experimentSummary());
}

async function saveLightingConfig(){
  var t=$('light-enabled-tgl');
  try{await postForm('/lighting_config',{enabled:t&&t.checked?'1':'0',count:String(lightPreset),frequency:lightFrequency});}
  catch(e){}
  loadLightingConfig();
}

function setLightFrequency(freq){
  lightFrequency=freq;
  updateLightOptionCards('light-frequency',freq,['slow','medium','fast']);
  saveLightingConfig();
}

function lightDelayMs(){
  return lightFrequency==='fast'?180:(lightFrequency==='slow'?650:350);
}


async function strobeTest(mode){
  if(!confirm(T('确认执行灯光注入序列？')))return;
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
  if(!skipConfirm&&!confirm(T('确认执行灯光注入？')))return;
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
  if(!confirm(T('确认上传 OTA 固件？上传完成可能会重启设备。')))return;
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
function resetOtaCredentials(){
  localStorage.removeItem('otaU');
  localStorage.removeItem('otaP');
  showToast(T('OTA 凭据缓存已清除'),true);
}
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
  setText('st-ota-pill',d.firmware||'--');
  setText('rel-current',d.firmware||'--');
  var buildLabel=d.uiBuildUtc||d.uiBuildId||d.buildEnv||'--';
  setText('ota-build',buildLabel);
  var appUsed=d.app_used?Math.round(d.app_used/1024)+'KB':'--';
  var appTotal=d.app_size?Math.round(d.app_size/1024)+'KB':'--';
  setText('ota-flash',appUsed+' / '+appTotal);
  setText('ota-sdk',(d.target||'--')+' / '+(d.idf||'--'));
}

var releaseUpdateUrl='';
async function loadOtaReleaseState(){
  var beta=await fetchJson('/update_beta');
  if(beta){
    var bt=$('rel-beta-tgl');
    if(bt)bt.checked=!!beta.beta;
    if(beta.version)setText('rel-current',beta.version);
  }
  var auto=await fetchJson('/auto_update');
  if(auto){
    var at=$('rel-auto-tgl');
    if(at)at.checked=!!auto.enabled;
  }
}
async function checkReleaseUpdate(){
  var btn=$('rel-check-btn');
  var install=$('rel-install-btn');
  if(btn)btn.disabled=true;
  if(install)install.disabled=true;
  releaseUpdateUrl='';
  setText('rel-status',T('检查中...'));
  setText('rel-msg',T('正在连接 GitHub Release，请稍候。'));
  try{
    var r=await fetch('/update_check');
    var txt=await r.text();
    var d={};
    if(txt){try{d=JSON.parse(txt)}catch(e){}}
    if(!r.ok)throw new Error((d&&d.error)?d.error:('HTTP '+r.status));
    setText('rel-current',d.current||'--');
    setText('rel-latest',d.latest||d.tag||'--');
    setText('rel-artifact',d.artifact||'--');
    var msg=T('已是最新版本');
    var cls='v-ok';
    if(d.update&&d.url){
      releaseUpdateUrl=d.url;
      msg=T('发现新版本');
      cls='v-warn';
      if(install)install.disabled=false;
      setText('rel-msg',T('发现 ')+(d.tag||d.latest)+T('，点击“安装更新”后设备会下载并重启。'));
    }else if(!d.url){
      msg=T('未找到匹配固件');
      cls='v-err';
      setText('rel-msg',T('Release 中没有 ')+(d.artifact||T('当前硬件'))+T(' 对应的固件文件。'));
    }else{
      setText('rel-msg',T('当前版本 ')+(d.current||'--')+T(' 已不低于 ')+(d.latest||'--')+'。');
    }
    setText('rel-status',msg);
    setCls('rel-status',cls);
  }catch(e){
    setText('rel-status',T('检查失败'));
    setCls('rel-status','v-err');
    setText('rel-msg',(e&&e.message)?e.message:T('检查失败'));
    showToast((e&&e.message)?e.message:T('检查失败'));
  }finally{
    if(btn)btn.disabled=false;
  }
}
async function installReleaseUpdate(){
  if(!releaseUpdateUrl){showToast(T('请先检查更新'));return}
  if(!confirm(T('确认安装 GitHub Release OTA 固件？设备会下载固件并自动重启。')))return;
  var btn=$('rel-install-btn');
  if(btn)btn.disabled=true;
  setText('rel-status',T('安装中...'));
  setCls('rel-status','v-warn');
  setText('rel-msg',T('设备正在下载并写入 OTA 固件，完成后会自动重启。'));
  try{
    await postForm('/update_install',{url:releaseUpdateUrl});
    setText('rel-status',T('已开始安装'));
    setText('rel-msg',T('OTA 已开始，设备将自动重启。'));
    showToast(T('OTA 已开始，设备将重启'),true);
  }catch(e){
    if(btn)btn.disabled=false;
    setText('rel-status',T('安装失败'));
    setCls('rel-status','v-err');
  }
}
async function toggleUpdateBeta(){
  var t=$('rel-beta-tgl');
  try{
    var d=await postForm('/update_beta',{beta:t&&t.checked?'1':'0'});
    if(d&&d.version)setText('rel-current',d.version);
    releaseUpdateUrl='';
    var install=$('rel-install-btn');
    if(install)install.disabled=true;
    setText('rel-status',T('未检查'));
  }catch(e){if(t)t.checked=!t.checked}
}
async function toggleAutoUpdate(){
  var t=$('rel-auto-tgl');
  try{await postForm('/auto_update',{enabled:t&&t.checked?'1':'0'});}
  catch(e){if(t)t.checked=!t.checked}
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
  setText('st-net-pill', d.connected ? (d.ssid||'--') : (d.connecting ? T('连接中') : T('未配置')));
  setStatusTriplet('net',
    d.connected?('STA '+d.ssid):(d.connecting?T('STA 连接中'):T('STA 未配置')),
    T('等待热点配置读取'),
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
    showToast(T('请输入 SSID'));
    setStatusTriplet('net',T('测试未发送'),T('NVS 未变更'),T('请输入中转 WiFi SSID'),'warn');
    return;
  }
  var data={ssid:ssid?ssid.value:'',pass:pass?pass.value:''};
  try{
    var r=await postForm('/relay_wifi_test',data);
    showToast((r&&r.connected)?'WiFi OK':'WiFi test ready',true);
  }catch(e){}
}

async function saveHotspot(reboot){
  if(reboot&&!confirm(T('确认保存网络配置并重启设备？')))return;
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
  // transient fetch failure: retry once, then surface a visible error instead of a silent empty form
  if(!d){await new Promise(function(resolve){setTimeout(resolve,500)});d=await fetchJson('/dns_rules')}
  if(!d){updateDnsProfileCards('');setStatusTriplet('dns',T('加载失败'),T('点「刷新」重试'),T('规则未读取'),'err');return}
  var bl=$('dns-blacklist');if(bl)bl.value=d.blacklist||'';
  var wl=$('dns-whitelist');if(wl)wl.value=d.whitelist||'';
  setText('dns-bl-cnt',(d.black_count||0)+' '+T('域名'));
  setText('dns-wl-cnt',(d.white_count||0)+' '+T('域名'));
  // Upstream DNS mode
  updateDnsUpstreamCards(d.upstream_mode||0);
  var profile=detectDnsProfile(d.blacklist,d.whitelist);
  updateDnsProfileCards(profile);
  var counts=(d.black_count||0)+T(' 黑 / ')+(d.white_count||0)+T(' 白');
  setStatusTriplet('dns',profile||T('自定义规则'),counts,T('等待 DNS 运行确认'),'ok');
  if(!normLines(d.blacklist)||normLines(d.blacklist)===normLines(OLD_DNS_BLACKLIST)){
    applyDnsProfile('conservative');
    updateDnsProfileCards('conservative');
    setStatusTriplet('dns',T('保守模式'),T('已自动填入 12 条规则'),T('请点击保存规则写入 NVS'),'warn');
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
  setStatusTriplet('dns',profile,countLines(p.blacklist)+T(' 黑 / ')+countLines(p.whitelist)+T(' 白'),T('规则已应用，等待保存'),'warn');
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
  setStatusTriplet('dns',T('规则已保存'),T('已写入 NVS'),T('等待 DNS 运行确认'),'ok');
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
  if(tab==='recorder')pollRecStatus();
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
    var rowCls=(name.toLowerCase().indexOf('err')>=0||idStr.toLowerCase().indexOf('txerr')>=0)?'warn':'';
    var dlc=(fr.dlc!==undefined)?fr.dlc:(fr.data?fr.data.length:'--');
    html+='<div class="diag-priority-row '+rowCls+'"><span class="hex">'+idStr+(name?' <span style="color:var(--tx3);font-size:10px">'+escHtml(name)+'</span>':'')
      +'</span><span>RX</span><span>'+dlc+'</span><span class="hex">'+dataStr+'</span><span>--</span></div>';
  }
  rows.innerHTML=html;
  setText('sniff-count',count+T(' 实时帧'));
  setText('diag-id-count',count||'--');
}

function toggleSniffPause(){
  sniffPaused=!sniffPaused;
  var btn=$('sniff-pause');
  if(btn)btn.textContent=sniffPaused?'▶':'⏸';
}

// Recorder
async function startRec(){
  var duration=$('rec-duration')?$('rec-duration').value:'120';
  var ids=$('rec-ids')?$('rec-ids').value.trim():'';
  try{await postForm('/rec_start',{duration_s:duration,ids:ids});}catch(e){return}
  recActive=true;
  $('rec-start').disabled=true;$('rec-stop').disabled=false;$('rec-dl').disabled=true;$('rec-meta').disabled=true;
  setText('rec-status',T('录制中')||'Recording...');
  pollRecStatus();
}
async function stopRec(){
  setText('rec-status',T('收尾中...')||'Finalizing...');
  try{await postForm('/rec_stop',{});}catch(e){setText('rec-status',T('停止失败')||'Stop failed');return}
  pollRecStatus();
}
function downloadRec(){window.location.href='/rec_download'}
function downloadRecMeta(){window.location.href='/rec_meta'}
function recFmtBytes(n){return n>=1048576?(n/1048576).toFixed(2)+' MB':(n/1024).toFixed(1)+' KB'}
async function pollRecStatus(){
  var d=await fetchJson('/rec_status');if(!d)return;
  recActive=!!d.active;
  var finalizing=!!d.finalizing;
  var elapsed=Math.floor((d.elapsed_ms||0)/1000),duration=Math.floor((d.duration_ms||120000)/1000);
  setText('rec-count',(d.count||0)+T(' 帧'));setText('rec-dropped',d.dropped||0);
  setText('rec-bytes',recFmtBytes(d.file_bytes||0));setText('rec-progress',Math.min(elapsed,duration)+' / '+duration+T(' 秒'));
  setText('rec-3c2-can1',(d.can1_rx_3c2||0)+' / '+(d.can1_tx_3c2||0)+' / '+(d.can1_tx_error_3c2||0));
  setText('rec-3c2-can2',(d.can2_rx_3c2||0)+' / '+(d.can2_tx_3c2||0)+' / '+(d.can2_tx_error_3c2||0));
  setText('rec-3c2-mux1',(d.can2_rx_3c2_mux1||0)+' / '+(d.can2_rx_3c2_mux1_idle||0));
  setText('rec-3c2-volume',(d.can2_rx_3c2_volume_up||0)+' / '+(d.can2_rx_3c2_volume_down||0));
  setText('rec-3c2-tx',(d.can2_tx_3c2||0)+' / '+(d.can2_tx_error_3c2||0));
  setText('rec-045-can1',(d.can1_rx_045||0)+' / '+(d.can1_tx_045||0)+' / '+(d.can1_tx_error_045||0));
  setText('rec-045-can2',(d.can2_rx_045||0)+' / '+(d.can2_tx_045||0)+' / '+(d.can2_tx_error_045||0));
  setText('rec-045-updown',(d.wheel_045_lever_up||0)+' / '+(d.wheel_045_lever_down||0));
  setText('rec-045-fwdback',(d.wheel_045_lever_forward||0)+' / '+(d.wheel_045_lever_back||0));
  setText('rec-045-cond',(d.wheel_045_cond_up||0)+' / '+(d.wheel_045_cond_down||0));
  setText('rec-045-last',(d.wheel_045_last_bus||'--')+' / '+(d.wheel_045_last_counter??'--')+' / '+toHex(d.wheel_045_last_crc||0));
  var status=recActive?T('录制中'):finalizing?T('收尾中'):d.storage_full?T('存储已满'):d.saved?T('已保存'):T('空闲');
  if((d.dropped||0)>0)status+=T(' · 丢帧 ')+d.dropped;
  if(d.stop_reason&&d.stop_reason!=='none')status+=' · '+d.stop_reason;
  setText('rec-status',status);
  $('rec-start').disabled=recActive||finalizing;$('rec-stop').disabled=!recActive;$('rec-dl').disabled=recActive||finalizing||!d.saved;$('rec-meta').disabled=recActive||finalizing||!d.saved;
}

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
  var lwMatchText=($('lw-match')&&$('lw-match').textContent)?$('lw-match').textContent:'--';
  setText('diag-last-write',lwMatchText);
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

// ── Gear assist placeholder telemetry ──────────────────────
async function pollGearAssist(){
  var d=await fetchJson('/gear_assist_status');
  if(!d)return;
  setText('shift-speed',(d.speed_kph!==undefined?d.speed_kph:'--')+' km/h');
  setText('shift-gear',d.gear||'--');
  setText('shift-brake',d.brake?(T('踩下')||'ON'):(d.brake_seen?(T('未踩')||'OFF'):'--'));
  setText('shift-fsd',d.can_online?'CAN Online':'CAN Offline');
  setCls('shift-fsd','v-dim '+(d.can_online?'v-ok':'v-err'));
}

// ── Legacy Real Wiring Truth Cards ────────────────────────
var legacyFsdConfigLoaded=false;
function renderLegacyFsdPolicyCards(policy){
  var cards=document.getElementById('legacy-fsd-policy-cards');
  if(!cards)return;
  var normalized=policy||'stable';
  if(policy==='legacy_stable')normalized='stable';
  if(policy==='legacy_experimental')normalized='experimental';
  var items=cards.querySelectorAll('.sel-card');
  for(var i=0;i<items.length;i++){
    var item=items[i];
    item.classList.toggle('active',item.getAttribute('data-policy')===normalized);
  }
}
async function loadLegacyFsdConfig(){
  var cfg=await fetchJson('/legacy_fsd_config');
  if(!cfg){legacyFsdConfigLoaded=false;return false;}
  if(cfg.policy)renderLegacyFsdPolicyCards(cfg.policy);
  legacyFsdConfigLoaded=true;
  return true;
}
async function setLegacyFsdPolicy(policy){
  renderLegacyFsdPolicyCards(policy);
  await postForm('/legacy_fsd_config',{policy:policy});
  showToast(policy==='stable'?'Legacy Stable':(policy==='legacy_tesla_parity'?T('Tesla Controller 对齐模式'):'Experimental'));
  poll();
}
// 5.0-beta01: the five legacy experimental controls (mux1 / speed profile /
// vision clear / legacy offset / speed-limit override) were removed from the
// overview page; their save/load helpers went with them. Backend endpoints
// and /status JSON keys are unchanged.
function renderLegacyTruth(d){
  const set=(id,v)=>{const e=document.getElementById(id); if(e)e.innerText=v;};
  const fsd=d.fsdDiag||{};
  const m0=fsd.mux0||{};
  const m1=fsd.mux1||{};
  set('legacy-fsd-strategy',fsd.policy==='legacy_tesla_parity'?T('Tesla Controller 对齐模式'):((fsd.policy==='legacy_experimental')?T('实验模式'):T('稳定')));
  renderLegacyFsdPolicyCards(fsd.policy);
  set('legacy-fsd-mux0',`RX ${m0.rx||0} / TX ${m0.tx||0} / ${m0.lastSkip||T('无')}`);
  set('legacy-fsd-mux1',`RX ${m1.rx||0} / TX ${m1.tx||0} / ${m1.lastSkip||T('无')}`);
  set('legacy-fsd-trigger',fsd.triggerSource||T('未知'));
  const svc=d.serviceDiag||{};
  const svctx=svc.tx||{};
  set('service-burst-state',`${svc.lastCommand==='enter'?T('进入'):svc.lastCommand==='exit'?T('退出'):T('无')} / ${T('剩余')} ${svc.remaining||0}`);
  set('service-last-data',svctx.lastData||'--');
  set('service-tx-evidence',`${T('成功')} ${svctx.txOk||0} / ${T('失败')} ${svctx.txErr||0}`);
  const wd=d.wheelDndDiag||{};
  set('wheel-hands-state',wd.dasStatusSeen?`AP${wd.apState??'--'} · ${T('手部')}${wd.handsState??'--'} · ${wd.warningActive?T('告警'):T('等待')} · ${wd.dasStatusDriver||'unknown'}`:T('等待 0x399'));
  set('wheel-native-3c2',wd.native045Seen?`${wd.native045AgeMs} ms · C${Number(wd.nativeCounter||0).toString(16).toUpperCase()}`:T('等待 CAN1 0x045'));
  set('wheel-base-frame',wd.baseFrameMode||T('等待中'));
  set('wheel-sequence-state',`${wd.sequenceState||'idle'} / ${wd.actionsThisEpisode||0} / ${wd.nextDirectionPositive===false?'-1':'+1'}`);
  set('wheel-action-type',`${wd.positiveTx||0} / ${wd.negativeTx||0} / ${wd.directionFlips||0}`);
  set('wheel-counter-state',`${wd.expectedCounterDuplicates||0} / ${wd.counterConflicts||0}`);
  set('wheel-echo-state',`${wd.ownEchoRx||0} / ${wd.gatewayMirrorRx||0}`);
  set('wheel-blocked-reason',wd.blockedReason||'--');
}

// ── Temp from system_status ────────────────────────────────
async function loadTemp(){
  var d=await fetchJson('/system_status');
  if(d&&d.temp_c!==undefined&&d.temp_c!==null){
    var t=parseFloat(d.temp_c);
    var el=document.getElementById('s-temp');
    if(el){
      el.textContent=t+'°C';
      // Color coding: >60°C red, >45°C orange, else green
      el.classList.remove('v-err','v-info','v-acc');
      el.classList.add(t>60?'v-err':(t>45?'v-info':'v-acc'));
    }
  }
  // Firmware version display
  if(d&&(d.firmware||d.version)){
    setText('s-ver',d.firmware||d.version);
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
function toggleMobMore(id){
  var p=$(id||'mob-more');
  if(p)p.classList.toggle('open');
}
function closeStandaloneMobMore(){
  var p=$('mob-more-single');
  if(p)p.classList.remove('open');
}
function updateMobTabs(pageId){
  var map={driving:'pg-overview',hardware:'pg-hardware',speed:'pg-speed',network:'pg-network',drivingstyle:'pg-drive',defense:'pg-defense',ota:'pg-ota',can:'pg-can',more:'pg-overview'};
  var tabs=document.querySelectorAll('.mob-tab[data-mobile-page]');
  for(var i=0;i<tabs.length;i++){
    var mob=tabs[i].getAttribute('data-mobile-page');
    tabs[i].classList.toggle('active',mob===pageId||map[mob]===pageId);
  }
}
function showMobilePage(name){
  var map={driving:'pg-overview',hardware:'pg-hardware',speed:'pg-speed',network:'pg-network',drivingstyle:'pg-drive',defense:'pg-defense',ota:'pg-ota',can:'pg-can'};
  closeStandaloneMobMore();
  showPage(map[name]||'pg-overview');
  updateMobTabs(name);
  if(name==='speed')loadSpeedStrategy();
  if(name==='network')pollWifiStatus();
}
function showStandaloneMorePage(name){
  closeStandaloneMobMore();
  showMobilePage(name);
}

// === Hoist fixed mobile chrome to <body> ===
// The bottom tab bars and "more" panels use position:fixed but are emitted at
// the end of .content (overflow-y:auto scroll container). On mobile browsers
// (notably iOS Safari), a position:fixed element nested inside a scroll
// container fails to repaint during momentum scroll on long pages, so the tab
// bar renders blank/disappears after scrolling (ported from single-CAN
// v1.0.7 hoistMobileChrome fix). Moving them to be direct children of <body>
// anchors position:fixed to the viewport reliably. #mob-tabs carries its own
// data-dual-hide because it no longer sits inside the data-dual-hide wrapper.
function hoistMobileChrome(){
  ['mob-tabs','mob-more-single','mob-tabs-dual','mob-more'].forEach(function(id){
    var el=document.getElementById(id);
    if(el&&el.parentNode!==document.body)document.body.appendChild(el);
  });
}

document.addEventListener('DOMContentLoaded',function(){
  hoistMobileChrome();
  // Desktop sidebar nav
  var navs=document.querySelectorAll('.nav-item');
  for(var i=0;i<navs.length;i++){
    navs[i].addEventListener('click',function(){
      showPage(this.getAttribute('data-page'));
    });
  }

  // Standalone mobile tab bar is wired inline via showMobilePage().
  // Dual-CAN mobile tab bar + "more" menu navigate to desktop pages.
  var mobDual=document.querySelectorAll('#mob-tabs-dual .mob-tab[data-page], #mob-more .mob-more-item[data-page]');
  for(var i=0;i<mobDual.length;i++){
    mobDual[i].addEventListener('click',function(){showPage(this.getAttribute('data-page'));});
  }

  // Initial page
  showMobilePage('driving');

  // OTA drag-drop
  setupOtaDrop();

  // Load initial data
  loadFirmwareInfo();
  loadCanPins();
  loadLegacyFsdConfig();
  loadPlugins();

  // Start polling
  poll();
  pollTick=function(){
    poll();loadTemp();
    var activePage=document.querySelector('.page.active');
    if(activePage){
      var pid=activePage.id;
      if(pid==='pg-can'){if(canTab==='sniffer'&&!sniffPaused)pollSniffer();else if(canTab==='recorder')pollRecStatus();else if(canTab==='debug')pollLastWrite()}
      if((pid==='pg-bus2'||pid==='pg-overview')&&!isSingleCan()&&CAP.can2Available)pollCAN2();
      if(pid==='pg-shift'&&!isSingleCan())pollGearAssist();
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
<!-- Mobile Standalone Pages (single-CAN product) -->
<!-- Mobile Bottom Tab Bar (standalone); data-dual-hide stays on the element
     itself because hoistMobileChrome() re-parents it to <body> at load. -->
<div class="mob-tabs" id="mob-tabs" data-dual-hide="1">
  <button class="mob-tab active" data-mobile-page="driving" onclick="showMobilePage('driving')"><div class="mob-icon">▣</div><div data-i18n="驾驶">驾驶</div></button>
  <button class="mob-tab" data-mobile-page="hardware" onclick="showMobilePage('hardware')"><div class="mob-icon">◇</div><div data-i18n="硬件">硬件</div></button>
  <button class="mob-tab" data-mobile-page="speed" onclick="showMobilePage('speed')"><div class="mob-icon">↗</div><div data-i18n="速度">速度</div></button>
  <button class="mob-tab" data-mobile-page="network" onclick="showMobilePage('network')"><div class="mob-icon">◎</div><div data-i18n="网络">网络</div></button>
  <button class="mob-tab" data-mobile-page="more" onclick="toggleMobMore('mob-more-single')"><div class="mob-icon">···</div><div data-i18n="更多">更多</div></button>
</div>
<div class="mob-more-panel" id="mob-more-single" data-dual-hide="1">
  <div class="mob-more-close" onclick="toggleMobMore('mob-more-single')">✕</div>
  <div class="mob-more-item" onclick="showStandaloneMorePage('drivingstyle')" data-i18n="◉ 驾驶风格">◉ 驾驶风格</div>
  <div class="mob-more-item" onclick="showStandaloneMorePage('defense')" data-i18n="◈ FSD 防护">◈ FSD 防护</div>
  <div class="mob-more-item" onclick="showStandaloneMorePage('ota')" data-i18n="⇧ OTA 升级">⇧ OTA 升级</div>
  <div class="mob-more-item" onclick="showStandaloneMorePage('can')" data-i18n="⌘ CAN 诊断">⌘ CAN 诊断</div>
</div>
<!-- Mobile Dual-CAN Nav (hidden in standalone) -->
<div class="mob-tabs" id="mob-tabs-dual" data-single-hide="1">
  <div class="mob-tab active" data-page="pg-overview"><div class="mob-icon">▣</div><div data-i18n="状态">状态</div></div>
  <div class="mob-tab" data-page="pg-drive"><div class="mob-icon">◉</div><div data-i18n="模式">模式</div></div>
  <div class="mob-tab" data-page="pg-speed"><div class="mob-icon">↗</div><div data-i18n="速度">速度</div></div>
  <div class="mob-tab" data-page="pg-network"><div class="mob-icon">◎</div><div data-i18n="网络">网络</div></div>
  <div class="mob-tab" onclick="toggleMobMore()"><div class="mob-icon">···</div><div data-i18n="更多">更多</div></div>
</div>
<div class="mob-more-panel" id="mob-more" data-single-hide="1">
  <div class="mob-more-close" onclick="toggleMobMore()">✕</div>
  <div class="mob-more-item" data-page="pg-overview" data-mob="1" data-i18n="▣ 状态">▣ 状态</div>
  <div class="mob-more-item" data-page="pg-hardware" data-mob="1" data-i18n="◇ 硬件模式">◇ 硬件模式</div>
  <div class="mob-more-item" data-page="pg-drive" data-mob="1" data-i18n="◉ 驾驶风格">◉ 驾驶风格</div>
  <div class="mob-more-item" data-page="pg-speed" data-mob="1" data-i18n="↗ 速度策略">↗ 速度策略</div>
  <div class="mob-more-item" data-page="pg-ota" data-mob="1" data-i18n="⇧ OTA 升级">⇧ OTA 升级</div>
  <div class="mob-more-item" data-page="pg-bus2" data-cap="can2" data-single-hide="1" data-i18n="✦ CAN2 控制">✦ CAN2 控制</div>
  <div class="mob-more-item" data-page="pg-network" data-mob="1" data-i18n="◎ 网络设置">◎ 网络设置</div>
  <div class="mob-more-item" data-page="pg-defense" data-mob="1" data-i18n="◈ FSD 防护">◈ FSD 防护</div>
  <div class="mob-more-item" data-page="pg-can" data-mob="1" data-i18n="⌘ CAN 诊断">⌘ CAN 诊断</div>
  <div class="mob-more-item" data-page="pg-shift" data-cap="shift" data-single-hide="1" data-i18n="⚙ 自动换挡">⚙ 自动换挡</div>
</div>
</body>
</html>)HTML";
