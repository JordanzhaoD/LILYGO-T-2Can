# 单 CAN 微雪版 FSD 功能对齐 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 tesla-fsd-controller 的 5 个 FSD 功能对齐到单 CAN 微雪版（`waveshare_single_can_standalone`），实车 Legacy 模式——补 2 个真缺口（HW3 chime、HW4 重写限速 5b）+ 功能 2 门控独立化 + 配置统一，全程不碰 0x370。

**Architecture:** 改动集中在 `include/handlers.h`（HW3Handler 921 分支补 chime、HW4Handler 新增 923 分支、LegacyHandler 功能 2 去 policy 门控）+ `include/web/mcp2515_dashboard.{h,ui.src.h}`（isaOverride 新增 NVS/JSON/UI 链路，参照 `isaChimeSuppress`/`fd`）。TDD：先在 `test/test_native_{legacy,hw3,hw4}/` 写失败测试，再实现。守卫测试 `test_no_epas_nag_contract.py` 全程保持绿。

**Tech Stack:** ESP-IDF (platformio espressif32@7.0.0)、C++17、unity 测试框架（native）、pytest（契约守卫）、ArduinoJson（/api/config）。

**Spec:** `docs/superpowers/specs/2026-06-20-single-can-legacy-fsd-alignment-design.md`

---

## File Structure

| 文件 | 责任 | 改动 |
|------|------|------|
| `include/handlers.h` | 三套 CAN handler（Legacy/HW3/HW4）+ CarManagerBase 状态 | Task 1（:465 门控）、Task 2（HW3 921）、Task 3（声明 isaOverride + HW4 923） |
| `test/test_native_legacy/test_legacy_handler.cpp` | Legacy handler native 测试 | Task 1 新增视觉限速 clear 测试 |
| `test/test_native_hw3/test_hw3_handler.cpp` | HW3 handler native 测试 | Task 2 新增 chime 测试 |
| `test/test_native_hw4/test_hw4_handler.cpp` | HW4 handler native 测试 | Task 3 新增 923 重写限速测试 |
| `include/web/mcp2515_dashboard.h` | NVS 持久化 + REST API + 配置应用 | Task 3 isaOverride 的 NVS/JSON/POST 链路 |
| `include/web/mcp2515_dashboard_ui.src.h` | Dashboard UI HTML/JS 源 | Task 3 defense 页新增 isaOverride 开关 |
| `platformio.ini` | 构建环境 | Task 5 先决：`waveshare_single_can_standalone` 的 Legacy 默认模式 |

---

## Task 1: 功能 2 视觉限速 clear 独立门控（Legacy）

去掉 `policy==Experimental` 限制，让 `visionLimitClearEnable` 在任意 policy 下独立生效。

**Files:**
- Modify: `include/handlers.h:465`
- Test: `test/test_native_legacy/test_legacy_handler.cpp`

- [ ] **Step 1: 写失败测试**（验证 Stable policy + visionLimitClearEnable 也 clear bit48）

在 `test/test_native_legacy/test_legacy_handler.cpp` 末尾（最后一个 `RUN_TEST` 之前，或 `int main()` 的 UNITY_END 前）追加：

```cpp
// --- 视觉限速 clear 独立于 policy (spec Task 1) ---

void test_legacy_vision_clear_works_under_stable_policy()
{
    // Stable policy + visionLimitClearEnable 应当 clear bit48
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::Stable;
    handler.legacyFsdDiag.visionLimitClearEnable = true;
    handler.fsdTriggered = true;   // injectionAllowed() 默认通过(Parked=true)

    CanFrame f = {.id = 1006};     // 0x3EE
    f.data[0] = 0x01;              // mux 1
    f.dlc = 8;
    // bit48 = byte6 bit0 — 先置1，期望被清
    f.data[6] = 0x01;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x00, mock.sent[0].data[6] & 0x01); // bit48 cleared
}

void test_legacy_vision_clear_disabled_does_not_clear_bit48()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::Stable;
    handler.legacyFsdDiag.visionLimitClearEnable = false;
    handler.fsdTriggered = true;

    CanFrame f = {.id = 1006};
    f.data[0] = 0x01;              // mux 1
    f.dlc = 8;
    f.data[6] = 0x01;              // bit48 set
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x01, mock.sent[0].data[6] & 0x01); // bit48 preserved
}
```

