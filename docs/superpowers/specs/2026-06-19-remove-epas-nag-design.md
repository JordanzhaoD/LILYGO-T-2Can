# 移除 EPAS-Faithful Nag 注入引擎（安全下架）— 设计规格

- **日期**: 2026-06-19
- **项目**: LILYGO-T-2Can-firmware（微雪 ESP32-S3 单 CAN 独立固件）
- **状态**: 设计已确认（Jordan 2026-06-19），待实现计划
- **作者**: Claude（为 Jordan 起草）
- **被取代的 spec**: `2026-06-19-upstream-epas-nag-modec-align-design.md`、`2026-06-17-epas-faithful-nag-unified-design.md`
- **关联事故**: `docs/EPAS-NAG-REMOVAL-INCIDENT.md`（实现期生成，禁令记录）

---

## 1. 背景与根因（为什么要下架）

### 1.1 事故经过

`DashEpasNagEngine`（`include/dash_epas_nag.h`）历经 5 轮实车调试。第 5 轮（commit `2f75109`，apActive 门控修复）打通了注入路径，固件已烧录上车验收。**实车验收时出现严重安全事故**：

- 启用 EPAS nag 后，**车身控制系统被禁用、车辆异常、严重故障提示**
- 现象持续，**直到拔掉模块、重启车机才恢复**
- 诊断页 `/status.epasNagDiag` 实测 `echo > 0`、`demand.gate` 在 `s1_hold / s2_mild / strong_*` 切换——即故障发生时，模块正在**主动向总线注入被篡改的 0x370 帧**

### 1.2 根因（为什么必然出事）

`0x370` 是 **EPAS（电子助力转向）控制器自己的状态报文**。`DashEpasNagEngine` 做的事是：把真实的 0x370 帧**回声发回总线**（每秒 ~100 次），内容被改写成

- `counter + 1`（伪装成序列里的下一帧）
- 一个由 demand-state 状态机算出的**合成扭矩**（1.8Nm / 缓和游走 / 0→2.1Nm 爬升）
- 一个**从扭矩幅值反推的 handsOnLevel**（即使驾驶员手未在盘上也声称"手在"）

这本质上是**冒充助力转向 ECU，广播"检测到手在盘上且有 XX 扭矩"的假状态**。EPAS 交叉校验后发现：

1. **counter 序列冲突**——真 ECU 与模块同时发 0x370，序号错乱 → 通信故障
2. **扭矩语义矛盾**——声称扭矩与真实驾驶员输入对不上 → EPAS 判定不一致 → 助力转向/车身控制进入故障保护

第 5 轮 apActive 修复让 `echo` 从 0 变 >0，**正是这些假帧开始上总线的瞬间，也正是故障出现的瞬间**——两件事是同一件事。**拔模块→假帧消失→重启车机→恢复**，因果链完整。

### 1.3 结论

这个思路在**原理上就是和助力转向 ECU 对着干**，属于行车安全（life-critical）级隐患。即使某次调参"看起来 nag 消失了"，也随时可能在别的工况让转向故障。**Jordan 决定：物理删除该功能，不是关闭。** 关闭只是把上膛的枪放在仪表盘上，误触即重演事故。

---

## 2. 目标与非目标

### 2.1 目标
1. **物理删除** `DashEpasNagEngine` 全部代码、测试、UI、诊断观测层——让危险注入路径从代码层面消失，无法被任何开关/误操作重新打开。
2. **还原耦合**：把老 bit-19 baseline 与新引擎的"二选一"耦合还原为无条件执行，精确恢复到新引擎加入**之前**的老 baseline 行为。
3. **保留所有在用功能**：老 `NagHandler`、bit-19 baseline、FSD unlock / AP Gate、防御总开关、轮 DND、`APActive` 等全部不动、继续工作。
4. **产出可烧录的安全固件**：构建 → 重烧 → smoke 验证 `epas_nag` 符号彻底消失。
5. **留下禁令记录**：把活跃 spec 转成事故/禁令文档，防止以后有人复辟这套"0x370 回声注入"思路。

