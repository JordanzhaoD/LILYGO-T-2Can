# Legacy CAN-A FSD Stability Diagnostic Design

Date: 2026-06-07  
Project: LILYGO T-2CAN firmware  
Scope: Legacy FSD activation on CAN-A / X197 Pin 13/14, TWAI diagnostics, parity-mode A/B testing, and non-reboot recovery

## 1. Summary

The real-car test on 2026-06-07 still used **Legacy mode on LILYGO CAN-A**, connected through X197 Pin 13/14. This corrects the earlier CAN-B assumption. The primary investigation target is therefore not MCP2515 / CAN-B buffer pressure, but this CAN-A path:

```text
X197 Pin 13/14
→ ESP32-S3 TWAI primary CAN
→ appLoop()
→ LegacyHandler
→ 0x3EE / 1006 mux0 and mux1
→ TWAI driver.send()
→ vehicle accepts or rejects the echoed frame
```

The observed symptoms are:

- error frames during real-car testing,
- FSD UI flicker or unstable activation,
- FSD activation state disappearing after running for a while,
- rebooting the LILYGO device temporarily restores activation, but stability remains poor.

The design goal is **not** to blindly copy `tesla-fsd-controller` into LILYGO. Instead, the next firmware should become a Legacy CAN-A diagnostic build that can explain exactly where the failure happens.

The recommended next version is:

```text
v4.1.1 Legacy CAN-A FSD Stability Diagnostic
```

## 2. Corrected Scope

### 2.1 In scope

This design focuses on:

1. Legacy FSD injection on CAN-A / TWAI.
2. 0x3EE / 1006 mux0 and mux1 behavior.
3. TWAI queue sizing, controller error state, and recovery visibility.
4. Legacy handler trigger/gate/TX diagnostics.
5. A/B comparison between current LILYGO Stable mode and `tesla-fsd-controller` parity mode.
6. Watchdog and non-reboot recovery when the activation path stalls.

### 2.2 Out of scope for this diagnostic build

These are intentionally deferred:

1. HW3 / HW4 0x3FD / 1021 handler migration.
2. CAN-B / MCP2515 TX buffer arbitration.
3. Service Mode, fog, stalk flash burst, wheel DND, and other CAN-B feature behavior.
4. Inline MITM hardware changes.
5. New high-risk CAN injection features.

CAN-B remains relevant for later LILYGO features, but it is not the primary explanation for the 2026-06-07 Legacy CAN-A test.

## 3. Source References

### 3.1 LILYGO current path

Key files:

- `include/app.h`
  - Main CAN-A app loop reads `appDriver->read(frame)` and dispatches `h->handleMessage(frame, *appDriver)`.
- `include/drivers/twai_driver.h`
  - LILYGO TWAI driver, current default RX queue 32 and TX queue 16.
- `include/handlers.h`
  - `LegacyHandler` handles 0x3EE / 1006 mux0/mux1 and already has `LegacyFsdDiag` for mux-level diagnostics.
- `include/dash_fsd_diag.h`
  - Existing Legacy FSD diag model.
- `include/web/mcp2515_dashboard.h`
  - Dashboard status, gate logic, and `/status` JSON.

### 3.2 `tesla-fsd-controller` references

Key files:

- `/Users/ziwind/my-vibe-project/tesla-fsd-controller/include/mod_fsd.h`
  - Reference Legacy/HW3/HW4 injection behavior.
- `/Users/ziwind/my-vibe-project/tesla-fsd-controller/include/drivers/twai_driver.h`
  - Reference TWAI queue sizing and simple accept-all behavior.
- `/Users/ziwind/my-vibe-project/tesla-fsd-controller/include/handlers.h`
  - Boot timing markers for first key CAN frames and first successful FSD modification.
- `/Users/ziwind/my-vibe-project/tesla-fsd-controller/src/main.cpp`
  - 1 Hz diagnostic log, TWAI status sampling, bus-off TEC/REC logging, FSD triggered/released log.

## 4. Current Findings

### 4.1 LILYGO Legacy already uses a conservative Stable policy

The current LILYGO Legacy Stable behavior is intentionally conservative:

```text
0x3EE mux0:
  - read trigger state from forceActivateRuntime or isFSDSelectedInUI(frame)
  - when triggered and allowed, set bit46 only
  - do not write speed profile by default

0x3EE mux1:
  - disabled by default in Stable mode
```

