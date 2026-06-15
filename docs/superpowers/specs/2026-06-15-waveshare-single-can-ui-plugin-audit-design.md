# Waveshare Single CAN UI, Official Plugins, and Audit Fixes Design

Date: 2026-06-15
Branch: `feature/waveshare-single-can-standalone`
Target env: `waveshare_single_can_standalone`

## 1. Purpose

The first Waveshare single-CAN standalone pass created a working isolated build, backend capability flags, route pruning, AP gate timing parity, docs, verification, and firmware assets. A follow-up pass is required because the UI and plugin experience still reflect too much of the older T-2CAN / dual-CAN product surface.

This design turns the branch into a coherent Waveshare single-CAN product without breaking the dual-CAN main project.

Goals:

1. Add AP Gate, delayed injection, AP injection state, and `AP-AUTO-RESTORE` controls to the driving-status UI.
2. Align AP timing and diagnostics with upstream `ev-open-can-tools v3.0.2-beta.3` safety behavior.
3. Add an upstream-compatible official JSON plugin manager rather than fixed hard-coded plugin cards.
4. Implement GTW2047 (`0x7FF`) plugin replay count according to upstream semantics.
5. Remove visible CAN2, T-2CAN, service/stalk/lighting, and Auto Shift product surfaces from the single-CAN UI.
6. Bring the mobile UI into feature parity with the vehicle/head-unit UI.
7. Audit the 10 completed tasks and fix issues found, while preserving dual-CAN behavior.

Non-goals:

- Do not remove dual-CAN source code.
- Do not change `lilygo_t2can_dual` default behavior.
- Do not claim full Tesla 2026.8.3.6 compatibility across all vehicles.
- Do not implement proprietary GTW SecurityAccess seed-to-key logic unless a separate authorized custom-key implementation is provided.
- Do not flash hardware, push, or merge without Jordan's explicit later request.

## 2. Product boundary

The follow-up is scoped to the Waveshare single-CAN standalone product profile.

Primary isolation mechanisms:

- Compile-time flag: `DASH_SINGLE_CAN_STANDALONE`
- Product env: `waveshare_single_can_standalone`
- Runtime capability source: `/status.capabilities`
- New plugin capability/status fields exposed through plugin APIs
- Contract tests that separately verify standalone and dual-CAN behavior

The dual-CAN project may keep CAN2, Bus2, Service Mode, Stalk Test, lighting, strobe, flash burst, and T-2CAN product language. The single-CAN UI must not show those surfaces.

## 3. Single-CAN UI information architecture

The UI keeps the existing Atlas visual style but changes the single-CAN product structure.

Recommended visible top-level modules for standalone:

1. Overview
2. Driving Status
3. Hardware Mode
4. Network Settings
5. OTA Update

Removed from standalone visible navigation:

- CAN2 Control
- Lighting / Fog / Strobe
- Service Mode
- Stalk Test
- Flash Burst / Burst
- Auto Shift

### 3.1 Product naming

Standalone UI should use one consistent product name:

- Preferred: `Atlas Single CAN`
- Acceptable secondary hardware descriptor: `Waveshare Single CAN`

Standalone visible UI must not display:

- `T-2CAN`
- `Atlas T-2CAN`
- `T-2CAN 控制面板`

Dual-CAN UI may keep existing names.

### 3.2 Overview page

Standalone overview should show:

- Device online state
- Single CAN state
- FSD state
- AP injection summary
- Network / OTA summary

Standalone overview must not show:

- CAN2 state
- CAN2 RX / TX / TXErr / EFLG
- CAN2 ID list
- CAN1 Online / CAN2 combined wording

Replace those with single-CAN wording:

- `Single CAN 状态`
- `CAN RX`
- `CAN TX`
- `CAN Error`
- `CAN Filter`

### 3.3 Driving Status page

Driving Status becomes the main safety and runtime page.

Required sections:

1. Core switches
   - FSD activation
   - AP Gate
   - Delayed injection
   - AP-AUTO-RESTORE
2. AP injection status
   - Waiting for AP
   - AP settling
   - Injecting
   - Blocked
