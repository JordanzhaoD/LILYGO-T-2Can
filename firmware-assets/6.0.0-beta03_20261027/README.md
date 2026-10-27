# 固件资产 v6.0.0-beta03（2026-10-27）— 全线三环境

**6.0 线第三个 beta，双实车问题修复版**：① CAN2 大灯爆闪在 Legacy
Model X（2019 MCU2）上完全失效 → 移植实车已验证的 0x292 成功路径并
治本 bus-off；② 左滚轮 DND 免打扰音量单向漂移 → 重构为原子配对脉冲。
共 4 个 commit（089de1b → 0c4c61b → be304c7 → 34ccad3）。

## 修复一：CAN2 大灯爆闪（Legacy Model X）

- **为什么完全不能用**：6.0 血统只实现了 Fresh 平台的 0x249
  SCCM_leftStalk（Model Y CRC），在 X 上 0x249 是 DAS_visualDebug
  调试遥测——注入结构性无效。
- **为什么历史上「刚插上能用、跑一阵就死」**：旧血统的 0x292
  STW_ACTN_RQ（BDY 总线）是真实路径（本车实车已验证），但注入帧与
  真实拨杆模块同 ID 不同数据 → CAN 位错误 → TEC 累积 → MCP2515
  bus-off → 控制器掉线（重启即好，与症状完全吻合）。
- **治本双管齐下**（移植自 v4.3.0-beta.28/29，commit d1f5873/77228ad）：
  1. **骑尾流注入**：只在真实 0x292 后 3-20ms 窗口内发、≥25ms 节流、
     模板 300ms 过期即停——碰撞近零；
  2. **驱动加固**：`send()` 发送前也检测 bus-off（原来只在接收路径
     检测，注入会静默死亡）+ 恢复冷却 3000→1000ms，偶发也能快速自愈。
- 平台分流在既有 `dashLegacyHandlerActive()` seam：Legacy 走 0x292
  （变异最近真实模板，绝不从零合成），Fresh 0x249 零改动。
- 新纯逻辑模块 dash_legacy_stalk_burst.h + 11 个 native 测试（bit 语义、
  CRC8 SAE J1850 向量、模板新鲜度、尾流窗口）。
- `/stalk_test` 与 `/burst` JSON 新增 `"stalk_platform"`:
  legacy0x292 / fresh0x249，实车可直查当前生效的编解码器。
- **雾灯未移植**：此血统从未有 X 可用路径（0x273/FOG_BASE 为死代码
  且无 UI 入口），需录料单独立项。

## 修复二：DND 左滚轮音量漂移（原子配对）

三根因（native 测试逐一实证）：搁浅（告警清/物理滚轮打断配平→净+1）、
竞态（配平 tick 与控制器真实帧对撞）、上限漏洞（3 对用完回 Idle 后
重挂路径放出第 4 个正向）。修复：正负 tick 合成一个不可分割的 burst
（新状态 BurstSecondDue=正向上总线后负向无条件欠账）+ 47ms 载波锁窗
+ 计数器连续性 + cap 守护补 Idle 路径 + 双槽 echo 指纹。

**实车验收**：读 `wheelDndDiag`，`positiveTx == negativeTx &&
burstSplitAborts == 0`。

## 验证矩阵

native_legacy_stalk_burst 11/11（新增，TDD 先红后绿）；
native_legacy_wheel_045_dnd 18/18（重写）；native_mcp2515_recovery 4/4；
native_dashboard 11/11；pytest 228 通过/701 子测试；三环境 ESP32 构建
SUCCESS；clang-format 净（存量漂移不动）。

## 实车检查项

1. 烧录后 `/burst` 确认 `"stalk_platform":"legacy0x292"`；
2. `/stalk_test?mode=flash` 单测爆闪；
3. 爆闪长跑期间看 `/bus2_ids` 的 `can2.txerr` 与恢复计数是否增长
   （beta.29 的 bus-off 治本当年未做长跑复测）；
4. DND 触发一次告警后读 `wheelDndDiag` 验收。

## 文件清单（11）

三平台各 bootloader / ota_app / full_merged + 共享 partitions.bin /
ota_data_initial.bin（三环境逐字节一致）。SHA256SUMS.txt 含全部 11 行。