并在 `main()` 的 `RUN_TEST` 区追加：
```cpp
RUN_TEST(test_legacy_vision_clear_works_under_stable_policy);
RUN_TEST(test_legacy_vision_clear_disabled_does_not_clear_bit48);
```

> 注：若 `test_legacy_handler.cpp` 的 mux-1 注入路径需要额外前置（如 `builtInFsdInjectionAllowed()`），参照该文件现有 mux-1 测试设置；Parked 默认 true 使 `injectionAllowed()` 通过。

- [ ] **Step 2: 跑测试确认失败**

```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware
pio test -e native_legacy -f test_native_legacy
```
Expected: `test_legacy_vision_clear_works_under_stable_policy` **FAIL**（当前 :465 要求 `policy==Experimental`，Stable 下不清 bit48）。

- [ ] **Step 3: 改门控**

修改 `include/handlers.h:465`：
```cpp
// 旧:
                if (legacyFsdDiag.policy == LegacyFsdPolicy::Experimental && legacyFsdDiag.visionLimitClearEnable)
                    setBit(frame, 48, false);
// 新:
                if (legacyFsdDiag.visionLimitClearEnable)
                    setBit(frame, 48, false);
```

- [ ] **Step 4: 跑测试确认通过**

```bash
pio test -e native_legacy -f test_native_legacy
```
Expected: 两个新测试 **PASS**，且现有 legacy 测试全绿（无回归）。

- [ ] **Step 5: Commit**

```bash
git add include/handlers.h test/test_native_legacy/test_legacy_handler.cpp
git commit -m "feat(legacy): vision limit clear independent of policy (spec task 1)

去掉 Experimental policy 门控，让 visionLimitClearEnable 在任意 policy
下独立生效。避免 Experimental 牵连其他行为。功能 3 仍走 bit-19 baseline，
不碰 0x370。"
```

---

## Task 2: 缺口 1 — HW3 chime 抑制（功能 4）

HW3Handler 921 分支当前只读 `fusedSpeedLimitRaw`，补 ISA chime 抑制写入（照搬 `HW4Handler:856-864`）。

**Files:**
- Modify: `include/handlers.h`（HW3Handler 921 分支，约 :536-546）
- Test: `test/test_native_hw3/test_hw3_handler.cpp`

- [ ] **Step 1: 写失败测试**

在 `test/test_native_hw3/test_hw3_handler.cpp` 末尾追加（照搬 HW4 chime 测试，适配 HW3）：

```cpp
// --- ISA speed chime suppression, HW3 (CAN ID 921) — spec Task 2 ---

void test_hw3_isa_suppress_sets_bit5_of_data1()
{
    handler.isaChimeSuppress = true;
    CanFrame f = {.id = 921};
    f.data[1] = 0x00;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x20, mock.sent[0].data[1] & 0x20);
}

void test_hw3_isa_suppress_preserves_existing_data1_bits()
{
    handler.isaChimeSuppress = true;
    CanFrame f = {.id = 921};
    f.data[1] = 0xC3;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_HEX8(0xE3, mock.sent[0].data[1]); // 0xC3 | 0x20
}

void test_hw3_isa_suppress_checksum_correct()
{
    handler.isaChimeSuppress = true;
    CanFrame f = {.id = 921};
    f.data[0] = 0x10; f.data[1] = 0x05;
    f.data[2] = 0; f.data[3] = 0; f.data[4] = 0; f.data[5] = 0; f.data[6] = 0;
    handler.handleMessage(f, mock);
    // OR 后 data[1]=0x25; checksum = id_lo+id_hi + data[0..6] = 0x99+0x03+0x10+0x25 = 0xD1
    TEST_ASSERT_EQUAL_HEX8(0xD1, mock.sent[0].data[7]);
}

void test_hw3_isa_suppress_runtime_off_skips_send()
{
    handler.isaChimeSuppress = false;
    CanFrame f = {.id = 921};
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw3_isa_suppress_still_captures_fused_limit()
{
    // 抑制开启时仍应读取 fusedSpeedLimitRaw（不能因 send 而 skip 读取）
    handler.isaChimeSuppress = true;
    CanFrame f = {.id = 921};
    f.data[1] = 0x15;             // fused limit raw = 0x15
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_UINT8(0x15, fusedSpeedLimitRaw);
}
```

