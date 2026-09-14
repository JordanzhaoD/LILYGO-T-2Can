# LILYGO T-2CAN 固件资产 v4.5.0-beta02（2026-09-11）

**相对 v4.5.0-beta01-calibrated 的变化（Jordan 2026-09-11 四项决定）：**

1. **UI 改名**：`AP 取消重请求` 开关 → **`8.3.6防甩机制`**（后端键 `ap_re_request`
   与接线不变，纯改名）。
2. **删除 #108 旧防甩家族**：Instant Engage / Minimal Inject / Soft Engage 全部
   移除（模块、UI 卡片、NVS、JSON、POST 参数、测试环境）——实车证明它们无法
   杜绝 2026.8.3.6 激活甩动，由 AP 取消+重请求协调器（唯一激活权威）取代。
   **Abort Guard 有意保留**：它是 0x399 AP state 8/9 后锁存、<2 干净退出才解锁
   的安全护栏（防中止后继续注入），不是防甩机制；协调器注入路径经过它。
3. **AP 门控三控件迁入 FSD 防护页顶部**：AP 注入门控 / 延迟注入（0-3000ms）/
   AP 自动恢复，与 8.3.6防甩机制 开关同页，统一 `saveApGateControls()` 保存；
   硬件页只留 AP 状态恢复卡，状态页控制区改为只读指引。
4. **资格窗口 1200ms 定案**（用户 2026-09-11 同意；原冻结候选 500 已否决，
   证据：原生 0x3EE mux0 载波 1Hz，500ms 窗口约半数轮次超时）。

本资产生产时未烧录、未接车。上一版本保留在 `../4.5.0-beta01_20260911-calibrated/`。

## 构建信息

- 环境：`lilygo_t2can_dual`（PlatformIO，ESP32-S3 N16R8，ESP-IDF）
- 分区表：`partitions_16mb_ota_4096k_nvs64.csv`（16MB flash，app0@0x20000）
- app 镜像 1,218,896 B（flash 占用 ~29.0% / 4,194,304 B app 区）
- 构建前验证：Python 合约 225 tests OK（bump VERSION 后重跑）；
  native **20 环境 561 cases 全绿**（`native_legacy_steer_defense` 环境已随功能
  删除）；ESP 构建 SUCCESS
- **版本嵌入复验**：固件内字符串 `4.5.0-beta02` ×7、`4.5.0-beta01` ×0
  （含 esp_app_desc 应用描述符——bump 后清 CMakeCache 重配置重编，避免
  PROJECT_VER 缓存旧值；早期 20260911 目录若构建于 bump 前请以本 README
  的 SHA256 为准重新下载）
- 合并自检：full_merged.bin 偏移 0 处 = 0xE9（ESP32-S3 bootloader 魔数）✅
- SHA256SUMS.txt 五文件全验

## 标定（继承 2026-09-10 实车抓包，不变）

`compiledApReRequestProfile()`：stalkByte 0 / mask 0x03 / FWD=1(取消) / RWD=2(启动) /
sourceBus=CAN_BUS_VEH / **qualificationWindowMs=1200** / cancelEvidenceTimeoutMs=1500 /
requestSeqTimeoutMs=1000 / stalkReleaseGapMs=100 / 取消证据 {1,2} / 故障 {8,9} /
资格集 {2,3}（成员资格，非边沿）。详见上一版 README 的标定表。

## ⚠️ 未验证声明（不变）

- **未烧录、未台架、未实车验证防甩效果。** 抓包与全绿测试证明编码、接线与
  状态机逻辑，**不证明合成取消+重请求能消除方向盘猛甩**。
- 开关默认仍为关。实车两级规程见
  `docs/AP-REREQUEST-VEHICLE-VALIDATION-PROCEDURE.md`（Tier 0 停车检查 →
  Tier 1 封闭场地低速首轮）。

## 文件

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v4.5.0-beta02_full_merged.bin` | 全量镜像（bootloader@0x0 + 分区表 + otadata + app@0x20000） |
| `LILYGO-T-2CAN_v4.5.0-beta02_ota_app.bin` | 仅 app 镜像，网页 OTA 用（保留 NVS/配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | 中间产物（esptool 分步烧录用） |
| `SHA256SUMS.txt` | 全部文件校验和 |

## 烧录方式（二选一）

- **网页 OTA（推荐，保留配置）**：设备网页 → 固件更新 → 上传
  `LILYGO-T-2CAN_v4.5.0-beta02_ota_app.bin`。
- **esptool 全量**：`esptool.py --chip esp32s3 write_flash 0x0
  LILYGO-T-2CAN_v4.5.0-beta02_full_merged.bin`
  （全量会覆盖 NVS，设备配置需重设）。

⚠️ **禁止 `pio run -t upload`**：flash_args 的 app@0x10000 与自定义分区表
app0@0x20000 冲突，会把 app 写进 nvs/otadata 区域损坏板子（beta.33 事故）。
bootloader 必须在 0x0（ESP32-S3 ROM 从 0x0 加载，非经典 ESP32 的 0x1000）。