3. Speed strategy
4. Driving style
5. Single-CAN diagnostics

The page must not show CAN2 state, Service Mode, Stalk Test, lighting, or Auto Shift controls.

### 3.4 Hardware Mode page

Hardware Mode keeps:

- Auto
- Legacy
- HW3
- HW4

Below hardware mode, add the official plugin manager. Hardware mode is where users choose the vehicle/hardware interpretation; plugins are runtime extensions that may be compatible or incompatible with that mode.

### 3.5 Mobile UI

The mobile UI must be updated, not left as old project residue.

Recommended mobile navigation:

1. Home
2. Driving
3. Plugins
4. Network
5. More, only if needed

Mobile Home:

- Single CAN state
- FSD state
- AP injection state
- Network / OTA summary

Mobile Driving:

- FSD activation
- AP Gate
- delayed injection
- AP-AUTO-RESTORE
- Waiting for AP / settling / injecting / blocked

Mobile Plugins:

- Install from URL
- Upload `.json`
- Paste JSON
- Installed plugin list
- Enable / disable
- Priority
- Detail view
- GTW2047 replay count

Mobile UI must not show:

- CAN2 Control
- Lighting / Strobe
- Auto Shift
- T-2CAN product copy

## 4. AP Gate, delayed injection, and AP-AUTO-RESTORE

The AP controls must be visible in Driving Status because they affect real-time injection safety.

### 4.1 Controls

Required controls:

| Control | Meaning | Default |
| --- | --- | --- |
| FSD activation | Master FSD activation capability | Keep current safe default |
| AP Gate | Require AP conditions before injection | On for standalone unless existing config says otherwise |
| Delayed injection | Require AP stable time before Legacy injection | On |
| Delay duration | AP stable time threshold | 2000 ms |
| AP-AUTO-RESTORE | Restore injection eligibility after AP returns and stabilizes | On, but never bypasses gates |

The existing `/config` `apg` path should remain for AP Gate to avoid breaking existing config behavior.

New or formalized persisted config:

- `ap_delay_ms`, default `2000`
- `ap_auto_restore`, boolean

Delay may be exposed in the UI as fixed safe presets:

- `0 ms`
- `1000 ms`
- `2000 ms`
- `3000 ms`

Default remains `2000 ms` to match upstream beta.3 timing intent.

### 4.2 AP injection state model

The UI should derive a single human-readable AP injection state from `/status` and `/status.fsdDiag.gate`.

States:

1. `等待 AP`
   - AP Gate is enabled.
   - AP active is false.
   - Legacy `0x3EE mux0` injection is not allowed.

2. `稳定计时中`
   - AP active is true.
   - `apStableMs < requiredStableMs`.
   - UI should show progress, e.g. `1260 / 2000 ms`.

3. `正在注入`
   - AP active is true.
   - `apStableMs >= requiredStableMs`.
   - CAN active, OTA guard, compile gates, and FSD/plugin gates allow injection.
   - `legacyFsdAllowed` or plugin injection eligibility is true.

4. `已阻断`
   - Any safety gate blocks injection.
   - UI shows `lastBlockedBy` / `gateReason`.

Relevant status fields already exist or should be formalized:

- `apGate`
- `apGateEnabled`
- `apGateOpen`
- `apAutoRestore`
- `lastInjectMs`
- `fsdDiag.gate.apActive`
- `fsdDiag.gate.apStableMs`
- `fsdDiag.gate.requiredStableMs`
- `fsdDiag.gate.legacyFsdAllowed`
- `fsdDiag.gate.lastBlockedBy`
- `fsdDiag.gate.gateReason`
- `fsdDiag.gate.blockedFrame`

### 4.3 Safety rules

- Legacy `0x3EE mux0` requires AP active for the configured stable delay when AP Gate is enabled.
- AP disengage resets the stable timer.
- Unknown, invalid, and SNA gear fail closed.
- Parked and Summoning do not bypass the Legacy mux0 settle gate.
- AP-AUTO-RESTORE does not bypass AP Gate, delayed injection, OTA guard, canActive, compile gates, or FSD/plugin enable state.
- UI wording must say this aligns with upstream beta.3 safety timing; it must not promise universal Tesla firmware compatibility.

