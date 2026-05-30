# FSD 源代码合并设计文档

**日期:** 2026-05-31
**项目:** LILYGO T-2CAN v4.01 (Atlas Dashboard)
**参考源:** DouyinFSD v3.68 公开源代码 + FSD操作指南截图 (10张)
**方案:** 方案A — 渐进式补齐 + 精准合并

---

## 1. 目标

将 DouyinFSD v3.68 公开源代码中已验证的功能逻辑精准合并到 Atlas T-2CAN v4.01 项目中，补齐截图中展示但本地项目缺失的界面和固件功能，同时不破坏已测试通过的FSD激活、CAN工具、OTA等核心功能。

## 2. 合并范围

### 2.1 保持不动的部分

- Legacy/HW3/HW4 FSD激活Handler（三轮审计通过）
- CAN2控制/嗅探/录像功能
- OTA升级功能
- NAT网关/DNS代理
- WiFi多SSID管理
- CAN工具（引脚配置/调试）
- pg-network 网络设置页面（完全保留）
- pg-bus2 CAN2控制页面（保留，灯光部分迁出后清理）
- pg-ota OTA升级页面
- pg-can CAN工具页面
- 现有 RESTful API 架构

### 2.2 需要新增的功能

| # | 功能 | 类型 | 源代码关键协议 |
|---|------|------|---------------|
| 1 | 后雾灯爆闪 (0x273) | 固件+UI | CAN-B 0x273帧，data[2]=0x90/0x10切换 |
| 2 | F1领航灯模式 | 固件+UI | 0x273帧，3连闪135ms+1500ms停顿 |
| 3 | 持续无限爆闪 | 固件+UI | 远光灯无限循环模式 |
| 4 | 仿生方向盘扭矩 | 固件重构 | 0x370正弦波随机扭矩(30-55振幅,350-500ms) |
| 5 | 音量消除DND | 固件+UI | 0x3C2 data[2]四步序列(0x01→0x00→0x3F→0x00) |
| 6 | 速度消除DND | 固件+UI | 0x3C2 data[3]四步序列(同上) |
| 7 | AP/EAP兼容DND | 固件+UI | 0x249拨杆检测AP模式 |
| 8 | 自动关机 | 固件+UI | 5分钟无CAN→deep_sleep，TWAI_RX唤醒 |
| 9 | WiFi自动关闭 | 固件+UI | 5分钟无Web→仅关STA保AP（见4.5节说明） |
| 10 | 车辆OTA保护 | 固件 | 0x318帧检测，暂停FSD注入 |
| 11 | 自动换挡占位页 | 仅UI | 储备功能展示，遥测只读 |

### 2.3 需要重构的功能

| # | 功能 | 重构原因 |
|---|------|----------|
| 1 | 速度偏移系统 | 采用源代码的3模式+4区间分段查表+平滑降速 |
| 2 | FSD防御页面 | 仿生扭矩替代固定echo，0x3C2替代ISA抑制 |
| 3 | 驾驶模式页面 | UI卡片布局+描述文字参考截图 |
| 4 | 模块配置页面 | 增加温度/挡位/CAN状态详细显示+新开关 |

## 3. 页面改造详情

### 3.1 页面总览 (9页→11页)

```
完全保留不动:                重构:
pg-hardware  激活模式 ✋不动  pg-overview  模块配置
pg-network   网络设置 ✋不动  pg-drive     驾驶模式
pg-bus2      CAN2控制        pg-speed     速度偏移
pg-ota       OTA升级  ✋不动  pg-defense   FSD防御
pg-can       CAN工具  ✋不动

新增:
pg-strobe   灯光特技
pg-shift    自动换挡(占位)
```

### 3.2 pg-overview 模块配置（重构）

**新增状态显示区域：**
- 模块温度（从 `/system_status` 获取 chipTemp，已有实现）
- 当前挡位 P/R/N/D（来源：CAN-A 0x118帧 → `apRestoreState.gearRaw`，映射：1=P, 2=R, 3=N, 4=D）
- CAN-A 状态判定：
  - `ok`：`canOnline=true` 且 `millis()-lastFrameMs < 3000`
  - `异常 (无数据)`：`canOnline=true` 但 `millis()-lastFrameMs > 3000`
  - `异常脱机 (检查连线)`：`canOnline=false`
- CAN-B 状态判定：
  - `ok`：MCP2515 EFLG=0 且有数据
  - `异常脱机 (检查连线)`：MCP2515 初始化失败或 EFLG 错误
