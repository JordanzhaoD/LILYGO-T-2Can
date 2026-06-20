# 单 CAN 微雪版 v1.0 稳定版加固与开源发布 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复持久化 bug + UI 风格统一，将单 CAN 微雪版作为 GPL-3.0 开源稳定版发布到 GitHub。

**Architecture:** 在 `fix/remove-dangerous-epas-nag` 分支修复代码（Task 1-4），然后在新工作空间 `waveshare-single-can-firmware/` 脱敏 + 开源发布（Task 5-8）。新工作空间不复制 `.git` 历史，规避旧 commit 敏感信息。

**Tech Stack:** ESP-IDF (C++17), platformio espressif32@7.0.0, ArduinoJson v7, 单 CAN env `waveshare_single_can_standalone` (esp32s3box/16MB), 原生 unity 测试 + pytest 守卫。

**Spec:** `docs/superpowers/specs/2026-06-21-stable-release-hardening-design.md`

**安全约束（life-critical，贯穿全计划）:** ⛔ 不碰 0x370；`test_no_epas_nag_contract.py` 必须保持绿；功能③只走 bit-19 baseline；烧录前 `chip_id` 确认板 MAC。

---

## Task 1: 持久化修复 — handleConfig 加 JSON 分支

**根因**：UI `postConfigJson()`（ui.src.h:3622-3646）POST `{fsdRuntime:{legacyOffset,overrideSpeedLimit}}` 到 `/config`，但 `handleConfig`（mcp2515_dashboard.h:2481）只读 form 参数、不解析 JSON body → 静默丢弃 → NVS 永不写 → 重启丢配置。

**修复**：handleConfig 开头加 JSON 解析分支，更新 nvs 镜像 + 应用 + 提交。

**Files:**
- Modify: `include/web/mcp2515_dashboard.h:2481`（handleConfig 开头）

