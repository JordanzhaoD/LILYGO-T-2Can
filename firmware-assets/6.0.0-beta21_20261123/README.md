# 固件资产 v6.0.0-beta21（2026-11-23）— beta20 0x3EE 开关独立审查修复

**本目录为 LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `e273ab5`（main）。uiBuildId =
`6.0.0-beta21-lilygo_t2can_dual-e273ab5287a9-2026-11-22T16:41:25Z`。

## 🔧 这次改了什么（beta20 独立审查发现的两个泄漏 + 一个文档错误）

beta20 新增的「0x3EE FSD 激活注入」总开关存在两处漏网写入点与一处
README 错误，beta21 全部修复：

1. **mux1 RX 骑载泄漏**：beta20 只 gate 了 mux0 骑载与保活 tick；当
   policy 为 TeslaParity（或 Experimental+mux1，均可在运行时经
   `/legacy_fsd_config` 配置）时，mux1 分支（bit19/vision-limit 清位）
   在开关关闭后仍向 0x3EE 写帧。已补同款早退 gate。
2. **JITTER Bit46Shot 泄漏**：v1.19 JITTER 过程的 one-shot 本身就是
   一次设备 0x3EE 写入（native mux0 克隆 + bit46），但其 permit 链未
   查询开关。已并入 permit 级联——开关关闭时过程走自身干净的
   fullReset("permitLost")。（仅影响显式开启 jitter 的配置，默认关。）
3. **README OTA 端点错误**：beta20 README 的 curl 示例写了
   `/ota_update`——固件实际路由是 `POST /update`。beta20 README 已地
   铆修正，本 README 从一开始就写对。

不受影响名单（审查逐行确认 + native 钉子测试锁死）：A3 对齐模式
0x2F8 镜像骑载、Ban Shield、Legacy 左滚轮免打扰（0x045 独立 tick）、
bit27 克隆模块。开关默认开 = 与 beta19/beta20 行为一致。

新增测试：native_legacy 5 用例（关= mux0/mux1/保活全停、0x2F8 骑载
存活、开= no-op）+ 合约钉子（NVS `f3ee` 往返、`/defense_config` 字段、
两处回调安装、handlers.h 恰好三处 gate、JITTER permit 子句、UI 令牌）。

## 📲 OTA 升级（板子在车上时的推荐路径）

1. 手机/电脑连板子 WebUI → **Release 在线更新** 页 → 上传
   `LILYGO-T-2CAN_v6.0.0-beta21_ota_app.bin`
2. 或命令行（端点是 **`/update`**，裸 body + X-File-Name 头，勿用
   multipart）：
   ```
   curl -X POST "http://<板子IP>/update" \
        -H "X-File-Name: LILYGO-T-2CAN_v6.0.0-beta21_ota_app.bin" \
        --data-binary @LILYGO-T-2CAN_v6.0.0-beta21_ota_app.bin
   ```
3. 升级后核对 `/status` 的 `uiBuildId` 以
   `6.0.0-beta21-lilygo_t2can_dual-e273ab5` 开头。

USB 全新整刷（一次性，S3 布局）：

```
esptool --chip esp32s3 write_flash \
  0x0     LILYGO-T-2CAN_bootloader.bin \
  0x8000  partitions.bin \
  0x19000 ota_data_initial.bin \
  0x20000 LILYGO-T-2CAN_v6.0.0-beta21_ota_app.bin
```

或直接 `write_flash 0x0 LILYGO-T-2CAN_v6.0.0-beta21_full_merged.bin`。

## 🧪 验证清单

- FSD 防御页关闭「0x3EE FSD 激活注入」后：
  - `/status` 的 `legacyFsdDiag.mux0/mux1` 无新增 TX 计数
  - A3 对齐模式下 `legacySmartSpeed.ride2f8TxOk` 持续增长
  - 左滚轮免打扰照常工作
- 若开启 jitter 实验同时关闭本开关：jitter 过程应立即 permitLost
  全复位（其自身语义），不残留 0x3EE 帧

## 门禁与镜像自检

- native：26 环境 **842/842 通过**（含 native_legacy 新 5 用例，85/85）
- pytest：573 passed；5 个失败为 52bd8e8 之前就存在的陈旧合约钉子
  （`directionPositive` 等，7f24209 移除字段后钉子未跟），与本版无关
- `lilygo_t2can_dual` wrapper 构建全绿（CMake 强制重配，app_desc=
  6.0.0-beta21）
- 镜像自检：0x0=E9 · 0x8000=AA50 · 0x20000=E9 ·
  app_desc=`6.0.0-beta21` · SHA256SUMS 五文件复核 ✓ ·
  full_merged 分区拼接核对 ✓
- app 1,229,280 B · full_merged 见 SHA256SUMS

`latest` 三指针已指向本目录。不 push（留给主会话）。
