# P4 Stability Baseline — bus_mask staged rollout (v0.1)

Date: 2026-11-03. Approved route: Jordan picked "A·减法基线，逐级点亮" after ten
forensics rounds concluded the crash family is platform-level (3-bus TWAI interrupt
storm + hosted) register/control-flow corruption, with the application layer —
including the blackbox recorder — exonerated by experiment L.

## Intent

Ship a car-stable P4 firmware NOW by running the board in the proven-stable
T2CAN-equivalent configuration (one active CAN bus), then re-enable the P4-only
buses one per release, validating each level in the car. Every level is a usable
firmware, not an experiment.

## Goals

- Baseline build boots with CanA active only; CanB/CanC compiled in but not
  constructed (their ISRs never install → interrupt regime equals T2CAN's).
- The enabled set is an NVS-persisted bitmask, changeable via the serial DIAG
  console, applied at boot (v0.1 does not construct/destruct drivers at runtime).
- Driving features, dashboard, hosted networking, and CanA-lane recording all
  work in the baseline.

## Non-goals (v0.1)

- No filter/drain/ISR-priority/IDF changes (reserved as per-level levers if a
  later level crashes).
- No UI toggle (DIAG only); no runtime driver hot-plug.
- T2CAN dual build behavior unchanged (mask is P4-3CAN-only via ifdef).

## Design

- NVS key `p4busmask` (u8), bits: 0=CanA (forced on), 1=CanB, 2=CanC.
  Default when absent = 0x01 (baseline).
- `t2canSetupSecondary()` / `t2canSetupCan3()` skip construction when their bit
  is clear; existing null-guards in the drains, TX helper, and health tick make
  the rest of the system no-op safely. Heap-guard slots adopted as null at boot
  stay null → stable, no trip.
- main.cpp exports `t2canBusMaskGet()/t2canBusMaskSet()/T2CAN_BUS_MASK_DEFAULT`;
  DIAG gains `bus_mask [1|3|5|7]` (get / set + "reboot to apply").
- Recorder v2 keeps running; CanB/CanC lanes simply record zero frames while
  disabled.

## Approaches considered

- A (chosen): subtract to T2CAN-equivalent, light up levels one per release —
  smallest change, fastest usable firmware, clean per-level attribution.
- B: fix tri-bus stability in one batch (filters+drain+ISR+IDF) — rejected: 4-5
  variables at once, no attribution if it still crashes.
- C: A plus an espressif issue in the background — adopted as optional follow-up
  on my time only, never blocking Jordan.

## Verification

- Native tests for mask decode/persist logic; pytest contract suite must stay
  green (dashboard logic untouched); gates: pytest → pio_3can build, serial.
- On-board: boot log states disabled buses explicitly.
- Field success = ≥30 min drive, zero crash, resets ledger clean, CanA features
  and recording usable. Then level-up: 0x05 (CanC) → 0x07 (full), one car run
  each; a crashing level gets the reserved levers as its focused fix.
