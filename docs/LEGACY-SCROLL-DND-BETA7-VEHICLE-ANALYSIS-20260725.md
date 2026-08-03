# Legacy Scroll DND Beta.7 Vehicle Analysis

Date: 2026-07-25

## Result

- Legacy FSD activation recovered. Native `0x3EE` RX and modified TX are both present.
- Legacy Scroll DND did not run.
- The dashboard showed `ap_inactive`, no hands-warning context, and no native `0x3C2 mux1` cache.

## Capture Summary

| Capture | Duration | Rows | Logical bus 1 | Logical bus 2 | 0x399 RX | 0x3C2 RX | 0x3EE RX/TX |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 77 | 119.993 s | 22,798 | 0 | 22,798 | 256 | 0 | 198 / 182 |
| 78 | 119.989 s | 23,234 | 0 | 23,234 | 211 | 0 | 191 / 163 |
| 79 | 120.000 s | 24,157 | 0 | 24,157 | 221 | 0 | 182 / 166 |

All 70,189 recorded rows use logical bus 2. No logical bus 1 row is present.

## 0x399 Evidence

Capture 77:

- AP state 6: 256 frames
- hands state 1/2/3: 6 / 238 / 12

Capture 78:

- AP state 6: 211 frames
- hands state 1/2/3: 10 / 180 / 21

Capture 79:

- AP state 1/2/6: 22 / 26 / 173
- hands state 1/2/3/4: 60 / 132 / 20 / 9

The vehicle therefore supplied valid active-AP context and entered primary Scroll DND warning states 3 and 4. The UI remained at state 0 because `mcpDashOnFrame()` rejected `0x399` whenever `frame.bus == CAN_BUS_PARTY`.

## 0x3C2 Evidence

`0x3C2` is absent from all three captures. The recorder filter includes `3C2`, MCP2515 runs receive-all mode, and each recording lasted about 120 seconds. This is not the historical 2,000-frame truncation problem.

The current Scroll DND implementation requires a native idle mux-1 frame:

```text
(data[0] & 0x03) == 1
data[2] == 0
data[3] == 0
age <= 1000 ms
```

Without that frame the state machine must fail closed with `no_native_mux1`.

## Root Causes

1. Software source-bus bug: the `0x399` diagnostic/trigger hook incorrectly excluded the only controller carrying the frame.
2. Missing cross-bus input: no native `0x3C2` reached either recorded logical bus.
3. Historical UI labels are not reliable physical connector names. The same logical bus 2 carries working Legacy `0x3EE`, `0x399`, and `0x370`; physical CAN1 must not be inferred from the enum name.

## Implemented Beta.8 Correction

- Accept `0x399` from either controller while Legacy is active.
- Record raw AP state, hands state, source bus/driver, and frame age.
- Preserve the existing native `0x3C2 mux1` requirement.
- Keep synthetic/fixed-template `0x3C2` transmission disabled.

## Next Vehicle Capture

1. Confirm beta.8 shows `AP6`, hands transitions, and source `mcp2515`.
2. Expect the block reason to change from `ap_inactive` to `no_native_mux1`.
3. Verify the second physical CAN pair is connected to the T-2CAN TWAI channel and check whether logical bus 1 begins receiving frames.
4. With the steering wheel operated manually, run an all-ID or dedicated second-channel capture and search for `0x3C2` or a vehicle-specific replacement ID.
5. Do not enable a synthetic `0x3C2` fallback until native counter/checksum/payload evidence is available for this vehicle.
