# P4-3CAN 录制引擎 v2 — P1（黑匣子+会话系统）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** P4 板常开黑匣子录制（PSRAM 环形三路全量、µs 时戳）+ 单键锁定落盘成会话文件 + 会话 API + 移动优先 UI 三区改版。

**Architecture:** 新引擎独立头 `dash_recorder_v2.h`，`DASH_RECORDER_V2` 宏隔离（仅 p4 env 编译）；共享 dashboard 头只加 ≤3 行/处的宏内挂点（RX×2 + TX×1 + 路由）。旧 `/rec_start` 链路字节不动。UI 改 `mcp2515_dashboard_ui.src.h` 源再 minify 成 blob，P4 条件渲染照 `can3Available` 先例。

**Tech Stack:** ESP-IDF 6 / Arduino 分量 / FreeRTOS / TWAI / SPIFFS / PSRAM heap_caps / Arduino WebServer（无 WebSocket，轮询）/ pytest token 契约测试。

**Spec:** `docs/superpowers/specs/2026-11-01-can-recorder-v2-design.md`（设计已批准 2026-11-01，计划从它推导，执行者两份都读）

## Global Constraints

- 宏隔离：`platformio.ini` 的 `[env:p4_3can]` 加 `-DDASH_RECORDER_V2`；全部 v2 代码在 `#if defined(DASH_RECORDER_V2)` 内；t2can 构建产物不得含 v2 符号
- 记录格式 16B packed（`static_assert(sizeof(RecRingFrame) == 16)`）：`uint32 tsUs`（`esp_timer_get_time()` 相对 µs）`uint32 id` `uint8 dlc` `uint8 flags`（bit0-1 bus 1/2/3，bit2-3 dir 0=R/1=T/2=E，bit4-7 预留零）`uint8 data[8]`
- 环形永远摄取三路全量（无 per-bus 门控）；bus 开关只是锁定 `bus_mask`
- 会话文件 `/sess_<seq>.bin` + `/sess_<seq>.meta.json`（SPIFFS 根）；seq 从 NVS 键 `rv2_seq` 单调递增，绝不重置
- meta 字段全集（Task 2 落盘时一次写齐）：`seq, boot_ms, wall(仅SNTP对上时才有此键), trigger:"manual", pre_ms_req, pre_ms_act, post_ms, bus_mask, shrink_reason(仅缩窗时), frames:{a,b,c,tx}, ring_overflow, ring_overflow_in_window(bool), twai_missed_a/b/c(锁定时读现有 diag 计数快照), last_error(仅失败会话)`
- TX 只挂 `mcpDashOnTxFrame`（mcp2515_dashboard.h:1247，函数内 1262 `dashRecordCanFrame(frame,'T')` 旁）；挂 `dashRecordCanFrame` 别处=双记
- 时间显示双钟：NVS/运行时 SNTP 对上才有真实时间；meta 同存 boot 相对；文件名只 seq
- 旧 `/rec_start`、`/rec_status`、`/rec_download`、`/rec_meta` 行为与产物字节不变
- UI 新标签必须进 i18n 字典（data-i18n 铁律）；commit message/release notes 纯英文
- 门禁顺序：pytest 契约（`/opt/homebrew/bin/python3.13 -m pytest`）→ `pio run -e p4_3can` → t2can 用 `IDF_MAINTAINER=1 pio run -e lilygo_t2can_dual`；除 UI 任务外，构建弄脏的 `mcp2515_dashboard_ui.h` 必须 `git checkout --` 还原
- 单写盘任务：同一时刻至多一个落盘任务存在（重复锁定 409）

## Review Focus

（spec 隐含但无任务测试覆盖、最可能咬人的五类输入——每行的测试已并入所属任务）

1. **tsUs 32 位回绕跨窗口**（71.6min 回绕时锁定的区间含回绕点）：无符号差值比较，不是绝对大小比较 → Task 1 Step 1 钉区间判断用 `(uint32)(a-b)` 语义 token
2. **锁定 post 窗口内再次锁定 / 落盘中再锁**：409 拒绝，不并发第二个写盘任务 → Task 2 测试钉 `recV2Writing` 守卫 token
3. **SPIFFS 写盘中途失败/满**：半文件不得留下冒充完整会话——写失败即删 bin、meta 不落、状态报错 → Task 2 测试钉失败清理路径
4. **bus_mask=0（三开关全关就锁定）**：400 拒绝，不让「按了啥也没有」的空会话产生 → Task 2 测试钉 mask 非零校验
5. **坏会话文件被下载**（bin 尾部截断/大小非 16 倍数）：下载前校验 `size % 16 == 0`，不整除 500 报错不吐坏数据 → Task 3 测试钉校验 token

