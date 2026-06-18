# EPAS-Faithful Nag 对齐上游 Mode-C（v2.16-beta.8）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `DashEpasNagEngine` 的扭矩决策从 B1/B2 的"固定值 + 固定 handsOnLevel"升级为上游 v2.16-beta.8 的 Mode-C demand-state 状态机（handsOnLevel 从扭矩推导 + 按 DAS 等级差异化扭矩 + 0x129 反向助力方向），对齐上游已验证逻辑。

**Architecture:** 路径 B（适配本地 `DashEpasNagEngine` 骨架）——保留 gate G1–G6 / diag / trace，把内核 `nextTorque()` 替换为 `nextDemandTorque(das, nowMs, outTorque, outLevel)` 状态机，`tryBuildEcho` 的 level 写入从 B2 `|=0x40` 改为 `handsLevelFromRaw()` 推导，方向由复用的 `steerAngleX10`（0x129，`mcp2515_dashboard.h:683` 已解析）驱动。纯函数 + 确定性 PRNG，TDD 全程 Red→Green。

**Tech Stack:** C++（ESP-IDF/Arduino 风格头文件）、PlatformIO `native_epas_nag` 单元测试（unity）、Python 契约测试（pytest）、`waveshare_single_can_standalone` env 构建。

**Spec:** `docs/superpowers/specs/2026-06-19-upstream-epas-nag-modec-align-design.md`

**参考上游:** `flipper-tesla-fsd` tag `v2.16-beta.8`，commit `2bc2f33`（`esp32/.firmware/fsd_handler.cpp:354-470` 的 `nag_faithful_modec()`）。

**测试命令速查:**
- native 单元：`pio test -e native_epas_nag -f test_native_epas_nag`
- Python 契约：`PYTHONPATH=test python3 -m pytest test/test_epas_nag_config_contract.py test/test_epas_nag_filters_contract.py test/test_epas_nag_wiring_contract.py test/test_epas_nag_trace_contract.py test/test_epas_nag_trace_persistence_contract.py -q`
- ESP 构建：`pio run -e waveshare_single_can_standalone`

**关键约束（硬性，违反会变砖/丢数据）:**
- 🚨 单 CAN 板烧录只用 `esptool.py` + full_merged.bin，**绝不** `pio run -t upload`（自定义分区 0x20000/16MB 会变砖）。
- 🚨 EPAS nag 默认 OFF（`dashEpasNagEnabled=false`）；bit19 默认开 baseline 不动；NagHandler 不删。
- 🚨 构建后若 ui.h 被 build-id stamp，`git restore include/web/mcp2515_dashboard_ui.h` 撤销；版本串来自 git describe，提交后必须 `rm -rf .pio/build/waveshare_single_can_standalone` 再重建。
- 🚨 忽略 clangd 虚假诊断（.h 当 C 处理）——只信 `pio`/测试结果。

**spec 实现细节修正（plan 内生效，spec §6.2 的 oversight）:**
- `prevDas` 初始 = `0`（**非** spec 写的 `0xFF`）。理由：`isStrong` 含 `0xFF`，若 `prevDas` 初始 `0xFF` 则 `prevStrong=true`，`!prevStrong && isStrong` 永不成立 → `strongEnterMs` 永远 0 → 0xFF 永不 ramp（违背 D5 兜底）。初始 `0` 让首次 `0xFF` 正常触发 strong 进入。

---

## File Structure

| 文件 | 责任 | 改动 |
|---|---|---|
| `include/dash_epas_nag.h` | EPAS nag 引擎（纯逻辑） | 删 `nextTorque`/`torqueWalk`/B2 `|=0x40`；加 `nextDemandTorque`/`handsLevelFromRaw`/`onSteeringAngle`/`clampInt16`/状态机字段/diag 4 字段；G5 加 `das==15` |
| `include/web/mcp2515_dashboard.h` | dashboard CAN 接收 + `/status` JSON | `:683` 0x129 块加 `onSteeringAngle` glue；`:2723` epasNagDiag JSON 加 4 字段 |
| `test/test_native_epas_nag/test_epas_nag.cpp` | unity 单元测试 | 加 demand-state 测试 + `makeDemandArmedEngine` helper；更新 B1/B2/echo 旧测试；`RUN_TEST` |
| `test/test_epas_nag_config_contract.py` | 源码符号契约 | 加 `nextDemandTorque`/`handsLevelFromRaw`/`onSteeringAngle`/`clampInt16`/`demand_idle` 断言 |
| 资产（`firmware-assets/`） | 烧录包 | full_merged.bin（授权后生成 + 烧录） |

---

## Task 1: 纯函数 `handsLevelFromRaw` + `clampInt16`

**Files:**
- Modify: `include/dash_epas_nag.h`（加在 `clampTorque` 之后，约 `:87`）
- Test: `test/test_native_epas_nag/test_epas_nag.cpp`

- [ ] **Step 1: 写失败测试**（加在 `test_nag_constants` 之后，约 `:31`）

```cpp
// ── Mode-C: handsOnLevel derived from torque magnitude (spec §7.1) ───
void test_hands_level_from_raw()
{
    DashEpasNagEngine e;
    TEST_ASSERT_EQUAL_UINT8(0, e.handsLevelFromRaw(2050)); // 0 Nm
    TEST_ASSERT_EQUAL_UINT8(0, e.handsLevelFromRaw(2149)); // <1.0 Nm
    TEST_ASSERT_EQUAL_UINT8(1, e.handsLevelFromRaw(2150)); // =1.0 Nm
    TEST_ASSERT_EQUAL_UINT8(1, e.handsLevelFromRaw(2249)); // <2.0 Nm
    TEST_ASSERT_EQUAL_UINT8(2, e.handsLevelFromRaw(2250)); // =2.0 Nm
    TEST_ASSERT_EQUAL_UINT8(2, e.handsLevelFromRaw(2260)); // strong peak
    TEST_ASSERT_EQUAL_UINT8(1, e.handsLevelFromRaw(1950)); // -1.0 Nm (dir=-1)
    TEST_ASSERT_EQUAL_UINT8(2, e.handsLevelFromRaw(1850)); // -2.0 Nm (dir=-1)
    TEST_ASSERT_EQUAL_UINT8(2, e.handsLevelFromRaw(1840)); // strong negative peak
}
```