### 2.2 非目标（YAGNI / 已决策）
- ❌ **不删老 `NagHandler`、不动 bit-19 baseline**（Jordan 2026-06-19 决策：只下架出事的新引擎，保留一直在用、未出事的轻度老路径）。
- ❌ **不动 handler 过滤器数组**（0x370/0x39B 留着给老 NagHandler 用；多留 filter ID 无害，删它有打断老路径的风险、零安全收益）。
- ❌ **不碰 `isDASAutopilotActive` status=6 潜伏 bug**（独立问题，影响面大，与本次下架无关，留 backlog）。
- ❌ **不改 FSD injection gate / 防御系统任何逻辑**。
- ❌ **不 push 任何 remote**（origin/main 是 hqs 不相关历史线，既有约束）。

---

## 3. 改动总览

| 类别 | 动作 | 说明 |
|---|---|---|
| 🗑️ 整文件删除 | 5 类 | 新引擎专属，无共享牵连 |
| ✂️ 外科摘除 | 2 个大文件 | 从宿主函数里摘 nag 行，宿主保留 |
| ↩️ 还原耦合 | 4 处 | 恢复新引擎加入前的老 baseline |
| 🛡️ 绝对不碰 | — | 老路径 + 共享依赖（见 §3.5） |

### 3.1 🗑️ 整文件/整目录删除
- `include/dash_epas_nag.h`（引擎本体，353 行）
- `include/dash_epas_nag_trace.h`（诊断录制 ring，98 行）
- `test/test_native_epas_nag/`（整个目录，46 个 Unity 单测）
- `test/test_epas_nag_config_contract.py`
- `test/test_epas_nag_filters_contract.py`
- `test/test_epas_nag_trace_contract.py`
- `test/test_epas_nag_trace_persistence_contract.py`
- `test/test_epas_nag_wiring_contract.py`
- `platformio.ini` 的 `[env:native_epas_nag]` 整块

### 3.2 ✂️ `include/web/mcp2515_dashboard.h`（7213 行，摘 nag 行保宿主）
- 删 include：`#include "dash_epas_nag.h"`、`#include "dash_epas_nag_trace.h"`（保留 `dash_tx_evidence.h`——轮 DND 共用）
- 删全局：`dashEpasNagEnabled`、`dashEpasNagCtrl` 实例、~15 个 trace 全局 + fwd-decl
- 删函数：`dashEpasNagOnFrame`、`dashEpasNagSnapshotTec`、`epasNagGateFromName`、`dashEpasNagTraceSample/Stop/Flush/Poll`、`handleEpasNagTraceDownload/Start/Stop`、`dashSerialPrintEpasNagStatus`、`dashSerialPrintEpasNagTrace`
- 删单行接线：`mcpDashOnFrame` 里的 `dashEpasNagOnFrame(f);`、`dashRecordApRestoreFrame` 0x129 块里的 `dashEpasNagCtrl.onSteeringAngle(...)`、防御 NVS load/save 里的 `def_epnag` 行、防御 POST 解析里的 `epas_nag` 操作数、loop 里的 `dashEpasNagTracePoll();`
- 删 JSON：`/status` 的 `"epasNag"` + `epasNagDiag` 对象、防御配置 JSON 的 `"epas_nag"`
- 删串口：`epas_nag` / `epas_nag_trace` 命令分派 + help 文本
- 删路由：`server.on("/epas_nag_trace"...)`
- 删赋值行：`dashApplyRuntimeState` 里的 `dashHandler->epasNagActive = ...`（见 §3.3）

### 3.3 ✂️ `include/web/mcp2515_dashboard_ui.src.h` + `.h`（前端，6 处 nag 块）
改 `.src.h` 后用 `scripts/minify_dashboard.py` 重新生成 `.h`。6 处：
1. 防御页 EPAS nag **开关行**（`def-epnag-tgl`，标签"EPAS 方向盘 nag [实验]"）
2. 状态**三联行**（`def-epas-nag-row` + `def-epas-echo/tx/tq`）
3. JS 渲染 `d.epasNagDiag` 块
4. JS 开关回显 `epnag.checked = !!d.epas_nag`
5. JS 防御摘要里的 `.epas_nag)`
6. JS POST 提交字段 `epas_nag:...`

