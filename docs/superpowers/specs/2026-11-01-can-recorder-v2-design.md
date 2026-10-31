# P4-3CAN 录制引擎 v2（黑匣子 + 会话系统）设计

日期：2026-11-01 · 状态：待 Jordan 批准 · 仓库：LILYGO-T-2Can-4.5.0-beta01 @ p4-3can-board-port

## 意图与为什么

P4-3CAN 板从「T2CAN 原封移植」毕业为新功能开发主力板（Ban Shield、自动换档、
免钥匙的逆向基础设施）。录制工具是这一切的地基，但它现在是「表单配置 + 手动
起停 + 单文件 + 毫秒时间戳」——不适合车机/手机操作、偶发问题抓不到、时序精度
不够分辨相邻帧（1800fps 合计时帧间隔 555µs）。

Jordan 的三点硬需求（2026-11-01 对话原话归纳）：
1. **录制哪条总线可以选择**——per-bus 开关；
2. **操作按钮更适合车机、手机**——大按钮、少步骤、盲操作友好；
3. **不丢关键数据**——完整性可证明。

用途双主线（后者优先）：完整时序回放（已知 ID 的精确时间序列对齐分析）＋
未知信号挖掘（问题2 DND 偶发失效这类「不知道该盯哪个 ID」的场景）。

## 目标 / 非目标 / 反目标

**目标**
- 常开黑匣子：板子运行期间三路帧持续进 PSRAM 环形缓冲，任意时刻可锁定
  「刚才」的前后窗口为会话文件——偶发问题不用预测何时发生
- µs 级时间戳：能分辨相邻帧，时序回放的精度地基
- 会话系统：多会话并存、列表管理、按需下载/删除、空间自动滚动
- PCAP + CSV 双格式导出：SavvyCAN/candump 工具链直接打开（新功能开发效率）
- 移动优先 UI：三 bus 大开关 + 大锁定按钮 + 环形状态指示
- 丢帧自证：每会话 meta 记录环形溢出计数与来源队列满计数

**非目标**
- 不做实时图形化信号绘制（阶段三再议；分析在 SavvyCAN/本机做）
- 不做云端上传
- 不改车辆功能引擎（dash_*.h 功能层零改动）

**反目标**
- T2CAN 车上板（beta10 语义）行为必须不变——共享代码的改动若波及其行为即失败
- UI 复杂化到需要说明书（违背「预测→单点击破」哲学）

## 约束（已核实的事实）

- 硬件：ESP32-P4，32MB flash（app0/app1 各 8MB OTA + **SPIFFS 16.5MB @0x1020000**），
  PSRAM 实测空闲 **33MB**（最大连续块 33MB，串口 system_status 2026-11-01 实测）
- 三路 TWAI：CanA（主，X179 13/14）/ CanB（TX4/RX5）/ CanC（TX26/RX27，
  listen-only）；TWAI_RX_QUEUE_LEN=128/路
- 帧率量级：三路合计上限 ~1800 帧/s（CanA 实测历史 ~800/s 上限）
- 现有录制链：dashRecordCanFrame → recFilterIds(≤16) → recQueue(2048×19B,
  PSRAM) → writer 批 32 帧写 SPIFFS /rec.bin；CSV 导出 bus 列 1/2/3
- 架构红线：`include/web/mcp2515_dashboard.h` 等 9 板型共享；「T2CAN 原封移植」
  纪律已收口（2026-10-31），P4 从本功能起走独立功能线
- WebUI 内嵌固件（mcp2515_dashboard_ui.h，gzip blob）；无 WebSocket（WebServer
  库），轮询为主
- 车机/手机通过 AP（100.100.1.1）访问 UI；AP 稳定性刚修复（NAPT 自愈 843857f）

## 方案裁定（对话已裁）

- **A 快门（预设卡片）**：小改动但偶发问题必须在场手按——被否为主轴
- **B 黑匣子（常开环形+锁定）**：单键击破偶发问题——**选定主轴**
- **C 台架（SavvyCAN 式全工具）**：UI 复杂度违背简约哲学——否，其 PCAP 格式
  与触发器思想被吸收进 B 的阶段二/三
- 落地形态：B 主轴 + per-bus 大开关门面 + PCAP 导出；分三阶段交付

## 设计

### D1 常开环形缓冲（黑匣子核心）

- 单一 PSRAM 环形 `RecRingFrame`（定长记录，见 D6 格式），容量按帧预算分配：
  目标全量回溯 **≥10 分钟**（1800fps×600s×16B ≈ 17.3MB，取 20MB 预算，
  PSRAM 33MB 承受得起；分配失败降级 10 分钟）
- **环形永远摄取三路全量，不做任何 per-bus 门控**（Jordan 裁定 2026-11-01：
  bus 开关只控制锁定导出）。理由：「不丢关键数据」的最强形态——任意时刻
  锁定都能拿到全部三路；分环/门控会造成「关闭期间真没了」的数据空洞，
  且统一环形才能表达跨总线时间对齐
