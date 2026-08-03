# Legacy NAG beta.12 Vehicle and Upstream Analysis

## Inputs

- Vehicle captures `can_recording (85).csv` through `(88).csv`
- Upstream tag `hypery11/flipper-tesla-fsd@v2.16-beta.10`
- Upstream commit `ef2f18c5e94e4915b531db60cc8d9bd12ce9b88b`

## Vehicle Result

Captures 87 and 88 prove that beta.12 fixed the short-burst failure. The
parallel `0x370` overlay ran continuously without a vehicle fault:

| Capture | Duration | 0x370 RX | 0x370 TX | TX/RX | Median TX period | Valid TX checksum |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 87 | 119.997 s | 2,684 | 2,611 | 97.3% | 40 ms | 2,611 / 2,611 |
| 88 | 119.998 s | 2,345 | 2,311 | 98.6% | 40 ms | 2,311 / 2,311 |

The vehicle nevertheless entered DAS hands state 3 twice in each capture. The
state-3 episodes lasted approximately 8.45 and 6.15 seconds in capture 87, and
10.22 and 9.78 seconds in capture 88. They eventually returned to states 1/2,
so the path has a partial effect, but it does not prevent recurring warnings.

Captures 85 and 86 contain only 2,000 frames and about 7.7 seconds each. They
show AP state 1 and no `0x370` TX, so they are not valid NAG-effect trials.

## Density Is Not the Limiter

The working captures already send almost one modified frame for every native
frame. Increasing the send rate cannot materially improve coverage and would
increase duplicate-counter and arbitration risk. All TX frames used valid
checksums, the learned source bus, and a continuous native-follower chain.

## EPAS-Faithful handsOnLevel Finding

The upstream beta.8 release notes described deriving `handsOnLevel` from torque
magnitude. Beta.10 explicitly revoked that behavior after ground-truth HW3 and
HW4 captures showed that real EPAS keeps `0x370` byte 4 bits 7:6 at zero even
when the driver is holding the wheel. The final tagged implementation preserves
the complete native byte 4 and uses `0x399`/`0x39B` as the warning-state ladder.

This directly conflicts with local beta.12, where every transmitted frame
forced `handsOnLevel=1` while every native frame in captures 87/88 carried
`handsOnLevel=0`. Beta.13 therefore preserves native byte 4. It does not copy
upstream's 2048 raw zero; the local vehicle-established 2050 zero remains.

## Torque Shape Finding

The local support waveform alternates direction on a fixed 1.2-second period,
and the escalated waveform alternates every 0.8 seconds. That can cancel a
direction-sensitive hands-on integrator. Upstream Mode-C instead:

- waits two seconds in DAS state 2, then uses a bounded one-sided random walk;
- chooses the sign opposite the current `0x129` steering angle;
- waits one second in states 3/4/5, then ramps toward 2.1 Nm;
- preserves native `handsOnLevel` and byte-4 low bits.

The current captures did not record `0x129`, so direction-aware torque cannot
be validated from this dataset. Beta.13 adds `0x129` and the candidate `0x5D9`
hands-on-reason frame to the recorder filter. No higher torque is enabled.

## Next Evidence Run

Record 120 seconds with the focused hexadecimal filter:

```text
129,370,399,3EE,5D9
```

Produce two paired runs on the same route and speed:

1. Legacy NAG enabled, driver maintains normal safe control.
2. Legacy NAG disabled, driver supplies a clearly labelled natural steering
   torque response when the warning appears.

The comparison must establish steering-angle sign, native torque sign,
warning-clear latency, and whether `0x5D9` contains a useful reason transition
before any direction-aware output is enabled. Increasing beyond the existing
1.8 Nm limit is not justified by captures 85-88.
