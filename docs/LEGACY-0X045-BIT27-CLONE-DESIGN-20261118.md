# Legacy 0x045 bit27 克隆免打扰 — 设计文档

> 状态：已批准（Jordan，2026-11-18）。取代已作废的档杆手势 DND（beta16，`dash_legacy_stalk_045_dnd.h` 全套删除）。
> 出处：社区固件逆向分析报告，参数经 Jordan 确认准确：**窗【2,5】、DAS>3 门、1.5s 间隔、发 13/14 针脚总线**。

## 一、为什么换路线

旧路线（滚轮 tick / 档杆 UP-DN 手势注入）是**动作模拟**：车端把每次注入当作一次性驾驶员交互，重置单次 nag 计时，但累积「只交互不出力」必然触发 Tesla strike-out 反制（handsState=7 红色方向盘 + AP 强制退出 + 5 次锁一周 Autosteer）。田野实证：beta16 行驶 ~1h/60km 出红轮。

bit27 克隆是**状态声明**：不模拟动作，而是周期性把最新原生 0x45 帧克隆一帧、置位 bit27（`StW_Cond_Flt`，data[3] bit3）重发。DAS 消费 CH 线上的 0x45 取「最新值」，bit27=1 持续生效 → hands-on 判定从源头看不到手离盘 → **nag 不产生（预防），而不是来了再清（对抗）** → 无交互累积 → 无 strike-out。

```
方向盘 BFT 原生 0x45 ─网关转发→ CH/DAS 主线（X197 13/14 = 固件 CAN1）◀ DAS
                                        ↑ 滚轮 DND 注入点（已验证 DAS 容忍 counter+1 骑流）
                                        ↑ bit27 克隆注入点（本方案，1.5s 一帧）
```

## 二、参数（照抄参考固件，不自行调整）

| 参数 | 值 | 语义 |
|---|---|---|
| 窗 | handsOnState ∈ {2,3,4,5} | 2=REQD_NOT_DETECTED（警告前夜）即介入；**上限 5=CHIME_2，到 6 立即停手——永不与 strike-out 域对抗（本方案的安全灵魂）** |
| DAS 门 | `DAS_autopilotState > 3` | AP 激活后才动作（照抄参考参数，>3 即 4..6；常量化便于田野调整） |
| 间隔 | 1500 ms | 克隆帧周期 |
| 注入线 | CAN1（X197 13/14） | 与滚轮 DND 同线，counter+1 / CRC8-J1850 / own-echo 纪律全套复用 |
| bit27 | `data[3] \|= 0x08` | 克隆时置位，其余字节与最新原生帧一致 |

## 三、模块设计

新文件 `include/dash_legacy_045_bit27_clone.h`（纯逻辑，无硬件依赖）：

- **状态机**：Disabled → Blocked（gate 外因：OTA/jitter/abortGuard/TWAI/context stale）→ WaitingWindow（AP 未激活或 handsOnState 窗外）→ Ready（窗内+门开）→ ClonePending（帧已生成待 TX）→ 回 Ready（1.5s 冷却）；TX 失败连续 2 次 → AutoDisarmed（重开开关恢复）。
- **安全栅**（复刻滚轮 DND 工程纪律）：nativeFresh 150ms、counter 续接（基于最新原生帧）、own-echo 30ms 指纹丢弃、**物理输入跳过**（原生帧滚轮位非 idle 时不克隆——不复制/放大人手动作，回 idle 恢复）、TX 失败 auto-disarm、abort guard 独立路径 `Legacy045Bit27Clone`。
- **诊断**（/status `clone27Diag`）：cloneTx、cloneTxFailed、windowEntries、gateBlocks、ownEchoRx、physicalSkip、lastCloneMs、native/clone counter、autoDisarmed、state/blocked。

接线：`t2can045Clone27Tick()`（main.cpp，gate 链同滚轮）；0x45/0x399 帧派发喂新模块；NVS 键 `def_c27`；/defense_config 字段 `clone27_dnd`；UI 沿用原档杆开关位换新开关「Legacy 状态克隆免打扰」。

## 四、作废清单

- 删除：`include/dash_legacy_stalk_045_dnd.h`、`test/test_native_legacy_stalk_045_dnd/`、platformio.ini 的 `native_legacy_stalk_045_dnd` env、main.cpp `t2canStalkDndTick()`、dashboard.h 的 stalk 派发/JSON/`stalk_dnd` 字段/NVS `def_st`、UI 档杆开关。
- 保留：`scripts/analyze_legacy_stalk_045_capture.py`（sess_137 标定工具，与本功能无关）。
- abort guard 枚举 `LegacyStalk045Dnd` → `Legacy045Bit27Clone`。
- Breaking：旧 NVS `def_st` 配置不迁移（开关默认 OFF，重新打开一次即可）。

## 五、验证路径

1. native 套件 pin：窗边界（1/2/5/6）、DAS 门（3/4）、1.5s 周期、克隆编码（只动 bit27+counter+CRC）、物理输入跳过、own-echo、TX 失败 disarm、counter 回绕、context stale。
2. 门禁：native 全绿 → ESP 构建（wrapper）。
3. 台架：回放 sess_137 的 0x45 流验证克隆帧编码与 counter 链。
4. 实车两阶段：①AP 激活短途——仪表无新告警/DTC，handsOnState 被压在 2 以下不进窗 ②1h+ 长途——nag 不出现、无 strike-out、clone27Diag.cloneTx 持续增长。
5. 风险观察点：若 DAS 对 counter 重号不容忍（仪表 fault/0x45 丢帧告警），立即关开关并回报数据。
