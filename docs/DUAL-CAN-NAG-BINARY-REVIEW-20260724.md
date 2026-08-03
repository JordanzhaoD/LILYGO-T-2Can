# Dual-CAN Legacy NAG Binary Review and Delivery Plan

Date: 2026-07-24

## 1. Scope

This review compares:

- `微雪版esp32比目鱼1.bin`
- `2can比目鱼固件1.bin`
- the correlated fish firmware source
- the Waveshare single-CAN Human Replay TSL6P and EPAS Late Echo implementations
- the local dual-CAN Legacy Continuous Absolute v4 vehicle captures 70 through 76

No CAN transmit behavior was changed as part of this review.

## 2. Binary Findings

| Item | Waveshare binary | 2CAN binary |
| --- | --- | --- |
| SHA-256 | `e0714156e85dba271cd39fdc486e3f2867939a55d0156f32bfd4f97a89db410b` | `ef8f5849ea3407e0e81a23930d0141ff60cc41e1ee8283e7f45c37af9ca210b8` |
| Image | ESP32-S3, 16 MB | ESP32-S3, 16 MB |
| App version | `43a8f6d` | `43a8f6d` |
| Build time | `Jun 2 2026 11:17:54` | `Jun 2 2026 11:17:54` |
| Framework | ESP-IDF `v5.5.4` | ESP-IDF `v5.5.4` |
| Second CAN | diagnostics compiled as fixed unavailable values | MCP2515 initialization, filters and runtime diagnostics present |

Both binaries expose the same NAG/DND feature family:

- `nagKillerEnabled`, `nagKillerMode`, burst and pause configuration
- Mode B four-level positive/negative torque configuration
- Mode C sweep configuration
- `0x399` hands-on state and warning diagnostics
- `0x3C2` cached scroll state and DND transmission diagnostics
- NAG and DND per-source CAN2 TX counters

The material difference is hardware enablement. The 2CAN image includes real MCP2515 setup and filter paths; the Waveshare image fixes the second-CAN readiness and hardware-filter fields to zero. There is no binary evidence of a different `0x370` counter-ownership algorithm.

The binaries correlate strongly with the supplied fish source, but strings and matching build metadata do not prove that every UI-visible NAG mode actively transmits. In the correlated source, `applyBuildModeGuards()` disables continuous NAG and test injection while retaining the scroll DND path. Runtime capture or disassembly would be required to prove otherwise.

## 3. What the Reference Design Actually Contributes

### 3.1 Reusable

- Cross-bus architecture: use `0x399` on the DAS bus as the warning trigger and CAN2 as the user-input action bus.
- Independent NAG/DND TX-source accounting.
- Fresh native `0x3C2` cache requirement.
- Event-driven action instead of continuous torque replacement.
- Burst/pause limits and explicit runtime diagnostics.

### 3.2 Not reusable without proof

- Treating a UI option or embedded string as proof that torque injection is active.
- Sending `native counter + 1` immediately after every `0x370` frame.
- Assuming a single-CAN timing result transfers unchanged to MCP2515 on CAN2.
- Coupling DND to cabin-camera suppression or `0x3FD` modification.
- Sending on both buses when the source bus is uncertain.

## 4. Late Human Replay Assessment

Human Replay TSL6P contributes useful bounded behavior:

- trigger only when `0x399 hands_on_state > 2`
- 1000 ms active, 1500 ms rest
- bounded sequence `+180, +150, -150, -180` raw units
- abort and TX-failure cooldown
- stop when the warning clears

EPAS Late Echo adds:

- native `0x370` checksum validation
- 35 to 45 ms cadence qualification
- eight clean frames before arming
- predicted next slot, scheduled 3 ms before it
- one in-flight TX token and final gate recheck
- preservation of byte 4 instead of forging a hands-on level

The local vehicle captures confirm a nominal 40 ms native period:

- 15,282 native intervals analyzed
- median 40 ms
- 89.88% of intervals are within 35 to 45 ms
- 8,026 predictions occurred after the reference tracker reached stable state
- the scheduled `predicted - 3 ms` point preceded the next observed native frame in 99.8% or more of stable predictions per capture
- the smallest observed scheduling margin was approximately 1 ms

This proves that receive-only shadow scheduling is feasible. It does not prove clean transmission. ESP32 task latency, MCP2515 SPI latency, CAN arbitration and recorder millisecond resolution consume the remaining margin.

More importantly, Late Echo still transmits the expected next counter before the vehicle transmits that same counter. On the current parallel harness it therefore changes the time of the collision but does not obtain ownership of the counter.

## 5. Why Direct Porting Is Rejected

