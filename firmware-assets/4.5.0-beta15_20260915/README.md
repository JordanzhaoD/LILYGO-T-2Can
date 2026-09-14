# LILYGO T-2CAN 固件资产 v4.5.0-beta15（2026-09-15）

**相对 v4.5.0-beta14 的变化**：8.3.6 防甩**协调器家族整体移除**（约 4,300 行，
含 beta13/14 全部容差/重试补丁与其 NVS 黑匣子、校准分析器），由单 CAN
v1.19.2 **实车冻结**的 JITTER 程序**逐字移植**取代（`dash_jitter_procedure.h`
593 行 + 39 例 native 测试逐字）。

## 为什么换（路线 B 论证）

- 协调器 beta13 实车 30 轮 70% 成功、9 次失败；beta14 三方案是给协调器
  续命的生命维持——继续修等于在已验证方案旁边维护一套未验证方案
- 单 CAN 同车同总线同拨杆的 JITTER：**开启上百次、0 例甩方向**
  （v1.19.1 起机制冻结，「不甩」为单车小样本表述）
- 模块本体逐字不改一字；全部适配在接线层

## 接线适配（相对单 CAN 的全部差异）

1. **四路观测（0x399/0x3EE/0x045/0x488）全部 VEH 总线过滤 + DLC 卫**——
   PARTY 副总线镜像帧不得驱动机器
2. 0x488 转向角用单 CAN 三位形态 `((b2>>5)&0x07)==2`；LegacyHandler
   0x370-NAG 的两位解码是另一条路径，两处注释声明差异有意
3. bit46 暂停接在 `dashLegacyFsdActivationAllowed` 头部
   （`procedureActive()` 在 `!apInjectionGate` 早真之前，顺序钉）
4. 滚轮 DND 仲裁换 `StalkOwnedByJitter`（窗口宽度不变）
5. 诊断 `legacyInjectionSafety.jitter` 22 键（链式引号规则逐字）；
   UI 9 格诊断面；模拟器 22 键镜像
6. NVS `jtr_on`（默认关）+ 旧 `apr_on` 载入时一次性迁移清除 +
   `aprbbx` 黑匣子命名空间一次性清空

## 体感变化（设计行为，不是故障）

- 开关名不变（`8.3.6防甩机制`），诊断面从 12 格换成 9 格
  （新增 FSD/EAP 落地拆分、bit46 武装/刷新计数）
- **失败 = 静默复位待机**（无重试、无锁定、无"自动重武装"概念）——
  下一次 AP 激活边沿重新开始；每行车段站下上限 1 次（P 档 rising edge 清零）
- **双 CAN 上独立 0x3EE bit46 TX 属新行为**（协调器只改活帧）：
  首场实车读 bit46Shots/Refreshes 与 FSD/EAP 落地计数
- 急停杆 = 关开关（立即 fullReset("off")）

## 验证

- pytest **228 全绿**（新增 4 钉：开关 fail-closed 全链 / AP 门控与 UI
  顺序 / 8 常量冻结 / 22 键发射字面 + UI 双面 + 模拟器键集 + VEH 四卫 +
  滚轮仲裁；新协调器家族全灭断言；CI 矩阵对账 22/22）
- native **22 环境逐个全 PASS**（native_jitter 39 例逐字；格式化后复跑）
- **clang-format 棘轮 0 违规**（CI lint 同款 v18 与本地 v22 双验证一致；
  批量格式化 29 个历史漂移文件为独立 style commit，冻结件
  dash_jitter_procedure.h / test_jitter.cpp 未动一字）
- JSON 重建器 ALL PARSE CLEAN（93/21/6 键）；minify --check 通过；
  模拟器冒烟（22 键、apReRequest 无、POST 持久）
- ESP 构建 SUCCESS（CMakeCache 已清；格式化后复构建），full_merged
  自检：0xE9@0x0 + 0xE9@0x20000

## 烧录（Jordan 自行，二选一）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000，
  损坏板子）

## 烧后验证

1. `curl -s <IP>/status | python3 -m json.tool` 整体可解析；版本 =
   4.5.0-beta15
2. `legacyInjectionSafety.jitter` 22 键在、`apReRequest` 对象**不在**
3. 开关默认关；「AP 注入门控」开启后 8.3.6 开关才生效
   （`effective` = 开关 ∧ 门控）
4. 首场实车判读：FSD/EAP 落地计数、bit46Shots/Refreshes、
   pumpFrames 是否异常膨胀（own-echo 观察项）

## 边界

- 未烧录、未接车、未做实车验证；「不甩」结论系单 CAN 单车小样本
  表述，不作跨车推广
- 无 own-echo TX 指纹（单 CAN 实车验证过无指纹形态，逐字移植决策）
- 未 push / 未发布（待 Jordan 报批）
