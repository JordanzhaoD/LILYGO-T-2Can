# Legacy 0x399 -> 0x3C2 Scroll DND Research

Date: 2026-07-24

## 1. Conclusion

The viable non-torque Legacy DND path is an event-driven cross-bus state machine:

1. Read `DAS_status` (`0x399`) from CAN1/TWAI.
2. Decode `hands_on_state = (data[5] >> 2) & 0x0F`.
3. When the state enters a confirmed warning range, request one bounded wheel action.
4. Reuse a fresh native `VCLEFT_switchStatus` (`0x3C2`, mux 1) frame from CAN2/MCP2515.
5. Change only the confirmed scroll byte and transmit `01 -> 00 -> 3F -> 00` at 100 ms intervals.
6. Stop immediately when the warning clears, CAN2/cache becomes stale, OTA starts, or the feature is disabled.

This path does not claim to satisfy the EPAS torque plausibility model. It attempts to acknowledge/reset the driver-attention warning through a normal user-input event. It should be tested independently from continuous `0x370` injection.

## 2. Evidence

### 2.1 0x399 trigger

The supplied sources consistently decode the warning state from `0x399 data[5]` bits 2..5. The correlated 2026-07-24 firmware source uses these trigger ranges:

- primary warning: `3..6`
- escalated warning: `9..10`
- clear/non-trigger: `0..2` and other states

Recent vehicle recordings 70-76 contain these observed AP/hands pairs:

| AP state | hands state | frames |
| --- | ---: | ---: |
| 6 | 1 | 30 |
| 6 | 2 | 1087 |
| 6 | 3 | 169 |
| 6 | 4 | 3 |
| 6 | 7 | 8 |

The recordings therefore confirm real transitions into states 3 and 4 while AP state 6 remains active. They do not contain `0x3C2`, because the recorder filter did not include CAN ID 962 (`0x3C2`).

### 2.2 0x3C2 action

The supplied wheel capture decoder identifies:

- `data[2]`: left/volume scroll, `0x01` up and `0x3F` down
- `data[3]`: right/speed scroll, `0x01` up and `0x3F` down

The candidate DND action is a neutral four-step gesture:

```text
data[2] = 01 -> 00 -> 3F -> 00
interval = 100 ms
```

Using equal and opposite ticks avoids a lasting volume change. The first vehicle test should use only the left/volume byte; the speed byte can alter the set speed and is a separate feature.

### 2.3 Native frame requirement

`0x3C2` is multiplexed. A valid base frame must be selected before injection. The correlated firmware requires a fresh mux-1 frame and preserves the native frame's other bytes. Other supplied source variants instead synthesize a base frame, advance the upper-nibble counter, and recalculate byte 7.

These are incompatible assumptions. Captures must establish the actual vehicle format before choosing one:

- mux discriminator and idle payload
- rolling-counter location and progression
- checksum/CRC location and algorithm
- native period
- whether duplicate or interleaved frames are accepted

Until then, the safer first experiment is fresh native-frame cloning with only `data[2]` changed, no synthetic fallback, and no independent counter/checksum rewrite.

## 3. Current Local Project Gap

The local `DashWheelDND` implementation is not currently a true `0x399`-reactive DND path:

- It starts once when the defense/DND gate changes from closed to open.
- It does not start when `0x399 hands_on_state` enters a warning state.
- It uses 50 ms steps instead of the observed 100 ms cadence.
- It caches every `0x3C2` frame rather than only a validated mux-1 frame.
- It accepts a native frame for only 500 ms instead of 1000 ms.
- It overwrites all of `data[0]` with a local counter and recalculates `data[7]`.
- Its optional speed sequence modifies `data[3]`, which must not be coupled to NAG DND.

The existing API toggles and diagnostics can be retained, but the runtime controller needs a separate reactive trigger state machine.

## 4. Recommended State Machine

```text
DISARMED
  -> ARMED when Legacy + CAN enabled + defense enabled + scroll DND enabled

ARMED
  -> WAIT_NATIVE until a valid, fresh 0x3C2 mux-1 frame is available
  -> TRIGGER_PENDING on 0x399 transition from non-warning to warning

TRIGGER_PENDING
  -> RUNNING if AP context is active and native frame age <= 1000 ms
  -> BLOCKED otherwise

RUNNING
  -> send data[2]: 01, 00, 3F, 00 at 100 ms
  -> COOLDOWN after the fourth successful send
  -> ABORT immediately on warning clear, TX failure, stale cache, OTA, or gate close

COOLDOWN
  -> ARMED after a bounded cooldown
  -> optionally retrigger only if the warning remains active and a configured maximum has not been reached
```

Recommended initial limits:

- trigger on entry into states `3..6`; log `9..10` but do not inject until observed in vehicle data
- one four-frame action per warning transition
- 2-second minimum cooldown
- maximum 3 actions per warning episode
- automatic disarm after 2 TX failures or loss of fresh native `0x3C2`
- feature default off and RAM-arm required for the first vehicle build

## 5. Decoupling Requirements

The reference firmware ties scroll DND to `cabinCameraDisableEnabled`, and on some profiles the same setting also clears cabin-camera-related data in `0x3FD mux1`. Do not copy this coupling.

The local feature should have an independent `legacy_scroll_dnd` switch. It must not:

- modify cabin-camera bits
- modify `0x3FD` as part of DND
- enable continuous `0x370` torque injection
- use the speed wheel automatically
- bypass OTA, CAN health, or TX-failure gates

This separation is especially important because the previous vehicle test reported a camera-cleaning/availability warning.

## 6. Required Diagnostics

Expose these fields in `/status` or `wheelDndDiag`:

- `armed`
- `handsState`
- `warningActive`
- `warningEpisode`
- `triggerCount`
- `nativeMux1Seen`
- `nativeMux1AgeMs`
- `nativeData`
- `sequenceState` and `sequenceStep`
- `txOk`, `txFail`, `lastTxData`
- `cooldownRemainingMs`
- `blockedReason`
- `autoDisarmed`

Suggested block reasons: `disabled`, `not_legacy`, `ap_inactive`, `warning_clear`, `can2_offline`, `no_native_mux1`, `stale_native`, `ota_guard`, `tx_failed`, and `cooldown`.

## 7. Vehicle Validation Plan

### Stage A: receive-only

Record both buses for 120 seconds with at least these IDs:

```text
0x118,0x186,0x370,0x389,0x399,0x3C2,0x3EE,0x3FD
```

Verify native `0x3C2` mux-1 payloads while manually moving the left wheel once up and once down. Determine counter/checksum behavior from the capture.

### Stage B: one-shot bench/vehicle action

- disable continuous `0x370` NAG
- enable only Legacy Scroll DND
- arm one action per `0x399` warning transition
- capture RX/TX on both buses
- confirm the four TX frames, native-frame ancestry, and no CAN errors

### Stage C: effect validation

Compare three runs:

1. no DND
2. one-shot scroll DND
3. bounded repeat scroll DND

Measure warning duration, next warning interval, acceptance/rejection evidence, user-visible volume behavior, AP/FSD continuity, CAN error counters, and camera-related alerts.

## 8. Decision Gate

Implementation can proceed after Stage A confirms a native CAN2 `0x3C2 mux1` frame. The current recordings prove the `0x399` trigger side but not the `0x3C2` transmit contract. Therefore the path is technically plausible and better isolated than continuous torque injection, but it is not yet vehicle-validated end to end.
