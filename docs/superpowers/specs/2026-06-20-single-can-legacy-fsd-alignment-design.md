# 单 CAN 微雪版 FSD 功能对齐设计（参考 tesla-fsd-controller）

- **日期**: 2026-06-20
- **状态**: 设计已确认（brainstorming 5 段全部通过），待实现规划
- **项目**: LILYGO-T-2Can-firmware，构建环境 `waveshare_single_can_standalone`（单 CAN 微雪版）
- **参考实现**: `/Users/ziwind/my-vibe-project/tesla-fsd-controller`
- **实车协议模式**: **Legacy**（非 HW3）

---

## 1. 背景与目标

把 tesla-fsd-controller 的 5 个 FSD 行为修改功能，对齐到单 CAN 微雪版：

1. legacy 速度偏移
2. 关闭视觉速度限制识别
3. 方向盘检测屏蔽
4. 限速提示音抑制
5. 重写速度限制功能

**关键发现**：单 CAN 微雪版当前默认烧 HW3 模式（`DASH_INITIAL_HW_MODE=3`），而实车是 Legacy 协议——模式配错导致 Legacy 报文（`0x3EE`/`0x2F8`/`0x438`）无人接收。切到 Legacy 模式后，多数功能本就存在。真正需要写新代码的缺口只有 2 个。

---

## 2. 安全禁令（硬约束，不可妥协）

⛔ **禁止未经台架验证的 `0x370`（EPAS 自身状态报文 EPAS3P_sysStatus）注入。** 本任务全程不碰 0x370（功能 3 走 bit-19 baseline）。

- 依据：2026-06-19 实车事故——向 `0x370` 注入合成扭矩+handsOn 帧（demand-state 动态状态机），触发车身控制系统禁用、车辆严重故障，拔模块+重启车机才恢复。属 **life-critical**。事故根因是动态扭矩方式，不是"任何注入必然立即危险"，但 0x370 注入仍有原理性风险（counter 冲突、与真 ECU 竞争同 ID）。
- 详见 `docs/EPAS-NAG-REMOVAL-INCIDENT.md`（2026-06-20 已订正措辞：从"禁止任何注入"精化为"禁止未经台架验证的注入"）。
- 守卫测试 `test/test_no_epas_nag_contract.py` 锁 8 符号（`DashEpasNag`/`dashEpasNag`/`epasNagDiag`/`tryBuildEcho`/`nextDemandTorque`/`EpasNagTraceRing`/`dash_epas_nag`/`def_epnag`）+ 禁止 `[env:native_epas_nag]`，必须全程保持绿。
- 复活判据：(1) 是否向 `0x370` 注入？若否，不受约束（bit-19 baseline 即此）。（2）若是，是否经台架验证（车辆不上路）证明不触发 EPAS 故障？未验证 → 停止。
- 放行：bit-19 baseline（在 `0x3EE`/`0x3FD`/`1021` mux-1 上 `setBit(19,false)`，非 0x370）。

---

## 3. 关键决策记录（brainstorming 过程）

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 功能 3 方向 | 只走 bit-19 baseline，绝不碰 0x370 | Jordan 决策；符合禁令 |
| NagHandler 处置 | 暂不处置，列 backlog | Jordan 决策；本任务不碰 0x370 |
| 推进范围 | 全 HW 模式对齐（Legacy/HW3/HW4） | Jordan 决策；设备通用性 |
| 执行顺序 | Legacy 优先验证 | 实车 Legacy，能立即上车验证 |
| 功能 2 门控 | 改代码让 `visionLimitClearEnable` 独立于 policy | 避免 Experimental policy 牵连其他行为 |

---

## 4. 范围

- **对象**：单 CAN 微雪版三套 handler——`LegacyHandler`、`HW3Handler`、`HW4Handler`（均在 `include/handlers.h`）。
- **原则**：每个功能对齐 tesla-fsd-controller **在它支持的模式下**的实现。tesla 不支持的模式（该架构车辆总线上不存在的报文），标注"无 tesla 基准"，**不强行造**——避免无依据注入。
- **报文按 HW 架构归属**：
  - Legacy 协议帧：`0x3EE`(1006)、`0x2F8`(760)、`0x438`(1080)
  - HW3/HW4 帧：`0x399`(921)、`0x3FD`(1021)
  - HW4 专属：`0x39B`(923)

---

## 5. 三模式 × 5 功能缺口矩阵

图例：✅ 已有且对齐 tesla / 🔧 需新增代码 / ❓ 待确认 / ➖ 不做

