# TX 分路 v2：0x3EE offsetOnly 路由 + bit27 nagKill 并行路径（2026-11-23 终版）

> v1 草案的字段映射被 builder 审查推翻（误读自 0x2F8 布局）；终版改走
> 仓库既有 offsetOnly 通道，字段正确性由 beta02 六周田野写入器背书。
> v2 增补（Jordan 11-23 晚）：bit27 状态克隆作为与滚轮 DND **并行**的
> 第二条免打扰路径（参考实现=Jordan 提供的逆向伪代码，参数与之对齐）。

## 1. 背景与田野证据

beta.04 实车（9 段录制）判定 TX 分路 v1 失败：

- **速度偏移失效根因**：A3 0x2F8 骑载移植自 Atlas P4 三总线（0x2F8 住
  PT 线），T2CAN 单接 X197——1080s 录制 0x2F8 宿主帧 0 次，物理不可能。
- **DND 间歇死亡**（段③：T·0x45=0 且 hs=3 持续 14s）：与 0x3EE 开关无
  因果（段②实证）。根因=TX 两连败 disarm 后无自愈，唯一复位=手动 toggle。
- **开关显示不同步**：`ci = canActive && fsd3eeInject` 混入总线状态。
- **注入开时车机卡顿**：速度请求 burst（2s 5 帧）叠加原生流。

### 物理标定（段① 实测 + 仓库写入器交叉验证）

| 事实 | 值 |
|---|---|
| X197 线上 id | 仅 7 个：0x45/0x118/0x186/0x370/0x399/0x3EE/0x488 |
| 0x3EE 原生流 | mux0+mux1 交替 ≈1.9Hz，帧内容完全静态（无 counter） |
| **速度偏移字段** | **b3 bits1-6**（kph+30 六位），bit7 强制 1，bit0 保留（`dashWriteLegacyOffsetTo3eeMux0`，`dash_legacy_smart_offset.h:70-77`） |
| **b5 常量** | `b5 |= 0x03` 解锁位模式（jitter 克隆同族 `|=0x43`） |
| **b7** | bits0-5=偏移百分比（数据字段，本仓库不给 0x3EE 算 CRC） |
| **激活 bit46** | **data[5] bit6（b5 bit6）**，`can_helpers.h` LSB-first setBit |
| beta02 引擎形态 | 原生帧后 1ms 跟发改写（RX 内联发送，同步 CAN 任务） |

段① 实测帧 `BC 20 C0 0C A8 → D4 20 C3 0C 9E` 按写入器解码：b3 原码
30→42（偏移 0→+12kph）、b5|=0x03、b7 百分比重写——与写入器逐位吻合。

## 2. 方案（Jordan 拍板：全骑载 + offsetOnly 路由）

**核心：把 master-off 分支对 mux0 的处理重新路由到 beta02 既有的
offsetOnly 通道**（`handlers.h:1008-1019`，diag `offsetOnlyTxOk`）——
不造新引擎，拆掉一堵错墙。

```
开关开（默认） → beta02 原版全量（激活引擎 + offsetOnly 通道原样）
开关关         → 激活引擎停
                ├─ DND：照骑 0x45（beta02 原版 + 事件内自愈，§2.2）
                └─ mux0：offsetOnly 通道照常跑（纯速度写入）
                   mux1：零发送（提前返回保持）
```

### 2.1 mux0 路由改造（`handlers.h` ~L900）

现 master-off 提前返回改为：`readMuxID` 提到返回之前；index==0 且
`legacySpeedOffsetRequested()` 且既有 gate 链（`injectionAllowed()` +
`abortGuard` + busLock + `builtInFsdInjectionAllowed()` +
`jitterInhibit3ee()`）通过 → 走 offsetOnly 发送（原生帧后内联跟发，
1:1 零 burst）；index==1 保持提前返回（mux1 零发送）。

master-off 分支当前跳过的状态更新（fsdTriggered/ADEnabled、mux0 diag、
jitterObserve3ee）保持跳过（beta.04 行为不变）。

### 2.2 DND 事件内自愈（Jordan 选定）

`dash_legacy_wheel_045_dnd.h` 引擎改造：

1. 新增 `disarmedAtMs_` 时间戳（引擎当前没有）。
2. disarm（TX 两连败）后 **800ms** 自动 `resetRuntime()` 复活（单 CAN
   2d498da 先例）。
3. **防循环闸**：同一警告事件内自愈至多 3 次；第 3 次后进入 30s 冷却，
   冷却期满后由警告**新沿**（引擎自身 `!isWarningState→isWarningState`
   区间定义，3-6/9-10）复活。
4. **顺序陷阱**（builder 抓出）：自愈检查必须在 `onDasStatus` 的
   `wasWarning` 计算**之前**执行——`resetRuntime()` 会清
   warningActive_/episodeActive_，放在末尾会把同一边沿刚开的 episode
   清掉、闲置一个 0x399 周期。
5. 用 0x45 引擎自己的 `resetRuntime()`（不是 CAN2 0x3C2 引擎的
   `clearAutoDisarm()`）。

### 2.3 `ci` 单一真源

`/status ci` 改为纯 `fsd3eeInject`。消费者 11 处已全枚举
（ui.src.h :2304/:2333/:2335/:2338-2340/:2363/:2403/:2409/:2413/:2444/
:2653-2680），无一真需要 canActive 语义；topbar 已有 `d.can` 连接显示。
可见变化：canActive=0+开关=1 → 「FSD ON」+「未连接」并存（正确语义）。
顺带关闭 :2657 记录的乐观切换不匹配。

### 2.4 bit27 nagKill 并行路径（新增，默认关·实验）

**形态：与 DND 完全同构的骑载引擎**（参数照 Jordan 提供的参考伪代码，
非 11-18 旧版）：