---

### Task 1: 环形缓冲核心 + 三挂点接入

**Files:**
- Create: `include/web/dash_recorder_v2.h`
- Modify: `platformio.ini`（p4 env 加 `-DDASH_RECORDER_V2`）
- Modify: `include/web/mcp2515_dashboard.h`（三挂点各 ≤3 行 + 顶部 include，全宏内）
- Test: `test/test_dash_recorder_v2.py`（新契约文件，模式照 `test_dashboard_api_contract.py` 的源码 token 断言）

**Interfaces:**
- Produces（后续任务全靠这些名字）:
  - `void dashRecV2Init()` — setup 时分配 PSRAM 环（20MB 或降级 10MB），幂等
  - `void dashRecV2Submit(const CanFrame &f, uint8_t busTag, char dir)` — busTag 用 `kRecV2BusA=1/kRecV2BusB=2/kRecV2BusC=3`；dir 'R'/'T'/'E'
  - `uint32_t dashRecV2SpanUs()` — 环内最旧→最新时长（状态环显示用）
  - 内部静态：`ringHead/ringCount/ringOverflow`（volatile uint32）、`ring`（RecRingFrame 数组指针）
- 挂点契约：`mcpDashOnFrame` 内 submit(busA 或 busB——按 `f.bus` 掩码换算)、`mcpDashSniffCanC` 内 submit(busC)、`mcpDashOnTxFrame` 内 submit(ok?'T':'E')

- [ ] **Step 1: 写失败契约测试** `test/test_dash_recorder_v2.py`

类 `DashRecorderV2ContractTests`（读源文件字符串断言）。测试点：
- `test_ring_frame_is_16_bytes_and_packed`：`static_assert(sizeof(RecRingFrame) == 16` 存在；字段顺序 token（`uint32_t tsUs`、`uint8_t flags`）
- `test_submit_uses_unsigned_wraparound_safe_span`：span/区间判断出现 `(uint32_t)(` 差值形式；断言源码无 `ring[i].tsUs < oldest` 直接比较 token（负样例：`assertNotIn("tsUs < ", src)` 太脆，改钉必须存在 `now - tsUs` 无符号形式于 span 计算函数内）
- `test_psram_first_with_fallback`：`heap_caps_malloc` + `MALLOC_CAP_SPIRAM` 存在；失败降级路径存在（`heap_caps_malloc` 第二次无 SPIRAM 或 10MB 常量）
- `test_three_hooks_gated_by_macro`：dashboard 源内 `dashRecV2Submit` 出现恰好 3 次且每次都在 `#if defined(DASH_RECORDER_V2)` 后的行区间内（用行号区间断言：宏行号 < 三次 submit 行号 < 下一个 `#endif`——简化为 submit 计数==3 且 `DASH_RECORDER_V2` 在 dashboard 出现 ≥4 次）
- `test_tx_hook_is_single_path`：`mcpDashOnTxFrame` 函数体内有 submit；断言 `dashRecordCanFrame` 调用点数量与改动前一致（grep 计数 == 之前的数字，防止 TX 双记新增）

- [ ] **Step 2: 跑测试确认红** — `pytest test/test_dash_recorder_v2.py -q` → FAIL（文件不存在/断言失败）

- [ ] **Step 3: 实现 `dash_recorder_v2.h`**

结构 + init/submit/span。要点：submit 用短临界区 `portENTER_CRITICAL`（RX 单任务 + TX 低频，够用）；`ringCount` 满则 `ringOverflow++`；tsUs 在 submit 时取 `esp_timer_get_time()`。分配：`heap_caps_malloc(20MB, MALLOC_CAP_SPIRAM)` 失败→10MB→再失败置 `ring=null`（引擎停用，状态 API 报 disabled）。

- [ ] **Step 4: 挂点接入 + platformio.ini**

