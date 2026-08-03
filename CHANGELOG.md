# Changelog

## [4.3.0-beta.27] - 2026-08-04

### Added

- Ported **Soft Engage** (upstream flipper-tesla-fsd v2.16-beta.10, #108
  sibling) to the Legacy `0x3EE mux0` activation gate. With the settle/edge
  verdict already allowing the frame, it holds `bit46` off until the steering
  wheel nears centre (`|angle| <= ±5.0°` and `steerValidity == 0`), or a 5 s
  long-curve timeout elapses so the driver is never stranded. Once released per
  engagement episode it stops gating. Implemented as the pure, native-tested
  `dashSoftEngageRelease()` helper (in `can_helpers.h`) wired into
  `dashLegacyFsdActivationAllowed()` after the steer-jerk defense verdict, using
  the existing `apRestoreState` steering parse. Disabled by default
  (zero-regression); the single-CAN firmware defaults it on, dual-CAN keeps it
  opt-in like the other experimental gates. Exposed via the `soft_engage`
  toggle (NVS `def_se`, `/defense_config`, dashboard switch) with 7 new
  `dashSoftEngageRelease` unit tests.

## [4.3.0-beta.26] - 2026-08-04

### Added

- Ported the `DashLegacySteerDefense` deep module from the single-CAN firmware,
  aligning Legacy `0x3EE mux0` activation with upstream flipper-tesla-fsd #108
  (v2.16-beta.16/.17/.19 parity; the core Abort Guard mechanism is unchanged
  through beta.24, whose later releases only touched adjacent HW detection and
  diagnostics). One module now owns engagement-edge detection, Instant Engage,
  the Minimal Inject burst, and the settle fallback on the Legacy mux0 path. It
  replaces the prior level-triggered `DashMinimalInject` quota on mux0; the base
  module is retained but no longer gates Legacy mux0. All controls default off,
  so continuous activation behavior is unchanged until explicitly armed.
- Added Legacy-only experimental **Instant Engage** (`ap_first_edge`), disabled
  by default: injects at the real AP engagement edge (state `2→3`), bypassing
  the settle wait so activation lands away from the later `6→8/9` abort edge.
- The **Minimal Inject** toggle now drives the steer defense edge burst: at
  each engagement edge it admits only five `0x3EE mux0` activation frames, then
  hard-stops until the next disengage→re-engage. Toggling it on mid-episode does
  not open the burst window (beta.17 edge semantics).
- Exposed `steerDefense` runtime diagnostics (engagement edges, Instant fires,
  Minimal burst budget/used/blocks) under `/defense_config` and a dashboard
  "Steer Defense" status tile. Ported the `test_native_legacy_steer_defense`
  unit suite (15 cases) covering edge detection, Instant one-shot, the 5-frame
  hard stop, mid-episode toggle, and uint32 wrap.

### Changed

- Abort Guard latch release threshold aligned to upstream `DAS_APSTATE_ENGAGED`
  (beta.15 parity): the latch now clears when AP drops below state `3`
  (disengaged `0/1` or available `2`), matching the engagement threshold used by
  AP-First and the steer defense. Previously it cleared only below state `2`.

## [4.3.0-beta.25] - 2026-07-30

### Added

- Added Legacy-only experimental `Abort Guard`, disabled by default. AP state
  `8/9` latches all Legacy writes (`0x3EE mux0/mux1`, `0x438`, `0x370`, and
  left-wheel DND) until a clean AP exit reports state `<2`.
- Added Legacy-only experimental `Minimal Inject`, disabled by default. Each
  `0x399` AP engagement permits exactly five successful `0x3EE mux0`
  activation writes, then blocks only further activation writes for that
  engagement. Source observation and speed-only output remain available.
- Added persisted `/defense_config` controls, dashboard toggles, and runtime
  diagnostics for latch state, AP state, blocked paths, quota, successful TX,
  and engagement count.

## [4.3.0-beta.24] - 2026-07-30

### Fixed

- Decoupled Legacy smart speed offset from the shared HW3/HW4 three-mode
  strategy. Legacy now persists and applies its native four modes: `off`,
  `manual`, `auto`, and `custom`.
- Added independent Legacy manual offset (`0..33 km/h`) and four custom bands
  (`0..63%`). The values drive only the verified `0x3EE mux0` offset path;
  `0x2F8` remains read-only limit telemetry and `Auto` still falls back to
  fresh `0x399` fused limit data.

## [4.3.0-beta.23] - 2026-07-29

### Fixed

- Restored the persisted Legacy AP settle delay. With AP gate enabled, Legacy
  now waits for `0x399` AP state `3..6` to remain continuous for the configured
  `0/1000/2000/3000 ms` before `0x3EE mux0` activation or speed writes proceed.
- AP exit, CAN disable, OTA guard, or gate disable clears the timer. `/status`
  now reports `settling`, the elapsed `apStableMs`, configured `requiredStableMs`,
  and `legacy_ap_state_delay_gate` while the delay is in effect.

## [4.3.0-beta.22] - 2026-07-29

### Fixed

- Fixed the Legacy AP injection gate. With the gate enabled, `0x3EE mux0`
  injection now waits for Legacy `0x399` AP state `3..6`; it no longer treats
  the generic `Parked` or `Summoning` state as a Legacy AP admission.
- Unified the Legacy activation callback, injection/runtime status, plugin
  context, and diagnostics around that same strict AP decision. `/status`
  reports `waiting_ap`, `legacy_ap_state_gate`, and `ApGate` while blocked.

### Compatibility

- With AP gate disabled, the established Legacy continuous activation path is
  unchanged. Enabling the experimental gate is fail-closed: it cannot bootstrap
  an initial AP state by sending bit46 before `0x399` reports AP active.

## [4.3.0-beta.21] - 2026-07-27

### Fixed

- Corrected the beta.20 Legacy Scroll DND volume-up side effect. Vehicle
  captures show that a warning normally needs two positive `0x045` actions:
  the first changes media volume and the second clears the warning.
- Added result-gated balancing. If the warning is still active when the next
  native idle arrives, beta.21 sends one inverse action to neutralize that
  unsuccessful positive attempt before retrying. If `0x399` clears first, the
  inverse is cancelled because the positive action was consumed by the prompt.
- Every inverse action follows a fresh native CAN1 `0x045` rolling counter and
  recalculates CRC-8/J1850. Driver input or any existing safety gate aborts it.

### Diagnostics

- Added per-episode positive count, pending compensation count, and completed
  volume-neutral episode count to the existing Scroll DND diagnostics.

### Preserved

- Legacy `0x3EE` activation, smart speed offset, `0x370`, recorder, network,
  NVS, and mobile navigation paths are unchanged.

## [4.3.0-beta.20] - 2026-07-27

### Fixed

- Removed the inverse `-1` left-wheel action after a successful Legacy NAG
  dismissal. Vehicle capture proved the `+1` action cleared `0x399` hands-on
  state before the inverse action, so the inverse was applied to audio as one
  volume-down step.
- Changed the `0x045` runtime to send one positive action, wait for a native
  idle/counter release and the warning result, then stop immediately when the
  warning clears. A still-active warning may retry after 800 ms, up to three
  positive attempts per episode.

### Preserved

- Retained the `legacy_scroll_dnd` NVS/API contract and deprecated diagnostic
  keys `negativeTx` and `balancedPairs`; they remain readable but no longer
  increase.
- Legacy `0x3EE` activation, smart speed offset, `0x370`, recorder, network,
  NVS, and mobile navigation paths are unchanged.

## [4.3.0-beta.19] - 2026-07-27

### Added

- Added a dedicated Legacy Model X left-scroll DND engine using CAN1 `0x045`
  and the vehicle-confirmed `0x399` hands-on warning trigger.
- Added native-frame-derived `+1 -> native idle -> -1 -> native idle` balancing,
  CRC-8/J1850 generation, rolling-counter following, expected-duplicate
  classification, Own-Echo rejection, and CAN2 `0x292` mirror diagnostics.
- Added runtime diagnostics for source freshness, generated counter/CRC,
  positive/negative TX, balanced pairs, retries, physical input, warning-clear
  latency, counter conflicts, TX failures, and runtime auto-disarm.

### Changed

- Legacy Scroll DND now transmits only CAN1 `0x045`; CAN2 `0x292` and `0x238`
  remain observation-only and the newer `0x3C2` implementation is isolated.
- Removed the Legacy Scroll DND dependency on the defense aggregate switch,
  slew-rate setting, D gear, and CAN2 online state. The persisted
  `legacy_scroll_dnd` switch is the independent feature control.
- Removed the obsolete Legacy speed-scroll `0x3C2` UI control while retaining
  its stored API field for backward compatibility.

### Safety

- Generated frames preserve all native fields except the left tick nibble,
  rolling counter, and CRC. Physical wheel input, OTA, AP exit, stale source,
  counter conflict, TWAI failure, or mode change aborts the active sequence.
- At most three balanced pairs are emitted in one uninterrupted warning
  episode; two consecutive TX failures auto-disarm runtime transmission.
- Existing Legacy activation, smart speed offset, and `0x370` paths are not
  modified by this release.

## [4.3.0-beta.18] - 2026-07-27

### Added

- Added receive-only Legacy Model S/X steering-wheel observation for
  `STW_ACTN_RQ`, prioritizing chassis/BFT ID `0x045` and also recording the
  gateway aliases `0x238` (powertrain) and `0x292` (body).
