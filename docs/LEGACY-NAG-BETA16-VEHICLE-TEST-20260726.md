# Legacy NAG beta.16 Vehicle Test

## Scope

- Firmware: `4.3.0-beta.16`
- Vehicle: 2019 Model X, software 2026.2.11
- Path under test: Legacy `0x370` Continuous Absolute with receive-only
  `0x488 DAS_steeringControl` direction input
- Existing Legacy FSD activation and smart speed offset are regression checks,
  not changed features.

## What Changed

The beta.15 directionless random walk produced valid frames with high TX
coverage, but the torque remained near zero and changed polarity frequently.
Beta.16 uses valid type-1 `0x488` requested steering angle to choose one torque
side opposite the requested angle. State 2 waits two seconds and then applies
0.8-1.8 Nm. States 3-5 wait one second and ramp toward 2.1 Nm. Native `0x370`
byte 4, counter progression, and checksum behavior are unchanged.

## Pre-Drive Checks

1. Confirm the dashboard reports version `4.3.0-beta.16`.
2. Leave Legacy NAG off and verify Legacy FSD activation and smart speed offset.
3. Start a 120-second recording with CAN1 and CAN2 enabled.
4. Enable Legacy NAG only after `rx488` is increasing.
5. Confirm `steeringControlType=1` and `steeringAgeMs` normally remains below
   250 ms. `steering_missing`, `steering_invalid`, or `steering_stale` means the
   torque path must remain stopped.

## Acceptance Evidence

Record at least one baseline run with Legacy NAG off and two comparable runs
with it on. Capture the whole interval from AP/FSD engagement through the first
warning and its clearing.

- `rx370`, `rx488`, and `tx370` increase during active FSD.
- `episodeDirection` is `-1` when decoded `0x488` angle is positive and `+1`
  when the angle is negative.
- `requestedTorqueCentiNm` and `targetTorqueCentiNm` remain on the selected side
  except during a slew-limited direction transition.
- Native TX byte 4 remains equal to the source RX byte 4.
- TX counter remains native counter + 1 and additive checksum remains valid.
- `counterCollisionObserved`, `txFail`, and `autoDisarmed` remain zero/false.
- Compare hands state-3 recurrence interval and clearing time against the
  baseline. A higher TX count alone is not proof of NAG suppression.

## Immediate Stop Conditions

Disable Legacy NAG and stop the drive if steering assistance changes, an EPAS
warning appears, body/camera controls become unavailable, CAN errors rise, or
the dashboard reports `degraded`, `counter_collision`, or `tx_failed`. Preserve
the CSV and `/status` or `/defense_config` diagnostics before restarting the
module.

## Data Needed After Test

- The complete 120-second CSV files, including RX and TX on both CAN channels.
- A dashboard screenshot while the warning is present.
- Approximate timestamps for FSD engagement, first warning, warning clearing,
  disengagement, and any vehicle fault.
- Whether the test was a straight road, left curve, or right curve. This is
  needed to validate the `0x488` direction relationship.
