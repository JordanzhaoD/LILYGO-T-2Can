# Blackbox Recorder（裸分区黑匣子）+ STA Panic 修复 — 设计文档

**日期**：2026-11-01（会话 3）
**状态**：待 Jordan 批准
**前置**：取代 `2026-11-01-can-recorder-v2-design.md` 中的 SPIFFS 落盘引擎（会话 1 spec）；UI/API 面承继不变。
**实车证据**：`resets=[[4,0],[4,0],[4,1]×5]` 全 PANIC；coredump 栈 `hosted_memcpy(src=NULL) ← rpc_wifi_sta_get_ap_info ← WiFiClass::status ← dashCheckWifi ← webTask`。

---

## 一、意图与为什么

实车录制 100% 失败的根因已由 coredump 定案：**esp_hosted 2.12.13 上游 bug**——STA 未关联时 `rpc_wifi_sta_get_ap_info` 返回"SUCCESS + NULL 载荷"，`hosted_memcpy` 对 NULL 断言 → 整板 PANIC。我们的 `WiFiClass::status()`（espidf_runtime.cpp:856）被 `dashCheckWifi` **每秒巡检**调用，STA 掉线瞬间必踩。车内 MyTeslaWiFi 边缘信号 + 长时间 flush 把暴露窗口拉满 → "锁定即重启"。另 2 次非 flush PANIC = 行车随机重启，同一颗雷。

次要痛点：SPIFFS 落盘 8KB/s，完整窗口最长 25 分钟——既是煎熬也是暴露窗口放大器。

Jordan 定夺：**A（panic 修复）+ B2（裸分区黑匣子）**，滚动窗口 5 分钟足够（实测 ~750fps 聚合 → 5min ≈ 4.1MB；给足冗余到 9MB）。

## 二、目标

1. **A**：STA 断开时不再调用 `esp_wifi_sta_get_ap_info`（事件门控），车内两类重启根除
2. **B2**：录制数据流式写入专用裸 flash 分区，边录边写；**锁定 = 瞬间**（只写 meta，零数据拷贝）
3. 锁定后的会话数据**原地保护**，直到下载/删除（不会被滚动覆写）
4. UI 与 API 面尽量不变（三区 UI、锁定按钮、会话列表、bin/CSV 下载、删除）
5. 删除 SPIFFS 大文件落盘路径 → 本周一整族 bug（GC 颠簸/预留/收缩/reclaim/孤儿）连根消失

## 三、非目标 / 反目标

- **非目标**：TX 行为、CanC UI、T2CAN 任何变化（宏门控 + 0 符号隔离不变）；PCAP 导出（仍 P2）；ID 白名单过滤（P2 甜点，B2 落地后按需加）
- **反目标**：任何"锁定后还要等落盘"的体验；静默丢弃已锁定的证据；OTA 流程改变（本次升级需重刷分区表属一次性动作）

## 四、方案权衡（已裁）

| 项 | 选项 | 判决 |
|----|------|------|
| A 手段 | shim 事件门控 7 调用点 vs 给 esp_hosted 打补丁 | **门控**。managed_components 不入库、版本钉死 2.12.13，本地补丁不可复现；门控关闭唯一已观测触发路径（1s 巡检），残窗=SDIO 事件在途的毫秒级 vs 1s 轮询，可忽略。上游报告挂 P2 |
| B 落盘 | B1 白名单轻锁 / B2 裸分区 / B3 SPIFFS 流式 | **B2**。B3 被物理否决（真实 13.6KB/s > SPIFFS 可持续 8KB/s 必丢帧）；B1 只治标（仍是大写入） |
| B2 数据保全 | 锁后拷贝到 SPIFFS vs 原地保护位图 | **保护位图**。拷贝比录入慢（8<13.6KB/s）必有竞态丢尾；位图=零拷贝、锁定零耗时 |
| 空间满策略 | 自动丢最旧 vs 拒绝新锁定 | **拒绝 + UI 提示**（"空间不足，请下载/删除旧会话"）。静默丢证据不可接受 |

## 五、A 设计：STA 关联门控