- Added per-bus `0x045` RX/TX/error counters and decoded observation counters
  for `StW_Lvr_Stat`, `StW_Cond_Psd`, switch activity, rolling counter, and CRC.
- Added a read-only CSV analyzer and an on-car capture procedure for mapping the
  2019 Model X volume wheel before any transmit implementation is considered.

### Safety

- This release does not transmit `0x045`, `0x238`, or `0x292`. Existing
  `0x3C2` Scroll DND remains fail-closed and is not remapped to a Legacy ID.

## [4.3.0-beta.17] - 2026-07-26

### Fixed

- Removed the circular Legacy activation dependency that required `APActive`
  and a settle delay before allowing `0x3EE mux0` bit46 injection. Bit46 is
  itself required to establish the AP/FSD state, which caused intermittent
  activation failures until stale vehicle state or a module restart changed
  the gate result.
- Aligned Legacy activation with `tesla-fsd-controller`: after the explicit
  force/UI trigger, only the CAN master, OTA safety guard, and existing plugin
  ownership check gate the built-in `0x3EE` send path.
- Kept the AP injection gate behavior for HW3/HW4 and auxiliary paths.

### Diagnostics

- Added `activationGateMode=explicit_trigger_can_ota` and
  `legacyIgnoresApGate=true`; Legacy now reports `armed` instead of incorrectly
  displaying `waiting_ap` before the first native `0x3EE mux0` source frame.

## [4.3.0-beta.16] - 2026-07-26

### Changed

- Replaced the directionless beta.7 random walk with a one-sided Mode-C model driven by receive-only `0x488 DAS_steeringControl`.
- Applies torque opposite the valid type-1 steering request, retains the last direction through the center deadband, and fails closed when `0x488` is missing, invalid, or older than 250 ms.
- Delays state-2 support for 2 seconds, delays states 3-5 escalation for 1 second, then uses bounded 0.8-1.8 Nm support or a 2.1 Nm ramp with 0.2 Nm/frame output slew.

### Preserved

- Keeps raw 2050 as zero, preserves native `0x370` byte 4, derives counter+1 and additive checksum from each native frame, and retains Own-Echo, counter-collision, and TX-failure protection.
- Does not inject `0x488`, write `0x399`, depend on unavailable `0x129`, or change Legacy FSD activation and smart speed-offset paths.

### Diagnostics

- Added `0x488` RX count, control type, decoded steering angle, source bus, age, selected episode direction, direction changes, and target torque to the Legacy NAG API.
- Vehicle effectiveness remains unverified pending beta.16 real-vehicle testing.

## [4.3.0-beta.15] - 2026-07-26

### Changed

- Replaced the Legacy `0x370` deterministic alternating waveform with the upstream `v2.16-beta.7` EPAS-faithful mean-reverting random walk.
- Uses raw 2050 as zero, a per-native-frame xorshift step of +/-40 raw units, 1/16 mean reversion, and the upstream asymmetric raw clamps 1780-2500 (-2.7 to +4.5 Nm).
- Removed steering-angle direction dependency; the random walk has no fixed polarity and does not require `0x129`.
- Stops output for DAS hands states 0 and 8 and skips native `handsOnLevel=1`, matching the upstream beta.7 admission behavior.

### Preserved

- Preserves the complete native `0x370` byte 4, including `handsOnLevel`.
- Continues to derive output from the native frame with counter+1, Tesla additive checksum, learned same-source bus, Own-Echo fingerprinting, collision degradation, and TX-failure auto-disarm.
- Existing Legacy FSD activation, smart speed offset, Scroll DND, UI, API, and NVS contracts are unchanged.

### Diagnostics

- Added `torqueModel=beta7_mean_reverting_random_walk`, `directionSource=none`, and `randomWalkSteps` to Legacy NAG diagnostics.

## [4.3.0-beta.14] - 2026-07-25

### Fixed

- Fixed `recorder queue allocation failed` on ESP32-S3 after Wi-Fi/WebUI startup.
- Allocated the 2,048-record queue payload from PSRAM first, with internal-heap fallback, and kept the FreeRTOS queue control block static.
- Added `queue_psram`, `queue_depth`, and heap diagnostics to `/rec_status` and allocation errors.

### Validation

- Recording remains queue-backed and non-blocking for CAN producers; the queue is initialized once and is not reallocated on every recording session.

## [4.3.0-beta.13] - 2026-07-25

### Fixed

- Aligned Legacy Continuous Absolute with upstream v2.16-beta.10 EPAS-faithful evidence by preserving native `0x370` byte 4, including `handsOnLevel`, instead of forcing level 1.
- Retained the vehicle-established 2050 raw zero point, existing +/-1.8 Nm limit, follower scheduling, checksum, and rolling-counter behavior.

### Diagnostics

- Added `0x129` steering angle and `0x5D9` hands-on-reason candidate traffic to the default 120-second recorder profile for direction and warning-cause correlation.

## [4.3.0-beta.12] - 2026-07-25

### Fixed

- Classified valid same-counter native `0x370` frames arriving 20-100 ms after TX as expected vehicle followers instead of Own-Echo or immediate collisions.
- Allowed expected native followers to drive the next rolling-counter overlay, restoring continuous Legacy torque output instead of degrading after 3-13 frames.
- Retained exact full-payload Own-Echo suppression and three-strike degradation for immediate same-counter conflicts or invalid-checksum followers.

## [4.3.0-beta.11] - 2026-07-25

### Added

- Admitted receive-only `0x3C2` evidence through the CAN1 TWAI filter in Legacy, HW3, and HW4 modes while retaining CAN2 MCP2515 receive-all monitoring.
- Added independent CAN1/CAN2 `0x3C2` RX, TX, and TX-error recorder counters to the API and dashboard.

## [4.3.0-beta.10] - 2026-07-25

### Fixed

- Decoupled the FSD defense master, Legacy 0x370 NAG, and HW3 slew limiter persisted controls.
- Replaced counter-only 0x370 own-echo detection with bus, DLC, counter, and full-payload fingerprints.
- Added explicit same-counter/different-payload collision diagnostics and automatic degradation after three collisions.
- Simplified the Legacy 0x370 runtime gates to CAN, Legacy handler, AP context, native frame, bus lock, and TX health.

## [4.3.0-beta.9] - 2026-07-25

- Aligned Legacy wheel DND with the reference firmware's `right_scroll` action model: `0x399` warning transitions now select the configured wheel action instead of starting speed DND on the UI switch edge.
- Added the native `0x3C2 mux1` reactive right-scroll sequence `01 -> 00 -> 3F -> 00` at 100 ms intervals; when both wheel actions are enabled, right-scroll is selected explicitly and reported in diagnostics.
- Added `dndActionType` to `wheelDndDiag` and kept the fail-closed native-frame, CAN2, OTA, cooldown, TX-failure, and auto-disarm gates. No synthetic `0x3C2` template or fixed-frame fallback was added.

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),

## [Unreleased]

## [4.3.0-beta.8] - 2026-07-25

### Fixed

- Process Legacy Scroll DND `0x399` context from its native source controller instead of excluding the MCP2515/PARTY channel. Vehicle captures 77-79 contain all `0x399` traffic on that logical channel, so the previous gate left diagnostics at `ap_inactive` despite AP state 6 and hands-warning states 3/4 being present.
- Added `0x399` source, age, raw AP state and hands state to `wheelDndDiag` and the dashboard.

### Safety

- Captures 77-79 contain no native `0x3C2` on either recorded logical bus across 360 seconds. Synthetic `0x3C2` fallback remains disabled; Scroll DND fails closed with `no_native_mux1` until the second channel exposes a verified native mux-1 frame.

## [4.3.0-beta.7] - 2026-07-25

### Fixed

- Restored source-driver-following for Legacy `0x3EE`: whichever controller receives the native frame handles and retransmits it through that same controller, matching `tesla-fsd-controller` and the last vehicle-working beta baseline.
- Removed the beta.6 static TWAI/CAN1 rejection that caused native `0x3EE` RX to increase while all Legacy activation TX remained zero on harnesses where physical CAN1 reaches the MCP2515 controller.
- Preserved secondary RX recorder single-write behavior through the handler `onFrame` callback.

### Reliability

- Retained bounded MCP2515 recovery after five consecutive TX failures or bus-off, including restoration of bitrate, Normal mode, filters or receive-all mode.
- Legacy payload, mux handling, activation bit 46, profile writes, AP gate and speed-offset calculations are unchanged.

## [4.3.0-beta.6] - 2026-07-24

### Added

- Added CAN2 native `0x3C2` capture diagnostics for total RX, mux-1, mux-1 idle, manual volume up/down, unexpected speed-wheel activity, injected TX, failed TX, and CAN1 sightings.
- Added the active recorder filter and a `wheel_3c2` evidence object to the recorder metadata export.
- Added live `0x3C2` evidence counters to the CAN recorder dashboard.
- Added CAN2 MCP2515 recovery diagnostics for recovery count, consecutive TX failures, and rejected mirrored `0x3EE` frames.

### Changed

- Expanded the default 120-second Legacy DND capture profile to `118,186,370,389,399,3C2,3EE,3FD,47D,47F,488` so native wheel traffic, warning context, camera-warning matrices, and steering-control context are retained together.
- Bound Legacy `0x3EE` to the internal TWAI channel. Vehicle testing showed this mapping was incorrect for the installed dual-CAN harness; this build is revoked and replaced by beta.7.
- Added bounded CAN2 MCP2515 recovery after five consecutive TX failures or bus-off, preserving the receive-all recorder mode after reset.

