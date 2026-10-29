# 固件资产 v6.0.0-beta07（2026-10-30）— 全线三环境

**6.0 线第七个 beta：DND 重做为「每警告周期单帧、方向逐周期交替」（Plan D）。**
在 beta06（beta02 基线回退版）之上，只动 DND 一个模块（commit faf10fa）。

## Plan D：放弃成对帧，改为跨周期对冲

- **为什么**：beta03-05 三轮证明，车机在同一个窗口内合并同向感知的脉冲，
  同周期内的 +1/−1 补偿对必有一半搁浅 —— 间距 4-10ms / 25ms / 600ms
  全部净漂移，600ms 还把释放超时拉到 1.2s 引发 P0（FSD 门控开启后无法激活）。
- **怎么做**：一个警告周期内的所有帧同方向；方向游标只在「本周期确有帧
  上线」后翻转。连续两个成功周期 +n / −n 互相抵消，漂移上界收敛到单周期
  预算（±3 tick），不再累积。
- **周期内重试保留**：警告未消且预算未满时，确认后 800ms 冷却再补一帧
  （同方向）。预算 3 帧为硬上限 —— beta02 的 3 对上限只挡重试路径，
  新鲜原生帧可以绕过，现已封死。
- **安全栏全部保留**：回声窗 30ms、计数冲突中止、物理滚轮输入中止、
  释放超时 300ms（native 测试钉死上限，防 beta05 P0 重演）、连续两次
  TX 失败自动解除。
- **WebUI 诊断**随之从「配对记账」换为「方向记账」：本周期方向 / 下周期
  方向 / 交替完成次数（防护页 Legacy 实测卡）。

## 与 beta06 的关系

- beta06 = 回退版（DND 是 beta02 老行为，音量漂移已知存在）。
- beta07 = 在该基线上的第一轮真正修复。爆闪链、FSD、其余一切与 beta06
  完全一致；分区表 / ota_data 哈希与 beta06 逐字节相同（可交叉验证）。

## 构建环境

与 beta06 相同：penv62（PIO 6.2）+ `IDF_MAINTAINER=1` + gitignore 的
`platformio_local.ini`（file:// 本地包钉死），编译器 14.2.0+20260121，
与 beta01-06 一致。门禁：native 23 env 672/672、pytest 230+720 子测试、
clang-format 干净、release-metadata 检查通过。

## 自检

六镜像全绿：0xE9 magic @0x0/@0x20000、版本串 beta07（beta01-06 不在场）、
app_desc.version=6.0.0-beta07（全量重建 + ccache，无旧版本残留）、
Plan D 状态名在场且成对帧时代字符串绝迹、三环境 partitions/ota_data
哈希一致且与 beta06 相同、ui stamp=6.0.0-beta07-<env>-faf10fa。

## 文件

| 文件 | 用途 |
|---|---|
| `LILYGO-T-2CAN_v6.0.0-beta07_ota_app.bin` | 双 CAN 主力板 OTA 应用（WebUI OTA 上传这个） |
| `LILYGO-T-2CAN_v6.0.0-beta07_full_merged.bin` | 双 CAN 全链镜像（esptool @0x0 整刷） |
| `Waveshare-S3-RS485-CAN_*` / `Waveshare-SingleCAN_*` | 微雪两环境对应资产 |
| `bootloader/partitions/ota_data` | 各环境原始分段（调试用） |

烧录纪律：禁止 `pio run -t upload`；WebUI OTA 或 esptool @0x0 全链。

## 实车验证点（beta07）

1. **音量漂移**：DND 触发多次后音量不再单向累积（允许单周期 ±3 内波动，
   跨周期应回中）。防护页「正向帧/反向帧/交替完成」应大致 1:1 增长。
2. **P0 未回归**：FSD 门控开启时激活正常（beta06 已恢复，beta07 必须
   保持）。
3. **警告消除率**：单帧是否足以消警告 —— 若实测单帧常失败、总靠周期内
   第 2/3 帧才消，反馈 diag 数字（actionsThisEpisode / retryCount），
   下一轮再调。
