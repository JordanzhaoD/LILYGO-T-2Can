# Legacy 0x045 档杆免打扰 — 建议方案与实施路径

> 状态：方案建议（未动代码）
> 目标：作为「滚轮免打扰」(`dash_legacy_wheel_045_dnd.h`) 的平行路径，解决滚轮 DND「时不时还会出现提醒」的 bug。
> 依据：本地 modelsx_intel 反编译库 + opendbc DBC + 项目实车标定三方交叉验证。
> **术语澄清（用户 2026-11-16 确认）：「档杆」= 速度控制杆 / 智能驾驶拨杆 = `SpdCtrlLvr_Stat`。**

---

## 一、档杆的确切位置与完整值域

「档杆」= `SpdCtrlLvr_Stat`，位于 **0x045 `STW_ACTN_RQ` 帧的 data[0] bit0-5**（opendbc Intel 小端起始位 0·6）。

opendbc 权威值域（`VAL_ SpdCtrlLvr_Stat`）：

| 值 | 宏 | 手势 | 对 AP 状态的影响 |
|---|---|---|---|
| 0 | IDLE | 杆回中 | 无（idle 基线，data[0]=0x40 含 VSL_Enbl_Rq bit6=1） |
| 1 | FWD | 前推到底 | **取消 AP / 降速** |
| 2 | RWD | 后拨到底 | **启动 AP / 加速** |
| 4 | UP_2ND | 上拨（二档） | 无（设定速度 +） |
| 8 | DN_2ND | 下拨（二档） | 无（设定速度 −） |
| 16 | UP_1ST | 上拨（一档） | 无（设定速度 +） |
| 32 | DN_1ST | 下拨（一档） | 无（设定速度 −） |

**关键区分（本方案的核心安全边界）：**

- `FWD(1)/RWD(2)` 是**电平型前后拨**，车辆会当作「取消/启动 AP」指令——**绝不能用于清除 NAG**。
- `UP_1ST(16)/DN_1ST(32)/UP_2ND(4)/DN_2ND(8)` 是**上下拨设定速度**手势，**不改变 AP 状态**——这才是「拨档杆清除 NAG」的正确物理动作（特斯拉实车上，AP 行驶中上下拨动速度杆即为清除「请轻握方向盘」提醒的标准动作）。

---

## 二、与滚轮 DND 的平行关系（架构对齐）

| | 滚轮 DND（现有，`dash_legacy_wheel_045_dnd.h`） | 档杆 DND（拟新增，`dash_legacy_stalk_045_dnd.h`） |
|---|---|---|
| 注入字段 | `StW_Sw` 左滚轮 tick（switch word bit6-9，data[4]/data[5]） | `SpdCtrlLvr_Stat` 上/下拨档位（data[0] bit0-5） |
| 副作用 | 音量 ±1（可逆、无害） | 设定速度 ±1 档（**可逆**，且是车辆自身的 NAG 清除语义） |
| 是否动 AP | 否 | 否（只要不用 FWD/RWD） |
| 触发源 | 0x399 hands-on 警告边沿 | 同左 |
| 安全栅 | counter/CRC、own-echo、物理输入 abort、TX 失败 auto-disarm、episode 预算 | 全盘复用 |
| 与 jitter 仲裁 | `procedureActive()` 期间让位 | 同左 |

两者共用同一帧 0x045，是天然平行路径，且**互相冗余**：滚轮 tick 走 data[4]/data[5]，档杆走 data[0]，字段不重叠，可独立开/关、独立诊断。

---

## 三、必须先补的实车证据（唯一的硬缺口）

**`UP_1ST/DN_1ST/UP_2ND/DN_2ND` 这四个档位的实车语义，项目里从未验证过。**

项目 jitter 程序 (`dash_jitter_procedure.h`) 只实车验证了 `FWD=0x41 / RWD=0x42`（2026-09-10 标定，14/14 取消、17/17 启动）。而「上下拨速度杆」这个手势：
1. 物理拨动时，`data[0]` 的 `SpdCtrlLvr_Stat` 具体变成哪个值（UP_1ST=0x10 还是 UP_2ND=0x04）——未知；
2. 是瞬态（拨一下回 0x40）还是电平保持——未知；
3. 合成注入该档位，车辆是否真的把它当作「清除 NAG」——未知。

**这一步不做，档杆免打扰就是空中楼阁。** 它是整个方案里唯一不能靠查代码/查库解决、必须上实车抓包回答的问题。

---

## 四、建议方案

### 阶段 0：定向实车抓包（前置，不可跳过）

