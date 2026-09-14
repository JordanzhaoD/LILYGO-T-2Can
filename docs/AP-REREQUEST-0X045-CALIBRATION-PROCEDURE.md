# AP 取消重请求 0x045 抓包标定流程（4.5.0-beta01）

## 目的

4.5.0-beta01 协调器的拨杆编码是 profile 驱动的，编译默认值为空（fail-closed）。
本流程用一次真实车辆抓包回答以下问题，产出可人工审查的 profile 候选：

| 参数 | 抓包要回答的问题 |
|---|---|
| `stalkByte` / `stalkMask` / `stalkFwdValue` / `stalkRwdValue` | 前推、后拨两个手势到底改 0x045 的哪个字节、哪些位、变成什么值 |
| `stalkReleaseGapMs` | 物理拨杆按下到松开的时长 |
| `cancelEvidenceApMask` / `postQualApMask` | 前推后 AP 掉到哪些状态；后拨后 AP 到达哪些状态 |
| `sourceBus` | 0x045 原生帧在哪条总线（bus 1 = 主 CAN/TWAI，bus 2 = 副 CAN/MCP2515） |

同时交叉验证两个既有假设：

- 设计合同（冻结）：FWD=取消 AP、RWD=启动 AP；
- wheel-DND 时代的枚举猜测：byte3 低 3 位 `{1:up, 2:down, 3:forward, 4:back}`（无 DBC 证据）。

分析器**不预设**字段位置：它从空闲基线差分独立发现变化的字节，再与上述假设对比。

## 前置条件

- 设备固件 ≥ 4.3.0-beta.3（录制器已内置，**无需为此抓包刷 4.5.0-beta01**）。
- 抓包期间设备**只收不发**：方向盘滚轮 DND（左滚轮免打扰）等所有会写 0x045 的功能
  必须关闭。CSV 中出现 `dir=T` 的 0x045 行会污染标定，分析器会明确警告。
- 设备已在车、CAN 已接好、手机/电脑能访问设备网页。
- ⚠️ 本仓库当前阶段不烧录、不接车。以下步骤是将来实施时的操作规程；
  涉及 AP 激活状态的部分（第二遍）必须在安全受控环境进行，不在公共道路试错。

## 第一遍：停车标定编码（AP 不激活）

停车（P 挡）即可，这一遍回答"拨杆动了哪些字节"，不需要 AP 参与。

1. 打开设备网页 → CAN 工具 → 录制器（Recorder）。
2. 时长选 **120 秒**；ID 过滤保留缺省 NAG 集（已含 0x045、0x399、0x3EE），
   或手动填 `45,399,3EE`（十六进制）。不要留空录全量流量。
3. 点开始录制，**先静置 15 秒**，双手离开方向盘和拨杆（学习空闲基线）。
4. 然后**只动右侧拨杆**（AP 拨杆），每个动作之间等 3 秒：
   1. 前推到底一次（正常取消 AP 的动作），松开，等 3 秒；
   2. 后拨到底一次（正常启动 AP 的动作），松开，等 3 秒；
   3. 重复前推 → 后拨，共 **3 组**（3 次前推 + 3 次后拨）。
5. 剩余时间静置，等录制到时结束（或提前停止），确认状态变为"已保存"。
6. 点"下载 CSV"保存，例如 `calib_park.csv`。

命令行等价操作（设备 IP 替换后）：

```bash
curl -X POST "http://<device-ip>/rec_start?duration_s=120&ids=45,399,3EE"
curl http://<device-ip>/rec_status     # 查看进度/已写帧数
curl -X POST "http://<device-ip>/rec_stop"
curl http://<device-ip>/rec_download -o calib_park.csv
```

## 第二遍：受控环境验证 AP 因果（回答"哪个方向取消/启动"）

这一遍才验证 FWD=取消、RWD=启动 的因果映射，需要 AP 真的被取消和启动。
**必须在安全受控环境、注意力全程在驾驶上时进行**；若不具备条件，跳过本遍，
编码标定结果仍有效（但 profile 的 AP 集合参数只能保守填 {2,3}/{0,1,2}，标注未验证）。

1. 同第一遍步骤 1–3 启动 120 秒录制，静置 15 秒。
2. 在 AP 可用的正常驾驶状态下：
   1. 激活 AP（方向盘或本机任意既有方式）；
   2. 等 AP 稳定约 3 秒，**前推拨杆取消 AP**，等 5 秒；
   3. **后拨拨杆重新请求 AP**，等 AP 恢复稳定 5 秒；
   4. 重复取消 → 重请求，共 **3 组**。
