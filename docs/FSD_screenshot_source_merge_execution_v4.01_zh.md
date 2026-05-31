# FSD 截图功能与公开源代码合并执行文档

当前版本：v4.01  
执行目标：把截图和公开源代码中“本地尚未实现但可安全落地”的功能合并到本地项目，同时不破坏已实测成功成果。

## 1. 执行原则

### 1.1 保留当前成果

以下内容不得破坏：

- 当前 `lilygo_t2can_dual` 构建环境。
- 当前 v4.01 UI 页面结构。
- 已修复的模块总开关与 FSD-V14 独立逻辑。
- 网络设置与黑名单合并成果。
- CAN1 / CAN2 命名。
- CAN2控制 导航命名。
- 现有 `/mode_hw`、`/drive_profile`、`/speed_strategy`、`/lighting_config`、`/defense_config`、`/gear_assist_status`、`/hotspot_config`、`/relay_wifi_test`、`/dns_rules` API。

### 1.2 禁止直接复制

公开源代码不能整段复制到本地，原因：

- Web server 框架不同。
- CAN driver 层不同。
- NVS key 和状态模型不同。
- 公开源代码把大量功能集中到 `/api/cmd`，本地项目已经拆成结构化 API。
- 公开源代码中有高风险 CAN 注入逻辑，必须逐项验证。

### 1.3 每个功能必须三层状态

每个新增功能都要能展示：

- UI 显示状态。
- NVS 持久化状态。
- 实际 CAN / 网络运行状态。

## 2. 当前本地 API 对照

| API | 当前用途 | 可复用程度 |
|---|---|---|
| `/status` | 主状态、CAN、FSD、速度、实验项 | 高 |
| `/system_status` | 固件版本、设备信息 | 高 |
| `/config` | 旧配置入口，仍被部分 UI 使用 | 中 |
| `/mode_hw` | Auto / legacy / HW3 / HW4 | 高 |
| `/drive_profile` | Auto / Sloth / Chill / Normal / Hurry / MAX | 高 |
| `/speed_strategy` | fixed / auto / custom | 高 |
| `/lighting_config` | 灯光配置保存 | 中 |
| `/defense_config` | 防御配置保存 | 中 |
| `/gear_assist_status` | 自动换挡状态占位 | 低 |
| `/hotspot_config` | AP SSID / 密码 / 隐藏 / 保存重启 | 高 |
| `/relay_wifi_test` | 外部中转 WiFi 测试 | 中 |
| `/dns_rules` | 独立黑白名单 API，复用 gateway DNS | 高 |
| `/gateway_dns` | DNS 黑白名单底层实现 | 高 |
| `/service_mode` | CAN2 0x339 service mode | 高 |
| `/stalk_test` | CAN2 0x249 PULL / PUSH 短时注入 | 中 |
| `/bus2_ids` | CAN2 ID 发现列表 | 高 |
| `/frames` | CAN Sniffer | 高 |
| `/rec_start` `/rec_stop` `/rec_download` | CAN Recorder | 高 |

## 3. 源代码指令迁移表

公开源代码统一使用 `/api/cmd?type=...`。本地应迁移为结构化 API。

| 源代码指令 | 源代码含义 | 本地映射 | 处理方式 |
|---|---|---|---|
| `tgl_mod` | 模块总闸 | `/config?can=` 或新增 `/module_config` | 保留独立开关 |
| `tgl_fsd` | FSD 破解启用 | 当前 FSD 总开关 / canActive | 继续复用 |
| `tgl_v14` | FSD-V14 模式 | `/drive_profile` MAX 或独立字段 | 建议新增独立 v14 字段 |
| `tgl_ap` | 自动关机 | 新增 `/power_policy` | 低风险合并 |
| `tgl_wifi_auto` | 5 分钟无操作关闭 WiFi | 新增 `/power_policy` | 低风险合并 |
| `hw` | Auto/HW3/HW4 | `/mode_hw` | 已有 |
| `set_profile` | 驾驶模式 | `/drive_profile` | 已有 |
| `offset_mode` | fixed/auto/custom | `/speed_strategy` | 已有 |
| `offset` | 固定百分比 | 扩展 `/speed_offset_config` | 需要新增 |
| `custom` | 四区间自定义 | 扩展 `/speed_offset_config` | 需要新增 |
| `tgl_s_auto` | 拨杆触发连闪 | 扩展 `/lighting_config` | 配置先行，运行后置 |
| `s_cnt` | 连闪次数 | `/lighting_config.count` | 已有 |
| `s_on` / `s_off` | 亮灭时间 | 扩展 `/lighting_config` | 需要新增字段 |
| `tgl_high_fog_strobe` | 后雾灯无限爆闪 | `/lighting_config.rear_fog_strategy=strobe` | 配置已有，运行未实现 |
| `tgl_high_fog_pilot` | F1 pilot 节奏 | 扩展 rear_fog_strategy=pilot | 需要新增枚举 |
| `tgl_s_cont` | 远光连续爆闪 | 扩展 `/lighting_config` | 高风险，默认关闭 |
| `tgl_dnd_en` | 防御总开关 | `/defense_config.enabled` | 已有 |
| `tgl_dnd_steer` | 仿生方向盘 | `/defense_config.bionic_steering` | UI/NVS 已有，运行未实现 |
| `tgl_dnd_vol` | 声音警告抑制 | `/defense_config.sound_warning_suppression` | 部分已有 |
| `tgl_dnd_speed` | 速度加减免打扰 | `/defense_config.speed_no_disturb` | UI/NVS 已有，运行未实现 |
| `tgl_ap_dnd` | AP/EAP 兼容 | `/defense_config.ap_eap_compatible` | UI/NVS 已有，运行未实现 |
| `save_dns` | 黑白名单保存 | `/dns_rules` 或 `/gateway_dns` | 已有 |
| `reboot_now` | 重启设备 | `/reboot` | 已有 |
| `force_save` | P 挡安全保存 | 可选新增 | 暂不必要 |

