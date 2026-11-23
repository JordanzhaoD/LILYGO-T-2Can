# 固件资产 v6.1.0-beta.05（2026-11-23）— TX 分路 v2 全骑载架构

> beta.04 → beta.05：**按 9 段实车录制数据重设计的 TX 分路**。
> 1. **速度偏移失效修复**：0x2F8 骑载在 X197 单线物理不可能（1080s
>    录制 0 宿主帧，移植自三总线 P4 未验证宿主存在性）→ 删除；
>    开关关时 mux0 速度改走 beta02 既有 **offsetOnly 纯速度通道**
>    （田野验证写入器，字节级与开关开时输出一致，测试钉扎）。
> 2. **滚轮 DND 事件内自愈**：TX 两连败 disarm 后 800ms 自动复活
>    （60s 滑窗限 3 次）——beta.04 段③「死 14 秒」的病根。
> 3. **bit27 nagKill 并行免打扰路径（实验，默认关）**：警告沿单帧
>    骑载原生 0x045（bit27+counter+1+CRC），hands [3,5] 窗、6+ 硬关；
>    与滚轮 DND 互认让路。参数照参考伪代码，比已作废 beta17-21 更
>    保守（单发 vs 1500ms 连发、窗 [3,5] vs [2,5]）。
> 4. **开关显示不同步修复**：/status ci = 纯 fsd_3ee_inject 单一
>    真源（去 canActive 耦合）。

**LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
uiBuildId = `6.1.0-beta.05-lilygo_t2can_dual-6492135e3b1e`
（已从本目录 ota_app.bin 内嵌字符串核对）。

## 版本系列说明（必读）

**6.0.0-beta03..beta21 全系作废**（beta21 实车红轮，路线整体否决）。
6.1.0 = beta02 干净基线 + TX 分路。滚轮免打扰引擎 = beta02 原版
+ 自愈；bit27 = 新增实验路径（**默认关**）。

## 开关语义（v2 终态）

| 开关 | 开（默认） | 关 |
|---|---|---|
| 0x3EE FSD 激活注入（首页大按钮=同源） | beta02 全量 | 激活引擎停（mux1 零发）；**速度偏移照走 offsetOnly**；DND/bit27 照常 |
| Legacy 左滚轮免打扰 | beta02 原版+自愈 | 停 |
| bit27 状态克隆免打扰（实验） | 警告沿单帧骑载 | 停（默认） |

## 📲 OTA 升级（板在车上）

WebUI「Release 在线更新」上传
`LILYGO-T-2CAN_v6.1.0-beta.05_ota_app.bin`，或：

```
curl -X POST "http://<板子IP>/update" \
     -H "X-File-Name: LILYGO-T-2CAN_v6.1.0-beta.05_ota_app.bin" \
     --data-binary @LILYGO-T-2CAN_v6.1.0-beta.05_ota_app.bin
```

升级后核对 `/status` 的 uiBuildId 为
`6.1.0-beta.05-lilygo_t2can_dual-6492135e3b1e`。

## 田野验证清单

1. 开关默认开：全行为=beta02 无回归
2. 关开关+原生激活：DND 清警告；**offsetOnlyTxOk 增长**；0x3EE
   激活形态帧（burst）消失；无红轮无卡顿
3. bit27 单独验证（DND 关、只开 bit27）：警告沿出克隆帧、
   `nagKillDiag.cloned` 增长、**无红轮**——若有任何异常立即关掉
   并抓 /status
4. 并行验证（DND+bit27 同开）：无 counter 冲突、诊断独立增长
5. 跑完抓 /status（legacySmartSpeed + wheelDndDiag + nagKillDiag）

## 门禁

native：legacy **70** + wheel_dnd **24** + nag_kill **7** + dashboard
**11** + jitter（相关套件 154/154 全绿）。ESP 构建 SUCCESS。镜像
自检 GREEN（版本号 / uiBuildId / nag-kill-tgl 位于 wheel-dnd 上方 /
0x2F8 骑载字样清零——速度策略页只读源描述为合法保留）。SHA256SUMS
五文件复核 ✓。

设计文档：`docs/TX-SPLIT-RIDE-0X3EE-DESIGN-20261123.md`（含 bit27
风险披露：参与过 beta21 红轮现场，根因未定案；本次为干净基线+
保守参数重启，默认关）。