并在 `main()` 的 `RUN_TEST(test_nag_constants);` 后加 `RUN_TEST(test_hands_level_from_raw);`。

- [ ] **Step 2: 验证失败**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: FAIL — `'class DashEpasNagEngine' has no member named 'handsLevelFromRaw'`（编译错误）。

- [ ] **Step 3: 实现**（`include/dash_epas_nag.h`，`clampTorque` 函数后）

```cpp
    static uint8_t handsLevelFromRaw(int32_t raw)
    {
        int32_t a = (raw >= 2050) ? (raw - 2050) : (2050 - raw);
        if (a >= 200)
            return 2;
        if (a >= 100)
            return 1;
        return 0;
    }

    // int16_t clamp for negative torque bands (clampTorque returns uint16_t).
    static int16_t clampInt16(int32_t v, int32_t lo, int32_t hi)
    {
        if (v < lo)
            return (int16_t)lo;
        if (v > hi)
            return (int16_t)hi;
        return (int16_t)v;
    }
```

- [ ] **Step 4: 验证通过**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: PASS（含新 `test_hands_level_from_raw`）。

- [ ] **Step 5: Commit**

```bash
git add include/dash_epas_nag.h test/test_native_epas_nag/test_epas_nag.cpp
git commit -m "feat(epas-nag): handsLevelFromRaw + clampInt16 pure helpers (Mode-C)"
```

---

## Task 2: `steerAngleX10` 字段 + `onSteeringAngle` + diag 4 字段（结构铺设）

**Files:**
- Modify: `include/dash_epas_nag.h`（`DashEpasNagDiag` `:25-39`、`DashEpasNagEngine` 字段区 `:56-66`）
- Test: `test/test_native_epas_nag/test_epas_nag.cpp`

- [ ] **Step 1: 写失败测试**

```cpp
// ── Mode-C: 0x129 steering direction + diag fields ──────────────────
void test_on_steering_angle_stores_and_defaults()
{
    DashEpasNagEngine e;
    // default 0 -> dir +1 path (verified in Task 4/5); setting positive flips dir
    e.onSteeringAngle(0);
    TEST_ASSERT_EQUAL_INT16(0, e.steerAngleX10);
    e.onSteeringAngle(500); // right turn -> X10 positive
    TEST_ASSERT_EQUAL_INT16(500, e.steerAngleX10);
    e.onSteeringAngle(-300);
    TEST_ASSERT_EQUAL_INT16(-300, e.steerAngleX10);
}
```

`main()` 加 `RUN_TEST(test_on_steering_angle_stores_and_defaults);`。

- [ ] **Step 2: 验证失败**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: FAIL — no member `steerAngleX10` / `onSteeringAngle`。

- [ ] **Step 3: 实现结构**

`DashEpasNagDiag`（`:25`）末尾（`lastEchoTorque` 后）加：

```cpp
    // Mode-C observability (spec §5 D8).
    uint8_t  lastDemandState = 0xFF;   // das snapshot at last inject
    uint16_t lastDemandTorque = 0;     // torque the demand machine returned
    uint8_t  lastHandsLevel = 0;       // level written to byte4
    const char *lastDemandGate = "idle"; // s1_hold/s2_mild/strong_ramp/...
```

`DashEpasNagEngine` 字段区（`apDisengageMs` 后，约 `:63`）加：

```cpp
    // ── Mode-C demand-state machine (spec §6.2) ───────────────
    bool     apActive{false};      // current AP state (noteApState refreshes)
    int16_t  steerAngleX10{0};     // 0x129 angle x10 (onSteeringAngle); dir = >0?-1:+1
    uint32_t s1EnterMs{0};
    uint32_t s2EnterMs{0};
    uint32_t strongEnterMs{0};
    uint32_t s2HoldUntilMs{0};
    int16_t  mildWalk{2050};
    int16_t  s2HoldRaw{2050};
    uint8_t  s2HoldLevel{0};
    bool     s2Level2Active{false};
    uint8_t  prevDas{0};           // ★ 0 (not 0xFF) so first 0xFF triggers strongEnterMs (D5)
    int16_t  lastRaw{2050};
    uint8_t  lastLevel{0};
```

纯函数区加 setter（`noteApState` 附近，约 `:126`）：

```cpp
    // 0x129 steering angle (spec §6.3 dir input). x10 deg, offset-binary already
    // decoded by mcp2515_dashboard.h:689; 0x3FFF-invalid arrives as 0 here.
    void onSteeringAngle(int16_t x10)
    {
        steerAngleX10 = x10;
    }
```

- [ ] **Step 4: 验证通过**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add include/dash_epas_nag.h test/test_native_epas_nag/test_epas_nag.cpp
git commit -m "feat(epas-nag): steerAngleX10 field + onSteeringAngle + diag fields (Mode-C)"
```

---

## Task 3: `nextDemandTorque` 骨架 + 排除值 + AP 门 + `das==1` hold

> 本 task 只建函数骨架与最简单分支（排除值 / AP 门 / das==1 hold）。mild/strong 在 Task 4/5 加。期间 tryBuildEcho 仍用旧 nextTorque（暂不删，避免编译断裂）——nextDemandTorque 暂未被 tryBuildEcho 调用，纯单测驱动。

**Files:**
- Modify: `include/dash_epas_nag.h`（`nextTorque` `:105` 之后加新函数；`noteApState` `:126` 加 `apActive` 刷新）
- Test: `test/test_native_epas_nag/test_epas_nag.cpp`

- [ ] **Step 1: 写失败测试**

```cpp
// ── Mode-C: nextDemandTorque demand-state machine (spec §6.3) ───────
// Helper: arm engine with given das state, AP on, no steering (dir +1).
static DashEpasNagEngine makeDemandEngine(uint8_t dasState)
{
    DashEpasNagEngine e;
    e.noteRuntime(true, true, true);
    e.noteApState(true, 1000); // apActive=true
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>((dasState & 0x0F) << 2);
    e.onDasGate(das);
    return e;
}