Captures 70 through 76 contain 7,597 injected `0x370` frames. Of those, 90.05% are followed within 100 ms by a native frame with the same counter and a different payload. The checksum is valid; the problem is competing payloads and counter ownership.

Directly porting Late Human Replay would retain these structural risks:

1. synthetic `n+1` is accepted or observed before native `n+1`
2. native `n+1` then arrives with real torque and a different checksum
3. receivers see duplicate counters with incompatible signal values
4. continuous or burst timing can still trip freshness/plausibility monitoring

Changing from absolute torque to a human-shaped sequence reduces signal harshness, but it does not solve this protocol conflict.

The single-CAN encoder also uses raw zero `0x800` (2048), while the local vehicle work established 2050 as the intended zero. It must not be copied byte-for-byte.

## 6. Recommended Dual-CAN Architecture

### Primary path: Legacy Scroll DND

Use the already implemented `0x399 -> 0x3C2` event path as the normal dual-CAN NAG feature:

```text
CAN1/TWAI: 0x399 warning transition
  -> validate AP state and hands-on state
  -> require fresh native CAN2 0x3C2 mux-1 idle frame
  -> CAN2/MCP2515: bounded neutral volume-wheel action
  -> cooldown or immediate abort
```

This path avoids EPAS torque and `0x370` counter competition. It must remain independent of camera-related fields, speed-wheel actions and continuous NAG.

### Experimental fallback: Dual-CAN Late Human Overlay

Keep a separate default-off research mode only if Scroll DND is ineffective after vehicle validation. It may reuse:

- `0x399` reactive trigger
- 1000/1500 ms bounded phases
- stable-cadence qualification
- one in-flight token
- final gate recheck
- byte-4 preservation
- TX failure and AP abort cooldown

It must not reuse:

- immediate echo
- continuous absolute torque
- forced hands-on level 1
- transmission on both buses
- the 2048 zero point
- unbounded retries

The first version should be shadow-only. It should calculate the proposed frame and due time, record them, and send nothing.

### Complete `0x370` replacement: hardware MITM only

Clean replacement requires isolating the source and destination sides of the same CAN segment, suppressing the original frame in one direction and forwarding exactly one modified frame per native period. The present T-2CAN channels attach to separate vehicle buses, not two isolated halves of one segment. Software alone cannot turn this harness into an inline gateway.

## 7. Fail-Closed Contract

Any experimental torque path must disarm on the first applicable condition:

- hardware mode is not Legacy
- CAN write master or NAG arm is off
- AP state is outside 3 through 6
- AP state enters 8 or 9
- `0x399` context is stale
- native `0x370` checksum, counter step or cadence becomes unstable
- selected source bus changes or becomes stale
- another NAG/DND engine is active
- a same-counter/different-payload collision is observed
- TX fails or its result is not acknowledged before timeout
- warning matrix, CAN bus-off, OTA or recorder safety guard asserts

Only one bus and one TX owner may be active. Re-arming after auto-disarm must require an explicit user action.

## 8. Delivery Phases

### Phase 0: preserve the current baseline

- Keep Continuous Absolute v4 disabled for normal driving.
- Do not combine `0x370` NAG and Scroll DND.
- Preserve current FSD activation and smart speed-offset behavior.

### Phase 1: validate Scroll DND receive contract

- Capture `0x399` on CAN1 and native `0x3C2 mux1` on CAN2 for 120 seconds.
- Manually move the left wheel up and down once.
- Confirm mux, idle payload, native counter/checksum behavior and cache freshness.

### Phase 2: validate one bounded Scroll DND action

- One action per warning transition.
- Compare AP-only and Scroll-DND runs on the same route.
- Stop on camera warnings, set-speed changes, CAN errors or AP degradation.

### Phase 3: build Late Human shadow diagnostics

- No TX.
- Log predicted due time, actual next RX time, prediction error, expected counter, selected bus and proposed payload.
- Require at least three paired 120-second captures with acceptable timing margins.

### Phase 4: decide topology

- If Scroll DND works, keep torque injection removed.
- If Scroll DND fails but shadow timing is stable, a limited overlay can be considered as a separate experimental build.
- If complete suppression is required, redesign the harness as a fail-open inline bridge before further `0x370` work.

## 9. Final Decision

The preferred practical dual-CAN solution is not a direct copy of Legacy Late Human Replay. It is:

1. `0x399 -> 0x3C2` Scroll DND as the primary, non-torque path.
2. Late Human Replay components used only for a receive-only shadow scheduler and diagnostics.
3. No production `0x370` overlay until collision-free ownership is demonstrated.
4. Hardware MITM for any claim of complete and clean `0x370` replacement.

This is the only plan consistent with the supplied binaries, the correlated source, the local dual-CAN topology and the observed vehicle counter collisions.