## 4. 第一批执行任务：低风险功能

### 4.1 自动关机 / WiFi 自动关闭

新增后端状态：

- `dashAutoShutdownEnabled`
- `dashWifiAutoOffEnabled`
- `dashAutoShutdownTimeoutSec`
- `dashWifiAutoOffTimeoutSec`
- `dashLastCanActivityMs`
- `dashLastWebActivityMs`

新增 API：

```text
GET  /power_policy
POST /power_policy
```

返回示例：

```json
{
  "ok": true,
  "auto_shutdown": false,
  "wifi_auto_off": false,
  "auto_shutdown_timeout_sec": 300,
  "wifi_auto_off_timeout_sec": 300,
  "can_idle_sec": 12,
  "web_idle_sec": 4,
  "runtime_action": "none"
}
```

UI 修改：

- 模块配置页增加“自动关机”。
- 模块配置页增加“关闭 WiFi”。
- 手机首页只显示状态，不放危险开关。

验收：

- 开关保存到 NVS。
- 重启后恢复。
- 不启用时不影响当前行为。
- 开启后先只写日志，不直接深睡眠；确认后再接入实际动作。

### 4.2 CAN2 错误诊断弹窗

新增 API：

```text
GET /can2_diagnostics
```

返回字段：

- `eflg`
- `online`
- `rx_error`
- `tx_error`
- `diagnosis[]`
- `suggestions[]`

诊断规则来自公开源代码：

- `0x80` RX1OVR
- `0x40` RX0OVR
- `0x20` TXBO
- `0x10` TXEP
- `0x08` RXEP
- `0x04` TXWAR
- `0x02` RXWAR
- `0x01` EWARN
- `0xFF` SPI 无响应

UI 修改：

- 系统状态或 CAN工具中增加“查看原因”按钮。
- 弹窗只解释错误，不自动重置 MCP2515。

验收：

- CAN2 离线时返回可读原因。
- 不影响当前 `/bus2_ids`。

### 4.3 网络设置截图对齐

保留当前底层 API：

- `/hotspot_config`
- `/wifi_config`
- `/wifi_connect`
- `/relay_wifi_test`
- `/gateway_status`
- `/dns_rules`

UI 优化：

- 热点设置：AP SSID / AP 密码 / 隐藏热点。
- 外部中转 WiFi：STA SSID / STA 密码 / 测试连接。
- 状态文案使用“正常 / 待机 / 异常”。
- 保存网络重启必须二次确认。

验收：

- 不改变当前 DNS 保守 / 激进策略。
- 不改变默认热点配置。

## 5. 第二批执行任务：速度偏移截图对齐

### 5.1 固定百分比

新增或扩展 API：

```text
GET  /speed_offset_config
POST /speed_offset_config
```

字段：

- `strategy`: fixed / auto / custom
- `fixed_percent`: 0 / 10 / 20 / 30 / 40 / 50
- `custom_pct_1`
- `custom_pct_2`
- `custom_pct_3`
- `custom_pct_4`
- `effective_percent`
- `detected_limit_kph`

UI：

- fixed 模式显示 0、10、20、30、40、50。
- auto 模式显示内置算法说明。
- custom 模式显示四个区间输入。

验收：

- 先只接配置，不改 handler 注入逻辑。
- 确认不会破坏现有 HW3/Legacy speed map。

### 5.2 自动偏移算法

公开源代码算法基于：

- `current_detected_limit`
- `web_offset_mode`
- `custom_pct_1..4`
- `absolute_cap`
- `current_smart_offset_pct`

