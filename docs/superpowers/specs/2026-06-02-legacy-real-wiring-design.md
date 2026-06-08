# Legacy Real Wiring Design

Date: 2026-06-02
Project: LILYGO T-2CAN firmware
Scope: Legacy channel real wiring, UI truth model, CAN-B diagnostics, and FSD flicker stabilization

## 1. Summary

Today’s real-car test confirmed that Legacy speed offset is now connected to a real CAN path and visible in the UI. However, Service Mode is still ineffective, and many newer features shown in the Legacy UI are not actually consumed by the Legacy handler or the CAN-B direct path.

This design chooses the **Real Wiring First** approach:

- Keep verified Legacy paths stable.
- Add evidence for CAN-B direct actions.
- Mark unsupported Legacy features truthfully.
- Stabilize Legacy FSD injection by defaulting to a conservative Stable Mode.
- Avoid large refactors and avoid falsely presenting saved configuration as active vehicle behavior.

The main product rule is:

> If the UI says a feature is active, it must also show whether that feature is saved, armed, actually sending CAN, or unsupported in the current Legacy runtime.

## 2. Current Findings

### 2.1 Verified Legacy speed offset path

Legacy speed offset is wired through the production path:

```text
WebUI speed strategy/custom config
→ runtime speed state
→ dashComputeLegacySimpleOffsetKph()
→ LegacyHandler receives 0x760
→ write 0x760 data[5] low 6 bits
→ driver.send()
```

This path must not be replaced with the old MPP byte6 helper. The old Legacy MPP fields may remain visible as experimental/not-wired status, but they must not be presented as the current production speed path.

### 2.2 Service Mode is CAN-B direct, not LegacyHandler

Service Mode currently uses an independent LILYGO CAN-B path:

```text
/service_mode?on=1|0
→ t2canSetServiceMode()
→ t2canServiceModeTick()
→ CAN-B / MCP2515 sends 0x339
```

The current design sends a four-frame burst at 10 ms intervals:

- enter: `00 00 00 00 00 80 00 00`
- exit: `00 00 00 00 00 00 00 00`

If the car does not respond, the UI must distinguish between:

- module did not send,
- module sent but TX failed,
- module sent successfully but vehicle response is unknown,
- module sent correctly but protocol/bus/vehicle condition is wrong.

### 2.3 Many visible Legacy features are not truly wired

Current classification:

| Feature | Current state | Legacy result |
| --- | --- | --- |
| Legacy FSD injection | Real handler path | Supported, but needs Stable Mode |
| Legacy speed offset | Real handler path | Supported |
| Service Mode | CAN-B direct | Needs TX evidence |
| Wheel DND | CAN-B direct | Needs native 0x3C2 base-frame logic |
| Fog Light | CAN-B direct | Supported, needs TX evidence |
| Stalk Test | CAN-B direct | Supported, needs TX evidence |
| Bionic Steering / NAG | Implemented in NagHandler | Unsupported on Legacy Dashboard path |
| Ban Shield | Consumed mainly by HW4Handler | Unsupported on Legacy |
| Legacy MPP custom | Config/calculation exists | Not wired |
| Gear Assist / Shift | Backend placeholder | Not implemented |

## 3. Design Goals

1. **P0: Stabilize Legacy FSD injection**
   - Default to Legacy Stable Mode.
   - Send mux0 bit46-only.
   - Do not send mux1 by default.
   - Do not write speed profile by default.
   - Add mux diagnostics to identify flicker/error-frame causes.

2. **P1: Add Service Mode 0x339 evidence**
   - Record last command, last data, burst count, TX OK, TX error, bus, and vehicle response status.
   - Do not claim Service Mode is active just because the endpoint was called.

3. **P2: Make Wheel DND use native 0x3C2 when available**
   - Cache recent native 0x3C2.
   - Prefer native base frame.
   - Default to waiting when native 0x3C2 is missing.
   - Keep synthetic fallback as an explicit experimental option only.

4. **P3: Make the UI truthful**
   - Show `Selected / Effective / Detected` handler state.
   - Show `UI Desired / Persisted / Runtime-CAN` state.
   - Mark unsupported/not-wired features clearly.
   - Separate “save config”, “arm injection”, “send now”, and “read-only/unsupported”.

## 4. Legacy FSD Stable Mode

### 4.1 Problem

Real-car testing shows Legacy FSD UI flicker and error frames. Static investigation found that the current Legacy 0x3EE behavior is more aggressive than the historical stable baseline.

Current behavior:

