# LILYGO T-2CAN 固件资产 v4.5.0-beta01-calibrated（2026-09-11）

**与 20260910 目录的区别：`compiledApReRequestProfile()` 已填入 2026-09-10
实车抓包标定值——`AP 取消重请求` 开关打开后会真正武装协调器。**
本资产生产时未烧录、未接车。上一个（空 profile、永不武装）版本保留在
`../4.5.0-beta01_20260910/` 供对照。

## 构建信息

- 环境：`lilygo_t2can_dual`（PlatformIO，ESP32-S3 N16R8，ESP-IDF）
- 分区表：`partitions_16mb_ota_4096k_nvs64.csv`（16MB flash，app0@0x20000）
- RAM 17.0%；Flash 28.8%（1,206,783 / 4,194,304 B）
- 构建前验证：Python 合约 225 tests OK；native 20 环境 434 cases 全绿；
  ESP 构建 SUCCESS（含 UI 卡片文案更新为标定后表述后复验）
- 合并自检：full_merged.bin 偏移 0 处 = 0xE9（ESP32-S3 bootloader 魔数）✅
- SHA256SUMS.txt 五文件全验 OK

## 标定来源（2026-09-10 实车抓包，Model X / 2026.8.3.6）

四个录制 CSV（两个静态手势 + 两个行驶 AP 时段；0x045 共 3202 帧
CRC8-J1850 全验证通过，设备零 0x045 发送）：

| profile 字段 | 值 | 证据 |
|---|---|---|
| stalkByte / stalkMask | **0 / 0x03** | idle byte0=0x40 → 前推 0x41、后拨 0x42；byte3 恒 0x30（wheel-DND 猜测证伪） |
| stalkFwdValue / stalkRwdValue | **1 / 2** | 14 次前推全部取消（6→1，161-526ms）；17 次后拨全部启动（AP 2→3，155-236ms） |
| sourceBus | CAN_BUS_VEH | 0x045 原生帧只在 CSV bus 1 |
| qualificationWindowMs | **1200**（偏离冻结候选 500） | 原生 0x3EE mux0 载波 1Hz：间隔中位 1000ms；500ms 窗口 ~50% 轮次超时，1200ms 残余未覆盖 ≤2.6% |
| cancelEvidenceTimeoutMs | 1500 | 观测取消最大 526ms（0x399 为 2Hz）≈3× 裕量 |
| requestSeqTimeoutMs / stalkReleaseGapMs | 1000 / 100 | 自有手势发送界；按压持续 1-3 帧（20-229ms） |
| cancelEvidenceApMask / cancelFaultApMask | {1,2} / {8,9} | 取消直落 6→1；观测 6→8→9 hands-on 级联 |
| postQualApMask | {2,3,4,5,6} | 资格集 {2,3} 冻结在代码；后续 2→3→6（6=行驶常态） |

## ⚠️ 未验证声明（不变）

- **未烧录、未台架、未实车验证协调器行为。** 抓包证明的是物理拨杆编码与
  取消/启动因果，**不证明合成重放能消除方向盘猛甩**。
- 开关默认仍为关。要台架验证：刷入 → 台架 → 打开 `AP 取消重请求` →
  观察诊断 `legacyInjectionSafety.apReRequest`。
- qualificationWindowMs=1200 是对冻结合同候选值 500 的**已标注偏离**
  （窗口语义仍冻结：半开 [t_request, t_request+window)）。Jordan 可否决
  改回 500：只改 `compiledApReRequestProfile()` 一行。

## 文件

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v4.5.0-beta01-calibrated_full_merged.bin` | 全量镜像（bootloader@0x0 + 分区表 + otadata + app@0x20000） |
| `LILYGO-T-2CAN_v4.5.0-beta01-calibrated_ota_app.bin` | 仅 app 镜像，网页 OTA 用（保留 NVS/配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | 中间产物（esptool 分步烧录用） |
| `SHA256SUMS.txt` | 全部文件校验和 |

## 烧录方式（二选一）

- **网页 OTA（推荐，保留配置）**：设备网页 → 固件更新 → 上传
  `LILYGO-T-2CAN_v4.5.0-beta01-calibrated_ota_app.bin`。
- **esptool 全量**：`esptool.py --chip esp32s3 write_flash 0x0
  LILYGO-T-2CAN_v4.5.0-beta01-calibrated_full_merged.bin`
  （全量会覆盖 NVS，设备配置需重设）。

⚠️ **禁止 `pio run -t upload`**：flash_args 的 app@0x10000 与自定义分区表
app0@0x20000 冲突，会把 app 写进 nvs/otadata 区域损坏板子（beta.33 事故）。
bootloader 必须在 0x0（ESP32-S3 ROM 从 0x0 加载，非经典 ESP32 的 0x1000）。