三处宏内挂点 + p4 env build_flags 加 `-DDASH_RECORDER_V2`。`dashRecV2Init()` 挂在 dashboard setup 路径（`SPIFFS.begin` 成功后，宏内一行）。

- [ ] **Step 5: 门禁** — pytest 绿 → `pio run -e p4_3can` 绿 → `IDF_MAINTAINER=1 pio run -e lilygo_t2can_dual` 绿 → `git checkout -- include/web/mcp2515_dashboard_ui.h` → `nm`/`strings` 抽查 t2can 产物无 `dashRecV2` 符号（`.pio/build/lilygo_t2can_dual/firmware.elf` 上 `~/.platformio/penv/bin/python -c "import subprocess; ..."` 或 `xtensa-esp32s3-elf-nm` 不可得时用 `strings | grep -c dashRecV2 == 0`）

- [ ] **Step 6: Commit** — `git add include/web/dash_recorder_v2.h platformio.ini include/web/mcp2515_dashboard.h test/test_dash_recorder_v2.py` + `feat(p4): always-on can ring buffer (recorder v2 core)`

### Task 2: 锁定状态机 + 会话落盘（缩窗/滚动删除）

**Files:**
- Modify: `include/web/dash_recorder_v2.h`
- Modify: `include/web/mcp2515_dashboard.h`（路由注册 2 行 + poll 挂点 1 行，宏内）
- Test: `test/test_dash_recorder_v2.py`（追加）

**Interfaces:**
- Consumes: Task 1 的 ring 内部结构
- Produces:
  - `bool dashRecV2LockRequest(uint32_t preMs, uint32_t postMs, uint8_t busMask, String &err)` — busMask bit0/1/2=A/B/C；全 0 → false(err)
  - `String dashRecV2StatusJson()` — `{enabled, span_s, fps:{a,b,c}, lock:{state:idle|arming|flushing, remain_ms, pre_ms, post_ms, bus_mask}, sessions_space_free}`
  - `void dashRecV2Loop()` — 每 loop 调：arming 到期→起一次性落盘任务；任务结束回 idle
  - 内部：`recV2Writing`（volatile bool）、写盘任务函数
  - 落盘算法（签名测不出来，钉死）：从 ringHead 逆序找 `tsUs >= now-preMs`（无符号差值）的起点→正序拷贝匹配 busMask 的帧→按 16B 直写 `/sess_<seq>.bin`；写完算 sha？不做（YAGNI），帧数进 meta
  - 缩窗：`预估 = 窗口内匹配帧数 × 16`（预扫描一遍 ring 数帧，不拷贝）；超可用空间→preMs 缩到最近能装下的边界（二分或按比例一次收缩，标记 `shrink_reason:"space"`）；可用空间 = SPIFFS free − 128KB reserve − 删最旧会话可回收量（仍不够才删，删到剩 1 个为止）
  - meta 落盘时机：bin 写成功之后；失败清理：`SPIFFS.remove(bin)` + 不写 meta + 状态 `last_error`

- [ ] **Step 1: 失败契约测试（追加）**

- `test_lock_request_validates_bus_mask`：`busMask == 0` 拒绝路径 token（`if (!busMask)` 或等价）+ 400 JSON 文案 token
- `test_single_writer_guard`：`recV2Writing` 存在；`dashRecV2LockRequest` 在 writing/arming 时返回 false 的守卫 token；handler 回 409 token
- `test_flush_failure_cleans_partial_file`：remove 调用出现在写失败路径（`recFile` 写失败分支内 `SPIFFS.remove`）
- `test_shrink_keeps_recent`：`shrink_reason` token + 预扫描计帧 token（`countWindow` 或等价名）+ `- 128 * 1024` reserve token
- `test_seq_nvs_monotonic`：`rv2_seq` NVS 键 token + `putUInt`/`getUInt` 对存在
- `test_wraparound_window_extraction`：落盘区间判断存在无符号差值形式（`(uint32_t)(now - ts)` 或同义）

- [ ] **Step 2: 红验证** — pytest 新增全 FAIL

- [ ] **Step 3: 实现锁定状态机与落盘任务**

