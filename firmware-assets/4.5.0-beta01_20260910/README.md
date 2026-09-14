# LILYGO T-2CAN 固件资产 v4.5.0-beta01（2026-09-10）

**用途：实车 0x045 抓包标定 + 后续台架验证。本资产生产时未烧录、未接车。**

## 构建信息

- 环境：`lilygo_t2can_dual`（PlatformIO，ESP32-S3 N16R8，ESP-IDF）
- 分区表：`partitions_16mb_ota_4096k_nvs64.csv`（16MB flash，app0@0x20000）
- UI build-id：`4.5.0-beta01-lilygo_t2can_dual-ddecec0b647e-2026-09-10T12:15:04Z`
- RAM 17.0%（55,776 / 327,680 B）；Flash 28.8%（1,206,663 / 4,194,304 B）
- 构建前验证：Python 合约 223 tests OK；native 20 环境 + ESP 构建在实现会话已全绿
- 合并自检：full_merged.bin 偏移 0 处 = 0xE9（ESP32-S3 bootloader 魔数）✅

## 本版本内容（4.5.0-beta01）

AP 取消+重请求协调器（0x045 FWD 取消 → DAS 证据 → RWD 重请求 → 500ms 窗口内
原生 0x3EE 同帧注入 bit46），三大屏障、替换不串联门控、NVS/UI/诊断全套。
**编译 profile 默认为空：`AP 取消重请求` 开关可打开但永不武装（fail-closed），
UI 显示"不可用·sourceUnconfigured"。**防甩效果未验证——需要真实 0x045 抓包
标定 profile 后台架验证（见 `docs/AP-REREQUEST-0X045-CALIBRATION-PROCEDURE.md`）。

抓包不需要本版本的新功能（录制器 ≥4.3.0-beta.3 就有，缺省 NAG 过滤集已含
0x045/0x399/0x3EE），刷本版本是为了设备与标定分析配套 + 协调器诊断可观察。

## 文件

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v4.5.0-beta01_full_merged.bin` | 全量镜像（bootloader@0x0 + 分区表 + otadata + app@0x20000） |
| `LILYGO-T-2CAN_v4.5.0-beta01_ota_app.bin` | 仅 app 镜像，网页 OTA 用（保留 NVS/配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | 中间产物（esptool 分步烧录用） |
| `SHA256SUMS.txt` | 全部文件校验和 |

## 烧录方式（二选一）

- **网页 OTA（推荐，保留配置）**：设备网页 → 固件更新 → 上传
  `LILYGO-T-2CAN_v4.5.0-beta01_ota_app.bin`。
- **esptool 全量**：`esptool.py --chip esp32s3 write_flash 0x0 LILYGO-T-2CAN_v4.5.0-beta01_full_merged.bin`
  （全量会覆盖 NVS，设备配置需重设）。

⚠️ **禁止 `pio run -t upload`**：flash_args 的 app@0x10000 与自定义分区表
app0@0x20000 冲突，会把 app 写进 nvs/otadata 区域损坏板子（beta.33 事故）。
bootloader 必须在 0x0（ESP32-S3 ROM 从 0x0 加载，非经典 ESP32 的 0x1000）。

## 未验证声明

- 未烧录、未台架、未实车。"激活方向盘猛甩已修复"不成立——协调器连 profile
  都还没有，当前是 fail-closed 占位。
- 实车验证顺序：抓包标定 → 人工审查候选 → 填 `compiledApReRequestProfile()`
  → 重跑测试 → 台架 → 才谈实车。
