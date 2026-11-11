# 固件资产 v6.0.0-beta14（2026-11-11）— T2CAN 左滚轮免打扰回合内恢复

**本目录为 LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `6dc28ef`（main），源 commit 链：`2d498da`（修复+VERSION）→ `6dc28ef`（UI 资产戳）。

## T2CAN 为什么这次要重建

beta08→beta13 全部是 **P4-3CAN 专属改动**（三总线上板 / 录制器 v2 黑盒 /
C6 失联降级 / 冲刷分块 / 串流关闸），T2CAN 共享层零变化——所以 beta08~10
资产目录里 T2CAN 一直逐字节沿用 beta07。**beta14 是 beta07 之后第一个触及
T2CAN 共享代码的版本**：`dash_legacy_wheel_045_dnd.h` 状态机修复，双板同病同治。

## beta14 修了什么（T2CAN 可感知的全部变化）

**Legacy 左滚轮免打扰不再"偶尔失效"。** 田野证据 sess_118/119（P4 抓录）定案：
状态机的 `abort()` 没有回合内恢复路径——任何瞬时挫折（确认帧丢失 / 计数器跳变 /
单次发送失败）都会把机器锁死成 Aborted，直到警告完全清除。而 hands 警告回合
长达成秒，整回合死透 = 功能"偶尔没反应"。sess_118 里 6.34 秒警告零发送
（7.17s 的原生 tick 帧是驾驶员手动滚动救场）；sess_119 里重试确认被录制丢弃窗
吞掉后 300ms 超时锁死。

修复（`2d498da`）：

1. **瞬时 abort 回合内自愈**：ReleaseTimeout / CounterConflict / TxFailed /
   运行时门瞬时翻转 / 无效原生帧 → 800ms 冷却后自动重武装（防与车端同向
   合并窗打架）。人手实际滚动（PhysicalInput）保持回合终态，不与驾驶员抢滚轮；
   连续两次发送失败仍自动解除武装（AutoDisarmed 两击闩锁不变）。
2. **预算改按发出帧数计**（`txAttemptsThisEpisode`）：恢复循环下仍严格
   3 帧/回合封顶，音量净漂移刻度与 beta02 相同。
3. **诊断新增** `wheelDndDiag.txAttemptsThisEpisode` / `recoveries`：
   开车后 `/status` 里 `recoveries > 0` = 自愈机制实际救过场。

## 文件清单

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v6.0.0-beta14_ota_app.bin` | OTA 应用镜像（WebUI OTA / 已在跑旧版时单写 0x20000） |
| `LILYGO-T-2CAN_v6.0.0-beta14_full_merged.bin` | USB 全新整刷合并镜像（从 0x0 一次写入） |
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
  0x20000 LILYGO-T-2CAN_v6.0.0-beta14_ota_app.bin
```

或直接 `write_flash 0x0 LILYGO-T-2CAN_v6.0.0-beta14_full_merged.bin`。
在跑旧版的板子升级：WebUI OTA 上传 ota_app，或 `write_flash 0x20000` 单写。
刷后核对 `/status` 的 `uiBuildId` = `6.0.0-beta14-lilygo_t2can_dual-6dc28ef6ef91`。

## 分区表（16MB，与既有布局逐项一致）

nvs@0x9000 64K · otadata@0x19000 8K · app0@0x20000 4M · app1@0x420000 4M ·
spiffs@0x820000 7936K · coredump@0xFE0000 128K

## 构建与验证

- 构建：干净重建（避 CMakeCache/PROJECT_VER 旧版本坑）；PlatformIO + ESP-IDF 6.0.1，
  merge 工具 esptool 5.4.0（S3 配方 0x0/0x8000/0x19000/0x20000）。
- 门禁：native 25/25 环境（815 用例）+ pytest 510 passed/754 subtests +
  双板构建 SUCCESS。
- 镜像自检：0x0=E9 · 0x8000=AA50 · 0x19000=FF · 0x20000=E9 ·
  app_desc.version=`6.0.0-beta14` · UI 戳 `6dc28ef6ef91` · 分区解码全对。
- app 1,221,648 B（app0 预算 4M 内）；full_merged 1,352,720 B。

## P4 / 微雪板说明

P4-3CAN 板 2 已通过 WebUI OTA 运行 beta14（无资产目录需求）；微雪两环境
共享层同样受益于本修复，需要资产时按同流程重建。
