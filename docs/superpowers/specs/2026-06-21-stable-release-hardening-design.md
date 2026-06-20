# 单 CAN 微雪版 v1.0 稳定版加固与开源发布设计

**日期**: 2026-06-21
**分支**: `fix/remove-dangerous-epas-nag`（HEAD `b96826d`，接续 [[2026-06-20-single-can-legacy-fsd-alignment-design]] Task 1-6）
**环境**: `waveshare_single_can_standalone`
**目标版本**: `v1.0.0-atlas-single-can`（首个公开稳定版）

---

## 1. 背景与目标

Jordan 实车 Legacy Model X 完成 L0 验证（6 小时稳定运行，FSD 正常），上轮 Task 1-6 的功能①（速度偏移）/②（视觉限速关闭）/⑤（重写限速）正常工作。本次目标：

1. **修复持久化 bug**：Legacy 速度偏移、重写限速、速度偏移参数重启丢失
2. **修复 UI 风格**：插件管理/已安装插件管理界面与车机主 UI 风格不统一
3. **开源发布**：新工作空间、GPL-3.0、脱敏、公开 GitHub repo

**不在本次范围**：nag 0x370 台架增强（→ v1.1 backlog）。

---

## 2. 实车 L0 测试结果（2026-06-20）

| 项 | 结果 |
|----|------|
| 固件稳定性 | ✅ 6 小时稳定运行，无报错，FSD 激活正常 |
| 功能①速度偏移 | ✅ 正常 |
| 功能②视觉限速关闭 | ✅ 正常 |
| 功能⑤重写限速 | ✅ 正常 |
| nag 抑制 | ⚠️ 能感觉到效果但不明显（bit-19 baseline 温和特性，预期）|
| 持久化 | ❌ 速度偏移/重写限速参数每次启动需重设 |

---

## 3. 三方探索发现（数据 + 代码 + UI 交叉印证）

### 3.1 数据分析（两段 CAN 录制，各 7.8 秒 / 2000 帧，RX+TX 混合）

| 注入目标 | 功能 | 录制有 TX？ | 结论 |
|---------|------|-----------|------|
| 0x3EE (1006) | 视觉限速重写 | ✅ 15-16 帧/段，RX/TX 1:1，改 b5|=0x40/b6 | 正常工作 |
| 0x2F8 (760) | 速度偏移 | ✅ 8 帧/段，b5 +14 | 设置后工作（运行内存有值）|
| 0x438 (1080) | Legacy 重写限速 | ❌ 全 dir=R，零 TX | **未注入**（持久化 bug 导致 overrideSpeedLimit 归 false）|
| 0x370 (880) | EPAS/nag | ❌ 全 dir=R，零注入签名 | 符合安全设计（EPAS nag 已物理删除，bit-19 走 0x3EE/0x3FD）|

**关键交叉印证**：0x438 零 TX 独立证实了代码审计的持久化断点。

### 3.2 持久化代码审计（根因 = 断点 A：没写 NVS）

**断点定位**：UI 把 `legacyOffset` / `overrideSpeedLimit` POST 到 `/config`（JSON body `{fsdRuntime:{...}}`），但 `handleConfig`（`mcp2515_dashboard.h:2481-3025`）**只读表单参数，不解析 JSON body** → 静默丢弃 → NVS 永不写。能写 `fg`/`fi` 的 JSON 逻辑在 `handleSettingsImport`（`/settings_import`，:5656/:5904-5937），UI 没往那发。

| 配置项 | NVS key | 写入端点（实际）| 断点 |
|--------|---------|---------------|------|
| legacyOffset | `fg` | `/config`（JSON 不被解析）| **A：没写 NVS** |
| overrideSpeedLimit | `fi` | `/config`（JSON 不被解析）| **A：没写 NVS** |
| removeVisionSpeedLimit | `fh` | `/legacy_fsd_config` | 通 |
| isaChimeSuppress | `fd` | `/defense_config` | 通 |
| isaOverride | `fj` | `/defense_config` | 通 |

**读取/应用链路全通**：`dashLoadPrefs`（:1488/:1617-1626）→ `dashApplyNvsRuntimeSwitches`（:6594-6615）→ `dashApplyRuntimeState`（:1282-1307）。正因为读链路通，重启后加载的是默认值（offset=0、override=false）→ 表现为"重启丢配置"。

**应用层短路放大效果**：`handlers.h:325`（`if effectiveOffset==0 return`）、`handlers.h:341`（`if !overrideSpeedLimit return`）。重启后字段归默认 → 短路 → 永不注入。**"效果不明显"与"持久化丢失"同源**。

### 3.3 UI 审计