void test_demand_rejects_excluded_das_states()
{
    for (uint8_t d : {0u, 8u, 15u})
    {
        DashEpasNagEngine e = makeDemandEngine(d);
        uint16_t t = 9999; uint8_t lvl = 9;
        TEST_ASSERT_FALSE(e.nextDemandTorque(d, 5000, t, lvl));
    }
}
void test_demand_ap_inactive_blocks()
{
    DashEpasNagEngine e = makeDemandEngine(3);
    e.noteApState(false, 1000);
    uint16_t t; uint8_t lvl;
    TEST_ASSERT_FALSE(e.nextDemandTorque(3, 5000, t, lvl));
}
void test_demand_state1_holds_then_stops()
{
    DashEpasNagEngine e = makeDemandEngine(1);
    uint16_t t; uint8_t lvl;
    // first call at 1100: arms s1EnterMs, within 500ms -> holds lastRaw (2050 init)
    TEST_ASSERT_TRUE(e.nextDemandTorque(1, 1100, t, lvl));
    TEST_ASSERT_EQUAL_UINT16(2050, t);
    // after 500ms window -> stops
    TEST_ASSERT_FALSE(e.nextDemandTorque(1, 1700, t, lvl));
}
```

`main()` 加三条 `RUN_TEST`。

- [ ] **Step 2: 验证失败**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: FAIL — no member `nextDemandTorque`。

- [ ] **Step 3: 实现**（`nextTorque()` 函数后，约 `:108`）

```cpp
    // ── Mode-C demand-state torque machine (spec §6.3) ───────────────
    // Returns true + fills outTorque/outLevel when this 0x370 should be echoed;
    // false (demand_idle) when the demand machine says "don't inject this frame".
    bool nextDemandTorque(uint8_t das, uint32_t nowMs, uint16_t &outTorque, uint8_t &outLevel)
    {
        // edge detection — record entry timestamps per state
        bool isStrong = (das == 3 || das == 4 || das == 5 || das == 0xFF);
        bool prevStrong = (prevDas == 3 || prevDas == 4 || prevDas == 5 || prevDas == 0xFF);
        if (prevDas != 1 && das == 1) s1EnterMs = nowMs;
        if (das != 1) s1EnterMs = 0;
        if (prevDas != 2 && das == 2) s2EnterMs = nowMs;
        if (das != 2) { s2EnterMs = 0; s2HoldUntilMs = 0; s2Level2Active = false; }
        if (!prevStrong && isStrong) strongEnterMs = nowMs;
        if (!isStrong) strongEnterMs = 0;
        prevDas = das;

        diag.lastDemandState = das;

        // AP gate + excluded das states (spec §6.3, upstream das_ap_state<2 analog)
        if (!apActive || das == 0 || das == 8 || das == 15)
        {
            lastRaw = 2050; lastLevel = 0;
            diag.lastDemandGate = "demand_idle";
            return false;
        }

        // direction: reverse-assist from 0x129 (spec §6.3 / upstream fsd_handler.c:973)
        int8_t dir = (steerAngleX10 > 0) ? -1 : 1;

        if (das == 1)
        {
            if (s1EnterMs != 0 && (nowMs - s1EnterMs) < 500u)
            {
                outTorque = (uint16_t)lastRaw; outLevel = lastLevel;
                diag.lastDemandGate = "s1_hold";
                return true;
            }
            lastRaw = 2050; lastLevel = 0;
            diag.lastDemandGate = "s1_done";
            return false;
        }

        // mild (das==2) and strong (else) branches added in Task 4 / Task 5.
        (void)dir;
        diag.lastDemandGate = "demand_idle";
        return false;
    }
```

`noteApState`（`:126`）改为同时刷新 `apActive`：

```cpp
    void noteApState(bool active, uint32_t nowMs)
    {
        apActive = active;                       // ★ Mode-C AP gate input
        if (prevApActive && !active)
        {
            apDisengageMs = nowMs;
            apBlackoutActive = true;
        }
        prevApActive = active;
    }
```

- [ ] **Step 4: 验证通过**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: PASS（三条新测试 + 全部既有测试，因 nextTorque 未删仍编译通过）。

- [ ] **Step 5: Commit**

```bash
git add include/dash_epas_nag.h test/test_native_epas_nag/test_epas_nag.cpp
git commit -m "feat(epas-nag): nextDemandTorque skeleton + das==1 hold + AP gate (Mode-C)"
```

---

## Task 4: `nextDemandTorque` das==2 mild 分支（含方向）

**Files:**
- Modify: `include/dash_epas_nag.h`（`nextDemandTorque` 内 `das==1` 块后）
- Test: `test/test_native_epas_nag/test_epas_nag.cpp`

- [ ] **Step 1: 写失败测试**

```cpp
void test_demand_state2_delays_then_mild_walks()
{
    DashEpasNagEngine e = makeDemandEngine(2);
    uint16_t t; uint8_t lvl;
    // within 2000ms delay -> no inject
    TEST_ASSERT_FALSE(e.nextDemandTorque(2, 1100, t, lvl));
    // still in delay at 2000
    TEST_ASSERT_FALSE(e.nextDemandTorque(2, 3000, t, lvl)); // 3000-1000=2000, <2000 false? boundary
    // past delay -> mild walk in positive band [2100,2250] (dir +1, no steer)
    TEST_ASSERT_TRUE(e.nextDemandTorque(2, 3100, t, lvl));
    TEST_ASSERT_TRUE(t >= 2100 && t <= 2250);
    TEST_ASSERT_TRUE(lvl == 1 || lvl == 2); // 0.5..2.0 Nm
}
void test_demand_state2_negative_band_when_steer_right()
{
    DashEpasNagEngine e = makeDemandEngine(2);
    e.onSteeringAngle(500); // right turn -> dir -1
    uint16_t t; uint8_t lvl;
    TEST_ASSERT_FALSE(e.nextDemandTorque(2, 1100, t, lvl)); // delay
    TEST_ASSERT_TRUE(e.nextDemandTorque(2, 3100, t, lvl));
    TEST_ASSERT_TRUE(t >= 1850 && t <= 2000); // negative band
}
```

> 注：`makeDemandEngine` 内 `noteApState(true,1000)` 让首次 `nextDemandTorque(2,1100)` 把 `s2EnterMs` 设为 1100；`3100-1100=2000`，因判定 `< 2000u` 为假已过 delay。`3000-1100=1900` 仍在 delay。

`main()` 加两条 `RUN_TEST`。

- [ ] **Step 2: 验证失败**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: FAIL — das==2 落到占位 `return false`，`test_demand_state2_delays_then_mild_walks` 的 inject 断言失败。

- [ ] **Step 3: 实现**（替换 Task 3 末尾的占位块）

把 `nextDemandTorque` 末尾的占位：
```cpp
        // mild (das==2) and strong (else) branches added in Task 4 / Task 5.
        (void)dir;
        diag.lastDemandGate = "demand_idle";
        return false;
