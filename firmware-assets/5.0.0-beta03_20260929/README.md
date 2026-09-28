# 固件资产 v5.0.0-beta03（2026-09-29）— 全线三环境

**相对 v5.0.0-beta02 的变化**：方案A（EAP 落地边沿补一发 bit46 克隆，重拨直
升 FSD）+ 方案C（Parked 挡位去抖，单帧 P 毛刺不再误触发）。**JITTER 防甩机制
本身零改动**（beta15 冻结件——取消/重请求泵、时序、阈值全部不动）。

## 根据来源（E7/E8 两段实车录制，2026-09-28 分析定案）

1. **方案A**：防甩窗口内人工重拨 13/13 全落 EAP（state3 采集窗口内无 bit46
   —— 被 procedureActive 暂停的正是我方自己的 base 路径）；而车辆已 engage
   状态下 bit46 帧一到，14-104ms 内升级 FSD（11/11）。修复 = state3 落地
   边沿复用现成 one-shot 机制补一发克隆（模板 >5s 失效则跳过，fail-closed）。
   司机不再等 5 秒死线尾部由 base 路径补救。**纯 beta15 血统延伸到新边沿，
   无 beta18 遗产**。
2. **方案C**：E7 录制中单帧 0x186 损坏帧在行车中被解码为 P（t=5.5-8.0s，
   车辆滚动中）→ 复位了正在运行的 JITTER 周期、吞掉重请求、清了会话预算、
   还虚假打开了 AP 注入门。修复 = Parked 需连续 3 帧 P 读数（280/390 两路
   挡位源共享计数）才上升；任何非 P 挡位瞬间清零；已为 true 的 Parked
   （DI 睡眠期 boot 默认）保持不变。

## 诊断新增（/status → legacyInjectionSafety.jitter）

- **`bit46LandingShots`**：EAP 落地边沿发出的克隆计数。**实车期望：
  ≈ `reengagedEap`**（每次 EAP 落地一发，模板失效除外）。
- `bit46Shots` / `bit46Refreshes` 保持分离不变。

## 本目录内容（三环境全套）

| 文件 | 环境 | 用途 |
|---|---|---|
| `LILYGO-T-2CAN_v5.0.0-beta03_full_merged.bin` | lilygo_t2can_dual | esptool 全量 @0x0（清配置） |
| `LILYGO-T-2CAN_v5.0.0-beta03_ota_app.bin` | lilygo_t2can_dual | WebUI OTA（保配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | lilygo_t2can_dual | 分组件烧录 |
| `Waveshare-S3-RS485-CAN_v5.0.0-beta03_*.bin` | waveshare_ESP32_S3_RS485_CAN | 同上两种 |
| `Waveshare-SingleCAN_v5.0.0-beta03_*.bin` | waveshare_single_can_standalone | 同上两种 |

三环境同为 ESP32-S3 / 16MB / `partitions_16mb_ota_4096k_nvs64.csv`
（bootloader@0x0, partitions@0x8000, otadata@0x19000, app0@0x20000）。
全部镜像自检：`0xE9@0x0` + `0xE9@0x20000`，版本串 ×7/镜像、beta02=0、
新键 `bit46LandingShots` ×1/镜像。

## 验证

- pytest **237 全绿**（738 子断言；合约 23 键 + 版本钉 5.0.0-beta03）
- native **672 全绿 / 22 环境**（新增：jitter 落地弹 3 测 —— 发射/计数分离/
  fail-closed 跳过/复位吞弹；hw4 挡位去抖 3 测 —— 单帧毛刺/三帧确认/
  跨源计数）
- clang-format dry-run 五文件全 CLEAN；发版元数据门通过；minify --check 过
- ESP 3 发布环境重建 SUCCESS（CMakeCache 已清），六镜像魔数+版本+新键自检过

## 烧录（从 beta02 升级）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- 烧后验证：版本 = 5.0.0-beta03；`/status` → `legacyInjectionSafety.jitter`
  里出现 `"bit46LandingShots":0`（键在场 = 新固件确认）

## 实车验证点（方案A 生效判据）

1. 拨杆触发防甩 → 窗口内重拨：**直接落 FSD 而非 EAP**（历史 13/13 EAP）
2. 若仍落 EAP：~100ms 内自动升级 FSD（bit46LandingShots 计数 +1）
3. 诊断关系：`bit46LandingShots ≈ reengagedEap`；`bit46Shots` 不变
4. 下次录制 SOP（方案C2 配套）：`ids=045,118,186,399,3EE,488` ——
   0x118/0x186 进白名单后可**直接观测**挡位毛刺（E7 只能推断）

## 边界

- 已 commit（本仓库）；**未 push、未发布 GitHub Release**
- JITTER 防甩机制冻结不动（方案B base-path 解暂停 / 方案D beta18 泵形
  未获批，不在本版）
