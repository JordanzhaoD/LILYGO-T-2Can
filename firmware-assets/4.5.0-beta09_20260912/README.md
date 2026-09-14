# LILYGO T-2CAN 固件资产 v4.5.0-beta09（2026-09-12）

**相对 v4.5.0-beta08 的变化**：两项**纯观测层**新增，回应第二份 Tier 1
CSV（162535.907）的「有时开启失败」根因定位——协调器 5/5 轮零缺陷，
「开启失败」实为**车侧拒激活**。8.3.6 防甩机制语义零改动（不改任何
门控 / 超时 / 注入逻辑）。

## 新增 1：「车辆响应」拒激活提示（P1）

**背景**：CSV 实锤一次原生 RWD 按杆被车无视——按杆后 6ms 车翻起
flag1、2.3s 才回 state2、**永不进 state3**；Jordan 等 6s 重按才激活。
设备全程正确静默（无激活边沿=不触发轮次），但驾驶者无从区分
「车拒绝了」与「盒子坏了」。

**现在**：8.3.6 开关开着（WaitDriverIntent 待命期）观测到原生 RWD
按压后 **2.5 秒内无 state3-6 激活边沿**：

- UI「FSD 防护 → 8.3.6防甩机制」诊断网格新增**「车辆响应」**格：
  - 等待期显示「等待车辆响应…」（2.5s 内）
  - 超时显示「**未接受激活 · 车侧拒绝**」（多次拒绝带 ×N 计数）
  - 正常激活或下一次按杆后回到「--」
- `/status` 诊断新增 `vehicleRefusal` / `rwdPressPending` /
  `vehicleRefusals` / `rwdPressMs`

**细节语义**：按住保持期的 10Hz 滚计数器帧只算一次按压；FWD 不
追踪（取消总是可观测的 6→1）；轮次进行中的按杆走既有 physicalInput
锁定路径，不触发本观测；设备自己的合成帧（own-echo 指纹）天然排除。

## 新增 2：0x399 高半字节 flag 位计数（P2）

0x399 byte0 的**高 4 位**是车侧标志位（此前在 dashboard 层被剥掉
丢弃）。跨两份 Tier 1 CSV 的假说：**flag1=驾驶员手力/扭矩**（手动
驾驶期突发、迟滞轮全程、被拒按杆后 6ms 翻起）、**flag4=停车/不可
用**、**flag5=剧烈捕捉后瞬态**——语义未破译。

`/status` 诊断新增 `apFlagCounts[16]` 直方图（每个 flag 值的帧数，
Disabled/门控关也计数）+ `lastApFlag` / `lastApFlagMs`。为后续实车
CSV 对照破译铺路。

## 现场怎么用（判读）

- **按杆没反应 1-2 秒** → 看「车辆响应」格：
  - 「未接受激活 · 车侧拒绝」= 车没接，**设备正常，勿连按**，稍等
    再按一次（CSV 中第二次按压即成功激活）
  - 「--」且轮次照常启动 = 一切正常
- 拒激活期间 `vehicleRefusals` 累计，截图/`/status` 留档即可

## 验证

- pytest **235 通过 + 721 subtests**（新增 beta09 双面钉测：模块
  常量/tick 顺序/边沿检测/JSON 裸形式双向钉/UI 双面/simulator 镜像）
- `pio test -e native_ap_rerequest` **60/60**（6 新用例：拒激活完整
  生命周期 / 激活边沿清除+二次拒累计 / 保持帧滚计数器去重 / 轮中
  按压不追踪 / FWD 不追踪 / flag 直方图+configure 重置）
- native **19 环境全 PASS** + 基础 native 155
- ESP 双构建 SUCCESS（lilygo + waveshare），产物 grep：
  **beta09×7 / beta08×0** 双环境（CMakeCache 已清）
- full_merged 自检：0xE9@0x0 + 0xE9@0x20000

## 烧录（Jordan 自行，二选一）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000，损坏板子）

## 烧后验证

1. `curl -s <IP>/status | python3 -m json.tool` 整体可解析
   （beta07 教训：JSON 坏 = 全 UI 假死，必做一步）
2. `legacyInjectionSafety.apReRequest` 内应见 `vehicleRefusal` /
   `apFlagCounts` 新键；行驶几分钟后 `apFlagCounts[0]` 应显著增长
   （0x399 每 500ms 一帧）
3. 8.3.6 开关开着、AP 未激活时按一次 RWD 且车不响应 → 2.5s 后
   「车辆响应」格应显示「未接受激活 · 车侧拒绝」

## 边界

- 未烧录、未接车、未做实车验证；两项新增均为纯观测，8.3.6 防甩
  语义零改动
- 未 commit / 未 push / 未发布
