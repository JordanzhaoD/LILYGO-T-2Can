# Legacy 0x3C2 Reference-Firmware Alignment Deployment

Version: `4.3.0-beta.9`

## Scope

This release aligns the local dual-CAN dashboard with the observable behavior of `NAG-lilygo-webui-16MB-20260724.bin`:

- `0x399` warning transition is the trigger.
- `dnd_speed` selects `right_scroll`.
- The action uses a fresh native `0x3C2 mux1` frame and changes only the speed-wheel field.
- The sequence is `0x01 -> 0x00 -> 0x3F -> 0x00`, 100 ms per step.
- Missing native evidence, stale cache, CAN2 offline, OTA, warning clear, TX failure or feature disable aborts the action.

The reference binary was not treated as a source of unproven fixed payloads. No synthetic `0x3C2` frame is sent when the vehicle has not exposed a native frame.

## Artifacts

Directory:

`firmware-assets/v4.3.0-beta.9_20260725-0313/`

- `LILYGO-T-2CAN_v4.3.0-beta.9_full_merged_20260725-0313.bin`: full flash image, offset `0x0`
- `LILYGO-T-2CAN_v4.3.0-beta.9_ota_app_20260725-0313.bin`: OTA application image only
- `bootloader_20260725-0313.bin`
- `partitions_20260725-0313.bin`
- `ota_data_initial_20260725-0313.bin`
- `SHA256SUMS.txt`
- `BUILD_INFO.md`

Verify before use:

```sh
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware
shasum -a 256 -c firmware-assets/v4.3.0-beta.9_20260725-0313/SHA256SUMS.txt
```

## Full Flash Deployment

Use the full image only for a complete flash. Confirm the serial port and disconnect the device from the vehicle CAN harness for the first boot.

```sh
esptool.py --chip esp32s3 --port /dev/cu.usbmodemXXXX erase_flash
esptool.py --chip esp32s3 --port /dev/cu.usbmodemXXXX \
  --baud 460800 write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 firmware-assets/v4.3.0-beta.9_20260725-0313/LILYGO-T-2CAN_v4.3.0-beta.9_full_merged_20260725-0313.bin
```

The `erase_flash` step clears NVS. Omit it when preserving existing settings is required, but retain a settings export first.

## OTA Deployment

Upload only:

`LILYGO-T-2CAN_v4.3.0-beta.9_ota_app_20260725-0313.bin`

Use the existing authenticated OTA page or `/update` endpoint. Do not upload the full merged image to the OTA application endpoint.

Physical OTA installation was not performed in this build session; the artifact is build-verified and layout-verified only.

## First-Boot Checks

1. Confirm firmware version is `4.3.0-beta.9`.
2. Confirm Legacy hardware mode and the independent module/defense gates.
3. Leave `dnd_speed` disabled for the first CAN-only diagnostic boot.
4. Open the CAN recorder and include at least:

   `0x118,0x186,0x370,0x389,0x399,0x3C2,0x3EE,0x3FD`

5. Confirm CAN1/CAN2 counters and TX error counters are healthy.
6. Verify `wheelDndDiag` reports `dndActionType: right_scroll` only when `dnd_speed` is enabled.
7. Enable the feature only after native `0x3C2 mux1` and idle frames are visible.

## Vehicle Test Gate

The first vehicle run must be a bounded one-shot test with continuous `0x370` NAG disabled. Verify:

- `native3c2Seen=true`
- `nativeMux1IdleSeen=true`
- `baseFrameMode=native_mux1_idle`
- `sequenceState` progresses through the four steps
- `tx` reports no failure
- `blockedReason` does not become `stale_native`, `no_native_mux1` or `tx_failed`
- no CAN bus error growth, AP/FSD interruption, camera warning or speed-setting anomaly

If any check fails, disable `dnd_speed`, stop recording, export `/status` and the CSV, and do not repeat the action until the frame path is reviewed.

## Validation Boundary

The local tests prove state-machine behavior and compilation. They do not prove that the target vehicle accepts the `0x3C2` sequence. The current supplied captures did not contain native `0x3C2`, so vehicle acceptance remains pending.
