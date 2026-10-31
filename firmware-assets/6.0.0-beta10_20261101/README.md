# 固件资产 v6.0.0-beta10（2026-11-01）— P4 C6 失联安全降级

**beta10 = beta09 + 一处 P4 安全修复（源自 1 号板 C6 终审诊断）。**
三板（LILYGO-T-2CAN、微雪两环境）**逻辑零改动**，11 个文件从
`6.0.0-beta09_20261101/` **逐字节搬移**（cmp 校验一致，仍为 beta07 bytes），
车上实跑基线不动，**不重建**。

## beta10 修了什么

1. **C6 WiFi 从机失联时整机不再循环重启**（1 号板终审副产物，对所有 P4 板生效）：
   继承自 arduino 出厂默认的 `ESP_HOSTED_TRANSPORT_RESTART_ON_FAILURE=y`
   会在从机链路断开时每 ~13 秒重启整机——行车中等于反复杀死 CAN 防御。
   现改为 `not set`：C6 失联时板子降级为纯 CAN 继续工作。
   现场实证（1 号板=天然坏 C6 样本）：诊断版 5 分钟 0 次重启；beta10 成品
   90 秒抓包 0 次重启、CAN B/C 照常上线、hosted 在 retry[100] 后优雅放弃。
2. **传输模式回归流式**（STREAMING_MODE）：诊断期临时切过的包模式
   （RX_NONE）已回退——流式是上游默认且 2 号板实证路径；包模式只是
   1 号板诊断手段，对坏 C6 无效（init 事件始终不至）。
3. 契约测试新增 `test_p4_sdkconfig_pins_hosted_safety_choices`：钉死上述
   两项，防止 PIO 写回悄悄恢复 arduino 默认。

## 1 号板 C6 终审（诊断结论，2026-10-31）

硅片不死（SDIO 枚举/数据流/GPIO54 复位全部活着），但 C6 从机固件状态在
滥用期（11 秒自杀循环反复打断其启动）退化为不可恢复：Atlas 代 host
（arduino 3.3.12）与 IDF esp_hosted 2.12.13（流式+包式）全部无法握手；
闲置 1 小时后连化石 AP 都不再播（比 10-20 时代更糟=退化实证）。C6 的
USB 未布线（ioreg 实证）、IO9/UART 无焊点、slave OTA 需活链路——修复
通道全封死。**1 号板定为 CAN 专用台架备件**，已刷 beta10 作为本修复的
永久现场证明板。

## ⚠️ P4 烧录纪律（与三板不同！）

P4 的 bootloader 在 **0x2000**（非 S3 的 0x0）。首刷四段链：

```
esptool --chip esp32p4 write_flash \
  0x2000  Atlas-P4-3CAN_bootloader.bin \
  0x8000  Atlas-P4-3CAN_partitions.bin \
  0x19000 ota_data_initial.bin \
  0x20000 Atlas-P4-3CAN_v6.0.0-beta10_ota_app.bin
```

（或 `write_flash 0x0 Atlas-P4-3CAN_v6.0.0-beta10_full_merged.bin` 整刷，
merged 自检：0x0=FF 垫、0x2000=0xE9、0x8000=0xAA、0x20000=0xE9。）
1 号板已于 2026-11-01 单写 app@0x20000 升级此版并联机验证（90 秒抓包：
零重启、CAN B/C 上线、hosted 优雅放弃）。
**此后升级 = WebUI OTA 或单写 app@0x20000。**
⚠️ 2 号板（实车候选）仍在 beta09，上车前必须升级本版并复验。

## 文件

| 文件 | 用途 |
|---|---|
| `Atlas-P4-3CAN_v6.0.0-beta10_ota_app.bin` | P4 OTA 应用（WebUI OTA / 单写 @0x20000） |
| `Atlas-P4-3CAN_v6.0.0-beta10_full_merged.bin` | P4 全链镜像（esptool @0x0 整刷） |
| `Atlas-P4-3CAN_bootloader.bin` / `Atlas-P4-3CAN_partitions.bin` | P4 原始分段（四段链用） |
| `LILYGO-T-2CAN_*` / `Waveshare-*` | 三板资产（beta09 bytes 搬移=仍 beta07 版） |
| `ota_data_initial.bin` | otadata 擦除镜像（FF×8192，四板通用） |
| `partitions.bin` | 三板共用分区表（16MB 布局；P4 用自己的） |

烧录纪律：禁止 `pio run -t upload`；WebUI OTA 或 esptool 全链/四段链。

## 构建环境

- P4：本仓库 2026-11-01 树（beta10 提交：sdkconfig 安全钉定 + VERSION/
  CHANGELOG + 契约测试），espressif32@7.0.0 + IDF 6.0.1 + riscv 15.2.0
  钉包。生效配置经 `.pio/build/p4_3can/config/sdkconfig.json` 复核：
  `ESP_HOSTED_TRANSPORT_RESTART_ON_FAILURE=False`、
  `ESP_HOSTED_SDIO_OPTIMIZATION_RX_STREAMING_MODE=True`；"Resetting myself"
  串在 ELF 中为 0（重启回调未链接）。
- 门禁：pytest 241+721 子测试全绿、native 184/184、黄金 env
  `lilygo_t2can_dual` 构建绿。

## 自检（本目录已过）

- P4 merged 镜像四标记各就各位（0x0=FF/0x2000=0xE9/0x8000=0xAA/
  0x20000=0xE9）。
- P4 固件：`6.0.0-beta10` 版本串在场；三板 11 文件与 beta09 目录
  cmp 逐字节一致。
