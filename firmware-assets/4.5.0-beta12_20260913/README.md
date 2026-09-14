# LILYGO T-2CAN 固件资产 v4.5.0-beta12（2026-09-13）

**相对 v4.5.0-beta11 的变化**：LittleGong（第三方 8.3.6 防甩固件，
ColorBlack_T-1.3.8）逆向分析得出的健壮性/容错机制，四项移植并入一版
（方案 4 按约舍弃）。**8.3.6 防甩语义依旧零改动，permit 表达式依旧
逐字钉死**——守卫改造全部实现在 vehicleOtaActive 的清除条件上。

## 四项移植

1. **OTA 守卫三路径（症状 2/3 头号嫌疑的行为修复）**：原清除条件
   「连续 6 帧非 OTA 的 0x318」在 0x318 停发时永久卡 true → permit
   静默死亡、仅拔插可恢复。现在：A. 6 帧清除保留；B. **停流超时降级**
   ——置位满 10 分钟且期间零 0x318（任意 dlc 都算载波存活）自动清除
   + `staleClears` 计数 + 日志 `[OTA] guard stale-clear`；C. **用户覆盖**
   ——FSD 防护页「忽略车辆 OTA 守卫」（诊断徽标；NVS `otg_ovr`），
   开=立即清除并暂停置位，关=干净重来（真 OTA 仍被 3 帧确认重新捕获）。
   `otaWatch` 与 `/vehicle_ota_status` 扩 `staleClears` / `override` /
   `blockedMs`。
2. **0x3EE 载波节奏观测（症状 1「只出现 EAP」判读）**：`/status` 新增
   `carrier3ee`（lastGapMs/maxGapMs/longGaps/total/lastMs/ageMs），
   过滤口径与资格注入所见完全一致（0x3EE mux0 CAN_BUS_VEH），longGap
   阈值 2000ms 严格大于。纯观测 JSON，无 UI 格。
3. **注入 TX 会计（症状 3 鉴别）**：`/status` 新增 `txAccounting`
   （attempts/ok/fails/consecutiveFails/lastOkAgeMs），账本 =
   legacyFsdDiag.mux0（recordMuxTx 仅 activation-allowed 路径 = bit46
   注入帧专用，速度偏移-only 写入不混入）。
5. **黑匣子条目 JSON 纯函数化（结构加固）**：发射挪入
   `dash_ap_blackbox.h` 的 `dashApBbEntryToJson`，native golden-string
   测试逐字符钉死全部 7 kind × 有/无 reason；buffer 不足返回 0（条目
   跳过，绝不截断）。beta06/beta07/beta10 三次引号事故的根因（发射
   代码在 native 不编译的 dashboard 头里）结构性闭合。

## 验证

- pytest **234 全绿**（含新增 `test_beta12_littlegong_port_ota_guard_
  carrier_tx` 全链双向钉：permit 逐字钉保持 + 新键裸形式正向/游离引号
  负向 + NVS/POST/UI/模拟器）
- native **22 环境逐个全 PASS**（新增 native_ota_guard 20/20、
  native_3ee_cadence 12/12；native_ap_blackbox 扩至 21/21 含 6 个
  golden-string 用例）
- ESP 双构建 SUCCESS（lilygo + waveshare），产物 grep **beta12×7 /
  beta11×0** 双环境（CMakeCache 已清）
- full_merged 自检：0xE9@0x0 + 0xE9@0x20000

## 烧录（Jordan 自行，二选一）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000，
  损坏板子）

## 烧后验证

1. `curl -s <IP>/status | python3 -m json.tool` 整体可解析
2. 版本 = 4.5.0-beta12；`otaWatch` 含 `staleClears`/`override`/
   `blockedMs`、`carrier3ee`/`txAccounting` 块在、FSD 防护页有
   「忽略车辆 OTA 守卫」开关（默认关）
3. 行驶中 `otaWatch.seen318` 持续增长、`carrier3ee.total` 增长且
   `maxGapMs` ≲2000（正常节奏）
4. 若再遇「无法注入」：先截 `/status` 再按规程 P0 三步（判读表已含
   staleClears/override/blockedMs/txAccounting/carrier3ee 新行，见
   `docs/AP-REREQUEST-VEHICLE-VALIDATION-PROCEDURE.md`）

## 边界

- 未烧录、未接车、未做实车验证；不声称实车方向盘猛甩已修复
- 未 commit / 未 push / 未发布
