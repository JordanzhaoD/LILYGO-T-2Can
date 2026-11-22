# 固件资产 v6.0.0-beta20（2026-11-23）— TX 分路：0x3EE FSD 激活注入独立开关

**本目录为 LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `670f0ef`（main）。uiBuildId =
`6.0.0-beta20-lilygo_t2can_dual-670f0efd9a0d-2026-11-22T16:xxZ`。

## 🔧 这次改了什么（针对 2026-11-23 田野问题）

**背景**：车辆改为原生方式激活 FSD 后，需要关闭本机 0x3EE 注入——但旧版
本上能停 0x3EE 的开关会连坐杀死「Legacy 左滚轮免打扰」与「速度智能偏移」。
两段 beta02 实车录制（2026-11-23）分析结论：0x399/0x045 输入完全健康
（apState=6、警告沿、CRC/counter 全对，引擎重放可正常出帧），总线零注入
——纯固件侧开关耦合问题。

**beta20 方案**：FSD 防御页新增独立总开关「**0x3EE FSD 激活注入**」
（默认**开**=与 beta19 行为逐字节一致；NVS `f3ee`；`/defense_config`
字段 `fsd_3ee_inject`）。**关闭后仅本机 0x3EE 写入全停**（激活注入、
保活、0x3EE 速度请求路径），并且：

- **左滚轮免打扰不受影响**（独立 tick，无 0x3EE 耦合）
- **速度智能偏移不受影响**——走 A3 对齐模式的 0x2F8 镜像骑载（不写
  0x3EE）；若速度偏移被 0x3EE 关闭挡住（非 A3 路径），诊断字段
  `legacySmartSpeed` 的 blockedReason 会明确报 `fsd3eeMasterOff`
- Ban Shield 不受影响

实现：handler 新增 `fsd3eeInjectAllowed` 回调，精确 gate 两处 0x3EE
写入点（RX 骑载 + 保活 tick），**不经过 checkAD/injectionAllowed**，
不误伤其他注入。

## 📲 OTA 升级（板子在车上时的推荐路径）

1. 手机/电脑连板子 WebUI → **Release 在线更新** 页 → 上传
   `LILYGO-T-2CAN_v6.0.0-beta20_ota_app.bin`
2. 或命令行（裸 body + X-File-Name 头，勿用 multipart）：
   ```
   curl -X POST "http://<板子IP>/update" \
        -H "X-File-Name: LILYGO-T-2CAN_v6.0.0-beta20_ota_app.bin" \
        --data-binary @LILYGO-T-2CAN_v6.0.0-beta20_ota_app.bin
   ```
3. 升级后核对 `/status` 的 `uiBuildId` 以
   `6.0.0-beta20-lilygo_t2can_dual-670f0ef` 开头。

USB 全新整刷（一次性，S3 布局）：

```
esptool --chip esp32s3 write_flash \
  0x0     LILYGO-T-2CAN_bootloader.bin \
  0x8000  partitions.bin \
  0x19000 ota_data_initial.bin \
  0x20000 LILYGO-T-2CAN_v6.0.0-beta20_ota_app.bin
```

或直接 `write_flash 0x0 LILYGO-T-2CAN_v6.0.0-beta20_full_merged.bin`。

## 🧪 上车验证清单（原生激活 + 0x3EE 关闭场景）

1. OTA 到 beta20 → FSD 防御页**关闭**「0x3EE FSD 激活注入」→ 确认
   「Legacy 左滚轮免打扰」开关仍在、速度四档配置不变
2. 原生激活 FSD 行驶：
   - 滚轮免打扰恢复工作（警告 ~1s 内被清，音量平衡）
   - 速度智能偏移生效（A3 对齐模式下 `/status` 的
     `legacySmartSpeed.ride2f8TxOk` 持续增长）
   - `/status` 无 `fsd3eeMasterOff` 误报（若出现=速度偏移走到了非
     A3 路径，回报数据）
3. 想恢复 0x3EE 注入：开关拨回即生效，无需重启

## 门禁与镜像自检

- native：dashboard / legacy / 3ee_keepalive / 3ee_cadence / wheel DND
  共 **143/143 通过**；`lilygo_t2can_dual` 与构建全绿（wrapper 构建）
- 镜像自检：0x0=E9 · 0x8000=AA50 · 0x20000=E9 ·
  app_desc=`6.0.0-beta20` · SHA256SUMS 五文件复核 ✓
- app 1,229,280 B · full_merged 见 SHA256SUMS

其余功能（beta19 的 930ms 首帧延迟、bit27 克隆模块等）行为不变；
`latest` 三指针已指向本目录。