并在 `main()` 追加对应 `RUN_TEST(...)`（5 个）。

> 注：`fusedSpeedLimitRaw` 是全局变量（`dash_hw3_speed.h` 声明），测试可直接读。若 setUp 未重置它，在测试内先赋值。

- [ ] **Step 2: 跑测试确认失败**

```bash
pio test -e native_hw3 -f test_native_hw3
```
Expected: 前 4 个 chime 测试 **FAIL**（HW3 921 当前不写 |=0x20）；最后一个 fused 读取测试可能 PASS（现有逻辑已读）。

- [ ] **Step 3: 实现 HW3 921 分支**

修改 `include/handlers.h` 的 HW3Handler 921 分支（当前 :536-546）：

```cpp
// 旧:
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            APActive = isDASAutopilotActive(readDASAutopilotStatus(frame));
            if (frame.dlc >= 2)
                fusedSpeedLimitRaw = static_cast<uint8_t>(frame.data[1] & 0x1F);
            return;
        }
// 新:
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            APActive = isDASAutopilotActive(readDASAutopilotStatus(frame));
            if (frame.dlc >= 2)
                fusedSpeedLimitRaw = static_cast<uint8_t>(frame.data[1] & 0x1F);
            // ISA chime suppress — runtime gate (spec Task 2, 对齐 HW4Handler:856-864)
            if ((bool)isaChimeSuppress && frame.dlc >= 8 && injectionAllowed())
            {
                frame.data[1] |= 0x20;
                frame.data[7] = computeVehicleChecksum(frame);
                framesSent++;
                driver.send(frame);
                if (onSend) onSend(0, true);
                return;
            }
            return;
        }
```

> 关键：先读 `fusedSpeedLimitRaw`（保留现有行为），再判断 chime 抑制。读取在写入之前，确保 `test_hw3_isa_suppress_still_captures_fused_limit` 通过。

- [ ] **Step 4: 跑测试确认通过**

```bash
pio test -e native_hw3 -f test_native_hw3
```
Expected: 5 个新测试 **PASS**，现有 HW3 测试全绿。

- [ ] **Step 5: Commit**

```bash
git add include/handlers.h test/test_native_hw3/test_hw3_handler.cpp
git commit -m "feat(hw3): ISA chime suppression on CAN 921 (spec task 2, gap 1)

HW3Handler 921 分支补 |=0x20 chime 抑制写入，对齐 HW4Handler:856-864。
isaChimeSuppress 复用现有 fd NVS + UI(sound_warning_suppression)。不碰 0x370。"
```

---

## Task 3: 缺口 2 — HW4 重写限速 5b（isaOverride 完整链路）

新增 `isaOverride` 开关 + HW4Handler 923(`0x39B`) 写入分支（强写 fused/vision 限速为 31=NONE）。NVS/UI 链路参照 `isaChimeSuppress`/`fd`/`sound_warning_suppression`。

**Files:**
- Modify: `include/handlers.h`（CarManagerBase 声明 + HW4Handler 923 分支）
- Modify: `include/web/mcp2515_dashboard.h`（NVS `fj` + nvs 变量 + apply + handlerPool + /api/config JSON + defense JSON/POST）
- Modify: `include/web/mcp2515_dashboard_ui.src.h`（defense 页 isaOverride 开关）
- Test: `test/test_native_hw4/test_hw4_handler.cpp`

- [ ] **Step 0: 确认 NVS key `fj` 未被占用**

```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware
grep -rn '"fj"' include/ src/ || echo "fj 可用"
```
Expected: 输出 `fj 可用`（无占用）。若已被占用，改用 `fk` 并全文替换下方所有 `fj`。

- [ ] **Step 1: 声明 isaOverride（让测试可编译）**

修改 `include/handlers.h`，在 CarManagerBase 的 `isaChimeSuppress` 声明后（:69 行下方）加：