- **触发**：0x399 到达，`hands = data[5]>>2 & 0xF ∈ [3,5]` → 置触发
  标志（每警告沿刷新；**不到 6**——strike-out 域红线保持）。
- **注入**：下一个**原生** 0x045 到达时：清标志 → 复制该帧 → 置 bit27
  （byte3 bit3，LSB-first）→ counter+1（b6 高半字节）→ CRC8-J1850
  重算 → 重发。**单发**：一个警告沿至多一帧，无间隔重发。
- **安全栅**（全套继承 DND 纪律）：own-echo 静默（重发帧回来不重触）；
  nativeFresh（原生 0x45 150ms 窗内才骑）；TX 两连败 disarm + 800ms
  事件内自愈（与 DND 同机制）；`hands ≥ 6` 硬关。
- **与滚轮 DND 的共存协调**：
  1. **互认**：bit27 克隆帧以 bit27 位为标记；DND 引擎把「bit27=1 的
     0x45」视为兄弟引擎帧——**不算原生帧**（不刷新 nativeFresh）、
     **不触发配对逻辑**。
  2. **让路**：bit27 触发帧发射的那一拍，DND 跳过本次 nextFrame（下
     一个原生帧再继续配对），避免双帧 counter 竞争。
  3. 两者独立开关、独立诊断（`nagKillDiag`：cloned/blockedReason/
     disarmed 计数）。
- **UI**：「Legacy 左滚轮免打扰」开关**上方**新增「bit27 状态克隆免打扰」
  （`nag-kill-tgl`），默认关，实验徽标，描述注明与 DND 并行可单开。
- **NVS 键**：`nk27`（新键，规避 beta17 时代 `def_c27` 残留值——已 OTA
  过 beta17-21 的板上旧键可能为 on，新键默认 off 安全）。
- **历史风险披露**：bit27 参与过 beta17-21（beta21 红轮现场三特性
  之一，根因未定案即回退）。本设计在 beta02 干净基线+全新参数（窗
  [3,5]、单发、跟帧）上重启，**默认关+实验徽标+单独田野验证**后才
  考虑转正。

### 2.5 UI 与文案

- defense 页 0x3EE 开关描述改：「关闭后本机 0x3EE 激活注入停发（含
  防甩程序——其 0x3EE 写入同样受本开关管辖）；速度智能偏移继续经
  0x3EE 纯速度通道（offsetOnly）写入速度字段，不碰激活位；左滚轮
  免打扰不受影响」。
- 诊断：`legacySmartSpeed` 增 `offsetOnlyTxOk` 暴露（已计数未上
  JSON）；删 `ride2f8TxOk`/`rideBlockedReason`（0x2F8 引擎整体移除）。

## 3. 非目标

- 不动 beta02 滚轮 DND 配对引擎本体（只加自愈+时间戳+bit27 互认）
- 不动激活引擎与 offsetOnly 通道本体（只改路由 gate）
- 不引入新帧 id；0x2F8 路线永弃
- bit27 不做 1500ms 间隔重发（旧版形态，弃）

## 4. 测试计划（含 B2 迁移项）

**迁移**：删 0x2F8 骑载钉（`test_legacy_handler.cpp:1223-1250`）+
`ride2f8TxOk` 字段；重写 `test_3ee_master_off_stops_all_0x3ee_writes`
（:1213）语义=master-off 时激活位帧消失但 offsetOnly 速度帧允许。

**新钉**：
1. **字节奇偶校验**：开关关的 offsetOnly 输出 ≡ 开关开的 offsetOnly
   输出（逐字节）；b3 按写入器变、b5|=0x03、b7 百分比、bit46 中立
2. master-off + offset-Off → mux0/mux1 全零
3. mux1 永不骑；own-echo 不链式；abortGuard/checkAD 门控矩阵
4. busLock/镜像总线 fail-closed（扩 `test_legacy_3ee_mirrored_buses_
   fail_closed` :651 到 offsetOnly）
5. DND：disarm 后 800ms 复活且发射配对；3 次后 30s 冷却；冷却内新沿
   忽略；健康引擎的普通警告沿**不**触发 resetRuntime（回归钉）
6. `/status ci` 纯度（fsd3eeInject 翻转必跟随，canActive 翻转不跟随）
7. `offsetOnlyTxOk` 进 status 契约 token（`test_dashboard_api_contract.py`
   模式）
8. **bit27 钉**：警告沿(hs=3)+原生 0x45 → 恰一帧克隆（bit27 置位、
   counter+1、CRC 正确）；hs=2 或 hs=6 → 零发射；单发（同沿第二原生帧
   不再发）；own-echo 不链式；**互认**：bit27 帧不刷新 DND nativeFresh、
   不触发 DND 配对；让路：bit27 发射拍 DND 零帧；disarm 自愈同 §2.2；
   默认关（NV nk27 不存在=off）
9. bit27 默认关的 blob/UI 钉（开关存在于 DND 上方、默认 off）

## 5. 田野验证清单

1. 开关默认开：全行为=beta02 无回归
2. 关开关+原生激活：DND 清警告；**offsetOnlyTxOk 增长**；0x3EE 激活
   形态帧（b5|=0x43/burst）消失；无红轮无卡顿
3. 段③重演测试：人为制造 TX 失败 → 800ms 内自动恢复
4. **bit27 单独验证**（DND 关、只开 bit27）：警告沿出现时恰一帧克隆、
   警告是否被预防/缓解、`nagKillDiag.cloned` 增长、**无红轮**
5. **并行验证**（DND+bit27 同开）：无 counter 冲突告警、两者诊断独立
   增长、无红轮
6. 跑完抓 /status 回传（legacySmartSpeed + wheelDndDiag + nagKillDiag）