```text
mux0:
  set bit46
  write speed profile

mux1:
  clear bit19
  optionally clear bit48
  send mux1
```

Stable baseline should be:

```text
mux0:
  bit46-only

mux1:
  disabled

mux2:
  disabled for Legacy
```

### 4.2 Policy model

Add a Legacy FSD policy:

```text
legacyFsdPolicy = stable | experimental
```

Default values:

```text
legacyFsdPolicy = stable
legacyFsdMux1Enable = false
legacyFsdProfileWriteEnable = false
legacyFsdVisionLimitClearEnable = false
```

### 4.3 Stable behavior

```text
On 0x3EE mux0:
  record mux0 rx
  determine triggerSource
  if not triggered: skip not_triggered
  if gate blocked: skip exact reason
  copy before payload
  set bit46 only
  send
  record after payload and TX result

On 0x3EE mux1:
  record mux1 rx
  do not send
  lastSkip = disabled_in_stable
```

### 4.4 Experimental behavior

Experimental mode preserves older/aggressive behavior as separately testable flags:

```text
mux0:
  set bit46
  if legacyFsdProfileWriteEnable: write speed profile

mux1:
  if legacyFsdMux1Enable: clear bit19 and send
  if legacyFsdVisionLimitClearEnable: also clear bit48
```

Experimental options are not default and must be labeled as A/B test controls in the UI.

## 5. UI Truth Model

### 5.1 Four UI action classes

| Label | Meaning | Examples |
| --- | --- | --- |
| Save config | Writes config/NVS only | speed strategy, lighting config, bootCan |
| Arm injection | Opens runtime gate; waits for target CAN frame | FSD main switch, AP gate, speed offset strategy |
| Send now | Starts immediate CAN send/burst | service mode, fog trigger, stalk test |
| Read-only / unsupported | Displays telemetry or unavailable state | Bionic on Legacy, Ban Shield on Legacy, Gear Assist |

### 5.2 Three state layers

Each important feature should display:

```text
UI Desired       user-selected intent
Persisted/NVS    saved backend value
Runtime/CAN      active runtime state and TX evidence
```

### 5.3 Legacy Runtime Matrix

Add an overview/module card:

```text
Selected HW: Legacy / Auto / HW3 / HW4
Effective Handler: LEGACY / HW3 / HW4
Detected HW: value from CAN detection
Default when Auto: LEGACY / HW3 / HW4
CAN Primary: online/offline
CAN Secondary: online/offline
AP Gate: Open / Closed / Disabled
OTA Guard: Allowed / Blocked
Injection Active: yes/no
```

### 5.4 Legacy FSD UI

Show:

```text
FSD injection: ON/OFF
Legacy policy: Stable / Experimental
Current strategy: mux0 bit46-only, mux1 disabled, speed profile disabled
mux0: RX / TX / ERR / last skip / last age
mux1: RX / TX / ERR / last skip / last age
triggerSource: force / ui_bit / false / unknown
```

Stable copy:

```text
Stable mode sends mux0 bit46-only and disables mux1 to reduce FSD UI flicker.
```

Experimental copy:

```text
Experimental options may cause FSD UI flicker or error frames. Enable only one option at a time for A/B testing.
```

### 5.5 Service Mode UI

Replace simple toggle semantics with “send now + evidence”:

```text
Service / Maintenance Mode
Action: Enter / Exit
CAN ID: 0x339
Burst: 4 frames
Last Data: 00 00 00 00 00 80 00 00
TX OK: 4
TX Err: 0
Bus: secondary
Vehicle Response: unknown
```

UI must not say “vehicle is in service mode” unless a future response signal proves it.

### 5.6 Wheel DND UI

Show native 0x3C2 readiness:

```text
Native 0x3C2: Seen / Waiting
Native age: N ms
Base frame mode: native / waiting_native / synthetic_fallback
Sequence: idle / waiting_native / running / sent / failed
Last TX: data, age, OK/error
```

Default behavior when native 0x3C2 is missing:

```text
Waiting for native 0x3C2. DND sequence is not sent to avoid blind synthetic frames.
```

### 5.7 Unsupported Legacy cards

Show, do not silently hide:

- Bionic Steering / NAG: `Unsupported on Legacy` because the send logic is in NagHandler and Dashboard Legacy handlerPool does not consume 0x370.
- Ban Shield: `Unsupported on Legacy` because LegacyHandler does not call `handleBanShield()` and does not filter 2047.
- Legacy MPP Custom: `Not wired / Experimental` because config and calculation exist but no production send call exists.
- Gear Assist: `Not implemented` because the backend currently reports that CAN control is not implemented.

