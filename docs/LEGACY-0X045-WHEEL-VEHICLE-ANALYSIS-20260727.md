# Legacy Model X 0x045 Wheel Vehicle Analysis - 2026-07-27

## Capture provenance

- Vehicle: 2019 Model X, Legacy architecture
- Vehicle software: 2026.2.11
- Firmware: LILYGO T-2CAN `4.3.0-beta.18`
- Left capture SHA256: `70105d359916f424cd44af27124b12bbc1eef2a3302a79bff569b4e341f5a7f4`
- Right capture SHA256: `680d8a9d07e2fac55de0273ed30c01399c30a00a62dabfd789d383d37e0ade0e`
- Both captures are 60 seconds and contain no `0x045`, `0x238`, or `0x292` TX/error rows.

## Confirmed bus topology

| Logical bus | CAN ID | Frames | Finding |
| --- | --- | ---: | --- |
| CAN1 | `0x045` | 676 left / 682 right | Primary live steering-wheel stream |
| CAN2 | `0x292` | 602 left / 631 right | Near-synchronous gateway copy of `0x045` |
| CAN2 | `0x238` | 454 left / 355 right | Constant PT representation; no captured wheel actions |

The `0x045` and `0x292` payloads are identical around wheel actions, normally
about one millisecond apart. The current parallel T-2CAN connection is not an
inline MITM between the SCCM and a receiver.

## Confirmed payload contract

The idle `0x045` payload is:

```text
40 21 00 30 00 00 C0 CRC
```

`C` is the rolling counter in the high nibble of byte 6. It advances modulo 16.
The native period is approximately 100 ms, with additional event frames during
wheel operation.

CRC byte 7 is CRC-8/J1850 over bytes 0 through 6:

- polynomial: `0x1D`
- initialization: `0xFF`
- xorout: `0xFF`
- no CAN-ID bytes in the CRC input

Every unique captured `0x045`/`0x292` payload satisfies this CRC contract.

## Confirmed wheel fields

Treat bytes 4 and 5 as a little-endian 16-bit switch word.

| Physical control | Switch word | Encoding |
| --- | ---: | --- |
| Left wheel positive tick | `0x0040` | signed nibble at bits 6..9 = `+1` |
| Left wheel negative tick | `0x03C0` | signed nibble at bits 6..9 = `-1` |
| Left wheel press | `0x0002` | bit 1 |
| Left wheel side A | `0x0001` | bit 0 |
| Left wheel side B | `0x0004` | bit 2 |
| Right wheel positive tick | `0x0400` | signed nibble at bits 10..13 = `+1` |
| Right wheel negative tick | `0x3C00` | signed nibble at bits 10..13 = `-1` |
| Right wheel press | `0x0010` | bit 4 |
| Right wheel side A | `0x0008` | bit 3 |
| Right wheel side B | `0x0020` | bit 5 |

The user action order was not precisely annotated, so `positive` and `negative`
are confirmed wire directions. Which one is visually up/down can be confirmed
with a shorter timestamped capture if needed.

## NAG feasibility conclusion

The frame format and physical controls are confirmed. The vehicle owner has
also completed an independent on-road physical-control test and confirmed that
moving the **left wheel** while the NAG warning is visible clears the warning.
This establishes the left wheel as the only current vehicle-accepted target;
the right wheel is not required for the proposed path.

The remaining unknown is synthetic-frame acceptance on the current parallel
CAN topology. Before reactive warning-triggered use, conduct a bounded manual
one-shot sender test:

1. Keep the feature default-off and explicitly arm a stationary test action.
2. Send one left-wheel positive tick derived from a fresh native CAN1 `0x045`.
3. Confirm the expected one-step volume response and inspect counter conflicts,
   CAN errors, SCCM/DAS warnings, and the following native frames.
4. Repeat with one negative tick, then verify the volume returns to baseline.
5. Capture `0x045`, `0x292`, `0x399`, and `0x370` throughout the test.

Only after this passes should the already-verified physical left-wheel action
be connected to the NAG warning trigger.

## Recommended implementation if physical acceptance passes

Prefer the left volume wheel because an opposite tick can restore the volume.
The bounded sequence should be based on fresh native CAN1 `0x045` frames:

```text
warning edge -> +1 tick -> native idle observed -> -1 tick -> native idle observed
```

Requirements:

- Legacy mode, explicit feature switch, CAN master, AP active, and fresh `0x399` warning edge;
- fresh CAN1 native `0x045` no older than 150 ms;
- copy bytes 0..6 from the newest native frame and modify only bits 6..9;
- derive the candidate counter from the latest native counter and recalculate CRC-8/J1850;
- no periodic free-running sender and no `0x292` dual-bus duplicate injection;
- one balanced pair per warning episode, at most three pairs per drive cycle;
- abort on warning clear, AP exit, OTA, bus error, stale source, counter conflict, or TX failure;
- expose RX, candidate TX, conflict, warning-clear latency, and auto-disarm diagnostics.

## Counter-collision limitation

The current hardware is connected in parallel. It cannot suppress the SCCM's
next native frame, so a synthetic `counter+1` frame may be followed by a native
frame with the same counter and different payload. This must be tested as a
single stationary/manual action before reactive NAG use. A true inline MITM
that replaces rather than races the source frame is the robust architecture.
