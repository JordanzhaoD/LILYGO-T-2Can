# Legacy 0x370 NAG — 历史记录与当前结论

**日期**: 2026-06-19
**状态**: 历史问题已关闭；Legacy Reactive NAG v2 已获准恢复
**下架提交**: `fix(epas-nag): physically remove DashEpasNagEngine injection (safety)`（分支 `fix/remove-dangerous-epas-nag`）
**下架依据 spec**: `docs/superpowers/specs/2026-06-19-remove-epas-nag-design.md`
**实现计划**: `docs/superpowers/plans/2026-06-19-remove-epas-nag.md`
**永久守卫测试**: `test/test_no_epas_nag_contract.py`

## 当前结论（2026-07-23）

用户已确认目标车辆上的 Legacy `0x370` NAG 实现安全，原“禁止恢复”告警自本日期起撤销。当前实现采用独立 Legacy 子功能、默认关闭、真实 `0x370` 基帧、同通道回声、Legacy AP 状态 3..6、数据新鲜度门控、幅度上限和发送诊断。实车抓包证明目标车辆的 `0x118` 挡位位域与旧定义不兼容，经用户确认后 Legacy NAG 不再使用 D 挡门；其他模块的挡位安全门保持不变。`test/test_no_epas_nag_contract.py` 已由负向符号禁令改为正向运行契约。

下文保留为旧动态 Mode-C 实现的历史问题记录，不代表对当前 Reactive NAG v2 的有效告警。

---

## 事故

第 5 轮 apActive 门控修复（commit `2f75109`）打通 `DashEpasNagEngine` 的 0x370 注入路径后，固件烧录上车验收。**实车启用 EPAS nag 即出现严重安全事故**：

- **车身控制系统被禁用、车辆异常、严重故障提示**
- 现象持续，**直到拔掉模块、重启车机才恢复正常**
- 诊断页 `/status.epasNagDiag` 实测 `echo > 0`、`demand.gate` 在 `s1_hold / s2_mild / strong_*` 切换——即故障发生时，模块正在**主动向总线注入被篡改的 0x370 帧**

## 根因

`0x370` 是 **EPAS（电子助力转向）控制器自身的状态报文**（`EPAS3P_sysStatus`，~100 Hz）。`DashEpasNagEngine` 做的事是：把真实的 0x370 帧**回声发回总线**，内容被改写成

- `counter + 1`（伪装成序列里的下一帧）
- 一个由 demand-state 状态机算出的**合成扭矩**（1.8 Nm / 缓和游走 / 0→2.1 Nm 爬升）
- 一个**从扭矩幅值反推的 handsOnLevel**（即使驾驶员手未在盘上也声称"手在"）

这本质上是**冒充助力转向 ECU，广播"检测到手在盘上且有 XX 扭矩"的假状态**。EPAS 交叉校验后发现：

1. **counter 序列冲突**——真 ECU 与模块同时发 0x370，序号错乱 → 通信故障；
2. **扭矩语义矛盾**——声称扭矩与真实驾驶员输入对不上 → EPAS 判定不一致 → 助力转向 / 车身控制进入故障保护。

第 5 轮 apActive 修复让 `echo` 从 0 变 >0，**正是这些假帧开始上总线的瞬间，也正是故障出现的瞬间**——两件事是同一件事。**拔模块 → 假帧消失 → 重启车机 → 恢复**，因果链完整。

这是 **life-critical**（行车安全）级别隐患。根因是 **demand-state 动态扭矩状态机**（0→2.1 Nm 爬升 + 从扭矩幅值反推 handsOn）与真实驾驶员输入产生语义矛盾，触发 EPAS 故障保护。需要区分两点：**(a) 动态/爬升/与真实输入矛盾的注入方式是危险的**（本次事故证明）；**(b) 0x370 注入本身存在原理性风险**（counter 冲突、与真 ECU 竞争同一个 ID），即使固定温和方式也不能视为普适安全——"某台车没出事"不等于"所有车都安全"（车型/EPAS 固件差异）。

## 历史限制（2026-07-23 已解除）

以下内容是旧实现下架时采用的限制，现已由 2026-07-23 的确认结论取代。它只用于解释为何旧动态 Mode-C 状态机曾被移除。

- 守卫测试 `test/test_no_epas_nag_contract.py` 现验证当前 Legacy Reactive NAG 的默认关闭、门控、同通道路由、幅度限制和 API/NVS/UI 契约。
- 任何"复活 nag 抑制"的想法，必须先回答两个问题：
  1. **它是否向 0x370 注入任何帧？** 若否，不受本禁令约束（如 bit-19 baseline 走 `0x3EE`/`0x3FD`/`1021`，安全）。
  2. **若是，是否经过台架验证（车辆不上路）证明该注入方式不触发 EPAS 故障？** 未经验证 → 停止。验证通过且经 Jordan 书面授权，方可解禁该**特定方式**。
- **台架验证前提**（未来若探索 0x370 nag 必须全部满足）：(1) 台架环境，车辆不上路行驶；(2) 明确的安全方式定义（扭矩曲线 / handsOn / counter 策略）；(3) 严格门控（默认关、AP 激活才发、一键关停）；(4) 异常立即停止 + 回滚。

## 保留项（为何不一起删）

- **老 `NagHandler`**（`include/handlers.h:705-828`）——⚠️ **2026-06-20 订正**：原文档将其描述为"counter+1 轻度回声 + bit-19 清除"，**与代码不符**。实际代码显示 NagHandler **仍向 0x370 注入篡改帧**：固定扭矩 `data[3]=0xB6`（1.80 Nm）+ `data[4]|=0x40`（handsOnLevel=1）+ counter+1 + 重算校验和 + `driver.send()`。它和已下架的 `DashEpasNagEngine` **都是 0x370 注入**，区别仅在算法（固定值 vs 动态状态机）。NagHandler 在 Model Y 2022 HW3 实测未出事（`handlers.h:700` 注释），但**这不构成普适安全保证**（车型/EPAS 固件差异 + counter 冲突原理性风险）。**NagHandler 的 0x370 注入待清理（backlog），未清理前属已知 life-critical 安全债。** 由 `nagKillerRuntime` 独立门控，有独立单测 `test_native_nag`。
- **bit-19 baseline**（`0x3EE`/`0x3FD`/`1021` mux-1 `setBit(19,false)`）：**非 0x370 路径**，不在本禁令范围，安全保留。
- **FSD unlock / AP Gate / 防御系统 / 轮 DND**：与 0x370 注入无关，全部保留，下架后回归测试全绿。

## 教训

1. 向安全 ECU 的**自身状态报文**注入篡改帧，等于和该 ECU 对着干，随时可能让它故障。
2. 诊断 `echo > 0` 是"注入正在发生"的证据，**不是**"功能正常"的证据——本次正是 echo 打通的瞬间出事。
3. 涉及行车安全的功能，默认 OFF 不够，必须**物理删除**才能杜绝误启用（关闭只是把上膛的枪放在仪表盘上）。
4. native 单测编译的是 host 子集；**固件级 `pio run` 构建**才会编译全部代码（含 `#ifdef ESP_PLATFORM` 段）。本次一个被误删的 `#ifdef ESP_PLATFORM` 守卫就是靠固件构建门控抓到的——native 测试没暴露。两个门控缺一不可。

## 相关文档（已标注下架）

- `docs/superpowers/specs/2026-06-17-epas-faithful-nag-unified-design.md`（beta.7 引擎 spec，⛔ 已下架）
- `docs/superpowers/specs/2026-06-19-upstream-epas-nag-modec-align-design.md`（Mode-C spec，⛔ 已下架）