> ⚠️ 构建会 stamp build-id 到 `mcp2515_dashboard_ui.h`，构建后必须 `git restore include/web/mcp2515_dashboard_ui.h`——**永远不要 commit 对它的改动**（既有约束）。

### 3.4 ↩️ 还原耦合（恢复老 baseline，已核实代码）
新引擎与老 bit-19 是"二选一"：新引擎开→让位、新引擎关→走老 bit-19。删新引擎后还原为**无条件**：
- `include/handlers.h:605`（HW3 mux-1）：条件去掉 `&& !(bool)epasNagActive` → mux-1 无条件转发 FSD-ready + 清 bit-19
- `include/handlers.h:956`（HW4 mux-1）：去掉 `if (!(bool)epasNagActive)` 包裹 → `setBit(frame, 19, false);` 无条件执行（bit-47 FSD-ready 本就无条件，不动）
- `include/handlers.h:73`：删 `Shared<bool> epasNagActive{false};` 字段
- `include/web/mcp2515_dashboard.h`：删 `dashHandler->epasNagActive = ...` 赋值行

> 还原后，0x370 处理回到"新引擎加入前"：老 NagHandler（counter+1 回声）+ 无条件 bit-19 清除。**危险假帧注入路径从代码层面不存在。**

### 3.5 🛡️ 绝对不碰（共享/保留）
- 老 `NagHandler`（`handlers.h:706-830`）+ 其单测 `test_native_nag/`
- `nagKillerRuntime` / `nagKillerActive`（NagHandler 门控）
- HW3/HW4 mux-1 的 `setBit(frame,19,false)` **本身**（只去掉包裹它的 `!epasNagActive`，保留清除动作）
- HW4 `setBit(frame,47,true)`（FSD-ready，FSD unlock 用）
- `CarManagerBase::APActive`、`isDASAutopilotActive` / `readDASAutopilotStatus`、`injectionAllowed` / `builtInFsdInjectionAllowed`、0x3EE FSD 激活块
- `dashDefenseEnabled`（防御总开关）、`canActive`、`dashDriver` / `dashHandler`
- `dash_tx_evidence.h`（轮 DND + main.cpp 共用）、`shared_types.h`
- handler 过滤器 `ids[]` 数组（含 880/923，不动；因此 `test_native_hw4/test_hw4_handler.cpp` 的 880/923 断言**无需改动**）

---

## 4. 数据流变化

**清理后**（0x370 处理回到新引擎加入前）：
```
0x370 @100Hz → 老 NagHandler（counter+1 回声，轻度，默认开，一直在用）   ✅ 保留
0x3EE mux-1  → FSD-ready 转发 + 无条件清 bit-19（老 baseline）            ✅ 保留
❌ 不再有"篡改扭矩 + 伪造 handsOn"的假 0x370 帧（出事根因，已物理移除）
```

---

## 5. 验证策略（安全第一）

### 5.1 测试
```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware
pio test -e native_nag                  # 老 NagHandler 仍全绿（证明没误伤）
pio test -e native_hw3 -e native_hw4 -e native_legacy \
         -e native_dashboard -e native_helpers -e native_injection_after_ap  # FSD/防御全绿
```
> `native_epas_nag` env 已删，`test_epas_nag_*.py` 已删，不再跑。

### 5.2 构建
```bash
rm -rf .pio/build/waveshare_single_can_standalone   # 清版本串缓存
pio run -e waveshare_single_can_standalone          # 期望 SUCCESS，Flash 占用下降
git restore include/web/mcp2515_dashboard_ui.h      # 去 build-id stamp（必做）
```

