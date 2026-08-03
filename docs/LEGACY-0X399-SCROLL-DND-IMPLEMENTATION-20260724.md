# Legacy 0x399 Scroll DND Implementation

Version: `4.3.0-beta.5`

## Implemented Runtime Path

```text
CAN1 0x399
  -> AP state = data[0] & 0x0F
  -> hands state = (data[5] >> 2) & 0x0F
  -> primary warning entry (3..6)
  -> require fresh CAN2 native 0x3C2 mux1 idle frame
  -> CAN2 data[2]: 01, 00, 3F, 00 at 100 ms
```

States 9 and 10 are counted but never trigger transmission. Clearing the warning cancels a pending reactive action.

## Gates

All gates must be open:

- CAN write master on
- defense master on
- `legacy_scroll_dnd` / legacy `dnd_volume` on
- effective handler is Legacy
- AP state is 3 through 6
- vehicle OTA inactive
- CAN2 driver available
- native idle `0x3C2 mux1` age no more than 1000 ms
- controller not auto-disarmed

The feature is default-off through the existing `def_dv` NVS default.

## Bounded Behavior

- four frames per action
- 100 ms between frames
- 2000 ms cooldown between actions
- maximum three actions per warning episode
- two consecutive TX failures cause automatic disarm
- toggling Legacy Scroll DND off and on clears automatic disarm

## Frame Contract

The controller only caches frames where:

```text
(data[0] & 0x03) == 0x01
data[2] == 0
data[3] == 0
```

The outgoing frame is cloned from that native idle frame. Reactive DND changes only `data[2]`. It does not synthesize a frame, advance a private counter, recalculate byte 7, or modify the speed-wheel byte.

## Independence

Legacy Scroll DND does not change or enable:

- cabin-camera configuration
- `0x3FD` fields
- Legacy Continuous Absolute `0x370`
- speed-wheel DND
- lighting or gear features

The existing manual speed-wheel test remains separate and is never triggered by `0x399`.

## API Compatibility

`GET /defense_config` returns both names:

```json
{
  "dnd_volume": true,
  "legacy_scroll_dnd": true
}
```

`POST /defense_config` accepts either name. Existing dashboards and stored `def_dv` NVS values remain compatible.

`wheelDndDiag` reports configuration, armed state, AP/hands state, warning episodes, native mux1 evidence, sequence progress, cooldown, TX results, automatic disarm and block reason.

## Validation Completed

- native Wheel DND tests: 13 passed
- aggregate native matrix: 70 passed
- Python/API/UI contracts: 214 passed, 691 subtests passed
- dual-CAN production compilation: passed
- browser checks: desktop 1440x900 and mobile 390x844 passed

These results validate software behavior only. Vehicle acceptance of the cloned `0x3C2` frames remains unverified.

## First Vehicle Test

1. Disable Legacy Continuous Absolute NAG.
2. Enable defense master and Legacy Scroll DND only.
3. Record both buses for 120 seconds with `118,186,370,389,399,3C2,3EE,3FD`.
4. Confirm `nativeMux1IdleSeen=true` and `blockedReason=warning_clear` before driving.
5. During a warning, confirm `handsState=3` or `4`, `triggerCount` increments and four successful TX frames appear.
6. Stop testing immediately on CAN errors, AP/FSD degradation, camera warnings, unexpected volume/set-speed changes, or `autoDisarmed=true`.
