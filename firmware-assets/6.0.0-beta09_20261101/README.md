# 固件资产 v6.0.0-beta09（2026-11-01）— P4-3CAN 桌面 CAN3 卡 + 芯片温度修复

**beta09 = beta08 + 三处修复（两处 Jordan 报障 + 一处连带发现的黄金 env 编译破绽）。**
三板（LILYGO-T-2CAN、微雪两环境）**逻辑零改动**——修复全在能力门/目标门内
惰性，三板二进制从 `6.0.0-beta08_20261101/`（即 beta07 bytes）**逐字节搬移**
（cmp 校验一致），车上实跑基线不动，**不重建**。

## beta09 修了什么

1. **车机桌面 UI 的 CAN3 卡**（Jordan 2026-10-31 报障）：beta08 只交付了
   总览 CAN3 卡片+手机端卡片，桌面「驾驶状态中心」指标行漏了 CAN3。现在
   CAN1/CAN2 旁新增 CAN3 卡（能力门 `can3Available`，三板隐藏），行宽
   自动撑到六列。
2. **芯片温度**（同日报障）：tsens 编译门只含 S2/S3，P4 恒无温度显示 `--`。
   P4 加入门后实测 34.1°C（其 tsens 属性表含同款 -10..80°C 档，配置零改动）。
3. **黄金 env 编译修复**：`DASH_PRODUCT_NAME` 兜底定义在首次使用之后，
   无 `-D` 覆盖的 env（`lilygo_t2can_dual` 等）编译必炸（beta08 收口时黄金门
   跑在引入提交之前被掩盖）。兜底上移+契约测试钉顺序；不影响任何已发布
   二进制，但源码树从 beta08 HEAD 构建三板会失败，**升级源码必须过 beta09**。

## ⚠️ P4 烧录纪律（与三板不同！）

P4 的 bootloader 在 **0x2000**（非 S3 的 0x0）。首刷四段链：

```
esptool --chip esp32p4 write_flash \
  0x2000  Atlas-P4-3CAN_bootloader.bin \
  0x8000  Atlas-P4-3CAN_partitions.bin \
  0x19000 ota_data_initial.bin \
  0x20000 Atlas-P4-3CAN_v6.0.0-beta09_ota_app.bin
```

（或 `write_flash 0x0 Atlas-P4-3CAN_v6.0.0-beta09_full_merged.bin` 整刷，
merged 自检：0x0=FF 垫、0x2000=0xE9、0x8000=0xAA、0x20000=0xE9。）
2 号板已于 2026-10-31 单写 app@0x20000 升级此版并联 LAN 验证（192.168.1.146，
桌面六卡+34.1°C 实渲染截图存证）。
**此后升级 = WebUI OTA 或单写 app@0x20000。**

## 文件

| 文件 | 用途 |
|---|---|
| `Atlas-P4-3CAN_v6.0.0-beta09_ota_app.bin` | P4 OTA 应用（WebUI OTA / 单写 @0x20000） |
| `Atlas-P4-3CAN_v6.0.0-beta09_full_merged.bin` | P4 全链镜像（esptool @0x0 整刷） |
| `Atlas-P4-3CAN_bootloader.bin` / `Atlas-P4-3CAN_partitions.bin` | P4 原始分段（四段链用；与 beta07/08 功能等价，仅内嵌编译时间戳不同） |
| `LILYGO-T-2CAN_*` / `Waveshare-*` | 三板资产（beta07 bytes 逐字节搬移，**仍为 beta07 版**；三板要从源码重建必须用 beta09 及以后——beta08 HEAD 编译不了三板） |
| `ota_data_initial.bin` | otadata 擦除镜像（FF×8192，四板通用） |
| `partitions.bin` | 三板共用分区表（16MB 布局；P4 用自己的） |

烧录纪律：禁止 `pio run -t upload`；WebUI OTA 或 esptool 全链/四段链。

## 构建环境

- 三板：与 `6.0.0-beta07_20261031/` 完全相同的 bytes，未重建（黄金 env
  `lilygo_t2can_dual` 在 beta09 树上构建绿，作为非回归门，未发布）。
- P4：本仓库 2026-10-31 树（beta09 提交链 `8086731` CAN3 桌面卡+tsens 门、
  `1c12305` DASH_PRODUCT_NAME 兜底前移），uiBuildId
  `…-1c123053b629-2026-10-31T04:53:35Z`——即 2 号板实跑的这一版；
  证据见 `docs/verification/p4-3can-bench-2026-11-01.md`（Task 8 节）。
  pio 6.2.0 + espressif32@7.0.0 + IDF 6.0.1 + riscv 15.2.0 钉包，
  `platformio_p4_build_quirks.py` 处理 P4 专属构建怪癖。
  门禁：契约测试 133 + python 240 全绿、native 184/184、黄金 env 构建绿。

## 自检（本目录已过）

- P4 merged 镜像四标记各就各位（0x0=FF/0x2000=0xE9/0x8000=0xAA/
  0x20000=0xE9）。
- P4 固件：`6.0.0-beta09` 版本串 ×7、beta08 串零残留、
  `Atlas-P4-3CAN` ×4（板名+AP SSID+module+product）、
  `can3Available` 能力在场、CAN C listen-only 启动串在场。
- 三板 11 个文件与 beta08 目录（=beta07 bytes）cmp 逐字节一致。
