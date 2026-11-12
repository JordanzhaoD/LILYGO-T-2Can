# Ban Shield X — Stage A / Stage B 操作手册（2026-11-12）

> 固件 `6.0.0-beta15`。设计文档：`Atlas-3CAN-P4/docs/ban-shield-0x368-forge-experiment-design-20261111.md`（v3，§6 台架 / §7 实车）。
> 本文只写"怎么做"；判据与安全边界以设计文档为准。

## 0. 前置（两阶段通用）

- 板 2（192.168.1.145）已 OTA 到 beta15，`/status` 里 `uiBuildId` 以 `6.0.0-beta15-p4_3can-` 开头（环境前缀必核——防 S3 镜像伪装，见记忆 p4 镜像坑）。
- FSD 防护页应出现 **防封禁 Ban Shield**（实验徽标）+ 实验臂下拉 + 四个状态芯片（开关状态 / 当前臂 / 已注入 / tripwire 命中）。
- `banForgeDiag` 字段（`/status`）：`pref / armed / arm / injected / armsRun / tripwireHits / lastDeviantMs`。
- 打开页面即生效的 tripwire 是常开的（pref=false 也计数）——Stage A 开场就该看到它安静（0x368 恒=模板）。

## 1. Stage A — 台架（不接车）

### 台架拓扑

第二块板 / CAN 分析仪做模拟源，向 **CanC（PT 总线）** 发 id=0x368(872) DLC=8 模板
`51 31 59 0F 88 49 67 00`，周期 ~100ms 加随机抖动（模拟网关 ~10Hz）。其余总线保持安静；
若板挂在台架 3 总线卡具上，CanA/CanB 无源也可。

### A1/A2 — 变换正确性（已在 native 门禁覆盖，此处只做抽看）

native 套件（`test_ban_forge_contract.py` → clang++ native main）已逐臂钉死七种变换的期望字节。
台架只抽看 B1 输出帧字节：`…49 47 00`（b6 bits7:5 由 3→2）。逐字节差异只允许目标位窗口。

### A3 — 回写机制与预算

1. FSD 防护页：开 防封禁 Ban Shield，实验臂选 **B1**，等待保存（`[BANFORGE]` dashLog 应有 pref/arm 事件）。
2. 模拟源持续发模板。预期 ≤5ms 内板发出 3 帧变换帧（预算=3 按发出尝试数），随后**自动解除武装**：
   - `/status`：`banForgeDiag.injected==3`、`armed==false`、`armsRun==1`；
   - dashLog：`[BANFORGE] arm complete (injected=3, arm=1)`；
   - TWAI2 回 LISTEN_ONLY（自动换挡保持关闭即可确认 mode-dirty 回退生效）。
3. v1 记录器导出核对：**T 行 id=872、bus=C**（beta15 修复的 PT 总线标记；若仍显示 bus=A 即回归，停）；
   E 行应为 0。
4. 回显静默：板自身发出的变换帧会被 CanC 嗅到（载荷≠模板），但 armed 窗 + 末次成功后 2s 内
   tripwire 必须静默——`tripwireHits` 保持 0。armed 结束 2s 后若模拟源仍在发模板（=模板），仍不计数。

### A4 — 安全闸（kill test）

1. 等 30s 冷却过掉，重新武装 B2（确认 `[BANFORGE]` arm 事件）。
2. 注入进行中（模拟源在发）**立刻关闭** 防封禁 Ban Shield 开关。
3. 预期：注入立即停止（不再有新 T 行 872），`armed==false`；因已有 attempts，进入 30s 冷却
   （冷却期内再选臂，dashLog 出现 rejected）。

### Stage A 通过标准

A3 三条全中 + A4 立停 + T/E 行总线标记正确。任一不中 → 停在台架，不进 Stage B。

## 2. Stage B — 实车（停车场静止 P 挡，不行驶）

### 2.0 板特性钉死（Jordan 定案 2026-11-12）