本地已有：

- `fusedSpeedLimitKph`
- `hw3CustomTarget`
- `legacyMppCustomTarget`
- `speedOffset`

执行建议：

- 不直接复制公开源代码算法。
- 先把 UI 与配置存储补齐。
- 后续在 HW3/Legacy 现有算法上做适配。

## 6. 第三批执行任务：灯光特技

### 6.1 配置字段补齐

扩展 `/lighting_config`：

- `auto_strobe_on_stalk`
- `on_time_ms`
- `off_time_ms`
- `continuous_highbeam`
- `rear_fog_strategy`: off / strobe / pilot / continuous

NVS：

- `lt_auto`
- `lt_on_ms`
- `lt_off_ms`
- `lt_cont`
- `lt_fog`

UI：

- 单次拨动触发连闪。
- 3 / 5 / 7 / 10 次。
- 慢速 / 中速 / 极速。
- 后雾灯 Strobe。
- F1 Pilot。
- 远光连续爆闪。

### 6.2 运行逻辑

本地当前已经有：

- CAN2 0x249 PULL / PUSH 短时注入。
- `t2canStalkTest()`
- `t2canStalkInjectTick()`

下一步先做：

- 基于现有 0x249 的短时序列，不移植 0x3C2 blind fire。
- 将 UI strobe sequence 调用 `/stalk_test`，已经部分完成。
- 后雾灯相关先只保存配置和显示“未实现运行”。

暂不做：

- 无限爆闪。
- 0x3C2 blind fire。
- 自动监听真实 0x3C2 后触发。

## 7. 第四批执行任务：FSD防御

### 7.1 当前本地已有能力

本地已有字段：

- `dashDefenseEnabled`
- `dashBionicSteering`
- `dashSpeedNoDisturb`
- `dashApEapCompatible`
- `isaChimeSuppress`
- `banShieldEnable`
- `hw3OffsetSlew`

本地已有 API：

```text
GET  /defense_config
POST /defense_config
```

### 7.2 缺失能力

公开源代码中的以下运行逻辑未合并：

- 0x370 仿生方向盘扭矩注入。
- 0x3C2 音量加减序列。
- 0x3C2 速度加减序列。
- 0x249 AP/EAP 物理拨杆模式切换。

### 7.3 建议执行顺序

1. 先新增 `/defense_runtime`，只返回当前支持状态。
2. UI 明确显示“配置已保存 / 运行未接入 / 需实车验证”。
3. 对 0x370 先做只读检测：记录 hands_on_state，不注入。
4. 对 0x3C2 先做手动单次测试 API，不自动触发。
5. 真车验证通过后，再允许绑定到防御开关。

## 8. 第五批执行任务：自动换挡

当前公开源代码截图和文字均说明该功能是预留安全核心功能，实际控制代码暂未部署。

本地当前 `/gear_assist_status` 返回：

- `available: false`
- `reason: gear assist CAN control not implemented in this firmware stage`
- speed / brake / gear 基础状态

执行建议：

- 保留自动换挡页面，但只做状态显示。
- 不实现自动换挡控制。
- UI 标记“储备功能，未开放”。
- 后续如果要开发，必须单独立项，要求真实 DBC、车辆测试方案和人工确认。

## 9. 验证清单

每阶段必须执行：

```text
python3 scripts/minify_dashboard.py
node --check /tmp/t2can_dashboard_current.js
python3 test/test_dashboard_api_contract.py
platformio run -e lilygo_t2can_dual
```

每次涉及固件发布：

```text
platformio run -e lilygo_t2can_dual
python3 -m esptool --chip esp32s3 merge-bin ...
shasum -a 256 firmware-assets/.../*.bin
```

真机验证：

- 连接 `Atlas-FSD`。
- 打开 `http://100.100.1.1/`。
- 检查 `/system_status` 固件版本。
- 检查每个按钮是否返回 JSON。
- 危险操作只在停车状态、实车确认后测试。

## 10. 回滚策略

每轮开发前：

- 保存当前 OTA app。
- 保存当前 full merged。
- 保存 SHA256。

如果新版本异常：

1. 使用上一版 full merged 全量刷机。
2. 或在面板可用时 OTA 回退。
3. 保留串口日志和 `/log` 输出。
4. 不清除用户 NVS，除非确认 NVS 配置导致问题。

## 11. 建议下一步

建议下一次实际开发先做“阶段 1 + 阶段 2 的低风险项”：

1. `/power_policy`
2. `/can2_diagnostics`
3. `/speed_offset_config`
4. 网络设置截图式 UI 整理
5. API 合约测试补充

暂时不要直接实现：

- 仿生方向盘扭矩注入。
- 速度加减免打扰自动触发。
- 后雾灯无限爆闪。
- 自动换挡控制。

