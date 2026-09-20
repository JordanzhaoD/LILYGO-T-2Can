# 固件资产 v5.0.0-beta01（2026-09-20）— 全线三环境

**相对 v4.5.0-beta15 的变化**（纯 beta15 基线 `7d1be0c` 之上，无 beta16~beta22
任何内容）：面板信息架构重排 + 功耗管理两处运行时修复。JITTER/防甩机制
零改动（beta15 冻结件原样）。

## 六项变更

1. **首页瘦身**：删除「Legacy 运行时状态」卡、「Legacy CAN-A 诊断」卡，
   「FSD 注入策略」卡删 5 个实验开关（保留 3 策略卡 + 4 状态格）
2. **AP 注入安全卡 → FSD 防护页**：落在 AP 门控/AP 自动恢复之下、
   8.3.6 防甩之上；后端 `apInjectionState`/`apAutoRestore` 契约不变
3. **功耗修复 A — WiFi 自动关闭生效**：旧版关 STA 后后台重连轮换数秒内
   拉回（互搏）。现 `dashCheckWifi` 入口压制 + 打开面板 1~2 s 内
   `dashPowerMgmtConsumeStaRestore()` 优雅恢复（`dashScheduleSTAConnect(1500)`）
4. **功耗修复 B — 自动关机唤醒极性 1→0**：CAN 空闲收发器 RX 恒高电平，
   旧极性一睡即醒（表现为"从未关机"）；现仅 dominant 流量（低电平）唤醒
5. **紧急控制（重启/清统计）→ 首页**：桌面/手机双端可见
6. **硬件页收拢**：AP Restore 卡删除（防护页开关成唯一入口）；
   手机 UI 文案同步

**NVS 安全重置**：被删 5 开关的残留持久化由固件首次启动一次性清除
（marker `ui_rst_50b1`）。后端 `/legacy_fsd_config`、`/config` GET、
`fsdRuntime` JSON 全保留（仅 UI 层删除）。

**可观测性**：`/status` 新增 `canIdleMs` / `webIdleMs` / `wifiStaDisabled`；
首页功耗卡新增只读空闲行（`ov-pwr-idle`）。

## 本目录内容（三环境全套）

| 文件 | 环境 | 用途 |
|---|---|---|
| `LILYGO-T-2CAN_v5.0.0-beta01_full_merged.bin` | lilygo_t2can_dual | esptool 全量 @0x0（清配置） |
| `LILYGO-T-2CAN_v5.0.0-beta01_ota_app.bin` | lilygo_t2can_dual | WebUI OTA（保配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | lilygo_t2can_dual | 分组件烧录 |
| `Waveshare-S3-RS485-CAN_v5.0.0-beta01_*.bin` | waveshare_ESP32_S3_RS485_CAN | 同上两种 |
| `Waveshare-SingleCAN_v5.0.0-beta01_*.bin` | waveshare_single_can_standalone | 同上两种 |

三环境同为 ESP32-S3 / 16MB / `partitions_16mb_ota_4096k_nvs64.csv`
（bootloader@0x0, partitions@0x8000, otadata@0x19000, app0@0x20000）。
全部镜像自检：`0xE9@0x0` + `0xE9@0x20000`，版本串 ×3、旧版本 0。

## 验证

- pytest **236 全绿**（738 子断言；含 6 组新钉：极性 0 / STA 压制+恢复 /
  3 新 JSON 键 / NVS 重置 / 页面位置 / 模拟器镜像）
- native **22 环境逐个全 PASS**；JSON 重建器 ALL PARSE CLEAN；
  minify --check；clang-format 棘轮 0 违规
- ESP 3 发布环境全 SUCCESS；模拟器冒烟 11/11（防护页顺序、双端紧急
  控制、功耗行数值跳动）
- 发版元数据门通过（`check_release_metadata.py`）
- 7 个非发布环境失败为**预先存在**（6 缺分区 CSV 自 beta01 + esp32_twai
  框架 tsens 头错），与本次无关

## 烧录（二选一）

- **WebUI OTA**（保配置，推荐）：上传对应环境 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000）

## 烧后验证（功耗台架要点）

1. `curl -s <IP>/status | python3 -m json.tool` 整体可解析；版本 =
   5.0.0-beta01；新键 canIdleMs/webIdleMs/wifiStaDisabled 在
2. **WiFi 自动关闭**（本次修复核心）：关闭全部面板 5 分钟 → 串口
   `[PWR] WiFi STA auto-off`，且 **STA 保持关闭 >1 分钟**（旧版数秒被
   拉回）→ 重开面板 1~2 s 见 restoring 日志、STA 回家网
3. **自动关机**（无 CAN 台架测）：开启后 5 分钟 → deep sleep 日志 →
   发一帧 CAN 应唤醒重启
4. 首页：无 Legacy 运行时状态/CAN-A 诊断卡；紧急控制在；防护页
   AP 门控下方有 AP 注入安全卡
5. 实车注意：Tesla 驻车网关零星流量可能让 5 分钟窗永不满——用
   `/status canIdleMs` 观测；若确如此，超时配置化是后续独立任务

## 版本号说明

Jordan 指示为 5.0-beta01；发版工具 semver 校验强制三段式，落地
**5.0.0-beta01**（VERSION 与 CHANGELOG 一致，CHANGELOG 条目纯英文）。

## 边界

- 已 commit（本仓库）；**未 push、未发布 GitHub Release、未烧录**
- JITTER 防甩机制零改动（beta15 冻结件）
