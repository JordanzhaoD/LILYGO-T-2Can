# 固件资产 v5.0.0-beta02（2026-09-20）— 全线三环境

**相对 v5.0.0-beta01 的变化**：修复功耗管理两开关在 UI 上无法生效的
bug + 功耗卡迁至硬件页（手机可达）。

## 根因（beta01 烧录实测发现）

功耗开关调用 `postForm('/power_mgmt','autoShutdown='+on)` 传的是**字符串**，
而 `postForm` 的实现用 `for(var k in data)` 遍历**对象**键——JS 里
`for...in` 遍历字符串会迭代**字符下标**，请求体退化成 `0=t&1=r&2=u&3=e`
乱码。后端 `hasArg("autoShutdown")` 永远 false → 什么都不存 → 轮询把
开关弹回关闭态。**该 bug 自 Phase 1 功耗 UI 引入起就存在**（所以开关
一直是"死的"，与 beta01 修的运行时语义是两层问题）。HW3 斜率开关
（`saveHw3Slew`）同款 bug 一并修复。

## 修复内容

1. **三处调用改对象形式**：
   `postForm('/power_mgmt',{autoShutdown:on?'1':'0'})`（wifiAutoOff、
   hw3OffsetSlew 同理）。测试新增守卫：**全 UI 禁止** `postForm` 第二参
   为字符串形式（正则扫描），杜绝同类回归。
2. **功耗管理卡整体迁移** `pg-overview`（desktop-home，手机端
   `display:none` 不可见）→ `pg-hardware`。手机入口：双 CAN「更多 →
   硬件模式」/ 单 CAN「硬件」tab；桌面入口：侧边栏「硬件模式」。
   id 语义化改名 `ov-auto-shutdown/ov-wifi-auto-off/ov-pwr-idle` →
   `hw-*`（单一数据源，不复制卡片）。空闲观测行随卡迁移，后端契约
   零改动。

## 本目录内容（三环境全套）

| 文件 | 环境 | 用途 |
|---|---|---|
| `LILYGO-T-2CAN_v5.0.0-beta02_full_merged.bin` | lilygo_t2can_dual | esptool 全量 @0x0（清配置） |
| `LILYGO-T-2CAN_v5.0.0-beta02_ota_app.bin` | lilygo_t2can_dual | WebUI OTA（保配置） |
| `bootloader.bin` / `partitions.bin` / `ota_data_initial.bin` | lilygo_t2can_dual | 分组件烧录 |
| `Waveshare-S3-RS485-CAN_v5.0.0-beta02_*.bin` | waveshare_ESP32_S3_RS485_CAN | 同上两种 |
| `Waveshare-SingleCAN_v5.0.0-beta02_*.bin` | waveshare_single_can_standalone | 同上两种 |

三环境同为 ESP32-S3 / 16MB / `partitions_16mb_ota_4096k_nvs64.csv`
（bootloader@0x0, partitions@0x8000, otadata@0x19000, app0@0x20000）。
全部镜像自检：`0xE9@0x0` + `0xE9@0x20000`，版本串 ×3、beta01=0。

## 验证

- pytest **237 全绿**（738 子断言；新增 beta02 守卫：对象形式调用钉 +
  字符串形式全灭 + 功耗卡硬件页位置钉 + 旧 id 全灭 + 手机入口钉）
- 模拟器功能实测：POST `autoShutdown=1` / `wifiAutoOff=1` → `/status`
  回读 `true/true`（开关真正存住）；乱码体对照不误存
- 模拟器 UI 冒烟 7/7（卡在硬件页、双手机导航入口、旧 id 全灭）
- JSON 重建器 ALL PARSE CLEAN；minify --check；发版元数据门通过
- ESP 3 发布环境重建 SUCCESS（CMakeCache 已清），三镜像魔数自检过

## 烧录（从 beta01 升级）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- 烧后验证：版本 = 5.0.0-beta02；硬件页见「功耗管理」卡；点开
  「自动关机」→ 刷新页面开关**保持开启**（不再弹回）；`/status` 中
  `"autoShutdown":true`
- 之后按 beta01 README 的功耗台架清单验运行时行为（STA 保持关闭
  >1 分钟、deep sleep 唤醒等）

## 边界

- 已 commit（本仓库）；**未 push、未发布 GitHub Release**
- JITTER 防甩机制零改动（beta15 冻结件）