```
替换为：
```cpp
        if (das == 2)
        {
            if (s2EnterMs != 0 && (nowMs - s2EnterMs) < 2000u)
            {
                diag.lastDemandGate = "s2_delay";
                return false;
            }
            if (nowMs < s2HoldUntilMs)
            {
                outTorque = (uint16_t)s2HoldRaw; outLevel = s2HoldLevel;
                diag.lastDemandGate = "s2_hold";
                return true;
            }
            int16_t lo = (dir < 0) ? 1850 : 2100;
            int16_t hi = (dir < 0) ? 2000 : 2250;
            if (mildWalk < lo || mildWalk > hi)
                mildWalk = (int16_t)((lo + hi) / 2);
            mildWalk += (int16_t)((int)(xorshift32() % 25u) - 12);
            mildWalk = clampInt16(mildWalk, lo, hi);
            outTorque = (uint16_t)mildWalk;
            outLevel = handsLevelFromRaw(mildWalk);
            if (handsLevelFromRaw(mildWalk) == 2 && !s2Level2Active)
            {
                s2HoldUntilMs = nowMs + 1000u;
                s2HoldRaw = mildWalk; s2HoldLevel = 2;
            }
            s2Level2Active = (handsLevelFromRaw(mildWalk) == 2);
            lastRaw = mildWalk; lastLevel = outLevel;
            diag.lastDemandGate = "s2_mild";
            return true;
        }

        // strong (das==3/4/5/0xFF) added in Task 5.
        (void)dir;
        diag.lastDemandGate = "demand_idle";
        return false;
```

- [ ] **Step 4: 验证通过**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: PASS（含两条 state2 测试）。

- [ ] **Step 5: Commit**

```bash
git add include/dash_epas_nag.h test/test_native_epas_nag/test_epas_nag.cpp
git commit -m "feat(epas-nag): nextDemandTorque das==2 mild walk + dir band (Mode-C)"
```

---

## Task 5: `nextDemandTorque` strong 分支（das==3/4/5/0xFF，含方向 + ramp + 0xFF 兜底）

**Files:**
- Modify: `include/dash_epas_nag.h`（`nextDemandTorque` 末尾）
- Test: `test/test_native_epas_nag/test_epas_nag.cpp`

- [ ] **Step 1: 写失败测试**

```cpp
void test_demand_state3_delays_then_ramps_positive()
{
    DashEpasNagEngine e = makeDemandEngine(3);
    uint16_t t; uint8_t lvl;
    TEST_ASSERT_FALSE(e.nextDemandTorque(3, 1100, t, lvl));   // within 1000ms delay
    TEST_ASSERT_FALSE(e.nextDemandTorque(3, 2000, t, lvl));   // 2000-1000=1000 still <1000? boundary: ==1000 not <1000 -> inject
    // just past delay: mag should be small (ramp start)
    bool ok = e.nextDemandTorque(3, 2100, t, lvl);             // active=100, phase=100 -> mag=42
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(t >= 2050 && t <= 2092);                  // 2050+42=2092
}
void test_demand_state3_ramp_peaks_at_2260()
{
    DashEpasNagEngine e = makeDemandEngine(3);
    uint16_t t; uint8_t lvl;
    e.nextDemandTorque(3, 1100, t, lvl);                       // arm strongEnterMs=1100 (delay)
    // active = now-1100-1000; phase in [500,1500) -> mag=210 hold
    TEST_ASSERT_TRUE(e.nextDemandTorque(3, 2700, t, lvl));     // active=600, phase=600 -> mag=210
    TEST_ASSERT_EQUAL_UINT16(2260, t);                         // 2050+210
    TEST_ASSERT_EQUAL_UINT8(2, lvl);
}
void test_demand_state3_negative_when_steer_right()
{
    DashEpasNagEngine e = makeDemandEngine(3);
    e.onSteeringAngle(500);
    uint16_t t; uint8_t lvl;
    e.nextDemandTorque(3, 1100, t, lvl);
    TEST_ASSERT_TRUE(e.nextDemandTorque(3, 2700, t, lvl));
    TEST_ASSERT_EQUAL_UINT16(1840, t);                         // 2050-210
    TEST_ASSERT_EQUAL_UINT8(2, lvl);
}
void test_demand_unseen_0xff_falls_into_strong()
{
    // makeArmedEngine leaves dasHandsOnState=0xFF (unseen). D5: treat as strong.
    DashEpasNagEngine e = makeArmedEngine();
    e.noteApState(true, 1000);
    uint16_t t; uint8_t lvl;
    e.nextDemandTorque(0xFF, 1100, t, lvl);                    // arm strongEnterMs (prevDas 0 -> 0xFF)
    TEST_ASSERT_TRUE(e.nextDemandTorque(0xFF, 2700, t, lvl));  // peak
    TEST_ASSERT_EQUAL_UINT16(2260, t);
}
```

`main()` 加四条 `RUN_TEST`。

- [ ] **Step 2: 验证失败**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: FAIL — strong 仍落占位 `return false`。

- [ ] **Step 3: 实现**（替换 Task 4 末尾占位）

把：
```cpp
        // strong (das==3/4/5/0xFF) added in Task 5.
        (void)dir;
        diag.lastDemandGate = "demand_idle";
        return false;
```
替换为：
```cpp
        // strong: das==3/4/5 (and 0xFF D5 fallback). 1s pause, then 0->2.1Nm ramp, 1.5s cycle.
        if (strongEnterMs != 0 && (nowMs - strongEnterMs) < 1000u)
        {
            diag.lastDemandGate = "strong_delay";
            return false;
        }
        uint32_t active = strongEnterMs ? (nowMs - strongEnterMs - 1000u) : 0u;
        uint16_t phase = (uint16_t)(active % 1500u);
        int16_t mag = (phase < 500u) ? (int16_t)(((int32_t)phase * 210) / 500) : 210;
        int16_t torq = (int16_t)(2050 + dir * mag);
        outTorque = (uint16_t)clampTorque((int32_t)torq);
        outLevel = handsLevelFromRaw(torq);
        lastRaw = torq; lastLevel = outLevel;
        diag.lastDemandGate = (phase < 500u) ? "strong_ramp" : "strong_hold";
        return true;
