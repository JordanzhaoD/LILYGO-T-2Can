# LILYGO T-2CAN 固件资产 v4.5.0-beta13（2026-09-13）

**相对 v4.5.0-beta12 的变化**：修复 beta12 实车严重故障——页面顶栏
「未连接」、CAN 离线显示、配置全灰无法保存、RX/TX 0/0——的单一根因。
**8.3.6 防甩语义零改动，LittleGong 四项移植本体无恙。**

## 根因与修复

beta10 观测层新增的 `,"permitInputs":{` 五输入分解对象**缺失闭合
花括号** → `legacyInjectionSafety` 永不闭合 → root `}` 被错位消费 →
`/status` 与 `/defense_config` 从首次 boot 起整体不可解析。页面 HTML
（gzip rodata 静态通道）正常送达，但 poll 数据全死 → 顶栏「未连接」
（poll 主动设置）+ 配置复选框回 HTML 默认灰 OFF + RX/TX 停 0/0。
症状与车/CAN/WiFi 无关（模块插不插车一样）；**OTA 刷机不清 NVS，
配置从未丢失，重刷本版即恢复显示**。

修复 = `driver` 键后补 `j += '}'` 并拆分 `#ifdef DRIVER_T2CAN_DUAL`
（非双驱动构建 4 输入同样闭合）；permitBits/tickHeartbeat/blackbox/
otaWatch 维持 apReRequest 层兄弟键位置（与模拟器/UI 一致）。

beta10 实车发现的 reason 收尾引号（beta11 单字符热修）是**同一增量内
的另一个 bug**——修完它本 bug 仍在并随 beta12 发布。这是第五次同类
JSON 破损事故（beta06/07/10/10-12）。

## 结构闭合（防第六次）

- 新增 `scripts/reconstruct_status_json.py`：按源序逐语句机械重建三个
  动态接口（handleStatus / dashDefenseConfigJson /
  handleVehicleOtaStatus，含全部内联 builder 与拼接表达式占位建模），
  json.loads 整体验析字面量骨架。本 bug 修前精确报出 EOF 截断，修后
  **ALL PARSE CLEAN**（93/21/6 顶层键）。
- 合约新增 `test_beta13_permitinputs_close_reconstruction_guard`
  （pytest 子进程跑重建器 + 闭合位置 order-pin + 模拟器兄弟形状）——
  dashboard 头 native 不编译，此前无任何自动化整解析门。

## 验证

- pytest **239 全绿**（含新守卫；版本断言同步 beta13）
- native **22 环境逐个全 PASS**（ap_rerequest 60 / ap_blackbox 21 /
  ota_guard 20 / 3ee_cadence 12 等）
- ESP 双构建 SUCCESS，产物 grep **beta13×7 / beta12×0** 双环境
  （CMakeCache 已清）
- full_merged 自检：0xE9@0x0 + 0xE9@0x20000

## 烧录（Jordan 自行，二选一）

- **WebUI OTA**（保配置，推荐；beta12 坏页面也能刷——/update 不依赖
  JSON 解析）：上传 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000，
  损坏板子）

## 烧后验证

1. `curl -s <IP>/status | python3 -m json.tool` **整体可解析**（本版
   核心；beta12 上此步必炸）
2. 版本 = 4.5.0-beta13；顶栏恢复「已连接」、配置开关恢复显示、RX/TX
   恢复增长
3. FSD 防护页「忽略车辆 OTA 守卫」开关在（默认关）；诊断网格「许可链」
   显示「正常」、「心跳」显示「活跃」
4. 行驶中 `otaWatch.seen318` 持续增长、`carrier3ee.total` 增长且
   `maxGapMs` ≲2000
5. 若再遇「无法注入」：按规程 P0 三步取证（判读表见
   `docs/AP-REREQUEST-VEHICLE-VALIDATION-PROCEDURE.md`）

## 边界

- 未烧录、未接车、未做实车验证；不声称实车方向盘猛甩已修复
- 未 commit / 未 push / 未发布