- FSD防封状态：黑名单域名数量

**开关控件（5个）：**
1. 模块总闸 — 已有 `canActive`
2. FSD破解启用 — 已有
3. FSD-V14模式 — 已有
4. **自动关机** — 新增 `autoShutdown` (POST /power_mgmt)
5. **关闭WiFi** — 新增 `wifiAutoOff` (POST /power_mgmt)

### 3.3 pg-drive 驾驶模式（UI重构）

6个模式卡片，3x2网格布局，每个卡片含图标+名称+描述：

| 模式 | 图标建议 | 描述 |
|------|----------|------|
| 自动智能 | ⚡ | "根据路况自动识别并切换模式" |
| 迟缓 Sloth | 🦥 | "慢节奏驾驶，最大程度保持安全距离" |
| 舒适 Chill | 🍃 | "平顺驾驶，适合日常通勤或家人出行" |
| 标准 Normal | ⚖️ | "动力线性，综合动力与经济性平衡" |
| V14狂飙 Hurry | 🚀 | "快速加速响应，针对紧急情况快速反应" |
| V14极限 MAX | 🔥 | "FSD系统极限加速，极速超车，极速爬坡" |

### 3.4 pg-speed 速度偏移（按源代码重构）

**3种偏移算法选项卡：**

**输入来源统一：** `limitKph = fusedSpeedLimitRaw * 5`（从CAN-A 0x399/921帧解码的融合限速值，与现有变量一致）

**模式0 — 固定百分比：**
- 手动选择：0% / 10% / 20% / 30% / 40% / 50%
- 直接写入0x3FD偏移字段

**模式1 — 自动偏移（默认）：**
- 分段查表算法（源代码 `update_smart_offset()`）：

| 当前限速 | 偏移比例 | 封顶值 |
|----------|----------|--------|
| ≤40 km/h | +50% | 60 km/h |
| ≤60 km/h | +50% | 90 km/h |
| ≤90 km/h | +30% | 117 km/h |
| ≤110 km/h | +20% | 132 km/h |
| ≤120 km/h | +10% | 132 km/h |

**模式2 — 自定义偏移：**
- 4个速度区间输入框：

| 区间 | 默认偏移 | NVS键 |
|------|----------|-------|
| 0-50 km/h | 30% | cp1 |
| 51-70 km/h | 20% | cp2 |
| 71-100 km/h | 10% | cp3 |
| 101-120 km/h | 10% | cp4 |

**新增特性：**
- 实时偏移显示："当前底盘实时生效偏移：+XX%"
- 平滑降速引擎：限速突降时以5km/h/s速率衰减（仅降速方向生效）
- 限速突升时瞬间跟随（无延迟）

**替换说明：**
- 废弃现有 `hw3CustomTarget[5]` + `hw3HighSpeedTarget[3]` 桶式映射
- 废弃 `PCT4`/`KPH5` 双编码，统一为源代码的百分比编码方式
- 保留 `hw3OffsetSlew` 斜率限制器框架，但速率固定为5km/h/s

### 3.5 pg-defense FSD防御（重构）

**5个功能开关：**

| # | 名称 | 开关 | 逻辑变更 |
|---|------|------|----------|
| 1 | 防御系统总开关 | 保留 | 控制所有防御子功能总入口 |
| 2 | 智能仿生方向盘 | 升级 | 从固定扭矩echo→0x370正弦波随机扭矩（见下方详细逻辑） |
| 3 | 声音取消消除警告 | 重构 | 替代现有 `isaChimeSuppress`：从0x399 ISA位抑制→0x3C2音量滚轮四步序列 |
| 4 | 速度加减免打扰 | 实现 | 0x3C2速度滚轮四步序列（现有 `dashSpeedNoDisturb` 变量已有，补充执行代码） |
| 5 | 兼容AP/EAP模式 | 实现 | 0x249拨杆检测AP模式切换防御策略（现有 `dashApEapCompatible` 变量已有，补充执行代码） |

**API参数映射（新增→现有→NVS）：**

| 新UI参数名 | 现有API参数名 | NVS键 | 变更说明 |
|-----------|-------------|-------|----------|
| 仿生方向盘 | `bionic_steering` | `def_bio` | 开关逻辑不变，底层执行逻辑从echo→仿生 |
| 声音消除 | `sound_warning_suppression` → 重命名为 `dnd_volume` | `def_sws`→`def_dvol` | NVS迁移：启动时检查旧键存在则迁移到新键 |
| 速度消除 | `speed_no_disturb` → 重命名为 `dnd_speed` | `def_nd`→`def_dspd` | 同上，NVS迁移 |

