# Alert Telemetry 台架验证（2026-11-14）

固件：ban-shield-x @ a631dac（beta15 + grid-fix + alert telemetry）
板 2：USB 连接（串口 open 不复位=安全旁观），WiFi 192.168.1.145

## 步骤

1. **OTA 告警构建到板 2**（裸 body + X-File-Name），验证 /status uiBuildId = `a631dac…`
2. **CAPTURE 级触发**（sess_124 原始帧，a144 配置告警）：
   ```
   rv2_frame 3 0x526 8 90 00 20 00 00 00 00 00
   ```
   预期：事件入列（/alert_events 见 `a144_config` sev=1）；自动开闸（/ring_status
   `stream_armed:true`）；~30s 后锁定（`/sessions` 新会话，meta 含 `"auto":true`）；
   锁毕自动关闸；防护页"告警取证"卡事件列表刷新
3. **CAPTURE 级第二类**（a062 跛行）：
   ```
   rv2_frame 3 0x526 8 3E 00 19 00 00 00 00 00
   ```
   预期：同上（sev=1，名=a062_limpReason）
4. **LOG 级对照**（a010 普通告警）：
   ```
   rv2_frame 3 0x526 8 0A 00 00 00 00 00 00 00
   ```
   预期：事件入列 sev=0，**不开闸不生成会话**
5. **0x346 矩阵位**：
   ```
   rv2_frame 3 0x346 8 00 00 00 00 00 00 00 20
   ```
   预期：w062_systemLimpMode 事件（sev=1）+ 自动抓拍；重复同帧→无新事件；
   /alert_events 的 matrix/matrixBits 显示 `…0020` + `w062_systemLimpMode`
6. **下载验证**：/session_download 拿自动会话（含回灌的监听集 pre-上下文）

## 结果（2026-11-14 实测，板 2 @192.168.1.147——重启后 DHCP 换新地址）

| # | 步骤 | 结果 |
|---|---|---|
| 0 | OTA a631dac → 后续修正至 038ea7f | ✅ uiBuildId 逐级验证 |
| B1 | a144 注入 | ✅ 事件入列（a144_config sev=1 data=9000200000000000）；自动开闸（flash writes active）；~30s 锁定=自动会话 seq=131；锁毕自动关闸；SM=cooldown→idle |
| B2 | a062 注入 | ✅ 同 B1（a062_limpReason sev=1，自动开闸） |
| B3 | a010 LOG 级对照 | ✅ 事件入列 sev=0（w010_hwPedalMonitorReset）；**不开闸不生成会话** |
| B4 | 0x346 w062 注入+重播 | ✅ 首注=事件+自动抓拍；重播=零新事件零重复开闸（上升沿去重）；matrix=0000000000000020 + matrixBits=[w062_systemLimpMode]（需 038ea7f：首版错位到 /frames 处理器，见下） |
| B5 | 自动会话下载 | ⚠️ 会话存在（64-192KB、frames 计数在案）但 CSV/bin 下载为空——**判别实验证明与告警执行器无关**（手动 ring_lock 会话同样空下载）；系稀疏合成会话（1-5 帧、pre_ms_act=0）在既有下载路径的时间窗过滤下的边缘案例。真实流量会话（sess_129 先例=289,747 帧）下载正常。**待田野真实流量复验** |

**过程中发现并修复的三个真 bug**：
1. matrix 暴露代码经锚点替换静默落进 /frames 处理器（handleAlertEvents 未得）→ 外科修正 + 契约改为函数域钉
2. **i18n 字典双逗号 → 全页 UI 死刑**（"Unexpected token ','"：一个 script 块=一个语法单元，构建期 minifier 静默放行、运行期全控件失灵）→ 修复 + **node --check 语法钉**（此后字典语法错无法再出厂）
3. 逗号链二次断裂（修双逗号时删掉的是与下一条目的分隔符）→ 语法钉当场抓获修正

**台架结论**：告警遥测特性链路（哨兵→severity→自动抓拍→会话→遥测端点→UI 卡）端到端验证通过；唯一开放项=B5 稀疏会话空下载（既有路径、非本特性引入、待真实流量复验）。
