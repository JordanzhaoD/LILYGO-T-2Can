# 固件资产 v6.1.0-beta02（2026-11-23）— beta02 干净基线 + TX 分路 + 首页主开关修正

> beta01 → beta02 唯一变化：**首页「模块总开关」改绑 0x3EE 注入开关**。
> 根因定案：首页主开关原 POST `/config can:0` 清 `canActive`——这正是
> "关一个开关死三个功能"的隐藏总闸。现在它只切 `fsd_3ee_inject`（与
> defense 页同源）：关 = 仅 0x3EE 停，滚轮免打扰与速度偏移（A3 骑载）
> 不受影响。`/config` 的 `ci` = `canActive && fsd_3ee_inject`，按钮状态
> 忠实反映 0x3EE 实际注入能力。

**LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `e29aa9d`（main）。uiBuildId =
`6.1.0-beta02-lilygo_t2can_dual-e29aa9dfc222-2026-11-23T00:xxZ`。

## 版本系列说明（必读）

**6.0.0-beta03..beta21 全系作废**（beta21 实车复现红色方向盘警告，路线
整体否决）。main 已硬回退到 beta02 发布提交（历史存档于
`archive/pre-rollback-beta21` 分支）。**6.1.0 = beta02 干净基线上的新系列**，
唯一新增功能 = TX 分路；滚轮免打扰引擎 = beta02 原版（唯一田野验证零失效
的版本）。

## TX 分路：「0x3EE FSD 激活注入」总开关

FSD 防御页新开关（**默认开 = 与 beta02 行为逐字节一致**）：

- **关**：本机 0x3EE 全部写入停止（mux0+mux1，单点 gate 于 1006 派发入口）
  ——配合车辆**原生激活**使用；速度智能偏移**自动改走 A3 0x2F8 骑载**
  （在车辆自己的 0x2F8 帧上重写 `UI_userSpeedOffset` 字段，实战标定过的
  A3.2 设计）；左滚轮免打扰**不受影响**（beta02 原本就是独立 tick）。
- **开**：一切与 beta02 相同（骑载休眠，0x3EE 速度请求路径原样）。
- 诊断：`/status` 的 `legacySmartSpeed.fsd3eeInject` / `ride2f8TxOk`；
  速度偏移被 0x3EE 关闭接管时 `blockedReason=fsd3eeMasterOff`。

## 📲 OTA 升级（板在车上）

WebUI「Release 在线更新」上传 `LILYGO-T-2CAN_v6.1.0-beta02_ota_app.bin`，
或：

```
curl -X POST "http://<板子IP>/update" \
     -H "X-File-Name: LILYGO-T-2CAN_v6.1.0-beta02_ota_app.bin" \
     --data-binary @LILYGO-T-2CAN_v6.1.0-beta02_ota_app.bin
```

（⚠️ 从 beta21 降级到 6.1.0-beta02 属跨系列 OTA：分区表相同可直接刷；
刷后 NVS 中旧系列键不影响新开关 `f3ee`，默认开。）

升级后核对 `/status` 的 uiBuildId 以
`6.1.0-beta02-lilygo_t2can_dual-e29aa9d` 开头。

USB 整刷（S3 布局 0x0/0x8000/0x19000/0x20000，文件替换为本目录）同历史
README。

## 田野验证清单

1. 开关默认开：滚轮 DND / 速度偏移行为 = beta02 原版（无回归）
2. 开关关 + 原生激活：滚轮 DND 清警告正常；`ride2f8TxOk` 持续增长；
   0x3EE 发送计数停增；**全程无红色方向盘警告**（beta02 引擎 + 原生激活
   的组合此前从未引入 strike-out 风险）
3. 跑完抓 `/status`（`legacySmartSpeed` + `wheelDndDiag` 段）回传

## 门禁

native：native_legacy **68/68**（+5 TX 分路钉子：关→0x3EE 零发送；
关+Manual 偏移→恰一帧 0x2F8 骑载且仅改 data[5] bits0-5；开→beta02 行为；
回调未绑定→no-op）+ dashboard + wheel 全绿。ESP 构建 SUCCESS（工具链适配
`-Wno-error=dangling-reference` 仅作用于新 GCC 误报）。镜像自检 GREEN ·
SHA256SUMS 五文件复核 ✓。
