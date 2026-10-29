# 固件资产 v6.0.0-beta06（2026-10-29）— 全线三环境

**6.0 线第六个 beta：DND 回退版（回到 beta02 基线）+ 爆闪保留。**
beta05 实车两罪：DND 音量漂移（向下或向上）依旧，且 FSD 门控
开启时无法激活（P0 回归）。共 8 个 commit（ef5c6b9 回退 →
8a2c7fc/b7b6b9a/aa4c1a0 爆闪链回移植 → 3061bed bump →
a76bd15 GCC14.2 误报降级旗标）。

## 回退：DND 0x045 音量补偿（第三轮失败后止损）

- **三轮修复全部失败**：beta03 原子配对（4-10ms）→ beta04 间距
  20ms → beta05 间距 600ms + 释放超时 1200ms——车机端合并语义
  不是帧间距问题，600ms（真实换向地板）依然净漂移，且方向
  不定（升/降都出现）。
- **P0 回归**：beta05 固件层唯一改动就是两个常量（间距/超时），
  把 DND 会话最长持有时间从 ≤300ms 拉到 ≥1.2s，与 FSD 激活
  落地窗口重叠——门控开启后无法激活。回退即移除。
- **DND 现在的行为 = beta02 原版**：+1 → 等原生空闲 → −1 的
  两段式。已知问题：第二帧可能搁浅导致音量漂移——这是在
  此基线上继续修复的对象，下一轮将放弃成对帧设计。

## 保留：爆闪（0x292 STW_ACTN_RQ）

- beta04 实车证据（爆闪在场时 FSD 激活正常，P 档对照成立）
  排除爆闪链是 P0 元凶，故原样回移植：0x292 移植 + bus-off
  加固 + NVS 持久化（bk..bn）+ 双构建守卫。
- 烧录 beta06 后爆闪开关/参数跨重启保留（beta04 起的行为）。

## 构建环境说明（本版与 beta01-05 编译器一致）

本地构建因 registry 工具链清单漂移（espressif32@7.0.0 的 registry
元数据自 4 月起强索 xtensa/riscv 15.2.0）改用 penv62（PIO 6.2）+
`IDF_MAINTAINER=1`（IDF 官方逃生门，把工具链版本门禁降为警告）
+ gitignore 的 `platformio_local.ini`（file:// 本地包钉死）。
编译器 = 14.2.0+20260121，与 beta01-05 完全一致。
GCC 14.2 对 ArduinoJson 惯用法有 -Wdangling-reference 误报
（GCC 15 已修，CI 不受影响），三个发布 env 已加
`-Wno-error=dangling-reference` 降级（a76bd15）。

## 自检

六镜像全绿：0xE9 magic @0x0/@0x20000、版本串 beta06（beta02-05
不在）、app_desc.version=beta06（全量重建，无增量缓存残留）、
三环境共享 partitions/ota_data 哈希一致、
stamp=6.0.0-beta06-lilygo_t2can_dual-a76bd150e141。

## 文件

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v6.0.0-beta06_ota_app.bin` | 双 CAN 主力板 OTA 应用（WebUI OTA 上传这个） |
| `LILYGO-T-2CAN_v6.0.0-beta06_full_merged.bin` | 双 CAN 全链镜像（esptool @0x0 整刷） |
| `Waveshare-S3-RS485-CAN_*` / `Waveshare-SingleCAN_*` | 微雪两环境对应资产 |
| `bootloader/partitions/ota_data` | 各环境原始分段（调试用） |

烧录纪律：禁止 `pio run -t upload`；WebUI OTA 或 esptool @0x0 全链。