### Safety

- Recorder changes are observation-only. They do not arm Scroll DND, alter `0x3C2`, or change any CAN transmission gate.
- A CAN2 RX count proves frame visibility only. Vehicle acceptance still requires paired baseline/action captures and an observable vehicle response.

## [4.3.0-beta.5] - 2026-07-24

### Added

- Added an independent Legacy Scroll DND path driven by CAN1 `0x399` hands-warning transitions and a fresh native CAN2 `0x3C2` mux-1 idle frame.
- Added warning episode, hands state, native-frame age, action count, cooldown, TX failure, auto-disarm, and block-reason diagnostics to `wheelDndDiag`.
- Added `legacy_scroll_dnd` as an explicit `/defense_config` alias while preserving the existing `dnd_volume` API and `def_dv` NVS key.

### Changed

- Changed the reactive wheel sequence to `01 -> 00 -> 3F -> 00` at 100 ms intervals and limited it to three actions per warning episode with a two-second cooldown.
- Reactive Scroll DND now changes only `0x3C2 data[2]`; it preserves the native counter/checksum bytes and never starts the speed-wheel path.
- The dashboard switch now waits for a real `0x399` warning instead of transmitting immediately when enabled.

### Fixed

- Fixed the dual-CAN mobile More menu remaining open after navigation to FSD Defense.
- Removed misleading UI coupling between ISA sound suppression and wheel/camera behavior.

### Safety

- Legacy Scroll DND remains default-off and requires CAN write, defense master, effective Legacy mode, active AP state 3 through 6, CAN2 availability, no vehicle OTA, and a native idle `0x3C2` mux-1 frame no older than 1000 ms.
- Hands-warning states 9 and 10 are observation-only. Two consecutive transmission failures automatically disarm the feature until the user toggles it off and on.
- Scroll DND does not modify cabin-camera fields, `0x3FD`, or continuous `0x370` NAG behavior. Vehicle effectiveness remains pending controlled validation.

## [4.3.0-beta.4] - 2026-07-24

### Fixed

- Fixed Legacy NAG being permanently blocked when real `0x399` frames report AP status 6. Legacy now consistently treats statuses 3 through 6 as active context without changing HW3/HW4 AP semantics.
- Removed the Legacy NAG D-gear gate after captures 68/69 proved the current `0x118` gear bit definition is incompatible with the target vehicle and causes rapidly changing false gear values.

### Added

- Added a native regression using the captured status-6/no-gear scenario and requiring a same-bus, counter+1, checksummed `0x370` TX frame.
- Added `gearGateRequired=false` and Legacy AP context limits to the NAG runtime diagnostics.

### Safety

- Legacy NAG remains default-off and still requires CAN write, the defense master, Legacy mode, fresh `0x399` status 3..6 context, same-bus learning, a native `0x370` frame, accepted native hands level, and successful bounded transmission.
- The D-gear gate is removed only from Legacy NAG. Lighting, fog, shifting, HW3/HW4, and other vehicle-write safety gates are unchanged.

## [4.3.0-beta.3] - 2026-07-24

### Added

- Added a 120-second queue-backed CAN recorder that streams compact 19-byte records to SPIFFS from a low-priority writer task.
- Added 60/90/120-second selection, editable hexadecimal CAN ID filtering, and a default Legacy NAG diagnostic filter (`118,186,370,389,399,3EE`).
- Added recorder progress, written-frame, dropped-frame, file-size, storage-full, finalizing, and stop-reason diagnostics.
- Added streamed binary-to-CSV download conversion so stopping a recording no longer serializes a large in-memory buffer.

### Changed

- Removed the 2000-frame RAM capture ceiling and synchronous `/rec_stop` CSV save path.
- Recorder producers now use non-blocking queue writes and count dropped records instead of blocking CAN processing.

### Safety

- A 120-second capture is storage-guaranteed only with a suitably narrow filter. Unfiltered full dual-CAN traffic stops explicitly on storage exhaustion and reports `storage_full` rather than silently truncating.

## [4.3.0-beta.2] - 2026-07-24

### Added

- Added the `Legacy Continuous Absolute v4` engine using raw torque zero point 2050, a hard `+/-1.8 Nm` limit, and hands-on level 1.
- Added continuous same-source `0x370` responses on the learned native CAN bus, one bounded retry, and automatic runtime shutdown after three consecutive logical TX failures.
- Added v4 state, coverage, silence, retry, torque, target-hands, and degradation diagnostics to `/status` and `/defense_config`.

### Changed

- Replaced the Legacy Reactive v2 burst algorithm with Continuous Absolute v4 while preserving the beta1 `bionic_steering` API, `def_bio` NVS key, dashboard switch, and page initialization logic.
- Native `0x370` target hands levels 0 and 1 are accepted; levels 2 and 3 remain blocked.

### Safety

- Legacy NAG remains default-off and requires the existing CAN-write master, defense master, Legacy mode, fresh Drive/AP context, learned bus affinity, and a live native `0x370` frame.
- This implementation is host-tested and firmware-build verified but remains pending controlled vehicle validation.

## [4.3.0-beta.1] - 2026-07-23

### Added

- Added the Waveshare-verified Legacy smart speed-offset engine with fixed, automatic, and four-zone custom percentage strategies.
- Added 3-second `0x2F8` map-limit priority with fresh `0x399` fused-limit fallback and default 5 km/h/s downward smoothing.
- Added automatic CAN1/CAN2 `0x3EE mux0` route learning, mirrored-frame ambiguity blocking, manual bus override, and same-driver transmission.
- Added `/legacy_speed_config`, `legacySmartSpeed` runtime diagnostics, NVS persistence, and desktop/mobile controls for route and smoothing.

### Changed

- Changed Legacy `0x2F8` handling to read-only telemetry; calculated offset requests now modify the native `0x3EE mux0` frame.
- Mapped the existing `/speed_strategy` and `/speed_custom` contracts to Legacy without changing HW3/HW4 behavior.
- Replaced hard-coded Legacy CAN-A/TWAI diagnostics with the actual frame bus and corresponding dual-CAN driver labels.

### Safety

- Legacy smart speed fails closed on stale/unknown limits, route ambiguity, bus mismatch, disabled CAN write, OTA/AP gate rejection, plugin ownership, or an unchanged output frame.
- Automatic routing requires three consistent native `0x3EE mux0` samples; mirrored frames require explicit CAN1/CAN2 selection.

## [4.2.0-beta.1] - 2026-07-23

### Added

- Added user-verified Legacy Reactive NAG v2 as an optional Legacy subfeature, reusing the existing defense/NVS contract.
- Added proactive and reactive `0x370` handling from real source frames with bounded torque adjustment, counter/checksum regeneration, and own-echo rejection.
- Added automatic CAN1/CAN2 target-bus learning and same-driver transmission to prevent dual-bus duplicate injection.
- Added `/status` and `/defense_config` Legacy NAG diagnostics for mode, selected bus, AP/hands-on state, block reason, RX/TX counts, failures, and frame ages.

### Changed

- Replaced the active `0x370` prohibition with positive runtime contracts after user confirmation on the target vehicle.
- Renamed the dashboard control from experimental bionic steering to verified `Legacy NAG免打扰`, while preserving the `bionic_steering` API and `def_bio` NVS key.

### Safety

- Legacy NAG remains default-off and requires Legacy mode, CAN write, defense master, D gear, engaged AP state, fresh DAS context, a learned target bus, and a live native `0x370` frame.

## [4.1.1] - 2026-07-23

### Added

- Added `waveshare_single_can_standalone` profile for Waveshare ESP32-S3-RS485-CAN single-CAN deployments.
- Added Dashboard capabilities for single-CAN UI/API pruning: CAN2, Bus2, Service Mode, Stalk/highbeam, and lighting-stunt surfaces are hidden or unregistered in standalone builds (controlled via the `DASH_SINGLE_CAN_STANDALONE` build flag).
- Added AP Injection Gate safety parity with upstream `ev-open-can-tools v3.0.2-beta.3`: Legacy `0x3EE mux0` requires AP active for 2 seconds when the gate is enabled, and live unknown gear fails closed.
- Added official JSON plugin manager for Waveshare Single CAN standalone: URL install, `.json` upload, paste JSON, disabled-by-default installs, per-plugin enable/priority/remove/detail, GTW2047 replay count, and SPIFFS persistence (`/plugins_state.json`) restored on boot.
- Added AP Gate delayed-injection controls and AP injection state to the standalone driving status UI (desktop + mobile).

### Changed

- Refreshed the desktop and mobile dashboard with the cockpit design system while preserving the LILYGO dual-CAN controls and APIs.
- Restored persisted FSD runtime controls through `GET /config`, including Legacy offset and speed-limit override state.
- Extended Legacy vision-limit clearing, HW3 ISA chime suppression, and HW4 CAN 923 ISA speed-limit rewriting.
- Cleaned standalone desktop and mobile UI to remove CAN2/T-2CAN/Auto Shift product surfaces while preserving dual-CAN builds.

### Security

- Physically removed the experimental EPAS-faithful NAG injection engine and its CAN `0x370` write path. Existing passive/handler behavior is retained; the removed engine must not be reintroduced without independent safety review and bench validation.