**仿生方向盘详细逻辑 (dash_bionic_steer.h):**

**实现方案：在现有 NagHandler 内部增加模式切换**
- `dashBionicSteering=true` → 仿生模式（正弦波随机扭矩）
- `dashBionicSteering=false` → legacy echo模式（现有固定1.80Nm）
- 不新建 Handler，在 NagHandler::handleMessage() 内部分支处理
- 受 `-D NAG_KILLER` 编译标志控制（与现有一致）

**仿生模式运行时异常保护：**
- 校验和计算失败 → 自动回退到 echo 模式该帧
- 连续3次帧格式异常 → 关闭仿生，记录日志，UI显示警告

```
触发条件: 0x399 data[5]>>2 & 0x0F >= 3 (hands_on_state警告)
注入帧: 0x370 (十进制880)
扭矩算法: base + amplitude * sin(phase)
随机参数:
  - 振幅: 30~55 (xorshift32伪随机)
  - 方向: 正/负随机切换
  - 持续时间: 350~500ms
帧处理:
  - counter: 原始帧counter+1
  - checksum: Tesla校验和重算
安全: 扭矩绝对值不超过60原始单位
```

**0x3C2滚轮DND详细逻辑 (dash_wheel_dnd.h):**

```
触发条件: 同仿生方向盘 (0x399警告检测)
发送通道: CAN-B (MCP2515)

音量消除 (dashDndVolume=true):
  Step1: data[2]=0x01 → 等50ms
  Step2: data[2]=0x00 → 等50ms
  Step3: data[2]=0x3F → 等50ms
  Step4: data[2]=0x00 → 完成

速度消除 (dashDndSpeed=true):
  Step1: data[3]=0x01 → 等50ms
  Step2: data[3]=0x00 → 等50ms
  Step3: data[3]=0x3F → 等50ms
  Step4: data[3]=0x00 → 完成

帧处理:
  - counter: 原始帧counter递增
  - checksum: Tesla校验和重算
```

### 3.6 pg-strobe 灯光特技（新页面）

**从pg-bus2中分离灯光相关控件，独立页面：**

**区域1 — 按键联动闪光 (Auto-Strobe):**
- "单次猛踩触发爆闪" 开关
- 触发次数：3下 / 5下 / 7下 / 10下 (按钮组)
- 底层：复用现有 `stalk_test` 逻辑

**区域2 — 战术爆闪频率:**
- 慢速 (300ms) / 中速 (200ms) / 极速 (100ms)
- 底层：复用现有 `dashLightingFrequency`

**区域3 — 持续爆闪:**
- "远光灯无限爆闪" 开关
- 底层：新增无限循环模式 (POST /strobe_cont)
- **安全退出条件：**
  - 非D挡自动停止（检测 `apRestoreState.gearRaw != 4`）
  - CAN-A 总线离线自动停止（`canOnline == false`）
  - WiFi断开超过60秒自动停止
  - 用户在UI手动关闭

**区域4 — 后雾灯特技:**
- "后雾灯无频爆闪 (Strobe)" 开关
- 底层：0x273帧，50ms切换 (dash_fog_light.h)
- 替代现有 `dashRearFogStrategy` 的 `strobe` 选项

**区域5 — F1领航灯:**
- "F1赛车风格镇流灯 (Pilot)" 开关
- 底层：0x273帧，3连闪135ms+1500ms停顿
- 替代现有 `dashRearFogStrategy` 的 `continuous` 选项

**后雾灯策略互斥：** fogStrobe 和 fogPilot 互斥，同时仅一个生效。开启一个自动关闭另一个。NVS持久化使用现有 `lt_fog` 键，扩展值域：0=off, 1=strobe, 2=pilot（废弃旧的2=continuous）。

**安全条件：** 仅D挡时灯光特技可激活。挡位来源统一使用 `apRestoreState.gearRaw`（CAN-A 0x118帧解码），值=4时为D挡。

**pg-bus2迁出清理清单：**

