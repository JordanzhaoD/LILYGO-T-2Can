# Waveshare 单 CAN 独立版设计规格

**日期:** 2026-06-14  
**分支:** `feature/waveshare-single-can-standalone`  
**目标项目:** `/Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware`  
**设计方向:** 能力开关独立版

---

## 1. 背景与目标

当前 `LILYGO-T-2Can-firmware` 同时包含 LILYGO T-2CAN 双 CAN 形态和 Waveshare ESP32-S3 RS485 CAN 单 CAN 形态。LILYGO 双 CAN 版本通过 TWAI + MCP2515 支持 CAN1/CAN2；Waveshare 硬件只有 ESP32-S3 内置 TWAI + 单路 CAN 收发器。

本设计目标是建立一个长期可维护的 **Waveshare 单 CAN 独立版本**，用于微雪 ESP32-S3-RS485-CAN 硬件模块。这个版本不是通过删除代码得到，而是通过清晰的 build profile、runtime capabilities、UI/API 裁剪和安全 gate 策略定义出来。

核心目标：

1. 保留 Waveshare 单 CAN 硬件接口能力。
2. 保留当前 Atlas 车机 UI 和手机 UI 的视觉风格。
3. 保留核心 FSD 激活、速度策略、驾驶风格、网络设置和 OTA 升级。
4. 隔离 CAN2、MCP2515、Bus2、Service Mode、Stalk/highbeam、灯光特技等双 CAN 功能。
5. 参考上游 `ev-open-can-tools v3.0.2-beta.3`，增强 AP 门控延迟注入安全策略，为 Tesla 2026.8.3.6 及后续版本提供更保守的安全基础和诊断能力。

非目标：

- 不在第一阶段删除 CAN2/MCP2515/灯光源码。
- 不承诺完整兼容 2026.8.3.6 全车型和所有地区固件。
- 不重写 Dashboard 视觉系统。
- 不做大型模块拆分。

---

## 2. 选择的方案

采用 **能力开关独立版**。

### 2.1 方案概述

新增或明确一个 Waveshare 单 CAN standalone 构建入口，在后端 `/status` 中暴露 capabilities，前端 UI 和后端 API 注册都基于 capabilities/编译宏裁剪功能。

### 2.2 为什么选择这个方案

这个方案平衡了三个目标：

- **长期维护:** 双 CAN 源码保留，后续同步上游或回退成本低。
- **用户清晰:** 单 CAN 版不会显示不能执行的 CAN2/灯光入口。
- **测试明确:** capabilities 成为 UI、API 和测试共同依据，避免隐藏逻辑分散在多个地方。

### 2.3 不选择的方案

- **最小 UI 隐藏版:** 改动最快，但后端能力边界不清楚，API 可能仍存在，长期维护容易混乱。
- **模块化大拆分版:** 架构最理想，但第一阶段改动面过大，容易影响当前 Dashboard 生成链路和上游同步。

---

## 3. 总体架构

### 3.1 Build/Profile 层

新增 Waveshare 单 CAN standalone 构建环境，固定命名为：

```text
waveshare_single_can_standalone
```

它基于现有 `waveshare_ESP32_S3_RS485_CAN`，但作为长期维护入口独立存在，保留以下硬件和系统特征：

- `DRIVER_TWAI`
- `ESP32_DASHBOARD`
- `TWAI_TX_PIN=GPIO_NUM_15`
- `TWAI_RX_PIN=GPIO_NUM_16`
- `TWAI_RX_QUEUE_LEN=64`
- `TWAI_TX_QUEUE_LEN=16`
- `PIN_LED=14`
- `DASH_STA_AP_GATEWAY`
- `partitions_16mb_ota_4096k_nvs64.csv`

新增 standalone 能力宏，例如：

```cpp
#define DASH_SINGLE_CAN_STANDALONE
```

或等价 build flag：

```ini
-DDASH_SINGLE_CAN_STANDALONE=1
```

默认硬件模式使用 Auto 检测，保留 Legacy/HW3/HW4 手动切换能力。

### 3.2 Capabilities 层

后端 `/status` 输出统一 capabilities，例如：

