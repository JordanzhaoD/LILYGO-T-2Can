# LILYGO T-2CAN 固件资产 v4.5.0-beta04（2026-09-11）

**相对 v4.5.0-beta03 的变化**（基于 2026-09-11 首轮 Tier 1 失败 CSV 的根因
分析，修复三个根因中的两个；取消时机本身不动——实车 22/31ms 就接受了）：

1. **重请求改为等车回可用态再发**（根因 A·时机错误）：
   - 实测：取消被接受后车在 DAS 状态 1 停 ~936-950ms 才回状态 2，而旧版
     在取消回位后 **1ms** 就发重请求——撞在状态 1 上；标定的 17 次成功
     RWD 后拨**全部从状态 2 起步**；
   - 新语义：取消证据只锁存等待（诊断 `reason="waitAvailable"`），等
     `0x399` 回状态 2 才发重请求手势；超时保护 3000ms（≈实测恢复时间
     的 3 倍）；
   - 两个新锁死 reason（fail-closed）：`availableWaitTimeout`（3s 内没回
     可用态）、`apActiveDuringWait`（取消落地后 AP 自己回激活——重请求
     已无意义）；取消证据**之前**的 state 3 仍表示"取消尚未生效"，继续
     等待不误杀（实测取消生效需 161-526ms）。
2. **滚计数器撞号修复**（根因 B）：首轮两轮失败 CSV 均出现相邻 1ms 两帧
   同计数器（8906/8907、84444/84445）。根因 = 新 press 只锚定原生模板
   计数器，而原生总线在 release gap 内克隆了我们的 cancel press 使模板
   推进到与自己上一帧相同。修复 = 新 press 锚定「模板计数器」与「自己
   上一帧计数器」中**环形更靠后者**+1（`ringAhead`，4-bit 环比较），我们
   自己的帧永远前进。
3. **（未修，暂无证据有害）资格窗口误锁死**（根因 C）：取消时机不动——
   边沿后 1ms 发出、22ms 生效，实车已接受。
4. **UI**：协调器状态行新增 `rrPhaseMap` 全量 31 项中文判读（含上述三个
   新 reason），Tier 1 现场不用查英文对照表。

**新时序体感（与 beta03 不同，属设计行为非故障）**：AP 激活 → 自己取消
（约 0.2-0.5s）→ **静默等车回可用态（约 1-1.5s，此间 AP 保持关闭、设备
零发送）** → 自己重新请求恢复（约 0.2-0.5s）→ bit46 注入。

本资产生产时未烧录、未接车。上一版本保留在 `../4.5.0-beta03_20260911/`。

## 构建信息

- 环境：`lilygo_t2can_dual`（PlatformIO，ESP32-S3 N16R8，ESP-IDF）
- 分区表：`partitions_16mb_ota_4096k_nvs64.csv`（16MB flash，app0@0x20000）
- app 镜像 1,220,032 B（flash 占用 ~29.1% / 4,194,304 B app 区）
- 构建前验证：Python 合约 226 tests OK（bump VERSION 后重跑）；native
  **19 环境全 PASS**（合计 441 cases；native_ap_rerequest **45 cases**，
  含新增 4 个：等可用态门控、availableWaitTimeout 半开边界、
  apActiveDuringWait（含取消证据前 state 3 继续等待对照）、计数器撞号
  回归）；ESP 双构建 SUCCESS（lilygo_t2can_dual + waveshare_single_can_
  standalone）
- **版本嵌入复验**：固件内字符串 `4.5.0-beta04` ×7、`4.5.0-beta03` ×0
  （双环境同验；bump 后已清 CMakeCache 重配置，规避 PROJECT_VER 缓存）
- 合并自检：full_merged.bin 偏移 0 处 = 0xE9（ESP32-S3 bootloader 魔数）✅
- SHA256SUMS.txt 五文件全验

## 标定（继承 2026-09-10 实车抓包；**beta04 新增两参数加粗**）

`compiledApReRequestProfile()`：stalkByte 0 / mask 0x03 / FWD=1(取消) /
RWD=2(启动) / sourceBus=CAN_BUS_VEH / qualificationWindowMs=1200 /
cancelEvidenceTimeoutMs=1500 / requestSeqTimeoutMs=1000 /
stalkReleaseGapMs=100 / 取消证据 {1,2} / 故障 {8,9} / 资格集 {2,3}
（成员资格，非边沿）/**重请求可用态集 requestApMask={2}（新）**/
**等可用超时 availableWaitTimeoutMs=3000（新，≈实测 ~940ms 的 3 倍）**。
详见 beta01-calibrated README 的标定表。

## ⚠️ 未验证声明（不变）

- **未烧录、未台架、未实车验证防甩效果。** 抓包、失败 CSV 根因分析与
  全绿测试证明编码、接线与状态机逻辑，**不证明合成取消+重请求能消除
  方向盘猛甩**。
- 开关默认仍为关。实车两级规程见
  `docs/AP-REREQUEST-VEHICLE-VALIDATION-PROCEDURE.md`（已适配 beta04
  新时序：Tier 0 停车检查 → Tier 1 封闭场地低速，**门控必须开**；判读表
  含 waitAvailable / availableWaitTimeout / apActiveDuringWait 三行）。
- 首轮 Tier 1 已证：**合成取消机制实车可用**（车接受了我们的 0x045）；
  未证：重请求能恢复 AP、防甩有效。

## 文件

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v4.5.0-beta04_full_merged.bin` | 全量镜像（bootloader@0x0 + 分区表 + otadata + app@0x20000） |
| `LILYGO-T-2CAN_v4.5.0-beta04_ota_app.bin` | 仅 app 镜像，网页 OTA 用（保留 NVS/配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | 中间产物（esptool 分步烧录用） |
| `SHA256SUMS.txt` | 全部文件校验和 |

## 烧录方式（二选一）

- **网页 OTA（推荐，保留配置）**：设备网页 → 固件更新 → 上传
  `LILYGO-T-2CAN_v4.5.0-beta04_ota_app.bin`。**设备上如果还是 beta03，
  请务必重烧 beta04**——新时序语义是本次验证对象。
- **esptool 全量**：`esptool.py --chip esp32s3 write_flash 0x0
  LILYGO-T-2CAN_v4.5.0-beta04_full_merged.bin`
  （全量会覆盖 NVS，设备配置需重设）。

⚠️ **禁止 `pio run -t upload`**：flash_args 的 app@0x10000 与自定义分区表
app0@0x20000 冲突，会把 app 写进 nvs/otadata 区域损坏板子（beta.33 事故）。
bootloader 必须在 0x0（ESP32-S3 ROM 从 0x0 加载，非经典 ESP32 的 0x1000）。