| pg-bus2 控件 | 去向 |
|-------------|------|
| 爆闪开关 + 触发次数 | → 迁至 pg-strobe 区域1 |
| 爆闪频率选择 | → 迁至 pg-strobe 区域2 |
| 后雾灯策略选择 | → 迁至 pg-strobe 区域4+5 |
| stalk拨杆测试按钮 | **保留在 pg-bus2** |
| CAN2 ID发现表 | **保留在 pg-bus2** |
| Service Mode (0x339) | **保留在 pg-bus2** |

**pg-bus2清理：** 迁出完成后，从pg-bus2移除灯光相关控件，保留ID发现表、拨杆测试、服务模式

### 3.7 pg-shift 自动换挡（新增占位页）

- 粉色背景警告卡片："储备功能，未开放"
- 说明文字："此为预置的驾驶辅助功能，核心逻辑（仅车速为0，物理挡位无变化时，且识别到后方距离小于40cm时，自动安全切换到停车挡位...）"
- 遥测数据只读展示：
  - 实时车速 (km/h)
  - 刹车状态
  - 当前挡位
- 无任何可操作控件

### 3.8 pg-overview 状态字段扩展

**`/status` JSON 新增字段：**

```json
{
  "chipTemp": 55.0,
  "gear": "N",
  "canAStatus": "ok",
  "canBStatus": "offline",
  "autoShutdown": true,
  "wifiAutoOff": false,
  "actOffset": 0,
  "speedLimit": 0,
  "vehicleSpeed": "0.0",
  "moduleEnable": true,
  "vehicleOta": false,
  "fogStrategy": 0,
  "strobeCont": false,
  "dndVolume": true,
  "dndSpeed": false
}
```

## 4. 固件新增模块

### 4.1 新增文件清单

| 文件 | 功能 | 行数估计 |
|------|------|----------|
| `include/dash_fog_light.h` | 0x273后雾灯控制器 | ~150行 |
| `include/dash_bionic_steer.h` | 0x370仿生方向盘扭矩 | ~200行 |
| `include/dash_wheel_dnd.h` | 0x3C2滚轮DND消除 | ~180行 |
| `include/dash_ota_guard.h` | 0x318车辆OTA保护 | ~80行 |
| `include/dash_power_mgmt.h` | 自动关机+WiFi关闭 | ~150行 |

### 4.2 重构文件

| 文件 | 变更 | 说明 |
|------|------|------|
| `include/dash_hw3_speed.h` | 重写 | 采用源代码3模式+4区间+平滑降速 |
| `include/can_frame_types.h` | 扩展 | 新增0x273/0x318/0x3C2帧结构定义 |
| `include/web/mcp2515_dashboard.h` | 扩展 | 新增API endpoint + 状态字段 |
| `include/web/mcp2515_dashboard_ui.src.h` | 重构 | UI页面改造(11页) |
| `src/main.cpp` | 扩展 | 初始化新模块 |

### 4.3 0x273 后雾灯控制器 (dash_fog_light.h)

```
基础帧: {0x81, 0xE1, 0x10, 0x40, 0x0B, 0x03, 0x30, checksum}
控制字段: data[2]
  0x90 = 雾灯亮
  0x10 = 雾灯灭
发送通道: CAN-B (MCP2515)

爆闪模式 (fogStrobe=true):
  50ms间隔 在 0x90/0x10 间切换
  仅D挡时激活

F1领航模式 (fogPilot=true):
  节奏: 亮135ms→灭135ms→亮135ms→灭135ms→亮135ms→停顿1500ms→循环
  仅D挡时激活
```

### 4.4 车辆OTA保护 (dash_ota_guard.h)

```
监听帧: 0x318 (CAN-A TWAI)
检测字段: data[6] & 0x03
  == 2 → otaCounter++
  != 2 → otaCounter归零, clearCounter++

判定逻辑:
  otaCounter >= 3 → vehicleOtaActive = true (暂停FSD注入)
  clearCounter >= 6 → vehicleOtaActive = false (恢复FSD注入)

全局影响:
  所有Handler注入前检查 vehicleOtaActive 标志
  vehicleOtaActive=true → 跳过所有CAN注入帧
```

### 4.5 功耗管理 (dash_power_mgmt.h)