## 6. Backend Status Design

### 6.1 Capability matrix

Add to `/status`:

```json
"cap": {
  "handler": "LEGACY",
  "selectedHw": 0,
  "effectiveHw": 0,
  "detectedHw": 0,
  "legacyFsd": "supported",
  "legacySpeedOffset": "supported",
  "serviceMode": "can_b_direct",
  "wheelDnd": "can_b_direct",
  "fogLight": "can_b_direct",
  "stalkTest": "can_b_direct",
  "bionicSteering": "unsupported_on_legacy",
  "banShield": "unsupported_on_legacy",
  "legacyMppCustom": "not_wired",
  "gearAssist": "not_implemented"
}
```

Supported enum values:

```text
supported
can_b_direct
armed_only
observe_only
not_wired
unsupported_on_legacy
not_implemented
experimental
```

### 6.2 FSD diagnostics

Add to `/status`:

```json
"fsdDiag": {
  "policy": "legacy_stable",
  "triggered": true,
  "triggerSource": "force",
  "forceRuntime": true,
  "mux0": {
    "enabled": true,
    "rx": 123,
    "tx": 120,
    "err": 0,
    "lastRxAgeMs": 15,
    "lastTxAgeMs": 12,
    "lastSkip": "sent",
    "before": "01 00 00 00 00 00 00 00",
    "after": "01 00 00 00 00 40 00 00"
  },
  "mux1": {
    "enabled": false,
    "rx": 122,
    "tx": 0,
    "err": 0,
    "lastRxAgeMs": 18,
    "lastTxAgeMs": -1,
    "lastSkip": "disabled_in_stable"
  },
  "gate": {
    "canActive": true,
    "otaAllowed": true,
    "apGateEnabled": false,
    "apGateOpen": true,
    "parked": false,
    "apActive": true,
    "summoning": false
  },
  "lastTx": {
    "bus": "primary",
    "id": "0x3EE",
    "mux": 0,
    "ok": true,
    "ageMs": 12
  }
}
```

Skip reason enum:

```text
sent
disabled_in_stable
disabled_by_policy
not_triggered
check_ad_false
ap_gate_closed
ota_blocked
can_inactive
tx_fail
dlc_short
no_handler
```

### 6.3 Service diagnostics

Add:

```json
"serviceDiag": {
  "state": true,
  "lastCommand": "enter",
  "burstFrames": 4,
  "burstIntervalMs": 10,
  "lastTxAgeMs": 80,
  "lastData": "00 00 00 00 00 80 00 00",
  "txOk": 4,
  "txErr": 0,
  "lastError": "none",
  "bus": "secondary",
  "vehicleResponse": "unknown"
}
```

### 6.4 Wheel DND diagnostics

Add:

```json
"wheelDndDiag": {
  "enabled": true,
  "volume": false,
  "speed": true,
  "native3c2Seen": true,
  "native3c2AgeMs": 120,
  "baseFrameMode": "native",
  "sequenceState": "idle",
  "sequenceStep": 0,
  "lastTxAgeMs": 55,
  "lastData": "10 00 00 01 00 00 00 84",
  "txOk": 4,
  "txErr": 0,
  "lastError": "none"
}
```

### 6.5 CAN-B endpoint consistency

`/bus2_ids` must include the fields that the front-end expects:

```json
"can2": {
  "rx": 123,
  "tx": 45,
  "txerr": 0,
  "eflg": "0x00"
}
```

It may also include compact service and Wheel DND diagnostics for the CAN2 page.

## 7. Minimal Implementation Strategy

### 7.1 New lightweight components

Add small headers rather than large refactors:

```text
include/dash_tx_evidence.h
include/dash_fsd_diag.h
include/dash_capabilities.h
```

#### `dash_tx_evidence.h`

Purpose: common TX evidence for Service, DND, Fog, Stalk, and optionally FSD.

Conceptual structure:

```cpp
struct DashTxEvidence {
    uint32_t id;
    uint8_t bus;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t txOk;
    uint32_t txErr;
    uint32_t lastTxMs;
    DashTxError lastError;
};
```

#### `dash_fsd_diag.h`

Purpose: Legacy FSD policy and mux diagnostics.

Includes:

- policy,
- experimental flags,
- trigger source,
- mux0/mux1 counters,
- before/after payload,
- skip reason.

#### `dash_capabilities.h`

Purpose: map selected/effective handler to capability states for `/status` and UI.

