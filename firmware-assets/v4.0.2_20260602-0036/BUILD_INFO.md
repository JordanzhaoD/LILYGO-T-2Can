# LILYGO T-2CAN v4.0.2 Release Assets (20260602-0036)

- Environment: lilygo_t2can_dual
- Board: LILYGO T-2CAN / ESP32-S3 16MB
- Default HW: Legacy/HW0
- Git revision: bb6f737
- Working tree changed entries: 13
- Build timestamp: 20260602-0036
- Flash layout used for full merged image:
  - 0x00000 bootloader
  - 0x08000 partitions
  - 0x19000 ota_data_initial
  - 0x20000 app/firmware

## Assets

- OTA app: LILYGO-T-2CAN_v4.0.2_ota_app_20260602-0036.bin
- Full merged: LILYGO-T-2CAN_v4.0.2_full_merged_20260602-0036.bin
- Bootloader: bootloader_20260602-0036.bin
- Partitions: partitions_20260602-0036.bin
- OTA data: ota_data_initial_20260602-0036.bin
- Checksums: SHA256SUMS.txt

## Notes

This build includes Legacy/HW0 speed offset real wiring: WebUI fixed/auto/custom speed strategy drives CAN 760 byte5 low6 UI_userSpeedOffset via dashComputeLegacySimpleOffsetKph().
