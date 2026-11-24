# 固件资产 v6.1.0-beta.07（2026-11-24）— 右滚轮按压免打扰

> beta.06 → beta.07：**bit27 状态机制免打扰彻底删除，换成右滚轮按压免打扰**。
>
> beta.06 实车判定：状态机制免打扰开启后车辆大量报警、无法使用——该路线
> 整体否决。新方向建立在**实车稳定的左滚轮免打扰**基础上，开发一个并行
> 机制：**NAG 提醒时合成右滚轮按压帧消除提示**（按压右滚轮即可消除 NAG）。
>
> **改动**：
> 1. **右滚轮按压引擎**（`dash_legacy_wheel_045_press.h`）：警告激活期间
>    （hands [3,6]|[9,10]），每个原生 idle 0x045 都 1:1 克隆、置右滚轮按压位
>    （switch word bit4 = data[4]|=0x10）、counter+1、重算 CRC，骑原生帧
>    1:1 跟发；警告清除立即停。按压无音量累积副作用，无需反向平衡。
> 2. **bit27 状态机制彻底删除**：引擎、测试、NVS `nk27`、`/status
>    nagKillDiag`、`/defense_config nag_kill`、UI 开关全部移除。
> 3. **与左滚轮免打扰并行、互让路**：两套引擎独立开关、独立诊断，同一
>    beat 上不抢 counter（beat-generational yield）。
> 4. 安全栅全保留：idle-only 骑载、own-echo 抑制、DND 互认让路、TX 两连败
>    disarm 自愈（800ms/60s 限 3 次）。

**LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
uiBuildId = `6.1.0-beta.07-lilygo_t2can_dual-c5e476526276`
（已从本目录 ota_app.bin 内嵌字符串核对）。

## 版本系列说明（必读）

**6.0.0-beta03..beta21 全系作废**（beta21 实车红轮）。
6.1.0 = beta02 干净基线 + TX 分路。左滚轮免打扰 = beta02 原版（实车稳定）；
右滚轮按压 = 新增实验路径（**默认关**）。

## 开关语义

| 开关 | 开（默认） | 关 |
|---|---|---|
| 0x3EE FSD 激活注入（首页大按钮=同源） | beta02 全量 | 激活引擎停；速度偏移走 offsetOnly |
| Legacy 左滚轮免打扰 | beta02 原版+自愈（实车稳定） | 停 |
| 右滚轮按压免打扰（实验） | 警告期持续按压骑载 | 停（默认） |

## 📲 OTA 升级（板在车上）

WebUI「Release 在线更新」上传
`LILYGO-T-2CAN_v6.1.0-beta.07_ota_app.bin`，或：

```
curl -X POST "http://<板子IP>/update" \
     -H "X-File-Name: LILYGO-T-2CAN_v6.1.0-beta.07_ota_app.bin" \
     --data-binary @LILYGO-T-2CAN_v6.1.0-beta.07_ota_app.bin
```

升级后核对 `/status` 的 uiBuildId 为
`6.1.0-beta.07-lilygo_t2can_dual-c5e476526276`。

## 田野验证清单（关键闸）

1. 开关默认关：全行为=beta.02 无回归（右滚轮按压默认关，不介入）
2. **右滚轮按压单独验证（左滚轮 DND 关、只开右滚轮按压）**：NAG 出现时
   `rightWheelPressDiag.pressed` 增长、提示消除、**无红轮无卡顿**——若有
   异常立即关掉并抓 /status
3. 左滚轮免打扰回归：默认关右滚轮按压，左滚轮照常清除警告
4. 并行验证（左右同开）：无 counter 冲突、诊断独立增长
5. 跑完抓 /status（wheelDndDiag + rightWheelPressDiag）

## 门禁

native 全量 **186/186 全绿**（右滚轮按压 9 例新增：按压编码、电平语义、
警告窗、echo 不链、动作帧拒绝、disarm 自愈、beat-yield；bit27 12 例删除）。
ESP 构建 SUCCESS（工具链 15.2.0）。镜像自检 GREEN（版本号 / uiBuildId /
0x0 魔数 0xe9）。SHA256SUMS 五文件复核 ✓。

## 抓包标定依据（右滚轮按压位）

实车三份录制（2026-11-24）交叉验证：右滚轮按压 = 0x045 开关字
（b4|b5<<8）bit4 = `data[4]|=0x10`；右滚轮侧键 B = bit5 = 0x20；左滚轮
按压 = bit1 = 0x02。本项目 `docs/LEGACY-0X045-WHEEL-VEHICLE-ANALYSIS` 的
权威位表与抓包一致。