| 功能 | Legacy | HW3 | HW4 |
|------|--------|-----|-----|
| **1 速度偏移** | ✅ `0x2F8` byte5（`handlers.h:319-335`） | ✅ `0x3FD` mux-2（`handlers.h:612-665`） | ✅ `0x3FD` mux-2 hw4OffsetRaw（`handlers.h:966-967`） |
| **2 视觉限速 clear** | ✅ `0x3EE` bit48（`handlers.h:465`，门 policy） | ➖ 不做（`0x3EE` 不存在） | ➖ 不做（同） |
| **3 方向盘屏蔽 bit-19** | ✅ `0x3EE` mux-1 | ✅ `1021` mux-1（`handlers.h:604-610`） | ✅ `1021` mux-1（`handlers.h:953`） |
| **4 chime 抑制** | ❓ 待确认（`0x399` 可能不存在） | 🔧 **需新增**（921 补写入） | ✅ `0x399` |=0x20（`handlers.h:856-864`） |
| **5a 重写限速 0x438** | ✅ slider=100（`handlers.h:337+`） | ➖ 不做（`0x438` 不存在） | ➖ 不做（HW4 用 5b） |
| **5b 重写限速 0x39B** | ➖ 不适用 | ➖ 不适用 | 🔧 **需新增**（923 补写入） |

**结论**：真正需要写新代码的缺口只有 **2 个**；功能 1、3 三模式已齐；其余是配置/policy 对齐。

---

## 6. 逐功能改动设计

### A. 先决步骤（让任何 Legacy 功能生效）
1. 微雪版切 Legacy 模式：Dashboard UI 切 `hwMode=0`（运行时），或改 `platformio.ini` 的 `DASH_INITIAL_HW_MODE`。
2. 上车 L0 验证功能 1/3/5a（应已工作）。

### B. 缺口 1 — HW3 chime 抑制（功能 4）
- **位置**：`include/handlers.h` → `HW3Handler::handleMessage` 的 921 分支（当前 `handlers.h:536-547` 只读 `fusedSpeedLimitRaw`）。
- **改动**：照搬 `HW4Handler:856-864` 成熟逻辑——
  ```cpp
  // ISA chime suppress — runtime gate
  if ((bool)isaChimeSuppress && frame.dlc >= 8 && injectionAllowed()) {
      frame.data[1] |= 0x20;
      frame.data[7] = computeVehicleChecksum(frame);
      framesSent++;
      driver.send(frame);
      if (onSend) onSend(0, true);
      return;
  }
  ```
- **门控**：`isaChimeSuppress && injectionAllowed()`，默认关。
- **NVS key**：复用现有 `fd`（`isaChimeSuppress`），UI 复用 defense 页 `sound_warning_suppression` 开关。
- **native 测试**：新增，验证 HW3 模式 921 写入 `|=0x20` + 校验和正确。

### C. 缺口 2 — HW4 重写限速 5b（功能 5b）
- **位置**：`include/handlers.h` → `HW4Handler::handleMessage`，新增 923(`0x39B`) 写入分支（当前 filter 含 923 但无写入逻辑）。
- **改动**：对齐 tesla `handleDASStatusISAOverride`——
  ```cpp
  if (frame.id == 923 && frame.dlc >= 8 &&
      (bool)isaOverride && injectionAllowed()) {
      frame.data[1] |= 0x1F;   // DAS_fusedSpeedLimit = 31 (NONE)
      frame.data[2] |= 0x1F;   // DAS_visionOnlySpeedLimit = 31 (NONE)
      frame.data[7] = computeVehicleChecksum(frame);
      framesSent++;
      driver.send(frame);
      if (onSend) onSend(0, true);
      return;
  }
  ```
- **门控**：`isaOverride && injectionAllowed()`，默认关。
- **NVS key**：新增 `isaOverride`（参考 tesla key `df`）。注意区别于功能 5a 的 `overrideSpeedLimit`（现有 key `fi`，作用于 `0x438` slider=100）——5a 与 5b 是不同报文、不同功能，不可混用开关。
- **native 测试**：新增，验证 HW4 模式 923 写入 raw=31 + 校验和正确。

### D. 配置/policy 对齐（不用写新代码）
| 功能 | 改动 |
|------|------|
| 1 速度偏移 | 三模式已有；统一 NVS key + UI 开关 + 默认 0（关） |
| 2 视觉限速 | Legacy 已有；**改门控**：去掉 `policy==Experimental` 限制，让 `visionLimitClearEnable` 在任意 policy 独立生效（`handlers.h:465`） |
| 3 bit-19 | 三模式 baseline 已有；可选加 UI 可见开关（当前 FSD 触发即清，用户不可见） |
| 5a 重写限速 | Legacy 已有；统一配置 |

