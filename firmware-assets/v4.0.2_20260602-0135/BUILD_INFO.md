# LILYGO T-2CAN v4.0.2 Release Assets (20260602-0135)

- Environment: lilygo_t2can_dual
- Board: LILYGO T-2CAN / ESP32-S3 16MB
- Default HW: Legacy/HW0
- Git revision: 4ee6198
- Working tree changed entries: 2
- Build timestamp: 20260602-0135
- Flash layout used for full merged image:
  - 0x00000 bootloader
  - 0x08000 partitions
  - 0x19000 ota_data_initial
  - 0x20000 app/firmware

## Assets

- OTA app: LILYGO-T-2CAN_v4.0.2_ota_app_20260602-0135.bin
- Full merged: LILYGO-T-2CAN_v4.0.2_full_merged_20260602-0135.bin
- Bootloader: bootloader_20260602-0135.bin
- Partitions: partitions_20260602-0135.bin
- OTA data: ota_data_initial_20260602-0135.bin
- Checksums: SHA256SUMS.txt

## Notes

This build includes Legacy CAN760 speed offset real wiring plus Batch A upstream port: VCSEC-spec Service Mode 0x339 four-frame pulse, CAN2 TX/TXErr/EFLG diagnostics, MCP2515 10MHz SPI, and README high-beam shared-bus limit documentation.