- `src/espidf_runtime.cpp`：`static volatile bool wifiStaAssociated = false;` 由既有 `wifiEventHandler`（espidf_runtime.cpp:111-124）维护：`WIFI_EVENT_STA_CONNECTED` → true；`WIFI_EVENT_STA_DISCONNECTED` → false；初始化/begin 前保持 false
- 新增内部 helper：`bool espWifiStaApInfoSafe(wifi_ap_record_t *out)`——`wifiStaAssociated == false` 时直接 return false（**不发 RPC**），否则调真 API 并回传成败
- 替换全部 7 个调用点：espidf_runtime.cpp:834/856/962、dash_gateway.h:1444、mcp2515_dashboard.h:5512/6177/6554
- 语义：断开时 `status()` 返回缓存 `wifiStaStatus`（不判 WL_CONNECTED）、`SSID()` 返回 ""、RSSI/gateway 状态按"无信息"分支——与台架断网时既有降级路径一致
- 全部仍限 P4 宏内（T2CAN 走原生命令路径，不走这些 RPC 包装，零变化）

## 六、B2 设计：裸分区黑匣子

### 6.1 分区重划（仅 32MB 表；T2CAN 各表不动）

```
nvs 0x9000/64K | otadata 0x19000/8K | app0 0x20000/8M | app1 0x820000/8M
spiffs 0x1020000/0x6C0000(6.75M) | blackbox 0x16E0000/0x900000(9M) | coredump 0x1FE0000/128K
```

- blackbox：`data, 0x40`（自定义 subtype），64KB 擦除块 × 144
- 容量判据：9MB ÷ 18B ≈ 52 万帧；@750fps 滚动 ~11.5min（>5min 要求，@1740fps 仍保 5min）；磨损：~5 块循环/驾驶小时 → 10 万次寿命 ≈ 上万小时，非问题
- SPIFFS 剩 6.75M：只存 meta JSON + legacy CSV（约可放 4-5 段 legacy 录制）

### 6.2 写入路径（替代 PSRAM 环 + flush 任务）

- `dashRecV2Submit()` 入口签名不变；帧格式不变（18B packed RecRingFrame）
- 新 sink：**双缓冲** 2×64KB staging（`portENTER_CRITICAL` 单临界区写入，同今日环插入语义）；缓冲满或 250ms 到期 → 交给写任务；写任务 erase-ahead（先擦下一块）顺序编程
- 溢出计数保留 → loss_badge 语义不变（staging 满才可能丢，flash 100ms 级擦除远快于填充）
- 删除：recV2FlushTask、reclaim、shrink、kRecV2SpaceReserve、环形缓冲本体
- 写指针持久化：NVS，每 8 块 + 每次 lock 时写；boot 从指针起 erase-ahead 重同步（陈旧指针最坏多擦几块，滚动数据无价值损失）

### 6.3 锁定语义（核心收益）

1. `POST /ring_lock {pre_s, post_s, bus_mask}`（API 不变）→ state=arming，记当前写偏移与 `t_cut = now − pre_s`（钳到本 boot 最老帧时刻 → `pre_ms_act` 语义保留）
2. 等 post_s → **对覆盖 `[t_cut, now]` 的 64KB 块置保护位**（块级；块内混有窗口外/其它总线的帧属正常，导出时按帧过滤）→ meta JSON 落 SPIFFS → state=idle。**全程零数据拷贝、亚秒完成**
3. **全局帧栅格**（建造者审查裁定）：分区头之后是连续 18B 帧数组，帧 k 恒在 `headerSize + 18k` —— 跨块连续、不按块对齐（块只是擦除单元）。任意偏移可解码
4. **按块 RAM 摘要**（审查裁定）：每块关闭时更新 `{firstTs, lastTs, countA/B/C, txCount}`（144 块 × ~24B ≈ 3.5KB RAM，仅本 boot）。锁定时对窗口内块求和 → **frames.a/b/c/tx 精确、亚秒**；`start/end_boot_us` 取窗口首尾帧 ts；`loss_badge` 取窗口内 staging 溢出计数。不扫 flash、无持久化块索引
5. 窗口语义：导出时**按帧时间戳过滤**（`ts ≥ t_cut` 且 bus ∈ mask）——与今日"帧精确、bus_mask 过滤、size % 18 == 0"的 bin 语义逐字节一致；`pre_ms_act = now − t_cut`
6. 滚动窗口**boot 即重置**（与今日 PSRAM 环开机从零同语义）；NVS 写指针仅为磨损连续性服务，不承担跨 boot 历史
7. 保护位图：144 bit，**boot 时扫 SPIFFS metas 重建**（meta=事实源，崩溃安全：崩溃在 meta 前锁未成立，无会话，与今日语义一致）
8. 写指针遇保护块 → 跳到下一未保护块；无未保护块可用 → **停止写入**（录制暂停、fps 置 0、状态提示），不覆盖证据、不静默丢弃
9. 删除会话 = 清保护位 + 删 meta（显式；下载不自动删）

