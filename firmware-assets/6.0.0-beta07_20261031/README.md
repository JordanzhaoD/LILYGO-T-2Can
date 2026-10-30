# 固件资产 v6.0.0-beta07（2026-10-31）— 四环境（新增 P4-3CAN）

**在 2026-10-30 三环境资产之上，原封不动新增 Atlas-P4-3CAN 板一路。**
版本号不 bump（同一固件 6.0.0-beta07），三板（LILYGO-T-2CAN、微雪两环境）
的二进制从 `6.0.0-beta07_20261030/` **逐字节搬移**（cmp 校验一致）——
车上实跑的 bytes 不变，只有 P4 一路是本次新构建。

## Atlas-P4-3CAN 是什么

- Waveshare ESP32-P4 WiFi6 开发板 + 3× SN65HVD230 CAN 收发器（板名
  `waveshare-p4-wifi6-sn65hvd230-x3`），ESP32-P4（RISC-V、32MB flash、
  hex PSRAM、WiFi 走板载 C6 companion @ SDIO）。
- **T2CAN 固件原封移植**：CanA（X179 13/14，DAS 主线）= TWAI0 主总线；
  CanB（X179 9/10，滚轮/拨杆总线）= 第二路原生 TWAI（controller_id=1，
  GPIO 4/5）——T2CAN 板上这个位置是 MCP2515 over SPI。功能与 UI 与
  beta07 完全一致，唯一可见差异 = 板名 Atlas-P4-3CAN。CanC（TWAI2，
  26/27）v1 不启动。
- AP：SSID `Atlas-P4-3CAN`（密码见 git-ignored profile），访问地址
  http://100.100.1.1（固件硬编码既有行为）。

## ⚠️ P4 烧录纪律（与三板不同！）

P4 的 bootloader 在 **0x2000**（非 S3 的 0x0）。首刷四段链：

```
esptool --chip esp32p4 write_flash \
  0x2000  Atlas-P4-3CAN_bootloader.bin \
  0x8000  Atlas-P4-3CAN_partitions.bin \
  0x19000 ota_data_initial.bin \
  0x20000 Atlas-P4-3CAN_v6.0.0-beta07_ota_app.bin
```

（或 `write_flash 0x0 Atlas-P4-3CAN_v6.0.0-beta07_full_merged.bin` 整刷，
merged 自检：0x0=FF 垫、0x2000=0xE9、0x20000=0xE9。）
板上原有 Atlas 固件/分区表/NVS 全被替换（预期）。**此后升级 = WebUI OTA
或单写 app@0x20000，永不再四段链（除非分区表再变）。**
P4 分区表独立（32MB 布局），与三板共享的 partitions.bin 不同文件。

## 文件

| 文件 | 用途 |
|---|---|
| `Atlas-P4-3CAN_v6.0.0-beta07_ota_app.bin` | P4 OTA 应用（WebUI OTA / 单写 @0x20000） |
| `Atlas-P4-3CAN_v6.0.0-beta07_full_merged.bin` | P4 全链镜像（esptool @0x0 整刷） |
| `Atlas-P4-3CAN_bootloader.bin` / `Atlas-P4-3CAN_partitions.bin` | P4 原始分段（四段链用） |
| `LILYGO-T-2CAN_*` / `Waveshare-*` | 三板资产（=20261030 逐字节搬移） |
| `ota_data_initial.bin` | otadata 擦除镜像（FF×8192，四板通用） |
| `partitions.bin` | 三板共用分区表（16MB 布局；P4 用自己的） |

烧录纪律：禁止 `pio run -t upload`；WebUI OTA 或 esptool 全链/四段链。

## 构建环境

- 三板：与 20261030 资产完全相同的 bytes（见上），未重建。
- P4：本仓库 2026-10-31 树（commit 链 `05d2ac9`→`d414331`，板型+第二 TWAI
  驱动+复审修正），pio 6.2.0 + espressif32@7.0.0 + IDF 6.0.1 + riscv 15.2.0
  钉包，`platformio_p4_build_quirks.py` 处理 ESP_IDF_VERSION 宏与 rev3
  bootloader 链接脚本两个 P4 专属构建怪癖；sdkconfig 声明真实 32MB flash。
  门禁：native 23 env 全绿、黄金 lilygo_t2can_dual 全绿、零 diff 审计
  （include/web 与 dash_* 未触碰）、独立全分支审查（0 Critical）。

## 自检（本目录已过）

- 四板 bootloader/app 镜像 0xE9 magic 各就各位（P4 @0x2000/@0x20000，
  S3 @0x0/@0x20000）。
- P4 固件：`6.0.0-beta07` 版本串在场（×7）、beta06 串零残留、板名
  `Atlas-P4-3CAN` ×3（板名+AP SSID+module 字段）。
- 三板与 20261030 目录 cmp 逐字节一致。