## [4.0.2] - 2026-05-31

### Added
- LILYGO T-2CAN dual-bus firmware is now included in CI/release workflows as `firmware-lilygo-t2can-dual.bin`.
- Auto-shift placeholder page now displays read-only gear-assist telemetry from `/gear_assist_status`.

### Changed
- FSD/CAN injection safety now gates handler-level injection through the OTA guard and keeps LILYGO release builds injection-off on boot.
- HW3 speed-offset SNA/NONE handling now preserves incoming mux-2 payload and clears offset telemetry.
- Fog/strobe control now sends a final OFF frame from the CAN task when stopped or when gear is unsafe/stale.
- Wheel DND settings are persisted and re-arm once when the defense gate opens.

### Fixed
- Power-management partial updates preserve omitted settings and use configurable wake pin selection.
- Bionic steering status/re-enable path now reads and resets the active handler state.
- Dashboard drive cards preserve Auto/Sloth/MAX state instead of collapsing to legacy speed profiles.
- Chip temperature display uses the red error style above 60°C.

## [3.0.4-beta.1] - 2026-05-28

### Changed (Breaking)
- FSD detection bit corrected from 37 to 38 (matches verified tesla-fsd-controller)
- All compile-time feature flags (ISA_SPEED_CHIME_SUPPRESS, EMERGENCY_VEHICLE_DETECTION,
  ENHANCED_AUTOPILOT, BYPASS_TLSSC_REQUIREMENT, DASH_FSD_252_COMPAT) replaced with
  runtime switches accessible from Dashboard UI
- LegacyHandler CAN 760: replaced MPP bucket table with simple offset write
- HW3Handler mux 2: complete rewrite with auto target, custom buckets, high-speed mode

### Added
- Auto hardware detection mode (hwMode=3): detects HW3/HW4 from CAN 920
- Ban Shield: CAN 2047 snapshot protection (opt-in)
- TLSSC bypass option (bit 38) for HW3+HW4
- HW4 speed offset support (hw4OffsetRaw) on mux 2
- Legacy CAN 1080 visionSpeedSlider override
- Legacy CAN 1006 mux 1 removeVisionSpeedLimit option (bit 48)
- HW3 auto speed targeting: <60->64kph, =60->100kph, 60-79->85kph, >=80->passthrough
- 15 new CarManagerBase state fields for runtime FSD configuration

and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.0.0-beta.5] - 2026-05-04

### Added

- RGB status LED now reflects runtime state at a glance: solid green = injecting + at least one client connected, blinking green = injecting with no client connected, solid red = injection stopped + connected, blinking red = injection stopped + no client. Solid blue indicates an in-progress OTA firmware update. "Connected" is true when any station is associated with the AP or when the STA uplink is up.

## [3.0.0-beta.4] - 2026-05-04

### Added

- Manual firmware upload now has a dashboard button to clear saved OTA username/password credentials from browser local storage.

## [3.0.0-beta.3] - 2026-05-04

### Added

- Plugin rules can now include an additional byte-mask match (`match_byte`, `match_mask`, `match_val`) so plugins can target specific bit states without adding dedicated firmware toggles.
- Added an example `0x370` duplicate-counter plugin that matches byte 4 bits 7:6 clear, forces byte 3 to `0xB6`, sets byte 4 bit 6, increments byte 6 low-nibble counter, and recomputes byte 7 checksum.

### Fixed

- ESP-IDF WiFi Internet saves now defer reconnect until after the HTTP response, preventing the dashboard request from being dropped while AP+STA mode changes channels.
- Switching saved WiFi networks now disconnects any existing STA association before connecting to the new SSID, avoiding repeated `sta is connected` / deauth log spam.
- WiFi scan now prepares AP+STA mode before scanning, so scans work when the device is otherwise in AP-only mode.
- ESP-IDF WiFi/httpd/netif component logs are reduced to warning/error levels to keep dashboard serial logs readable.

## [3.0.0-beta.2] - 2026-05-04

### Changed

- Pin ESP-IDF builds to ESP-IDF v6.0.1 through PlatformIO `platformio/espressif32` 7.0.0 and keep legacy Arduino boards in `legacy-arduino`.
- Build all supported ESP-IDF and legacy Arduino targets in GitHub Actions, including release artifacts for AtomS3 Mini CAN Base and Waveshare ESP32-S3 RS485/CAN.

### Fixed

- Release and test workflows now run legacy Arduino board builds from `legacy-arduino`, so RP2040 and Feather M4 CI no longer look for removed root Arduino environments.
- CI installs the Python package needed by ESP-IDF 6.0.1 tooling and skips clang-format on the generated dashboard payload header.

## [3.0.0-beta.1] - 2026-05-03

### Added

- ESP-IDF 5.5 native build path replacing the Arduino framework on supported targets (M5Stack AtomS3 Lite verified). Adds `src/espidf_runtime.cpp` plus `include/platform/espidf_runtime.h` providing Arduino-compatible shims (`String`, `WiFi`, `WebServer`, `Preferences`, `Update`, `HTTPClient`, `SPIFFS`, etc.) on top of ESP-IDF APIs.
- ESP-IDF `led_strip` (RMT) implementation of the on-board RGB status LED so the AtomS3 Lite indicator works under IDF (green = injecting, red = idle).
- "Status LED Brightness" subsection in the Configuration card with a slider and number input (0–255), persisted in NVS (`led_b`) and applied live via `/led_brightness`.
- Multi-SSID WiFi support — up to 4 saved networks. The device tries each in turn until one connects (e.g. home + phone hotspot). New endpoints `/wifi_networks`, `/wifi_delete`; `/wifi_config` accepts an `idx` argument to update a specific slot. Legacy single-SSID NVS keys auto-migrate to slot 0 on first boot.
- `scripts/minify_dashboard.py` — minifies the dashboard HTML/CSS/JS (csscompressor + terser + htmlmin) and emits a gzipped `DASH_HTML_GZ[]` payload served with `Content-Encoding: gzip`. Dashboard source-of-truth moved to `include/web/mcp2515_dashboard_ui.src.h`.

### Changed

- Flash usage on `m5stack-atoms3-mini-can-base` reduced from 79.0 % (1,243,217 B) to 64.8 % (1,019,835 B) — about 223 KB saved with no feature loss. Drivers:
  - `CONFIG_COMPILER_OPTIMIZATION_SIZE=y` (was `OPTIMIZATION_DEBUG`),
  - `CONFIG_NEWLIB_NANO_FORMAT=y` plus integer formatting for the FPS field,
  - `CONFIG_ESP_ERR_TO_NAME_LOOKUP=n`,
  - `CONFIG_BOOTLOADER_LOG_LEVEL_ERROR=y`,
  - gzipped dashboard HTML (≈120 KB raw rodata → ≈30 KB compressed).