1. 停 P 挡、AP 不激活、滚轮 DND 关闭（避免污染）。
2. Recorder 过滤 `45,399,3EE`，120s，先静置 15s 学 idle 基线（data[0] 应为 0x40）。
3. **只上下拨动速度杆**（智能驾驶拨杆）：上拨、下拨，各 3 次，间隔 3s，动作要快（模拟「拨一下」）。
4. 下载 CSV，差分 **data[0]** 的 bit0-5 变化。
5. **判定门槛**：
   - 若 data[0] 在拨动时从 0x40 跳到 0x50/0x60（UP_1ST=0x10 或 UP_2ND=0x04 与 0x40 的组合）或 0x48/0x28 等，且拨完回 0x40 → 锁定确切值，判定瞬态，进入阶段 2。
   - 若 data[0] 全程只有 0x40/0x41/0x42（即只有前后拨才有变化）→ 说明上下拨速度杆在 Legacy S/X 上不走 `SpdCtrlLvr_Stat`，需另找信号源（可能走 0x249，见 `docs/stalk_0x249_crc_solved_zh.md`）。

### 阶段 1：先确认滚轮 DND bug 是否已根治（低成本，并行先做）

- beta14 已把「瞬态 abort latch 致死」改为「retry cooldown 内自动 re-arm」（`dash_legacy_wheel_045_dnd.h` 419-436 行）。
- 下次实车验证「时不时还出现提醒」是否复现。**若 beta14 已根治，档杆免打扰的紧迫性下降**——它是滚轮 bug 的平行兜底，滚轮修好了就不急着上档杆。

### 阶段 2：实现平行控制器（仅当阶段 0 判定 UP/DN 档位可用）

1. 新增 `include/dash_legacy_stalk_045_dnd.h`（纯逻辑，无硬件依赖），骨架复刻 `dash_legacy_wheel_045_dnd.h`，仅换动作字段：
   - 滚轮版改 `StW_Sw` 左 tick（switch word bit6-9）；
   - 档杆版改 `SpdCtrlLvr_Stat`（data[0] bit0-5）写入 UP/DN 档位，**永不写 FWD/RWD**。
2. 注入序列建议（对称、可逆、与滚轮 DND 一致）：
   ```
   0x399 warning 边沿 → 合成 data[0]=0x40|UP_1ST(0x50) → 等 native idle(0x40) → 合成 data[0]=0x40|DN_1ST(0x60) → 等 native idle → 结束
   ```
   （上拨+1、下拨−1，设定速度归零漂移，与滚轮 DND 的 +1/−1 同构）
3. `src/main.cpp` 新增 `t2canStalkDndTick()`，复用 `t2canWheelDndTick()` 的整套 gate 链（canActive / Legacy handler / OTA / abortGuard / TWAI / jitter 仲裁）。
4. 独立 NVS 开关 `legacy_stalk_dnd`，UI 独立卡片，`/status` 独立 `stalkDndDiag`。
5. native 单测 + Python 合约测试，字段向量用阶段 0 抓包的真实值。
6. 台架 → 实车评估，验收照搬滚轮 DND 实车验收清单（`LEGACY-0X045-SCROLL-DND-IMPLEMENTATION-PLAN-20260727.md` 第 14 节），额外加一条「确认注入 UP/DN 后 AP 状态不变、设定速度归位」。

---

## 五、风险清单

| 风险 | 等级 | 缓解 |
|---|---|---|
| 误用 FWD/RWD 当免打扰动作 → 自动取消/重启 AP | **高** | 实现层硬约束：档杆控制器只写 UP/DN 档位，代码断言禁止 0x41/0x42 |
| UP/DN 档位实车语义未验证（是否真清除 NAG、是否瞬态） | **高** | 阶段 0 定向抓包 + 阶段 2 前单次人工物理确认 |
| 上下拨速度杆在 Legacy S/X 上不走 `SpdCtrlLvr_Stat` | 中 | 阶段 0 抓包证伪后转 0x249 拨杆路径 |
| 设定速度漂移（UP/DN 不对称泄漏） | 中 | 对称 +1/−1 序列，与滚轮 DND 同构，episode 预算封顶 |
| 与 jitter / 滚轮 DND 争抢 0x045 发送权 | 中 | 复刻既有仲裁，`procedureActive()` 期间让位 |

---

## 六、一句话结论

「档杆」= **速度控制杆 `SpdCtrlLvr_Stat`（0x045 data[0] bit0-5）**。档杆免打扰的正确动作是它的 **UP/DN 上下拨档位（设定速度调整，不改 AP 状态）**，而非 FWD/RWD（那会取消/启动 AP，危险）。但 **UP/DN 档位的实车语义项目从未验证过**（jitter 只验证了 FWD/RWD），所以第一优先仍是：① 定向抓包确认上下拨速度杆在 data[0] 的落点与瞬态性；② 顺带确认 beta14 是否已根治滚轮 bug。这两条清楚了，档杆平行路径即可按 `dash_legacy_wheel_045_dnd.h` 同构复刻实现。
