# FSD 截图与公开源代码功能合并开发计划

适用项目：LILYGO T-2CAN Tesla Model Y FSD CAN Dashboard  
当前本地版本：v4.01  
参考资料：

- 截图目录：`/Users/ziwind/Desktop/FSD截图`
- 公开源代码：`/Users/ziwind/Codex/DouyinFSD /源代码与抓包数据/公开源代码.txt`
- 本地 UI：`include/web/mcp2515_dashboard_ui.src.h`
- 本地后端：`include/web/mcp2515_dashboard.h`
- 本地双 CAN 入口：`src/main.cpp`

## 1. 总体结论

当前本地项目已经完成了截图中大部分“页面结构”和“配置契约”的第一轮重构，包括：

- 模块配置
- 激活模式
- 驾驶模式
- 速度偏移
- CAN2控制
- FSD防御
- OTA升级
- 网络设置 / DNS 黑白名单
- CAN工具
- 手机端底部导航和更多菜单
- v4.01 固件版本注入

但是，公开源代码中还有几类功能没有被本地项目完整实现：

- 自动关机 / WiFi 自动关闭。
- 自动换挡页面只做了状态占位，本地没有执行换挡 CAN 控制。
- 灯光特技目前本地只做到 CAN2 stalk test 和配置保存，没有完整移植自动爆闪、远光连续爆闪、后雾灯 Strobe / Pilot 引擎。
- FSD防御里本地有配置项和部分 handler 字段，但没有完整移植公开源代码里的 0x370 仿生方向盘扭矩、0x3C2 声音/速度加减序列。
- 公开源代码的 CAN B 错误诊断弹窗比较详细，本地只显示基础 EFLG / RX / TX 错误。
- 公开源代码的网络状态包含外部中转连接文案和 net_rx/net_tx 展示，本地已有网关状态但 UI 表达还不完全一致。

合并策略应当是“保留本地已实测成功的 API 和 CAN 主逻辑，只把缺失功能按模块迁移”，不能直接整段复制公开源代码。

## 2. 为什么不能直接复制源代码

公开源代码是 Arduino 风格单文件实现，使用：

- `ESPAsyncWebServer`
- `Preferences`
- `MCP2515 can_mcp2515`
- 全局 `volatile` 状态
- `/api/cmd?type=...&val=...` 单入口命令模型
- 大量直接 CAN 帧改写和注入

本地项目当前结构是：

- ESP-IDF / PlatformIO 构建。
- 本地同步 WebServer API。
- `include/web/mcp2515_dashboard.h` 负责 dashboard API、NVS、Web 后端。
- `include/handlers.h` 负责 Legacy / HW3 / HW4 CAN handler。
- `src/main.cpp` 负责 LILYGO T-2CAN 双 CAN、CAN2 MCP2515、stalk test、service mode。
- 当前已经有 `/mode_hw`、`/drive_profile`、`/speed_strategy`、`/lighting_config`、`/defense_config`、`/gear_assist_status`、`/hotspot_config`、`/relay_wifi_test`、`/dns_rules` 等结构化 API。

因此合并原则是：

1. UI 只调用本地已有或新增的结构化 API，不恢复 `/api/cmd` 大杂烩接口。
2. 后端保留当前 NVS key、handler 状态和双 CAN 分层。
3. CAN 注入类功能必须默认关闭，并标记“未验证 / 实验”。
4. 每个功能保持三层状态：UI 显示状态、NVS 持久化状态、实际运行状态。
5. 实车未验证前，只允许做配置保存、状态展示、短时测试，不允许默认启用持续注入。

## 3. 截图功能对照矩阵