### 7.2 Files expected to change during implementation

```text
include/dash_tx_evidence.h
include/dash_fsd_diag.h
include/dash_capabilities.h
include/handlers.h
include/dash_wheel_dnd.h
include/web/mcp2515_dashboard.h
include/web/mcp2515_dashboard_ui.src.h
include/web/mcp2515_dashboard_ui.h
src/main.cpp
test/test_native_legacy/*
test/test_native_dashboard/*
test/test_dashboard_api_contract.py
```

### 7.3 Batch plan

#### Batch 1: Legacy FSD Stable + diagnostics

- Add FSD policy and diag.
- Default Legacy to Stable.
- mux0 bit46-only.
- mux1 disabled by default.
- Add `/status.fsdDiag`.
- Add UI Stable/Experimental status.
- Add native and API tests.

#### Batch 2: Service 0x339 TX evidence

- Add TX evidence.
- Record Service burst sends.
- Add `/status.serviceDiag`.
- Return service diagnostics from `/service_mode`.
- Add UI evidence card.

#### Batch 3: Wheel DND native 0x3C2

- Cache native 0x3C2 from CAN-B drain.
- Use native base frame by default.
- Wait instead of synthetic send when native is missing.
- Add `/status.wheelDndDiag`.
- Add UI native status.

#### Batch 4: Capability Matrix and UI truth pass

- Add `/status.cap`.
- Add Legacy Runtime Matrix.
- Mark unsupported/not-wired features.
- Add Fog/Stalk TX evidence.
- Fix `/bus2_ids.can2`.
- Add overview risk banner.

## 8. Testing and Verification

### 8.1 Automated checks

Before any real-car test:

```bash
python3 scripts/minify_dashboard.py --check
python3 -m pytest test/test_dashboard_api_contract.py
pio test -e native_legacy
pio test -e native_dashboard
pio test -e native_injection_after_ap
pio test -e native_helpers
pio run -e lilygo_t2can_dual
```

### 8.2 Batch 1 tests

Native Legacy tests:

1. Stable mux0 bit46-only.
2. Stable mux1 disabled.
3. Experimental mux1 enabled.
4. Experimental speed profile write enabled/disabled.
5. Trigger source: force vs UI bit vs false.
6. Gate skip reasons: CAN inactive, OTA blocked, AP gate closed, checkAD false.

API tests:

- `/status` includes `fsdDiag`.
- mux0/mux1 fields are present and typed.
- default policy is stable.

### 8.3 Batch 2 tests

Service tests:

1. Enter burst sends four 0x339 frames with `data[5]=0x80`.
2. Exit burst sends four 0x339 frames with `data[5]=0x00`.
3. TX failures increment `txErr` and set `lastError=tx_fail`.
4. `/status` and `/service_mode` expose `serviceDiag`.

### 8.4 Batch 3 tests

Wheel DND tests:

1. No native 0x3C2 means no send, `waiting_native`.
2. Fresh native 0x3C2 means send based on native frame.
3. Stale native 0x3C2 is treated as missing.
4. Synthetic fallback only sends when explicitly enabled.

### 8.5 Batch 4 tests

Capability/UI contract tests:

- `/status.cap` contains all capability fields.
- `/bus2_ids.can2` contains rx/tx/txerr/eflg.
- UI includes required truth labels:
  - Legacy Stable,
  - mux0 bit46-only,
  - mux1 disabled,
  - Service 0x339,
  - Waiting for native 0x3C2,
  - Unsupported on Legacy,
  - Save config,
  - Arm injection,
  - Send now,
  - Read-only/unsupported.

## 9. Real-Car Verification

### 9.1 Pre-flight

Before connecting to the vehicle:

- Confirm firmware version and UI build timestamp.
- Confirm `effectiveHw=LEGACY`.
- Confirm Legacy Stable is default.
- Confirm Experimental flags are all off.
- Confirm Bionic/Ban Shield are shown as unsupported on Legacy.

### 9.2 Test A: Legacy FSD Stable

Configuration:

```text
FSD main switch: ON
Policy: Stable
mux1: disabled
speed profile write: disabled
vision limit clear: disabled
AP Gate: OFF
```

Pass criteria:

- FSD UI flicker disappears or is significantly reduced.
- mux0 TX increments.
- mux1 TX remains 0.
- mux1 lastSkip is `disabled_in_stable`.
- TX error remains 0.

If it fails, record:

- mux0 before/after,
- mux0 lastSkip,
- txErr,
- apGateOpen,
- otaAllowed,
- triggerSource.

