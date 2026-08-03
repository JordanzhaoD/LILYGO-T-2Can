# Legacy Model X 0x045 Wheel Observation - beta.18

## Purpose

This stage identifies the native steering-wheel action contract on the target
2019 Model X before considering any transmit implementation. It is strictly
receive-only.

The Tesla S/X 2026.2 signal data maps `STW_ACTN_RQ` as follows:

| Network | CAN ID |
| --- | --- |
| Chassis / Body Front | `0x045` |
| Powertrain | `0x238` |
| Body | `0x292` |

The recorder prioritizes `0x045` and retains the two gateway aliases so the
physical X197 route can be established from vehicle evidence.

## Decoded observation fields

For an eight-byte `0x045` frame:

- `data[3] & 0x07`: `StW_Lvr_Stat` (`0` idle, `1` up, `2` down,
  `3` forward, `4` back)
- `(data[3] >> 4) & 0x03`: `StW_Cond_Psd` (`0` idle, `1` up,
  `2` down)
- `data[4] | data[5] << 8`: steering-wheel switch bit mask
- `(data[6] >> 4) & 0x0F`: rolling counter
- `data[7]`: native CRC byte

The exact physical control assigned to each field is not assumed. It must be
derived from isolated button and wheel actions on the target vehicle.

## Vehicle capture procedure

1. Park the vehicle and keep the existing CAN wiring unchanged.
2. Open CAN Tools, then Recorder.
3. Keep the beta.18 default filter and choose 60 seconds.
4. Start recording and wait five seconds without touching the wheel.
5. Move only the left volume wheel up once, then wait three seconds.
6. Move only the left volume wheel down once, then wait three seconds.
7. Press the left wheel once, then wait three seconds.
8. Move the left wheel left and right separately, with three-second gaps.
9. Stop, wait for Saved, and download both CSV and diagnostics JSON.
10. Repeat in a separate recording for the right wheel.

Analyze a capture with:

```bash
python3 scripts/analyze_legacy_wheel_045_capture.py /path/to/can_recording.csv
```

## Acceptance gate for a future sender

A transmit stage must not be implemented until a capture establishes all of:

- the native ID and physical bus;
- the idle payload and message period;
- the exact field transition for volume up and volume down;
- rolling-counter progression;
- CRC behavior from multiple native frames;
- no unexpected TX rows in this receive-only build.

Existing Legacy activation, smart speed offset, `0x370` diagnostics, and the
fail-closed `0x3C2` path are unchanged.
