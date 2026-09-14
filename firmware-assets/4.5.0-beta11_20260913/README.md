# LILYGO T-2CAN 固件资产 v4.5.0-beta11（2026-09-13）

**相对 v4.5.0-beta10 的变化**：**单字符热修**——beta10 实车烧录后配置
保存与页面显示全面异常的根因，是黑匣子条目发射段 `"reason":"` 打开后
拼接 `e.reason` 直接连接 `'}'`（缺一行 `j += '"';`），每条 entry 产出
`"reason":"xxx}` 未闭合字符串吞掉其后所有 JSON；boot 条目开机即写、
黑匣子永不为空，故首次 boot 起 /status 与 /defense_config 整体不可
解析（保存链本身未坏，NVS 配置完好——重刷即恢复，与 beta06→beta07
同签名）。beta11 = beta10 + 该收尾引号，**8.3.6 防甩语义与 beta10
观测层零改动**：beta10 README 描述的全部观测字段（permitInputs /
tickHeartbeat / otaWatch / blackbox）、UI「许可链」「心跳」两格、
P0 取证规程在 beta11 上完全适用（详见 4.5.0-beta10 README 与
`docs/AP-REREQUEST-VEHICLE-VALIDATION-PROCEDURE.md`）。

## 防复发钉测（合约 5b）

第三次同类逃逸（beta06 散引号 / beta07 数值后字符串收尾形 / beta10
缺收尾引号）后，合约新增「字符串值**收尾**纪律」双向钉测：

- 正向：kind 值经 `","ms":` 字面量收尾、reason 值经显式 `j += '"';`
  收尾的精确四行块钉死；
- 负向：`e.reason;` 直连 `'}'` 的坏形式 assertNotIn。

## 验证

- pytest **237 全绿**（含 5b 新钉测）
- native **20 环境逐个全 PASS**（含 native_ap_blackbox 15/15）
- ESP 双构建 SUCCESS（lilygo + waveshare），产物 grep **beta11×7 /
  beta10×0** 双环境（CMakeCache 已清；两产物与 beta10 均有真实字节
  差异，非缓存复用）
- full_merged 自检：0xE9@0x0 + 0xE9@0x20000

## 烧录（Jordan 自行，二选一）

- **WebUI OTA**（保配置，推荐，beta10 烧过的设备直接刷本文件即恢复
  显示）：上传 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000，
  损坏板子）

## 烧后验证

1. `curl -s <IP>/status | python3 -m json.tool` 整体可解析（beta07
   教训的烧后一步验证；beta10 设备刷 beta11 后页面/保存即恢复正常）
2. `legacyInjectionSafety.apReRequest` 内 `permitInputs` /
   `tickHeartbeat` / `otaWatch` / `blackbox` 新键在、blackbox 首条为
   `boot`（seq=1）、「许可链」格「正常」、「心跳」格「活跃」
3. 行驶中 `otaWatch.seen318` 持续增长（0x318 周期帧）
4. 拔电重插一次 → blackbox 新 `boot` 条目且旧条目保留（NVS 跨断电
   存活验证）

## 边界

- 未烧录、未接车、未做实车验证；热修不涉及防甩行为
- 未 commit / 未 push / 未发布
