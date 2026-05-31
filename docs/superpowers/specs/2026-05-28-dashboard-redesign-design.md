# T-2CAN Dashboard Redesign Spec

**Date:** 2026-05-28
**Scope:** Frontend-only rewrite of `mcp2515_dashboard_ui.src.h`; backend `mcp2515_dashboard.h` unchanged.

## Design Decisions

| Decision | Choice |
|----------|--------|
| Goal | Adopt reference UI visual style, keep T-2CAN actual features only |
| Navigation | Left sidebar (160px), 9 pages |
| Color scheme | Purple modern (retain current palette) |
| Module split | 9 pages, fine-grained |
| Implementation | Single HTML file rewrite (Approach A) |
| Backend API | Zero changes |

## Color Palette

| Token | Hex | Usage |
|-------|-----|-------|
| `--sidebar-bg` | `#111827` | Sidebar background |
| `--main-bg` | `#1f2937` | Main content background |
| `--card-bg` | `#374151` | Cards and sections |
| `--card-bg-alt` | `#1f2937` | Nested elements inside cards |
| `--accent` | `#7c3aed` | Active sidebar item, selected cards, buttons |
| `--accent-light` | `#a78bfa` | Accents, badges, CAN IDs |
| `--ok` | `#4ade80` | Online, active, success |
| `--err` | `#f87171` | Offline, error, danger |
| `--warn` | `#fbbf24` | Warnings, pending |
| `--info` | `#60a5fa` | Info, secondary data |
| `--tx1` | `#f9fafb` | Primary text |
| `--tx2` | `#d1d5db` | Secondary text |
| `--tx3` | `#9ca3af` | Tertiary text |
| `--border` | `#4b5563` | Borders |
| `--header-bg` | `#111827` | Top status bar |

## Layout Structure

```
┌─────────────┬──────────────────────────────────────┐
│  Sidebar     │  Top Status Bar (full width)         │
│  160px fixed │  CAN FPS | Status Badges | Uptime    │
│              ├──────────────────────────────────────┤
│  Atlas       │                                      │
│  T-2CAN      │  Page Content Area                   │
│  控制面板    │                                      │
│  ─────────   │                                      │
│  概览        │  (scrollable, one page visible)       │
│  模块配置    │                                      │
│  FSD 开关    │                                      │
│  速度偏移    │                                      │
│  Bus2 控制   │                                      │
│  FSD 防御    │                                      │
│  OTA 升级    │                                      │
│  网络设置    │                                      │
│  CAN 工具    │                                      │
│              │                                      │
│  ─────────   │                                      │
│  中/EN | ☀   │                                      │
└─────────────┴──────────────────────────────────────┘
```

- Sidebar: fixed position, full height, scrollable if needed on small screens
- Top bar: sticky, shows critical status regardless of current page
- Content area: scrollable, renders one of 9 pages based on sidebar selection

## 9 Pages

### Page 1: Overview (`pg-overview`)

Purpose: At-a-glance system health and FSD quick toggle.

| Element | Description |
|---------|-------------|
| FSD toggle card | Large card with toggle switch, shows ON/OFF status with color |
| Stats grid (2x3) | CAN Bus status, RX count, TX count, Frame rate, HW version, Speed offset |
| Chip temperature | ESP32 die temperature readout |
| Uptime | Device uptime (shown in top bar, duplicated here) |

Data source: `/status` poll (existing `handleStatus()`).

### Page 2: Hardware Config (`pg-hardware`)

Purpose: Select autopilot hardware generation and speed profile.

| Element | Description |
|---------|-------------|
| HW version cards (2x2) | Auto (recommended, highlighted), Legacy, HW3, HW4. Selected = purple border + accent bg |
| Profile selection grid | Depends on selected HW. 3-column cards showing profile names |

Data source: `/status` (hw, profile), `/config` endpoints.

### Page 3: FSD Switch (`pg-fsd`)

Purpose: Master FSD injection control.

| Element | Description |
|---------|-------------|
| Big toggle card | Visual switch with ON/OFF state, colored indicator (green=ON, red=OFF) |
| Boot injection toggle | "Enable on boot" checkbox, maps to `DASH_INJECTION_ON_BOOT` behavior |
| Status text | Current injection state description |

Data source: `/status` (canActive), `/config` for boot setting.

### Page 4: Speed Offset (`pg-speed`)

Purpose: Configure custom speed limit mapping.

| Element | Description |
|---------|-------------|
| HW3 speed toggle | Enable/disable HW3 custom speed |
| Speed bucket table | Grid: actual speed → target speed (30→45, 40→60, etc.) with editable inputs |
| Custom speed toggle | Enable custom bucket values |
| High-speed table | 80→90, 100→110, 120→130 with editable inputs |
| Encoding selector | Speed encoding method dropdown |
| Realtime stats row | Fused value, Stock offset, Raw write target |

Data source: `/status` (speedOffset, fused, etc.), `/config` endpoints for save.

### Page 5: Bus2 Control (`pg-bus2`)

Purpose: Monitor and interact with the secondary CAN bus (MCP2515).

| Element | Description |
|---------|-------------|
| Bus2 ID table | Scrollable table: ID (hex), DLC, Data (hex), Count. Auto-refreshed |
| Stalk injection test | Two buttons: PULL (flash) and PUSH (high beam), with duration control |
| Service Mode toggle | Enable/disable 0x339 continuous injection on bus2 |
| Status indicator | Shows stalk test countdown or idle state |

Data source: `/status` (b2 object), `/bus2_ids`, `/stalk_test`, `/service_mode`.