```
自动关机 (autoShutdown=true):
  触发: millis() - lastCanActivityMs > 300000 (5分钟)
  动作: MCP2515.sleepMode() → TWAI.stop() → driver_uninstall()
         → esp_deep_sleep_start()
  唤醒: EXT0(TWAI_RX_PIN, HIGH) — CAN总线有活动时自动唤醒
  NVS键: auto_shutdown (bool)

WiFi自动关闭 (wifiAutoOff=true):
  触发: millis() - lastWebActivityMs > 300000 (5分钟)
  动作: WiFi.mode(WIFI_AP) — 仅关闭STA（互联网中转），保留AP（手机/车机连接）
  恢复: AP保持运行，手机重新连接后可在网络设置页手动重连STA
  NVS键: wifi_auto_off (bool)
  注意: 不会完全关闭WiFi，AP始终可用，用户随时可通过Web界面恢复STA
```

### 4.6 速度偏移重构 (dash_hw3_speed.h)

**完全采用源代码算法：**

```cpp
// 固定百分比模式 (offsetMode=0)
offset = manualOffset; // 0~50%

// 自动偏移模式 (offsetMode=1) — 分段查表
if (limitKph <= 40) target = min(60, limitKph * 1.5);
else if (limitKph <= 60) target = min(90, limitKph * 1.5);
else if (limitKph <= 90) target = min(117, limitKph * 1.3);
else if (limitKph <= 110) target = min(132, limitKph * 1.2);
else target = min(132, limitKph * 1.1);
offset = target - limitKph;

// 自定义偏移模式 (offsetMode=2) — 4区间
if (limitKph <= 50) pct = customPct1;
else if (limitKph <= 70) pct = customPct2;
else if (limitKph <= 100) pct = customPct3;
else pct = customPct4;
target = limitKph * (1 + pct/100.0);
offset = target - limitKph;

// 平滑降速引擎 (所有模式共用)
if (offset < smoothedOffset) {
  // 降速方向: 5km/h/s 平滑衰减
  smoothedOffset = max(offset, smoothedOffset - 5.0 * dt);
} else {
  // 升速方向: 瞬间跟随
  smoothedOffset = offset;
}
```

**废弃项：**
- `hw3CustomTarget[5]` 桶式映射 → 替换为 4区间百分比
- `hw3HighSpeedTarget[3]` → 合并入4区间
- `PCT4`/`KPH5` 双编码 → 统一百分比编码
- `hw3WireEncoding` 配置 → 移除

**保留项：**
- `hw3OffsetSlew` 框架（速率改为固定5km/h/s）
- NVS持久化配置
- HW3/HW4/Legacy Handler注入入口不变

**Legacy路径说明：** 现有 `dash_legacy_speed.h` 的 Legacy MPP 速度映射系统（`legacyMppOverride`、`legacyMppCustomTarget[]`）保持不动。3模式+4区间重构仅影响 HW3/HW4 路径。Legacy Handler 在选择 Legacy 硬件模式时仍使用原有 MPP 速度逻辑。编译标志 `DASH_H3_SPEED` 控制是否启用新算法。

## 5. 新增API端点

| 方法 | 路径 | 参数 | 用途 |
|------|------|------|------|
| POST | `/speed_custom` | `cp1,cp2,cp3,cp4` (0-50%) | 4区间自定义偏移 |
| POST | `/fog_light` | `fogStrategy` (0=off/1=strobe/2=pilot) | 后雾灯控制（互斥策略） |
| POST | `/strobe_cont` | `enabled` (bool) | 持续爆闪开关 |
| POST | `/power_mgmt` | `autoShutdown,wifiAutoOff` (bool) | 功耗管理 |
| GET | `/vehicle_ota_status` | — | OTA保护状态 |

**扩展现有端点：**

| 端点 | 新增参数/字段 |
|------|---------------|
| `/status` | chipTemp, gear, canAStatus, canBStatus, autoShutdown, wifiAutoOff, actOffset, speedLimit, vehicleSpeed, moduleEnable, vehicleOta, fogStrategy(0/1/2), strobeCont, dndVolume, dndSpeed |
| `/defense_config` | dndVolume, dndSpeed, bionicMode |
| `/speed_strategy` | offsetMode (0/1/2), manualOffset |

## 6. 执行阶段

### 阶段1: 安全基础 (零侵入)

**目标：** 添加独立新模块，不动任何现有功能

| 任务 | 文件 | 说明 |
|------|------|------|
| 1.1 | `dash_ota_guard.h` | 0x318 OTA检测，设置全局标志 |
| 1.2 | `dash_power_mgmt.h` | 自动关机+WiFi关闭 |
| 1.3 | `can_frame_types.h` | 新增0x273/0x318/0x3C2帧定义 |
| 1.4 | `mcp2515_dashboard.h` | 新增4个POST端点 + 1个GET端点 |
| 1.5 | pg-overview UI | 状态显示扩展+新开关 |