```

- [ ] **Step 4: 验证通过**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: PASS（四条 strong 测试 + 全部既有）。

- [ ] **Step 5: Commit**

```bash
git add include/dash_epas_nag.h test/test_native_epas_nag/test_epas_nag.cpp
git commit -m "feat(epas-nag): nextDemandTorque strong ramp + dir + 0xFF fallback (Mode-C)"
```

---

## Task 6: `tryBuildEcho` 改用 demand 机 + level 推导 + 适配既有 echo 测试 + 删 `nextTorque`/`torqueWalk`

> 核心。Mode-C 的 delay 会破坏"单次 tryBuildEcho 立即注入"的既有假设；引入 `makeDemandArmedEngine` helper（das==2，预烧 delay），把帧构造类测试迁过去。同时删 B1 `nextTorque`/`torqueWalk`、B2 `|=0x40`。

**Files:**
- Modify: `include/dash_epas_nag.h`（`tryBuildEcho` `:153-192`、删 `nextTorque` `:105-108` 与 `torqueWalk` `:64`）
- Test: `test/test_native_epas_nag/test_epas_nag.cpp`

- [ ] **Step 1: 改写测试**（替换既有 helper + 受影响测试）

在 `makeArmedEngine`（`:14`）后加新 helper：

```cpp
// Mode-C: engine armed + das==2 (mild) + delay window already burned, so the
// NEXT tryBuildEcho at nowMs>=3001 injects. Used by frame-construction tests
// (checksum/counter/count/diag) that need a guaranteed inject on one call.
static DashEpasNagEngine makeDemandArmedEngine()
{
    DashEpasNagEngine e;
    e.noteRuntime(true, true, true);
    e.noteApState(true, 1000);
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(2 << 2);
    e.onDasGate(das);
    uint8_t in[8]; buildEpasFrame(in, 0, 1); uint8_t out[8];
    e.tryBuildEcho(in, 8, 1100, out); // arm s2EnterMs=1100, returns false (in delay)
    return e;
}
```

替换以下既有测试（用 `makeDemandArmedEngine()` + `nowMs=3100`）：

`test_echo_counter_increments_and_wraps`（`:194`）：
```cpp
void test_echo_counter_increments_and_wraps()
{
    for (uint8_t c = 0; c < 16; c++)
    {
        DashEpasNagEngine e = makeDemandArmedEngine();
        uint8_t in[8];
        buildEpasFrame(in, 0, c);
        in[6] = static_cast<uint8_t>(0xA0 | c);
        uint8_t out[8];
        TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 3100, out));
        uint8_t expected = static_cast<uint8_t>(0xA0 | ((c + 1) & 0x0F));
        TEST_ASSERT_EQUAL_HEX8(expected, out[6]);
    }
}
```

`test_echo_checksum_correct`（`:208`）：`DashEpasNagEngine e = makeArmedEngine();` → `makeDemandArmedEngine();`，`tryBuildEcho(in, 8, 1000, out)` → `tryBuildEcho(in, 8, 3100, out)`。

`test_echo_torque_in_valid_range_data23`（`:220`）：同上替换 helper 与 nowMs。

`test_echo_count_increments`（`:231`）：改为连续三帧（每帧时间推进，且每帧重新 arm——或用 strong 兜底一次 arm 后连续）。最简：
```cpp
void test_echo_count_increments()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    e.tryBuildEcho(in, 8, 3100, out);
    e.tryBuildEcho(in, 8, 3110, out);
    e.tryBuildEcho(in, 8, 3120, out);
    TEST_ASSERT_EQUAL_UINT32(3, e.diag.echoCount);
}
```

`test_diag_records_real_torque_and_counter`（`:256`）与 `test_diag_records_echo_torque`（`:269`）：替换 helper 为 `makeDemandArmedEngine()`、nowMs `1000`→`3100`。

`test_reset_clears_state`（`:242`）：
```cpp
void test_reset_clears_state()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    e.tryBuildEcho(in, 8, 3100, out);
    e.reset();
    TEST_ASSERT_EQUAL_UINT8(DashEpasNagEngine::kDasUnseen, e.dasHandsOnState);
    TEST_ASSERT_EQUAL_UINT32(0, e.apDisengageMs);
    TEST_ASSERT_EQUAL_UINT16(2050, (uint16_t)e.mildWalk);   // mildWalk reset to center
    TEST_ASSERT_EQUAL_UINT8(0, e.prevDas);                  // ★ prevDas reset to 0 (not 0xFF)
    TEST_ASSERT_EQUAL_INT16(0, e.steerAngleX10);
}
```

`test_echo_forces_handson_active`（B2，`:185`）→ 改为 level 推导测试：
```cpp
// Mode-C (replaces B2): handsOnLevel is DERIVED from torque magnitude, not
// forced to 1. Strong peak (2.1Nm) -> level 2; mild midband (>=1Nm) -> level 1.
void test_echo_level_derived_from_torque()
{
    // strong peak path: das==0xFF, AP on, advance past delay -> 2260 -> level 2
    DashEpasNagEngine e;
    e.noteRuntime(true, true, true);
    e.noteApState(true, 1000);
    uint8_t in[8]; buildEpasFrame(in, 0, 5);
    uint8_t out[8];
    e.tryBuildEcho(in, 8, 1100, out);            // arm strongEnterMs (das 0xFF unseen)
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 8, 2700, out));   // peak hold
    TEST_ASSERT_EQUAL_UINT8(2, (out[4] >> 6) & 0x03);     // level 2 written to bits[7:6]
    TEST_ASSERT_EQUAL_UINT8(2, e.diag.lastHandsLevel);
}
```

删除 `test_torque_stays_in_bounds`、`test_torque_returns_fixed_positive`、`test_torque_sequence_is_deterministic` 三个函数及其 `RUN_TEST` 行（`:341-343`）。它们的等价覆盖由 Task 3–5 的 demand 测试承担。

`main()` 里移除三条旧 `RUN_TEST(test_torque_*)`，保留 `RUN_TEST(test_echo_level_derived_from_torque);`（替换原 `test_echo_forces_handson_active` 那行）。

- [ ] **Step 2: 验证失败**（先只改测试，不改实现 → 编译错或断言失败）

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: FAIL — `tryBuildEcho` 仍走 B2 `|=0x40` 且调旧 `nextTorque`，level/echoCount 断言不匹配；或 `nextTorque` 仍存在使旧引用编译过但行为错。

- [ ] **Step 3: 改实现**（`include/dash_epas_nag.h`）

删 `nextTorque()`（`:105-108` 整个函数）与字段 `uint16_t torqueWalk{kTorqueCenter};`（`:64`）。

`tryBuildEcho`（`:153`）替换为：

```cpp
    bool tryBuildEcho(const uint8_t inData[8], uint8_t dlc, uint32_t nowMs, uint8_t outData[8])
    {
        if (dlc != 8)
        {
            diag.suppressCount++;
            diag.lastGate = "invalid_dlc";
            return false;
        }

        diag.lastRealTorque = static_cast<uint16_t>(((inData[2] & 0x0Fu) << 8) | inData[3]);
        diag.lastRealCnt = static_cast<uint8_t>(inData[6] & 0x0Fu);

        const uint8_t handsOn = static_cast<uint8_t>((inData[4] >> 6) & 0x03u);
        diag.lastHandsOn = handsOn;

        if (!shouldInject(handsOn, nowMs))
        {
            diag.suppressCount++;
            diag.lastGate = "gated";
            return false;
        }

        uint16_t torq = 0;
        uint8_t level = 0;
        if (!nextDemandTorque(dasHandsOnState, nowMs, torq, level))
        {
            diag.suppressCount++;
            diag.lastGate = diag.lastDemandGate; // demand_idle / s1_done / *_delay
            return false;
        }

        diag.lastDemandTorque = torq;
        diag.lastHandsLevel = level;
        diag.lastEchoTorque = torq;
        diag.lastGate = diag.lastDemandGate;

        outData[0] = inData[0];
        outData[1] = inData[1];
        outData[2] = static_cast<uint8_t>((inData[2] & 0xF0u) | ((torq >> 8) & 0x0Fu));
        outData[3] = static_cast<uint8_t>(torq & 0xFFu);
        outData[4] = static_cast<uint8_t>((inData[4] & 0x3Fu) | (uint8_t)(level << 6)); // ★ level derived
        outData[5] = inData[5];
        uint8_t cnt = static_cast<uint8_t>(inData[6] & 0x0Fu);
        cnt = static_cast<uint8_t>((cnt + 1) & 0x0Fu);
        outData[6] = static_cast<uint8_t>((inData[6] & 0xF0u) | cnt);
        outData[7] = teslaChecksum0x370(outData);

        diag.echoCount++;
        return true;
    }