```json
{
  "capabilities": {
    "singleCan": true,
    "can2Available": false,
    "lightingBusSupported": false,
    "serviceModeSupported": false,
    "stalkTestSupported": false,
    "bus2SnifferSupported": false,
    "fsdActivation": true,
    "speedStrategy": true,
    "driveProfile": true,
    "networkSettings": true,
    "otaUpdate": true,
    "canDiagnostics": true
  }
}
```

这份能力声明是 UI 渲染、API 行为和测试断言的共同来源。

### 3.3 Safety Injection 层

AP gate 对齐上游 `ev-open-can-tools v3.0.2-beta.3` 的安全时序：

- 普通注入走 AP stable gate。
- Legacy FSD activation `0x3EE mux0` 需要 AP active 连续 2 秒。
- unknown / invalid / SNA gear fail-closed，不当作 Park。
- AP disengage 后立即清空 stable timer。
- Dashboard 输出 gate diagnostics，支持实车验证。

### 3.4 UI/API 裁剪层

车机 UI 和手机 UI 视觉风格保持当前 Atlas Dashboard 风格。功能通过 capabilities 裁剪：

- 支持的功能正常显示。
- 不支持的 CAN2/灯光功能不渲染。
- 后端不注册 CAN2-only API；若保留兼容 endpoint，必须明确返回 `supported:false`。

设计原则：

> 单 CAN 微雪版不是删出来的，而是通过清晰 profile + capabilities 定义出来的。

---

## 4. 功能保留清单

### 4.1 驾驶状态

保留驾驶状态中心，包括：

- FSD 注入状态。
- AP gate 状态。
- CAN 健康。
- 运行时间。
- 当前硬件模式。
- 当前驾驶风格。
- 当前速度策略。
- 固件版本。
- 基础运行指标。

### 4.2 手机现场遥控

保留当前手机 UI 风格和核心遥控能力：

- 保留长按 FSD 核心动作。
- 保留短状态卡片。
- 不在手机首屏显示高风险或不支持的 CAN2/灯光操作。

### 4.3 硬件模式

默认 Auto 检测，保留手动模式：

- Auto
- Legacy
- HW3
- HW4

UI 需要明确显示当前自动识别结果和 fallback 状态。

### 4.4 驾驶风格

保留现有驾驶风格：

- Auto
- Sloth
- Chill
- Normal
- Hurry
- MAX

继续复用现有 `/drive_profile` API。

### 4.5 速度策略

保留现有速度策略：

- 固定
- 自动
- 自定义

继续复用现有 `/speed_strategy` 和 `/speed_custom` API。

### 4.6 FSD 防护

保留并增强：

- AP gate。
- OTA guard。
- gate diagnostics。
- Legacy `0x3EE mux0` 2 秒 gate。
- unknown gear fail-closed。

### 4.7 网络设置

保留：

- WiFi AP 热点。
- STA 连接。
- STA-AP gateway。
- DNS/NAT 规则。
- 黑白名单持久化。
- 设置导入/导出。

### 4.8 OTA 升级

保留：

- 手动 `.bin` 上传。
- GitHub release 检查。
- stable/beta channel。
- 自动更新开关。
- OTA 凭据配置。

### 4.9 CAN 诊断

保留单 CAN 诊断：

- TWAI RX/TX/error counters。
- 单 CAN sniffer/recorder 中适用部分。
- debug log。
- gate diagnostics。

不显示 CAN2 子区域。

---

## 5. 功能隔离清单

以下功能源码保留，但在 Waveshare single-CAN standalone build 中隔离。

### 5.1 CAN2 控制

- UI 不显示 CAN2 控制页。
- capabilities 返回 `can2Available=false`。
- 单 CAN build 不初始化 MCP2515 secondary bus。

### 5.2 Bus2 IDs / MCP2515 counters

- UI 不显示 Bus2 ID 发现列表。
- 单 CAN build 不注册 `/bus2_ids`。
- 双 CAN build 保持原行为。

### 5.3 Service Mode `0x339`

- 单 CAN build 不注册 `/service_mode`。
- 源码保留给 LILYGO 双 CAN build。

### 5.4 Stalk / highbeam `0x249`

- 单 CAN build 不注册 `/stalk_test`。
- UI 不显示 flash/highbeam 测试按钮。

### 5.5 后雾灯 / strobe / F1 pilot

- UI 隐藏灯光特技执行入口。
- 若 `/fog_light` 或 `/strobe_cont` 需要为兼容保留，必须返回：