| 截图页面 | 截图功能 | 本地现状 | 差距 | 建议优先级 |
|---|---|---|---|---|
| 模块配置 | CAN fps、已开启、FSD防封保护、运行时间 | 已有顶部状态栏和模块状态 | CAN2 错误诊断文案不够细 | P2 |
| 模块配置 | FSD-V14 模式 | 已映射为驾驶 MAX/V14 相关 UI | 需要明确独立 NVS 状态与运行状态 | P1 |
| 模块配置 | 模块总闸 / FSD 破解启用 | 已有模块总开关和 FSD 总开关 | 需继续确保互不联动 | P0 已处理，需回归 |
| 模块配置 | 自动关机 | 本地未完整实现 | 缺少 5 分钟无 CAN 数据自动关闭模块 | P2 |
| 模块配置 | 关闭 WiFi | 本地未完整实现 | 缺少网页 5 分钟无操作后 WiFi 降温关闭 | P2 |
| 激活模式 | Auto / HW3.0 / HW4.0 | 本地已有 Auto / legacy / HW3 / HW4 | 截图无 legacy，本地保留 legacy 合理 | P0 已有 |
| 驾驶模式 | Auto / Sloth / Chill / Normal / Hurry / MAX | 本地已有 `/drive_profile` 六档 | 需要实车验证每档映射 | P1 |
| 速度偏移 | fixed / auto / custom | 本地已有 `/speed_strategy` | 截图固定百分比按钮与四区间自定义 UI 还不完全一致 | P1 |
| 速度偏移 | 当前底盘实时生效偏移 | 本地显示 `soff` / fused speed | 应增加“当前实际偏移百分比 / 原始值”说明 | P2 |
| 灯光特技 | 单次拨动触发连闪 | 本地只有手动 stalk test 和 strobe sequence | 缺少监听原车 0x3C2 触发自动连闪 | P2，实车验证后 |
| 灯光特技 | 3/5/7/10 下 | 本地已有 count | 可保留 | P0 已有 |
| 灯光特技 | 慢速/中速/极速 | 本地已有 slow/medium/fast | 与公开源代码 on/off ms 可进一步对齐 | P1 |
| 灯光特技 | 后雾灯无限爆闪 / Pilot | 本地有 rear_fog_strategy 配置 | 后端未真正注入后雾灯策略 | P2，危险实验 |
| FSD防御 | 防御总开关 | 本地已有 `/defense_config.enabled` | 需明确运行状态 | P1 |
| FSD防御 | 仿生方向盘 | 本地有 UI/NVS 字段 | 未完整实现 0x370 扭矩波形注入 | P3，高风险 |
| FSD防御 | 声音警告抑制 | 本地映射 `isaChimeSuppress` | 与公开源代码 0x3C2 音量加减不是同一能力 | P2 |
| FSD防御 | 速度加减免打扰 | 本地有 `speed_no_disturb` 配置 | 未实现 0x3C2 速度加减序列 | P3，高风险 |
| FSD防御 | AP/EAP 兼容 | 本地有配置字段 | 未完整实现公开源代码的 0x249 物理拨杆模式切换 | P2 |
| OTA升级 | 上传 .bin | 本地已有 OTA | 可补充进度/错误文案 | P2 |
| 黑名单 | 黑名单/白名单保存 | 本地已有网络页面合并 DNS 黑白名单 | 基本完成 | P0 |
| 自动换挡 | 速度、刹车、挡位状态 | 本地 `/gear_assist_status` 只返回 unavailable | 状态展示可做，执行控制不能贸然实现 | P2 状态，P4 控制 |
| 网络设置 | AP SSID / 密码 / 外部中转 WiFi / 测试连接 | 本地已有 hotspot_config、wifi_config、relay_wifi_test | UI 仍可更贴近截图，但后端已覆盖 | P1 |

## 4. 公开源代码可合并功能分组

### 4.1 可以直接按契约合并的功能

这些功能主要是状态、配置或 UI 行为，风险较低：

- 自动关机配置项。
- WiFi 自动关闭配置项。
- 网络状态文案增强。
- CAN2 错误诊断弹窗。
- 速度偏移截图式固定百分比按钮。
- 自定义四区间百分比 UI。
- OTA 上传错误文案优化。
- 黑名单 / 白名单“写入并部署”反馈。

### 4.2 需要适配后才能合并的功能

这些功能有后端逻辑，但必须改成本地架构：

- 源代码 `/api/cmd?type=...` 指令需要拆成结构化 API。
- 源代码 `Preferences` key 需要映射到本地 `dashSavePrefs()` / `dashLoadPrefs()`。
- 源代码 `ESPAsyncWebServer` 回调需要改成本地 `server.on()` handler。
- 源代码 DNS 黑白名单要继续复用本地 `/gateway_dns` 和 `/dns_rules`。
- 源代码 WiFi 保存重启要复用本地 `/hotspot_config`、`/wifi_config`、`/wifi_connect`。

### 4.3 只能作为实验功能合并的功能

这些功能涉及车辆行为或驾驶辅助提示，不应默认启用：

- 0x370 仿生方向盘扭矩注入。
- 0x3C2 音量加减。
- 0x3C2 速度加减。
- 后雾灯无限爆闪。
- 远光灯无限爆闪。
- AP/EAP 免打扰物理拨杆模式。
- 自动换挡控制。

这类功能必须满足：

- 默认关闭。
- UI 明确标记“未验证 / 实验”。
- NVS 保存和运行状态分离。
- 危险动作二次确认。
- 真车验证记录 CAN ID、数据、车速、挡位、刹车状态。

## 5. 建议开发阶段

### 阶段 0：冻结当前已实测成果

目标：

- 给当前 v4.01 打一个本地发布资产快照。
- 保存 full merged、OTA app、SHA256。
- 记录当前可用功能。

验收：

- `platformio run -e lilygo_t2can_dual` 成功。
- `test/test_dashboard_api_contract.py` 成功。
- v4.01 控制面板功能不回退。

### 阶段 1：补齐低风险 UI 与状态

目标：