```

- [ ] **Step 4: 验证通过**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: PASS（全部测试，含改造后的 echo/diag/reset + level 推导 + demand 机）。

- [ ] **Step 5: Commit**

```bash
git add include/dash_epas_nag.h test/test_native_epas_nag/test_epas_nag.cpp
git commit -m "feat(epas-nag): tryBuildEcho uses demand machine + derived level; drop B1/B2 (Mode-C)"
```

---

## Task 7: `shouldInject` G5 加 `das==15` + 契约

**Files:**
- Modify: `include/dash_epas_nag.h`（`shouldInject` `:144`）
- Test: `test/test_native_epas_nag/test_epas_nag.cpp`

- [ ] **Step 1: 写失败测试**

```cpp
void test_gate_das_15_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(15 << 2);
    e.onDasGate(das);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
```

`main()` 加 `RUN_TEST(test_gate_das_15_blocks);`。

- [ ] **Step 2: 验证失败**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: FAIL — `shouldInject` 返回 true（das==15 未被排除）。

- [ ] **Step 3: 实现**（`:144`）

```cpp
        if (das == 0 || das == 8 || das == 15)                   // G5: NOT_REQD / SUSPENDED / (15)
            return false;
```

- [ ] **Step 4: 验证通过**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add include/dash_epas_nag.h test/test_native_epas_nag/test_epas_nag.cpp
git commit -m "fix(epas-nag): G5 also excludes das==15 (Mode-C upstream alignment)"
```

---

## Task 8: `reset()` 重置 Mode-C 状态机字段

**Files:**
- Modify: `include/dash_epas_nag.h`（`reset` `:194-203`）
- Test: `test/test_native_epas_nag/test_epas_nag.cpp`

- [ ] **Step 1: 确认测试**（Task 6 的 `test_reset_clears_state` 已断言 `mildWalk==2050`/`prevDas==0`/`steerAngleX10==0`，作为本 task 的 RED→GREEN 驱动；若已绿，补一条针对性测试）

```cpp
void test_reset_clears_demand_machine()
{
    DashEpasNagEngine e = makeDemandArmedEngine();
    uint8_t in[8]; buildEpasFrame(in, 0, 1); uint8_t out[8];
    e.tryBuildEcho(in, 8, 3100, out);    // populate s2EnterMs/mildWalk/prevDas
    e.reset();
    TEST_ASSERT_EQUAL_UINT32(0, e.s1EnterMs);
    TEST_ASSERT_EQUAL_UINT32(0, e.s2EnterMs);
    TEST_ASSERT_EQUAL_UINT32(0, e.strongEnterMs);
    TEST_ASSERT_EQUAL_UINT8(0, e.prevDas);
    TEST_ASSERT_FALSE(e.apActive);
}
```

`main()` 加 `RUN_TEST(test_reset_clears_demand_machine);`。

- [ ] **Step 2: 验证失败**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: FAIL — `reset()` 未清新字段（`prevDas`/`s2EnterMs` 等残留）。

- [ ] **Step 3: 实现**（替换 `reset()`）

```cpp
    void reset()
    {
        prng = 0xDEADBEEFu;
        dasHandsOnState = kDasUnseen;
        diag.dasHandsOnState = kDasUnseen;
        prevApActive = false;
        apBlackoutActive = false;
        apDisengageMs = 0;
        // Mode-C machine
        apActive = false;
        steerAngleX10 = 0;
        s1EnterMs = s2EnterMs = strongEnterMs = s2HoldUntilMs = 0;
        mildWalk = 2050; s2HoldRaw = 2050; s2HoldLevel = 0;
        s2Level2Active = false;
        prevDas = 0; lastRaw = 2050; lastLevel = 0;
    }
```

（删旧 `torqueWalk = kTorqueCenter;` 行，字段已在 Task 6 删除。）

- [ ] **Step 4: 验证通过**

