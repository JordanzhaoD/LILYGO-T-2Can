# LILYGO T-2CAN 固件资产 v4.5.0-beta05（2026-09-11）

**相对 v4.5.0-beta04 的变化**（基于 2026-09-11 深夜 beta04 首轮 Tier 1 CSV
第二轮锁死的根因修复）：

1. **陈旧 lastTx 锚点修复（本轮唯一代码改动，必修）**：
   - **实车证据链**：轮 1 全链走通后，轮 2 的取消 press 发了计数器 7，
     而车在等 6（原生自己发 6）——车无视我们的帧，冲突检测
     （6≠7 且 6≠8）把整轮 `counterConflict` 锁死，release 从未发出；
   - **根因**：beta04 的环形比较（`ringAhead`）没有时间门控。轮 1 的
     release 计数器（83 秒前）在 4-bit 环上"领先"轮 2 开局的新鲜模板
     恰好 1 格，锚点被陈旧值劫持。这个环形关系后续每轮有 9/16≈56%
     概率命中——不修则多轮测试大概率复发；
   - **修复 = 时间新近性规则**：新 press 只在「自己上一帧比最近一次
     原生模板刷新更新」（原生在我们发送后还没说过话 = beta03 撞号
     场景）时锚定自己上一帧；**其余情况永远锚模板+1**——车接受
     「它自己当前计数器+1」（轮 1 的 press/release/request 三次回显
     全部证明）。`ringAhead` 已删除：时间能区分陈旧与当前，环不能。

2. **同场 CSV 的另一个关键观察（未修，方向已明确）**：轮 1 的
   bit46 注入后 FSD 3→6 激活 115ms——**注入激活 FSD 实车首次证明**；
   但 6 态只保持 250ms 就 6→1 掉线，且 **Jordan 确认全程方向盘没有
   甩动**。这排除了"甩→司机纠正"假设：取消+重请求的**时机假设拿到了
   迄今最强的正面证据（不甩）**；掉线是车自己掉的，最可能方向 =
   单帧 bit46 断言不足（掉线发生在注入后 365ms，下一个 1Hz 载波前
   634ms）。鉴别手段见下"下轮录制"。

**新时序体感（与 beta04 相同，无变化）**：AP 激活 → 自己取消
（约 0.2-0.5s）→ 静默等车回可用态（约 1-1.5s，此间 AP 保持关闭、
设备零发送）→ 自己重新请求恢复（约 0.2-0.5s）→ bit46 注入。

**下轮 Tier 1 录制过滤**：`45,399,3EE,488`（**新增 488** =
DAS_steeringControl 转向扭矩，掉线瞬间有无物理转向动作的直接证据，
用于鉴别"车自掉/断言不足" vs 其它原因）。

本资产生产时未烧录、未接车。上一版本保留在 `../4.5.0-beta04_20260911/`。

## 构建信息

- 环境：`lilygo_t2can_dual`（PlatformIO，ESP32-S3 N16R8，ESP-IDF）
- 分区表：`partitions_16mb_ota_4096k_nvs64.csv`（16MB flash，app0@0x20000）
- app 镜像 1,220,032 B（与 beta04 同尺寸；full_merged 1,351,104 B）
- 构建前验证：Python 合约 226 tests OK（bump VERSION 后重跑）；native
  **20 环境全 PASS**（含 native_ap_rerequest **46 cases**，新增 1 个：
  陈旧 lastTx 回归——完整轮 1 → 83s 原生流量 → 轮 2 模板环形落后
  stale lastTx 1 格，press 必须锚模板+1、车回显不得误判冲突、release
  必须照常发出；合计 583 cases）；ESP 双构建 SUCCESS（lilygo_t2can_dual
  + waveshare_single_can_standalone）
- **版本嵌入复验**：固件内字符串 `4.5.0-beta05` ×7、`4.5.0-beta04` ×0
  （双环境同验；bump 后已清 CMakeCache 重配置，规避 PROJECT_VER 缓存）
- 合并自检：full_merged.bin 偏移 0 处 = 0xE9（ESP32-S3 bootloader 魔数）✅
- SHA256SUMS.txt 五文件全验

## 标定（继承 2026-09-10 实车抓包 + 2026-09-11 Tier 1 修正，本版未动）

`compiledApReRequestProfile()`：stalkByte 0 / mask 0x03 / FWD=1(取消) /
RWD=2(启动) / sourceBus=CAN_BUS_VEH / qualificationWindowMs=1200 /
cancelEvidenceTimeoutMs=1500 / requestSeqTimeoutMs=1000 /
stalkReleaseGapMs=100 / 取消证据 {1,2} / 故障 {8,9} / 资格集 {2,3} /
重请求可用态集 requestApMask={2} / 等可用超时 availableWaitTimeoutMs=3000 /
延续集 {2..6}。详见 beta01-calibrated README 的标定表。

## ⚠️ 未验证声明

- **未烧录、未台架、未实车验证防甩效果。** 轮 1 的"无甩动"是单车、
  当日、单次观察（Jordan 体感证词），不构成"方向盘猛甩已修复"的证明；
  多轮重复验证前不得如此表述。
- 已实车证明：合成取消/重请求手势车接受并回显；bit46 资格注入可激活
  FSD（3→6，115ms）。未证：激活后能稳定保持（轮 1 仅 250ms）、
  多轮不再锁死（beta05 修复的软件验证已过，实车待验）。
- 开关默认仍为关。实车两级规程见
  `docs/AP-REREQUEST-VEHICLE-VALIDATION-PROCEDURE.md`（Tier 0 停车检查 →
  Tier 1 封闭场地低速；**门控必须开**）。

## 文件

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v4.5.0-beta05_full_merged.bin` | 全量镜像（bootloader@0x0 + 分区表 + otadata + app@0x20000） |
| `LILYGO-T-2CAN_v4.5.0-beta05_ota_app.bin` | 仅 app 镜像，网页 OTA 用（保留 NVS/配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | 中间产物（esptool 分步烧录用） |
| `SHA256SUMS.txt` | 全部文件校验和 |

## 烧录方式（二选一）

- **网页 OTA（推荐，保留配置）**：设备网页 → 固件更新 → 上传
  `LILYGO-T-2CAN_v4.5.0-beta05_ota_app.bin`。**设备上如果还是 beta04，
  请务必重烧 beta05**——轮 2 锁死是 beta04 自身 bug，beta05 是本次
  验证对象。
- **esptool 全量**：`esptool.py --chip esp32s3 write_flash 0x0
  LILYGO-T-2CAN_v4.5.0-beta05_full_merged.bin`
  （全量会覆盖 NVS，设备配置需重设）。

⚠️ **禁止 `pio run -t upload`**：flash_args 的 app@0x10000 与自定义分区表
app0@0x20000 冲突，会把 app 写进 nvs/otadata 区域损坏板子（beta.33 事故）。
bootloader 必须在 0x0（ESP32-S3 ROM 从 0x0 加载，非经典 ESP32 的 0x1000）。
