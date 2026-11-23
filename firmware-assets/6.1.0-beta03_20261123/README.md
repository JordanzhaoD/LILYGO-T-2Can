# 固件资产 v6.1.0-beta03（2026-11-23）— TX 分路总闸覆盖修复 + 首页开关恢复修复

> beta02 → beta03 唯一变化：**两处独立审查修复**（无新功能、无行为面扩大）。
> 1. JITTER 防甩 one-shot（Bit46Shot 本身就是一次 0x3EE 写入）纳入
>    「0x3EE FSD 激活注入」总闸 permit 级联——总闸关时防甩程序
>    fullReset("permitLost") 而不是漏发 0x3EE（与已作废 beta21 线的同款修法）。
> 2. 首页「模块总开关」开启时的恢复路径修复：原用 `!S.can`（=canOnline
>    总线在线）做条件，`canActive=0` 而总线在线的板子（旧总开关留下的
>    田野现场状态）永远无法从首页恢复。现在开启时无条件补发
>    `can:1+force:1+bootCan:1`（已武装时在 handleConfig 中为幂等 no-op）。

**LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `d0cc172`（main）。uiBuildId =
`6.1.0-beta03-lilygo_t2can_dual-d0cc1723dc77-2026-11-23T01:22:35Z`
（已从本目录 ota_app.bin / full_merged.bin 内嵌字符串核对）。

## 版本系列说明（必读）

**6.0.0-beta03..beta21 全系作废**（beta21 实车复现红色方向盘警告，路线
整体否决）。main 已硬回退到 beta02 发布提交（历史存档于
`archive/pre-rollback-beta21` 分支）。**6.1.0 = beta02 干净基线上的新系列**，
唯一新增功能 = TX 分路（beta01）；beta02 修首页开关绑定；beta03 修两处
覆盖/恢复缺陷。滚轮免打扰引擎 = beta02 原版（唯一田野验证零失效的版本）。

## TX 分路：「0x3EE FSD 激活注入」总开关

FSD 防御页新开关（**默认开 = 与 beta02 行为逐字节一致**）：

- **关**：本机 0x3EE 全部写入停止——mux0+mux1（1006 派发入口单点 gate）
  **+ JITTER Bit46Shot（beta03 起入 permit 级联）**——配合车辆**原生激活**
  使用；速度智能偏移**自动改走 A3 0x2F8 骑载**（在车辆自己的 0x2F8 帧上
  重写 `UI_userSpeedOffset` 字段，data[5] bits0-5、raw=kph+30、保留
  bits6-7，dlc=7 内）；左滚轮免打扰**不受影响**（beta02 原本就是独立 tick）。
- **开**：一切与 beta02 相同（骑载休眠，0x3EE 速度请求路径原样）。
- 已知残留：插件引擎（用户自装 JSON 规则）可按设计自行写 1006，不受此
  总闸约束（`pluginOwnsFsdActivation` 语义，属显式用户代码）。
- 诊断：`/status` 的 `legacySmartSpeed.fsd3eeInject` / `ride2f8TxOk`；
  速度偏移被 0x3EE 关闭接管时 `blockedReason=fsd3eeMasterOff`。

## 📲 OTA 升级（板在车上）

WebUI「Release 在线更新」上传 `LILYGO-T-2CAN_v6.1.0-beta03_ota_app.bin`，
或（本固件 /update 接受裸 body + X-File-Name，Basic 认证在本构建为透传）：

```
curl -X POST "http://<板子IP>/update" \
     -H "X-File-Name: LILYGO-T-2CAN_v6.1.0-beta03_ota_app.bin" \
     --data-binary @LILYGO-T-2CAN_v6.1.0-beta03_ota_app.bin
```

（⚠️ 跨系列说明：从 beta21 或 6.1.0-beta01/02 升到 beta03，分区表相同
（d7cca49…）可直接刷。NVS 沿用同键 `f3ee`：beta21 板上若曾关过同义开关，
升级后保持关闭，属预期继承；其余旧系列键（如 `def_c27`）为新固件不读取
的孤儿键，无害。）

升级后核对 `/status` 的 uiBuildId 以
`6.1.0-beta03-lilygo_t2can_dual-d0cc1723dc77` 开头。

USB 整刷（S3 布局 0x0/0x8000/0x19000/0x20000，文件替换为本目录）同历史
README。

## 田野验证清单

1. 开关默认开：滚轮 DND / 速度偏移行为 = beta02 原版（无回归）
2. 开关关 + 原生激活：滚轮 DND 清警告正常；`ride2f8TxOk` 持续增长；
   0x3EE 发送计数停增；**若开了防甩（jtr_on）：TX 证据里 0x3EE 也应为零
   （beta03 修复点）**；全程无红色方向盘警告
3. 首页开关关闭再开启（含 canActive 曾为 0 的板子）：ci 变 1 且不回弹
   （beta03 修复点）；关闭期间 DND 仍工作
4. 跑完抓 `/status`（`legacySmartSpeed` + `wheelDndDiag` 段）回传

## 门禁

native：全环境 **583/583**（native_legacy 68/68 含 5 个 TX 分路钉子：
关→0x3EE 零发送；关+Manual 偏移→恰一帧 0x2F8 骑载且仅改 data[5] bits0-5、
bits6-7 保留；开→beta02 行为；回调未绑定→no-op）。ESP 构建
`IDF_MAINTAINER=1 pio run -e lilygo_t2can_dual` SUCCESS（干净重建
`.pio/build/lilygo_t2can_dual`，VERSION bump 后 touch CMakeLists 防
app_desc 滞后；二进制内嵌 6.1.0-beta03 共 7 处已核对）。merge_firmware.sh
镜像自检 GREEN（0x0=0xE9 / 0x8000=0xAA50 / 0x20000=0xE9）· SHA256SUMS
五文件复核 ✓ · latest 三指针指向本目录 ✓。

注：jitter permit 修复位于 `src/main.cpp`（ESP-only，无 native 桩架），
以田野清单第 2 条代验。
