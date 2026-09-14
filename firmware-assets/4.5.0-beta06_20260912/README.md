# LILYGO T-2CAN 固件资产 v4.5.0-beta06（2026-09-12）

**相对 v4.5.0-beta05 的变化**（基于 2026-09-11 beta05 三段 Tier 1 CSV 的
三项已批准修复 + 一项用户决定的简化）：

1. **资格窗口 1200 → 2400ms**（修复 1，必修）：
   - **实车证据链（f2 轮 A）**：窗口内唯一的 mux0 载波落在
     press→release 的 100ms 间隙里（被回显窗口吞掉），下一载波距
     t_request 整 2000ms——1200ms 窗口仍会过期。该形态 ~12% 轮次命中；
   - 2400ms 覆盖观测最坏值（mux0→mux0 最长 2s）留余量；更长窗口带来的
     AP 退出竞态由 `apExitedDuringWindow` 即时围栏 + 自动重武装兜底；
     窗口语义（半开、自 t_request）冻结不动。

2. **车辆侧锁死自动重武装**（修复 2，Tier 1 最大的实用性修复）：
   - beta05 实车痛点：出错后必须手动 关 8.3.6开关→开 才能继续流程；
   - `windowExpired` / `apExitedDuringWindow` / `apActiveDuringWait` /
     `apFaultState` 四类**车辆侧**异常 → 自动回武装态（WaitDriverIntent，
     与正常完成轮相同围栏：完整 AP 退出 + intent + 激活边沿才开新轮），
     **无需拨任何开关**；
   - 上限：连续 3 次（中间无完成轮）→ 真 FailedLocked
     （`autoRearmLimit`，此时才需开关 关→开）；
   - 硬件类原因（counterConflict/txFailed/permitLost/physicalInput/
     invalidNative045/dasStale/各超时）**仍立即锁死**——fail-closed 不变。

3. **configure() 保留 ActiveInjection 轮次**（修复 3）：
   - beta05 轮次中途"注入停止但车仍保持 6 态"的主嫌疑 =
     `/config` POST 触发的全量重置杀了 bit46 断言链；
   - 保留条件严格：同 enable + 同门控 + **字节同一** profile 且
     ActiveInjection 阶段才早退保留；任何真实模式变化仍全量重置；
     握手阶段永不保留。

4. **AP 注入门控延时链整体删除**（用户决定，简化）：
   - 1/2/3s「延迟注入」只为旧直通路径防甩而生；防甩由本机制负责后属
     冗余。UI 下拉、`/config` 的 `ap_delay_ms`、NVS `ap_dly`（载入时
     主动清残键）、settle 计时器、`legacy_ap_state_delay_gate` 变体、
     simulator 镜像全链移除；
   - 删除后「门控开 + 8.3.6开关关」= 0x399 确认 AP active 后**立即**放行。

**健壮性增强（随本版一并实施）**：`lastEndedReason`（跨重武装/状态变化
保留的结束原因——beta05"先记 reason 再动开关"教训的字段级修复）+
`autoRearms` 计数进 /status、/defense_config、UI 诊断网格（上次结束原因/
自动重武装两格）；`cancelEvidenceSeen` 跨轮清理（屏障 B：陈旧锁存 + 轮 2
等待期的 active 观测曾会假锁 `apActiveDuringWait`）；configure 严格
memcmp 条件；NVS 残键清理。

**新时序体感（与 beta05 相同，无变化）**：AP 激活 → 自己取消
（约 0.2-0.5s）→ 静默等车回可用态（约 1-1.5s）→ 自己重新请求恢复
（约 0.2-0.5s）→ bit46 注入。**出错后不再需要拨开关**（车辆侧异常自动
重武装，连续 3 次才锁）。

**下轮 Tier 1 录制过滤**：`45,399,3EE,488`（488 = 转向扭矩，鉴别
"车自掉/断言不足" vs 物理转向输入）。

本资产生产时未烧录、未接车。上一版本保留在 `../4.5.0-beta05_20260911/`。

## 构建信息

