# Legacy NAG beta.15 Vehicle Analysis

## Inputs

- Vehicle: 2019 Model X
- Vehicle software: 2026.2.11
- Firmware: `4.3.0-beta.15`
- Captures: `can_recording (94).csv` through `(96).csv`
- Active model: upstream beta.7-style directionless mean-reverting random walk

Each capture is approximately 120 seconds. All recorded traffic is labelled as
logical Bus 2.

## 0x370 Transmission Contract

| Capture | RX | TX | TX/RX | TX torque mean | TX torque stddev | TX checksum | byte4/counter pairing |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 94 | 2,097 | 1,778 | 84.8% | -0.11 Nm | 0.71 Nm | 1,778/1,778 | 1,778/1,778 |
| 95 | 1,780 | 1,663 | 93.4% | -0.01 Nm | 0.62 Nm | 1,663/1,663 | 1,663/1,663 |
| 96 | 1,993 | 1,876 | 94.1% | 0.00 Nm | 0.64 Nm | 1,876/1,876 | 1,876/1,876 |

For every TX paired with the preceding native RX within 100 ms:

- byte 4 is identical;
- counter is native counter +1;
- bytes 0, 1, and 5 are identical;
- additive checksum is valid.

The firmware path is therefore functioning as implemented. The failure is not
caused by byte4 corruption, checksum, counter construction, or a dead sender.

## Torque Distribution

The configured clamp is -2.7 to +4.5 Nm, but mean reversion keeps almost all
frames near zero:

| Capture | Absolute torque >=1.0 Nm | >=1.5 Nm | >=2.0 Nm | observed min/max |
| --- | ---: | ---: | ---: | --- |
| 94 | 16.8% | 3.6% | 0.2% | -2.11/+2.10 Nm |
| 95 | 10.5% | 1.8% | 0.1% | -1.83/+2.05 Nm |
| 96 | 11.8% | 1.5% | 0.1% | -1.81/+2.16 Nm |

Only roughly one frame in 6-10 exceeds 1 Nm, and almost no frame exceeds 2 Nm.
Torque crosses zero frequently and has no relationship to steering direction.
The nominal clamp therefore overstates the effective hands-on stimulus.

The beta.7 xorshift step of +/-0.4 Nm and 1/16 pull toward zero produces a
stationary standard deviation near 0.7 Nm. That is materially below the 1.48 Nm
standard deviation claimed for the original reference capture.

## DAS Warning Timing

`DAS_handsOnState` is decoded from `0x399` byte 5 bits 5:2.

| Capture | State-3 durations | State-3 start intervals |
| --- | --- | --- |
| 94 | 3.65, 7.71 s | 54.19 s |
| 95 | 5.42, 9.21, 4.59, 1.46, 2.66, 2.57 s | 18.48, 20.72, 23.28, 13.62, 15.81 s |
| 96 | 1.59, 3.00, 3.21, 3.29, 3.02, 1.80 s | 20.98, 27.06, 20.01, 16.71, 16.34 s |

Capture 94 reproduces the approximately 48-57-second full attention timer.
Captures 95 and 96 show that after a state-3 episode the system often does not
fully reset: warning waves return every 13-27 seconds. The random walk has not
only failed to eliminate the timer; in these runs it produced repeated shorter
warning cycles.

TX remains active during state 3. State-3 torque is still centred near zero and
usually contains few frames above 1 Nm. This rules out a simple trigger-timing
failure.

## Why Beta.7 Failed

The upstream issue #100 record contains an important caveat that was not part
of the release headline: the 22,598-frame stream used to infer beta.7 could not
be proven to be the commander's injected stream rather than native EPAS. The
upstream maintainer later described beta.7 as a naive flat centred-zero walk
and replaced it after a negative vehicle test.

The target Model X result now independently confirms that negative result on a
bus where `0x370` is definitely visible and transmitted. A centred-zero random
walk looks active on a CAN trace, but it does not provide sustained directional
torsion-bar torque. The vehicle's attention integrator continues to count
toward state 3.

The 48-57-second period is therefore the vehicle's normal hands-off timer, not
a firmware scheduler period.

## Available Direction Source

Although `0x129` remains absent, `0x488 DAS_steeringControl` is abundant:

| Capture | 0x488 RX | Valid control type 1 | decoded angle range |
| --- | ---: | ---: | --- |
| 94 | 4,360 | 4,065 | -9.65 to +7.95 degrees |
| 95 | 3,551 | 3,551 | -2.55 to +8.35 degrees |
| 96 | 3,832 | 3,832 | -15.75 to +9.05 degrees |

The decode used is the upstream signal definition:

```text
controlType = byte2[7:6]
rawAngle = ((byte0 & 0x7F) << 8) | byte1
angleDeg = rawAngle * 0.1 - 1638.35
```

For fresh type-1 frames with absolute request angle >=0.5 degrees, the current
random torque was opposite the requested angle only 52.5%, 49.7%, and 56.7% of
the time. That is effectively random polarity.

`0x488` is a DAS requested angle rather than a physical steering-wheel sensor,
but during active AP it is the best continuously available direction signal in
the current topology.

## Recommended Solution: Model-X 0x488 Mode-C

Replace the beta.7 flat random walk with a default-off, vehicle-specific
demand-state engine:

1. Preserve raw zero 2050, complete native byte4, counter+1, checksum, same-bus
   routing, Own-Echo fingerprinting, and all TX fault gates.
2. Use only fresh `0x488` control type 1 as the direction source. Torque sign is
   opposite the requested angle. Stale/invalid `0x488` fails closed.
3. Apply a center deadband. Inside the deadband, retain the last validated sign
   for the current warning episode; if no sign exists, do not transmit.
4. DAS states 0 and 8: stop immediately and clear the episode.
5. DAS state 1: hold the last value for at most 500 ms, then stop so the vehicle
   can register a completed warning reset.
6. DAS state 2: after a short qualification period, use a one-sided organic
   walk in the 0.8-1.8 Nm band. Never cross zero within an episode.
7. DAS states 3/4/5: ramp on the same side to 1.8-2.1 Nm over 500 ms and hold
   with small bounded variation until the state falls.
8. Do not derive or force handsOnLevel. The target vehicle evidence supports
   preserving native byte4.

This combines the useful parts of upstream Mode-C with the signal actually
available on this Model X. It addresses the measured failure directly: torque
becomes sufficiently dense, one-sided, and direction-correlated instead of
merely increasing the nominal random-walk clamp.

## Required Staged Validation

Before active `0x488` direction control, add a read-only shadow diagnostic that
records decoded angle, control type, freshness, proposed sign, and proposed
torque. Perform a labelled center/left/center/right capture to confirm the sign
convention.

Then run paired 120-second trials on the same route:

1. beta.15 random-walk baseline;
2. 0x488 Mode-C enabled.

Acceptance requires:

- no state-3 entry in a complete 120-second run, or a clear recurrence increase
  beyond 120 seconds;
- no repeated 13-27-second warning waves;
- no EPAS/body/camera fault;
- TX checksum and byte4 preservation remain 100%;
- stale or invalid 0x488 produces no active torque.

Do not solve this by increasing random-walk amplitude, increasing TX density,
forcing handsOnLevel, or writing `0x399`. Those changes do not address the
measured zero-mean/direction mismatch and increase fault risk.
