# 固件资产 v6.0.0-beta18（2026-11-19）— P0 热修：/status JSON 断裂

**本目录为 LILYGO T-2CAN（lilygo_t2can_dual / ESP32-S3 16MB）生产资产。**
源码 `2c9ff57`（main）。uiBuildId =
`6.0.0-beta18-lilygo_t2can_dual-2c9ff57c766a-2026-11-19T09:09:35Z`。

## 修了什么（相对 beta17 的唯一变化）

beta17 的 `appendClone27DiagJson` 在 `blockedReason` 处多发了一个闭合引号
（`"blockedReason":"…"","tx"`），`/status` JSON 在 clone27Diag 处解析失败。
断点之后的所有 UI 数据段（CAN 诊断、速度、运行状态、配置回显）全部回退为
空——WebUI 表现为「CAN1 不工作 + 出现幽灵 3 路 CAN 卡片 + 配置保存看似
失效」。取证结论（二进制特征探针 + 串口启动日志 + /defense_config 实测）：
**CAN1/TWAI 硬件全程正常、NVS 保存实际成功、无任何 P4/3CAN 代码混入**——
纯 JSON 拼接手误。beta18 删除多余引号，`/status` 端到端可解析。

板上验收（2026-11-19）：`/status` JSON 合法（7764B 全解析）；
`capabilities.can3Available=false`；clone27Diag/wheelDndDiag 完整回读；
`/defense_config` 保存往返 ✓。

镜像自检（全绿）：0x0=E9 · 0x8000=AA50 · 0x20000=E9 ·
app_desc=`6.0.0-beta18`（uiBuildId `…-2c9ff57c766a-2026-11-19T09:09:35Z`）·
SHA256SUMS 五文件逐一复核 ✓ · app 1,228,816 B / full_merged 1,359,888 B。

## 文件清单与烧录

同 beta17 布局（S3：0x0 / 0x8000 / 0x19000 / 0x20000），见
`../6.0.0-beta17_20261118/README.md` 的烧录命令，把文件名替换为本目录。
根部 `LILYGO-T-2CAN_latest_*` 三指针已指向本目录。

## 其余内容

滚轮 beta02 回退引擎 + 0x045 bit27 状态克隆免打扰的功能说明、使用与
实车两阶段验证清单，全部沿用 beta17 README，无行为变化。