```json
{
  "ok": false,
  "supported": false,
  "reason": "single_can_build"
}
```

不能返回 `ok:true`。

### 5.6 MCP2515 driver

- 源码保留。
- 只在 `DRIVER_T2CAN_DUAL` 或 MCP2515 相关 build 编译。
- Waveshare standalone build 不引入 SPI/MCP2515 初始化路径。

---

## 6. AP Gate / 2026.8.3.6 安全增强

### 6.1 参考上游

参考上游 release：

- `ev-open-can-tools v3.0.2-beta.3`
- Release: `https://github.com/ev-open-can-tools/ev-open-can-tools/releases/tag/v3.0.2-beta.3`

关键上游策略：当 AP Injection Gate 启用时，Legacy FSD activation 的 `CAN 0x3EE mux 0` 需要等 Autopilot 连续 active 2 秒后才允许注入，避免初始接管时 parked gate 误判和 activation timing race。

### 6.2 决策流程

每次准备注入时按顺序判断：

1. 读取 frame ID、mux、AP 状态、档位状态和当前硬件模式。
2. 如果 live gear 是 unknown / invalid / SNA，则 fail-closed。
3. 普通注入路径必须满足 AP stable gate。
4. 如果 frame 是 Legacy `0x3EE mux0` 且 AP Injection Gate 开启，则必须 AP active 连续 2 秒。
5. 如果 AP disengage，则立即清空 stable timer。
6. 输出 allow/block，并记录 reason。

### 6.3 `0x3EE mux0` 特殊规则

匹配条件：

```cpp
id == 0x3EE && mux == 0
```

要求：

- AP Injection Gate 开启时，必须 AP active 连续 2 秒。
- Parked / Summoning 不能绕过这条特殊规则。
- AP falling edge 立即清零 timer。

### 6.4 Unknown gear fail-closed

以下状态不能当作 Park：

- unknown
- invalid
- SNA
- reserved gear

原因：2026.8.3.6 相关安全问题中，误判 Park 会导致行驶中 gate 被错误打开。

### 6.5 诊断字段

`/status.fsdDiag.gate` 保持或增加：

```json
{
  "enabled": true,
  "allowed": false,
  "reason": "legacy_fsd_wait_ap_2s",
  "apActive": true,
  "apStableMs": 850,
  "requiredStableMs": 2000,
  "parked": false,
  "summoning": false,
  "lastInjectMs": 123456,
  "blockedFrame": "0x3EE mux0"
}
```

这些字段用于回答：

- 为什么当前没有注入？
- 是否命中 `0x3EE mux0` 2 秒规则？
- 是否误判 Park？
- AP disengage 后 timer 是否正确清零？
- 2026.8.3.6 后续版本是否出现新 gate 条件？

### 6.6 兼容性边界

文档中应明确说明：

> 本版本对齐上游 `v3.0.2-beta.3` 安全时序修复，降低 2026.8.3.6 相关误注入风险，并提供诊断字段支持后续版本适配；不承诺 2026.8.3.6 全车型直接可用。

---

## 7. API 设计原则

### 7.1 支持能力

支持的 API 正常返回现有结构，例如：

- `/status`
- `/config`
- `/drive_profile`
- `/speed_strategy`
- `/speed_custom`
- `/hotspot_config`
- `/wifi_*`
- `/update_*`

### 7.2 不支持能力

不支持的 CAN2-only API 首选不注册：

- `/service_mode`
- `/stalk_test`
- `/burst`
- `/bus2_ids`

如果出于兼容原因保留 endpoint，返回必须包含：

```json
{
  "ok": false,
  "supported": false,
  "reason": "single_can_build"
}
```

### 7.3 API 不允许误导

单 CAN build 下，不支持的动作不能返回 `ok:true`。这条规则优先级高于前端兼容性。

---

## 8. UI 设计原则

### 8.1 视觉风格

保留当前 Atlas Dashboard 风格：

- 驾驶状态中心作为默认首页。
- 手机现场遥控保留 native-app-style 体验。
- CAN 诊断保留工程控制台风格。
- 技术 acronym 保持英文，如 FSD、CAN、OTA、AP、HW、NVS。

### 8.2 动态裁剪

UI 根据 `/status.capabilities` 动态裁剪：