`handleRingLock`（POST，参数 pre_s 60|180|300 默认 180、post_s 默认 30、bus_mask）、`handleRingStatus`（GET）路由注册进 `setupDashWebServer` 宏内区。落盘任务 `xTaskCreatePinnedToCore(..., 1)`（core 1，照 `dashGatewayStartDns` 的 xTaskCreatePinnedToCore 先例）。meta 字段照 Global Constraints 清单。`dashRecV2Loop()` 挂 dashboard 主 loop（宏内一行）。

- [ ] **Step 4: 门禁** — 同 Task 1 Step 5

- [ ] **Step 5: Commit** — `feat(p4): ring lock-to-session flush with space shrink`

### Task 3: 会话列表 / 下载（CSV|bin）/ 删除 API

**Files:**
- Modify: `include/web/dash_recorder_v2.h`（+路由 3 行宏内）
- Test: `test/test_dash_recorder_v2.py`（追加）

**Interfaces:**
- Produces:
  - `String dashRecV2SessionsJson()` — `[{seq,wall,boot_ms,pre_ms_act,post_ms,bus_mask,frames:{a,b,c,tx},size,loss_badge}]` 按新到旧
  - `handleSessions` GET `/sessions`；`handleSessionDownload` GET `/session_download?seq=&fmt=csv|bin`（chunked 流式照 `handleRecDownload` 先例 mcp2515_dashboard.h:4896）；`handleSessionDelete` POST `/session_delete {seq}`
  - CSV 列：`ts_us,bus,dir,id,dlc,b0..b7`（bus 用 1/2/3，dir R/T/E）

- [ ] **Step 1: 失败契约测试（追加）**

- `test_download_validates_frame_alignment`：下载路径有 `size % sizeof(RecRingFrame)` 校验 + 非 0 报错 token
- `test_csv_header_exact`：`ts_us,bus,dir,id,dlc,b0,b1,b2,b3,b4,b5,b6,b7` 字符串原样在源内
- `test_delete_removes_bin_and_meta`：`SPIFFS.remove` 对 bin+meta 两文件 token
- `test_sessions_json_newest_first`：列表构建逆序 token（`>= 0; i--` 或等价）

- [ ] **Step 2: 红验证** → **Step 3: 实现**（CSV 转换逐帧 `snprintf` 照 4925 行先例；bin 直接流式透传）→ **Step 4: 门禁** → **Step 5: Commit** `feat(p4): session list/download/delete api with csv export`

### Task 4: SNTP 双钟时间

**Files:**
- Modify: `include/web/dash_recorder_v2.h` + `mcp2515_dashboard.h`（STA connected 边沿挂点 ≤2 行宏内——`dashCheckWifi` 的 connected 分支，Task 已存在的 `dashGatewayOnStaConnected` 调用行旁）
- Test: `test/test_dash_recorder_v2.py`（追加）

**Interfaces:**
- Produces: `void dashRecV2TrySyncTime()`（幂等：成功过就跳过；`configTime(0,0,"pool.ntp.org","ntp.aliyun.com")` + getLocalTime 探测）；`bool dashRecV2HasWall()`；meta/UI 用它决定填 wall 还是空
- meta：wall 字段仅对上时才写，否则省略（不写假时间）

- [ ] **Step 1: 失败契约测试** — `test_sntp_sync_once_gated`：`configTime` token、幂等守卫 token（`wallSynced` 或等价）、挂点在 connected 分支 token；`test_meta_wall_optional`：wall 写入前有 `dashRecV2HasWall` 条件 token
- [ ] **Step 2: 红** → **Step 3: 实现** → **Step 4: 门禁** → **Step 5: Commit** `feat(p4): sntp wall clock with boot-relative fallback for sessions`