- 写入点（经 builder check 修正）：
  - RX：三个汇聚点已存在——CanA/CanB 走 handler onFrame→`mcpDashOnFrame`，
    CanC 走 `mcpDashSniffCanC`（mcp2515_dashboard.h:1106）；在这两个函数内的
    P4 宏分支提交环形
  - TX：**只挂 `mcpDashOnTxFrame`（驱动 onSendFrame 回调）单一路径**。
    不得挂 `dashRecordCanFrame` 调用点——TX 帧在现有调用图里会被记两次
    （onSendFrame 回调 + main.cpp/dashboard 的显式调用），双记会破坏 D7 的
    帧数一致性自证。旧 /rec.bin 的双记现状原样保留（T2CAN 红线）
- 写入并发（经 builder check 核实）：三路 RX 由单任务 `app_can_task`
  （core 0）依次排空（main.cpp:902-935）→ RX 侧单写者；TX 侧来自
  send 调用者上下文（帧率低）→ RX/TX 双写者，用原子头指针或短临界区，
  具体留建造者
- 满环滚动覆盖最旧，溢出计数 `ringOverflow++`（进会话 meta）

### D2 锁定（单键击破）

- 手动锁定：UI 大按钮 → `POST /ring_lock {pre_s, post_s, bus_mask}`；
  pre 默认 180s（可预设 60/180/300），post 默认 30s（锁定后继续录满 post
  窗口才落盘，期间 UI 显示倒计时）
- **空间不足=自动缩窗保最近**（Jordan 裁定 2026-11-01）：预估会话大小 >
  可用空间（含滚动删除旧会话后的空间）时，自动把 pre 窗口缩到装得下的
  长度，**优先保最近段**（事发点通常就在最近）；缩窗后 UI 明示实际锁定
  的窗口长度，meta 记录 `requested_pre_s / actual_pre_s / shrink_reason`
- 落盘时机：post 窗口到期 → 从环形拷贝 [now-pre, now+post] 区间内匹配
  bus_mask 的帧 → 紧凑格式写 `/sess_<seq>.bin` + 同名 meta json
- 落盘期间环形继续滚动（拷贝与写入并行，环形不冻结）——「锁定期间又发生
  的事」不丢
- 写满 SPIFFS：自动滚动删除最旧会话（保留 ≥1 个），meta 记录被挤掉的事实

### D3 会话系统

- SPIFFS 多会话：`/sess_<seq>.bin` + `.meta.json`；seq 单调递增（NVS 记账）。
  **文件名只用 seq**，不依赖任何时钟（Jordan 裁定 2026-11-01）
- 时间显示（Jordan 裁定 2026-11-01：SNTP+相对兜底）：STA 连网时 configTime
  对时一次，对上后会话 meta 与 UI 列表显示真实日期时间；未对上时显示
  「开机 +X:XX」相对时间（meta 同时存 boot 相对 µs 与真实时间，有真实时间
  才填）——两套并存，永不显示假时间
- API：`GET /sessions`（列表：seq/时长/帧数/丢帧数/bus 掩码/大小/时间）、
  `GET /session_download?seq=&fmt=csv|pcap|bin`、`POST /session_delete`
- meta 内容：起止时间（双钟）、pre/post 窗口（含 requested/actual）、bus 掩码、
  帧计数（按 bus 分列）、ringOverflow、twai 队列满计数、触发方式
  （manual/未来 auto）、当时的车辆配置快照（mode/功能开关）——「有利于开发」
  的上下文证据
- 旧 `/rec_start` 路径保留不动（T2CAN 对齐 + 兜底），P4 UI 默认入口切到黑匣子

### D4 导出格式

- 板载紧凑 `.bin`：16B/帧（见 D6）
- CSV：沿用现列（ts 变 µs）、bus 列 1/2/3
- **PCAP**：classic pcap（LINKTYPE_CAN 190，µs 时间戳），SavvyCAN 直接打开；
  实现 = 导出时逐帧转换（无压缩诉求，flash 上现转现流）
- 三格式同一数据源，fmt 参数选择

### D5 UI（移动优先改版）

- 录制页重构为三区（自上而下）：
  1. **状态环**：环形剩余回溯分钟数（全局恒定值——环形永远全录三路）+
     三路实时 fps 微型指示
  2. **大操作区**：三个 bus 大开关（色块 + CanA/CanB/CanC 名 + 实时帧率）——
     语义为**锁定时哪些线进会话**（预填 bus_mask，非摄取门控）；大锁定按钮
     （≥64px 高，主色）；pre 窗口用分段选择器（60s/3min/5min）
  3. **会话列表**：每项一行（时间+时长+帧数+丢帧徽标），点开＝下载
     CSV/PCAP/bin + 删除；有新会话自动置顶高亮
