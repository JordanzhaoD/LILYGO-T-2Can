# 固件资产 v6.1.0-beta.08（2026-11-24）— 右滚轮按压修正 + DND 解耦

> beta.07 → beta.08：**右滚轮按压三处修正 + 免打扰机制与 FSD 总开关解耦**。

## 实车反馈的两个问题与修复

### 问题 1：右滚轮按压"部分有效、偶发失效、出现提示"

三处根因（Jordan 四份实车抓包 + opendbc DBC 交叉验证）：

1. **警告窗错位**：旧窗 `[3,6]∪[9,10]` 漏掉 hands=2（`NOT_DETECTED`，NAG 最初始、
   最常见的状态）。实车 hands 大部分时间停在 2，只短暂升到 3。→ 改为 **`[2,7]`
   全阶梯**（2=未检测到手、3=视觉、4/5=蜂鸣、7=锁 AP 硬停）。
2. **按压位错误**：beta.07 用中键按压（bit4 0x0010），正确是**左侧键**
   （bit5 0x0020，停车抓包 99 次验证）。→ 改为 **b4|=0x20**。
3. **无释放去抖**：hands 在 1↔2 间 21-26ms 抖动，"清除即停"导致 NAG 反弹。
   → 加 **500ms 释放去抖**（进入警告态即时按压，连续回到 [0,1] 满 500ms 才停）。

### 问题 2：免打扰机制被 FSD 注入总开关错误耦合

免打扰引擎（左滚轮 + 右滚轮按压）gate 链错误依赖 `canActive`（FSD 注入总开关），
导致总开关关时免打扰失效，需"先开后关"的假动作。→ gate 改为 **`canOnline`
（总线连通）**，与 FSD 注入开关解耦（符合原设计意图"wheel DND unaffected —
independent tick"）。

## 版本系列说明（必读）

6.0.0-beta03..beta21 全系作废。6.1.0 = beta02 干净基线 + TX 分路。
左滚轮免打扰 = beta02 原版（实车稳定）；右滚轮按压 = 实验路径（默认关）。

## 开关语义

| 开关 | 开（默认） | 关 |
|---|---|---|
| 0x3EE FSD 激活注入 | beta02 全量 | 激活引擎停；速度偏移走 offsetOnly |
| Legacy 左滚轮免打扰 | 独立于 FSD 开关 | 停 |
| 右滚轮按压免打扰（实验） | 独立于 FSD 开关 | 停（默认） |

## 📲 OTA 升级（板在车上）

WebUI「Release 在线更新」上传
`LILYGO-T-2CAN_v6.1.0-beta.08_ota_app.bin`，或：

```
curl -X POST "http://<板子IP>/update" \
     -H "X-File-Name: LILYGO-T-2CAN_v6.1.0-beta.08_ota_app.bin" \
     --data-binary @LILYGO-T-2CAN_v6.1.0-beta.08_ota_app.bin
```

升级后核对 `/status` 的 uiBuildId 为
`6.1.0-beta.08-lilygo_t2can_dual-228a624831b2`。

## 田野验证清单（关键闸）

1. 开关默认关：全行为=beta.02 无回归
2. **右滚轮按压单独验证**：NAG 出现（含 hands=2 初始态）即触发，
   `rightWheelPressDiag.pressed` 增长、提示消除、**无红轮无卡顿**
3. **免打扰独立性验证**：FSD 总开关关闭状态下，右滚轮按压/左滚轮 DND 照常工作
4. 并行验证（左右同开）：无 counter 冲突
5. 跑完抓 /status（wheelDndDiag + rightWheelPressDiag）

## 门禁

native 全量 **189/189 全绿**（右滚轮按压 12 例：全阶梯 [2,7]、0x20 按压位、
500ms 去抖、抖动不中断）。ESP 构建 SUCCESS。镜像自检 GREEN（0x0 魔数 0xe9）。
SHA256SUMS 五文件复核 ✓。

uiBuildId = `6.1.0-beta.08-lilygo_t2can_dual-228a624831b2`
