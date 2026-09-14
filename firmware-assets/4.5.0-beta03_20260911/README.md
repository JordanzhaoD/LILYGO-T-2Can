# LILYGO T-2CAN 固件资产 v4.5.0-beta03（2026-09-11）

**相对 v4.5.0-beta02 的变化（Jordan 2026-09-11 定义的整体框架）：**

1. **AP 注入门控成为 8.3.6防甩机制的运行前提**：
   - 门控关 = 机制**完全静默**（`phase=disabled`、`reason="apGateOff"`、零
     0x045 发送、零拦截）——此时 Legacy 走原始直通路径，这是**非 8.3.6
     车型的正常设计模式**（用强制激活即可，不甩）；
   - 门控关时「8.3.6防甩机制」开关**仍可拨动**（记住选择，状态行显示
     「不可用 · AP 门控未开启」），门控重开**自动恢复武装**（WaitDriverIntent），
     无需重拨开关，永不进入 FailedLocked（模式切换非故障）；
   - `permit` 刻意**不含**门控——门控切换经 configure 全重置，永不触发
     permitLost 锁死；
   - 模块内三重独立防线（active / activationAllowed+inhibitDevice3ee /
     ownsStalk+nextStalkFrame guard+startRound 边沿）各含 apGateOpen 检查，
     任一漏改也不激活。
2. **FSD 防护页重排**（零 id 增删）：FSD 防护总开关（置顶，仅联动 Ban
   Shield，不管辖 8.3.6机制）→ AP 注入门控/延迟/自动恢复三卡 →
   8.3.6防甩机制开关 + 诊断网格。总览文案同步。
3. **诊断新增**：`apGateOpen` 布尔键 + `unavailableReason="apGateOff"`
   （profile 错误仍优先）。

本资产生产时未烧录、未接车。上一版本保留在 `../4.5.0-beta02_20260911/`。

## 构建信息

- 环境：`lilygo_t2can_dual`（PlatformIO，ESP32-S3 N16R8，ESP-IDF）
- 分区表：`partitions_16mb_ota_4096k_nvs64.csv`（16MB flash，app0@0x20000）
- app 镜像 1,219,152 B（flash 占用 ~29.0% / 4,194,304 B app 区）
- 构建前验证：Python 合约 226 tests OK（bump VERSION 后重跑）；
  native **19 环境 19×PASS 全绿**（含新增 6 个门控用例，41 cases in
  native_ap_rerequest）；ESP 双构建 SUCCESS（lilygo_t2can_dual +
  waveshare_single_can_standalone——native 不编译 dashboard 头，3 参
  configure 调用由固件编译验证）
- **版本嵌入复验**：固件内字符串 `4.5.0-beta03` ×7、`4.5.0-beta02` ×0
  （双环境同验；bump 后已清 CMakeCache 重配置，规避 PROJECT_VER 缓存）
- 合并自检：full_merged.bin 偏移 0 处 = 0xE9（ESP32-S3 bootloader 魔数）✅
- SHA256SUMS.txt 五文件全验

## 标定（继承 2026-09-10 实车抓包，不变）

`compiledApReRequestProfile()`：stalkByte 0 / mask 0x03 / FWD=1(取消) / RWD=2(启动) /
sourceBus=CAN_BUS_VEH / **qualificationWindowMs=1200** / cancelEvidenceTimeoutMs=1500 /
requestSeqTimeoutMs=1000 / stalkReleaseGapMs=100 / 取消证据 {1,2} / 故障 {8,9} /
资格集 {2,3}（成员资格，非边沿）。详见 beta01-calibrated README 的标定表。

## ⚠️ 未验证声明（不变）

- **未烧录、未台架、未实车验证防甩效果。** 抓包与全绿测试证明编码、接线与
  状态机逻辑，**不证明合成取消+重请求能消除方向盘猛甩**。
- 开关默认仍为关。实车两级规程见
  `docs/AP-REREQUEST-VEHICLE-VALIDATION-PROCEDURE.md`（Tier 0 停车检查含
  新增门控关→开验证 → Tier 1 封闭场地低速首轮，**门控必须开**）。

## 文件

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v4.5.0-beta03_full_merged.bin` | 全量镜像（bootloader@0x0 + 分区表 + otadata + app@0x20000） |
| `LILYGO-T-2CAN_v4.5.0-beta03_ota_app.bin` | 仅 app 镜像，网页 OTA 用（保留 NVS/配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | 中间产物（esptool 分步烧录用） |
| `SHA256SUMS.txt` | 全部文件校验和 |

## 烧录方式（二选一）

- **网页 OTA（推荐，保留配置）**：设备网页 → 固件更新 → 上传
  `LILYGO-T-2CAN_v4.5.0-beta03_ota_app.bin`。
- **esptool 全量**：`esptool.py --chip esp32s3 write_flash 0x0
  LILYGO-T-2CAN_v4.5.0-beta03_full_merged.bin`
  （全量会覆盖 NVS，设备配置需重设）。

⚠️ **禁止 `pio run -t upload`**：flash_args 的 app@0x10000 与自定义分区表
app0@0x20000 冲突，会把 app 写进 nvs/otadata 区域损坏板子（beta.33 事故）。
bootloader 必须在 0x0（ESP32-S3 ROM 从 0x0 加载，非经典 ESP32 的 0x1000）。