插件管理卡 `#plugins-card` 外壳已套驾驶舱皮（`card cockpit-card`），但 `renderPluginsStatus()`（`mcp2515_dashboard_ui.src.h:2649-2665`）生成的"已安装插件"列表项是**裸 HTML**（原生 checkbox、无 class 按钮、裸 details/pre），`.plugin-item` 类无 CSS 定义。插件功能本身真实（9 个 `/plugins/*` 端点 + `dash_plugin_engine.h` + 测试），不删，只修 UI。

---

## 4. 决策汇总

| 决策 | 选择 | 理由 |
|------|------|------|
| 开源策略 | 公开 + 脱敏 | 成熟项目对外发布，去个人/车辆标识 + 强免责 |
| repo 范围 | 新工作空间，platformio.ini 只留单 CAN env | 专注 + 构建稳（公共代码保留）|
| nag 策略 | v1.0 保持 bit-19；0x370 台架增强 → v1.1 | 0x370 上次出事故，台架验证是独立安全流程，不阻塞发布 |
| 持久化修复 | 方案 2：handleConfig 加 JSON 分支 | 语义清晰、局部改动、不误伤（方案 1 改端点有覆盖风险）|
| LICENSE | GPL-3.0 | Copyleft 保护代码开放性 |

---

## 5. 设计方案

### 第 1 块：持久化修复（方案 2）

**改动**：`handleConfig`（`mcp2515_dashboard.h:2481`）开头加 JSON 解析分支。

```
1. 读 server.arg("plain") 拿 JSON body
2. deserializeJson 解析 fsdRuntime.legacyOffset / fsdRuntime.overrideSpeedLimit
3. 更新 nvs 镜像：nvsLegacyOffset / nvsOverrideSpeedLimit
4. 更新 dashHandler->legacyOffset / dashHandler->overrideSpeedLimit
5. dashApplyRuntimeState() + dashSavePrefs()（提交 NVS）
6. 返回成功响应
```

**关键坑（必须处理）**：第 3 步更新 nvs 镜像不可省略。否则下次 handler 切换/can 状态变化触发 `dashApplyRuntimeState()` 时，用旧镜像把刚设的值覆盖回去——表面设置成功，转眼丢失。

**验证闭环**：设置 → 重启 → Dashboard 值保留 → 录制验证 0x2F8 b5 改动（offset≠0 不再短路）+ 0x438 出现 TX（override=true 不再短路）。

**测试**：
- native 单测（若 handleConfig JSON 解析能抽可测函数）：验证 legacyOffset/overrideSpeedLimit 解析 + 镜像更新
- 真机回归（必须）：完整 NVS 往返，证伪"重启丢配置"

### 第 2 块：UI 统一

**改动**：`renderPluginsStatus()`（`mcp2515_dashboard_ui.src.h:2649-2665`）裸 HTML → 套现有设计系统类。

| 裸元素 | 改成 | 复用类（行号）|
|--------|------|-------------|
| 原生 checkbox | 胶囊开关 | `.tgl`/`.tgl-track`（L431）|
| number input | 数字框 | `.mock-input`（L442）|
| 无 class 删除按钮 | 渐变按钮+危险态 | `.ds-btn`（L443）|
| 裸 details | 折叠区 | `.collapse`（L447）|
| .plugin-item 容器 | 半透明卡片行 | 新增 CSS，复刻 `.ctl-row`（L426）|
| .warn/.hint | 全局色 | 新增 `color:var(--warn)`/`var(--cockpit-dim)` |

**新增 CSS**：`mcp2515_dashboard_ui.src.h:446` 后补 `.plugin-item` 等规则。

**关键坑**：改 `ui.src.h` 后**必须重新生成嵌入字符串头**（`ui.h`/`dashboard.h`），否则浏览器加载旧 JS。

### 第 3 块：脱敏

**策略**：新工作空间**不复制 `.git` 历史**（全新 `git init`），只搬代码文件，旧 commit 里的板 MAC/个人路径天然不带入。

| 类别 | 处理 |
|------|------|
| 板 MAC `28:84:85:49:93:fc` | 占位 `XX:XX:XX:XX:XX:XX` |
| 个人路径 `/Users/ziwind` | 相对路径或移除 |
| WiFi 凭证 `DASH_SSID`/`DASH_PASS` | 占位 `CHANGE_ME` |
| 账号/密钥 | 移除，用环境变量/占位 |
| CAN 录制 CSV | 不进 repo（本地保留）|
| 历史 report / memory | 不进 repo |

### 第 4 块：开源发布

**流程**：
```
新工作空间 git init（不复制 .git）
  → 扫描脱敏 + platformio.ini 瘦身（只留 waveshare_single_can_standalone）
  → LICENSE(GPL-3.0) + README + .gitignore + DISCLAIMER
  → pio run -t clean && pio run（干净构建验证）
  → 本地烧录回归（板 MAC 那块板，smoke 通过）
  → 固件资产打包（bootloader/partitions/firmware + SHA256 + 烧录脚本）
  → GitHub 新 public repo + push + tag v1.0.0-atlas-single-can + Release
```