Run: `pio test -e native_epas_nag -f test_native_epas_nag`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add include/dash_epas_nag.h test/test_native_epas_nag/test_epas_nag.cpp
git commit -m "fix(epas-nag): reset() clears Mode-C demand-state fields"
```

---

## Task 9: dashboard 0x129 glue（`onSteeringAngle`）

**Files:**
- Modify: `include/web/mcp2515_dashboard.h:683-692`（`dashRecordApRestoreFrame` 的 `0x129` 块）

- [ ] **Step 1: 写失败测试**（Python 契约）

`test/test_epas_nag_config_contract.py` 末尾加：

```python
def test_0x129_feeds_onSteeringAngle():
    import pathlib
    p = pathlib.Path("include/web/mcp2515_dashboard.h")
    src = p.read_text()
    # the 0x129 block must call onSteeringAngle with the parsed steerAngleX10
    block = src[src.index("frame.id == 0x129 && frame.dlc >= 6"):]
    block = block[:block.index("return;")]
    assert "dashEpasNagCtrl.onSteeringAngle" in block, \
        "0x129 block must feed steerAngleX10 to the nag engine"
    assert "steerAngleX10" in block
```

- [ ] **Step 2: 验证失败**

Run: `PYTHONPATH=test python3 -m pytest test/test_epas_nag_config_contract.py::test_0x129_feeds_onSteeringAngle -q`
Expected: FAIL — `onSteeringAngle` not in block。

- [ ] **Step 3: 实现**（`mcp2515_dashboard.h:683` 块，`apRestoreState.steerMs = now;` 后、`return;` 前加一行）

把：
```cpp
        apRestoreState.steerAngleX10 = angleRaw == 0x3FFF ? 0 : static_cast<int16_t>(angleRaw) - 8192;
        apRestoreState.steerSpeedX10 = static_cast<int16_t>(static_cast<int32_t>(speedRaw) * 5 - 40960);
        apRestoreState.steerMs = now;
        return;
```
改为：
```cpp
        apRestoreState.steerAngleX10 = angleRaw == 0x3FFF ? 0 : static_cast<int16_t>(angleRaw) - 8192;
        apRestoreState.steerSpeedX10 = static_cast<int16_t>(static_cast<int32_t>(speedRaw) * 5 - 40960);
        apRestoreState.steerMs = now;
        dashEpasNagCtrl.onSteeringAngle(apRestoreState.steerAngleX10); // Mode-C dir input (spec D4)
        return;
```

- [ ] **Step 4: 验证通过**

Run: `PYTHONPATH=test python3 -m pytest test/test_epas_nag_config_contract.py::test_0x129_feeds_onSteeringAngle -q`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add include/web/mcp2515_dashboard.h test/test_epas_nag_config_contract.py
git commit -m "feat(epas-nag): feed 0x129 steerAngleX10 to nag engine (Mode-C dir)"
```

---

## Task 10: `/status` epasNagDiag JSON 加 4 字段

**Files:**
- Modify: `include/web/mcp2515_dashboard.h:2764`（`echo370` 对象后、闭合 `}}` 前）
- Test: `test/test_epas_nag_config_contract.py`

- [ ] **Step 1: 写失败测试**

```python
def test_epasNagDiag_has_modec_fields():
    import pathlib
    src = pathlib.Path("include/web/mcp2515_dashboard.h").read_text()
    block = src[src.index('"epasNagDiag":'):]
    block = block[:block.index("appendCapabilitiesJson")]
    for field in ('"demandState"', '"demandTorque"', '"handsLevel"', '"demandGate"'):
        assert field in block, f"epasNagDiag JSON missing {field}"
```

- [ ] **Step 2: 验证失败**

Run: `PYTHONPATH=test python3 -m pytest test/test_epas_nag_config_contract.py::test_epasNagDiag_has_modec_fields -q`
Expected: FAIL — 字段缺失。

- [ ] **Step 3: 实现**（`:2759-2765` 的 `echo370` 闭合后插入 demand 对象）

把：
```cpp
        j += (unsigned)ng.lastEchoTorque;
        j += "}}";
    }
```
改为：
```cpp
        j += (unsigned)ng.lastEchoTorque;
        j += "},\"demand\":{\"state\":";
        j += (unsigned)ng.lastDemandState;
        j += ",\"torque\":";
        j += (unsigned)ng.lastDemandTorque;
        j += ",\"handsLevel\":";
        j += (unsigned)ng.lastHandsLevel;
        j += ",\"gate\":\"";
        j += ng.lastDemandGate ? ng.lastDemandGate : "idle";
        j += "\"}}";
    }
```

- [ ] **Step 4: 验证通过**

Run: `PYTHONPATH=test python3 -m pytest test/test_epas_nag_config_contract.py::test_epasNagDiag_has_modec_fields -q`
Expected: PASS。

- [ ] **Step 5: Commit**

```bash
git add include/web/mcp2515_dashboard.h test/test_epas_nag_config_contract.py
git commit -m "feat(epas-nag): /status epasNagDiag demand object (Mode-C observability)"
```

---

## Task 11: 头部注释 + 全量契约回归

**Files:**
- Modify: `include/dash_epas_nag.h:1-18`（Spec 引用）
- Test: 全部 EPAS nag 契约

- [ ] **Step 1: 写失败测试**（契约：Mode-C 符号存在）

`test/test_epas_nag_config_contract.py` 加：

```python
def test_dash_epas_nag_has_modec_symbols():
    import pathlib
    src = pathlib.Path("include/dash_epas_nag.h").read_text()
    for sym in ("nextDemandTorque", "handsLevelFromRaw", "onSteeringAngle",
                "clampInt16", "demand_idle", "strong_ramp", "s2_mild"):
        assert sym in src, f"dash_epas_nag.h missing Mode-C symbol: {sym}"
    # B1/B2 removed
    assert "nextTorque()" not in src, "B1 nextTorque() must be removed"
    assert "| 0x40u" not in src and "|= 0x40u" not in src, "B2 forced level must be removed"

def test_dash_epas_nag_spec_ref_updated():
    import pathlib
    src = pathlib.Path("include/dash_epas_nag.h").read_text()
    assert "2026-06-19-upstream-epas-nag-modec-align-design.md" in src, \
        "header must reference the Mode-C spec"
```

- [ ] **Step 2: 验证失败**

