# TX 分路：0x3EE 主开关 + A3 骑载接管（beta02 基线）— 设计文档

> 状态：已批准（Jordan，2026-11-23，方案 B）。基线 = 6.0.0-beta02（aa4c16d，干净回退）。
> 背景：beta17-21 路线田野否决（beta21 实车红轮），全部删除；本设计是 beta02 上的唯一新增功能。

## 一、目标与非目标

**目标**：新增「0x3EE FSD 激活注入」总开关（默认 ON）——关掉后本机 0x3EE 全部写入停止（原生激活驾驶），同时：
- 左滚轮免打扰照常（beta02 的 DND tick 本就独立，零改动）
- 速度智能偏移照常——由 **A3 0x2F8 骑载自动接管**（从已田野标定的 A3.2 设计移植，归档分支 archive/pre-rollback-beta21）

**非目标/红线**：
- 不动 beta02 的 DND 引擎、jitter、FSD 激活逻辑（开关 ON = 与 beta02 逐字节一致）
- 不引入 930ms 消窗、bit27 克隆、alignMode 开关体系（已否决路线，永不回归）
- 开关 ON 时 A3 骑载不激活（fallback 仅在开关 OFF 时发生）

## 二、实现

### handlers.h
1. 新回调成员 `bool (*fsd3eeInjectAllowed)() = nullptr;`——未绑定或返回 true = 纯 no-op（native/独立场景零影响）。
2. **0x3EE 写入点 gate（beta02 共 2 处，均在 `frame.id == 1006` 处理函数内：mux0 骑载 ~972、mux1 ~1060）**：函数早期统一分支——回调返回 false 时记 diag（`legacySpeedDiag.blockedReason="fsd3eeMasterOff"`，若速度请求在）并 return（车辆自己的 0x3EE 帧透传）。
3. **A3 骑载（0x2F8=760 处理器内）**：`speedRequested && !fsd3eeInjectAllowed() && injectionAllowed() && abortGuard.allowsInjection() && !jitterInhibit` → `computeLegacySpeedOffset()`（beta02 原生）→ 在刚收到的 0x2F8 帧上重写 `data[5]` bits0-5（`raw=kph+30`，保留 bit6-7 含 MPH/KPH 位）→ send。**纯镜像骑载**：只改车辆自己发的帧上的一个字段，节奏跟随车端 ~7Hz，永不自由发帧。
4. diag：`LegacySpeedRuntimeDiag` 增 `ride2f8TxOk`、`ride2f8BlockedReason`。

### dashboard.h
`dashFsd3eeInject`（默认 true）· NVS `f3ee` · `/defense_config` 字段 `fsd_3ee_inject`（GET/POST）· 回调绑定（handler 安装路径）· `/status` 顶层 `fsd3eeInject` · legacySmartSpeed JSON 增 ride2f8 字段。

### UI（.src.h → minify）
defense 页「0x3EE FSD 激活注入」开关（滚轮免打扰旁），文案说明 A3 接管语义；保存/回填/i18n。

## 三、验证

- native 钉子：开关关→1006 处理函数零 send；关+速度请求→0x2F8 骑载 send 且仅改 data[5]；开关开→行为与 beta02 相同（no-op）；回调未绑定→no-op。
- 门禁：native 全绿 + `IDF_MAINTAINER=1 pio run -e lilygo_t2can_dual`（beta02 无 wrapper）。
- 发版 6.1.0-beta01（新系列，旧号已废）+ 资产 + OTA 指引（POST /update + X-File-Name）。
- 田野：开关关 + 原生激活 → DND 清警告、`ride2f8TxOk` 增长、0x3EE 计数停增、无红轮。