This is safer than the original `tesla-fsd-controller` Legacy behavior, which does:

```text
0x3EE mux0:
  - set bit46
  - setSpeedProfileV12V13()

0x3EE mux1:
  - clear bit19
  - optionally clear bit48 when removeVisionSpeedLimit is enabled
```

Therefore, if LILYGO Stable still flickers or loses activation, the first assumption should not be “add more bits.” The first assumption should be “we lack enough evidence to know whether the handler received, decided, sent, or the car accepted the frame.”

### 4.2 Legacy mux0/mux1 TX truth is partially good already

Legacy 0x3EE mux0/mux1 already records real `driver.send()` result:

```text
bool ok = driver.send(frame)
legacyFsdDiag.muxN.recordTx(ok, nowMs)
onSend(mux, ok)
```

This is good and should be preserved.

However, nearby Legacy paths still need truth-model cleanup:

- 0x2F8 / 760 speed offset currently sends without preserving the real send result in handler-level evidence.
- 0x438 / 1080 visionSpeedSlider override does the same.

These paths can add extra TWAI TX load and should report true success/failure.

### 4.3 TWAI controller evidence is missing

`tesla-fsd-controller` samples `twai_get_status_info()` and logs important controller fields when bus-off happens:

```text
state
tx_error_counter
rx_error_counter
tx_failed_count
rx_missed_count
```

LILYGO currently checks bus-off inside the driver, but does not expose enough TWAI controller state in `/status` or persistent logs.

This prevents us from answering these real-car questions:

1. Did TWAI enter bus-off?
2. Did TX errors increase before FSD stopped?
3. Did RX missed / overrun increase before mux0 stopped?
4. Did recovery happen?
5. Is the issue caused by our transmitted frames or by received bus errors?

### 4.4 LILYGO TWAI RX queue is smaller than `tesla-fsd-controller`

`tesla-fsd-controller` uses:

```text
rx_queue_len = 128
tx_queue_len = 16
```

LILYGO currently defaults to:

```text
TWAI_RX_QUEUE_LEN = 32
TWAI_TX_QUEUE_LEN = 16
```

Since Legacy FSD activation depends on continuously receiving 0x3EE mux0/mux1, a smaller RX queue can make LILYGO more sensitive to Dashboard/WiFi load or filter false positives. If RX queue misses increase, `fsdTriggered` may stop refreshing and FSD may appear to “die” until reboot.

### 4.5 LILYGO has more gates than `tesla-fsd-controller`

`tesla-fsd-controller` Legacy logic is mostly:

```text
fsdTriggered = forceActivate || isFSDSelectedInUI(frame)
if fsdTriggered && fsdEnable:
  send modified frame
```

LILYGO adds a richer safety gate:

```text
canActive
OTA guard
AP gate
handler injectionGateOpen()
compile-time enhanced autopilot gate
```

This is useful, but when it fails, the current user-facing reason is not specific enough. A real-car failure can therefore look like “FSD stopped working” even though the actual reason is one gate turning false.

## 5. Failure Taxonomy

The diagnostic build should classify failures into these buckets.

### 5.1 No source frame

Condition:

```text
0x3EE mux0 rx is stale or not increasing
```

Meaning:

- LILYGO is no longer receiving the Legacy FSD source frame on CAN-A.
- This is not a handler-bit problem.

Likely causes:

- TWAI RX missed / overrun,
- TWAI bus error,
- physical layer issue,
- filter/starvation issue,
- 0x3EE not present on the expected bus under the current vehicle state.

### 5.2 Not triggered

Condition:

```text
0x3EE mux0 rx increases, but fsdTriggered=false
```

Meaning:

- The handler sees the source frame, but `forceActivateRuntime || isFSDSelectedInUI(frame)` is false.

Likely causes:

- UI bit not present,
- force activate not on,
- incorrect bit assumption for this Legacy vehicle,
- wrong or unexpected mux/payload.

### 5.3 Gate blocked

Condition:

```text
fsdTriggered=true, but injectionAllowed()=false
```

Meaning:

- The handler wants to inject, but LILYGO policy blocks it.

Must be split into exact reasons:

```text
canActive=false
OTA guard=false
AP gate enabled but not open
handler state not parked/AP/summoning
compile-time gate denied
```

### 5.4 TX failed