实验场次前，把三项板特性临时 OFF（全部 NVS 可逆，记下原值）：

| 特性 | 原值（先记录！） | 实验值 |
|---|---|---|
| alignMode（0x3EE 对齐模式） | （记录） | OFF |
| Legacy 激活（FSD 注入） | （记录） | OFF |
| 0x3EE keepalive | （记录） | OFF |

目的：观测通道归因唯一化——0x3EE 任何页变化都只能来自注入或车端。
**实验后按记录恢复**（alignMode 恢复与否见当场判定，默认恢复）。

### 2.1 干净基线

钉死三项后、打任何臂之前：会话录制器录一段静止基线（≥60s，三总线），
导出后核对 0x3EE 与 sess_120 静止基线同形（页0 两变体 / 页1 恒 `01 00 02 00 00 00 05 80`）。
手机录屏同步开着。

**会话录制器操作（sess_120 同款，CAN 诊断页）**：
1. `黑盒串流` 开关 **ON**（武装环形缓冲——注意：这是捕获的前置条件，
   `/ring_lock` 在未武装时直接拒绝 "stream not armed"）；
2. 实验进行（环内保留最近 N 秒三总线全量，PSRAM 20MB）；
3. 时机到 → 选回溯窗（60/180/300s）+ 三总线 → `一键锁定`
   （封存回溯段 + 续录 30s → 自动生成会话文件）；
4. `录制会话` 列表 → `下载BIN` / `下载CSV`（sess_NNN.bin/csv）；
5. **收尾必做：`黑盒串流` 开关 OFF**（beta13 红线是"平时常开"，
   取证期间短开是设计用途；武装期间速度显示可能闪烁，静止实验无碍）。

### 2.2 臂序列

每臂 = 开关已在 ON、实验臂下拉选 B n → 板收到下一帧网关真帧时注入变换帧，至预算 3 帧自动停；
臂间 ≥30s 观察期（冷却本身 30s，用满）。顺序 B1→B2→B3→B4→B5；**B6/B7 条件兜底臂仅在
B1-B5 全无反应且 Jordan 当场批准后打**。

观察窗（每臂注入后 10s）按判别力看：

1. CanA 0x3EE 回读：mux 页任何字节变化 vs 基线（最硬判据，会话导出后比对）；
2. 屏幕：FSD 徽标 / 菜单项 / Autopilot 设置页（录屏对照）；
3. 网关 0x368 后续帧有无变化 / 停发（弱证据，记录备查）；
4. `banForgeDiag`：injected==3、armed 归 false、tripwireHits 变化（**若 tripwire 命中且时间窗
   不是自家回显 → 网关载荷变了，本身就是重大信号，记录并进入下一臂**）。

### 2.3 立即终止条件（任一触发 → 开关 OFF + 停场）

- 任何告警 / 弹窗 / 车机非预期行为；
- 挡位指示 / 驾驶 UI 异常；
- E 行（发送失败）非零且持续；
- 0x3EE 出现基线外变化**且**当前无臂在注（归因不清即停）；
- 任何拿不准的情况——终止永远便宜过继续。

### 2.4 收尾

1. 防封禁 Ban Shield 开关 OFF（pref 归 false，armed 已由自动停机清掉）；
2. 恢复 2.0 三项板特性到记录值；
3. 会话导出 + 录屏归档到证据目录（含 `uiBuildId` 截图）；
4. 对照设计文档 §8 判决矩阵出结论。

## 3. 快速参考

- 每臂注入上限：3 帧（硬编码）或自首帧注入 600ms，先到先停；
- 臂间冷却：30s（显式解除若已有 attempts 同样进冷却）；
- 变换方向恒为降档 3→2（ENHANCED，无害方向）；
- B6 = B1 + b7 加法校验；B7 = b6 整字节 0x00 探针；
- 重启后 armed 恒 false（fail-closed），pref 保持——开场上电后仍需手动重新选臂。