```cpp
    Shared<bool> isaChimeSuppress{false};
    Shared<bool> isaOverride{false};      // spec Task 3: HW4 重写限速 5b (0x39B raw=31 NONE)
```

- [ ] **Step 2: 写失败测试**

在 `test/test_native_hw4/test_hw4_handler.cpp` 末尾追加：

```cpp
// --- ISA override rewrite speed limit, HW4 (CAN ID 923 / 0x39B) — spec Task 3 ---

void test_hw4_isa_override_sets_fused_limit_to_none()
{
    handler.isaOverride = true;
    CanFrame f = {.id = 923};
    f.data[1] = 0x00;             // DAS_fusedSpeedLimit bits 8-12
    f.data[2] = 0x00;             // DAS_visionOnlySpeedLimit bits 16-20
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x1F, mock.sent[0].data[1] & 0x1F); // fused = 31 NONE
    TEST_ASSERT_EQUAL_HEX8(0x1F, mock.sent[0].data[2] & 0x1F); // vision = 31 NONE
}

void test_hw4_isa_override_preserves_high_bits()
{
    handler.isaOverride = true;
    CanFrame f = {.id = 923};
    f.data[1] = 0xE0;             // 高3位保留
    f.data[2] = 0xE0;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_HEX8(0xFF, mock.sent[0].data[1]); // 0xE0 | 0x1F
    TEST_ASSERT_EQUAL_HEX8(0xFF, mock.sent[0].data[2]);
}

void test_hw4_isa_override_checksum_correct()
{
    handler.isaOverride = true;
    CanFrame f = {.id = 923};
    f.data[0] = 0x10; f.data[1] = 0x00; f.data[2] = 0x00;
    f.data[3] = 0; f.data[4] = 0; f.data[5] = 0; f.data[6] = 0;
    handler.handleMessage(f, mock);
    // OR 后 data[1]=0x1F, data[2]=0x1F
    // checksum = id_lo(0x9B) + id_hi(0x03) + 0x10 + 0x1F + 0x1F + 0+0+0 = 0xEC
    TEST_ASSERT_EQUAL_HEX8(0xEC, mock.sent[0].data[7]);
}

void test_hw4_isa_override_runtime_off_skips_send()
{
    handler.isaOverride = false;
    CanFrame f = {.id = 923};
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}
```

并在 `main()` 追加 4 个 `RUN_TEST(...)`。

- [ ] **Step 3: 跑测试确认失败**

```bash
pio test -e native_hw4 -f test_native_hw4
```
Expected: 前 3 个 override 测试 **FAIL**（HW4Handler 当前无 923 写入分支）；第 4 个（off）PASS。

- [ ] **Step 4: 实现 HW4 923 分支**

修改 `include/handlers.h` 的 HW4Handler::handleMessage，在 921 分支之后、1016 分支之前（约 :866 处）插入 923 分支：

```cpp
        // ISA override — rewrite fused/vision speed limit to NONE (spec Task 3, 对齐 tesla handleDASStatusISAOverride)
        if (frame.id == 923 && frame.dlc >= 8 &&
            (bool)isaOverride && injectionAllowed())
        {
            frame.data[1] |= 0x1F;   // DAS_fusedSpeedLimit = 31 (NONE)
            frame.data[2] |= 0x1F;   // DAS_visionOnlySpeedLimit = 31 (NONE)
            frame.data[7] = computeVehicleChecksum(frame);
            framesSent++;
            driver.send(frame);
            if (onSend) onSend(0, true);
            return;
        }
```

> 位置：放在 `if (frame.id == 921) {...}` 闭合括号之后、`if (frame.id == 1016)` 之前。HW4Handler filter 已含 923（:836），无需改 filter。

- [ ] **Step 5: 跑测试确认通过**

```bash
pio test -e native_hw4 -f test_native_hw4
```
Expected: 4 个新测试 **PASS**，现有 HW4 测试全绿。

- [ ] **Step 6: NVS 持久化链路（参照 `isaChimeSuppress`/`fd`）**

在 `include/web/mcp2515_dashboard.h`，参照 `nvsIsaChimeSuppress` 的所有触点，新增 `isaOverride`/`fj`：

