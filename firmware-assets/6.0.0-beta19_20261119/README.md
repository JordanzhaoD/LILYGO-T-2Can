# 固件资产 v6.0.0-beta19（2026-11-19）— 左滚轮免打扰首击时序修复 + UI 移除 bit27 状态克隆开关

**本目录为 LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `2970137`（main）。app_desc.version = `6.0.0-beta19`，
uiBuildId = `6.0.0-beta19-lilygo_t2can_dual-297013704afd-2026-11-19T13:29:57Z`。

## beta19 修了什么（相对 beta18 的变化）

### 修复：左滚轮免打扰「时灵时不灵」——首击 tick 过早，命中率仅 ~1/3

12 段实车录制（2026-11-19）逐帧解码，得出**无一例外**的确定性规律：

- 车端 nag 清除存在 **~930ms 生效延迟（debounce）**：警告（handsState≥3）
  出现后，车端需约 930ms 才进入「接受滚轮 tick 清除」的窗口；
- tick 在**警告沿后 0–135ms** 发出 → **13/13 全部失效**；
- tick 在**警告沿后 927–976ms** 发出 → **7/7 全部清除**（中位 934ms）。

beta02 引擎在警告沿（0–2ms）**立即**发第一对 +1/−1 tick，第一对永远浪费；
清除全靠 800ms cooldown 后重试的第二对侥幸落进窗口。当警告在窗口外升级、或
状态机因 handsState 1↔2 抖动卡死、第三对又回到新警告沿 0ms 时，nag 一路升级
到红轮——这就是「有时有效、有时失效」的本质。

**修复**：首击 +1 tick 从「警告沿立即发」改为「警告沿后延迟 ~930ms 发」，直接
命中清除窗口中心、一次到位。改动为纯时序优化——**不动** tick 编码、滚动
计数器、CRC8-J1850、+1/−1 平衡配对引擎，也不动 state 7 永不注入栅。

新增常量 `kFirstTickDelayMs = 930`（实测中位 934ms，范围 927–976ms），
方便按车/固件代际在田野微调。

顺带修复：`resetRuntime()` 未重置 `lastHandsState_`（strike-out 沿锁存），
红轮（state 7）出现后重开开关会吞掉下一次 7→沿检测；现在已补上。

### UI：移除「Legacy 状态克隆免打扰」开关

0x045 bit27 状态克隆路线**暂停**（等参考固件确认信号位与发送节奏）。开关已从
防御 UI 移除（HTML 行 + JS 读写 + i18n 词条），**无法再开启**。后端字段
（`def_c27` / `clone27_dnd`）与引擎（`dash_legacy_045_bit27_clone.h`）保留：
UI 无来源 → 字段恒 false → 引擎停摆 → **不再发出任何 0x045 克隆帧**，规避
bit27 帧与原生帧 counter 重号（实测 74–99%）触发车端 fault 的风险。

## 文件清单

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v6.0.0-beta19_ota_app.bin` | OTA 应用镜像（WebUI OTA / 在跑旧版时单写 0x20000） |
| `LILYGO-T-2CAN_v6.0.0-beta19_full_merged.bin` | USB 全新整刷合并镜像（从 0x0 一次写入） |
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
  0x20000 LILYGO-T-2CAN_v6.0.0-beta19_ota_app.bin
```

或直接 `write_flash 0x0 LILYGO-T-2CAN_v6.0.0-beta19_full_merged.bin`。
在跑旧版的板子升级：WebUI OTA 上传 ota_app，或 `write_flash 0x20000` 单写。
刷后核对 `/status` 的 `uiBuildId` 以 `6.0.0-beta19-lilygo_t2can_dual-` 开头。

## 分区表（16MB，与既有布局逐项一致）

nvs@0x9000 64K · otadata@0x19000 8K · app0@0x20000 4M · app1@0x420000 4M ·
spiffs@0x820000 7936K · coredump@0xFE0000 128K

## 构建与验证

- 构建：**clean 重建**（改 VERSION 必须 clean，否则 CMakeCache 里的 PROJECT_VER
  停留在旧版本）；PlatformIO + ESP-IDF 6.0.1，merge 工具 esptool 5.6.0
  （S3 配方 0x0/0x8000/0x19000/0x20000）。
- 门禁：`native_legacy_wheel_045_dnd` **20/20**（含新增
  `test_first_tick_deferred_to_nag_clear_window` 与
  `test_reset_runtime_clears_strike_out_edge_latch`）；`lilygo_t2can_dual`
  构建 SUCCESS。
- 镜像自检（全绿）：0x0=E9 · 0x8000=AA50 · 0x20000=E9 ·
  app_desc.version=`6.0.0-beta19`（uiBuildId `…-297013704afd-…`）·
  SHA256SUMS 五文件逐一复核 ✓。
- app 1,228,288 B（app0 预算 4M 内）；full_merged 1,359,360 B。

## 左滚轮免打扰使用说明（实车两阶段）

1. **短途第一阶段**：打开「Legacy 左滚轮免打扰」开关，AP 激活行驶——盯仪表，
   任何新告警 = 立即关开关并抓 `/status`。正常表现：warningEpisodes 计数增长，
   `balancedPairs` 遥测保持，nag 在 ~930ms 后被一次性清除。
2. **长途第二阶段（1h+）**：NAG 出现即被清除、不升级红轮、音量不漂移。
3. 跑完抓 `/status` 的 `wheelDndDiag` 回传分析。

## 备注

- bit27 状态克隆开关已从 UI 移除，`/status` 的 `clone27Diag.enabled` 恒为
  false（诊断保留，供确认引擎停摆）。参考固件确认后重新上 UI 时，反向恢复
  本目录 diff 对应的 UI 行即可。
- P4 / 微雪板按同流程重建（共享层改动）。