### 9.3 Test B: mux1 A/B

Only after Stable is acceptable.

Configuration:

```text
Policy: Experimental
mux1Enable: ON
speedProfileWrite: OFF
visionLimitClear: OFF
```

If flicker appears, mux1 is a likely root cause and should remain disabled by default.

### 9.4 Test C: speed profile A/B

Configuration:

```text
Policy: Experimental
mux1Enable: OFF
speedProfileWrite: ON
visionLimitClear: OFF
```

If flicker appears, speed profile write is a likely root cause and should remain disabled in Stable.

### 9.5 Test D: AP Gate A/B

Compare AP Gate OFF vs ON.

If flicker only appears with AP Gate ON and `lastSkip=ap_gate_closed` appears frequently, the issue is gate-state oscillation rather than frame content.

### 9.6 Test E: Service Mode 0x339

Pass criteria at module-send layer:

- `serviceDiag.txOk=4`.
- `serviceDiag.txErr=0`.
- `lastData` matches enter or exit command.
- CAN2 EFLG is normal.

Vehicle-response pass criteria:

- Vehicle actually enters/exits service mode.

If module-send passes but vehicle does not respond, conclusion is:

```text
The module sent the 0x339 burst successfully, but vehicle response was not observed. Next investigation should verify protocol content, target bus, and vehicle preconditions.
```

### 9.7 Test F: Wheel DND

Pass criteria:

- Without native 0x3C2, no send occurs and UI shows waiting.
- With native 0x3C2, DND sends based on native frame.
- No TX error or new error frames appear.

### 9.8 Test G: Fog/Stalk

Pass criteria:

- Fog 0x273 and Stalk 0x249 update TX evidence.
- Gear gate is shown clearly.
- TX errors are visible and not hidden.

## 10. Failure Triage

### 10.1 FSD still flickers

| Observation | Interpretation |
| --- | --- |
| mux0 after only changes bit46 but flicker remains | bit46/timing itself may not be stable for this vehicle |
| mux1 TX > 0 in Stable | Stable policy not applied correctly |
| txErr > 0 | send-layer issue |
| frequent `ap_gate_closed` | gate oscillation |
| triggerSource alternates force/ui_bit | trigger source instability |
| before/after changes multiple bytes | Stable policy implementation error |

### 10.2 Service still ineffective

| Observation | Interpretation |
| --- | --- |
| txOk < 4 | burst did not complete |
| txErr > 0 | CAN-B send failure |
| lastData wrong | protocol implementation error |
| txOk=4 but no vehicle response | protocol/bus/precondition issue |
| CAN2 EFLG abnormal | MCP2515/CAN-B physical or bus problem |

### 10.3 DND ineffective

| Observation | Interpretation |
| --- | --- |
| native3c2Seen=false | current bus does not observe 0x3C2 |
| waiting_native | expected safety block |
| txOk increments but vehicle no response | field/bus/trigger condition needs investigation |
| txErr > 0 | send-layer failure |

## 11. Acceptance Criteria

### 11.1 Code acceptance

- All listed automated tests pass.
- `lilygo_t2can_dual` builds successfully.
- Dashboard header is regenerated from `.src.h`.
- No unrelated refactor is included.

### 11.2 UI acceptance

- Legacy Runtime Matrix is visible.
- FSD shows Stable/Experimental and mux diagnostics.
- Service shows 0x339 TX evidence.
- Wheel DND shows native 0x3C2 status.
- Unsupported Legacy features are marked clearly.
- CAN2 page shows rx/tx/txerr/eflg.

### 11.3 Real-car acceptance

- Legacy Stable reduces or eliminates FSD UI flicker.
- mux0 TX is active, mux1 TX is 0 in Stable.
- Service button proves 0x339 burst TX even if vehicle response remains unknown.
- Wheel DND does not blind-send without native 0x3C2.
- UI no longer presents unsupported Legacy features as active.

## 12. Non-Goals

This design intentionally does not:

- merge NagHandler into LegacyHandler,
- wire Ban Shield into LegacyHandler,
- restore old Legacy MPP byte6 as production speed path,
- implement Gear Assist,
- guarantee Service Mode vehicle response without further protocol evidence,
- add hidden CAN injections without UI evidence and tests.

## 13. Next Step

After this spec is approved, create an implementation plan using the Superpowers writing-plans workflow. The plan should execute the four batches in order, with Batch 1 treated as the highest priority because it addresses real-car FSD UI flicker and error frames.