Condition:

```text
handler attempted to send and driver.send() returned false
```

Meaning:

- The handler reached TX, but TWAI did not accept the frame.

Likely causes:

- TX queue timeout,
- TWAI bus-off,
- driver not ready,
- controller error state.

### 5.5 TX accepted but vehicle behavior unstable

Condition:

```text
mux0 rx increases
fsdTriggered=true
gate allowed
mux0 tx increases
mux0 err stays zero
TWAI controller appears healthy
but FSD still flickers or drops on the car
```

Meaning:

- LILYGO can transmit, but the vehicle does not stably accept the shared-bus echo.

Likely causes:

- echo timing is wrong,
- mux1/speed-profile parity is required on this vehicle,
- duplicate/stale frame semantics,
- counter/checksum acceptance rules,
- shared-bus injection is less reliable than inline MITM for this signal.

This case is where `legacy_tesla_parity` mode becomes useful.

## 6. Proposed Design

## 6.1 Batch A — TWAI diagnostic visibility

Add a lightweight TWAI diagnostic snapshot to the LILYGO TWAI driver and expose it through `/status`.

Fields:

```json
"twai": {
  "state": "running|bus_off|recovering|stopped|unknown",
  "tec": 0,
  "rec": 0,
  "msgsToTx": 0,
  "msgsToRx": 0,
  "txFailed": 0,
  "rxMissed": 0,
  "rxOverrun": 0,
  "arbLost": 0,
  "busError": 0,
  "recoveries": 0,
  "lastRecoveryAgeMs": 0
}
```

Implementation notes:

- Use `twai_get_status_info()` where ESP-IDF is available.
- Keep native tests compilable by stubbing or guarding ESP-only fields.
- Increment `recoveries` inside the driver’s recovery path.
- Do not change injection behavior in this batch.

Why this matters:

- It tells us whether “error frames” are TX-side, RX-side, or bus-off related.
- It explains whether “running for a while then failing” correlates with controller degradation.

## 6.2 Batch B — TWAI queue and filter diagnostic mode

Align the diagnostic build with the known stable `tesla-fsd-controller` TWAI queue sizing:

```text
TWAI_RX_QUEUE_LEN = 128
TWAI_TX_QUEUE_LEN = 16
```

Options:

1. Make RX queue 128 the new default for LILYGO T-2CAN dashboard builds.
2. Or add a build flag:

```text
-D TWAI_RX_QUEUE_LEN=128
```

The first diagnostic build should prefer option 1 for simplicity.

Add counters for:

```text
framesReadFromTWAI
framesAcceptedByExactFilter
framesDroppedByExactFilter
readDrainBudgetHits
```

Reason:

- LILYGO uses a hardware mask plus exact software filtering.
- Sparse filter sets can admit false positives.
- If false-positive traffic consumes the read drain budget, real 0x3EE processing may become delayed or starved.

A later build can add an explicit `DASH_TWAI_ACCEPT_ALL_DIAG` switch. It is not required for the first diagnostic version if the above counters are enough.

## 6.3 Batch C — Legacy failure reason model

Extend `LegacyFsdDiag` so mux0/mux1 can explain not just “check_ad_false,” but the exact reason.

Add a structured gate snapshot:

```json
"gateReason": {
  "canActive": true,
  "otaAllowed": true,
  "apGateEnabled": false,
  "apGateOpen": true,
  "parked": true,
  "apActive": false,
  "summoning": false,
  "finalAllowed": true,
  "lastBlockedBy": "none|can_active|ota|ap_gate|compile_gate"
}
```

Extend mux status with:

```json
"mux0": {
  "rx": 0,
  "tx": 0,
  "err": 0,
  "lastRxAgeMs": 0,
  "lastTxAgeMs": 0,
  "lastSkip": "sent|not_triggered|gate_blocked|tx_fail|dlc_short",
  "before": "...",
  "after": "...",
  "bus": "can_a",
  "driver": "twai"
}
```

For the first implementation, this can reuse the existing `LegacyFsdDiag` and add fields rather than replacing the structure entirely.

## 6.4 Batch D — Legacy 760 and 1080 TX truth cleanup

Change Legacy 760 and 1080 paths from optimistic success reporting to true send-result reporting.

Current problematic shape:

```cpp
driver.send(frame);
if (onSend) onSend(0, true);
```