- 环境：`lilygo_t2can_dual`（PlatformIO，ESP32-S3 N16R8，ESP-IDF）
- 分区表：`partitions_16mb_ota_4096k_nvs64.csv`（16MB flash，app0@0x20000）
- app 镜像 1,219,552 B；full_merged 1,350,624 B
- 构建前验证：Python 合约+回归 **228 tests OK**（bump VERSION 后重跑；
  新增 beta06 钉测 2 个：自动重武装/configure 保留/诊断链、延时链端到端
  移除）；native **19 环境全 PASS**（含 native_ap_rerequest **54 cases**，
  新增 8 个：窗口过期重武装+轮 2 完整围栏、重武装上限→autoRearmLimit、
  完成轮清预算、硬件类仍锁、轮 2 锁存新鲜性、configure 三态）；
  ESP 双构建 SUCCESS（lilygo_t2can_dual + waveshare_single_can_standalone）
- **版本嵌入复验**：固件内字符串 `4.5.0-beta06` ×7、`4.5.0-beta05` ×0
  （双环境同验；bump 后已清 CMakeCache 重配置，规避 PROJECT_VER 缓存）
- 合并自检：full_merged.bin 偏移 0 处 = 0xE9（ESP32-S3 bootloader 魔数）✅
- SHA256SUMS.txt 五文件全验

## 标定（继承 2026-09-10 实车抓包 + 2026-09-11 Tier 1 修正，本版仅改窗口）

`compiledApReRequestProfile()`：stalkByte 0 / mask 0x03 / FWD=1(取消) /
RWD=2(启动) / sourceBus=CAN_BUS_VEH / **qualificationWindowMs=2400**（本版
唯一标定变化）/ cancelEvidenceTimeoutMs=1500 / requestSeqTimeoutMs=1000 /
stalkReleaseGapMs=100 / 取消证据 {1,2} / 故障 {8,9} / 资格集 {2,3} /
重请求可用态集 requestApMask={2} / 等可用超时 availableWaitTimeoutMs=3000 /
延续集 {2..6}。详见 beta01-calibrated README 的标定表。

## ⚠️ 未验证声明

- **未烧录、未台架、未实车验证防甩效果。** beta05 轮 1 的"无甩动"是
  单车、当日、单次观察（Jordan 体感证词），不构成"方向盘猛甩已修复"
  的证明；多轮重复验证前不得如此表述。
- 已实车证明：合成取消/重请求手势车接受并回显；bit46 资格注入可激活
  FSD（3→6，115ms）。未证：激活后能稳定保持（beta05 轮 1 仅 250ms）、
  自动重武装在实车上的恢复体验、2400ms 窗口的实际命中率（软件验证已
  过，实车待验）。
- 开关默认仍为关。实车两级规程见
  `docs/AP-REREQUEST-VEHICLE-VALIDATION-PROCEDURE.md`（Tier 0 停车检查 →
  Tier 1 封闭场地低速；**门控必须开**）。

## 文件

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v4.5.0-beta06_full_merged.bin` | 全量镜像（bootloader@0x0 + 分区表 + otadata + app@0x20000） |
| `LILYGO-T-2CAN_v4.5.0-beta06_ota_app.bin` | 仅 app 镜像，网页 OTA 用（保留 NVS/配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | 中间产物（esptool 分步烧录用） |
| `SHA256SUMS.txt` | 全部文件校验和 |

## 烧录方式（二选一）

- **网页 OTA（推荐，保留配置）**：设备网页 → 固件更新 → 上传
  `LILYGO-T-2CAN_v4.5.0-beta06_ota_app.bin`。
- **esptool 全量**：`esptool.py --chip esp32s3 write_flash 0x0
  LILYGO-T-2CAN_v4.5.0-beta06_full_merged.bin`
  （全量会覆盖 NVS，设备配置需重设）。

⚠️ **禁止 `pio run -t upload`**：flash_args 的 app@0x10000 与自定义分区表
app0@0x20000 冲突，会把 app 写进 nvs/otadata 区域损坏板子（beta.33 事故）。
bootloader 必须在 0x0（ESP32-S3 ROM 从 0x0 加载，非经典 ESP32 的 0x1000）。