Run: `PYTHONPATH=test python3 -m pytest test/test_epas_nag_config_contract.py::test_dash_epas_nag_has_modec_symbols test/test_epas_nag_config_contract.py::test_dash_epas_nag_spec_ref_updated -q`
Expected: FAIL — spec 引用还是 0617。

- [ ] **Step 3: 实现**（`dash_epas_nag.h:12`）

把：
```cpp
// Spec: docs/superpowers/specs/2026-06-17-epas-faithful-nag-unified-design.md
```
改为：
```cpp
// Spec: docs/superpowers/specs/2026-06-19-upstream-epas-nag-modec-align-design.md
//      (supersedes 2026-06-17-epas-faithful-nag-unified-design.md — Mode-C demand-state)
```

- [ ] **Step 4: 全量回归**

Run native:
```
pio test -e native_epas_nag -f test_native_epas_nag
```
Expected: 全 PASS。

Run contracts:
```
PYTHONPATH=test python3 -m pytest test/test_epas_nag_config_contract.py test/test_epas_nag_filters_contract.py test/test_epas_nag_wiring_contract.py test/test_epas_nag_trace_contract.py test/test_epas_nag_trace_persistence_contract.py -q
```
Expected: 全 PASS。

- [ ] **Step 5: Commit**

```bash
git add include/dash_epas_nag.h test/test_epas_nag_config_contract.py
git commit -m "docs(epas-nag): point header at Mode-C spec + contract guard for symbols"
```

---

## Task 12: ESP 构建 + 资产生成 + smoke（授权后烧录）

**Files:**
- Build: `.pio/build/waveshare_single_can_standalone/`
- Asset: `firmware-assets/waveshare-single-can-standalone_<TS>/`（TS 用固定标签 `20260619-modec`，因脚本里禁 `Date.now`）

- [ ] **Step 1: 清缓存（版本串来自 git describe，必须重建）**

```bash
rm -rf .pio/build/waveshare_single_can_standalone
```

- [ ] **Step 2: 构建**

```
pio run -e waveshare_single_can_standalone
```
Expected: `SUCCESS`，RAM ≤ ~45%、Flash ≤ ~32%。若有 `mcp2515_dashboard_ui.h` build-id 副作用：`git restore include/web/mcp2515_dashboard_ui.h`。

- [ ] **Step 3: minify（若 ui.h 受影响）+ merge_bin**

```bash
python3 scripts/minify_dashboard.py 2>/dev/null || true
# merge: bootloader@0x0 partitions@0x8000 ota_data@0x19000 app@0x20000 (full_merged per project layout)
```
按项目既有 merge 流程生成 `full_merged.bin`（bootloader 在 `.pio/build/waveshare_single_can_standalone/bootloader.bin` 根目录，非子目录）。记录 SHA256。

- [ ] **Step 4: smoke（USB 串口连板，授权后 erase + 烧录）**

```bash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem21101 --baud 921600 erase_flash
esptool.py --chip esp32s3 --port /dev/cu.usbmodem21101 --baud 921600 \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 <full_merged_path>
```
Expected: 启动日志 `Handler switched to LEGACY`、`epasNag OFF`、版本串含新 `g<sha>`；`/status` 返回 `epasNagDiag.demand` 对象；防御页 epas 子开关默认 OFF。

- [ ] **Step 5: 写 BUILD_INFO + Commit 资产目录**

在 `firmware-assets/waveshare-single-can-standalone_20260619-modec/BUILD_INFO.md` 记录：git revision、版本串、app size、full_merged SHA256、目标板 MAC、三个 Mode-C 验收点（见下）。`git add` 资产目录 + BUILD_INFO。

```bash
git add firmware-assets/waveshare-single-can-standalone_20260619-modec
git commit -m "feat(epas-nag): Mode-C assets (v2.16-beta.8 align) waveshare single-can"
```

---

## 实车验收（烧录后，Jordan 执行）

1. **Mode-C 行为**：开 EPAS 子开关 + 防御总开关，复现 nag。`/status.epasNagDiag.demand.gate` 应在 `s2_mild`/`strong_ramp`/`strong_hold` 间切换（不再是 `inject`）；`demand.torque` 随 DAS 等级变化；`demand.handsLevel` = 2（强提醒峰值）。
2. **nag 是否减少/消失**：方向盘 nag 应减弱（强提醒时 level 2 + 2.1Nm 是上游已验证组合）。
3. **方向**：trace 的 echoTorque 在右转时应出现负向（<2050），左转正向。
4. 若仍无效：`/epas_nag_trace` CSV + `demand` diag 精确暴露"DAS 哪个等级、我们回了多大扭矩/level、车理不理"——不再盲改。

---

## Self-Review

**Spec coverage:**
- §3 D1（Mode-C + 0x129）→ Task 3–6, 9 ✓
- D2（路径 B）→ 全程改 `DashEpasNagEngine` 骨架 ✓
- D3（2050 基准）→ 所有数值（2100/2250/1850/2000/2260/1840）用 2050 ✓
- D4（0x129 dir）→ Task 2 字段 + Task 9 glue + Task 4/5 dir 用法 ✓
- D5（0xFF strong 兜底）→ Task 5 `test_demand_unseen_0xff_falls_into_strong` + prevDas=0 修正 ✓
- D6（G1–G6 保留）→ 未动；G5+das==15 Task 7 ✓
- D7（默认 OFF）→ 未改 dashEpasNagEnabled ✓
- D8（diag 4 字段）→ Task 2 字段 + Task 10 JSON ✓
- D9（时间常量 500/2000/1000/1000/1500）→ Task 3/4/5 伪码 ✓

**Placeholder scan:** 无 TBD/TODO；每步含完整代码或确切命令。

**Type consistency:** `nextDemandTorque(uint8_t,uint32_t,uint16_t&,uint8_t&)` 签名 Task 3 定义、Task 6 调用一致；`handsLevelFromRaw(int32_t)` Task 1 定义、Task 4/5 调用一致；`onSteeringAngle(int16_t)` Task 2 定义、Task 9 调用一致；diag 字段名（lastDemandState/lastDemandTorque/lastHandsLevel/lastDemandGate）Task 2 定义、Task 6 赋值、Task 10 序列化一致。

**已知偏离 spec（plan 内已注明）:** `prevDas` 初始 `0` 而非 spec §6.2 的 `0xFF`（否则违背 D5，详见 plan 顶部修正说明）。
