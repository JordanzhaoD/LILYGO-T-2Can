# Legacy Model X 0x045 Scroll DND Implementation Plan

## 1. Objective

Implement a production-capable Legacy NAG suppression path for the verified
2019 Model X by synthesizing the left scroll-wheel action that has already been
confirmed on the vehicle to clear an active NAG warning.

This is a new Legacy-specific path. It must not alter the vehicle-tested Legacy
FSD activation, smart speed offset, or EPAS `0x370` implementations.

Target release: `4.3.0-beta.19`.

## 2. Confirmed vehicle contract

The implementation is based on the two 60-second vehicle captures from
2026-07-27 and the owner's on-road result.

- Primary source and transmit bus: CAN1/TWAI.
- Native steering-wheel frame: `0x045`.
- CAN2 `0x292` is a gateway mirror and is observation-only.
- CAN2 `0x238` is not a usable wheel-action source.
- Native idle payload: `40 21 00 30 00 00 C0 CRC`.
- Counter: high nibble of byte 6, modulo 16; preserve byte 6 low nibble.
- Checksum: CRC-8/J1850 over bytes 0 through 6, poly `0x1D`, init `0xFF`,
  xorout `0xFF`; the CAN ID is not included.
- Left positive tick: switch-word bits 6..9 = `+1`, mask `0x0040`.
- Left negative tick: switch-word bits 6..9 = `-1`, mask `0x03C0`.
- All unrelated payload fields must remain identical to the newest native
  CAN1 `0x045` frame.
- A physical left-wheel movement during the NAG warning clears the warning.

## 3. Reference-source findings

### HW3-4-NAG-clean-20260712

The reference reads the hands-on state from `0x399`, treats states `3..6` and
`9..10` as warning states, caches a native wheel frame, and runs a balanced
scroll sequence. Its sequence is:

```text
+1 -> idle -> -1 -> idle
```

The implementation is non-blocking and separates the warning trigger from the
timed action service.

### FSD dual-CAN source

The reference `fire_3c2()` function advances the cached rolling counter,
recalculates the frame checksum, and stores the generated counter back into its
local cache. `0x399` warning state `>=3` starts a balanced volume sequence with
a cooldown.

### Public source

The public source uses the same architecture and explicitly notes that the
locally generated counter must be retained between generated frames. It also
uses a non-blocking four-step sequence at approximately 100 ms per step.

### Required adaptation

All three references target the newer `0x3C2` wheel contract. Their trigger and
state-machine design is reusable, but their payload fields and additive Tesla
checksum are invalid for this Legacy Model X. The local implementation must use
the confirmed `0x045` fields and CRC-8/J1850 contract above.

## 4. Proposed runtime architecture

Add a dedicated `DashLegacyWheel045Dnd` controller. Do not overload the current
`DashWheelDND`, which is built around CAN2 `0x3C2` mux1 frames.

```text
CAN1 native 0x045 idle ----> source cache + counter tracker -----+
                                                               |
CAN1 0x399 warning edge ----> episode state machine ------------+--> CAN1/TWAI 0x045 TX
                                                               |
CAN1/CAN2 recorder ----------> own-echo/mirror diagnostics -----+
```

The Legacy controller owns only:

- CAN1 `0x045` source caching and native-action detection;
- `0x399` AP/hands-on warning state;
- the balanced left-wheel sequence;
- generated-frame counter/CRC handling;
- own-echo, gateway-mirror, timeout, conflict, and TX diagnostics.

The existing `0x3C2` controller remains isolated for later HW3/HW4 use and must
not send while the Legacy `0x045` path is selected.

## 5. Trigger and gate policy

The Legacy scroll-DND switch is independent of the FSD-defense aggregate switch
and independent of the `0x370` slew-rate setting.

Required runtime gates:

