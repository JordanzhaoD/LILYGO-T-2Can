# Legacy NAG beta4 Vehicle Capture Analysis

Date: 2026-07-24

Inputs: `can_recording (70).csv` through `can_recording (76).csv`

## Executive conclusion

Legacy Continuous Absolute v4 proves that `0x370` is the effective path, but the current parallel-bus implementation cannot provide clean continuous replacement. It injects the next rolling counter before the vehicle sends that same counter. The vehicle then sends a different payload with the duplicate counter. This both halves the effective injection rate and creates a message-freshness/plausibility risk.

Do not fix this by simply removing the current own-echo block. That would approximately double the conflicting traffic and increase risk. Keep beta4 NAG disabled for normal driving until a beta5 strategy is validated.

## Capture inventory

| Capture | Duration (s) | Native 0x370 RX | Injected 0x370 TX | TX after native | Same-counter vehicle frame <=100 ms |
|---|---:|---:|---:|---:|---:|
| 70 | 120.000 | 2,723 | 1,334 | 1,334 | 1,224 |
| 71 | 119.994 | 2,799 | 1,421 | 1,421 | 1,329 |
| 72 | 34.675 | 769 | 377 | 377 | 338 |
| 73 | 56.768 | 1,317 | 655 | 655 | 615 |
| 74 | 119.998 | 2,507 | 1,273 | 1,273 | 1,080 |
| 75 | 120.000 | 2,833 | 1,403 | 1,403 | 1,344 |
| 76 | 119.997 | 2,341 | 1,134 | 1,133 | 911 |
| **Total** | **691.432** | **15,289** | **7,597** | **7,596** | **6,841** |

Key measurements:

- Native `0x370` rate: 22.11 frames/s.
- Injected `0x370` rate: 10.99 frames/s, or 49.69% of native RX.
- All 7,597 injected checksums are valid.
- 90.05% of TX frames are followed within 100 ms by a vehicle frame with the same counter but a different payload.
- Every TX except one uses `native counter + 1`.
- Typical sequence: native counter `n`, injected counter `n+1`, then vehicle native counter `n+1` with different torque/hands fields.

The checksum is not the problem. Counter ownership and competing payloads are the problem.

## Why suppression is partial

The current handler marks any `0x370` received within 100 ms with the last TX counter as `OwnEcho`. The captures show these are normally the vehicle's next native frames, not exact loopback copies. They have the same counter but different payload and checksum.

Consequences:

1. The vehicle frame is falsely rejected as an own echo.
2. Injection occurs on approximately every other native frame.
3. The receiver sees alternating native and synthetic torque/hands values.
4. The receiver also sees repeated counters with incompatible payloads.

The state machine adds a second loss mechanism. When DAS hands state is `1`, it holds for 500 ms and then stops sending. Across all captures:

| DAS hands state | Native 0x370 opportunities | Injected | Coverage |
|---:|---:|---:|---:|
| 1 | 334 | 53 | 15.9% |
| 2 | 12,921 | 6,514 | 50.4% |
| 3 | 1,853 | 950 | 51.3% |
| 4 | 25 | 13 | 52.0% |
| 7 | 68 | 34 | 50.0% |

All 1,300 captured `0x399` samples report a non-zero hands state. The synthetic `0x370` traffic did not drive the sampled DAS state back to zero, although the driver-observed warning frequency was partially reduced.

## Torque plausibility risk

The injected frame replaces measured torque with an absolute synthetic value around raw zero `2050`:

- Support state: approximately +/-0.4 to +/-1.2 Nm.
- Escalated state: approximately +/-1.2 to +/-1.8 Nm.
- Vehicle native frames continue between injections, generally close to the real measured torque.

This produces abrupt native/synthetic/native transitions at roughly 11 Hz. It is not equivalent to a real continuous driver torque signal. The stronger state is activated when DAS hands state is already `>=3`, which can add signal inconsistency while the warning is escalating.

## Camera/body-control warning

The capture cannot prove the exact source of the displayed camera-cleaning warning:

- Default filtering omitted APP warning matrix IDs `0x47D` and `0x47F`.
- The seven captures contain no warning text or event timestamp.
- `0x389` PMM system-fault reason stayed `0` in all 1,382 captured samples.

The warning may be a generic Autopilot availability consequence of CAN plausibility/freshness rejection, or it may be a real camera-obstruction event. The repeated-counter evidence makes an injection-related fault credible, but not proven.

Next capture filter should be:

`118,186,370,389,399,3EE,47D,47F,488`

Record an AP-only baseline and an injection run on the same route, and note the warning timestamp.

## Recommended beta5 plan

### Phase 1: safety hotfix

1. Force NAG off once after upgrading from beta4; require explicit re-arm.
2. Replace counter-only own-echo detection with an exact TX fingerprint: bus, counter, checksum and full payload in a very short loopback window.
3. Add a collision diagnostic for same counter plus different payload within 100 ms.
4. Auto-disarm NAG on repeated collisions, warning-matrix assertion, TX errors or AP context loss.
5. Add `0x47D` and `0x47F` to the diagnostic recorder profile and export their decoded warning bits in `/rec_meta`.

Phase 1 must not increase injection rate. Its purpose is to identify and stop unsafe behavior.

### Phase 2A: parallel-bus reactive overlay

This is the practical option for the existing parallel X179 harness:

1. Retire continuous absolute injection as the default.
2. Trigger short bounded pulses only when DAS hands state enters a selected warning state.
3. Use a native-relative perturbation rather than replacing the signal with absolute +/-1.8 Nm.
4. Start with no more than +/-0.3 Nm and a multi-second cooldown.
5. Do not escalate torque when hands state rises to `>=3`; stop and disarm instead.
6. Preserve all non-target bits and add warning/freshness abort telemetry.

This can reduce prompt frequency but cannot guarantee complete elimination because the original source frame remains on the bus.

### Phase 2B: true inline replacement

Complete, clean replacement requires a physical man-in-the-middle topology:

1. Split the same CAN segment into source and destination sides.
2. Bridge all admitted traffic bidirectionally.
3. Suppress the original `0x370` in one direction.
4. Forward exactly one modified `0x370` per native period with the original counter cadence.
5. Fail open or bypass safely if the controller resets or the bridge misses deadlines.

The current two channels are connected to separate vehicle buses, not two isolated halves of the same `0x370` path. MITM therefore requires harness/topology changes and must not be enabled by software alone.

## Rejected quick fixes

- Removing the own-echo block and sending on every native frame: doubles collisions.
- Increasing torque beyond +/-1.8 Nm: increases plausibility and steering-risk exposure.
- Continuing injection through hands state `>=3`: compounds an already escalating warning state.
- Editing `0x399` DAS state directly: changes a broader Autopilot status contract and is not justified by these captures.
- Claiming the camera warning is solved without `0x47D/0x47F` evidence: unsupported by the filtered data.

## Release recommendation

- Keep `v4.3.0-beta.4` as an analysis build only.
- Mark Legacy Continuous Absolute v4 as vehicle-observed partial suppression with a counter-collision warning.
- Implement Phase 1 before any further vehicle run.
- Validate Phase 2A with AP-only versus injection paired captures before considering an inline hardware design.
