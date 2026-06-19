# ⛔ EPAS-Faithful Nag 注入 — 下架事故记录（禁止复辟）

**日期**: 2026-06-19
**状态**: 已物理移除
**下架提交**: `fix(epas-nag): physically remove DashEpasNagEngine injection (safety)`（分支 `fix/remove-dangerous-epas-nag`）
**下架依据 spec**: `docs/superpowers/specs/2026-06-19-remove-epas-nag-design.md`
**实现计划**: `docs/superpowers/plans/2026-06-19-remove-epas-nag.md`
**永久守卫测试**: `test/test_no_epas_nag_contract.py`

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

这是**原理性**隐患，不是调参 bug：任何向 0x370 注入篡改帧都可能让转向故障，属 **life-critical**（行车安全）级别。

## ⛔ 禁令

**禁止任何形式的"回声 / 伪造 0x370（EPAS 状态报文）注入"**——无论是固定扭矩、状态机扭矩、还是 counter+1 之外的任何篡改，都不行。0x370 是助力转向 ECU 的自身状态，向其注入假帧会触发车身控制故障。

- 永久守卫测试 `test/test_no_epas_nag_contract.py` 会阻止 `DashEpasNag` / `dashEpasNag` / `epasNagDiag` / `tryBuildEcho` / `nextDemandTorque` / `EpasNagTraceRing` / `dash_epas_nag` / `def_epnag` 这类符号重新进入编译源（扫描 `include/`、`src/`、`platformio.ini`）。
- 任何"复活 nag 抑制"的想法，必须先回答：**它是否向 0x370 注入任何帧？** 若是，停止。

## 保留项（为何不一起删）

- **老 `NagHandler` + bit-19 baseline**（`include/handlers.h`）：counter+1 轻度回声 + bit-19 清除，实测未出事，Jordan 2026-06-19 授权保留。它与本事故的"篡改扭矩 / 伪造 handsOn"引擎是**不同代码路径**，由 `nagKillerRuntime` 独立门控，有独立单测 `test_native_nag`。
- **FSD unlock / AP Gate / 防御系统 / 轮 DND**：与 0x370 注入无关，全部保留，下架后回归测试全绿。

## 教训

1. 向安全 ECU 的**自身状态报文**注入篡改帧，等于和该 ECU 对着干，随时可能让它故障。
2. 诊断 `echo > 0` 是"注入正在发生"的证据，**不是**"功能正常"的证据——本次正是 echo 打通的瞬间出事。
3. 涉及行车安全的功能，默认 OFF 不够，必须**物理删除**才能杜绝误启用（关闭只是把上膛的枪放在仪表盘上）。
4. native 单测编译的是 host 子集；**固件级 `pio run` 构建**才会编译全部代码（含 `#ifdef ESP_PLATFORM` 段）。本次一个被误删的 `#ifdef ESP_PLATFORM` 守卫就是靠固件构建门控抓到的——native 测试没暴露。两个门控缺一不可。

## 相关文档（已标注下架）

- `docs/superpowers/specs/2026-06-17-epas-faithful-nag-unified-design.md`（beta.7 引擎 spec，⛔ 已下架）
- `docs/superpowers/specs/2026-06-19-upstream-epas-nag-modec-align-design.md`（Mode-C spec，⛔ 已下架）
