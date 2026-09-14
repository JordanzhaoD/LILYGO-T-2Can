# LILYGO T-2CAN 固件资产 v4.5.0-beta10（2026-09-13）

**相对 v4.5.0-beta09 的变化**：**P1 观测层**——回应 beta08 实车三症状
反馈（① 运行 AP 后只出现 EAP；② 强退 FSD 后再开无法注入；③ 长时间
运行后无法注入）。症状 2/3 开关循环无效、仅拔插断电恢复 = 挂死态在
协调器**之外**的 RAM 里，断电恰好销毁证据。beta10 **只加观测不加行为**：
8.3.6 防甩语义零改动，permit 表达式逐字钉死（合约测试双向钉）。

## 四处埋点（全部进 `/status` 的 `legacyInjectionSafety.apReRequest`）

1. **`permitInputs`**（canActive / legacyHandler / otaGuardClear /
   abortGuardOk / driver 五输入逐项分解）+ `permitBits` 位图
   （bit0 canActive / bit1 legacyHandler / bit2 driver / bit3 otaClear /
   bit4 abortGuardOk）——再遇「无法注入」，一眼看出五个输入哪个断了。
2. **`tickHeartbeat`**（count / lastMs / ageMs）——协调器 tick 心跳；
   两次 `/status` 间 count 不涨 = CAN 任务层饿死（独立于 permit 的证据）。
3. **`otaWatch`**（seen318 / last318Ms / confirm / clear / active）。
   **重要更正**：OTA 守卫并非此前误判的"死代码"——它实际接线于 CAN 帧
   分发器，且清除需连续 6 帧非 OTA 的 0x318；**若置位后 0x318 停发则
   守卫永久卡 true → permit 静默死亡 → 无提示 → 仅拔插可恢复**。与症状
   2/3 完美吻合，列为**头号嫌疑**（现有 `/vehicle_ota_status` 亦可查证）。
4. **`blackbox[8]`**（NVS 黑匣子，跨断电存活）：环形 8 条，记录
   boot / heartbeat(5min) / roundStart / roundEnd / lock / permitChange /
   intentChange（flappy 种类 5s 限速），每条含 seq / ms / tick / phase /
   permitBits / apState / reason。开机 NVS 恢复并打 boot 标记——**意外
   断电后第一次 `/status` 即显示断电前最后的证据**（Boot 记录之前的
   条目就是上一任固件死前记录的）。

## UI 变化

「FSD 防护 → 8.3.6防甩机制」诊断网格新增两格：

- **许可链**：五个输入全通显示「正常」；断时显示「断: XX/XX」中文
  点名（总开关/处理器/驱动/OTA守卫/AbortGuard）
- **心跳**：`count · 活跃` 或 `count · 停滞Ns`（停滞 = tick 饿死）

## 下次故障取证规程（P0 纪律，已写入规程文档）

1. **先截图/`curl /status`**（含 defense_config）——再什么都别动
2. 再试开关循环（症状 2/3 预期无效，但留下证据）
3. 仍无效才拔电重插；重插后**先看 blackbox**（Boot 之前的条目 =
   断电前证据），再操作任何开关
4. 注意：wedge 期间 Web UI 可能也不可达——黑匣子跨重启存活正是为此

## 判读速查

| 观察 | 结论 |
|---|---|
| `permitInputs.otaGuardClear=false` + `otaWatch.active=true` + `last318Ms` 停涨 | OTA 守卫卡死实锤（P2 修复方向） |
| `tickHeartbeat` 两次读取 count 不涨 | CAN 任务层饿死（tick 挂死） |
| `permitInputs` 断在 canActive | CAN 驱动退化（canActive=UI 可见，易排除） |
| blackbox 里 Boot 前有 `permitChange` 且 permitBits bit3=0 | 断电前 OTA 守卫已断 |
| blackbox 最后一条 `lock` reason=counterConflict 等 | 协调器内锁死（另一类问题） |

## 验证

- pytest **233 全绿**（新增 beta10 双钉测：permit 表达式逐字钉/
  守卫包含性正反钉/黑匣子 NVS 先于 Boot 记录顺序钉/UI 双面/模拟器镜像/
  9 个坏引号形式全死）
- `pio test -e native_ap_blackbox` **15/15**（栈序/容量丢弃/flap guard/
  dump-load 往返+续号/错误长度拒绝/截断/无效 kind/clear/permitBits）
- native **21 环境逐个全 PASS**
- ESP 双构建 SUCCESS（lilygo + waveshare），产物 grep **beta10×7 /
  beta09×0** 双环境（CMakeCache 已清）
- full_merged 自检：0xE9@0x0 + 0xE9@0x20000

## 烧录（Jordan 自行，二选一）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000，
  损坏板子）

## 烧后验证

1. `curl -s <IP>/status | python3 -m json.tool` 整体可解析
   （beta07 教训：JSON 坏 = 全 UI 假死，必做一步）
2. `legacyInjectionSafety.apReRequest` 内应见 `permitInputs` /
   `tickHeartbeat` / `otaWatch` / `blackbox` 新键；blackbox 首条应为
   `boot`（seq=1）
3. 行驶中「心跳」格应显示「活跃」；`otaWatch.seen318` 应持续增长
   （0x318 周期帧）
4. 拔电重插一次 → blackbox 里出现新 `boot` 条目且**旧条目保留**
   （NVS 跨断电存活验证）

## 边界

- 未烧录、未接车、未做实车验证；纯观测层，8.3.6 防甩语义零改动
- 未 commit / 未 push / 未发布
