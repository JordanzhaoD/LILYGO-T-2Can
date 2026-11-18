# 固件资产 v6.0.0-beta17（2026-11-18）— 滚轮回退 beta02 引擎 + 档杆换 0x045 bit27 状态克隆

**本目录为 LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `7ded1e7`（main）。app_desc.version = `6.0.0-beta17`，
uiBuildId = `6.0.0-beta17-lilygo_t2can_dual-7ded1e7ce8a4-2026-11-18`。

## beta17 修了什么（T2CAN 可感知的全部变化）

### 修复：左滚轮免打扰「运行一段后失效」回归（回退 beta02 引擎）

实车判定：beta02 的滚轮免打扰完全稳定；beta16 约 1 小时行驶后停止清除 NAG，
警告最终升级到红色方向盘。根因是 beta07（单帧+跨回合方向交替）与 beta14
（回合内 abort 自愈）两笔改动引入的行为回归。beta17 将状态机**逐行回退到
beta02 原子配对引擎**：

- 每个警告回合 `+1 tick → 等原生确认 → −1 补偿 → 等确认`，**音量天然平衡**
  （净漂移为零，`balancedPairs` 遥测）；
- **软预算**：3 对上限只管自动续期链，警告持续时 Idle 路径重新武装继续清
  ——beta02「中途从不放弃」的本质；
- 瞬时 abort 在回合内静默、下一个警告沿自动复活（beta02 既知特性，
  换取整趟可靠）。

### 换路线：档杆手势作废 → 0x045 bit27 状态克隆免打扰

beta16 的档杆（速度杆 UP/DN 手势）路线**整体移除**：手势模拟属于「动作
欺骗」，车端累积「只交互不出力」后必然触发 strike-out 惩罚（红色方向盘、
AP 强制接管、5 次锁 Autosteer 一周）——数学上无法避免，与滚轮同死因。

替代路线是「**状态声明**」：新增 `Legacy 状态克隆免打扰` 开关，按参考固件
逆向报告的实车验证参数（窗、门、间隔逐字照抄），克隆 CAN1（X197 13/14，
DAS/CH 线）上最新的原生 0x45 帧、置位 `StW_Cond_Flt`（bit27，data[3] bit3）、
滚动计数器 +1、CRC8-J1850 重发：

- **handsOn 窗 {2..5}**：从「要求手在盘上但未检测到」（警告前夜）就开始
  维持状态声明——**预防提醒产生，而不是事后清除**；
- **autopilotState > 3 门**：AP 激活域之外完全静默；
- **1500ms 间隔**：低频单帧克隆，不淹没原生流；
- **窗到 6 硬关**：模块永不注入对抗 strike-out 域（安全灵魂——与作废的
  手势路线的本质区别）。

安全栅全套沿用滚轮纪律：原生帧 150ms 新鲜度、计数器续接、own-echo 指纹
丢弃、**物理滚轮操作期间绝不克隆**（不复制/放大人手输入）、连续 2 次 TX
失败自动解除直到重开开关。

### 保留：红色方向盘（state 7）只计数不清除

`struckOutEpisodes` 诊断计数 handsState==7 沿（`/status` 可查）；两个免打扰
模块都**永不**对 state 7 注入——对 strike-out 注入会被特斯拉记录为一次
strike-out（5 次 = Autosteer 禁用一周）。

## 文件清单

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v6.0.0-beta17_ota_app.bin` | OTA 应用镜像（WebUI OTA / 已在跑旧版时单写 0x20000） |
| `LILYGO-T-2CAN_v6.0.0-beta17_full_merged.bin` | USB 全新整刷合并镜像（从 0x0 一次写入） |
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
  0x20000 LILYGO-T-2CAN_v6.0.0-beta17_ota_app.bin
```

或直接 `write_flash 0x0 LILYGO-T-2CAN_v6.0.0-beta17_full_merged.bin`。
在跑旧版的板子升级：WebUI OTA 上传 ota_app，或 `write_flash 0x20000` 单写。
刷后核对 `/status` 的 `uiBuildId` 以 `6.0.0-beta17-lilygo_t2can_dual-` 开头。

## 分区表（16MB，与既有布局逐项一致）

nvs@0x9000 64K · otadata@0x19000 8K · app0@0x20000 4M · app1@0x420000 4M ·
spiffs@0x820000 7936K · coredump@0xFE0000 128K

## 构建与验证

- 构建：干净重建（避 CMakeCache/PROJECT_VER 旧版本坑）；PlatformIO +
  ESP-IDF 6.0.1，merge 工具 esptool 5.6.0（S3 配方 0x0/0x8000/0x19000/0x20000）。
- 门禁：`native_legacy_wheel_045_dnd` 18/18（重写 pin beta02 配对行为）；
  `native_legacy_045_bit27_clone` 16/16；`native_dashboard` 通过；
  `lilygo_t2can_dual` 与 `p4_3can` 构建全绿。
- 镜像自检：0x0=E9 · 0x8000=AA50 · 0x20000=E9 ·
  app_desc.version=`6.0.0-beta17`。
- app 1,228,832 B（app0 预算 4M 内）；full_merged 1,359,904 B。
- 已于 2026-11-18 晚 USB 烧录上板（`pio run -t upload`，hash 全验），
  固件内已确认 `clone27Diag` / `left_045_paired_compensation` 等特征串。

## 状态克隆免打扰使用说明（实车两阶段）

1. **短途第一阶段**：打开「Legacy 状态克隆免打扰」开关（默认 OFF），AP
   激活行驶——盯仪表，任何新告警 = 立即关开关并抓 `/status`。正常表现：
   `clone27Diag.cloneTx` 每 1.5s 增长，handsOn 被压在 2 以下。
2. **长途第二阶段（1h+）**：NAG 不再出现、无红色方向盘、`windowEntries`
   接近 0 = bit27 声明把提醒压死在源头。
3. 跑完抓 `/status` 的 `clone27Diag` 与 `wheelDndDiag` 回传分析。

## P4 / 微雪板说明

滚轮回退与状态克隆均为共享层改动；P4 资产按同流程重建（p4_3can 已过构建门禁）。