**README 章节**：
1. 顶部强免责声明（醒目框）：仅供研究教育、禁公共道路、可能违反当地法规和 Tesla 服务条款、自担风险
2. 项目简介 + 功能列表
3. 硬件清单（微雪 ESP32 单 CAN 板 + CAN H/L 接线）
4. 构建 + 烧录步骤（platformio / esptool）
5. Dashboard 使用说明
6. 贡献指南 + GPL-3.0 许可证

**.gitignore**：`.pio/`、`.vscode/`、固件二进制（Release 发）、`.env`、`secrets`、CAN 录制。

**release 资产**：固件二进制放 GitHub Releases（不进 git 历史），tag 关联。

**事故文档**：`EPAS-NAG-REMOVAL-INCIDENT.md` 进 repo 用**精简版**（教训总结 + 安全原则，不暴露具体扭矩值/counter 细节）。Jordan 审 spec 时可定要不要进。

---

## 6. 工作空间结构

**新路径**：`/Users/ziwind/my-vibe-project/waveshare-single-can-firmware/`（从 `LILYGO-T-2Can-firmware` 复制，独立成项）。

**platformio.ini 瘦身**：只保留 `[env:waveshare_single_can_standalone]`，删除 LILYGO T-2CAN 双 CAN 等其他 env。公共代码（handlers.h、dashboard、plugin engine）全保留——单 CAN 构建依赖。

---

## 7. 测试与验证策略

| 块 | 验证 | 通过标准 |
|----|------|---------|
| 持久化 | 真机回归（必须）+ native 单测 | 重启后配置不丢，0x2F8/0x438 注入 |
| UI 统一 | 浏览器截图对比 | 列表项视觉与主卡一致 |
| 0x370 禁令守卫 | `test_no_epas_nag_contract.py` | 2/2 绿 |
| 构建 | `pio run -t clean && pio run` | SUCCESS |
| 烧录 smoke | 板 MAC 那块板 | 干净启动无 panic |

---

## 8. 风险与边界

- **v1.0 硬约束**：EPAS nag 禁令守卫保持绿，**不碰 0x370**。bit-19 baseline 是 v1.0 nag 上限。
- **禁令**：禁止未经台架验证的 0x370 注入。任何 0x370 复活须台架验证（车不上路）+ 明确安全方式 + Jordan 书面授权。
- **持久化修复关键坑**：必须更新 nvs 镜像，否则 dashApplyRuntimeState 覆盖。
- **UI 关键坑**：改 ui.src.h 必须重新生成嵌入字符串头。
- **速度偏移算法**：v1.0 先修持久化，实车再评估。若仍不够，`dashComputeLegacySimpleOffsetKph` 优化 → v1.1。

---

## 9. v1.1 backlog

1. nag 0x370 台架增强（台架 + 书面授权 + 安全方式设计）
2. `test_hw4_checkAD_blocks` 测试语义对齐（不改注入代码）
3. 速度偏移算法优化（若实车仍不够）

---

## 10. 安全约束（life-critical，不可违背）

- ⛔ 禁止未经台架验证的 0x370 注入
- 守卫 `test/test/test_no_epas_nag_contract.py` 必须保持绿（8 symbols + 禁止 `[env:native_epas_nag]`）
- 功能③方向盘屏蔽只走 bit-19 baseline（0x3EE/0x3FD/1021 mux-1 setBit(19,false)），非 0x370
- 所有新增注入保留三重门控：`fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed()`
- 烧录前 esptool `chip_id` 确认板 MAC

---

## 11. 关键 file:line 索引

| 位置 | file:line |
|----|-----------|
| 字段定义 | `include/handlers.h:63-70` |
| 0x2F8 offset 短路 | `include/handlers.h:324-325` |
| 0x438 override 短路 | `include/handlers.h:341` |
| **持久化断点：UI POST /config** | `include/web/mcp2515_dashboard_ui.src.h:3622-3646` |
| **持久化断点：handleConfig 不解析 JSON** | `include/web/mcp2515_dashboard.h:2481-3025` |
| 唯一能写 fg/fi 的 handler（错端点）| `include/web/mcp2515_dashboard.h:5656, 5904-5937` |
| dashSavePrefs 写 NVS | `include/web/mcp2515_dashboard.h:1399-1407` |
| 启动加载 NVS | `include/web/mcp2515_dashboard.h:1488, 1617-1626` |
| 启动应用 nvs→字段 | `include/web/mcp2515_dashboard.h:1282-1307, 6594-6615` |
| **UI 断点：renderPluginsStatus** | `include/web/mcp2515_dashboard_ui.src.h:2649-2665` |
| UI 设计系统类 | `include/web/mcp2515_dashboard_ui.src.h:426-447` |
| /defense_config 写 fd/fj（链路通）| `include/web/mcp2515_dashboard.h:3053-3066` |