3. 停止录制、下载 CSV，例如 `calib_ap.csv`。

安全要点：全程手扶方向盘；任何异常立即接管；不在公共道路试错；
这一遍只做物理拨杆动作，设备不发送任何帧。

## 分析

在电脑上（仓库根目录）：

```bash
python3 scripts/analyze_rerequest_045_calibration.py calib_park.csv calib_ap.csv
# 机器可读输出：
python3 scripts/analyze_rerequest_045_calibration.py calib_ap.csv --json
```

输出要点：

- `press events`：每个拨杆动作的开始时间、持续时长、变化的字节
  （`byte3 0x30->0x31` 格式）、以及 0x399 关联分类
  （`forward_cancel` = 事件后 AP 从激活掉到 ≤2；`rearward_request` = 事件后 AP 到达 {2,3}）。
- `profile_proposal`：
  - `status: proposed` 且两类手势都 ≥1 次 → 给出 `stalk_byte/stalk_mask/stalk_fwd_value/stalk_rwd_value`；
  - `matches_design_assumption_F1_R2`：是否与冻结合同（FWD=1/RWD=2）一致；
  - `matches_wheel_dnd_enumeration_F3_R4`：是否与 wheel-DND 枚举猜测（3/4）一致；
  - `stalk_release_gap_ms_candidate`、`cancel_evidence_ap_states_observed`、
    `post_qual_ap_states_observed`：时序与 AP 集合的观测建议。
- 两遍抓包的 byte/mask/值结论必须一致；`inconsistent_values` / `ambiguous_diff`
  表示动作序列有混杂（如同时碰到滚轮），需重抓。
- 质量门（全部满足才可进入 profile 审查）：
  - `CRC valid` = 全部帧；counter 连续（可用
    `scripts/analyze_legacy_wheel_045_capture.py` 交叉检查 counter/CRC/镜像）；
  - `tx_warning` 为空；`idle_ratio` 合理（大部分时间空闲）；
  - 前推、后拨各自 3 次事件全部同类、同值。

## 从标定到 profile（人工步骤）

1. 人工审查 `profile_proposal` 与两遍一致性；不要直接机器写入固件。
2. 把确认值填入 `include/dash_ap_rerequest_activation.h` 的
   `compiledApReRequestProfile()`（编码四参数、`stalkReleaseGapMs`、
   `sourceBus`、AP 集合三个 mask；`qualificationWindowMs` 见下方"首次标定结果"）。
3. `cancelEvidenceTimeoutMs` / `requestSeqTimeoutMs` 取观测到的
   事件→AP 变化时长的中位数加安全裕量，并在 CHANGELOG 标注来源。
4. 重跑 native 测试与合约测试；台架验证后才允许实车评估。
   本流程不构成"防甩已修复"的证据——那是后续台架与实车验证的结论。

## 首次标定结果（2026-09-10 抓包，已完成填入）

2026-09-10 Jordan 提供四个实车 CSV（两个静态手势 + 两个行驶 AP 时段），
按本流程分析后 profile 已填入并全量验证（Python 225 + native 20 环境 434
+ ESP SUCCESS），资产见 `firmware-assets/4.5.0-beta01_20260911-calibrated/`：

- 编码：**byte0 bits0-1，FWD=1（0x41）/ RWD=2（0x42）**，idle byte0=0x40；
  byte3 恒 0x30 —— wheel-DND byte3 猜测被证伪。
- 因果：14 次前推全部取消（6→1）；17 次后拨全部启动（AP 2→3）。
  冻结值映射 F1/R2 得到实车确认。
- **窗口偏离**：原生 0x3EE mux0 载波 1Hz（间隔中位 1000ms），冻结候选
  500ms 只覆盖 ~50% 轮次；编译值取 **1200ms**（残余未覆盖 ≤2.6%），
  已在 CHANGELOG / README / 代码注释三处标注，Jordan 可否决改回 500。
- 分析器两处按实车数据修正：AP=2→3 的后拨事件归 rearward_request；
  关联窗口截断到下一次按压（避免把下一次手势的启动归因给上一次按压）。

抓包证明的是物理拨杆编码与取消/启动因果；协调器合成重放的防甩效果
仍属未验证（无台架、无实车）。
