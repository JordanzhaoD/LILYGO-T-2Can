# 固件资产 v6.0.0-beta01（2026-10-22）— 全线三环境

**大版本 6.0 第一个 beta**：beta04 的 UI 精简八任务 + 风格回退 + 白天/i18n
根治，加上 **8.3.6 防甩机制取消循环根因修复（Plan A）**——驾驶引擎行为
变更，版本基线重置进入 6.0 线。

## Plan A：防甩取消循环修复（本次核心）

2026-09-28 实车录制（(7)/(8) 段）定案：防甩开启时 FSD 存活率 ≈ 0%。
根因 = 重挂成功后 5s 死线超时 `fullReset("timeout")` 清零 `cycles_` →
车自行 s2→s3 重啮合（杆位保持、零人工）开新周期 → s6 车道捕捉又被
0x41 取消——25 次捕捉 14 次死于 1-19ms，10-13s/轮自驱动循环。

修复：`landedThisSession_` 标志——本驾驶会话内一次 3/6 落地后，取消门
跨超时保持耗尽（一次会话最多一取消 + 一重挂，LittleGong 原意）；仅会话
边界（Park 边沿 / permit 丢失 / 门控关 / 开关关 / apError）重新武装。
Park 边沿在 Idle 态也无条件重开预算（防锁定跨停车存活）。
45° 中止 / 90° 复位 / 47ms 载波锁 / 泵参数全部零改动。

## beta04 八任务（一并包含）

1. 紧急控制两按钮进手机「现场遥控」页（车机视觉不变）
2. 删除「灯光特技」页（方案A：仅 UI + fog/strobe_cont 后端；burst/stalk/
   service_mode 零改动保留）
3. 删除「功耗管理」整模块（前后端+手机）
4. 首页驾驶状态中心：新增 CAN2 块（5 列满行）+ 字号自适应防折行
5. 速度策略页：「Legacy 智能速度偏移」区上移至状态显示区下方
6. CAN2 控制页：Service Mode 区上移 + svc-mode-status 状态行
7. 删除「Legacy 0x370 NAG 主路径」功能区（引擎休眠，bionicSteering
   默认 false 永久关闭）
8. 白天模式根治 + i18n 根治 + 视觉风格回退 beta03 圆角紫青玻璃
   （brutalist 初版被否决；保留 mono 数据字体与 micro-label 字距）

## 本目录内容（三环境全套）

| 文件 | 环境 | 用途 |
|---|---|---|
| `LILYGO-T-2CAN_v6.0.0-beta01_full_merged.bin` | lilygo_t2can_dual | esptool 全量 @0x0（清配置） |
| `LILYGO-T-2CAN_v6.0.0-beta01_ota_app.bin` | lilygo_t2can_dual | WebUI OTA（保配置） |
| `*_bootloader.bin`（按环境命名 ×3） | 各环境自身 | 分组件烧录（三环境 sdkconfig 各异，不通用） |
| `partitions.bin` / `ota_data_initial.bin` | 三环境通用 | 分组件烧录（逐字节比对跨环境一致，一份通用） |
| `Waveshare-S3-RS485-CAN_v6.0.0-beta01_*.bin` | waveshare_ESP32_S3_RS485_CAN | 同上两种 |
| `Waveshare-SingleCAN_v6.0.0-beta01_*.bin` | waveshare_single_can_standalone | 同上两种 |

三环境同为 ESP32-S3 / 16MB / `partitions_16mb_ota_4096k_nvs64.csv`
（bootloader@0x0, partitions@0x8000, otadata@0x19000, app0@0x20000）。

## 验证记录

- 防甩修复 TDD：+2 native 测试（落地锁定跨超时 / Park 重开预算）+
  landing_split 改走 Park 边界 + reengage_success 钉「无二次取消」；
  jitter native 44/44，native 全 21 env PASSED，pytest 228+690 subtests
- ESP 3 发布环境重建 SUCCESS（清 CMakeCache）；六镜像自检：三
  full_merged 0xE9@0x0 + 0xE9@0x20000；三 OTA app 版本串 6.0.0-beta01
  在场 / 5.0.0-beta04 = 0 / uiBuildId=1ecbc3c；三 app gzip UI blob 解压
  逐字节同源；8b 红 token（E61919/FF2A2A/C61414）全零；紫 #7c3aed ×2 +
  radius-xl 在场；删除模块 token（pg-strobe / power_mgmt /
  def-bionic-tgl / legacy-nag-strategy）不在镜像；新 token（s-can2 /
  fitCockpitVals / svc-mode-status / html.light / ov-can-row）全在场
- **bootloader 本次为清缓存后重新生成，哈希与 beta04 不同**（三环境仍
  各异如常；0xE9 结构自检过）。若从 beta03/beta04 全量升级，建议连
  bootloader 一起刷（四段链），保持构建链自洽
- partitions / otadata 跨环境逐字节一致（通用一份）
- latest 三件指针已指向本版（LILYGO-T-2CAN_latest_*）

## 升级（从 beta02/beta03/beta04）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- 全量升级：esptool 四段链（bootloader@0x0 / partitions@0x8000 /
  otadata@0x19000 / app@0x20000），注意会清 NVS 配置
- 烧后验证：版本 = 6.0.0-beta01；侧栏无「灯光特技」；FSD 防护页无
  Legacy NAG 区；首页驾驶状态中心 5 块（双 CAN）

## 实车验证要点（Plan A）

- 防甩开启 + FSD 激活：录制确认 0x41 取消每会话至多一次、重挂后的
  s6 不再被取消、FSD 存活时长从秒级恢复到正常水平
- 中途停车再出发：新会话防甩应恢复（首次 s6 仍会预防性取消）
- diag 观察：`cycles` 在落地后跨周期保持 ≥1（锁定的可观测信号）

## 边界

- 已 commit（本仓库）；**未 push、未烧录、未 OTA** —— 待另行批准
- 驾驶引擎改动范围：仅 `dash_jitter_procedure.h`（landedThisSession_ +
  fullReset 增参 + Park Idle 重开）；NAG v4 / bionic steer / 速度偏移 /
  AbortGuard 零改动
