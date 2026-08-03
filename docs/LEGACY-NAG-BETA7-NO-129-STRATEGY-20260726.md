# Legacy NAG beta.7 and No-0x129 Strategy

> Superseded for the target 2019 Model X on vehicle software 2026.2.11 by
> `4.3.0-beta.15`. At the user's direction, beta.15 implements the beta.7
> directionless mean-reverting random walk without waiting for `0x129`.

## Upstream Evidence

Source reviewed:

- repository: `hypery11/flipper-tesla-fsd`;
- tag: `v2.16-beta.7`;
- commit: `dedb996d66e7e1e6893646cdc699fbbf7d441c2a`;
- release date: 2026-06-15 UTC.

Beta.7's experimental EPAS-faithful path does not use steering angle or
`0x129`. It was reconstructed from a 22,598-frame capture of a third-party
device that appeared to suppress the warning.

For every admitted native `0x370` frame, beta.7:

1. requires the regular NAG switch;
2. skips native `handsOnLevel == 1`;
3. skips DAS hands states 0 and 8;
4. generates a mean-reverting random walk around raw torque 2050;
5. clamps the walk to raw 1780-2500, approximately -2.7 to +4.5 Nm;
6. preserves the complete native byte 4 in faithful mode;
7. copies the other native fields, advances the low counter nibble by one, and
   regenerates the Tesla additive checksum.

The faithful branch ignores steering angle and does not use the normal
3-5-frame grip-pulse torque. Its only hands-on hypothesis is that a live,
varying, near-zero torque distribution looks like genuine EPAS traffic.

## Why Beta.7 Is Not the Final Answer

The beta.8 release explicitly says beta.7's first implementation was too naive
and did not hold on 14.x. Beta.8 replaced it with the `0x399` demand-state
machine and used `0x129` to keep torque on one side, opposite steering angle.

The local vehicle evidence is consistent with that failure:

- local beta.14 already preserves native byte 4;
- counter/checksum and TX coverage are already correct;
- current positive/negative torque alternates inside the warning window;
- the average synthetic torque over each episode remains near zero;
- state 3 continues to recur approximately every 54 seconds.

Before the vehicle-specific beta.15 decision, copying beta.7 was assessed as
changing deterministic oscillation into stochastic
oscillation, but it would still lack a persistent direction. It would also
expand beyond the local validated +/-1.8 Nm boundary. It is therefore not a
recommended active fallback.

## Why 0x129 Is Currently Missing

Captures 89-93 contain only logical Bus 2 records. They contain no logical
Bus 1 rows at all, not merely zero `0x129` rows. The current recorder's ID list
is a software recording filter after driver reception; it is not proof that
the other controller or physical bus carries no `0x129` traffic.

For the supplied vehicle context, `0x370` is expected on the Chassis-CAN tap.
`0x129` may live on another native network and may not be gateway-forwarded to
that tap. Model 3/Y X179 pin assignments must not be applied to a 2019 Model X
connector without vehicle-specific confirmation.

## Phase A: Locate 0x129 Before Changing Torque

Implement a receive-only `0x129 locator` recorder mode:

- disable all CAN injection while active;
- put both TWAI and MCP2515 controllers in listen-only/receive-only mode;
- install a hardware acceptance filter for standard ID `0x129` on each
  controller, rather than relying only on the CSV filter;
- expose independent CAN1/CAN2 RX counts, last payload, interval, DLC, and
  receive-error counters;
- record the physical-controller identity without translating it through the
  historical logical bus labels;
- restore normal controller filters only after the locator stops.

Run the locator on each electrically verified CAN pair available at the
vehicle connector. Use Tesla Service Mode CAN Port information when available,
and confirm the pair with an oscilloscope before moving a transceiver. An
external dual-channel SocketCAN logger is the preferred independent check.

The first capture should request only `0x129`. Once it is visible, record
`129,370,399,488,5D9` while clearly marking center, left, center, and right
wheel positions.

## Phase B: If 0x129 Is Truly Unavailable

Use the following fallbacks in order. Do not jump directly to an unrestricted
fixed-polarity sender.

### B1: Validate 0x488 as a Direction Source

`0x488` is visible in all five captures at high rate. It represents the DAS
steering request, not the physical steering-angle sensor, so it is only a
candidate.

First add read-only diagnostics:

- decoded request angle and control type;
- freshness and source controller;
- sign changes and timestamps;
- correlation with physically labelled left/right manoeuvres.

If repeated captures prove that the decoded sign matches vehicle turn
direction during active AP, use fresh `0x488` only while AP steering control is
valid. Do not use stale, invalid, or zero-control-type data.

### B2: Straight-Road-Only Fixed Polarity

If no validated direction frame exists, a controlled fixed-polarity experiment
is possible only as a vehicle-specific calibration, not as a general mode:

- default off and separately armed;
- retain raw zero 2050 and the +/-1.8 Nm limit;
- perform separate short positive and negative trials on the same straight,
  empty route;
- use `0x488` only as a curve/invalidation gate, not as a direction source;
- stop sending when the absolute steering request exceeds a conservative
  threshold, AP context is stale, or any TX/bus error appears;
- store the selected polarity as vehicle-specific experimental calibration;
- never change polarity within one warning episode.

Compare state-3 recurrence interval and clearing time for both signs. A sign is
accepted only if it improves repeated trials without steering, EPAS, camera,
or body-control faults.

### B3: Native-Torque Sign as a Hold-Only Hint

The native `0x370` torsion-bar torque can be observed, but near-zero sensor
offset and noise make it unsuitable as the primary sign source. It may only be
used to retain a previously established human torque sign when magnitude and
freshness exceed validated thresholds. If no valid sign exists, do not inject.

### Rejected Fallbacks

- unrestricted beta.7 random walk: upstream negative result and excessive
  torque range;
- alternating positive/negative waveform: current captures show cancellation;
- treating `0x488` as steering angle without labelled correlation;
- deriving direction from the unconfirmed `0x5D9` payload;
- increasing TX density beyond the current near-native coverage;
- forcing or deriving `handsOnLevel` instead of preserving native byte 4.

## Recommended Development Order

1. Add the receive-only dual-controller `0x129 locator` and verify recorder bus
   attribution.
2. Capture each verified physical CAN pair with injection disabled.
3. If `0x129` remains absent, add read-only `0x488` decode/correlation
   diagnostics.
4. Only after labelled left/right evidence, implement a default-off direction
   provider with priority `fresh 0x129 -> validated 0x488 -> no output`.
5. Keep fixed-polarity calibration as a separately labelled vehicle-specific
   experiment, not an automatic fallback.

This order preserves the useful beta.7 finding--native byte 4 and genuine
counter/checksum--without repeating its unvalidated directionless torque model.