- **旧表单式录制面板收进「高级」折叠区**（Jordan 裁定 2026-11-01）：默认
  收起，点开展开原面板；API 原样保留
- 所有交互单手可达（下半屏）、无 hex 输入框、无嵌套弹层
- i18n 双语照旧（data-i18n 铁律：新标签必须进字典）

### D6 记录格式与时间戳

- 环形/落盘统一记录 16B packed：
  `uint32 tsUs`（相对启动 µs，71.6min 回绕，会话跨回绕由 meta 起止锚定换算）
  `uint32 id` `uint8 dlc` `uint8 flags`（bit0-1 bus, bit2-3 dir, bit4-7 预留零）
  `uint8 data[8]`
- 经 builder check 核实：`CanFrame` 不携带扩展帧/RTR 信息（驱动 read() 已
  丢弃 msg.extd/msg.rtr），flags 不设 extd/RTR 位——Tesla 总线为标准帧，
  预留零安全；不动共享驱动代码
- 时间源 `esp_timer_get_time()`；RX 循环收到帧即取（非入环时取），
  抖动仅来自队列驻留——meta 附每路队列驻留 P50/P99 采样（阶段二），
  先不做硬件时间戳
- 旧 RecFrame 19B/毫秒格式只服务旧 `/rec_start` 路径，两代并存互不转换

### D7 丢帧自证（「不丢关键数据」的可证明形态）

链路上每一级都有计数，全部进会话 meta：
1. TWAI 硬件/驱动层：队列满丢帧计数（twai_missed_msg 计数器，现有 diag 有）
   + RX 队列加深 **64→512**（builder check 核实：platformio.ini p4 env 以
   `-DTWAI_RX_QUEUE_LEN=64` 覆盖了驱动头默认的 128，三路同值——P2 项目）
2. 环形层：ringOverflow（滚动覆盖即「回溯窗口外的丢失」，属于设计内）
3. 落盘层：拷贝区间与 meta 帧数一致性校验
判断标准写入 UI：会话列表丢帧徽标 = 环形溢出>0 且溢出点落在窗口内
（溢出但已被滚出窗口＝无害，不亮徽标）

### D8 板型隔离（守住 T2CAN 原封红线)

- 新引擎整体进新头 `include/web/dash_recorder_v2.h`，编译开关
  `#if defined(DRIVER_P4_3CAN)`（platformio.ini p4 env 加 `-DDASH_RECORDER_V2`）
- mcp2515_dashboard.h 仅加小挂点，全在宏内：`mcpDashOnFrame`（RX CanA/B）、
  `mcpDashSniffCanC`（RX CanC）、`mcpDashOnTxFrame`（TX 单路径，防双记）、
  路由注册四处，每处 ≤3 行
- UI：P4 板的 UI blob 是同一份——新录制页用同一 blob 内的 P4 条件渲染
  （前端 JS 读 /capabilities 已有的板型能力位，不加新 blob 机制）
- 契约测试：新文件全量 token 契约 + 「T2CAN env 编译产物不含 v2 符号」检查

### 阶段划分

- **P1（本批）**：D1 环形 + D2 手动锁定 + D3 会话 API + CSV/紧凑导出 +
  D5 UI 三区改版 + D6/D7 时间戳与计数 —— 台架可端到端验证
- **P2**：PCAP 导出、pre/post 预设细化、队列驻留统计、RX 队列加深
- **P3**：自动触发器（DND 状态机异常→自动锁定，问题2 专属武器）、
  每会话配置快照、按 ID 实时过滤观察页（台架模式轻量版）

## 留给建造者

- 环形写入的并发方案（各 RX 任务独立写 vs 单收集任务）——先量测 RX 任务
  核心分布再定；无论哪种，溢出计数不可丢
- SPIFFS 写会话文件的吞吐验证（16B×N 帧流式写 vs 分块缓冲）
- UI blob 的条件渲染具体机制（capabilities 位已有先例，照抄最短路径）
- PCAP 头字节序与 SavvyCAN 实测兼容性验证（阶段二）
- µs 回绕换算的 meta 锚定字段命名

## 验收（P1 台架）

1. 上电即录（无任何操作），环形指示可见
2. 台架发帧（模拟器/回放）→ 锁定 3min → 会话出现、帧数与模拟量一致、
   丢帧徽标不亮
3. bus 开关只开 CanC → 会话仅含 CanC 帧（环形仍含三路全量，开关只影响落盘）
4. 空间不足时锁定 → 自动缩窗保最近，meta 记录 requested/actual 窗口
5. 手机单手完成全部操作（锁屏状态下 3 步内拿到下载链接）
6. T2CAN env（lilygo_t2can_dual）构建产物不含 v2 符号；车上板行为零变化
7. 全程契约测试绿；pytest 门禁照旧（native → penv build → pytest）
