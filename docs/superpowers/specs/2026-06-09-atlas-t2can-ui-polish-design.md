# Atlas T-2CAN UI Polish Design

Date: 2026-06-09
Project: LILYGO T-2CAN firmware dashboard
Scope: Visual and interaction polish for the already implemented Atlas dashboard redesign.

## Goal

Bring the current production dashboard closer to the approved high-fidelity design at `designs/atlas-t2can-ui-redesign/Atlas T-2CAN UI Redesign.html` without replacing the production information architecture.

The implementation should feel more like a car cockpit, a native mobile remote, and a professional CAN engineering console while preserving existing firmware behavior.

## Approved approach

Use the **recommended enhancement** route:

- Preserve the existing 11 production navigation entries.
- Preserve existing DOM ids, JavaScript functions, backend API calls, NVS semantics, and test contracts.
- Improve the visual structure of the three approved surfaces:
  - `驾驶状态` / cockpit home
  - mobile `现场遥控`
  - `CAN 诊断` engineering mode
- Keep the default visible UI in precise Chinese.
- Keep standard technical acronyms in English: FSD, CAN, CAN1, CAN2, HW, NVS, OTA, TX, RX, EFLG, DBC, GPIO.

## Non-goals

- Do not replace the production UI with the design prototype's three-screen demo shell.
- Do not remove existing production pages or controls.
- Do not add or change CAN injection behavior.
- Do not add backend routes.
- Do not change firmware persistence semantics.
- Do not make dangerous controls easier to trigger.

## Current gaps

### Cockpit home

The current implementation already has a large FSD state and cockpit-oriented markup, but it still reads partly like a dense admin dashboard. The biggest gaps are:

- Weak vehicle/cockpit visual on the right side.
- Background and card layers are flatter than the design prototype.
- Primary FSD control can feel like a normal button rather than the dominant safety action.
- Status cards are useful but need stronger grouping and more deliberate spacing.

### Mobile remote

The current 390px layout is functionally correct, but it can look more like a native mobile app:

- Primary remote card needs stronger gradient, shadow, rounded corners, and state hierarchy.
- Bottom navigation should feel deliberate and app-like while retaining existing production routes.
- The home surface should stay low-density; dangerous actions remain outside first-touch surfaces.
- Spacing should support one-handed use and avoid compressed desktop-card feeling.

### CAN diagnostics

The current page has KPI rail, filter, tabs, and live frame table. It needs stronger engineering-console polish:

- KPI rail should feel like a compact diagnostic instrument panel.
- Live frame table should expose a clearer `DLC` / data-event hierarchy where feasible without breaking current data rendering.
- Error-priority rows and recent-write states need more visual emphasis.
- Timeline/discovered-ID style summaries can be added as frontend-only presentation if sourced from existing data or static placeholders that do not imply unsupported behavior.

## Design details

### 1. Shared design tokens

Add focused dashboard polish tokens on top of existing variables:

- Deeper cockpit background gradient.
- Softer glass panels with translucent fills.
- Stronger panel border and glow tokens.
- Dedicated positive/warning/danger glow tokens.
- Larger cockpit radii and card shadows.

These tokens should be implemented in `include/web/mcp2515_dashboard_ui.src.h` only and regenerated into `include/web/mcp2515_dashboard_ui.h`.

### 2. Cockpit home refinements

Enhance `pg-overview` while keeping the existing ids:

- Add a cockpit shell/background treatment around the home content.
- Make the FSD state panel more immersive with large typography, glow, and a stronger safety button.
- Add a right-side vehicle visual area using lightweight CSS/SVG-like HTML, not heavy external assets.
- Show a simplified lane/vehicle silhouette and three key status tiles: drive profile, protection state, and temperature/health.
- Keep existing production metrics visible but visually group secondary details below the primary state.
- Keep `id="fsd-toggle"`, `id="fsd-label"`, `id="ov-master-tgl"`, `id="ov-fsd-tgl"`, `id="fsd-boot-tgl"`, `id="s-can"`, `id="s-can2"`, `id="s-fps"`, `id="s-temp"`, `id="s-ver"`, `id="ov-hw"`, `id="m-drive"`, `id="m-speed"`, and `id="m-defense"` intact.

### 3. Mobile remote refinements

Enhance the mobile-only home surface:

- Keep `mobile-remote` as the mobile-first surface.
- Apply a native-app card treatment: light surface, large gradient primary card, large OFF/ON state, and one obvious long-press action.
- Keep the bottom tab bar but tune icon size, labels, active color, spacing, and safe-area padding.
- Ensure first screen shows only status and primary control; destructive operations remain in secondary/more pages.
- Preserve existing ids: `m-fsd-tgl`, `m-can`, `m-can2`, `m-fsd`, `m-fps`, `m-alert`, `m-hw`, `m-drive`, `m-speed`, `m-defense`.

### 4. CAN diagnostics refinements

Enhance `pg-can` without removing existing tabs or controls:

- Strengthen `diag-shell`, `diag-kpi-rail`, and `diag-main` styling.
- Add clearer section labels for `工程模式`, `错误优先`, `帧筛选`, `实时帧`, `记录器`, `控制器`, and `调试`.
- Improve table readability with fixed-width ID/DIR/DLC/AGE style presentation where the current data model allows it.
- Add visual emphasis for warning/error rows and recent write mismatch/match states.
- Keep existing ids: `sniff-filter`, `sniff-pause`, `sniff-rows`, `can-recorder`, `can-controller`, `can-debug`, `ctrl-eflg`, `ctrl-rxerr`, `ctrl-txerr`, `lw-injected`, `lw-match`.

### 5. JavaScript behavior

Keep backend calls unchanged. Allowed frontend-only changes:

- Update visual classes for ON/OFF state.
- Keep cockpit and mobile state labels synchronized.
- Keep drive profile label synchronized to cockpit vehicle tiles and mobile status.
- Avoid new confirmation behavior unless it preserves or strengthens current dangerous-action safety.

## Testing and verification

Required verification after implementation:

1. Contract tests for dashboard UI tokens and preserved ids.
2. Dashboard minification check.
3. Native dashboard tests.
4. Firmware build.
5. Browser simulator screenshots for:
   - desktop cockpit home
   - 390px mobile remote
   - desktop CAN diagnostics
6. Browser console check. Known simulator-only missing endpoints should be fixed in the simulator when practical, or reported separately if unrelated.

## Success criteria

The work is complete when:

- The three key surfaces visibly move closer to the design prototype while staying production-safe.
- Existing firmware UI contracts pass.
- Generated dashboard header is regenerated from the source header.
- No backend or CAN behavior changes are introduced.
- Screenshots show improved cockpit depth, mobile app feel, and CAN engineering-console hierarchy.