- 增加自动关机配置项。
- 增加 WiFi 自动关闭配置项。
- 增强 CAN2 EFLG 诊断弹窗。
- 增强网络状态文案。
- 速度偏移页面增加固定百分比按钮和四区间自定义说明。

不做：

- 不增加新的 CAN 注入。
- 不修改 handler 主路径。

验收：

- UI 按钮可操作。
- NVS 可保存。
- 重启后状态恢复。
- API 返回 JSON 正常。

### 阶段 2：补齐截图功能契约

目标：

新增或扩展 API：

- `/power_policy`：自动关机、WiFi 自动关闭、超时时间。
- `/can2_diagnostics`：EFLG 解码、CAN2 错误原因、建议检查项。
- `/speed_offset_config`：fixed percent、custom buckets、effective offset。
- `/lighting_runtime`：区分配置状态与实际 CAN2 运行状态。
- `/defense_runtime`：区分配置状态、handler 支持状态、实际触发状态。

验收：

- UI 不直接猜测状态。
- 所有页面都能显示 UI/NVS/运行三层状态。
- API 合约测试覆盖新增字段。

### 阶段 3：灯光特技安全合并

目标：

- 在本地 `src/main.cpp` 的 CAN2 层增加独立 strobe engine。
- 优先只支持手动短时序列。
- 后雾灯 strobe / pilot 先只保存配置，不自动执行。
- 自动拨动触发连闪只在捕获到稳定 0x3C2 真实帧后启用。

验收：

- 默认关闭。
- 需要二次确认。
- CAN2 离线时不可执行。
- 每次执行写入日志和 Last Write Check。

### 阶段 4：FSD防御增强项审查合并

目标：

- 明确本地已有 `isaChimeSuppress`、`banShield`、`hw3OffsetSlew` 与公开源代码的功能差异。
- 如果要移植 0x370 仿生方向盘，先做只读检测和模拟日志，不直接注入。
- 如果要移植 0x3C2 音量/速度加减，先做手动单次测试 API。

验收：

- 不默认执行扭矩/声音/速度注入。
- 所有实验项均显示“未验证”。
- 真车验证前只允许开发者模式打开。

### 阶段 5：自动换挡只做状态，不做控制

目标：

- 完善 `/gear_assist_status` 的速度、刹车、挡位展示。
- UI 保留截图中的自动换挡页面，但明确“储备功能，未开放”。
- 不实现自动切换前进/倒车挡。

原因：

自动换挡属于高风险车辆控制，公开源代码本身也说明“功能代码暂未部署”。本地项目应先保留状态页面，不应贸然实现控制。

## 6. 优先级建议

P0 已完成或必须保持不变：

- v4.01 已实测基础成果。
- 模块总开关与 FSD-V14 独立。
- 网络设置和黑名单合并。
- CAN1 / CAN2 命名。
- CAN2控制 导航命名。

P1 建议下一轮实现：

- 自动关机 / WiFi 自动关闭配置。
- 网络设置 UI 完整对齐截图。
- 速度偏移 fixed/auto/custom UI 细化。
- CAN2 EFLG 诊断弹窗。
- 防御状态三层化。

P2 实车验证后实现：

- 自动拨动触发连闪。
- 后雾灯 strobe / pilot。
- AP/EAP 兼容运行逻辑。
- 0x3C2 声音加减单次测试。

P3 暂不建议实现：

- 仿生方向盘扭矩注入。
- 速度加减免打扰自动触发。
- 自动换挡控制。

## 7. 合并审查建议

1. 不要恢复公开源代码的红色主题作为主视觉，本地已按用户要求恢复紫色现代风格。
2. 不要把 `/api/cmd` 作为兼容入口长期保留；如果要支持旧 UI，可做只读迁移层，但内部仍调用结构化 API。
3. 所有新增 NVS key 使用本地命名空间，不使用源代码 `strobe_cfg` 全量 key，避免与当前 NVS 混乱。
4. CAN 注入类功能必须统一走本地 `dashLog()`、`dashRecordCanFrame()`、Last Write Check。
5. 所有危险操作必须二次确认，并且 CAN 离线时返回明确错误。
6. 对源代码里的 CAN 帧位修改，必须先写入注释、测试样例和抓包出处，不能只按魔法数字移植。
7. 每一阶段都要生成 OTA app 和 full merged 资产，保留可回退版本。

## 8. 需要人工确认的问题

1. 是否仍然要求保留当前品牌 `Atlas`，还是截图中的“现代生存指南”只作为功能参考。
2. 自动关机是否只关闭模块输出，还是进入 ESP 深睡眠。
3. WiFi 自动关闭后，是否需要通过 CAN 活动自动恢复热点。
4. 后雾灯 strobe / pilot 是否有已验证 CAN ID 和 checksum 抓包。
5. 自动换挡是否只做状态展示，还是未来确实要进入控制开发。
6. AP/EAP 兼容模式的实车验证车型、年份、HW 版本。

