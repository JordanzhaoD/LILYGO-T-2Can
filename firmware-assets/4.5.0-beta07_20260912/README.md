# LILYGO T-2CAN 固件资产 v4.5.0-beta07（2026-09-12）

**相对 v4.5.0-beta06 的变化**：一处单字符修复——beta06 引入的
`autoRearms` 诊断字段把 `/status` 与 `/defense_config` 的 JSON 弄坏了。

## 症状（真机复现 + 硬件确认）

1. **「UI 后台所有开关打开后，重新进入就是关闭状态」**
   —— 配置其实**全部保存成功**（设备 RAM/NVS 实测 ci/defenseEnabled/
   apGate/ap_re_request 全为 true）。坏的是**读回**：`/defense_config`
   响应在 `autoRearms` 处断裂 → `loadDefenseConfig()` 解析失败早退 →
   复选框停在 HTML 默认 OFF。
2. **顶栏「未连接」** —— `/status` 同一处断裂 → `poll()` 拿到 null →
   红徽章（与 CAN 在线无关的触发路径）。
3. **「疑似 WiFi 不稳」** —— 实测 180s / 80 次轮询 **0 次失败**、
   0 次重启、NVS 写探针 8/8 成功；WiFi/保存链均健康，观感问题源于
   上述解析失败。

## 根因

`dashAppendLegacySafetyControlsJson` 的链式拼接约定：字符串值后面的
字面量写 `","key":`（前导引号负责闭合上一个字符串值）；**数值**值后面
必须写 `,"key":`。beta06 在数值 `autoRearms` 后误用了字符串收尾形：

```text
产出: ..."autoRearms":0","round":0...    ← 0 后面的 " 是游离引号
```

`mcp2515_dashboard.h` 原 `j += R"JSON(","round":)JSON";` → 修复为
`j += R"JSON(,"round":)JSON";`。真机抓包验证：删掉这一个引号后
`/status`（99 键）与 `/defense_config`（20 键）**均完整解析**，
全文件仅此一处坏点。

## 为什么测试没拦住

合约断言用的是无边界子串 `,"autoRearms":` —— 坏形式 `","autoRearms":`
包含它，绿灯放行。本次新增 `test_status_json_auto_rearms_emission_is_valid`
双向钉死（正确裸形式必须在、坏形式必须在死）。

## 验证

- pytest 233 通过 + 719 subtests（含新回归测试）
- `pio test -e native_ap_rerequest` 54/54
- ESP 双构建 SUCCESS（lilygo_t2can_dual + waveshare_single_can_standalone），
  产物 grep：beta07×7 / beta06×0，坏字面量 0 处
- 真机（192.168.1.247，串口+HTTP 双通道）：/status 与 /defense_config
  单引号修复后完整解析；保存往返探针全绿

## 烧录（Jordan 自行，二选一）

- **WebUI OTA**（保配置，推荐）：上传 `*_ota_app.bin`
- **esptool 全量**（清配置）：`esptool write_flash 0x0 *_full_merged.bin`
- ⚠️ **禁 `pio run -t upload`**（app@0x10000 ≠ 分区表 app0@0x20000，损坏板子）

## 边界

- 未烧录、未接车、未做实车验证；8.3.6 防甩机制语义零改动
  （修复仅在诊断 JSON 拼接层）
- 未 commit / 未 push / 未发布