**验证：** 现有9个页面功能正常，新增开关可切换

**OTA Guard零侵入实现：** `vehicleOtaActive` 标志在 `dashInjectionActive()` 函数中增加一个条件检查。`dashInjectionActive()` 是所有Handler注入前的统一入口函数，在此处增加一个bool检查属于最小侵入。Handler内部逻辑完全不修改。

### 阶段2: 速度偏移重构

**目标：** 替换速度偏移算法为源代码实现

| 任务 | 文件 | 说明 |
|------|------|------|
| 2.1 | `dash_hw3_speed.h` | 重写3模式+4区间+平滑降速 |
| 2.2 | pg-speed UI | 3选项卡+实时偏移+算法说明 |
| 2.3 | `POST /speed_custom` | 新端点 |
| 2.4 | `/status` 扩展 | actOffset, speedLimit, vehicleSpeed |
| 2.5 | 旧代码保留 | 编译开关 `USE_NEW_SPEED_ALGO` |

**验证：** 3种偏移模式分别测试，平滑降速可观测

### 阶段3: FSD防御系统重构

**目标：** 升级防御系统为仿生+0x3C2架构

| 任务 | 文件 | 说明 |
|------|------|------|
| 3.1 | `dash_bionic_steer.h` | 0x370正弦波随机扭矩 |
| 3.2 | `dash_wheel_dnd.h` | 0x3C2四步序列 |
| 3.3 | NagHandler升级 | 仿生模式+fallback |
| 3.4 | pg-defense UI | 5开关+描述 |
| 3.5 | `/defense_config` 扩展 | 新参数 |

**验证：** 防御各子功能独立测试，fallback回退正常

### 阶段4: 灯光特技系统

**目标：** 实现独立灯光特技页面和底层逻辑

| 任务 | 文件 | 说明 |
|------|------|------|
| 4.1 | `dash_fog_light.h` | 0x273后雾灯(爆闪+F1领航) |
| 4.2 | pg-strobe 新页面 | 5区域灯光控制 |
| 4.3 | 持续爆闪实现 | 无限循环远光灯 |
| 4.4 | pg-bus2清理 | 移除已迁出灯光控件 |
| 4.5 | sidebar/nav更新 | 新增灯光特技导航项 |

**验证：** 爆闪/雾灯/F1各模式测试

### 阶段5A: UI收尾

| 任务 | 文件 | 说明 |
|------|------|------|
| 5.1 | pg-drive UI | 6模式卡片+描述 |
| 5.2 | pg-shift 占位页 | 储备功能展示 |
| 5.3 | pg-overview完善 | 温度/挡位/CAN状态文字 |

### 阶段5B: 集成验证与发布

| 任务 | 文件 | 说明 |
|------|------|------|
| 5.4 | 全量回归测试 | API契约+功能验证 |
| 5.5 | minify + gzip | UI压缩更新 |

## 7. 风险与缓解

| 风险 | 等级 | 缓解措施 |
|------|------|----------|
| 速度偏移重构影响FSD激活 | 高 | 编译开关 `USE_NEW_SPEED_ALGO` 可即时回退 |
| 仿生扭矩0x370格式错误 | 中 | 严格复制源代码字节逻辑，保留fallback |
| 0x3C2与方向盘冲突 | 中 | 四步序列50ms间隔，防御总开关控制 |
| 0x273帧校验失败 | 中 | 复用源代码已验证base frame |
| DeepSleep无法唤醒 | 低 | 源代码已验证TWAI_RX唤醒路径 |
| Flash空间不足 | 中 | 新增~3KB固件，gzip UI增~2KB |

## 8. 回滚策略

- 速度偏移：`#define USE_NEW_SPEED_ALGO 0` 回退
- 仿生扭矩：`dashBionicSteering=false` 回退固定echo
- 雾灯/滚轮：配置开关关闭即停用
- 全局：git revert 到当前 HEAD (13cbf78)

## 9. 不在此范围内

- pg-network 网络设置页面（完全保留不动）
- pg-ota OTA升级页面（完全保留不动）
- pg-can CAN工具页面（完全保留不动）
- NAT网关/DNS代理逻辑
- WiFi多SSID管理
- CAN引脚配置
- 自动换挡底层实现（仅UI占位）
- Legacy/HW3/HW4 FSD激活Handler核心逻辑