- `WebServer::send_P` no longer copies the body into a `String` (which OOM'd on the 130 KB dashboard HTML and aborted the httpd task). It now streams large responses in 4 KB chunks via `httpd_resp_send_chunk` directly from the source pointer (`sendRaw`).
- `WebServer::begin()` bumps the IDF httpd task stack to 16 KB and moves the per-request URL-query buffer to the heap, fixing a stack overflow on the first dashboard hit.

### Fixed

- `httpd` task stack overflow on the first client connection on ESP-IDF builds.
- `bad_alloc` / `terminate` reboot when serving the root dashboard page over the soft-AP on ESP-IDF builds.

## [2.6.0-beta.1] - 2026-04-30

### Added

- Added MCP2515 recovery regression coverage for the RP2040 CAN driver.
- Added `platformio_profile.example.h` as the committed build-config template.

### Changed

- `platformio_profile.h` is now treated as a local-only build config and ignored by git. Copy `platformio_profile.example.h` to `platformio_profile.h` before building, then keep board choices, credentials, and keys in the local file.
- Build documentation and helper-script errors now describe `platformio_profile.h` as the local build config and point fresh checkouts to the example file.

### Fixed

- RP2040 MCP2515 builds now recover the CAN controller after repeated TX failures or MCP2515 bus-off (`EFLG_TXBO`) instead of staying silent until power-cycled.
- CI test and release builds now create their local `platformio_profile.h` from `platformio_profile.example.h` before applying per-board build profiles.

## [2.5.2] - 2026-04-29

Stable release bundling the AP Injection Gate Smart Summon fixes from `2.5.2-beta.1` through `2.5.2-beta.6`.

### Fixed

- AP Injection Gate no longer drops during Smart Summon launches that report `DI_autonomyControlActive` while still in Park before an immediate turn. Definite Park frames now clear the summon latch only when ACA is inactive, so the earlier non-zero `UI_selfParkRequest` survives the shift out of Park even when no fresh request frame appears after the shift.

## [2.5.2-beta.6] - 2026-04-29

### Fixed

- AP Injection Gate no longer drops during Smart Summon launches that report `DI_autonomyControlActive` while still in Park before an immediate turn. Definite Park frames now clear the summon latch only when ACA is inactive, so the earlier non-zero `UI_selfParkRequest` survives the shift out of Park even when no fresh request frame appears after the shift.

## [2.5.2-beta.5] - 2026-04-28

### Fixed

- AP Injection Gate no longer stays open for 3-5 s after Autopilot is disengaged, and no longer treats plain TACC as an open-gate condition. `DI_autonomyControlActive` (ACA) on CAN 280 is set during AP, TACC, *and* Smart Summon, so it cannot drive Summoning by itself. Summoning now requires both `ACA=1` *and* a `UI_selfParkRequest` non-zero command observed in the current autonomy episode. ACA falling edge resets the spr-seen flag, so a subsequent TACC engagement does not re-latch the gate. The 5 s ACA-only timeout is removed: AP disengage drops ACA immediately and the gate closes immediately, restoring instant AP / TACC re-engagement.

## [2.5.2-beta.4] - 2026-04-28

### Fixed

- AP Injection Gate now opens on a freshly-booted module when the car is asleep / locked with Sentry. While the DI is asleep, CAN ID 280 (`DI_systemStatus`) is not broadcast at all (only DAS / autopilot ECU IDs 921/1016/1021/2047 keep transmitting), so the previous boot-time `Parked=false` default left the gate stuck at `Waiting AP` until the driver pressed the brake to wake the DI. `Parked` now defaults to `true` at boot; the first `DI_systemStatus` frame with a driving gear (R/N/D) flips it to false. If the DI never reports, the car is asleep / parked and the gate remains open by design.

## [2.5.2-beta.3] - 2026-04-28

### Fixed

- AP Injection Gate now stays open for the full duration of a Smart Summon / Smart Park session. Detection now uses `DI_autonomyControlActive` (CAN ID 280, bit 50 / byte 6 bit 2) as the primary "summon active" signal in addition to `UI_selfParkRequest` on CAN 1016. The DI bit is held high for the entire time the car is being driven by an autonomy stack, so the 5 s spr-only timeout no longer expires mid-summon when the UI command pulse drops to 0 while the car keeps driving itself.

## [2.5.2-beta.2] - 2026-04-28

### Fixed

- AP Injection Gate no longer drops while Summon shifts the vehicle to Reverse. `clearSummonOnPark()` now fires only on a definitive `DI_gear == 1 (P)` value, not on the permissive `isVehicleParked` set that also includes `0=INVALID` and `7=SNA`. SNA can blip during gear transitions (e.g. P->R under Summon control), and the previous logic would clear `Summoning` on that blip and close the gate mid-summon. Shift to Drive was unaffected because `selfParkRequest` stayed non-zero long enough to re-latch `Summoning`; Reverse was reaching gear faster than the latch could recover.

## [2.5.2-beta.1] - 2026-04-28

### Fixed

- AP Injection Gate now opens while the car is asleep / locked with Sentry. `isVehicleParked` now treats `DI_gear` values `0=INVALID` and `7=SNA` as parked in addition to `1=P`. When the DI is asleep it reports SNA on CAN ID 280, which previously left `Parked=false` and kept the gate stuck at `Waiting AP` until the driver pressed the brake to wake the DI. Driving states (R=2, N=3, D=4) are still not parked, so the gate behavior on a moving car is unchanged.

## [2.5.1] - 2026-04-28

Stable release bundling all changes from 2.4.2 onwards (`2.5.0-beta.5` through `2.5.0-beta.12`). Notably, the `Start after AP` dashboard toggle now gates plugin injection on AP, Park, *and* Summon / Smart Park, which makes the firmware compatible with vehicles running Tesla software release **2026.14.3** and newer — these versions reject the always-on injection used by earlier dashboards, so the toggle must be enabled to keep injection working on those vehicles.

### Added

- Dashboard speed profiles now include an `Auto` mode. Auto follows the vehicle follow-distance selection, while a manual dashboard profile stays locked and is injected instead of being overwritten by the car.

### Fixed

- Legacy speed profile selection is now written back into outgoing CAN ID `1006` mux `0` frames so the selected profile actually takes effect on vehicle behavior instead of only changing the observed internal state. (rolled up from 2.4.2-beta.1)
- AP Injection Gate now detects active AP from recorded HW3 `1021` mux `0` frames by reading the observed AD bit, so plugin injection no longer stays stuck at `Waiting AP` after Autopilot is engaged.
- AP Injection Gate now waits for DAS `AutopilotStatus` active states instead of the 1021 UI/config bit, preventing plugin injection from switching to Active when AP is not engaged.
- OTA update finalize now passes `true` to `Update.end()` to force completion regardless of residual byte count, fixing the `Update finalize failed` error that could occur after a successful download via GitHub S3 redirects.
- OTA error paths now log `Update.errorString()` at every failure point (begin, write, finalize) so the root cause is visible in the dashboard log instead of a generic message.
- AP Injection Gate now also opens while the vehicle is in Park, so Summon unlock injection can run while parked and stops again after shifting to Drive.
- Dashboard manual profile selection now persists in firmware state and is applied to Legacy, HW3, and HW4 injection paths.
- AP Injection Gate park detection now also reads `DI_systemStatus` (CAN ID 280) `DI_gear`, so the gate reopens when the vehicle is shifted to Park on Chassis-bus connections that do not carry `DIF_torque`/`DIR_torque` (CAN ID 390). MCP2515 hardware filter slots updated for Legacy, HW3, and HW4 modes to admit ID 280.
- AP Injection Gate now also stays open while Summon / Smart Park is active. HW3 and HW4 handlers parse `UI_driverAssistControl` (CAN ID 1016) `UI_selfParkRequest` (byte 3 bits 4-7); when the request is non-zero (4=PRIME, 5=PAUSE, 7/8=AUTO_SUMMON_FWD/REV, 11=SMART_SUMMON), `Summoning` is asserted and held for 5 s after the last activity, so injection keeps running once the vehicle shifts out of Park into Drive/Reverse under Summon control.
- AP Injection Gate `Summoning` flag is force-cleared whenever the vehicle returns to Park, so a manual P->D shift after a completed summon correctly waits for AP again instead of latching the gate open until reboot.

## [2.5.0-beta.12] - 2026-04-27

### Fixed

- AP Injection Gate Summon detection no longer latches `Summoning` permanently after the first summon use. `UI_summonHeartbeat` is no longer used for activity tracking because it keeps cycling 0..3 indefinitely once summon has run, which prevented the gate from ever closing again. Detection now relies solely on `UI_selfParkRequest` (CAN 1016, byte 3 bits 4-7); the flag holds for 5 s after the last non-zero command and is also force-cleared whenever the vehicle returns to Park, so a manual P->D shift after a completed summon correctly waits for AP again.

## [2.5.0-beta.11] - 2026-04-27

### Fixed

- AP Injection Gate now also stays open while Summon / Smart Park is active. HW3 and HW4 handlers parse `UI_driverAssistControl` (CAN ID 1016) `UI_summonHeartbeat` (byte 0 bits 2-3) and `UI_selfParkRequest` (byte 3 bits 4-7); when either is non-zero, `Summoning` is asserted and held for 1500 ms after the last activity, so injection keeps running once the vehicle shifts out of Park into Drive/Reverse under Summon control.

## [2.5.0-beta.10] - 2026-04-27

### Fixed

- AP Injection Gate park detection now also reads `DI_systemStatus` (CAN ID 280) `DI_gear`, so the "Start after AP" gate reopens when the vehicle is shifted to Park on Chassis-bus connections that do not carry `DIF_torque`/`DIR_torque` (CAN ID 390). MCP2515 hardware filter slots updated for Legacy, HW3, and HW4 modes to admit ID 280.

## [2.5.0-beta.9] - 2026-04-27

### Added

- Dashboard speed profiles now include an `Auto` mode. Auto follows the vehicle follow-distance selection, while a manual dashboard profile stays locked and is injected instead of being overwritten by the car.

### Fixed

- Dashboard manual profile selection now persists in firmware state and is applied to Legacy, HW3, and HW4 injection paths.

## [2.5.0-beta.8] - 2026-04-27

### Fixed

- AP Injection Gate now also opens while the vehicle is in Park, so Summon unlock injection can run while parked and stops again after shifting to Drive.

## [2.5.0-beta.7] - 2026-04-25

### Fixed

- OTA update finalize now passes `true` to `Update.end()` to force completion regardless of residual byte count, fixing the `Update finalize failed` error that could occur after a successful download via GitHub S3 redirects.
- OTA error paths now log `Update.errorString()` at every failure point (begin, write, finalize) so the root cause is visible in the dashboard log instead of a generic message.

## [2.5.0-beta.6] - 2026-04-24

### Fixed

- AP Injection Gate now waits for DAS `AutopilotStatus` active states instead of the 1021 UI/config bit, preventing plugin injection from switching to Active when AP is not engaged.

## [2.5.0-beta.5] - 2026-04-24

### Fixed

- AP Injection Gate now detects active AP from recorded HW3 `1021` mux `0` frames by reading the observed AD bit, so plugin injection no longer stays stuck at `Waiting AP` after Autopilot is engaged.

## [2.5.0] - 2026-04-24

First stable release of the 2.5 series. Bundles all changes from 2.5.0-beta.1 through 2.5.0-beta.4.

### Added

- Dashboard configuration now includes a GTW 2047 plugin replay control, and settings backup/import now preserves plugin replay preferences.
- Dashboard configuration now includes an AP Injection Gate toggle that arms plugin injection but waits until AP/NoA is observed active before sending plugin frames.
- Plugin rules now support `counter` fields and `emit_periodic` for cached GTW mux 3 broadcasts, including editor support and updated plugin documentation.
- Plugin rules now support a `bus` field (`CH`, `VEH`, `PARTY`, comma-separated string, bitmask, or array) to restrict matching to specific CAN bus pins; frames with unknown bus still match for backwards compatibility.
- Plugin rules now support a `mux_mask` field (alias `muxMask`) to control which bits of byte 0 are compared for mux matching; values 0-7 default to low-3-bit mask, values 8-255 default to full-byte mask, enabling low-nibble and full-byte DBC mux styles.
- Plugin list API now includes `bus` and `mux_mask` per rule so the dashboard UI and support exports reflect full rule configuration.
- Plugin editor gained bus and mux-mask input fields per rule, and the rule label, summary, and conflict panel now show bus pin and mux/mask.
- GTW periodic emit can now optionally try to silence native gateway broadcasts through a UDS diagnostic sequence using extended session, SecurityAccess, `CommunicationControl`, and `TesterPresent`.
- HW3 dashboard builds now expose an optional offset slew limiter for plugin-driven mux 2 offset changes.
- Added the shared `INJECTION_AFTER_AP` build option for behaviour-option builds; `ENHANCED_AUTOPILOT` mux 1 injection now waits for AP to be active when this option is enabled.
- Added a WiFi dashboard regression test that covers the WiFi settings UI, backend routes, status payload, and backup fields.
- Added native PlatformIO test environments `native_plugin_engine` and `native_plugin_engine_custom_key` for running plugin engine unit tests without hardware.

### Changed

- Plugin rules now allow up to 16 operations per rule instead of 8.
- `PLUGIN_FILTER_IDS_MAX` raised to 32 (was equal to `PLUGIN_RULES_MAX` = 16) and made overridable at build time.
- Replayed GTW frames, periodic emits, and repeated Rule Test sends now advance counter fields and refresh checksums between sends.
- Dashboard plugin details, validation, support exports, and docs now describe the new replay, counter, and periodic emit behavior.
- `gtw_silent: true` is now silently treated as disabled at parse time unless `PLUGIN_GTW_UDS_CUSTOM_KEY` is defined at build time; the periodic emit still works but no UDS sequence is started and `0x684`/`0x685` filter IDs are not injected.
- UDS request frames sent by the GTW silencing state machine are now tagged with the bus of the frame that seeded the periodic emit cache.
- Incoming frames with `CAN_BUS_ANY` are now normalized to `CAN_BUS_DEFAULT` in the main app loop before being passed to the plugin engine.
- Plugin rule mux matching and test-rule matching refactored into shared `pluginRuleMatchesBus` / `pluginRuleMatchesMux` helpers used by both the engine and the dashboard.
- Plugin editor mux input range extended to -1-255 (was -1-7) to support full-byte DBC mux values.

### Fixed

- GTW silent-mode plugin rules now add the required UDS request/response CAN IDs to the active filter set so the diagnostic state machine can observe replies.
- The CAN analyzer now labels UDS `0x28 CommunicationControl` requests by name.
- WiFi Internet status now follows the live STA connection state reliably after connect attempts and page refreshes instead of getting stuck on `Connecting to ...` or `Not configured`.
- Corrupted WiFi SSID fragments are now ignored in saved settings and filtered from `/wifi_status` responses.
- The WiFi settings form no longer overwrites the SSID field while it is being edited, and the status header no longer depends on optional labels being present.
- Dashboard CAN sniffer and recorder buffers now clamp incoming frame DLC before copying frame data.
- Plugin mux matching now ignores zero-DLC frames instead of treating them as mux 0.
- Plugin conflict detection and detail panel now correctly account for bus mask and mux mask when determining whether two rules can affect the same frame.
- Custom-key plugin engine native tests now validate output against the configured `PLUGIN_GTW_UDS_KEY_READY` value instead of a hard-coded key byte.

## [2.5.0-beta.4] - 2026-04-24

### Added

- Dashboard configuration now includes an AP Injection Gate toggle that arms plugin injection but waits until AP/NoA is observed active before sending plugin frames.
- Added the shared `INJECTION_AFTER_AP` build option for behaviour-option builds; `ENHANCED_AUTOPILOT` mux 1 injection now waits for AP to be active when this option is enabled.

### Fixed

- Custom-key plugin engine native tests now validate output against the configured `PLUGIN_GTW_UDS_KEY_READY` value instead of a hard-coded key byte.

## [2.5.0-beta.3] - 2026-04-24

### Added

- Plugin rules now support a `bus` field (`CH`, `VEH`, `PARTY`, comma-separated string, bitmask, or array) to restrict matching to specific CAN bus pins; frames with unknown bus still match for backwards compatibility.
- Plugin rules now support a `mux_mask` field (alias `muxMask`) to control which bits of byte 0 are compared for mux matching; values 0–7 default to low-3-bit mask, values 8–255 default to full-byte mask, enabling low-nibble and full-byte DBC mux styles.
- Plugin list API now includes `bus` and `mux_mask` per rule so the dashboard UI and support exports reflect full rule configuration.
- Plugin editor gained bus and mux-mask input fields per rule, and the rule label, summary, and conflict panel now show bus pin and mux/mask.
- `PLUGIN_FILTER_IDS_MAX` raised to 32 (was equal to `PLUGIN_RULES_MAX` = 16) and made overridable at build time.
- Added native PlatformIO test environments `native_plugin_engine` and `native_plugin_engine_custom_key` for running plugin engine unit tests without hardware.

### Changed

- `gtw_silent: true` is now silently treated as disabled at parse time unless `PLUGIN_GTW_UDS_CUSTOM_KEY` is defined at build time; the periodic emit still works but no UDS sequence is started and `0x684`/`0x685` filter IDs are not injected.
- UDS request frames sent by the GTW silencing state machine are now tagged with the bus of the frame that seeded the periodic emit cache.
- Incoming frames with `CAN_BUS_ANY` are now normalized to `CAN_BUS_DEFAULT` in the main app loop before being passed to the plugin engine.
- Plugin rule mux matching and test-rule matching refactored into shared `pluginRuleMatchesBus` / `pluginRuleMatchesMux` helpers used by both the engine and the dashboard.
- Plugin editor mux input range extended to −1–255 (was −1–7) to support full-byte DBC mux values.

### Fixed

- Plugin conflict detection and detail panel now correctly account for bus mask and mux mask when determining whether two rules can affect the same frame.

## [2.5.0-beta.2] - 2026-04-24

### Fixed
- Dashboard CAN sniffer and recorder buffers now clamp incoming frame DLC before copying frame data.
- Plugin mux matching now ignores zero-DLC frames instead of treating them as mux 0.

## [2.5.0-beta.1] - 2026-04-23

### Added
- Dashboard configuration now includes a GTW 2047 plugin replay control, and settings backup/import now preserves plugin replay preferences.
- Plugin rules now support `counter` fields and `emit_periodic` for cached GTW mux 3 broadcasts, including editor support and updated plugin documentation.
- GTW periodic emit can now optionally try to silence native gateway broadcasts through a UDS diagnostic sequence using extended session, SecurityAccess, `CommunicationControl`, and `TesterPresent`.
- HW3 dashboard builds now expose an optional offset slew limiter for plugin-driven mux 2 offset changes.
- Added a WiFi dashboard regression test that covers the WiFi settings UI, backend routes, status payload, and backup fields.

### Changed
- Plugin rules now allow up to 16 operations per rule instead of 8.
- Replayed GTW frames, periodic emits, and repeated Rule Test sends now advance counter fields and refresh checksums between sends.
- Dashboard plugin details, validation, support exports, and docs now describe the new replay, counter, and periodic emit behavior.

### Fixed
- GTW silent-mode plugin rules now add the required UDS request/response CAN IDs to the active filter set so the diagnostic state machine can observe replies.
- The CAN analyzer now labels UDS `0x28 CommunicationControl` requests by name.
- WiFi Internet status now follows the live STA connection state reliably after connect attempts and page refreshes instead of getting stuck on `Connecting to ...` or `Not configured`.
- Corrupted WiFi SSID fragments are now ignored in saved settings and filtered from `/wifi_status` responses.
- The WiFi settings form no longer overwrites the SSID field while it is being edited, and the status header no longer depends on optional labels being present.

## [2.4.2-beta.1] - 2026-04-23

### Fixed
- Legacy speed profile selection is now written back into outgoing CAN ID `1006` mux `0` frames so the selected profile actually takes effect on vehicle behavior instead of only changing the observed internal state.
- Added a native Legacy regression test that verifies the selected speed profile bits are injected into the transmitted mux `0` frame.

## [2.4.1] - 2026-04-22

### Added
- Dashboard header now shows the latest observed `GTW_autopilot` state next to the selected hardware mode.
- Native dashboard regression tests now assert that dashboard handlers do not send frames, increment `framesSent`, or fire `onSend`.
- README and dashboard footer now include the support and gift information from `main`.

### Changed
- Dashboard builds no longer compile automatic CAN injection paths from Legacy, HW3, or HW4 handlers; enabled plugins are the automatic injection path.
- HW3 no longer listens for or injects Track Mode request frames.
- ESP32 dashboard builds now use a 4MB OTA partition layout with larger app slots while preserving SPIFFS storage.
- Plugin documentation now describes the dashboard's plugin-only automatic injection behavior instead of firmware overlap warnings.
- Rule Test count and interval controls now use visible labels with `1` and `100` as placeholders, while empty fields still default to one injection every 100 ms.
- Support issue flow now copies the dashboard support report to the clipboard and opens the General Issue form without prefilled title or body text.

### Fixed
- ESP32 dashboard hotspot startup is more reliable by starting the AP earlier, disabling WiFi sleep, validating saved AP/STA credentials, and falling back to AP-only mode when STA connection attempts time out.

## [2.4.1-beta.3] - 2026-04-22

### Fixed
- ESP32 dashboard hotspot startup is more reliable by starting the AP earlier, disabling WiFi sleep, validating saved AP/STA credentials, and falling back to AP-only mode when STA connection attempts time out.

## [2.4.1-beta.2] - 2026-04-20

### Changed
- Rule Test count and interval controls now use visible labels with `1` and `100` as placeholders, while empty fields still default to one injection every 100 ms.
- Support issue flow now copies the dashboard support report to the clipboard and opens the General Issue form without prefilled title or body text.

## [2.4.1-beta.1] - 2026-04-20

### Added
- Dashboard header now shows the latest observed `GTW_autopilot` state next to the selected hardware mode.
- Native dashboard regression tests now assert that dashboard handlers do not send frames, increment `framesSent`, or fire `onSend`.

### Changed
- Dashboard builds no longer compile automatic CAN injection paths from Legacy, HW3, or HW4 handlers; enabled plugins are the automatic injection path.
- HW3 no longer listens for or injects Track Mode request frames.
- ESP32 dashboard builds now use a 4MB OTA partition layout with larger app slots while preserving SPIFFS storage.
- Plugin documentation now describes the dashboard's plugin-only automatic injection behavior instead of firmware overlap warnings.

## [2.4.0] - 2026-04-19

## [2.4.0-beta.1] - 2026-04-19

### Added
- Dashboard plugin priority controls now let users choose which enabled plugin wins overlapping bit writes
- Dashboard plugin conflict warnings now show firmware overlaps, plugin priority overlaps, and the first enabled injection priority

### Changed
- Plugin installs now start disabled so users can review priority and conflicts before enabling them
- Rule Test now waits for the next matching live CAN frame, applies the selected rule to that frame, and injects the captured result with the chosen count and interval
- Dashboard polling now keeps `/status` as the connection gate and backs off non-critical polls during reconnects

### Fixed
- Plugin rules targeting the same CAN ID and mux are merged into one injected frame per incoming frame, preventing contradictory duplicate plugin sends in the same cycle
- TWAI dashboard filtering now drops sparse-mask false positives in software, reducing dashboard load when plugin CAN IDs widen the hardware mask
- Dashboard status rendering no longer breaks when optional status-grid elements are removed or rearranged
- Dashboard remains responsive under heavier CAN/plugin load by limiting per-loop frame draining and giving the web task more scheduling priority

## [2.3.2-beta.2] - 2026-04-18

### Changed
- Dashboard no longer exposes or persists speed profile control; follow distance and the derived profile remain visible as read-only status
- Dashboard profile-related boot logging and legacy NVS cleanup now reflect the new plugin-managed model without stale `SP` or profile-lock state

### Fixed
- Dashboard core no longer injects speed profile or speed offset back onto CAN for Legacy, HW3, or HW4 handlers; those values are now observational unless a plugin explicitly modifies those frames
- Dashboard plugin toggles now apply immediately and batch their persistence, avoiding repeated Wi-Fi stalls while enabling or disabling multiple plugins

## [2.3.2-beta.1] - 2026-04-18

### Changed
- Dashboard builds now ignore all `BEHAVIOUR OPTIONS` from `platformio_profile.h`; these overrides are plugin-managed and are no longer compiled into firmware when `ESP32_DASHBOARD` is enabled

### Fixed
- Dashboard injection stop now also blocks plugin-based frame injection instead of letting enabled plugins keep sending
- TWAI dashboard profile syncing no longer forces commented-out behavior options into the build
- Dashboard boot/runtime state no longer reports legacy built-in ISA, emergency vehicle detection, TLSSC bypass, or nag handling as active in plugin-managed builds
- Plugin cards no longer show the obsolete built-in conflict warning badge and message

## [2.3.1] - 2026-04-18

### Fixed
- Dashboard hardware defaults now follow `platformio_profile.h` reliably for dashboard builds, even when older `DASH_DEFAULT_HW` values still exist in the selected PlatformIO environment
- Reflashing a dashboard build with a new default hardware mode now migrates stale stored hardware defaults from NVS without overwriting an explicit hardware choice made later in the web UI

## [2.3.0] - 2026-04-18

### Added
- Dedicated documentation pages for build and flash setup, dashboard usage, and a docs index for GitHub Pages
- Hardware-specific example plugins for HW3 and HW4 feature replacements, including AD activation, TLSSC bypass, nag suppression, Summon unlock, ISA chime suppression, emergency vehicle detection, and HW4 speed offsets

### Changed
- Dashboard Features card now only exposes Enable Logging; the other vehicle overrides are no longer shown there
- GitHub Actions are now split into separate workflows for tests, releases, and GitHub Pages deployment
- Dashboard and README documentation now reflect the plugin-based override flow and current Pages structure

## [2.2.0] - 2026-04-18

First stable release of the 2.2 series. Bundles all changes from 2.2.0-beta.1 through 2.2.0-beta.14.

### Added
- Plugin Editor UI with live JSON preview, duplicate-name detection, download/export, install support, loading of installed plugins, and a rule test tool for sending generated CAN frames
- CAN Sniffer support for switching between on-wire 11-bit IDs and prefixed DBC JSON IDs, with filtering for both formats
- Dashboard improvements including plugin capacity visibility, default CAN pin hints, a hidden SSID option for the WiFi hotspot, and an Atom S3 Mini injection toggle on the built-in button

### Changed
- Dashboard defaults now follow the selected build flags, including injection-on-boot behavior and vehicle-aware web UI defaults for TWAI and MCP2515 targets
- `esp32_feather_v2_mcp2515` now uses the new MCP2515 driver and web UI
- Reinstalling an existing plugin now preserves its enabled state and works cleanly when the plugin list is already full

### Fixed
- Plugin enable/disable and remove state is now persisted correctly across reboot, and install/remove/toggle actions refresh the dashboard immediately without falling back to a misleading connection error
- Dashboard polling, confirmation flows, and plugin detail panels now behave reliably across reconnects and Chrome on iOS
- WiFi STA/AP persistence, optional NVS reads, injection persistence, and runtime AD gating now behave consistently across reboots and firmware updates
- Atom and Atom S3 builds now use the correct RGB LED pins and a release-safe ESP32 LED API path across the supported CI toolchains

## [2.2.0-beta.14] - 2026-04-18

### Fixed
- Plugin install, remove and enable/disable actions in the dashboard now refresh the plugin list immediately instead of waiting for a manual page refresh
- Dashboard plugin installs no longer fall back to a misleading "Connection error" when the plugin was already applied and only the response body was interrupted
- Atom and Atom S3 dashboard builds now use an ESP32 RGB LED API path that stays compatible across the release CI toolchains

## [2.2.0-beta.13] - 2026-04-18

### Added
- Atom S3 Mini builds can now toggle injection with the built-in button on GPIO41, with the state saved so it persists across reboot

### Fixed
- Atom and Atom S3 dashboard builds now drive the built-in RGB status LED from the correct board pins so injection-off shows red and injection-on shows green reliably

## [2.2.0-beta.12] - 2026-04-17

### Fixed
- Plugin detail panels in the dashboard now stay open across the background plugin-list refresh instead of collapsing unexpectedly
- Dashboard confirmation prompts now use an in-page modal so reboot and other confirm actions work reliably in Chrome on iOS

## [2.2.0-beta.11] - 2026-04-17

### Added
- Plugin Editor can now load installed plugins for in-place editing
- Plugin Editor now includes a rule test tool that can send a generated CAN frame multiple times at a chosen interval

### Changed
- Reinstalling an existing plugin by name now preserves its enabled/disabled state and still works when the plugin list is already full

### Fixed
- Atom S3 and Atom Lite dashboard builds can now save CAN pin settings that use GPIO 6-11 instead of being blocked by the generic ESP32 flash-pin restriction

## [2.2.0-beta.10] - 2026-04-17

### Fixed
- Plugin enabled/disabled state is now persisted across reboot instead of defaulting back to enabled on startup
- Removing a plugin now also clears its persisted enabled/disabled state
- Dashboard background polling now stops cleanly after repeated connection failures and points users to the STA IP after WiFi handoff instead of continuously spamming timeout errors

## [2.2.0-beta.9] - 2026-04-17

### Added
- CAN Sniffer now has a toggle to switch between on-wire 11-bit CAN IDs and DBC JSON IDs with the current bus prefix
- The sniffer filter now accepts both on-wire IDs and prefixed DBC JSON IDs

### Changed
- Migrated `esp32_feather_v2_mcp2515` to the new MCP2515 driver and web UI
- Dashboard feature and injection defaults now follow the selected build flags, and `DASH_INJECTION_ON_BOOT` can be used to start injecting automatically after boot
- Dashboard grid inputs now size correctly in narrower layouts, and `platformio_profile.h` no longer ships with `DRIVER_TWAI` and `HW3` preselected by default

### Fixed
- "Stop Injecting" now persists across reboot instead of silently re-enabling injection on startup
- Runtime AD gating is now applied consistently across Legacy, HW3, and HW4 handlers so blocked mux paths no longer keep injecting

## [2.2.0-beta.8] - 2026-04-16

### Added
- Plugins card now shows the maximum plugin capacity, current usage, and a clearer message when the plugin limit has been reached
- `/plugins` now returns `maxPlugins`, and plugin install errors include the configured maximum

### Fixed
- `DRIVER_TWAI` dashboard builds now treat the vehicle selection in `platformio_profile.h` as the default web UI hardware only; if none is selected, `HW3` is used by default
- WiFi STA credentials are now loaded correctly from NVS on boot after being saved through the dashboard
- Optional AP and WiFi preference reads no longer spam `Preferences` `NOT_FOUND` errors when keys have not been stored yet
- Added the option to show default can pins in webdashboard

## [2.2.0-beta.7] - 2026-04-15

### Added
- Hidden SSID option in the WiFi Hotspot card. When enabled, the access point does not broadcast its SSID — clients have to enter the name manually. Setting is persisted in NVS and included in the settings backup/restore JSON (`ap.hidden`)
- `/ap_config` endpoint accepts a new `hidden` parameter; `/ap_status` returns the current `hidden` flag

## [2.2.0-beta.6] - 2026-04-15

### Changed
- Dashboard layout: the two separate Firmware Update cards (GitHub OTA and manual .bin upload) have been merged into a single card. Manual .bin upload is now a collapsible section under the primary update controls
- WiFi Internet moved into its own top-level card (previously nested inside the Plugins card) — it is used for both firmware updates and plugin downloads, so it deserves its own slot

## [2.2.0-beta.5] - 2026-04-15

### Fixed
- Firmware Update check: no longer offers older versions as "updates". A proper semantic-version comparison is now used (major.minor.patch plus alpha/beta/rc pre-release ranking), so a device on `2.2.0-beta.4` will not be prompted to "update" to `2.0.0` or `2.1.0`
- Auto-Update on Boot uses the same comparison (older releases are skipped)
- CI release job: firmware binaries are now reliably attached to GitHub releases. The workflow first creates the release as a draft, uploads all assets, and then publishes it, which works around the repository's "immutable releases" setting that previously blocked asset uploads after publish

## [2.2.0-beta.4] - 2026-04-15

### Changed
- Default dashboard credentials (`changeme`) are now allowed at build time. Users are expected to change the WiFi AP password and OTA credentials at runtime via the dashboard WiFi Hotspot card (persisted in NVS, OTA-safe)
- Build no longer fails when `DASH_PASS` / `DASH_OTA_PASS` are left at the default `changeme` placeholder

### Removed
- Nag Killer toggle removed from the dashboard Features card. The underlying `NAG_KILLER` build flag remains available for advanced users who want to compile it in

## [2.2.0-beta.3] - 2026-04-15

### Fixed
- Firmware Update check: "JSON parse error" when Beta Channel is enabled (reduced GitHub API response size by using `per_page=1` instead of `per_page=5`, avoids ArduinoJson heap overflow on ESP32)
- CI: firmware artifacts are now correctly attached to GitHub releases. Previous releases failed to attach binaries due to an "immutable release" error when the release was published before the upload step ran

### Changed
- CI release notes: workflow now extracts the matching section from `CHANGELOG.md` for each tag instead of using the whole file, and auto-detects prerelease based on the tag name (`beta`/`alpha`/`rc`)

## [2.2.0-beta.2] - 2026-04-15

### Added
- Auto-Update on Boot: optional toggle in the Firmware Update card. When enabled, the device checks GitHub for a newer release ~15 seconds after the WiFi Internet connection comes up and installs it automatically
- Respects the Beta Channel toggle (only installs prereleases when that is on)
- Setting persisted in NVS so it survives firmware updates
- New endpoints: `/auto_update` (GET/POST)

## [2.2.0-beta.1] - 2026-04-15

### Added
- Plugin Editor card: create plugins via a form UI without writing JSON manually
- Support for all plugin ops: set_bit, set_byte, or_byte, and_byte, checksum
- Per-rule configuration of CAN ID, optional mux value, and send flag
- Live JSON preview updating as you edit, with collapsible rule sections
- Client-side validation (ID, mux, bit 0-63, byte 0-7, value 0-255, hex input `0xFF` supported)
- One-click Install via existing `/plugin_upload` endpoint (no backend changes)
- Download generated plugin as a standalone `.json` file for sharing or backup
- Duplicate-name detection against existing installed plugins

## [2.1.0] - 2026-04-15

First stable release of the 2.1 series. Bundles all changes from 2.1.0-beta.1 through 2.1.0-beta.5.

### Added
- **Rebrand & UX:** renamed UI from "ADUnlock" to "ev-open-can-tools"; dynamic footer with firmware version and device IP; GitHub and Discord links in the footer
- **Plugins:** info icon next to "Plugins" with inline explanation and link to plugin documentation with examples
- **CAN Pins:** runtime-configurable CAN TX/RX GPIO pins via the dashboard, persisted in NVS so custom pin configurations survive OTA firmware updates; validation (GPIO 0-39, TX != RX, GPIO 6-11 blocked for SPI flash)
- **WiFi Internet:** network scanner with RSSI and channel info; static IP configuration (IP, gateway, subnet, DNS); dedicated status and scan endpoints
- **WiFi Hotspot:** change AP name and password via the dashboard; credentials stored in NVS and survive firmware updates
- **OTA firmware updates from GitHub releases:** check for updates and install directly from the dashboard, with beta channel toggle
- **Status badges** ("saved" / "firmware default") on WiFi Hotspot and WiFi Internet cards, plus an info icon explaining NVS persistence
- **Settings Backup / Restore:** export all persistent settings (AP, WiFi Internet, CAN pins, beta flag) as JSON and restore in one go — disaster recovery for full-erase or cross-device migration

### Note
- WiFi credentials have been OTA-safe since the original 2.1 series; the badges and backup feature make that explicit and add recovery paths for full-flash-erase scenarios.

## [2.1.0-beta.5] - 2026-04-15

### Added
- Visible "saved" / "firmware default" status badge on the WiFi Hotspot and WiFi Internet cards
- Info icon on WiFi Hotspot card explaining that credentials are stored in NVS and survive firmware updates
- Settings Backup card: export all persistent settings (AP, WiFi Internet, CAN pins, beta flag) as JSON for safekeeping or migration to another device
- Settings Restore: upload a previously exported JSON to restore all persistent settings in one go
- New endpoints: /settings_export (GET), /settings_import (POST)

### Note
- WiFi credentials were already OTA-safe in prior versions; these changes make the persistence explicit in the UI and add disaster-recovery via backup file

## [2.1.0-beta.4] - 2026-04-15

### Added
- Runtime-configurable CAN TX/RX pins: configure GPIO pins for the TWAI transceiver directly from the dashboard
- Pin configuration is persisted in NVS so it survives OTA firmware updates
- New "CAN Pins" dashboard card with validation (GPIO 0-39, TX != RX, GPIO 6-11 blocked for SPI flash) and reboot flow
- New endpoints: /can_pins (GET/POST)

### Fixed
- OTA updates no longer risk breaking CAN communication on boards with custom pin configurations — once pins are configured via the dashboard they persist across updates

## [2.1.0-beta.3] - 2026-04-15

### Added
- Plugin info icon: click the (i) next to "Plugins" to see an inline explanation of what plugins are and how they work, with a link to the documentation and examples
- Footer community links: GitHub repository and Discord invite are now linked in the dashboard footer

## [2.1.0-beta.2] - 2026-04-15

### Changed
- Rebrand: renamed all UI references from "ADUnlock" to "ev-open-can-tools"
- Footer now shows current firmware version and dynamically adapts to the device IP
- Removed hardcoded hardware references from footer

### Added
- WiFi network scanner: scan and display available networks in the dashboard, select by clicking
- Signal strength indicators (RSSI) and channel info for each scanned network
- Static IP configuration: optionally set IP, gateway, subnet mask and DNS server
- OTA firmware update from GitHub releases: check for updates and install directly from the dashboard
- Beta channel toggle: switch between stable and pre-release firmware versions
- Firmware version auto-injected from VERSION file at build time
- Dedicated WiFi status endpoint (/wifi_status) and scan endpoint (/wifi_scan)
- WiFi hotspot configuration: change AP name and password via the dashboard
- New update endpoints: /update_check, /update_install, /update_beta
- New AP endpoints: /ap_config, /ap_status

## [2.0.0] - 2026-04-14

### Added
- Plugin system: install CAN frame modification rules as JSON files via web dashboard
- Plugin Manager UI card with install from URL, file upload, enable/disable and remove
- Paste JSON (offline): install plugins by pasting JSON directly into the dashboard — no internet or file picker needed
- Plugin detail view: expandable rule inspector showing CAN IDs, mux values and all operations per plugin
- Conflict detection: warns with a visual indicator when plugin CAN IDs overlap with base firmware handlers
- WiFi STA mode (AP+STA) for internet access to download plugins
- Plugin engine with mux-aware matching and operations: set_bit, set_byte, or_byte, and_byte, checksum
- Automatic CAN filter merging for plugin-required IDs
- ArduinoJson v7 dependency for all ESP32 dashboard environments
- New API endpoints: /plugins, /plugin_upload, /plugin_install, /plugin_toggle, /plugin_remove, /wifi_config
- Plugin documentation with format reference, examples and CAN ID table (docs/plugins.md)

### Fixed
- Credential placeholder check in build script now matches platformio_profile.h defaults
- CI release job: firmware artifacts renamed to unique filenames to prevent upload conflicts

## [1.0.0] - 2026-04-10

First release
