# 固件资产 v6.0.0-beta08（2026-11-01）— P4-3CAN 三总线观察版

**beta08 = beta07 + P4 专属三路 CAN 观察车道。** Jordan 需求（2026-10-31）：
板名要显示 P4-3CAN、三路 CAN 状态可见、诊断里能抓三路帧、记录器能录三路。
三板（LILYGO-T-2CAN、微雪两环境）**逻辑零改动**——CAN C 车道全在
`DRIVER_P4_3CAN` 门内编译惰性，三板二进制从 `6.0.0-beta07_20261031/`
**逐字节搬移**（cmp 校验一致），车上实跑基线不动，**不重建**。

## beta08 改了什么（仅 P4 一路）

1. **板名**：UI 标题/锁屏由固件 `product` 字段驱动（新宏
   `DASH_PRODUCT_NAME`），P4 显示 `Atlas-P4-3CAN`；三板默认值仍是
   `Atlas T-2CAN`，渲染字节不变。
2. **CAN C 车道**（TWAI2，GPIO 26/27，X179 18/19 PT 动力总线）：
   **listen-only，永不 TX、永不 ACK**。帧只进嗅探环 + 记录器（带总线
   标签），不进车辆状态机、不进 0x399/0x045/0x3C2 证据计数（网关镜像
   保护）。
3. **三总线 UI**：总览页 CAN3 RX 卡片 + 移动端 CAN3 卡（能力门
   `can3Available`，三板隐藏）；嗅探器每帧显示 CAN1/CAN2/CAN3 标签
   （可过滤）；记录器 CSV bus 列 CAN C=3，`/status` 新增 `can3` 对象
   （rx/tx/txerr/eflg）。

## ⚠️ P4 烧录纪律（与三板不同！）

P4 的 bootloader 在 **0x2000**（非 S3 的 0x0）。首刷四段链：

```
esptool --chip esp32p4 write_flash \
  0x2000  Atlas-P4-3CAN_bootloader.bin \
  0x8000  Atlas-P4-3CAN_partitions.bin \
  0x19000 ota_data_initial.bin \
  0x20000 Atlas-P4-3CAN_v6.0.0-beta08_ota_app.bin
```

（或 `write_flash 0x0 Atlas-P4-3CAN_v6.0.0-beta08_full_merged.bin` 整刷，
merged 自检：0x0=FF 垫、0x2000=0xE9、0x8000=0xAA、0x20000=0xE9。）
2 号板已于 2026-10-31 烧录此版并联 LAN 验证（192.168.1.146）。
**此后升级 = WebUI OTA 或单写 app@0x20000。**

## 文件

| 文件 | 用途 |
|---|---|
| `Atlas-P4-3CAN_v6.0.0-beta08_ota_app.bin` | P4 OTA 应用（WebUI OTA / 单写 @0x20000） |
| `Atlas-P4-3CAN_v6.0.0-beta08_full_merged.bin` | P4 全链镜像（esptool @0x0 整刷） |
| `Atlas-P4-3CAN_bootloader.bin` / `Atlas-P4-3CAN_partitions.bin` | P4 原始分段（四段链用；与 beta07 功能等价，仅内嵌编译时间戳不同） |
| `LILYGO-T-2CAN_*` / `Waveshare-*` | 三板资产（beta07 bytes 逐字节搬移，**仍为 beta07 版**） |
| `ota_data_initial.bin` | otadata 擦除镜像（FF×8192，四板通用） |
| `partitions.bin` | 三板共用分区表（16MB 布局；P4 用自己的） |

烧录纪律：禁止 `pio run -t upload`；WebUI OTA 或 esptool 全链/四段链。

## 构建环境

- 三板：与 `6.0.0-beta07_20261030/` 完全相同的 bytes，未重建（黄金
  env `lilygo_t2can_dual` 在 beta08 树上构建绿，作为非回归门，未发布）。
- P4：本仓库 2026-10-31 树（beta08 提交链 `2c07ee9` CAN C 车道+三总线
  UI，`d6dfceb` product 字段补 /status 端点），uiBuildId
  `…-d6dfceb7e0e8-2026-10-31T04:28:41Z`——即 2 号板实跑的这一版；
  证据见 `docs/verification/p4-3can-bench-2026-11-01.md`。pio 6.2.0 +
  espressif32@7.0.0 + IDF 6.0.1 + riscv 15.2.0 钉包，
  `platformio_p4_build_quirks.py` 处理 P4 专属构建怪癖。
  门禁：契约测试 130 + python 237 全绿、native 184/184、黄金 env 构建绿。

## 自检（本目录已过）

- P4 merged 镜像四标记各就各位（0x0=FF/0x2000=0xE9/0x8000=0xAA/
  0x20000=0xE9）。
- P4 固件：`6.0.0-beta08` 版本串 ×7、beta07 串零残留、
  `Atlas-P4-3CAN` ×4（板名+AP SSID+module+product）、
  `can3Available` 能力在场、CAN C listen-only 启动串在场。
- 三板 11 个文件与 20261031 目录 cmp 逐字节一致。