### 5.3 危险代码消失铁证
```bash
grep -rin "epasNag\|DashEpasNag\|epas_nag\|epasNagDiag\|nextDemandTorque\|tryBuildEcho" \
  include/ src/ test/ platformio.ini
# 期望：零活代码命中（仅 CHANGELOG.md / docs/ 历史文字记录可残留）
```

### 5.4 烧录 + smoke
```bash
PORT=/dev/cu.usbmodem21101   # 烧录前 ls /dev/cu.usbmodem* 重新确认
esptool.py --chip esp32s3 --port $PORT --baud 921600 flash_id   # 确认 MAC 28:84:85:49:93:fc
esptool.py --chip esp32s3 --port $PORT --baud 921600 erase_flash
esptool.py --chip esp32s3 --port $PORT --baud 921600 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 firmware-assets/waveshare-single-can-standalone_<date>-nag-removed/waveshare-single-can-standalone_full_merged_<date>-nag-removed.bin
# smoke（pyserial 显式 DTR/RTS 抓启动日志）：期望无 panic、**无 `epas_nag enabled=...` 日志行**（功能已消失的铁证）
```

### 5.5 验收标准（全部满足才算完成）
1. `grep` 活代码零命中（§5.3）
2. 老 NagHandler + FSD/防御单测全绿（§5.1）
3. 构建 SUCCESS（§5.2）
4. 烧录 Hash verified + smoke 干净启动 + **启动日志无 `epas_nag`**（§5.4）
5. Dashboard 防御页**不再有** "EPAS nag" 开关（UI 已删）
6. `/status` JSON **不再有** `epasNag` / `epasNagDiag` 键

---

## 6. 分支、提交与交付

- **分支**：`fix/remove-dangerous-epas-nag`（已基于 `feature/ui-v11-redesign@2f75109` 创建，本地保留，**不 push**）
- **提交粒度**：建议分 3 个 commit 便于审查/回滚
  1. `fix: remove DashEpasNagEngine injection (safety)` —— 删引擎 + trace + 还原 handlers/dashboard 耦合 + UI
  2. `test: drop epas_nag test suite and contracts` —— 删测试 + platformio env
  3. `docs: replace epas-nag specs with removal incident record` —— 生成 `EPAS-NAG-REMOVAL-INCIDENT.md`，prune handoff nag 章节
- **固件资产**：`firmware-assets/waveshare-single-can-standalone_<date>-nag-removed/`，**bin 不 commit**（仓库约定）
- **回滚点**：每个 commit 后跑 §5.1 测试；任一步红即停下定位，不累积错误

---

## 7. 安全禁令（写入事故记录，防止复辟）

实现期生成 `docs/EPAS-NAG-REMOVAL-INCIDENT.md`，记录：
1. 事故经过 + 根因（本文 §1）
2. **禁令**：**禁止任何形式的"回声/伪造 0x370（EPAS 状态报文）注入"**——无论是固定扭矩、状态机扭矩、还是仅 counter+1 回声之外的任何篡改。0x370 是助力转向 ECU 自身状态，向其注入假帧会触发车身控制故障，life-critical。
3. 老 NagHandler / bit-19 baseline 为何**单独保留**（counter+1 轻度回声，实测未出事，Jordan 授权保留）。
4. 指向本 spec 作为下架依据。

---

## 8. 风险与对策

| 风险 | 对策 |
|---|---|
| 还原 bit-19 耦合时手抖改错条件 | 改后立即跑 `native_hw3`/`native_hw4`/`native_nag`，全绿才继续 |
| 漏删某处 nag 引用导致编译错 | grep §5.3 + 构建必过；编译错即定位补删 |
| minify 后 `.h` build-id stamp 误 commit | 构建后 `git restore mcp2515_dashboard_ui.h`；提交前 `git status` 复核 |
| 烧录用 `pio run -t upload` 会砖 | 只用 `esptool.py write_flash 0x0 full_merged.bin`（既有约束） |
| 误 push origin/main | 不 push；本地分支保留 |