6a. 在 `nvsIsaChimeSuppress` 声明处（grep `nvsIsaChimeSuppress` 找到全局变量声明，约文件前部）加：
```cpp
extern bool nvsIsaChimeSuppress;
extern bool nvsIsaOverride;        // spec Task 3
```
（若 `nvsIsaChimeSuppress` 是 `static`，则在该 `static` 声明旁加 `static bool nvsIsaOverride = false;`）

6b. 在读取处（参照 `nvsIsaChimeSuppress = prefs.getBool("fd", false);` 约 :1614）加：
```cpp
    nvsIsaOverride = prefs.getBool("fj", false);
```

6c. 在写入处（参照 `prefs.putBool("fd", ...)` 约 :1397）加：
```cpp
    prefs.putBool("fj", (bool)dashHandler->isaOverride);
```

6d. 在应用到处（参照 `dashHandler->isaChimeSuppress = nvsIsaChimeSuppress;` 约 :1297）加：
```cpp
    dashHandler->isaOverride = nvsIsaOverride;
```

6e. 在 handlerPool 循环处（参照 `handlerPool[i]->isaChimeSuppress = nvsIsaChimeSuppress;` 约 :6582）加：
```cpp
    handlerPool[i]->isaOverride = nvsIsaOverride;
```

6f. 在 `/api/config` 写入端点（参照 `if (fsd["isaChimeSuppress"]...) p.putBool("fd", ...)` 约 :5895）加：
```cpp
    if (fsd["isaOverride"].is<bool>())
        p.putBool("fj", fsd["isaOverride"].as<bool>());
```

6g. 在 `/api/config` JSON 输出（参照 `,"isaChimeSuppress":...` 约 :2338 和 stored 版 :5600）加：
```cpp
    j += ",\"isaOverride\":";
    j += dashHandler ? ((bool)dashHandler->isaOverride ? "true" : "false") : "false";
```
和（stored 版，参照 :5605）：
```cpp
    j += ",\"isaOverride\":" + String(storedIsaOverride ? "true" : "false");
```
（需在 stored 读取处参照 `storedIsaChimeSuppress = p.getBool("fd", ...)` 约 :5466 加 `storedIsaOverride = p.getBool("fj", nvsIsaOverride);`，并声明 `storedIsaOverride`）

> 每个触点都用 grep 定位 `isaChimeSuppress`/`nvsIsaChimeSuppress`/`storedIsaChimeSuppress` 的精确行，在相邻位置镜像添加 isaOverride。

- [ ] **Step 7: defense UI 开关（参照 `sound_warning_suppression`/`def-sound-tgl`）**

7a. 在 `include/web/mcp2515_dashboard_ui.src.h` 的 defense 页，紧挨"声音警告抑制"开关（:1285-1291）之后加：
```html
  <div class="setting-row">
    <div>
      <div class="setting-name">限速覆盖 <span class="exp-badge">HW4</span></div>
      <div class="setting-desc">0x39B 强写融合/视觉限速为 NONE</div>
    </div>
    <label class="tgl"><input type="checkbox" id="def-isa-override-tgl" onchange="saveDefenseConfig()"><div class="tgl-track"></div></label>
  </div>
```

7b. 在 `dashDefenseConfigJson()`（dashboard.h:2991）的 `"sound_warning_suppression"` 后加：
```cpp
    j += ",\"isa_override\":";
    j += dashHandler ? ((bool)dashHandler->isaOverride ? "true" : "false") : (nvsIsaOverride ? "true" : "false");
```

7c. 在 `handleDefenseConfig()`（dashboard.h:3019）的 hasArg 判断（:3021-3024）加 `"isa_override"`，并在处理段（参照 `sound_warning_suppression` 处理 :3045-3051）加：
```cpp
        if (server.hasArg("isa_override"))
        {
            bool v = dashArgTruthy(server.arg("isa_override"));
            nvsIsaOverride = v;
            if (dashHandler)
                dashHandler->isaOverride = v;
        }
```

7d. 在前端 JS 的 `loadDefenseConfig`/`saveDefenseConfig`（grep `def-sound-tgl` 定位 JS 绑定），镜像添加 `def-isa-override-tgl` 的读取（`document.getElementById('def-isa-override-tgl').checked = cfg.isa_override`）与提交（表单加 `isa_override=`）。