- `can2Available=false` 时隐藏 CAN2 控制和 Bus2 区域。
- `lightingBusSupported=false` 时隐藏灯光特技执行入口。
- `serviceModeSupported=false` 时隐藏 Service Mode。
- `stalkTestSupported=false` 时隐藏 flash/highbeam 测试。

### 8.3 不显示死按钮

不支持功能不渲染，而不是显示灰色不可点按钮。原因是 Jordan 的产品哲学是简约，单 CAN 用户不需要看到与硬件无关的功能。

### 8.4 诊断页保留必要信息

CAN 诊断页保留单 CAN 信息：

- TWAI 状态。
- RX/TX counters。
- error counters。
- debug log。
- AP gate diagnostics。

---

## 9. 分支与交付策略

### 9.1 分支名

建议分支：

```text
feature/waveshare-single-can-standalone
```

### 9.2 实施阶段

实施计划分五步：

1. 新增 standalone profile/env。
2. 增加 capabilities 输出。
3. UI 和 API 按 capabilities 裁剪。
4. AP gate 对齐上游 beta.3。
5. 更新文档、测试和固件资产。

### 9.3 交付物

最终交付包括：

- 设计文档。
- 实施计划。
- 独立 feature 分支。
- 测试报告。
- Waveshare OTA app。
- Waveshare full-flash 资产。
- README / 用户指南中的“单 CAN 微雪独立版”入口。
- 2026.8.3.6 AP gate 适配说明和限制声明。

---

## 10. 验证矩阵

### 10.1 静态与单元验证

需要覆盖：

- `python3 scripts/minify_dashboard.py --check`
- Dashboard API contract capabilities 断言。
- `native_injection_after_ap`。
- helper tests：unknown gear fail-closed。
- Legacy `0x3EE mux0` 2 秒 gate。
- AP disengage 清 timer。

### 10.2 构建验证

至少运行：

```bash
pio run -e waveshare_single_can_standalone
```

如公共代码影响双 CAN，则增加：

```bash
pio run -e lilygo_t2can_dual
```

构建验证必须确认：

- Waveshare standalone 不初始化 MCP2515。
- Waveshare standalone 不注册 CAN2-only routes。
- Waveshare standalone 不暴露 CAN2 UI。
- OTA artifact 命名清晰。

### 10.3 UI/行为验证

需要确认：

- 车机 UI 风格不变。
- 手机现场遥控保留。
- FSD/速度/驾驶风格/网络/OTA 可用。
- CAN2/灯光入口不渲染。
- unsupported endpoint 不返回 `ok:true`。

### 10.4 实机验证

Waveshare 板刷入后确认：

- 串口启动日志显示 Waveshare single-CAN build。
- TWAI GPIO15/16 ready。
- `canActive` 默认安全状态符合预期。
- Dashboard `/status.capabilities` 正确。
- 未接车前确认注入关闭。
- 接车前确认 AP gate 默认状态和诊断字段。

---

## 11. 风险与缓解

### 11.1 风险：误删双 CAN 能力

缓解：第一阶段不删除 CAN2/MCP2515/灯光源码，只通过 profile/capabilities 隔离。

### 11.2 风险：UI 隐藏但 API 仍可执行

缓解：单 CAN build 不注册 CAN2-only API；兼容 endpoint 必须返回 `supported:false`。

### 11.3 风险：2026.8.3.6 兼容被过度承诺

缓解：文档明确只对齐上游安全时序修复，不承诺全车型兼容。

### 11.4 风险：AP gate 状态误判

缓解：unknown gear fail-closed、AP stable timer、Legacy `0x3EE mux0` 2 秒 gate、Dashboard diagnostics。

### 11.5 风险：上游同步冲突

缓解：避免大拆分，集中在 profile、capabilities、UI 条件渲染和 safety gate 小范围改动。

---

## 12. 成功标准

这个分支完成时应满足：

1. 有明确的 Waveshare single-CAN build 入口。
2. `/status.capabilities` 能准确描述单 CAN 能力。
3. UI 保持 Atlas 风格，并隐藏 CAN2/灯光特技入口。
4. FSD 激活、速度策略、驾驶风格、网络设置、OTA 保留。
5. AP gate 对齐上游 `v3.0.2-beta.3` 安全时序。
6. 单 CAN build 不初始化 MCP2515，不注册 CAN2-only routes。
7. 测试和构建验证通过。
8. 文档明确说明 2026.8.3.6 的安全适配边界。