### E. 配置项/UI 统一原则
- 5 个功能在 Dashboard 都有清晰开关（部分目前只埋在 `/api/config` JSON 里没 UI）。
- **默认全关**（安全第一）。
- 配置项命名/默认值对齐 tesla-fsd-controller（用户视角一致）。

---

## 7. 测试与验证分级

### Native 测试（无硬件）
对齐项目现有 `test_native_hw3/hw4/legacy` + pytest 模式：
- 缺口 1：HW3 921 写入 `|=0x20` + 校验和
- 缺口 2：HW4 923 写入 raw=31 + 校验和
- 功能 2 独立门控：`visionLimitClearEnable` 在 Stable/Experimental/TeslaParity 任意 policy 生效
- 默认值：所有新开关默认关
- 守卫回归：`test_no_epas_nag_contract.py` 保持绿
- 全量回归：现有 native + pytest 全绿

### 实车验证分级
**L0（立即，Legacy 实车）**：
- 模式确认 `effectiveHw=LEGACY`
- 功能 1：设 offset，看仪表/FSD 设定速度
- 功能 3：FSD 期间 nag 是否抑制
- 功能 5a：开 `overrideSpeedLimit`，看限速覆盖
- 功能 2：开 `visionLimitClearEnable`，看视觉限速牌识别关闭
- 功能 4 ❓ 定论：**抓总线确认 Legacy 车有无 `0x399`(921)**——有则 chime 适用，无则 Legacy 不做

**L1（代码改完后，HW3/HW4 待台架/其他车）**：HW3 chime、HW4 5b，标"待实车验证"，默认关。

### 回滚机制
- 每个功能×模式独立开关，默认关 → 出问题关开关即停注入（不必回烧）。
- 固件级：每步保留上个已知好版本，可回烧。

### 上车核对清单（每步）
- [ ] 模式 = Legacy
- [ ] 功能开关状态符合预期
- [ ] 注入计数 `framesSent` 有增长
- [ ] 无故障灯/车辆异常（**异常立即关开关 + 拔模块**）

---

## 8. 部署顺序（增量，每步验证）
1. **先决**：微雪版切 Legacy 模式 → L0 验证功能 1/3/5a
2. 功能 2 独立门控改动 → L0 验证视觉限速 clear
3. 缺口 1（HW3 chime）代码 + native 测试（L1 待验证）
4. 缺口 2（HW4 5b）代码 + native 测试（L1 待验证）
5. 配置项/UI 统一 + 全默认关
6. 全量回归 + 守卫测试绿
7. 生成 Legacy 资产烧录 → L0 实车验收

---

## 9. Backlog 与紧急项

### Backlog（本任务不做，明确记录）
1. **⛔ NagHandler 0x370 注入清理**（life-critical 安全债）：现存 `NagHandler`（`handlers.h:705-828`）仍向 `0x370` 注入篡改扭矩(`0xB6`)+handsOn(`|0x40`)+counter+1+校验和帧，与禁令判据冲突（禁令文档把它误述为"只 counter+1 轻度回声"，与代码不符）。建议按禁令精神物理删除，并修正禁令文档。
2. 功能 4 Legacy `0x399` 适用性：待 L0 抓总线确认。
3. HW3/HW4 缺口实车验证：待 L1。

### 🚨 紧急并行项（独立于本任务，强烈建议先做）
车上若烧 `apactive-fix`（commit `2f75109`，**事故触发版**，0x370 注入路径已打通），是即时风险。建议本任务开始前先换 `fix/remove-dangerous-epas-nag` 分支构建的 nag 已删干净版。

---

## 10. 主要风险与对策
| 风险 | 对策 |
|------|------|
| 报文/字段 HW 适用性 | L0 实车验证为准，不靠代码推断 |
| 功能 2 policy 牵连 | 独立门控规避 |
| HW3/HW4 改动无法立即验证 | 标 L1 待验证，默认关 |
| 0x370 禁令 | 本任务不碰，守卫测试把关 |
| apactive-fix 事故版在车上 | 紧急并行项，优先换干净版 |

---

## 11. 明确不做（YAGNI / 无 tesla 基准）
- 功能 2、5a 在 HW3/HW4（报文不存在）
- 功能 5b 在 Legacy/HW3（`0x39B` HW4 专属）
- 功能 4 在 Legacy 待确认（`0x399` 可能不存在）
- **任何形式的 `0x370` 注入**
