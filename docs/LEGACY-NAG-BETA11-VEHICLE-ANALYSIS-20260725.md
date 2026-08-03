# Legacy NAG beta.11 Vehicle Analysis - Captures 82-84

## Result

The torque path reached the correct live controller and transmitted valid
`0x370` overlays, but beta.11 treated the vehicle's expected next native frame
as a counter collision. It therefore degraded after only 3-13 transmitted
frames. Scroll DND could not start because no native `0x3C2` was visible on
either recorded logical bus.

## Capture Summary

| Capture | Duration | Frames | Bus 1 frames | Bus 2 frames | 0x370 RX/TX | 0x399 RX | 0x3C2 RX/TX |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 82 | 104.145 s | 24,043 | 0 | 24,043 | 2,330 / 4 | 201 | 0 / 0 |
| 83 | 119.993 s | 27,757 | 0 | 27,757 | 2,674 / 6 | 237 | 0 / 0 |
| 84 | 120.001 s | 27,882 | 0 | 27,882 | 2,728 / 13 | 232 | 0 / 0 |

All observed `0x370`, `0x399`, and `0x3EE` traffic was labelled logical bus 2.
This is consistent with the working Legacy activation path and proves that the
dashboard's CAN1/CAN2 names must not be treated as connector truth.

## Torque Frame Evidence

All 23 transmitted `0x370` frames had:

- valid additive checksum;
- raw torque `2230`, which is `2050 + 180` or `+1.8 Nm`;
- `handsOnLevel = 1`;
- the same controller and bus as the native source frame;
- rolling counter equal to native counter plus one.

For almost every TX, the vehicle emitted a valid same-counter frame with a
different payload about 39-40 ms later. A few samples arrived around 79-80 ms
with the following counter, indicating a missed or suppressed intermediate
sample. The 39-40 ms frame is expected native continuation of the source ECU,
not an exact loopback of the module TX.

## Root Cause

beta.11 correctly distinguished exact Own-Echo by full payload, but still
classified every valid same-counter/different-payload native continuation as
`counter_collision`. The three-strike protection then auto-disarmed the sender.
That limited output to short bursts and made torque NAG ineffective.

## beta.12 Correction

- Exact bus, DLC, counter, and payload match within 100 ms remains Own-Echo.
- A valid-checksum, different-payload frame with the TX counter arriving
  20-100 ms later is an expected native follower.
- An expected follower is allowed to drive the next counter overlay, restoring
  continuous frame-synchronous output.
- Same-counter/different-payload frames inside 20 ms, or invalid-checksum
  followers, remain immediate collisions and retain three-strike degradation.
- TX failures, AP context freshness, bus lock, Legacy mode, and explicit user
  enable remain mandatory.

## Scroll DND

Across 344 seconds and 79,682 recorded frames, neither logical bus exposed a
single `0x3C2`. The `0x399` warning source was present on bus 2, including AP=6
and hands states 2/3, but without a native `0x3C2 mux1` base frame the current
Scroll DND correctly fails closed. No synthetic fallback is enabled.

## Vehicle Validation Required

beta.12 changes counter scheduling behavior and is not vehicle-verified. The
next test should record `0x370,0x399,0x3C2,0x3EE` for 120 seconds and confirm:

1. `expectedNativeFollowerObserved` rises continuously;
2. `counterCollisionObserved` remains zero;
3. TX cadence is close to native `0x370` cadence rather than 3-13 frames total;
4. no steering, body-control, CAN error, or driver-assistance fault appears;
5. disabling the Legacy NAG switch stops TX immediately.