- [ ] **Step 1: 确认 handleConfig 当前开头**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && sed -n '2481,2484p' include/web/mcp2515_dashboard.h`
Expected:
```
static void handleConfig()
{
    bool hwChanged = false;
```

- [ ] **Step 2: 在 handleConfig 开头插入 JSON 分支**

Edit `include/web/mcp2515_dashboard.h`，把：
```cpp
static void handleConfig()
{
    bool hwChanged = false;
```
替换为：
```cpp
static void handleConfig()
{
    // --- FSD runtime JSON body ({fsdRuntime:{legacyOffset,overrideSpeedLimit}}) ---
    // UI postConfigJson() posts here as application/json. Without this branch the
    // body was silently dropped and legacyOffset/overrideSpeedLimit never reached
    // NVS (reboot reset them). Must update nvs mirror THEN dashApplyRuntimeState,
    // else the next apply overwrites with the stale mirror.
    if (server.hasArg("plain"))
    {
        JsonDocument doc;
        DeserializationError jsonErr = deserializeJson(doc, server.arg("plain"));
        if (!jsonErr && doc["fsdRuntime"].is<JsonObject>())
        {
            JsonObject fsd = doc["fsdRuntime"].as<JsonObject>();
            bool changed = false;
            if (fsd["legacyOffset"].is<int>())
            {
                int offset = fsd["legacyOffset"].as<int>();
                if (offset < -30) offset = -30;
                if (offset > 225) offset = 225;
                nvsLegacyOffset = offset;
                changed = true;
            }
            if (fsd["overrideSpeedLimit"].is<bool>())
            {
                nvsOverrideSpeedLimit = fsd["overrideSpeedLimit"].as<bool>();
                changed = true;
            }
            if (changed)
            {
                dashApplyRuntimeState();  // nvs mirror -> dashHandler fields
                dashSavePrefs();          // dashHandler fields -> NVS commit
                dashLog("[CFG] fsdRuntime JSON: legacyOffset/overrideSpeedLimit applied");
            }
        }
    }
    bool hwChanged = false;
```

**为何用 `nvsLegacyOffset`/`nvsOverrideSpeedLimit` 镜像而非直接 `prefs.putX`**：`dashApplyRuntimeState()`（:1282）从镜像写 dashHandler，`dashSavePrefs()`（:1399-1407）从 dashHandler 写 NVS。只更新镜像 + 调这两个函数，保证镜像/dashHandler/NVS 三者一致，避免下次 apply 用旧镜像覆盖（隐蔽陷阱）。

- [ ] **Step 3: 构建验证（编译通过）**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e waveshare_single_can_standalone 2>&1 | tail -5`
Expected: `SUCCESS` + RAM/Flash 占用，无 `JsonDocument`/`deserializeJson` 未定义错误。

- [ ] **Step 4: 守卫测试保持绿（0x370 禁令未触碰）**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e native -t test 2>&1 | tail -3 && python test/test/test_no_epas_nag_contract.py 2>&1 | tail -3`
Expected: native 测试通过 + `OK`（守卫 2/2 绿）。

- [ ] **Step 5: Commit**

```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware
git add include/web/mcp2515_dashboard.h
git commit -m "fix(persist): handleConfig parse fsdRuntime JSON for legacyOffset/overrideSpeedLimit

UI postConfigJson posted {fsdRuntime:{...}} to /config but handleConfig only
read form args, silently dropping the JSON body -> NVS never written -> reboot
reset legacyOffset/overrideSpeedLimit. Add JSON branch: update nvs mirror then
dashApplyRuntimeState + dashSavePrefs (mirror->handler->NVS commit). Closes the
reboot-resets-config bug (0x438 zero TX root cause, data-confirmed)."
```

---

## Task 2: UI 统一 — renderPluginsStatus 套驾驶舱类

**根因**：`renderPluginsStatus()`（ui.src.h:2649-2665）用 innerHTML 拼的列表项是裸 HTML（原生 checkbox/无 class 按钮/裸 details），`.plugin-item` 无 CSS 定义。

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h:447`（.collapse 后加 CSS）
- Modify: `include/web/mcp2515_dashboard_ui.src.h:2655-2661`（renderPluginsStatus 模板）

- [ ] **Step 1: 在 .collapse CSS 后追加插件列表项样式**

Edit `include/web/mcp2515_dashboard_ui.src.h`，在：
```css
.collapse { margin-top:14px; background:rgba(255,255,255,.03); border:1px solid var(--ds-card-stroke); border-radius:10px; padding:10px 14px; font-size:11.5px; color:var(--cockpit-dim); }
```
之后追加：
```css
/* === 已安装插件列表项（renderPluginsStatus，套驾驶舱设计系统） === */
.plugin-item { background:rgba(255,255,255,.03); border:1px solid var(--ds-card-stroke); border-radius:11px; padding:11px 15px; }
.plugin-item .pi-head { display:flex; align-items:center; justify-content:space-between; gap:10px; flex-wrap:wrap; }
.plugin-item .pi-name { font-weight:600; color:var(--tx1); font-size:13px; }
.plugin-item .pi-ver { color:var(--cockpit-dim); font-size:10.5px; }
.plugin-item .pi-controls { display:flex; align-items:center; gap:10px; }
.plugin-item .pi-prio { width:56px; margin-bottom:0; text-align:center; }
.plugin-item .pi-hint { font-size:10.5px; color:var(--cockpit-dim); margin-top:6px; }
.plugin-item .pi-warn { font-size:10.5px; color:var(--warn); margin-top:4px; }
.plugin-item .pi-details { margin-top:8px; }
.plugin-item .pi-details > pre { margin:6px 0 0; background:rgba(0,0,0,.25); border:1px solid var(--ds-card-stroke); border-radius:7px; padding:8px; font-size:10px; color:var(--tx2); overflow:auto; }
.ds-btn-danger { background:linear-gradient(135deg,#991b1b,#ef4444); }
```

**为何用 `.pi-*` 局部类而非全局 `.warn`/`.hint`**：避免污染全局样式（其他页面可能用 `.warn`/`.hint`），作用域限定在 `.plugin-item` 内最安全。

- [ ] **Step 2: 重写 renderPluginsStatus 列表项模板**

Edit `include/web/mcp2515_dashboard_ui.src.h`，把：
```javascript
    return '<div class="plugin-item"><div><b>'+esc(name)+'</b> <small>'+esc(p.version||'1.0')+'</small>'
      +'<div class="hint">目标ID: '+esc(ids)+' · 规则: '+(p.ruleCount||0)+'</div>'
      +'<div class="warn">'+esc(conflicts||p.lastError||'')+'</div></div>'
      +'<label>启用 <input type="checkbox" '+(p.enabled?'checked':'')+' onchange="togglePlugin(\''+escAttr(name)+'\',this.checked)"></label>'
      +'<label>优先级 <input type="number" min="1" max="8" value="'+(p.priority||1)+'" onchange="setPluginPriority(\''+escAttr(name)+'\',this.value)"></label>'
      +'<button onclick="removePlugin(\''+escAttr(name)+'\')">删除</button>'
      +'<details><summary>详情</summary><pre>'+esc(JSON.stringify(p,null,2))+'</pre></details></div>';
```
替换为：
```javascript
    return '<div class="plugin-item">'
      +'<div class="pi-head"><div><span class="pi-name">'+esc(name)+'</span> <small class="pi-ver">'+esc(p.version||'1.0')+'</small></div>'
      +'<div class="pi-controls">'
      +'<label class="tgl" title="启用"><input type="checkbox" '+(p.enabled?'checked':'')+' onchange="togglePlugin(\''+escAttr(name)+'\',this.checked)"><div class="tgl-track"></div></label>'
      +'<input class="mock-input pi-prio" type="number" min="1" max="8" value="'+(p.priority||1)+'" onchange="setPluginPriority(\''+escAttr(name)+'\',this.value)" title="优先级">'
      +'<button class="ds-btn ds-btn-danger" onclick="removePlugin(\''+escAttr(name)+'\')">删除</button>'
      +'</div></div>'
      +'<div class="pi-hint">目标ID: '+esc(ids)+' · 规则: '+(p.ruleCount||0)+'</div>'
      +(conflicts||p.lastError ? '<div class="pi-warn">'+esc(conflicts||p.lastError)+'</div>' : '')
      +'<details class="collapse pi-details"><summary>详情</summary><pre>'+esc(JSON.stringify(p,null,2))+'</pre></details>'
      +'</div>';
```

**映射**：checkbox→`.tgl`/`.tgl-track`（胶囊开关），number→`.mock-input.pi-prio`，button→`.ds-btn.ds-btn-danger`（红渐变），details→`.collapse.pi-details`，hint/warn→`.pi-hint`/`.pi-warn`。

- [ ] **Step 3: 重新生成 ui.h（minify_dashboard.py）**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e waveshare_single_can_standalone 2>&1 | tail -3`
Expected: `SUCCESS`（构建自动触发 minify_dashboard.py 重新生成 ui.h）。

验证 ui.h 已更新：
Run: `git diff --stat include/web/mcp2515_dashboard_ui.h`
Expected: 文件有改动（minified 字符串变了）。若空，手动跑 `python scripts/minify_dashboard.py` 再构建。

- [ ] **Step 4: Commit**

```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h
git commit -m "style(ui): 已安装插件列表项套驾驶舱设计系统类

renderPluginsStatus 的裸 HTML (原生 checkbox/无 class 按钮/裸 details) 改用
现有 .tgl/.mock-input/.ds-btn/.collapse 类 + 新增 .plugin-item/.pi-* CSS。
消除插件卡内上下视觉割裂 (外壳已换皮, 列表项此前是默认浏览器样式)。"
```

---

## Task 3: 代码回归 + 守卫审计

**Files:** 无修改，纯验证。

- [ ] **Step 1: 干净构建**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e waveshare_single_can_standalone -t clean && pio run -e waveshare_single_can_standalone 2>&1 | tail -3`
Expected: `SUCCESS`（注意：clean 后 version 串会刷新，避免 .pio/build 缓存旧 commit 号）。

- [ ] **Step 2: 全量 native 测试**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && for e in native native_hw3 native_hw4 native_legacy native_plugin_engine; do echo "=== $e ==="; pio run -e $e -t test 2>&1 | tail -2; done`
Expected: 每个 env 测试通过。记录 `test_hw4_checkAD_blocks`（既有失败，backlog，非本次引入）外的通过情况。

- [ ] **Step 3: 0x370 禁令守卫（life-critical）**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python test/test/test_no_epas_nag_contract.py`
Expected: `OK`（8 symbols 守卫 + 禁止 `[env:native_epas_nag]`，2/2 绿）。

- [ ] **Step 4: 记录回归结果**

在 commit message 或 handoff 记录：构建 SUCCESS、native 通过数、守卫 OK、既有失败清单。

---

## Task 4: 真机烧录 + 持久化回归（Jordan 协作）

**这是 Jordan 硬件操作 + 我分析。持久化 bug 只能真机证伪（spec 决策：只靠真机回归）。**

- [ ] **Step 1: 确认板 MAC**

Run: `esptool.py --port /dev/cu.usbserial-* chip_id`（Jordan 提供端口）
Expected: MAC `28:84:85:49:93:fc`（确认是 Jordan 单 CAN 微雪板）。

- [ ] **Step 2: 烧录固件**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e waveshare_single_can_standalone -t upload 2>&1 | tail -3`（或 esptool 逐分区烧）
Expected: `Hash verified`，version 显示最新 commit。

- [ ] **Step 3: smoke 测试（台架未接车）**

串口监听：`pio device monitor -e waveshare_single_can_standalone`
Expected: 干净启动，无 panic，无 `epas_nag` 日志行，`canActive=NO`（台架）。

- [ ] **Step 4: 持久化回归（核心验证）**

Jordan 操作：
1. Dashboard 连板 → 切 `hwMode=0` LEGACY
2. pg-overview Legacy FSD 卡：设 `legacy-offset-inp` = 某非零值（如 5），`legacy-override-tgl` = 开
3. **重启板子**（断电或 reboot 命令）
4. 重启后 Dashboard 检查：legacy-offset 仍显示 5，override 仍开

Expected: 重启后值保留（修复前会归 0/关）。若仍丢 → 回 Task 1 排查（可能镜像更新逻辑或端点 body 格式问题）。

- [ ] **Step 5: 接车 FSD 实测 + 录制（Jordan 操作）**

Jordan 接车开 FSD，录制 CAN（含 TX）：
1. 设 legacyOffset + overrideSpeedLimit（非默认）
2. FSD 激活期间录制 ~30 秒
3. 发我录制 CSV

Expected（我分析）：0x438 出现 dir=T（overrideSpeedLimit=true 不再短路），0x2F8 b5 偏移量符合设的 offset。

- [ ] **Step 6: 0x370 仍零 TX（安全确认）**

我分析录制确认 0x370 无 dir=T、无注入签名 b3=182/handsOn bit6。

- [ ] **Step 7: 记录回归结论**

更新 memory：持久化修复实车验证 ✅/❌，0x438 TX 结论，0x370 安全确认。

---

## Task 5: 新工作空间 + 脱敏

**Files:**
- Create: `/Users/ziwind/my-vibe-project/waveshare-single-can-firmware/`（新工作空间）

- [ ] **Step 1: 复制 repo（不复制 .git）**

Run:
```bash
cd /Users/ziwind/my-vibe-project
rsync -a --exclude='.git' --exclude='.pio' --exclude='firmware-assets' \
  --exclude='.vscode' --exclude='*.csv' \
  LILYGO-T-2Can-firmware/ waveshare-single-can-firmware/
```
Expected: 新目录生成，无 `.git`/`.pio`/资产/录制。

- [ ] **Step 2: platformio.ini 瘦身（只留单 CAN env + native 测试 env）**

Edit `waveshare-single-can-firmware/platformio.ini`，删除这些 env 段（保留 `[env:waveshare_single_can_standalone]` + 所有 `[env:native*]` 测试 env）：
- `[env:esp32_twai]`、`[env:esp32s2_twai]`、`[env:esp32_feather_v2_mcp2515]`
- `[env:lilygo_tcan485_hw3]`、`[env:m5stack-atomic-can-base]`、`[env:m5stack-atoms3-mini-can-base]`
- `[env:esp32_ext_mcp2515]`、`[env:waveshare_ESP32_S3_RS485_CAN]`、`[env:lilygo_t2can_dual]`

Run: `cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware && grep -c "^\[env:" platformio.ini`
Expected: 只剩 waveshare_single_can_standalone + native* env（约 12-14 个）。

- [ ] **Step 3: 扫描脱敏**

Run（在新工作空间）:
```bash
cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware
echo "=== 板 MAC ===" ; grep -rn "28:84:85:49:93:fc\|2884854993fc" . --include='*.md' --include='*.h' --include='*.cpp' --include='*.py' --include='*.ini' 2>/dev/null | grep -v '.pio'
echo "=== 个人路径 ===" ; grep -rn "/Users/ziwind" . --include='*.md' --include='*.py' --include='*.sh' --include='*.ini' 2>/dev/null | grep -v '.pio'
echo "=== WiFi 凭证 ===" ; grep -rn "DASH_SSID\|DASH_PASS" include/ src/ platformio.ini 2>/dev/null | grep -v '.pio' | head
```
Expected: 列出所有命中。逐个改成占位（MAC→`XX:XX:XX:XX:XX:XX`，路径→相对/移除，WiFi 默认值→`CHANGE_ME`）。

- [ ] **Step 4: 删除不该开源的内容**

删除：`docs/PROJECT_HANDOFF_*.md`、`docs/superpowers/handoffs/`、`docs/EPAS-NAG-REMOVAL-INCIDENT.md`（Jordan 决策不进）、所有 `*_REPORT*.md`、CAN 录制、`ATLAS_RECOVERY_*.md` 等含个人/运维信息的文档。

Run:
```bash
cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware
rm -f docs/PROJECT_HANDOFF_*.md docs/EPAS-NAG-REMOVAL-INCIDENT.md
rm -rf docs/superpowers/handoffs/
find docs -name '*_REPORT*.md' -delete 2>/dev/null
```

- [ ] **Step 5: 新 repo git init + 干净构建验证**

```bash
cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware
git init
pio run -e waveshare_single_can_standalone -t clean && pio run -e waveshare_single_can_standalone 2>&1 | tail -3
```
Expected: 干净构建 SUCCESS（确认瘦身 + 脱敏没破坏构建）。

- [ ] **Step 6: 初始 commit（本地）**

```bash
cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware
git add -A && git commit -m "chore: initial import - waveshare single CAN standalone (脱敏开源版)"
```

---

## Task 6: 开源资产（LICENSE / README / .gitignore / DISCLAIMER）

**Files:**
- Create: `waveshare-single-can-firmware/LICENSE`（GPL-3.0）
- Create: `waveshare-single-can-firmware/README.md`
- Create: `waveshare-single-can-firmware/.gitignore`
- Create: `waveshare-single-can-firmware/DISCLAIMER.md`

- [ ] **Step 1: GPL-3.0 LICENSE**

Run: `cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware && curl -sL https://www.gnu.org/licenses/gpl-3.0.txt -o LICENSE && head -1 LICENSE`
Expected: `GNU GENERAL PUBLIC LICENSE`（若网络不可用，从 https://www.gnu.org/licenses/gpl-3.0.txt 全文复制）。

- [ ] **Step 2: .gitignore**

Create `waveshare-single-can-firmware/.gitignore`:
```
.pio/
.vscode/
*.bin
*.elf
*.map
.env
secrets/
CAN_*.csv
can_recording*
*.log
```

- [ ] **Step 3: DISCLAIMER.md（强免责）**

Create `waveshare-single-can-firmware/DISCLAIMER.md`（顶部醒目免责，中文+英文）：
```markdown
# ⚠️ 免责声明 / Disclaimer

**仅限研究、教育与工程学习目的。/ For research, educational, and engineering study purposes only.**

- 本项目涉及车辆 CAN 总线交互，**禁止在公共道路使用**。/ This project interacts with vehicle CAN bus. **Do NOT use on public roads.**
- 使用本项目的代码/固件**可能违反当地交通法规与车辆制造商服务条款**。/ Use may violate local traffic laws and the vehicle manufacturer's Terms of Service.
- 使用者**自行承担一切法律与安全风险**，作者不承担任何责任。/ Users assume ALL legal and safety risks. The author bears NO responsibility.
- 车辆改装/调试可能导致**车辆故障、事故、保修失效、保险拒赔**。/ Vehicle modification may cause malfunction, accidents, warranty void, insurance denial.

**继续使用即表示你已阅读、理解并接受以上全部条款。/ By proceeding you accept all terms above.**
```

- [ ] **Step 4: README.md**

Create `waveshare-single-can-firmware/README.md`（章节：顶部免责框 + 简介 + 功能 + 硬件清单 + 构建 + 烧录 + Dashboard + 贡献 + GPL-3.0）。顶部第一行必须是免责警示框链接 DISCLAIMER.md。

- [ ] **Step 5: Commit**

```bash
cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware
git add LICENSE README.md .gitignore DISCLAIMER.md
git commit -m "docs: add GPL-3.0 LICENSE, README, strong disclaimer, gitignore"
```

---

## Task 7: 固件资产打包（Release 用）

**Files:**
- Create: `waveshare-single-can-firmware/firmware-v1.0.0/`（资产目录，不进 git，附 Release）

- [ ] **Step 1: 确认资产路径**

Run: `cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware && ls -1 .pio/build/waveshare_single_can_standalone/ | grep -E "bootloader|partition|firmware"`
Expected: `bootloader.bin`、`partitions.bin`、`firmware.bin`（ESP-IDF 构建产物）。

- [ ] **Step 2: 打包资产 + SHA256**

Run:
```bash
cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware
mkdir -p release/v1.0.0-atlas-single-can
cp .pio/build/waveshare_single_can_standalone/bootloader.bin release/v1.0.0-atlas-single-can/
cp .pio/build/waveshare_single_can_standalone/partitions.bin release/v1.0.0-atlas-single-can/
cp .pio/build/waveshare_single_can_standalone/firmware.bin release/v1.0.0-atlas-single-can/
cd release/v1.0.0-atlas-single-can && shasum -a 256 *.bin > SHA256SUMS && cat SHA256SUMS
```
Expected: 三个 bin + SHA256SUMS 文件。

- [ ] **Step 3: 烧录脚本（flash.sh）**

Create `waveshare-single-can-firmware/release/v1.0.0-atlas-single-can/flash.sh`:
```bash
#!/usr/bin/env bash
# 烧录 waveshare single CAN standalone v1.0.0
# 用法: ./flash.sh /dev/cu.usbserial-XXXX
set -e
PORT="${1:-/dev/cu.usbserial-XXXX}"
esptool.py --chip esp32s3 --port "$PORT" --baud 921600 \
  write_flash -z \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
echo "✅ 烧录完成。串口监听: pio device monitor 或 esptool.py run"
```
Run: `chmod +x release/v1.0.0-atlas-single-can/flash.sh`

- [ ] **Step 4: 验证资产可烧（在 Jordan 板上）**

Jordan 用 flash.sh 烧录板 MAC `28:84:85:49:93:fc`，smoke 通过。这是 release 前的最终验证。

---

## Task 8: GitHub 发布（Jordan 账号 + 我执行）

- [ ] **Step 1: Jordan 创建空 public repo**

Jordan 在 GitHub 创建空 public repo（不带 README/LICENSE/gitignore，本地已有）。记录 repo URL（如 `github.com/jordan/waveshare-single-can-firmware`）。

- [ ] **Step 2: 关联远程 + push**

Run:
```bash
cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware
git remote add origin <Jordan提供URL>.git
git branch -M main
git push -u origin main
```
Expected: push 成功（本地 commit 全部上传）。

- [ ] **Step 3: 打 tag v1.0.0-atlas-single-can**

```bash
cd /Users/ziwind/my-vibe-project/waveshare-single-can-firmware
git tag -a v1.0.0-atlas-single-can -m "v1.0.0-atlas-single-can: 首个公开稳定版 (GPL-3.0)"
git push origin v1.0.0-atlas-single-can
```

- [ ] **Step 4: GitHub Release（附固件资产）**

Jordan 在 GitHub → Releases → Draft new release：
- 选 tag `v1.0.0-atlas-single-can`
- Title: `v1.0.0-atlas-single-can`
- Body: 简介 + 免责警示 + 硬件清单 + 烧录步骤（flash.sh）
- Attach: `release/v1.0.0-atlas-single-can/` 下的 bootloader.bin、partitions.bin、firmware.bin、SHA256SUMS、flash.sh
- Publish

- [ ] **Step 5: 更新 memory**

记录：开源 repo URL、tag、release 链接、版本号方案（release tag v1.0.0 对外，固件 build_id 沿用内部机制）。

---

## 自审

**Spec coverage**:
- 持久化修复（spec §5 第1块）→ Task 1 ✓
- UI 统一（spec §5 第2块）→ Task 2 ✓
- 脱敏（spec §5 第3块）→ Task 5 Step 3-4 ✓
- 开源发布（spec §5 第4块）→ Task 5-8 ✓
- 真机回归（spec §7）→ Task 4 ✓
- 守卫（spec §7/§10）→ Task 1 Step 4 + Task 3 Step 3 ✓
- v1.1 backlog（spec §9）→ 不在本计划，记录在 spec ✓
- 事故文档不进 repo（Jordan 决策）→ Task 5 Step 4 ✓

**Placeholder scan**: 无 TBD/TODO；所有代码步骤含完整代码；命令含 expected output ✓

**Type consistency**: `nvsLegacyOffset`(int)/`nvsOverrideSpeedLimit`(bool) 在 Task 1 与 handlers.h:65-66、dashboard.h:217/219/1302-1303 一致；ArduinoJson v7 `JsonDocument` 与 handleSettingsImport 用法一致；CSS 类 `.pi-*` 在 Task 2 Step 1（定义）与 Step 2（使用）一致 ✓
