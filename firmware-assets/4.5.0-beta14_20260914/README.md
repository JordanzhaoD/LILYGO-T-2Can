# LILYGO T-2CAN 固件资产 v4.5.0-beta14（2026-09-14）

**相对 v4.5.0-beta13 的变化**：beta13 实车分析定案的健壮性三方案——
计数器 +2 容差、车辆侧失败重分类、轮内重试——一次修到位。
**安全不变式全部保持**：Barrier A、物理拨杆锁、8/9 故障态锁、permit
链零改动；profile 新字段默认 0 严格等于 beta13 行为。

## 实车依据与三方案（2026-09-13 四份 CSV，~30 轮 70% 成功）

9 个失败轮全部归因为车辆侧瞬态 + 1 自检误判，预期失败率 31%→<10%：

1. **计数器 +2 容差（方案一）**：取消 release TX 后原生模块广播暂停
   213-290ms、恢复时跳一号，被旧「∈{lastTx, lastTx+1}」检查误判为
   counterConflict 硬锁（104 手势 3 例 +2 跳变全良性）。现环形比较：
   +2 容忍并计 `counterSkips`（诊断新键，正常轮次 >0 无需在意）；
   冲突=原生**落后**（beta04 真冲突形态）或跳前超 2，仍硬锁。
2. **车辆侧失败重分类（方案二）**：请求被拒（`apExitedDuringRequest`，
   重试预算耗尽后）与车无视取消（`cancelIgnored`，state3 持续到超时）
   纳入自动重武装集合（六成员，3 次上限不变）——「报错退出」硬锁
   体感消失。证据超时精化：车仍 active → `cancelIgnored`（重武装）；
   否则维持 `cancelEvidenceTimeout` 硬锁。
3. **轮内重试（方案三）**：①请求被拒（2→1）回等 state2（~940ms）
   自动重发，每轮 ≤2 次（覆盖 RequestSequence 与 WaitQualification
   两相位——0x399 2Hz 量化使拒绝多在 release 后才被检出）；②取消
   700ms 无证据且车仍 state3 → 重发一次取消，证据超时延至 2200ms。
   LittleGong 的「稳定」=人肉重试，这三项把那份耐心装进自动化。

另含 profile 结构体去填充修复（新增字段打开对齐填充孔令 configure()
的 memcmp 保留路径读到旧栈字节意外全重置；字段 4→2→1 降序重排 +
`static_assert(sizeof==40)` 钉死）。

## 体感变化（设计行为，不是故障）

- 重请求被拒会**静默再等 ~1s 自动重发**（每轮 ≤2 次）——等待可能
  比往常长 1-2s；
- 取消被无视 ~0.7s 会**自动重发一次取消**——可能出现「取消-等待-
  再取消」；
- 计数器 +2 跳变不再触发 counterConflict 硬锁（改计 counterSkips）。

## 验证

- pytest **236 全绿**（合约新增
  `test_beta14_field_robustness_tolerance_reclass_retries` 六节钉：
  容差算式/重分类 reason/3a 预算式与双相位/3b 字段对与 700+2200/
  sizeof==40 memcmp 守卫/JSON 链 order-pin + UI 双面；版本断言同步
  beta14）
- native **22 环境逐个全 PASS**（ap_rerequest 68 用例——含 3a/3b
  重试、+2 容差、重分类、memcmp 保留路径全链）
- ESP 双构建 SUCCESS，产物 grep **beta14×7 / beta13×0** 双环境
  （CMakeCache 已清）
- full_merged 自检：0xE9@0x0 + 0xE9@0x20000

## 烧录（Jordan 自行，二选一）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000，
  损坏板子）

## 烧后验证

1. `curl -s <IP>/status | python3 -m json.tool` 整体可解析；版本 =
   4.5.0-beta14
2. `apReRequest` 内新键 `counterSkips` / `cancelRetries` /
   `requestRetries` 存在且为 0
3. 状态行新 reason 中文条目：请求被拒·轮内重试等待 / 取消被无视·
   重发已武装 / 车辆无视取消·已自动重武装
4. 行驶验证按 `docs/AP-REREQUEST-VEHICLE-VALIDATION-PROCEDURE.md`
   （判读表已含 beta14 四新行）

## 边界

- 未烧录、未接车、未做实车验证；不声称实车方向盘猛甩已修复
- 未 commit / 未 push / 未发布
