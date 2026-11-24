# 固件资产 v6.1.0-beta.06（2026-11-24）— nagKill 电平型 1:1 跟随

> beta.05 → beta.06：**bit27 nagKill 从「沿型单发」改为「电平型 1:1 跟随」**。
>
> 田野判定：单帧 bit27 淹没在 10 Hz 原生 0x045 流里毫无作用——「方向盘
> 状态有效」这个声明必须**持续覆盖**（每个原生帧都跟一帧 bit27 版）才成立。
> 与滚轮 DND 的配对清除、0x3EE offsetOnly 的速度写入一样，一切成功路线都是
> **骑原生流，时序与原生同步**。
>
> **改动**：
> 1. `onDasStatus` 改维护 hands 电平（`inWindow_`），不再武装一次性 trigger；
>    hands ∈ [3,5] 持续期间，每个原生 idle 0x045 到达都跟发一帧 bit27 克隆
>    （bit27 + counter+1 + CRC8-J1850），出窗或 ≥6 立即停。
> 2. 删除 `kTriggerExpireMs` 过期栅（沿型残留，电平型由 0x399 流驱动）。
> 3. 保留全部安全栅：idle-only 骑载、own-echo 抑制、DND 互认让路
>    （beat-generational yield）、TX 两连败 disarm 自愈（800ms/60s 限 3 次）。
> 4. 诊断 `triggerArmed` → `inWindow`（/status `nagKillDiag` 同步改）。

**LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
uiBuildId = `6.1.0-beta.06-lilygo_t2can_dual-fd28afd6a4da`
（已从本目录 ota_app.bin 内嵌字符串核对）。

## 版本系列说明（必读）

**6.0.0-beta03..beta21 全系作废**（beta21 实车红轮，路线整体否决）。
6.1.0 = beta02 干净基线 + TX 分路。bit27 = 实验路径（**默认关**）。

## 开关语义

| 开关 | 开（默认） | 关 |
|---|---|---|
| 0x3EE FSD 激活注入（首页大按钮=同源） | beta02 全量 | 激活引擎停；速度偏移走 offsetOnly；DND/bit27 照常 |
| Legacy 左滚轮免打扰 | beta02 原版+自愈 | 停 |
| bit27 状态克隆免打扰（实验） | 电平型 1:1 跟随骑载 | 停（默认） |

## 📲 OTA 升级（板在车上）

WebUI「Release 在线更新」上传
`LILYGO-T-2CAN_v6.1.0-beta.06_ota_app.bin`，或：

```
curl -X POST "http://<板子IP>/update" \
     -H "X-File-Name: LILYGO-T-2CAN_v6.1.0-beta.06_ota_app.bin" \
     --data-binary @LILYGO-T-2CAN_v6.1.0-beta.06_ota_app.bin
```

升级后核对 `/status` 的 uiBuildId 为
`6.1.0-beta.06-lilygo_t2can_dual-fd28afd6a4da`。

## 田野验证清单（关键闸）

1. 开关默认开：全行为=beta02 无回归
2. 关开关+原生激活：DND 清警告；offsetOnlyTxOk 增长；0x3EE 激活形态帧消失；无红轮无卡顿
3. **bit27 单独验证（DND 关、只开 bit27）**：警告持续期间 `nagKillDiag.cloned`
   随原生流持续增长（电平型逐帧克隆）、出窗即停、**无红轮**——若有任何异常
   立即关掉并抓 /status
4. 并行验证（DND+bit27 同开）：无 counter 冲突、诊断独立增长
5. 跑完抓 /status（legacySmartSpeed + wheelDndDiag + nagKillDiag）

## 门禁

native 全量 **189/189 全绿**（含 nag_kill 12 例：持续警告逐帧克隆、出窗即停、
hands6 硬关、重入恢复、echo 不链、disarm 自愈）。ESP 构建 SUCCESS。镜像自检
GREEN（版本号 / uiBuildId / 0x0 魔数 0xe9）。SHA256SUMS 五文件复核 ✓。

设计文档：`docs/TX-SPLIT-RIDE-0X3EE-DESIGN-20261123.md`（含 bit27 风险披露：
参与过 beta21 红轮现场，根因未定案；本次为干净基线+电平型重启，默认关，
田野闸门照旧——bit27 单独验证无红轮才转正）。