### Task 5: UI 三区改版 + capabilities + i18n + blob 再生

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`（录制页重写为新三区 + 旧面板收折叠；CAP 加 `recorderV2Available:false` + `hasCap` 分支）
- Modify: `include/web/mcp2515_dashboard.h`（dashCapabilitiesJson 加 `"recorderV2Available"` 字段，宏内 true/false）
- Modify: `scripts/minify_dashboard.py` 产出 → `include/web/mcp2515_dashboard_ui.h`（本任务的 ui.h 是**生成物并需 commit**，不 checkout）
- Test: `test/test_dash_recorder_v2.py`（追加 UI 契约）

**Interfaces:**
- Consumes: `/ring_status`、`/ring_lock`、`/sessions`、`/session_download`、`/session_delete`（Task 2/3 的 API）
- UI 结构（自上而下）：①状态环（span_s + 三路 fps）；②大操作区（三 bus 色块开关 A/B/C 预填 bus_mask、pre 分段 60s/3min/5min、锁定大按钮 ≥64px，arming 时变倒计时）；③会话列表（行=时间/时长/帧数/丢帧徽标 loss_badge，点开=CSV/bin 下载+删除）；「高级」折叠收纳旧表单面板（`<details>` 或等价折叠，不改旧面板内部逻辑）
- 轮询：状态环 `/ring_status` 2s 间隔（页面可见时）

- [ ] **Step 1: 失败契约测试（追加）**

- `test_capabilities_expose_recorder_v2`：dashboard 源 `recorderV2Available` token；UI src `CAP` 对象含 `recorderV2Available` + hasCap 分支 token
- `test_ui_has_three_zones`：src 内 `ring_status`、`ring_lock`、`sessions` fetch token 各 ≥1；`rec-v2-lock`（大按钮 id）与 `rec-v2-bus`（开关容器 id）token
- `test_ui_keeps_legacy_panel_folded`：旧面板容器仍在 src + 新折叠容器 token（`rec-advanced`）
- `test_i18n_new_labels_registered`：新 data-i18n 键（≥6 个：锁定/回溯/会话/下载CSV/下载BIN/删除）在 src 的 i18n 字典对象里都有键值（键名枚举进测试）
- `test_lock_button_polls_status`：`setInterval` + `ring_status` 同现 token

- [ ] **Step 2: 红验证** — pytest FAIL

- [ ] **Step 3: 改 UI src + capabilities + minify 再生 blob**

样式沿用现有设计语言（圆角紫青主题、卡片、data-i18n 双语）；移动优先（下半屏大触控、无 hex 输入）。运行 `python3 scripts/minify_dashboard.py`（如脚本带参数照文件头注释）生成 ui.h。

- [ ] **Step 4: 门禁 + 浏览器实测**

pytest 绿 → `pio run -e p4_3can` 绿 → 烧录 2 号板（单写 app@0x20000，esptool 入口=全局 penv）→ 手机/浏览器（板 AP 或 LAN）打开录制页核对三区渲染、旧面板折叠、i18n 双语切换、锁定按钮 arming 倒计时可见（无帧源时 span=0 属正常）。

- [ ] **Step 5: Commit** — `git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard.h include/web/mcp2515_dashboard_ui.h test/test_dash_recorder_v2.py` + `feat(p4): recorder v2 mobile-first capture ui`

### Task 6: 台架端到端验收 + bench 文档

**Files:**
- Modify: `docs/verification/p4-3can-bench-2026-11-01.md`（追加附录 D：v2 台架验收记录）
- Modify: `README.md` 仅当有需固化的使用说明（可选）

**Interfaces:** Consumes: 全部前任务的板载行为。

- [ ] **Step 1: 台架逐项验收（对照 spec 验收 1-5、7）**

无帧源时可验：①上电即录状态可见；④缩窗（先造占位文件耗 SPIFFS 或直接锁大窗口预扫逻辑——若无帧则跳过，标「待实车」）；⑤单手操作流程；⑦门禁全绿。锁 60s 空窗口→会话产生（frames=0 也应有会话与 meta，验证链路）。串口 DIAG 可加 `rv2_status` 打印（可选，若加则并入本任务 commit）。
- [ ] **Step 2: 实车验收清单写进 bench 文档**（帧数一致性、丢帧徽标、CanC-only 会话——交 Jordan 上车执行）
- [ ] **Step 3: Commit** — `docs(p4): recorder v2 bench acceptance and field checklist`

---

**执行备注（给执行者）：**
- 每任务 commit 前自查：`git status` 只含本任务文件；`mcp2515_dashboard_ui.h` 除 Task 5 外必须 checkout 还原
- 串口设备：`/dev/cu.usbmodem5CF71553661`（2 号板）；esptool：`~/.platformio/penv/bin/esptool --chip esp32-p4 ... write_flash 0x20000 .pio/build/p4_3can/firmware.bin`
- 板可能连着 MyTeslaWiFi（LAN 不可达）——验收走串口或让板回 JordanAP
- 凭据/ profile 文件绝不入库；push 前必须问 Jordan