### 6.4 下载与会话 API

- `GET /sessions` 列表（meta 源）字段兼容；`size` = 导出帧数 × 18（精确）
- `.../sess_N/bin`：HTTP 分块流读分区范围，**逐帧按 `ts ≥ t_cut` + bus_mask 过滤后输出**——产物与今日 bin 逐字节同义（读速 MB/s 级，5min 会话秒级下载）
- `.../sess_N/csv`：同一过滤路径的板端流式转码（4KB 读块→CSV 行→chunked 响应；纯读无 GC，不再有 SPIFFS 写墙）
- 主机端解码工具：`scripts/decode_blackbox.py`（bin → 今日同列 CSV，含 magic/版本头自校验），入库

### 6.5 UI / 状态面

- 三区 UI 不动；锁按钮流程不变；state 机 `idle→arming→idle`（**flushing 态消失**）
- `/ring_status` 字段兼容：`span_s`=当前滚动窗；`sessions_space_free`=未保护块字节；`seq`、`resets`（重启台账保留——它刚立了大功）不变

### 6.6 升级烧录程序（一次性，建造者审查加固）

分区表变更 → 本次烧录 = **表@0x8000 + 擦 otadata@0x19000 + app@0x20000**（otadata 擦除为强制步：任何曾走过 OTA 的板 otadata 可能指向 app1，新表下跑旧 app = SPIFFS 尺寸错配被格式化 + 记录器找无分区，症状像 bug 实为烧录组合错误）。烧前 `read_flash 0x8000` 回验旧表、烧后回验新表。适用范围：本周期只动 2 号板；任何后续刷 beta11+ p4_3can 的板（含 3 号板）都必须走此链。板上 SPIFFS 现为空（会话已全失）无损失；此后升级恢复单写 app/OTA。版本号 ship 时 bump beta11 + CHANGELOG（英文）。

## 七、验证计划

1. **门禁**：pytest（新引擎单测：wrap/保护/满拒/位图重建/meta schema + A 门控 7 点 pin + T2CAN 0 符号）→ pio 双板构建
2. **台架 A 复现判决**：烧录后主动断 STA（DIAG/HTTP 触发）+ 洪水 + 1s 巡检 → 修复前 1s 内 PANIC、修复后干净降级、AP 存活、台账零新增
3. **台架 B2 端到端**：flood 6000fps×90s → lock pre=60s → 亚秒完成 → bin 下载帧数逐字节核对 → 解码 CSV → 连续两次 lock 验证保护与满拒 → 重启验证位图重建
4. **实车清单**：录制→锁定→（当晚）下载；期间观察 STA 掉线不再重启；`resets` 台账保持干净

## 八、风险与对策

- 64KB 擦除的 ~100ms cache 停顿 vs CAN/网络：远低于 R4 饿死阈值（5s），台架 3 验证
- 分区表烧录风险：沿用首烧四段链纪律，烧前 `read_flash 0x8000` 回验
- esp_hosted 其它 RPC 同型 NULL 坑（未观测）：P2 审计 `_h_memcpy` 全量调用面
- 写指针 NVS 磨损：8 块一次 ≈ 1 次/分钟级，NVS 页级均摊，非问题

## 九、留给建造者

> 建造者审查（2026-11-01）已过：A 部分零疑问；B2 三问（bin 语义/亚秒锁定机制/烧录链）已在 §6.3/6.4/6.6 裁定。

- RecRingFrame 打包 static_assert（分区格式钉死）；分区头 magic/版本（帧栅格锚点 `headerSize`）
- erase-ahead 与保护位跳块的合并实现（块状态机：erased/writing/protected）
- 按块摘要的写入时机（块关闭 + staging 部分块的实时累计）
- chunked 下载与流式 CSV 的 HTTP 细节（现有 WebServer 能力边界；bin 过滤路径的每帧成本核算）
- meta 兼容字段最终表；`/ring_status` 各字段在新语义下的精确定义（`sessions_space_free`=未保护块字节）
- pytest 文件组织（新 test_blackbox.py + 保留 API 面 pin；test_dash_recorder_v2.py 中 flush/reclaim 相关用例迁移或删除）
- decode 脚本与 bin 帧校验
- 黑匣子分区缺失时的优雅降级（如 spark 构建烧到旧表：记录器禁用 + 状态页提示，不崩）
