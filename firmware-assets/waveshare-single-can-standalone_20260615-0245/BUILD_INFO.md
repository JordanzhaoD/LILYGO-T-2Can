# Waveshare 单 CAN 独立版 Release Assets (20260615-0245)

- Environment: waveshare_single_can_standalone
- Board: Waveshare ESP32-S3-RS485-CAN / ESP32-S3 16MB (single TWAI CAN, GPIO15 TX / GPIO16 RX)
- Product profile: DASH_SINGLE_CAN_STANDALONE=1 (single-CAN; no MCP2515 / CAN2 runtime)
- Default HW mode: Auto (DASH_INITIAL_HW_MODE=3)
- Git revision: f814a76
- Build timestamp: 20260615-0245
- UI build: 4.1.0-lilygo_t2can_dual-f814a7671158-2026-06-14T18:38:54Z
- App size: 1,123,216 bytes (Flash 26.5% of 4 MB app partition)
- Flash layout used for full merged image:
  - 0x00000 bootloader
  - 0x08000 partitions
  - 0x19000 ota_data_initial
  - 0x20000 app/firmware

## Assets

- OTA app: waveshare-single-can-standalone_ota_app_20260615-0245.bin (Dashboard OTA 升级用)
- Full merged: waveshare-single-can-standalone_full_merged_20260615-0245.bin (esptool 全量刷写用，烧到 offset 0x0)
- Bootloader: bootloader_20260615-0245.bin
- Partitions: partitions_20260615-0245.bin
- OTA data: ota_data_initial_20260615-0245.bin
- Checksums: SHA256SUMS.txt

## Safety boundary

AP Injection Gate 安全策略对齐 ev-open-can-tools v3.0.2-beta.3：Legacy 0x3EE mux0 在 gate 开启时必须 AP active 连续 2 秒；unknown/invalid/SNA gear 默认 fail-closed。降低 2026.8.3.6 相关误注入风险，但不承诺全车型直接兼容。首次接车前先连 Dashboard 确认 /status.capabilities.singleCan=true、can2Available=false，并确认 FSD 注入关闭。