### Page 6: FSD Defense (`pg-defense`)

Purpose: Configure HW3 offset slew protection.

| Element | Description |
|---------|-------------|
| Slew toggle | Enable/disable slew rate limiting |
| Config table | Max descent rate, minimum offset hold values |
| Trigger counter | Number of times protection has activated |
| Status indicator | Green dot + "Protection active" or dim "Inactive" |

Data source: `/status` (slew data), `/config` for save.

### Page 7: OTA Update (`pg-ota`)

Purpose: Firmware upload and version info.

| Element | Description |
|---------|-------------|
| Version info card | Current version, build timestamp, flash usage |
| Upload area | Drag-drop or click-to-select .bin file |
| Upload button | Triggers OTA flash |
| Progress bar | Shown during upload |

Data source: `/status` (fw version), OTA upload endpoint.

### Page 8: Network Settings (`pg-network`)

Purpose: WiFi, gateway, and DNS filtering configuration.

| Section | Elements |
|---------|----------|
| WiFi Hotspot | SSID, password, hidden toggle, connected clients count |
| WiFi Internet | Up to 4 saved networks, connection status |
| STA-AP Gateway | NAT toggle, diagnostics grid (AP/STA/NAT/Radio/DNS/DNS Slow/Pending/Upstream/Clients), network performance mode toggle |
| Upstream DNS | Auto / 223.5.5.5 (Ali) / 119.29.29.29 (Tencent) / Custom input. Conservative/Aggressive mode profiles |
| DNS Filter Rules | Blacklist textarea, whitelist textarea, domain counts, DNS test input, filter record list with refresh/clear buttons |

Data source: `/status` (network), `/config` endpoints, gateway DNS endpoints.

### Page 9: CAN Tools (`pg-can`)

Purpose: CAN bus diagnostics and raw frame tools.

| Section | Elements |
|---------|----------|
| CAN Pin Config | Bus1 TX/RX (TWAI), Bus2 CS/SCK/MISO/MOSI/RST (MCP2515 SPI). Display current GPIO assignments |
| Sub-tabs | Sniffer, Recorder, Controller, Debug (tab switch within page) |
| Sniffer | Live frame table (ID, Dir, Data, Age), filter input, pause button, frame count |
| Recorder | Start/stop recording, frame limit, download CSV button, record status |
| Controller | CAN controller health, error flags, RX/TX/error counters per mux |
| Debug | Debug log toggle, last write check (compare injected vs bus frame) |

Data source: `/status`, `/sniff`, `/record`, `/can_controller`, `/last_write` endpoints.

## Sidebar Header

- **Title**: "Atlas" (line 1, bold, `--tx1`) + "T-2CAN 控制面板" (line 2, small, `--tx3`)
- Separated from nav items by a thin border

## Sidebar Behavior

- **Active item**: Purple background (`#7c3aed`), white text
- **Inactive item**: Transparent background, gray text (`#9ca3af`)
- **Hover**: Light gray background (`#1f2937`)
- **Bottom section**: Language toggle (中/EN) and theme toggle (dark/light)
- **Mobile (< 768px)**: Sidebar collapses to hamburger menu, overlay on tap

## Top Status Bar

Visible on all pages, contains:

| Element | Color source |
|---------|-------------|
| CAN FPS | `--info` if > 0, `--tx3` if 0 |
| Connection badge | `--ok` (Connected) / `--err` (Offline) |
| FSD status badge | `--ok` (Active) / `--warn` (Standby) / `--err` (OFF) |
| Uptime | `--tx3` |

## I18N

Default language: Chinese. Toggle via sidebar button.

All visible strings go through the existing `T()` function with `I18N_ZH` / `I18N_EN` maps. Add ~40 new entries for sidebar labels and page titles.

## Implementation Constraints

1. **Single file**: All HTML/CSS/JS in `mcp2515_dashboard_ui.src.h` inside `R"HTML(...)HTML";`
2. **No backend changes**: All `/status`, `/config`, `/sniff`, etc. endpoints remain as-is
3. **Build pipeline**: `minify_dashboard.py` processes `.src.h` → `.h` (minified + gzipped)
4. **Flash budget**: 16MB flash, current dashboard ~30KB gzipped. New design estimated ~45-55KB gzipped (well within budget)
5. **Poll interval**: Same as current (1s for status, 0.5s for sniffer when active)
6. **Browser target**: Tesla MCU browser (Chromium-based, limited), mobile Safari, Chrome

## Files Changed

| File | Action |
|------|--------|
| `include/web/mcp2515_dashboard_ui.src.h` | Full rewrite (new HTML/CSS/JS) |
| `include/web/mcp2515_dashboard_ui.h` | Auto-generated by minify script |

**No other files touched.** Backend (`mcp2515_dashboard.h`), main loop (`main.cpp`), and build config (`platformio.ini`) remain unchanged.

## Verification

1. `pio run -e lilygo_t2can_dual` → SUCCESS
2. `pio test -e native` → all tests pass
3. Flash to device, connect to AP at `http://100.100.1.1`
4. Verify:
   - All 9 sidebar pages render and navigate correctly
   - FSD toggle works (ON/OFF reflected in status)
   - Speed offset configuration saves and applies
   - Bus2 ID table populates with live data
   - OTA upload completes successfully
   - DNS blacklist/whitelist saves and filters correctly
   - CAN sniffer shows live frames
   - Language toggle (中/EN) works across all pages
   - Mobile responsive (sidebar collapses on narrow screens)
