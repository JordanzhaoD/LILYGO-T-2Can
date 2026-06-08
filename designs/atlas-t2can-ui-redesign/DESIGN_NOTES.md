# Atlas T-2CAN UI Redesign Concept

## Direction

Jordan approved a hybrid direction:

- **A — vehicle cockpit homepage** for the car/tablet view
- **C — mobile command remote** for the phone view
- **B — mission-control density** for engineering diagnostics

The recommended route is **driving-first**: users should see only the most important driving and safety state on the home screens, then drill into dense engineering pages when debugging.

## High-fidelity prototype

Open the prototype here when the local design server is running:

```text
http://localhost:4311/atlas-t2can-ui-redesign/Atlas%20T-2CAN%20UI%20Redesign.html
```

File:

```text
designs/atlas-t2can-ui-redesign/Atlas T-2CAN UI Redesign.html
```

## Screens included

1. **车机首页 / Cockpit Home**
   - Large FSD injection state
   - Hold-to-enable primary control
   - Permanent CAN/FSD defense chips
   - Hardware, drive mode, speed mode, runtime summary
   - Vehicle-surface card for car-like cockpit feel

2. **手机首页 / Remote Card**
   - Native-app-style card layout
   - One primary action
   - Fewer metrics than desktop
   - Bottom navigation: Home / Drive / Speed / Network / More
   - Dangerous operations moved out of the home surface

3. **CAN 诊断页 / Engineering Mode**
   - Higher density retained intentionally
   - KPI rail, filter, segmented tabs, error-first frame table
   - Interactive frame filter in the prototype
   - Makes dense engineering data feel deliberate instead of cluttered

## Implementation translation notes

If this concept is later implemented in `include/web/mcp2515_dashboard_ui.src.h`:

- Keep current backend APIs unchanged in the first pass.
- Rework the overview page into a true cockpit/status center.
- Treat mobile as a separate UX mode through responsive CSS, not just compressed desktop cards.
- Keep dense tables on CAN/CAN2/Debug pages only.
- Use explicit danger hierarchy for FSD injection, reboot, burst/strobe, and service mode.
- Preserve existing 11-page navigation but group less-used/experimental features behind clearer hierarchy.

## Verification performed

- Served over HTTP with `python3 -m http.server 4311 --directory designs`.
- Loaded in Playwright at the served URL.
- Checked console errors after fixing favicon: 0 errors, 0 warnings.
- Verified nav between cockpit/mobile/diagnostics.
- Verified FSD state toggle updates both cockpit and phone mockups.
- Verified CAN diagnostics filter returns the expected `TXERR` row.
- Verified 390px mobile viewport has no horizontal overflow.
