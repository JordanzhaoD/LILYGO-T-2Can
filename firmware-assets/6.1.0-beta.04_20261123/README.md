# 固件资产 v6.1.0-beta.04（2026-11-23）— 防护页布局修复 + 双入口注记

> beta03 → beta.04 唯一变化：**UI 层两处修复（零固件行为变化）**。
> 1. **防护页行序修复**：新开关「0x3EE FSD 激活注入」原插在「Legacy
>    左滚轮免打扰」开关与它的诊断表（告警状态/原生 0x045/配对计数/
>    回声镜像，DOM ID 全为 `wheel-*`）中间，把滚轮引擎和它自己的体检表
>    切断——诊断表视觉上挂到了 0x3EE 开关名下。现顺序为
>    滚轮开关 → 滚轮诊断表 → 0x3EE 开关 → 速度免打扰（已在镜像内
>    gzip blob 解压断言钉扎）。
> 2. **双入口注记**：0x3EE 开关描述末尾注明「与首页『模块总开关』为
>    同一开关的两个入口，状态互通」（中英双语），消除"是否重复开关"
>    的困惑。首页大按钮与防护页开关本就同源（NVS `f3ee` /
>    `fsd_3ee_inject`），任何一处改动另一处同步。
>
> 版本号改为 **beta.04 点号式**：修复 parseVersion 同脉 beta02→beta03
> 判"不更新"的 quirk（preNum 只解析带点 `beta.N`）——beta.04 对所有
> 已装版本 OTA 正确提示更新。

**LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `ad0e4b6`（main，UI 修复 commit `31c3766`）。uiBuildId =
`6.1.0-beta.04-lilygo_t2can_dual-31c3766880a6`
（已从本目录 ota_app.bin 内嵌字符串核对）。

## 版本系列说明（必读）

**6.0.0-beta03..beta21 全系作废**（beta21 实车复现红色方向盘警告，路线
整体否决）。main 已硬回退到 beta02 发布提交（历史存档于
`archive/pre-rollback-beta21` 分支）。**6.1.0 = beta02 干净基线上的新
系列**，唯一新增功能 = TX 分路；滚轮免打扰引擎 = beta02 原版（唯一
田野验证零失效的版本）。

## TX 分路：「0x3EE FSD 激活注入」总开关

FSD 防御页 + 首页「模块总开关」双入口（同一状态）：

- **关**：本机 0x3EE 全部写入停止（mux0+mux1+jitter，单点 gate 于 1006
  派发入口 + main.cpp permit 级联）——配合车辆**原生激活**使用；速度
  智能偏移**自动改走 A3 0x2F8 骑载**；左滚轮免打扰**不受影响**。
- **开**（默认）：一切与 beta02 相同。
- 诊断：`/status` 的 `legacySmartSpeed.fsd3eeInject` / `ride2f8TxOk`；
  被总闸接管时 `blockedReason=fsd3eeMasterOff`。

## 📲 OTA 升级（板在车上）

WebUI「Release 在线更新」上传
`LILYGO-T-2CAN_v6.1.0-beta.04_ota_app.bin`，或：

```
curl -X POST "http://<板子IP>/update" \
     -H "X-File-Name: LILYGO-T-2CAN_v6.1.0-beta.04_ota_app.bin" \
     --data-binary @LILYGO-T-2CAN_v6.1.0-beta.04_ota_app.bin
```

升级后核对 `/status` 的 uiBuildId 为
`6.1.0-beta.04-lilygo_t2can_dual-31c3766880a6`。

USB 整刷（S3 布局 0x0/0x8000/0x19000/0x20000，文件替换为本目录）。

## 田野验证清单

1. 开关默认开：滚轮 DND / 速度偏移行为 = beta02 原版（无回归）
2. 防护页视觉：滚轮开关下紧跟自己的诊断表；0x3EE 开关在诊断表之后
3. 开关关 + 原生激活：滚轮 DND 清警告正常；`ride2f8TxOk` 持续增长；
   0x3EE 发送计数停增；**全程无红色方向盘警告**
4. 跑完抓 `/status`（`legacySmartSpeed` + `wheelDndDiag` 段）回传

## 门禁

native：dashboard **11/11** + legacy **68/68** + wheel_dnd + jitter
（141/141 相关套件全绿）。ESP 构建 SUCCESS。镜像自检 GREEN：
版本号 / uiBuildId / 双入口注记 zh+en / 行序断言
（wheel-diag < 0x3EE-switch < speed-nd）全部 PASS。
SHA256SUMS 五文件复核 ✓。
