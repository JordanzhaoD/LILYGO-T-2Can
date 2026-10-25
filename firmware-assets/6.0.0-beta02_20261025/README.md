# 固件资产 v6.0.0-beta02（2026-10-25）— 全线三环境

**6.0 线第二个 beta，纯 UI 修复版**：修复 6.0.0-beta01 实车暴露的
「网络设置黑名单/DNS 过滤规则框显示为空」问题。零固件 C++ 改动、
零驾驶引擎改动、零持久化改动——存储链路经真机全链路验证本来就是好的。

## 本次修复：DNS 规则加载失败不再隐形

2026-10-25 实车报告：车内打开网络设置，黑名单框为空、模式卡未停在
保守模式，被解读为「配置保存失败 / 过滤失效」。当天真机（beta01 板）
排查定案：**存储与过滤本体全部正常**（12/4 规则跨重启持久；dig 实测
telemetry.tesla.cn → 0.0.0.0 拦截、白名单放行、无关域名转发）——真正
发生的是打开页面那一刻 `GET /dns_rules` 瞬时失败（车内 STA 断开后
持续扫描回连，ESP32 单射频扫描导致 AP 侧突发丢包），而 UI 对单次
失败完全静默（全局 toast 需连续 3 次失败才弹，任何一次成功轮询即清零
计数），HTML 初始态恰好是「空框 + 激进卡高亮」，酿成假象。

修复（commit 73cad23）：
- `loadGatewayDns` 失败后 500ms 自动重试一次；仍失败则状态芯片转红
  「加载失败 | 点「刷新」重试 | 规则未读取」并清空模式卡高亮
- HTML 初始模式卡改为保守高亮（与出厂自动填入保守 12 条一致）
- 新增 3 条 i18n 词条；设备侧合成 uiBuildId（version-env-<bump-commit>-utc，三环境各自）：
  - 6.0.0-beta02-lilygo_t2can_dual-0a346148ab61-2026-10-25T13:12:40Z
  - 6.0.0-beta02-waveshare_ESP32_S3_RS485_CAN-0a346148ab61-2026-10-25T13:12:21Z
  - 6.0.0-beta02-waveshare_single_can_standalone-0a346148ab61-2026-10-25T13:12:30Z

## 本目录内容（三环境全套）

| 文件 | 环境 | 用途 |
|---|---|---|
| `LILYGO-T-2CAN_v6.0.0-beta02_full_merged.bin` | lilygo_t2can_dual | esptool 全量 @0x0（清配置） |
| `LILYGO-T-2CAN_v6.0.0-beta02_ota_app.bin` | lilygo_t2can_dual | WebUI OTA（保配置） |
| `*_bootloader.bin`（按环境命名 ×3） | 各环境自身 | 分组件烧录（三环境 sdkconfig 各异，不通用） |
| `partitions.bin` / `ota_data_initial.bin` | 三环境通用 | 分组件烧录（逐字节比对跨环境一致，一份通用） |
| `Waveshare-S3-RS485-CAN_v6.0.0-beta02_*.bin` | waveshare_ESP32_S3_RS485_CAN | 同上两种 |
| `Waveshare-SingleCAN_v6.0.0-beta02_*.bin` | waveshare_single_can_standalone | 同上两种 |

三环境同为 ESP32-S3 / 16MB / `partitions_16mb_ota_4096k_nvs64.csv`
（bootloader@0x0, partitions@0x8000, otadata@0x19000, app0@0x20000）。

## 验证记录

- UI 修复三路径 Playwright（模拟器）：正常加载 / 拦截请求→500ms 重试
  →红色错误芯片+双卡不亮（旧版为空框+激进高亮+零提示）/ 解除后恢复
- pytest 228 passed + 690 subtests（含版本钉 6.0.0-beta02）
- ESP 3 发布环境清 CMakeCache 重建 SUCCESS；六镜像自检：三
  full_merged 0xE9@0x0 + 0xE9@0x20000；三 OTA app 版本串 6.0.0-beta02
  在场 / 6.0.0-beta01 = 0；三 app gzip UI blob 解压
  逐字节同源；8b 红 token（E61919/FF2A2A/C61414）全零；紫 #7c3aed
  在场；本次修复新 token（加载失败/点「刷新」重试/规则未读取/
  conservative 初始高亮）全在场
- merge 命令先用 beta01 组件逐字节复现验证后才用于本次封装
- latest 三件指针已指向本版（LILYGO-T-2CAN_latest_*）

## 升级（从 beta01）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`——本版没有任何
  NVS/SPIFFS 布局变更，DNS 规则与防御配置原样保留
- 全量升级：esptool 四段链（bootloader@0x0 / partitions@0x8000 /
  otadata@0x19000 / app@0x20000），注意会清 NVS 配置
- 烧后验证：版本 = 6.0.0-beta02；网络页正常加载时保守卡高亮 + 12/4
  规则在场；断开 WiFi 打开网络页应见红色「加载失败」提示（修复生效的
  直观判据）

## 边界

- 驾驶引擎（jitter Plan A / NAG / 速度偏移 / AbortGuard）与 DNS 后端、
  持久化、NAT 层零改动
- 「左滚轮免打扰」排查结论同日另案：代码路径与 beta03 逐字节相同、
  设备侧开关全开 armed，待下次实车按隔离清单复测（先关 jitter 触发
  nag，读 wheelDndDiag）
