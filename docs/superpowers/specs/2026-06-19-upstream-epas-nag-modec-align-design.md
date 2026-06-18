# EPAS-Faithful Nag 对齐上游 Mode-C（v2.16-beta.8）— 设计规格

- **日期**: 2026-06-19
- **项目**: LILYGO-T-2Can-firmware（微雪 ESP32-S3 单 CAN 独立固件）
- **状态**: 设计已确认（2026-06-19），待实现计划
- **作者**: Claude（为 Jordan 起草）
- **前置 spec**: `2026-06-17-epas-faithful-nag-unified-design.md`（beta.7 统一引擎，已实现为 `dash_epas_nag.h`）
- **参考上游**: [hypery11/flipper-tesla-fsd v2.16-beta.8](https://github.com/hypery11/flipper-tesla-fsd/releases/tag/v2.16-beta.8)
  - 关键提交 `2bc2f33` "rewrite EPAS-faithful nag as a demand-state (Mode-C) machine (#100)"
  - 引入提交 `b9bb6c2` "EPAS-faithful nag mode (v2.17 experimental)"（beta.7，已被 beta.8 替代）

---

## 1. 背景与根因

### 1.1 现状（前置 spec 已实现 + 两轮实车）

`2026-06-17` spec 落地了 `DashEpasNagEngine`（`include/dash_epas_nag.h`），采用上游 beta.7 的 EPAS-faithful 思路：回声 `0x370`、counter+1、均值回归 ~0Nm 扭矩、不碰 handsOnLevel。随后两轮实车调试：

| 轮次 | 提交 | 改动 | 实车结果 |
|---|---|---|---|
| B1 | `a420943` | 扭矩固定 +1.80Nm（raw 2230）替代 ~0Nm 随机游走 | echoTorque=2230 100% 命中，**nag 仍无效** |
| B2 | `6537e91` | 强制 `handsOnLevel=1`（`outData[4]\|=0x40`） | 待实车（板子已烧录 6537e91） |

### 1.2 根因（上游已替我们证伪）

上游 beta.8 提交 `2bc2f33` 的 commit message 原话：

> *"beta.7's EPAS-faithful was a flat centred-0 torque walk with **handsOnLevel pinned** — confirmed it didn't hold on 14.x. Replaced with a demand-state machine … handsOnLevel **DERIVED from synthetic torque magnitude**."*

**本地 B1+B2 正属于上游已证伪的"固定 handsOnLevel"类别**：
- B1 把扭矩固定（不再是均值回归，但仍是单一静态值）。
- B2 把 handsOnLevel 固定为 1。

上游实测结论：不管 pin 0（beta.7）还是固定值，2026.14.x 预检都不接受；必须让 **handsOnLevel 跟着扭矩幅值动态推导**，且回声帧要按 DAS 提醒强度做**差异化扭矩**（轻度提醒发缓和扭矩、强提醒发大扭矩爬升）——即 **Mode-C 按需状态机**。

### 1.3 上游 Mode-C 的三条核心改变（相对 beta.7 / 本地 B2）

1. **handsOnLevel 从扭矩幅值推导**（不再固定）：`|raw-center| ≥ 2.0Nm → level 2`；`≥ 1.0Nm → level 1`；否则 0。让回声帧"自洽"——扭矩大就说"手在用力"，扭矩小就说"手轻搭"。
2. **扭矩按 DAS 提醒强度分级**（demand-state）：
   - state 1（手在）：前 500ms 保持上一帧，之后停止注入。
   - state 2（轻度提醒）：延迟 2 秒，然后 ±0.5~2.0Nm 缓和随机游走；幅值到 2Nm 时锁住 1 秒。
   - state 3/4/5（强提醒）：延迟 1 秒，然后 0→2.1Nm 线性爬升并保持，1.5 秒一个周期。
3. **方向反向于方向盘转角**（需 0x129）——**本期引入**（D4，复用现有 `dashRecordApRestoreFrame` 解析）。

---

## 2. 目标与非目标

### 2.1 目标
1. 把 `DashEpasNagEngine` 的扭矩决策从"固定值（B1）"升级为 **demand-state 状态机**，对齐上游 beta.8 Mode-C。
2. 把 handsOnLevel 从"固定 level 1（B2）"升级为 **从扭矩幅值推导**，对齐上游。
3. 保留本地全部门控（G1–G6）、诊断可观测性、trace CSV——Mode-C 只替换"扭矩怎么算 / level 怎么定"两个内核。
4. 保持默认关闭、实验性开关不变；bit19 默认开 baseline 不动。

### 2.2 非目标（YAGNI）
- ✅ **0x129 方向盘转角：本期引入**（Jordan 2026-06-19 决定）。复用现有解析——`dashRecordApRestoreFrame`（`mcp2515_dashboard.h:683-692`）已每帧解析 `apRestoreState.steerAngleX10`，只需加一行喂给引擎。**不加 filter、不加 parser、无回归风险**。
- ❌ **不保留 B2 双模式**：上游已证伪固定 handsOnLevel，Mode-C 直接替换 B2，不留可切换的旧路径（避免技术债 + 诊断复杂度）。
- ❌ 不改 handler filterIds（不加 0x129/0x370，0x370 已在 filter）。
- ❌ 不删 NagHandler（旧方法，双 CAN bus4 仍用）。
- ❌ 不动 bit19 默认开 baseline。

---

## 3. 已确认的设计决策

| # | 决策点 | 选择 |
|---|---|---|
| D1 | 对齐范围 | **核心移植 Mode-C**（demand-state 扭矩机 + handsOnLevel 推导 + 0x129 方向，见 D4） |
| D2 | 实现路径 | **路径 B：适配本地 `DashEpasNagEngine` 架构**（非路径 A 纯函数搬运）。保留本地 gate/diag/trace 骨架，把 Mode-C 内核嵌进 `nextTorque` 与 `tryBuildEcho` |
| D3 | 扭矩编码基准 | **保持本地 `raw 2050 = 0Nm`**（Nm = raw*0.01 − 20.5，NagHandler byte3=0xB6=2230 实测验证）。上游数值（2048 基准）按 +2 换算到本地基准 |
| D4 | 扭矩方向 | **从 0x129 方向盘转角推导（反向助力，对齐上游）**：`dir = steerAngleX10>0 ? -1 : +1`。复用 `dashRecordApRestoreFrame`（`mcp2515_dashboard.h:683`）已解析的 `apRestoreState.steerAngleX10`（每帧更新，主分发 1051 无条件调用），只加一行 `onSteeringAngle()` 喂给引擎 |
| D5 | B2 去留 | **Mode-C 替换 B2**。B1 的固定 `kTorqueHoldNm` 与 B2 的 `\|=0x40` 均被 Mode-C 内核替代，不留双模式开关 |
| D6 | 外层门控 | **保留 G1–G6**。Mode-C 作为 G1–G6 通过后的内层扭矩决策；G5 扩展排除 `das==15` |
| D7 | 默认值 / 安全 | `dashEpasNagEnabled` 默认 OFF 不变；bit19 默认开 baseline 不动；NagHandler 不删 |
| D8 | 诊断增强 | `DashEpasNagDiag` 加 `lastDemandState` / `lastDemandTorque` / `lastHandsLevel` / `lastDemandGate`；trace CSV 复用现有 echoTorque/realTorque 列 |
| D9 | 时间常量 | 对齐上游：state1 hold 500ms；state2 delay 2000ms、level2 hold 1000ms；strong delay 1000ms、ramp 周期 1500ms（前 500ms 线性爬升） |

---

## 4. 架构（路径 B）

`DashEpasNagEngine` 骨架与职责不变（见 0617 spec §4）。本次只替换其内部两个决策点：

```
                  ┌─────────────────────────────────────────────────┐
  CAN 0x370       │  DashEpasNagEngine（现有骨架，不改外部接口）        │
  (EPAS, ~100Hz) ─▶│  ① onDasGate(): 抽 byte5[5:2] → dasHandsOnState  │ ← 不变
  0x399/0x39B     │  ② shouldInject(handsOn, nowMs): G1–G6 外层门     │ ← G5 +das==15
  (DAS 门控)   ──▶│  ③ nextDemandTorque(das, nowMs): ★ Mode-C 状态机  │ ← 新（替换 nextTorque）
  0x129 (steer)──▶│  ④ onSteeringAngle(x10): ★ 0x129 → dir（反向）   │ ← 新（D4）
                  │  ⑤ handsLevelFromRaw(raw): ★ level 推导           │ ← 新（替换 B2 |=0x40）
                  │  ⑥ tryBuildEcho(): 写帧，level 来自 ⑤             │ ← 改 level 写入
                  │  ⑦ teslaChecksum0x370(): 加法校验和               │ ← 不变
                  └─────────────────────────────────────────────────┘
```

**设计原则**：① ② ④ ⑤ ⑥ ⑦ 全是纯函数/纯逻辑，可脱离硬件单元测试。③ 状态机虽有时序，但 `nowMs` 由调用方传入（无 `millis()`/`random()`），PRNG 用 seeded xorshift32——**确定性、可单测**（与现有引擎一致，dash_epas_nag.h:16-17）。

---

## 5. 组件清单（仅列改动）

| 组件 | 改动 | 备注 |
|---|---|---|
| `nextDemandTorque(das, nowMs, outTorque, outLevel)` | **新**，替换 `nextTorque()` | 返回 bool：false=本帧不 inject（state1 超 500ms / state2 延迟内 / strong 延迟内 / das 排除值）；true=注入并填 outTorque+outLevel |
| `handsLevelFromRaw(raw)` | **新**，替换 B2 `\|=0x40` | 纯函数：`\|raw−2050\| ≥200→2, ≥100→1, else 0` |
| `onSteeringAngle(int16_t x10)` | **新**（D4 方向输入） | 存 `steerAngleX10`；dashboard glue 在 0x129 帧调用（`mcp2515_dashboard.h:683` 块内） |
| `clampInt16(v, lo, hi)` | **新** inline int16_t 钳位 | 现有 `clampTorque` 返回 uint16_t，不适用负 band |
| `tryBuildEcho()` | **改** level 写入 | `outData[4] = (inData[4] & 0x3F) \| (level<<6)`（替换 `inData[4]\|0x40`） |
| `shouldInject()` G5 | **扩展** | 排除 `das==15`（上游：`das==0\|\|das==8\|\|das==15` 不注入） |
| 状态机字段 | **新增成员** | 见 §6.2 |
| `DashEpasNagDiag` | **加字段** | `lastDemandState` / `lastDemandTorque` / `lastHandsLevel` / `lastDemandGate` |

**不改签名**：`onDasGate`、`noteRuntime`、`teslaChecksum0x370`、gate G1/G2/G3/G4/G6、kCanId/kDasId*/kBlackoutMs 等常量。
**小改（签名不变，体内补充）**：`noteApState` 顺带刷新 `apActive` 成员；`reset()` 重置上述新状态机字段 + `apActive=false` + `prevDas=0xFF`。

---

## 6. demand-state 扭矩机（核心，对齐上游 `nag_faithful_modec`）

### 6.1 状态分级（按 `dasHandsOnState`，本地 2050 基准；dir 由 0x129 推导见 §6.3）

| das | 含义 | Mode-C 行为 | 扭矩范围（raw / Nm） |
|---|---|---|---|
| 0 / 8 / 15 | NOT_REQD / SUSPENDED / (15) | 不 inject（G5/G6 外层已拦，Mode-C 兜底 return false） | — |
| 0xFF | 未见 DAS 帧 | **strong 路径兜底**（本地 D5"unseen 注入"适配：显式把 0xFF 归入 strong，见 §6.3 注） | ramp→2260 / +2.1Nm |
| 1 | 手在 | 前 500ms 保持 `lastRaw`，之后 return false | = 上一帧 |
| 2 | 轻度提醒 | 前 2000ms return false；之后 mild 随机游走（按 dir 选正/负 band）；幅值≥2Nm 锁住 1000ms | dir=+1: 2100–2250；dir=−1: 1850–2000（±0.5~2.0Nm） |
| 3 / 4 / 5 | 强提醒 | 前 1000ms return false；之后 0→±2.1Nm 线性爬升（500ms，方向按 dir），保持到 1500ms 周期重置 | dir=+1: 2050→2260；dir=−1: 2050→1840（0~±2.1Nm） |

> **das==0xFF 协调**：上游 else 分支虽把非 1/2 的值归到 strong，但其 `isStrong` 边沿判定 `(das==3||4||5)` 不含 0xFF，且上游 0xFF 时 `das_ap_state<2` 会先 return false → 实际不注入。本地只有 `apActive` bool、无法做精细 ap_state 判定，而 D5 要求"unseen 注入"，故**显式把 0xFF 归入 strong**（§6.3 `isStrong` 含 0xFF）使其走 ramp 兜底——保守地提供大扭矩，兑现"未见门控就当作需要 nag"的兜底语义。

### 6.2 新增状态机成员字段

```
uint32_t s1EnterMs   = 0;   // 进入 das==1 的时刻
uint32_t s2EnterMs   = 0;   // 进入 das==2 的时刻
uint32_t strongEnterMs = 0; // 进入 das==3/4/5/0xFF 的时刻
uint32_t s2HoldUntilMs = 0; // state2 level2 锁住截止时刻
int16_t  mildWalk     = 2050; // state2 随机游走当前值
int16_t  s2HoldRaw    = 2050; // state2 锁住时的扭矩
uint8_t  s2HoldLevel  = 0;
bool     s2Level2Active = false;
uint8_t  prevDas      = 0xFF; // 边沿检测
int16_t  lastRaw      = 2050; // state1 hold 用
uint8_t  lastLevel    = 0;
bool     apActive     = false; // 当前 AP 激活状态（noteApState 顺带更新；Mode-C 内层 AP 门用，区别于 prevApActive 的边沿用途）
int16_t  steerAngleX10= 0;     // 0x129 方向盘角度×10（onSteeringAngle 更新；0x3FFF 无效时置0）。dir = >0?−1:+1
```

### 6.3 `nextDemandTorque(das, nowMs, outTorque, outLevel)` 伪码

```
// 边沿检测：记录各状态进入时刻
if (prevDas != 1 && das == 1) s1EnterMs = nowMs;
if (das != 1) s1EnterMs = 0;
if (prevDas != 2 && das == 2) s2EnterMs = nowMs;
if (das != 2) { s2EnterMs = 0; s2HoldUntilMs = 0; s2Level2Active = false; }
// ★ 本地 D5 适配：把 das==0xFF（未见 DAS 帧）显式归入 strong，使其走 ramp 兜底。
//   上游 isStrong 仅含 3/4/5，但上游 0xFF 时 das_ap_state<2 会先 return false；
//   本地只有 apActive bool，无法做该精细判定，故显式归 strong 以兑现 D5"unseen 注入大扭矩"。
bool isStrong = (das==3||das==4||das==5||das==0xFF);
bool prevStrong = (prevDas==3||prevDas==4||prevDas==5||prevDas==0xFF);
if (!prevStrong && isStrong) strongEnterMs = nowMs;
if (!isStrong) strongEnterMs = 0;
prevDas = das;

// AP 门（Mode-C 内层冗余，对齐上游 das_ap_state<2 不注入）
if (!apActive || das==0 || das==8 || das==15) {
    lastRaw = 2050; lastLevel = 0; return false;   // demand_idle
}

// ★ 方向：反向助力（对齐上游 fsd_handler.c:973）。0x129 steerAngleX10>0（右转）→ 负扭矩；≤0 → 正扭矩
int8_t dir = (steerAngleX10 > 0) ? -1 : 1;

if (das == 1) {
    if (s1EnterMs && nowMs - s1EnterMs < 500) {     // s1_hold
        outTorque = lastRaw; outLevel = lastLevel; return true;
    }
    lastRaw = 2050; lastLevel = 0; return false;     // state1 超 500ms 停止
}
if (das == 2) {
    if (s2EnterMs && nowMs - s2EnterMs < 2000) return false;  // s2_delay
    if (nowMs < s2HoldUntilMs) {                              // s2_hold (level2 锁住)
        outTorque = s2HoldRaw; outLevel = s2HoldLevel; return true;
    }
    // s2_mild 随机游走（按 dir 选 band：+1→[2100,2250] / -1→[1850,2000]，本地 2050 基准）
    int16_t lo = (dir < 0) ? 1850 : 2100;
    int16_t hi = (dir < 0) ? 2000 : 2250;
    if (mildWalk < lo || mildWalk > hi) mildWalk = (int16_t)((lo + hi) / 2);
    mildWalk += (int16_t)((int)(xorshift32() % 25) - 12);
    mildWalk = clampInt16(mildWalk, lo, hi);
    outTorque = mildWalk; outLevel = handsLevelFromRaw(mildWalk);
    if (handsLevelFromRaw(mildWalk) == 2 && !s2Level2Active) {   // |幅值|≥2Nm 锁住（正负通用）
        s2HoldUntilMs = nowMs + 1000; s2HoldRaw = mildWalk; s2HoldLevel = 2;
    }
    s2Level2Active = (handsLevelFromRaw(mildWalk) == 2);
}
else {  // strong (das==3/4/5/0xFF)
    if (strongEnterMs && nowMs - strongEnterMs < 1000) return false;  // strong_delay
    uint32_t active = strongEnterMs ? (nowMs - strongEnterMs - 1000) : 0;
    uint16_t phase = active % 1500;                                    // strong_ramp/hold
    int16_t mag = (phase < 500) ? (int16_t)(phase * 210 / 500) : 210;  // 0→210 爬升后保持
    outTorque = (int16_t)(2050 + dir * mag);                          // ★ dir 决定正负
    outLevel = handsLevelFromRaw(outTorque);
}
lastRaw = outTorque; lastLevel = outLevel;
return true;
```

> **钳位**：`clampInt16(v,lo,hi)` 为新增 inline int16_t 辅助（现有 `clampTorque` 返回 uint16_t，不适用负 band）；mild band 已由 lo/hi 保证；strong `dir*mag` 上限 ±210 → raw ∈ [1840,2260]，在 kTorqueMin(1780)/kTorqueMax(2500) 内，仍用 `clampTorque()` 作最终保险。
> **方向默认**：`steerAngleX10==0`（0x3FFF 无效 / 未见过 0x129）→ dir=+1，回退正扭矩（与 B2 已验证方向一致），不会因 0x129 缺失而失效。

---

## 7. handsOnLevel 推导与回声帧构造

### 7.1 `handsLevelFromRaw(raw)`（纯函数，本地 2050 基准）
```
a = (raw >= 2050) ? (raw - 2050) : (2050 - raw)
if (a >= 200) return 2;   // ≥ +2.0Nm
if (a >= 100) return 1;   // ≥ +1.0Nm
return 0;
```
- raw 2260 → a=210 → **level 2**（strong ramp 峰值）
- raw 2250 → a=200 → level 2（mild 上界）
- raw 2150 → a=100 → level 1
- raw 2050 → a=0 → level 0

### 7.2 `tryBuildEcho()` 改动（只列 level 写入，其余字节布局同 0617 §7.2）
```
// 旧（B2）: outData[4] = inData[4] | 0x40u;            // 固定 level 1
// 新（Mode-C）:
if (!nextDemandTorque(dasHandsOnState, nowMs, torq, level)) {
    diag.suppressCount++; diag.lastGate = "demand_idle"; return false;
}
diag.lastDemandState = dasHandsOnState;
diag.lastDemandTorque = torq;
diag.lastHandsLevel = level;
diag.lastDemandGate  = <见 6.3 注释 s1_hold/s2_mild/strong_ramp 等>;
diag.lastEchoTorque  = torq;
outData[2] = (inData[2] & 0xF0u) | ((torq >> 8) & 0x0Fu);
outData[3] = (uint8_t)(torq & 0xFFu);
outData[4] = (uint8_t)((inData[4] & 0x3Fu) | (level << 6));   // ★ level 推导写入 bits[7:6]
// outData[0/1/5/6/7] 同现有（透传 + counter+1 + checksum）
```

### 7.3 扭矩物理换算（不变，本地 2050 基准）
- `Nm = raw*0.01 − 20.5`
- raw 2050↔0Nm；2100↔+0.5Nm；2150↔+1.0Nm；2250↔+2.0Nm；2260↔+2.1Nm

---

## 8. 错误处理与安全机制

| 场景 | 处理 |
|---|---|
| G1–G6 外层门未过 | 零发送（不变） |
| Mode-C 判定本帧不 inject（延迟窗/hold 结束/das 排除） | `suppressCount++`，`lastGate="demand_idle"`，零发送——**非失败，是按需停手** |
| state2 mild 游走出界 | lo/hi 钳位 + 重新居中 |
| strong mag 超 210 | phase 逻辑上限 210，`clampTorque()` 兜底 |
| AP 脱开边沿 | G6 静默窗（1.5s，kBlackoutMs）零发送（不变） |
| das==0xFF（未见 DAS） | strong 兜底注入（D5 一致） |
| 开关持久化 | 默认 OFF；开启跨重启（不变） |
| PRNG | xorshift32 种子 `0xDEADBEEF`（确定性，不变） |

**安全不变量（硬保证，不变）**：防御关→零发送；实验性开关关→零发送；脱开静默窗内→零发送；bit19 baseline 默认开不受影响。

---

## 9. 实车验证风险与已知局限

| 风险 | 说明 | 缓解 |
|---|---|---|
| **Mode-C 未实车** | 上游 host 测试 248 通过但作者注明"needs on-car validation" | 默认关闭 + 纯逻辑测试 100% 对齐上游；diag/trace 全链路可观测 |
| **0x129 方向依赖现有解析** | dir 来自 `steerAngleX10`（`dashRecordApRestoreFrame` 解析） | 复用成熟解析、不加 filter/parser；0x129 缺失或 0x3FFF 无效时 dir 回退 +1（不失效）；实车用 trace 的 handsOn/echoTorque 旁路加 steerAngle 验证方向 |
| **das==0xFF 兜底走 strong** | 未见 DAS 时发大扭矩，可能过于激进 | 与本地 D5"unseen 注入"语义一致；实车若 EPAS 反应异常可收窄为 mild 或 fail-closed |
| **回声竞速**（继承 0617 #1） | counter+1 回声须抢在真帧前 | NagHandler 已验证通路；trace 的 swTxErr/twaiTec 列监控 |
| **仅 0x370 总线**（继承 0617） | 只对承载 0x370 的底盘 CAN 有效 | 已知局限，文档化 |

---

## 10. 测试策略

### 10.1 纯函数 / 状态机单元测试（`test/test_native_epas_nag/`，最重要）
- `handsLevelFromRaw`：2260→2、2250→2、2150→1、2050→0、1950→1（负向）、1850→2。
- `nextDemandTorque` demand-state 真值表（对齐上游 test_fsd_core.c:333-410）：
  - das==0/8/15 → return false。
  - das==1：进入后 <500ms 返回 lastRaw（hold）；>500ms 返回 false。
  - das==2：进入后 <2000ms return false；>2000ms 返回 [2100,2250] 内扭矩；幅值≥2250 时后续 1000ms 锁住。
  - das==3/4/5：进入后 <1000ms return false；之后扭矩从 2050 线性爬到 2260（500ms 内），保持；1500ms 周期重置回 2050。
  - das==0xFF → strong 路径（兜底）。
  - 边沿检测：das 1→2→3 切换时各 enterMs 正确重置。
  - AP 关（apActive=false）→ return false。
  - **方向（D4）**：`onSteeringAngle(500)`（右转）→ state2 mild 落 [1850,2000]、state3 strong 走 2050−mag（峰值 1840）；`onSteeringAngle(0)` 或未调 → 正 band [2100,2250] / 2050+mag（峰值 2260）；`onSteeringAngle(-300)`（左转）→ 正 band（≤0 同正）。
- `tryBuildEcho` level 写入：strong 峰值时 `outData[4] & 0xC0 == 2<<6`；mild 中段时 `== 1<<6`；counter+1、checksum 正确（回归）。
- `shouldInject` G5 扩展：das==15 → false（新）；das==0xFF → true（D5 回归）。

### 10.2 旧测试更新（B1/B2 被 Mode-C 替换）
- B1 `test_torque_returns_fixed_positive`（断言 2230）→ 改为 `test_demand_torque_state3_ramps_to_2260`。
- B2 `test_echo_forces_handson_active`（断言 `out[4]&0x40`）→ 改为 `test_echo_level_derived_from_torque`（strong→level2, mild→level1）。

### 10.3 契约测试（`test/test_epas_nag_*_contract.py`）
- `/status` epasNagDiag 含新字段 `lastDemandState`/`lastDemandTorque`/`lastHandsLevel`/`lastDemandGate`。
- `handsLevelFromRaw` / `nextDemandTorque` / `onSteeringAngle` / `clampInt16` / `demand_idle` 字符串存在于源码。
- `mcp2515_dashboard.h` 的 `0x129` 块内调用 `dashEpasNagCtrl.onSteeringAngle(...)`。

### 10.4 安全不变量测试（硬保证，回归）
防御关→零发送；开关关→零发送；脱开窗内→零发送。

---

## 11. 开放问题与假设

1. ~~对齐范围~~ → **已决议（D1）**：核心移植 Mode-C + 0x129 方向（Jordan 2026-06-19 扩展）。
2. ~~实现路径~~ → **已决议（D2）**：路径 B 适配本地架构。
3. ~~扭矩基准~~ → **已决议（D3）**：保持本地 2050，上游数值换算。
4. ~~方向~~ → **已决议（D4）**：从 0x129 `steerAngleX10` 推导（反向助力），复用现有解析。
5. ~~B2 去留~~ → **已决议（D5）**：Mode-C 替换，不留双模式。
6. **B2 板子（6537e91）实车**：Jordan 尚未实测 B2。因上游强信号已表明固定 handsOnLevel 无效，Mode-C 实现后直接烧录覆盖 B2，B2 不单独测（避免浪费实车轮次）。若 Jordan 仍想先测 B2 收集对照数据，可在 Mode-C 资产生成前插入。
7. **dash_epas_nag.h 头部注释**：实现后将 Spec 引用从 0617 更新为 0619（或并列两个）。

---

## 12. 实现切入点（供 writing-plans 展开）

1. **`include/dash_epas_nag.h`**（主战场）：
   - 加状态机成员字段（§6.2）。
   - 删 `nextTorque()`（B1 固定）/ 其调用，加 `nextDemandTorque(das, nowMs, outTorque, outLevel)`（§6.3）。
   - 加 `handsLevelFromRaw(raw)` 纯函数（§7.1）。
   - 加 `onSteeringAngle(int16_t x10)` 方法 + `steerAngleX10` 成员（D4 方向输入）。
   - 加 `clampInt16(v,lo,hi)` inline 辅助（负 band 钳位）。
   - 改 `tryBuildEcho`：调 `nextDemandTorque`，level 推导写入 `outData[4]`（§7.2），删 B2 `|=0x40`。
   - `shouldInject` G5 加 `das==15` 排除。
   - `DashEpasNagDiag` 加 4 个观测字段。
   - `reset()` 重置新状态机字段 + `steerAngleX10=0`。
   - 头部注释 Spec 引用更新。
2. **`include/web/mcp2515_dashboard.h`**：
   - `handleStatus` 的 `epasNagDiag` 序列化加 4 个新字段（仿现有 hw3SlewCount 模式）。trace CSV 列不变（echoTorque/realTorque/handsOn 复用）。
   - **0x129 glue（D4）**：在 `dashRecordApRestoreFrame` 的 `0x129` 块（`:683-692`）解析 `steerAngleX10` 后，加 `dashEpasNagCtrl.onSteeringAngle(apRestoreState.steerAngleX10);`（return 前）。`dashEpasNagCtrl` 是文件级 static（`:200`），对该函数可见。
3. **`test/test_native_epas_nag/test_epas_nag.cpp`**：加 demand-state 测试（§10.1），更新 B1/B2 旧测试（§10.2）。
4. **`test/test_epas_nag_*_contract.py`**：加新字段/新函数契约（§10.3）。
5. **构建 + 资产**：`pio run -e waveshare_single_can_standalone` + `python3 scripts/minify_dashboard.py`（若 ui.h 受影响）+ `git restore include/web/mcp2515_dashboard_ui.h`（去 build-id stamp）+ esptool merge_bin 资产。
6. **烧录**：授权后 esptool erase_flash + write_flash full_merged（覆盖 6537e91）。

---

## 附录 A：上游 Mode-C 关键代码定位

| 上游文件 | 行 | 内容 |
|---|---|---|
| `esp32/.firmware/fsd_handler.cpp` | 354-470 | `nag_faithful_modec()`（ESP32 端 Mode-C 实现，本 spec §6 对齐源） |
| `esp32/.firmware/fsd_handler.cpp` | 357-362 | `nag_hands_level_from_raw()`（§7.1 对齐源） |
| `fsd_logic/fsd_handler.c` | 428/438/468 | `das_hands_on_state = (buffer[5]>>2)&0x0F`（本地 `onDasGate` 已对齐） |
| `fsd_logic/fsd_handler.c` | 879 | `steering_angle_deg = raw*0.1f`（0x129 解析公式，本地复用 `steerAngleX10`，符号等价） |
| `fsd_logic/fsd_handler.c` | 973 | `dir = (steering_angle_deg > 0.0f) ? -1 : 1`（反向助力，§6.3 对齐源） |
| `test/test_fsd_core.c` | 333-410 | demand-state 测试（§10.1 对齐源） |

## 附录 B：CAN ID 速查

| CAN ID (dec/hex) | 名称 | 用途 |
|---|---|---|
| 880 / 0x370 | EPAS3P_sysStatus | 注入目标 + 读 handsOn + 写扭矩/level |
| 921 / 0x399 | DAS_status (legacy/HW3) | DAS 门控 byte5[5:2] |
| 923 / 0x39B | DAS_status (Highland HW3/HW4) | DAS 门控 byte5[5:2] |
| 297 / 0x129 | SteeringAngle | **本期复用现有解析**（`mcp2515_dashboard.h:683` 的 `steerAngleX10`），反向助力方向输入 |