1. CAN master enabled.
2. Effective handler is Legacy.
3. `legacy_scroll_dnd` is enabled and persisted in NVS.
4. Vehicle OTA guard is clear.
5. A fresh CAN1 `0x399` frame reports AP active (`3..6`).
6. Hands-on state enters `3..6` or `9..10`.
7. A native CAN1 `0x045` idle frame is no older than 150 ms.
8. TWAI is running and not bus-off/recovering.

No D-gear gate is required. AP state and warning state are the functional
context. CAN2 availability is not a gate.

## 6. Frame-generation contract

For each generated event frame:

1. Copy bytes 0..6 from the newest native CAN1 `0x045` idle frame.
2. Reject the source if DLC is not 8, CRC is invalid, the left/right wheel
   fields are non-idle, or the source is older than 150 ms.
3. Preserve every field except the left signed tick nibble and counter.
4. Clear only switch-word bits 6..9, then encode `+1` or `-1`.
5. Set byte 6 high nibble to `(nativeCounter + 1) & 0x0F`; preserve its low
   nibble.
6. Recalculate byte 7 with CRC-8/J1850 over bytes 0..6.
7. Send only through the primary TWAI driver on CAN1.
8. Store the complete generated payload and timestamp as an own-TX fingerprint.

Never generate or transmit `0x292` or `0x238`.

## 7. Balanced sequence

Use native idle frames as the release steps instead of transmitting synthetic
idle frames. This reduces a four-frame reference sequence to two generated
event frames:

```text
0x399 warning edge
  -> generated left +1 using fresh native idle counter C
  -> wait for the next valid native CAN1 0x045 idle
  -> generated left -1 using that native idle counter
  -> wait for the next valid native CAN1 0x045 idle
  -> pair complete
```

The resulting vehicle-visible behavior remains:

```text
+1 -> native idle/release -> -1 -> native idle/release
```

The inverse tick restores the one-step volume change. It must still be sent if
the warning clears after the first tick, unless AP exits, OTA starts, TWAI
fails, or a real driver wheel action is observed.

## 8. Parallel-bus counter strategy

The device is a parallel CAN participant, not an inline SCCM MITM. A generated
counter `C+1` can be followed by the SCCM's native idle frame with the same
counter. The implementation must classify this expected duplicate separately
from an actual counter discontinuity.

Rules:

- Cache native RX only; never advance the native cache from a TX callback.
- Ignore a received payload that matches the own-TX fingerprint within 30 ms.
- After TX counter `C+1`, accept a native idle with counter `C+1` as the
  expected source duplicate/release.
- Also accept the next native idle with counter `C+2` if the gateway or driver
  suppresses the duplicate.
- Abort if the next native counter is outside those two expected values, if the
  source CRC fails, or if no native idle arrives within 300 ms.
- A physical left/right wheel action always wins: abort the synthetic sequence
  and do not send the balancing tick against a driver's intentional input.

The diagnostics must report expected duplicates and true conflicts separately.

## 9. Warning retry policy

- Trigger immediately on the first transition into hands-on state `3..6` or
  `9..10`.
- Allow one balanced pair at a time.
- If the warning remains active 800 ms after a completed pair, allow another
  pair.
- Maximum three balanced pairs in one uninterrupted warning episode.
- End the episode when hands-on state returns to `0..2`, AP exits, or `0x399`
  becomes stale for more than 1 second.
- A later independent warning episode may trigger again.

This follows the reference projects' warning-driven retry behavior without a
free-running periodic sender.

## 10. Failure and abort behavior

Abort the active sequence on:

- Legacy mode exit or CAN master off;
- feature switch off;
- vehicle OTA active;
- AP exit or stale `0x399`;
- physical wheel input;
- stale/invalid native `0x045`;
- unexpected native counter progression;
- TWAI bus-off/recovery;
- TX failure.

Two consecutive TX failures auto-disarm runtime transmission until the user
turns the feature off and on again or restarts the module. NVS configuration
remains unchanged; runtime auto-disarm is reported separately.

## 11. API and UI changes

Keep the existing `/defense_config` API and NVS key for
`legacy_scroll_dnd`. Do not add another user-facing master gate.

Replace the current Legacy `0x3C2` diagnostics with:

- configured / runtime armed / auto-disarmed;
- CAN1 `0x045` native RX and valid-idle RX;
- source age, native counter, generated counter, and CRC;
- warning state, AP state, episode count, pair count, and retry count;
- sequence state: idle, positive-sent, wait-idle-1, negative-sent,
  wait-idle-2, complete, aborted;
- positive/negative TX success and failure;
- expected duplicate, true counter conflict, own echo, and `0x292` mirror
  confirmation counters;
- warning-clear latency from first positive TX;
- last blocked/abort reason.

UI, NVS, and actual runtime state must be displayed as separate values.

## 12. Implementation steps

1. Add `include/dash_legacy_wheel_045_dnd.h` with CRC, frame mutation,
   state machine, diagnostics, and no hardware dependencies.
2. Add native tests using payloads and CRC vectors from the 2026-07-27 vehicle
   captures.
3. Route CAN1 RX `0x045` and `0x399` into the new controller before the Legacy
   handler returns from `0x045` profile processing.
4. Add a CAN-task service function that transmits generated `0x045` only through
   the primary source driver and records TX evidence.
5. Disable the old Legacy-to-`0x3C2` mapping; keep the `0x3C2` implementation
   compiled but isolated.
6. Reuse the existing `legacy_scroll_dnd` API/NVS value and remove the CAN2 and
   defense-aggregate gates from this Legacy path.
7. Extend `/status`, `/defense_config`, recorder metadata, and both desktop and
   mobile diagnostics.
8. Add own-echo and CAN2 `0x292` mirror correlation without using either as a
   source frame.
9. Add API-contract and UI regression tests.
10. Run native tests, Python contract tests, minify the dashboard, build
    `lilygo_t2can_dual`, and package beta.19 full/OTA assets.

## 13. Required automated tests

- All captured `0x045` CRC vectors validate.
- Counter wrap `0xF -> 0x0` works and preserves byte 6 low nibble.
- Positive and negative generation modifies only bits 6..9, counter, and CRC.
- Non-wheel payload fields and byte 4 unrelated buttons remain unchanged.
- CAN2 `0x292` and `0x238` can never become TX candidates.
- Invalid CRC, active native wheel input, stale source, OTA, AP exit, and
  bus-off all block TX.
- Warning states `3..6` and `9..10` trigger; states `0..2` do not.
- The second event waits for a new native idle frame.
- Expected native duplicate and true counter conflict are distinguished.
- Own TX echo cannot overwrite the native source cache or retrigger an action.
- At most three balanced pairs occur in one warning episode.
- Legacy activation, smart speed offset, and `0x370` native tests remain
  unchanged and pass.
- Existing UI/API persistence regression suite passes.

## 14. Vehicle acceptance criteria

The implementation is considered successful only when a recorder capture and
UI diagnostics show all of the following during a real NAG event:

1. `0x399` warning edge detected on CAN1.
2. Positive and negative `0x045` event frames transmitted successfully.
3. Both frames have valid CRC and expected counters.
4. Native idle releases are observed after both event frames.
5. No TWAI error-counter growth, bus-off, SCCM warning, or driver-assistance
   fault is introduced.
6. Hands-on state returns to `0..2` and the visible NAG warning clears.
7. Audio volume returns to its original level after the balanced pair.

## 15. Merge boundary

Files expected to change:

- `include/dash_legacy_wheel_045_dnd.h` (new)
- `src/main.cpp`
- `include/web/mcp2515_dashboard.h`
- `include/web/mcp2515_dashboard_ui.src.h`
- generated `include/web/mcp2515_dashboard_ui.h`
- native and Python contract tests
- `VERSION`, `CHANGELOG.md`, build metadata, and beta.19 release documents

Do not refactor unrelated handlers or restore discarded beta.2-beta.9 behavior.
Do not change Legacy activation, smart-speed, `0x370`, recorder streaming, DNS,
network, or mobile-navigation behavior except where diagnostics must display the
new `0x045` path.
