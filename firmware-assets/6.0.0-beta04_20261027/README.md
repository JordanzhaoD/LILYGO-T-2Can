# 固件资产 v6.0.0-beta04（2026-10-27）— 全线三环境

**6.0 线第四个 beta，beta03 实车反馈双修复版**：① DND 音量仍持续
减小 → 根因不在固件配对（beta03 已完美）而在节奏，f1→f2 间距对齐
真实滚轮物理地板；② 爆闪开关/参数断电即丢 → 并入 NVS 持久化。
共 4 个 commit（15320f7 → ce6384a → 7ccc3fc → 8535f46）。

## 修复一：DND 左滚轮音量漂移（节奏根因，beta03 证伪后的翻案）

- **为什么 beta03 没修好**：beta03 抓帧证明固件层已完美——14 帧严格
  组成 7 对、counter 连续、零 TX 错误、两帧都被网关镜像上车；字节码
  也是原生真码（段13 实证 0x40/00=+1、0xC0/03=−1）。但 f1→f2 间距
  只有 4-10ms，而真实滚轮物理最小同向帧距 = 19-20ms（模块上报地板）、
  反向转换 ≥600ms——4-10ms 的反向对物理上不可能 → 车机把第二帧
  合并/丢弃 → 每对净 −1 → 音量持续降。凶手从"固件侧丢 f2"（beta03
  前）换成了"车机侧丢 f2"（beta03 后），病征没变。
- **修复**：`kBurstMinGapMs` 3 → 25ms（实测地板 20ms + 余量，仍远小于
  300ms 释放超时）。native 测试全部重定时 + 新增常量钉回归测试
  （防止不知情回退）。
- **诚实风险**：若车机反向合并窗口 >25ms，需迭代 50/100ms——可能
  还要 1-2 轮实车。
- **新验收标准**：不看诊断计数（pos==neg 在 beta03 就已满足但音量仍
  漂——该标准只验固件侧，必要非充分），**耳朵听音量不漂才算过**。

## 修复二：爆闪开关/参数 NVS 持久化

- **为什么会丢**：开关和参数原本只存 RAM（代码注释自认 RAM-only,
  OFF on boot）——车一断电重启回 OFF。"运行一段失效"主解释 = 跨
  驾驶 session 断电丢开关（120s 录制中 0x292 零 TX 与此一致）。
- **修复**：完全套用 FSD 开关既有 NVS 模式（key "bk"~"bn"）：开关或
  参数一改即存，开机一次性恢复（boot-apply 在 dashLoadPrefs 内，
  dashApplyRuntimeState 不碰它避免 mid-session 覆盖）。UI 零改动——
  界面本就回读固件状态。首次烧录默认不变（关、3/180/180）。
- 单 CAN（Waveshare）构建零变化：新代码全在 `#ifdef DRIVER_T2CAN_DUAL`
  内（首轮构建曾在单 CAN 上断链，已按既有边界补齐）。
- **备选根因未排除**：若持久化后"运行一段失效"仍复现，查
  `/bus2_ids` 的 `can2.txerr` 与恢复计数（bus-off 复发线）。

## 验证矩阵

TDD 先红后绿（native：原子对 1024ms 断言翻红 + 常量钉翻红，后绿）；
native_legacy_wheel_045_dnd 19/19（含新钉）；native 23 env / 608 用例
全绿；pytest 229 + 715 子测试；三环境 ESP32 构建 SUCCESS（清缓存
重建后 app_desc 版本 = beta04，无 beta03 残留）；六镜像自检：0xE9
magic @0x0/0x20000 全过、版本串 beta04 在/beta03 不在 ×9 镜像、
partitions.bin/ota_data_initial.bin 三 env 逐字节一致、uiBuildId =
6.0.0-beta04-lilygo_t2can_dual-7ccc3fc。

## 实车检查项

1. **DND 验收=耳朵**：触发告警 → DND 补偿 → 听音量是否稳定（不看
   诊断计数）；
2. 爆闪开关打开 → 车断电上下电 → 开关仍开、参数仍对；
3. 若爆闪仍"跑一阵死"：读 `/bus2_ids` 的 `can2.txerr` +
   `t2canBus2RecoveryCount`（bus-off 复发排查线）。

## 文件清单（11）

三平台各 bootloader / ota_app / full_merged + 共享 partitions.bin /
ota_data_initial.bin（三环境逐字节一致）。SHA256SUMS.txt 含全部 11 行。
单 CAN 两平台的 app 与 beta03 行为完全一致（新增代码全在双 CAN
ifdef 内，尺寸差异仅来自 uiBuildId stamp）。