## 5. Official JSON plugin manager

The standalone plugin feature must align with upstream `ev-open-can-tools` official plugin design. It is not limited to three FSD plugin cards.

Correct concept:

```text
Plugin system = runtime JSON CAN rule system
Legacy/HW3/HW4 FSD Activation = optional built-in preset plugins / examples
```

### 5.1 Dashboard workflow

The plugin card supports:

- Install from URL
- Upload `.json`
- Paste JSON offline
- Plugin Editor
- Rule Test
- Installed plugin list
- Enable / Disable
- Priority
- Remove
- Detail view
- GTW2047 replay count

New installs must start disabled. Users review details, conflicts, and priority before enabling.

### 5.2 Plugin storage

Plugins persist across reboots.

Preferred storage: SPIFFS, matching upstream docs.

If the current branch lacks initialized SPIFFS support, implementation must either:

1. Add SPIFFS initialization in a contained way for standalone/plugin builds, or
2. Document and implement a constrained fallback only if SPIFFS is not feasible.

The design preference is SPIFFS because upstream plugin docs state plugins are stored on SPIFFS.

### 5.3 Plugin JSON format

Top-level fields:

| Field | Required | Description |
| --- | --- | --- |
| `name` | yes | Plugin name, max 31 chars. Installing the same name replaces the old plugin. |
| `version` | no | Version string, default `1.0`. |
| `author` | no | Author displayed in dashboard. |
| `rules` | yes | Non-empty array of CAN rule objects. |

`name` is the user-facing identifier. The firmware may derive an internal safe filename/id, but replacement and UI identity follow `name`.

Rule fields:

| Field | Required | Description |
| --- | --- | --- |
| `id` | yes | CAN frame ID, decimal. Example: `2047` for `0x7FF`. |
| `bus` | no | `CH`, `VEH`, `PARTY`, comma-separated string, bitmask, or array. |
| `mux` | no | Byte-0 mux. `-1` or omitted matches any mux. |
| `mux_mask` / `muxMask` | no | Mask for mux matching. |
| `match_byte` / `matchByte` | no | Extra byte index match. |
| `match_mask` / `matchMask` | no | Extra match mask. |
| `match_val` / `match_value` / `matchValue` | no | Extra match value. |
| `match` | no | Object form: `{ "byte": 4, "mask": 192, "val": 0 }`. |
| `ops` | yes | Ordered operations. |
| `send` | no | Include composed output frame on CAN. Defaults true. |

Supported operations for this pass:

- `set_bit`
- `set_byte`
- `or_byte`
- `and_byte`
- `counter`
- `checksum`
- `emit_periodic`

Unknown fields should be preserved where possible but ignored by the execution engine. Unknown operation types should make the plugin invalid or incompatible, not crash the firmware.

### 5.4 Resource limits

Use upstream limits:

| Resource | Limit |
| --- | --- |
| Max installed plugins | 8 |
| Max rules per plugin | 16 |
| Max operations per rule | 16 |
| Max filter IDs per plugin | 32 |

Additional local limits should be explicit:

- Max plugin JSON byte size
- Max URL length
- Max paste/install request size

### 5.5 Installed plugin list

Each installed plugin displays:

- Name
- Version
- Author
- Enabled state
- Priority number
- Rule count
- Target CAN IDs in decimal and hex
- Bus / mux compatibility summary
- Conflict warnings
- Last error
- Actions: detail, load in editor, remove

Install does not imply enable.

### 5.6 Priority and conflict behavior

Default priority is install order. `#1` has highest priority.

When multiple enabled plugins match the same incoming CAN ID, bus, and mux, the firmware composes one output frame.

If two enabled plugins write the same bit in the same frame cycle:

- Higher-priority plugin owns the bit.
- Lower-priority plugin cannot overwrite that bit.
- Dashboard shows a priority overlap warning.
- Detail view should show which plugin wins.

### 5.7 Plugin Detail View

Detail view shows:

- CAN IDs targeted by each rule, hex and decimal
- Bus pin and compatibility
- Mux and mux mask
- Additional match fields
- Operations in execution order, for example:
  - `set_bit(46, true)`
  - `counter(byte=0, mask=0x0f, step=1)`
  - `emit_periodic(100 ms)`
  - `checksum(byte 7)`

This is a safety feature. Users must be able to inspect what a plugin changes before enabling it.

### 5.8 Plugin Editor and Rule Test

Plugin Editor lets users build a plugin from form fields and preview JSON.

Rule Test is a manual dashboard action:

1. User selects or edits one rule.
2. User clicks wait for next matching live CAN frame.
3. Firmware captures the matching frame.
4. UI shows before / after data.
5. User chooses a send count.
6. Firmware sends the preview frame only after manual confirmation.

Rule Test constraints:

- Default send count: 1
- Max send count: 5
- No automatic loop
- Still subject to canActive, OTA guard, and applicable safety gates

If implementation scope is too large for one pass, Rule Test may be Phase 2, but the spec and UI placement must reserve it explicitly.

## 6. GTW2047 replay and periodic emit

Use upstream terminology: `GTW2047`, `0x7FF`, `GTW_autopilot`. Jordan's earlier `GTW207` reference maps to this feature.

### 6.1 Replay count semantics

GTW2047 replay count only repeats a modified GTW2047 frame produced by enabled plugin rules.

Flow:

```text
incoming GTW2047 frame
  -> enabled plugin rules match
  -> ops apply to a copy
  -> matching rules compose one modified output frame
  -> firmware sends the modified frame once
  -> if replayCount > 1, firmware immediately repeats the modified frame
```

Rules:

- Only CAN ID `2047` / `0x7FF` is affected.
- Only modified frames are repeated.
- Rule must have `send: true` or default send behavior.
- Non-2047 frames are never affected by plugin replay count.
- Replay count default is `1`.
- Replay count range is `1..5` unless upstream changes are intentionally adopted later.
- Counter operations and checksums must be re-applied or advanced correctly for every repeated send.

UI wording:

```text
GTW2047 Replay Count
Only repeats modified 0x7FF frames generated by enabled plugins.
Default 1x. Higher values increase bus load.
```

### 6.2 `emit_periodic`

`emit_periodic` is valid only for GTW2047 rules with:

- `id: 2047`
- `mux: 3`
- `send: true`

Behavior:

1. First matching live GTW mux 3 frame seeds a cache after all other operations apply.
2. Dashboard loop sends the cached modified frame at the configured interval.
3. Periodic send may continue even when no new live GTW mux 3 frame arrives.
4. Interval allowed range: `10..5000 ms`; default `100 ms`.

### 6.3 `gtw_silent`

`gtw_silent` is part of upstream design but requires proprietary Tesla SecurityAccess seed-to-key support.

GTW UDS endpoints:

- Request: `0x684`
- Response: `0x685`

The silencing sequence requires:

1. Extended diagnostic session
2. SecurityAccess request seed
3. SecurityAccess send key
4. CommunicationControl disable-Tx
5. TesterPresent keepalive

This repository must not fake this capability.

Behavior:

- If `PLUGIN_GTW_UDS_CUSTOM_KEY` is not defined, `gtw_silent: true` is parsed as unavailable/disabled.
- `emit_periodic` may still work.
- Firmware does not run the UDS silencing sequence.
- Firmware does not add `0x684` / `0x685` filters for silent mode unless custom-key support is present.
- UI shows `GTW Silent: unavailable - custom key build required`.

## 7. Plugin APIs

Recommended API set:

### `GET /plugins/status`

Returns engine status, limits, replay count, and installed plugins.

Example shape:

```json
{
  "engineAvailable": true,
  "engineEnabled": true,
  "storage": "spiffs",
  "replayCount": 1,
  "limits": {
    "maxPlugins": 8,
    "maxRulesPerPlugin": 16,
    "maxOpsPerRule": 16,
    "maxFilterIdsPerPlugin": 32
  },
  "installed": [
    {
      "name": "My Plugin",
      "version": "1.0",
      "author": "Your Name",
      "enabled": false,
      "priority": 1,
      "ruleCount": 1,
      "targetIds": ["0x399"],
      "compatible": true,
      "lastError": "",
      "conflicts": []
    }
  ]
}
```