- [ ] **Step 8: 跑全量 native + 构建确认**

```bash
pio test -e native_legacy -e native_hw3 -e native_hw4
```
Expected: 三个 env 全绿。

```bash
# 生成 dashboard_ui.h（ui.src.h 是源，需重新生成头文件）
grep -n "mcp2515_dashboard_ui.h\|generate\|xxd\|generate_ui" platformio.ini scripts/ 2>/dev/null
# 按项目现有方式重新生成 ui.h（通常构建时自动，或 scripts/ 下有生成脚本）
```
若构建自动从 ui.src.h 生成 ui.h，跳过；否则按 `scripts/` 现有生成脚本执行。

```bash
pio run -e waveshare_single_can_standalone
```
Expected: `SUCCESS`（RAM/Flash 在预算内）。

- [ ] **Step 9: Commit**

```bash
git add include/handlers.h include/web/mcp2515_dashboard.h include/web/mcp2515_dashboard_ui.src.h test/test_native_hw4/test_hw4_handler.cpp
git commit -m "feat(hw4): ISA override rewrite speed limit on CAN 923 (spec task 3, gap 2)

新增 isaOverride 开关 + HW4Handler 923 分支，强写 DAS_fusedSpeedLimit /
DAS_visionOnlySpeedLimit 为 31(NONE)，对齐 tesla handleDASStatusISAOverride。
完整 NVS(fj)/JSON/UI(def-isa-override-tgl) 链路，参照 isaChimeSuppress。
默认关。注意区别于 overrideSpeedLimit(5a, 0x438)。不碰 0x370。"
```

---

## Task 4: 全量回归 + 守卫测试 + 默认关审计

**Files:** 无代码改动（验证 + 审计）

- [ ] **Step 1: 全量 native 回归**

```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware
pio test -e native_legacy -e native_hw3 -e native_hw4 -e native_helpers -e native_nag
```
Expected: 全部 PASS。

- [ ] **Step 2: 守卫测试（0x370 禁令）必须保持绿**

```bash
pytest test/test_no_epas_nag_contract.py -v
```
Expected: `test_no_epas_nag_symbols_in_compiled_source` PASS、`test_no_native_epas_nag_env` PASS。

```bash
# 全量 pytest 契约/回归
pytest test/ -v
```
Expected: 全绿（含 test_dashboard_api_contract、test_rec_reliability_contract 等）。

- [ ] **Step 3: 默认关审计**

确认 5 功能默认全关（grep 默认值）：
```bash
grep -n "getBool(\"fd\", false\|getBool(\"fi\", false\|getBool(\"fj\", false" include/web/mcp2515_dashboard.h
grep -n "legacyOffset{0}\|isaChimeSuppress{false}\|isaOverride{false}\|overrideSpeedLimit{false}" include/handlers.h
```
Expected: 所有功能默认 `false`/`0`（关）。`visionLimitClearEnable` 默认 `false`（`dash_fsd_diag.h:240`）。

- [ ] **Step 4: Commit（若有审计修正）**

若审计发现某默认值非关，修正后 commit；否则跳过。

---

## Task 5: 先决 Legacy 模式 + 资产生成 + L0 实车验证（Jordan 操作）

> ⚠️ 此 Task 涉及实车，由 Jordan 在台架/实车执行。每步异常立即关开关 + 拔模块。

**Files:** `platformio.ini`（可选改默认）

- [ ] **Step 1: 决定 Legacy 模式配置方式**

两种方式选一：
- **方式 A（推荐，改默认）**：`platformio.ini` 的 `[env:waveshare_single_can_standalone]`（:195）把 `-DDASH_INITIAL_HW_MODE=3` 改为 `-DDASH_INITIAL_HW_MODE=0`（LEGACY）。
- **方式 B（运行时 UI 切换）**：不改默认，烧录后在 Dashboard → 硬件页切 `hwMode=0 (LEGACY)`，重启。

若选 A：
```bash
# platformio.ini:195
-    -DDASH_INITIAL_HW_MODE=3
+    -DDASH_INITIAL_HW_MODE=0
```

