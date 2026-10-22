# 固件资产 v5.0.0-beta04（2026-10-22）— 全线三环境

**相对 v5.0.0-beta03 的变化**：UI 精简八任务 + 白天模式/中英文切换根治
+ 风格回退（初版 brutalist Tactical Telemetry 被否决，恢复 beta03
圆角紫青玻璃风格）。**防甩 JITTER、速度偏移等驾驶功能引擎零改动**
（仅删 UI/配置面，引擎休眠保留）。

## 八任务一览

1. 紧急控制两按钮进手机「现场遥控」页（车机视觉不变）
2. 删除「灯光特技」页（方案A：仅 UI + fog/strobe_cont 后端；burst/stalk/
   service_mode 零改动保留）
3. 删除「功耗管理」整模块（前后端+手机）
4. 首页驾驶状态中心：新增 CAN2 块（5 列满行）+ 字号自适应防折行（单值
   单行）
5. 速度策略页：「Legacy 智能速度偏移」区上移至状态显示区下方
6. CAN2 控制页：Service Mode 区上移 + svc-mode-status 状态行（零后端改动）
7. 删除「Legacy 0x370 NAG 主路径」功能区（保守：接线全删、引擎休眠，
   bionicSteering 默认 false 永久关闭）
8. 白天模式根治（html.light 完整变量套，纸面亮色 + 紫 accent 同暗色，
   27 处硬编码 rgba 变量化）+ i18n 根治（data-i18n 全量标记 + 词典补全 +
   动态文案 T() + 切换即时重渲染 + pytest 词典守卫）+ 视觉风格：曾实施
   Tactical Telemetry 直角红，评审否决后整体回退 —— 恢复 beta03 圆角
   （xl 28px/lg 20px）、紫 #7c3aed 强调 + 青紫渐变、半透明玻璃面板
   + backdrop blur；保留 mono 数据字体与 micro-label 字距；语义红
   （危险按钮/hold-to-arm/错误态）不动

## 本目录内容（三环境全套）

| 文件 | 环境 | 用途 |
|---|---|---|
| `LILYGO-T-2CAN_v5.0.0-beta04_full_merged.bin` | lilygo_t2can_dual | esptool 全量 @0x0（清配置） |
| `LILYGO-T-2CAN_v5.0.0-beta04_ota_app.bin` | lilygo_t2can_dual | WebUI OTA（保配置） |
| `*_bootloader.bin`（按环境命名 ×3） | 各环境自身 | 分组件烧录（三环境 bootloader sdkconfig 各异，不通用） |
| `partitions.bin` / `ota_data_initial.bin` | 三环境通用 | 分组件烧录（逐字节比对一致，一份通用） |
| `Waveshare-S3-RS485-CAN_v5.0.0-beta04_*.bin` | waveshare_ESP32_S3_RS485_CAN | 同上两种 |
| `Waveshare-SingleCAN_v5.0.0-beta04_*.bin` | waveshare_single_can_standalone | 同上两种 |

三环境同为 ESP32-S3 / 16MB / `partitions_16mb_ota_4096k_nvs64.csv`
（bootloader@0x0, partitions@0x8000, otadata@0x19000, app0@0x20000），
partitions/otadata 两件经逐字节比对跨环境一致；bootloader 三环境各异按环境命名。

## 验证记录

- pytest 218 passed + 690 subtests（含新增 i18n 词典守卫、CAN 行字形自适应
  + 颜色守卫、速度页/CAN2 页顺序断言）
- native 逐环境全绿：642/642 用例 / 22 环境（PIO 逐 env，未用裸 `pio test`；native 测试源码相对 beta03 零改动）
- ESP 3 发布环境重建 SUCCESS（CMakeCache 已清）；六镜像自检：0xE9@0x0 +
  0xE9@0x20000、版本串 beta04 在场 / beta03=0、删除模块 token
  （pg-strobe / power_mgmt / def-bionic-tgl / legacy-nag-strategy）不在
  app 镜像
- 终审（全新上下文审查）Important 修复：1280 下 5 列 CAN 行实值裁剪 →
  .m5 专属字号阶梯（18/16/14/12px）+ ellipsis 兜底 + v-* 颜色复合选择器
  复活（存量缺陷）+ loadTemp 改 classList 增删保留 val/档位类；浏览器最坏
  值矩阵（Offline×2 / Waiting / 999.9 Hz / 100.5°C 红）1280×800 全部
  scrollWidth ≤ clientWidth（fix1-canrow-worstcase-1280.png）
- minified UI 终检：删除项全缺席，s-can2 / fitCockpitVals / vf-md|sm|xs /
  svc-mode-status / ov-can-row / cockpit-metrics.m5 / html.light /
  rebootDevice / resetStats 全在场
- 浏览器双主题×双语四象限目检通过（截图 docs/verification/ui-beta04/，
  不入资产）
- 风格回退重产（2026-10-22 二次构建）：3 env 重编 SUCCESS；三 app 镜像
  gzip UI blob 解压逐字节同源（216KB），8b 红 token（E61919/FF2A2A/
  230,25,25/C61414）全零、紫 #7c3aed ×2（root+light）、radius-xl ×3；
  SHA256SUMS 11 bin 全 OK；回退后浏览器暗/亮 1280 + 移动端 390 computed
  核验（radiusXl 28px / panel rgba(15,23,42,.82) / blur 22px / accent
  紫 / 无横向溢出），console 0 error 0 warning

## 升级（从 beta03）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- 烧后验证：版本 = 5.0.0-beta04；侧栏无「灯光特技」；硬件页无「功耗管理」；
  FSD 防护页无 Legacy NAG 区；首页驾驶状态中心 5 块（双 CAN）
- 主题切换按钮白天模式全页可读；语言切换全页即时生效（含动态状态文案）

## 边界

- 已 commit（本仓库）；**未 push、未烧录、未 OTA** —— 待另行批准
- 驾驶功能引擎（JITTER 防甩、速度偏移、NAG v4、bionic steer）全部休眠
  保留未动；删除仅限 UI 面与 dashboard 配置/诊断发射面