Desired shape:

```cpp
bool ok = driver.send(frame);
if (onSend) onSend(0, ok);
record evidence for 760 or 1080;
```

This does not change what bytes are sent. It only makes the evidence honest.

## 6.5 Batch E — Boot timing and event log migration

Migrate the `tesla-fsd-controller` boot timing idea, adapted for Legacy CAN-A:

Track first-seen times:

```text
first 0x398 / 920 GTW_carConfig
first 0x399 / 921 DAS_status_ISA if present on this bus
first 0x3EE / 1006 mux0
first 0x3EE / 1006 mux1
first Legacy FSD TX OK
first Legacy FSD TX FAIL
```

Add 1 Hz event transition log entries:

```text
CAN OK / CAN ERROR
FSD triggered
FSD released with injected count
Legacy mux0 no source frame
Legacy gate blocked by <reason>
TWAI bus-off TEC=<n> REC=<n>
TWAI tx_failed +<n>
TWAI rx_missed +<n>
```

Expose the same data through `/status` so real-car test snapshots do not require serial logs.

## 6.6 Batch F — Legacy parity A/B mode

Add a new Legacy FSD policy:

```text
legacy_stable
legacy_tesla_parity
legacy_experimental
```

### legacy_stable

Current default. Keep unchanged:

```text
mux0:
  set bit46 only
mux1:
  disabled
```

### legacy_tesla_parity

A/B mode that mirrors `tesla-fsd-controller` Legacy behavior:

```text
mux0:
  set bit46
  setSpeedProfileV12V13(frame, speedProfile)

mux1:
  set bit19 false
  if removeVisionSpeedLimit: set bit48 false
```

Purpose:

- Test whether this vehicle needs the same mux0/mux1 behavior as `tesla-fsd-controller` to keep FSD stable.
- Keep this separate from `experimental` so field results are easier to compare.

Default:

```text
legacy_stable
```

The UI must label parity mode clearly:

```text
Tesla Controller 对齐模式：用于 A/B 测试，不是默认安全模式。
```

## 6.7 Batch G — Legacy watchdog and non-reboot recovery

Add a Legacy diagnostic watchdog that does not reboot the ESP32.

Watchdog states:

```text
healthy
no_source_frame
gate_blocked
tx_degraded
tx_ok_vehicle_unconfirmed
```

Detection examples:

```text
If mux0.lastRxAgeMs > threshold:
  state = no_source_frame

If mux0.rx increases but tx does not and lastSkip is gate-related:
  state = gate_blocked

If mux0.err increases or TWAI tx_failed increases:
  state = tx_degraded

If tx continues with no TWAI errors but vehicle still appears unstable:
  state = tx_ok_vehicle_unconfirmed
```

Recovery behavior:

- On TWAI bus-off, let the driver recover as it already does, but record recovery count and last reason.
- On repeated TX failures, clear stale FSD runtime state:

```text
fsdTriggered=false
ADEnabled=false
lastInjectMs=0
```

- Do not reboot the entire device automatically.
- Do not silently re-enable more aggressive injection.

## 7. UI and Status Requirements

The Dashboard should show a single Legacy FSD health card in the module/overview page.

Required fields:

```text
硬件模式：Legacy / CAN-A
0x3EE mux0：RX / TX / ERR / last age
0x3EE mux1：RX / TX / ERR / last age
触发来源：force / UI bit / false
最后跳过原因：sent / not triggered / gate blocked / tx fail / no source
Gate：canActive / OTA / AP gate / final
TWAI：state / TEC / REC / tx_failed / rx_missed
Legacy 策略：stable / tesla parity / experimental
健康状态：healthy / no source / gate blocked / tx degraded / unconfirmed
```

The UI should avoid ambiguous English-only status. Use Chinese labels with exact technical values in smaller text.

Example user-facing messages:

```text
FSD 停止：未持续收到 0x3EE mux0 源帧
FSD 停止：Gate 阻止，原因 canActive=false
FSD 异常：TWAI TX 失败增加，TEC=xx REC=yy
FSD 未确认：TX 成功但车辆状态仍不稳定，建议切换 Tesla Controller 对齐模式做 A/B
```

## 8. Testing Strategy

### 8.1 Unit tests / native tests

Add tests for:

1. Legacy Stable still sends mux0 bit46 only.
2. Legacy Stable still skips mux1.
3. Legacy parity mode writes speed profile on mux0.
4. Legacy parity mode sends mux1 with bit19 false.
5. `driver.send(false)` increments mux err and stores `tx_fail`.
6. Gate false stores the exact gate reason.
7. Legacy 760 and 1080 use real send result.
8. Watchdog classifies stale mux0 as `no_source_frame`.
9. Watchdog classifies gate block as `gate_blocked`.
10. TWAI diagnostic JSON compiles in native build with stub values.

### 8.2 API contract tests

Update `test/test_dashboard_api_contract.py` to assert `/status` contains:

```text
fsdDiag.gateReason
fsdDiag.health
fsdDiag.mux0.bus
fsdDiag.mux0.driver
twai.state
twai.tec
twai.rec
twai.txFailed
twai.rxMissed
legacy_fsd_config supports legacy_tesla_parity
```

### 8.3 Build verification

Required local checks:

```bash
python3 scripts/minify_dashboard.py --check
python3 -m pytest test/test_dashboard_api_contract.py
pio test -e native_dashboard
pio test -e native_legacy
pio run -e lilygo_t2can_dual
```

### 8.4 Real-car test procedure

For the next real-car test:

1. Start with `legacy_stable`.
2. Confirm hardware mode shows `Legacy / CAN-A`.
3. Save `/status` before enabling FSD.
4. Enable FSD.
5. Save `/status` immediately after first successful activation.
6. Let it run until the issue appears.
7. Save `/status` immediately after failure.
8. If mux0 TX is healthy and TWAI is healthy but vehicle still flickers, switch to `legacy_tesla_parity` and repeat.
9. Do not test CAN-B flash/fog/wheel features during this FSD stability test.

## 9. Design Decisions

### 9.1 Do not make `legacy_tesla_parity` the default

Default remains `legacy_stable` because it changes fewer bits and has lower risk.

Parity mode is an A/B tool, not the first-line production behavior.

### 9.2 Do not solve this by automatic reboot

Automatic reboot hides the root cause and can create unsafe behavior. The diagnostic build should recover the driver and clear stale runtime state without rebooting.

### 9.3 Do not prioritize CAN-B arbitration in this build

The 2026-06-07 test used Legacy on CAN-A, so CAN-B arbitration is not part of the immediate root-cause path. It can be revisited after CAN-A stability is understood.

### 9.4 Keep implementation incremental

The first implementation should not combine all possible FSD behavior changes. It should first make the system explain itself. Behavior changes are limited to:

1. RX queue size alignment,
2. honest TX result recording,
3. optional `legacy_tesla_parity` A/B mode.

## 10. Acceptance Criteria

The design is successful when the next real-car failure can be classified into one of these exact outcomes:

1. No 0x3EE mux0 source frame.
2. 0x3EE mux0 exists but trigger is false.
3. Trigger is true but a named gate blocks injection.
4. Trigger and gate are true, but TWAI TX fails.
5. TWAI reports bus-off or rising error counters.
6. TX succeeds and TWAI is healthy, but vehicle still does not accept the echo, requiring parity-mode or timing investigation.

The implementation is not considered complete until `/status` can support that classification without relying on guesswork.

## 11. Open Questions for Field Validation

These questions are not blockers for writing the diagnostic build, but must be answered by the next real-car test:

1. During failure, does `fsdDiag.mux0.rx` continue increasing?
2. During failure, is `fsdDiag.triggered` true or false?
3. During failure, is `fsdDiag.gateReason.finalAllowed` true?
4. Does TWAI `txFailed`, `rxMissed`, `busError`, or TEC/REC increase before failure?
5. Does `legacy_tesla_parity` improve stability when Stable shows TX success but vehicle behavior remains unstable?
6. Is mux1 needed on this Legacy vehicle to maintain stable FSD state?

## 12. Recommended Next Step

Proceed to an implementation plan for:

```text
v4.1.1 Legacy CAN-A FSD Stability Diagnostic
```

Implementation should follow the batches in this order:

1. TWAI diagnostic visibility.
2. TWAI RX queue alignment and filter counters.
3. Legacy gate/failure reason expansion.
4. Legacy 760/1080 TX truth cleanup.
5. Boot timing and 1 Hz event logs.
6. `legacy_tesla_parity` A/B mode.
7. Watchdog and non-reboot recovery.
