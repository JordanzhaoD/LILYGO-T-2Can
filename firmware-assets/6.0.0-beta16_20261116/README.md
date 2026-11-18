# 固件资产 v6.0.0-beta16（2026-11-16）— T2CAN 档杆免打扰（平行于左滚轮免打扰）

**本目录为 LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `4a8f3f3`（main）+ 未提交的档杆免打扰改动（`include/dash_legacy_stalk_045_dnd.h`
等）。app_desc.version = `6.0.0-beta16`。

## T2CAN 这次为什么要重建

beta14 之后，滚轮免打扰已经恢复稳定（sess_118/119 的瞬时 abort 锁死已修复）。
本次 beta16 带来的是**第二条、独立于滚轮的 NAG 清除路径**——**档杆（速度控制杆）
免打扰**，以及一个被档杆功能顺带暴露的非 P4 编译修复。

## beta16 修了什么（T2CAN 可感知的全部变化）

### 新增：Legacy 档杆免打扰（平行于左滚轮免打扰）

FSD 防护页新增「Legacy 档杆免打扰」开关，紧挨在「Legacy 左滚轮免打扰」上方。
它与滚轮免打扰共用同一套状态机与安全栅栏，但**动作字段完全不同**：

- 滚轮免打扰：驱动 0x045 `STW_ACTN_RQ` 的 switch word（data[4]/data[5]）左滚轮 tick。
- 档杆免打扰：驱动同帧的 `SpdCtrlLvr_Stat`（data[0] bit0-5），用**速度设定拨杆的
  上/下拨档位** `UP_1ST`(0x50) / `DN_1ST`(0x60) 方向交替，清除 NAG 提醒。

**为什么档杆路径更安全**：速度拨杆的上下拨只调整设定速度（±1），**结构上不触碰
AP 的启动/取消轴**；而档杆的前推 FWD(0x41)/后拨 RWD(0x42) 分别取消/启动 AP，被
编码层硬禁止（`isSafeLever`），任何禁用值只计入 `forbiddenLeverRejects` 诊断，
绝不外发。这是与滚轮免打扰同构的「两轴安全」设计，落到档杆字段上。

**实车已 pin（sess_137）**：每次物理拨动档杆 = 1–3 帧 `1ST` 瞬时脉冲 + 持续 `2ND`
保持（上拨 `0x50`→`0x44`，下拨 `0x60`→`0x48`）；data[3] 恒 0x30、switch word 恒 0，
证明档杆只活在 data[0]。清除 NAG 认的是 `1ST` 瞬态。抓包时 AP=0（停车标定），
证明该手势与 AP 状态无关。

### 修复：非 P4 环境编译

`dashDefenseConfigJson()` 原本无条件读取 `g_alertWatchEnabled`，而该变量只在
`DRIVER_P4_3CAN` 下存在（alert watch 是 P4 专属）。这曾挡住包括 T2CAN 在内的
所有非 P4 构建。现已用 `#if defined(DRIVER_P4_3CAN)` 保护，非 P4 输出
`alertWatch:false`。

## 文件清单

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v6.0.0-beta16_ota_app.bin` | OTA 应用镜像（WebUI OTA / 已在跑旧版时单写 0x20000） |
| `LILYGO-T-2CAN_v6.0.0-beta16_full_merged.bin` | USB 全新整刷合并镜像（从 0x0 一次写入） |
| `LILYGO-T-2CAN_bootloader.bin` | 首刷四段链之一（0x0） |
| `partitions.bin` | 首刷四段链之一（0x8000） |
| `ota_data_initial.bin` | 首刷四段链之一（0x19000） |

根部 `LILYGO-T-2CAN_latest_*` 三指针已同步指向本目录内容。

## 烧录（S3 布局，与 P4 的 0x2000 不同！）

USB 全新整刷（一次性）：

```
esptool --chip esp32s3 write_flash \
  0x0     LILYGO-T-2CAN_bootloader.bin \
  0x8000  partitions.bin \
  0x19000 ota_data_initial.bin \
  0x20000 LILYGO-T-2CAN_v6.0.0-beta16_ota_app.bin
```

或直接 `write_flash 0x0 LILYGO-T-2CAN_v6.0.0-beta16_full_merged.bin`。
在跑旧版的板子升级：WebUI OTA 上传 ota_app，或 `write_flash 0x20000` 单写。
刷后核对 `/status` 的 `uiBuildId` 以 `6.0.0-beta16-lilygo_t2can_dual-` 开头。

## 分区表（16MB，与既有布局逐项一致）

nvs@0x9000 64K · otadata@0x19000 8K · app0@0x20000 4M · app1@0x420000 4M ·
spiffs@0x820000 7936K · coredump@0xFE0000 128K

## 构建与验证

- 构建：干净重建（避 CMakeCache/PROJECT_VER 旧版本坑，版本号从 VERSION 注入
  app_desc）；PlatformIO + ESP-IDF 6.0.1，merge 工具 esptool 5.6.0
  （S3 配方 0x0/0x8000/0x19000/0x20000）。
- 门禁：native `native_legacy_stalk_045_dnd` 12/12 通过；滚轮 DND
  `native_legacy_wheel_045_dnd` 25/25 通过（未破坏）；抓包分析器
  `test_analyze_legacy_stalk_045_capture.py` 3/3 通过。
- 镜像自检：0x0=E9 · 0x8000=AA50 · 0x19000=FF · 0x20000=E9 ·
  app_desc.version=`6.0.0-beta16`。
- app 1,227,424 B（app0 预算 4M 内）；full_merged 1,358,496 B。

## 档杆免打扰使用说明

1. 开 AP 跑起来，触发 NAG 提醒（手离开方向盘）。
2. 进 UI **FSD 防护页**，打开「Legacy 档杆免打扰」开关（默认 OFF）。
3. 观察：NAG 是否被清除、AP 状态是否保持（不取消不重启）、设定速度是否只 ±1。

> 注意：开关默认 OFF。sess_137 已 pin 了档杆编码，但「AP 激活状态下注入 0x50/0x60
> 能否实际清除 NAG」的端到端闭环仍需一次实车确认，通过后再考虑放开默认值。

## P4 / 微雪板说明

档杆免打扰是共享层改动，P4 与微雪板同样受益；需要对应资产时按同流程重建。