- [ ] **Step 2: 生成固件资产**

```bash
pio run -e waveshare_single_can_standalone
```
Expected: `SUCCESS`。记录 `firmware.bin` / `bootloader.bin` / `partitions.bin` 路径（`.pio/build/waveshare_single_can_standalone/`）。

按项目惯例生成 full-merged 资产 + SHA256（参照 `scripts/` 现有资产生成脚本，或 memory 记录的 esptool merge 流程）。

- [ ] **Step 3: 烧录到微雪板**

按项目现有烧录流程（memory：esptool `write_flash` @0x0，或逐分区）。烧前确认板子 MAC。

```bash
# 示例（按项目实际 esptool 命令调整）
esptool.py --chip esp32s3 --port /dev/cu.usbmodem* --baud 921600 \
  write_flash 0x0 <full_merged.bin>
```

- [ ] **Step 4: L0 实车验证清单（Legacy，逐项核对）**

启动后连 Dashboard，确认并逐项测：

- [ ] `effectiveHw=LEGACY`（硬件页 / `/api/status`）
- [ ] **功能 1 速度偏移**：设 `legacyOffset`（如 5 kph），看仪表/FSD 设定速度变化；`framesSent` 增长
- [ ] **功能 3 bit-19**：FSD 期间观察 nag 是否抑制；无故障灯
- [ ] **功能 5a 重写限速**：开 `overrideSpeedLimit`，看限速是否被覆盖
- [ ] **功能 2 视觉限速**：开 `visionLimitClearEnable`，看视觉限速牌识别是否关闭
- [ ] **功能 4 ❓ 定论**：抓总线（Dashboard CAN 监听或 `/api/status`）确认 Legacy 车有无 `0x399`(921) 报文。有 → 开 `sound_warning_suppression` 测 chime 抑制；无 → 记录"Legacy 车无 0x399，功能 4 不适用"
- [ ] **全程无故障灯/车辆异常**（异常立即关全部开关 + 拔模块）

- [ ] **Step 5: 记录 L0 结果**

把 L0 结果（每功能 ✅/❓/❌ + framesSent + 0x399 结论）记入 memory（`lilygo-waveshare-single-can-standalone` 相关记忆条目）或新建 handoff 文档。

- [ ] **Step 6: Commit（若改了 platformio.ini 默认模式）**

```bash
git add platformio.ini
git commit -m "chore(single-can): default DASH_INITIAL_HW_MODE=0 (LEGACY) for Jordan's car"
```

---

## Self-Review（计划自审）

**1. Spec 覆盖**：
- 功能 1 速度偏移：三模式已有（spec §5 矩阵 ✅），Task 4 审计默认关 ✅
- 功能 2 视觉限速：Task 1（门控独立）✅
- 功能 3 bit-19：三模式已有 baseline，不碰 0x370（spec §5 ✅），Task 4 审计 ✅
- 功能 4 chime：Task 2（HW3 缺口）✅；Legacy ❓ 在 Task 5 Step 4 定论 ✅
- 功能 5 重写限速：5a 已有（Task 4 审计），5b Task 3（HW4 缺口）✅
- 安全边界（不碰 0x370）：Task 4 Step 2 守卫测试 ✅
- 先决 Legacy 模式：Task 5 Step 1 ✅

**2. 占位符扫描**：无 TBD/TODO。Task 3 Step 6/7 的 NVS/UI 触点用 grep 精确定位 + 参照 `isaChimeSuppress` 镜像（有明确 follow 对象，非占位符）。Task 5 烧录命令标注"按项目实际调整"（实车步骤，合理）。

**3. 类型一致性**：`isaOverride`（Shared<bool>）在声明（handlers.h）、测试、NVS、JSON、UI 全程一致。`isa_override`（JSON 字段，snake_case）vs `isaOverride`（C++，camelCase）—— 与现有 `sound_warning_suppression`/`isaChimeSuppress` 的命名惯例一致。NVS key `fj` 全程一致。

**4. 范围**：5 个 Task 聚焦单一子系统（单 CAN 微雪版 FSD 对齐），每个 Task 自包含可测试可 commit。无需拆分。