### `POST /plugins/install_url`

Installs a plugin from a raw HTTP/HTTPS URL.

### `POST /plugins/upload`

Installs a plugin from uploaded `.json` file content.

### `POST /plugins/install_json`

Installs a plugin from pasted JSON.

### `POST /plugins/toggle`

Parameters:

- `name`
- `enabled`

### `POST /plugins/priority`

Parameters:

- `name`
- `priority`

### `POST /plugins/remove`

Parameter:

- `name`

### `POST /plugins/replay_count`

Parameter:

- `count`, range `1..5`

### `POST /plugins/rule_test`

Manual test API for Phase 1 if feasible, otherwise reserved for Phase 2.

## 8. Single-CAN bus compatibility

Upstream plugin `bus` supports `CH`, `VEH`, `PARTY`, combinations, bitmask, and arrays.

Standalone has one physical CAN bus. The design must map or reject bus usage explicitly.

Rules:

- If `bus` is omitted, allow and match the standalone default bus.
- If `bus` includes the configured standalone default bus, allow.
- If `bus` targets a non-existent bus or CAN2-only surface, mark incompatible or reject install.
- UI detail view must show bus compatibility.

Suggested default mapping:

- Use the same default bus naming as upstream if project constants exist.
- If no bus model exists yet, define a small compatibility layer rather than inventing UI-only labels.

## 9. Plugin injection path and built-in handlers

Upstream states that dashboard builds do not inject CAN frames from built-in handlers; enabled plugins are the automatic injection path.

The current branch already has built-in handler injection logic. The follow-up must avoid duplicate injection.

Recommended transition model:

1. Standalone keeps built-in handler observation and diagnostics.
2. Enabled plugins become the preferred automatic injection path for plugin-managed features.
3. If a FSD Activation plugin is enabled, the corresponding built-in FSD injection path is disabled or treated as fallback-disabled for that same feature.
4. If no plugin is enabled, current built-in FSD injection may remain as fallback, but UI must show injection source:
   - `Built-in`
   - `Plugin`
   - `Disabled`
5. Plugin execution remains subject to AP Gate, delayed injection, OTA guard, canActive, and FSD master enable.

This preserves existing behavior while aligning the product with upstream plugin direction.

## 10. Standalone UI cleanup audit

The implementation plan must include a source and generated UI audit.

Standalone visible UI must not contain:

- `CAN2`
- `T-2CAN`
- `自动换挡`
- `Auto Shift`
- `Service Mode`
- `Stalk Test`
- `Lighting`
- `Fog`
- `Strobe`
- `Bus2`
- `Burst` / `Flash Burst` user-facing controls

Exception: developer comments, dual-CAN code paths, or docs may mention these if not visible in standalone UI and if dual-CAN behavior is preserved. Contract tests should distinguish user-visible standalone UI from retained dual-CAN implementation.

Function support matrix for standalone should show supported features first:

- Single CAN read/write
- FSD Activation
- AP Gate
- delayed injection
- AP-AUTO-RESTORE
- official JSON plugin system
- GTW2047 replay count
- speed strategy
- driving style
- network settings
- OTA update

Unsupported dual-CAN features may be documented in a compact help section, not promoted as main UI.

## 11. Testing and verification

### 11.1 Contract tests

Add or update Python contract tests for:

- AP Gate controls in Driving Status
- delayed injection control
- AP-AUTO-RESTORE control
- AP injection state strings
- mobile AP controls and status
- plugin install URL/upload/paste UI
- installed plugin list
- enable/disable switch per plugin
- priority control
- detail view strings
- GTW2047 replay count UI
- mobile plugin management
- standalone visible UI forbidden strings
- standalone route calls do not hit CAN2-only endpoints
- dual-CAN route registrations remain available in dual builds

### 11.2 Native tests

Add or update native tests for:

- AP Gate delay config default and persistence
- AP disengage resets stable timer
- AP-AUTO-RESTORE does not bypass gates
- plugin parser accepts valid official JSON
- parser rejects invalid JSON safely
- plugin install default disabled
- enable/disable affects execution
- priority conflict resolution
- GTW2047 replay only affects modified 2047 frames
- non-2047 frames never replay due to replay count
- counter/checksum behavior across repeated sends
- `emit_periodic` only accepts GTW2047 mux 3
- `gtw_silent` unavailable without `PLUGIN_GTW_UDS_CUSTOM_KEY`
- bus compatibility rejects or marks unsupported standalone bus targets

### 11.3 Build and binary verification

Run at minimum:

- Python contract tests
- dashboard minify check
- native dashboard tests
- native AP gate tests
- native plugin tests
- `pio run -e waveshare_single_can_standalone`
- `pio run -e lilygo_t2can_dual`

For standalone firmware, verify binary strings or generated artifacts do not expose standalone-forbidden route strings such as CAN2/lighting/stalk/service entrypoints. Do not rely on ESP-IDF `compile_commands.json` to prove build flags are applied.

## 12. Audit of the previous 10 tasks

The implementation must explicitly review previous work for:

1. malformed JSON or status output regressions
2. UI capability hiding holes, especially mobile menu leaks
3. AP Gate beta.3 parity issues
4. hard-coded 2000 ms where user-configurable delay should be used
5. routes present in standalone when they should be absent
6. UI functions calling routes that standalone does not register
7. generated UI header staleness
8. dual-CAN build regressions
9. plugin docs/tests mismatches
10. firmware asset assumptions that should not affect this design pass

Findings should be fixed in the implementation branch, not only documented.

## 13. Acceptance criteria

The follow-up is complete only when all criteria are met.

### Product UI

- Standalone visible UI says `Atlas Single CAN` or equivalent, not `T-2CAN`.
- Standalone visible UI does not show CAN2, Auto Shift, service/stalk/lighting/burst controls.
- Driving Status contains AP Gate, delayed injection, AP-AUTO-RESTORE, and AP injection state.
- Hardware Mode contains the official plugin manager.
- Mobile UI exposes the same standalone AP and plugin capabilities.

### AP safety

- Default delay is 2000 ms.
- AP disengage clears stable timer.
- AP-AUTO-RESTORE does not bypass any safety gate.
- Waiting / settling / injecting / blocked states are truthful and sourced from backend status.

### Official plugins

- Plugins install from URL, upload, and pasted JSON.
- New installs default disabled.
- Installed plugins persist.
- Each plugin can be enabled, disabled, prioritized, removed, and inspected.
- Conflict/priority behavior follows upstream design.
- Invalid plugin data is rejected safely.
- Bus-incompatible plugins are rejected or clearly marked incompatible in standalone.

### GTW2047

- Replay count defaults to 1 and is bounded.
- Replay count only repeats modified `2047` / `0x7FF` frames produced by enabled plugins.
- Non-2047 frames are not affected.
- Counter/checksum are handled correctly for repeated sends.
- `emit_periodic` is limited to GTW2047 mux 3.
- `gtw_silent` is unavailable without custom key support and is clearly shown as such.

### Dual-CAN protection

- `lilygo_t2can_dual` builds successfully.
- Dual-CAN source features remain available.
- Single-CAN UI cleanup does not remove dual-CAN runtime behavior.

## 14. Implementation sequencing recommendation

Use Subagent-Driven Development again, but split the work into smaller phases than the previous 10-task pass:

1. RED contract tests for standalone UI cleanup, AP UI, plugin UI, and GTW replay semantics.
2. RED native tests for AP config and plugin engine core.
3. AP config/status implementation.
4. Official plugin parser/storage/status API.
5. Plugin execution engine and priority conflict resolution.
6. GTW2047 replay and `emit_periodic`.
7. Vehicle/head-unit UI update.
8. Mobile UI update.
9. Standalone cleanup audit fixes.
10. Full verification and final review.

Implementation may adjust exact task boundaries, but it must preserve the design constraints above.
