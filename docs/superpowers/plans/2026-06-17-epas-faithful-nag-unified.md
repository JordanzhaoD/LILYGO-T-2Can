# EPAS-Faithful 统一方向盘 Nag 抑制 — 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新建 `DashEpasNagEngine`，用上游 EPAS-faithful 的 counter+1 回声 + 均值回归 ~0Nm 扭矩，统一覆盖 legacy/HW3/HW4 的方向盘 nag 抑制，解决 legacy 一直无效的问题。

**Architecture:** 一个 dashboard 级纯结构体引擎（仿 `DashWheelDND`），不含驱动依赖；三个 handler 的 filter 增加 `0x370`(+`0x39B`)，经通用 `mcpDashOnFrame` 回调把帧转发给引擎，引擎门控通过时用全局 `dashDriver` 抢发回声帧。默认关闭（实验性）；过渡期 HW3/HW4 的 bit19 nag 保留为默认开 baseline，faithful 开启时由 `epasNagActive` 标志跳过 bit19（D7）。

**Tech Stack:** ESP-IDF (ESP32-S3) C++17、PlatformIO（`waveshare_single_can_standalone` env + `native` 测试 env）、Unity 单元测试、Python unittest 契约测试。

**Spec:** `docs/superpowers/specs/2026-06-17-epas-faithful-nag-unified-design.md`
**基线分支:** `feature/ui-v11-redesign`（D6）

**关键事实（写代码前必读）：**
- 单 CAN 微雪板只有一条 TWAI 总线（无 MCP2515/CAN-B）。`0x370`(880)/`0x399`(921)/`0x39B`(923) 都走主总线，经 `appLoop → 活跃 handler::handleMessage → onFrame(=mcpDashOnFrame)`。所以本引擎挂在 `mcpDashOnFrame`，用全局 `dashDriver`(mcp2515_dashboard.h:113) 发送——**不要**模仿 `DashWheelDND` 的 CAN-B/MCP2515 tick 路径（那是 `DRIVER_T2CAN_DUAL` 专用，单 CAN 不编译）。
- 帧必须先进 handler 的硬件 filter 才会被读。`LegacyHandler` 原 filter 不含 880 → `0x370` 根本到不了 handler。所以必须给三个 handler 的 `filterIds()` 加 880。
- EPAS `0x370` 校验和 = `sum(data[0..6]) + 0x73`（算术加法，非 CRC），与现有 `NagHandler`（handlers.h:778）一致。

---

## File Structure

| 文件 | 责任 | 动作 |
|---|---|---|
| `include/dash_epas_nag.h` | 引擎纯结构体：常量、纯 helper（校验和/扭矩）、观察、门控、回声构造 | **新建** |
| `test/test_native_epas_nag/test_epas_nag.cpp` | 引擎 Unity 单元测试（校验和/扭矩/门控真值表/回声） | **新建** |
| `platformio.ini` | 新增 `[env:native_epas_nag]` + 加入 `[env:native]` 聚合列表 | **修改** |
| `include/handlers.h` | 三个 handler `filterIds()` 加 880(+923)；HW3/HW4 bit19 加 `!epasNagActive` 门控；`CarManagerBase` 加 `epasNagActive` | **修改** |
| `include/web/mcp2515_dashboard.h` | include、全局、runtime 门控、`dashEpasNagOnFrame` 胶水、`mcpDashOnFrame` 调用、NVS 持久化、HTTP `/defense_config`、`/status` JSON | **修改** |
| `test/test_epas_nag_filters_contract.py` | filter 契约（880/923 存在、count 匹配） | **新建** |
| `test/test_epas_nag_wiring_contract.py` | 接线契约（全局/胶水/runtime/include/bit19 门控） | **新建** |
| `test/test_epas_nag_config_contract.py` | 持久化/HTTP/status JSON 契约 | **新建** |

---

## Task 1: 引擎纯结构体 + 纯函数单元测试（核心）

**Files:**
- Create: `include/dash_epas_nag.h`
- Test: `test/test_native_epas_nag/test_epas_nag.cpp`
- Modify: `platformio.ini`（加 env + 聚合列表）

- [ ] **Step 1: 创建引擎头文件 `include/dash_epas_nag.h`（先只放常量 + 纯 helper，让测试能引用）**

```cpp
#pragma once

// ──────────────────────────────────────────────────────────────
// dash_epas_nag.h — EPAS-faithful unified steering nag suppression
//
// Echoes CAN 0x370 (EPAS3P_sysStatus, ~100 Hz) with counter+1 and a
// mean-reverting ~0 Nm torque so the car INFERS hands-on instead of
// reading a forced flag. One 0x370 path covers legacy/HW3/HW4; the
// per-generation DAS hands-on gate (0x399 legacy/HW3, 0x39B Highland
// HW3/HW4) only decides WHEN to inject.
//
// Spec: docs/superpowers/specs/2026-06-17-epas-faithful-nag-unified-design.md
// Default OFF (experimental). bit19 nag stays as default-on baseline;
// this engine takes over only when dashEpasNagEnabled is set (D7).
//
// Deterministic: no millis()/random() inside — time is passed in via
// nowMs; PRNG is a seeded xorshift32. Unit-testable as a plain struct.
// ──────────────────────────────────────────────────────────────

#include <cstdint>
#include <cstring>

#include "dash_tx_evidence.h"

struct DashEpasNagDiag
{
    uint32_t echoCount = 0;
    uint32_t suppressCount = 0;
    uint8_t dasHandsOnState = 0xFF;   // 0xFF = unseen
    uint8_t lastHandsOn = 0;
    const char *lastGate = "idle";
    DashTxEvidence tx{};
};

struct DashEpasNagEngine
{
    // ── constants ────────────────────────────────────────────
    static constexpr uint32_t kCanId{880};          // 0x370 EPAS3P_sysStatus
    static constexpr uint32_t kDasIdLegacy{921};    // 0x399 DAS_status (legacy/HW3/Juniper fallback)
    static constexpr uint32_t kDasIdHighland{923};  // 0x39B DAS_status (Highland HW3/HW4)
    static constexpr uint16_t kTorqueCenter{2050};  // raw for 0.00 Nm  (Nm = raw*0.01 - 20.5)
    static constexpr uint16_t kTorqueMin{1780};     // ~-2.70 Nm
    static constexpr uint16_t kTorqueMax{2500};     // ~+4.50 Nm
    static constexpr uint16_t kPullDivisor{16};     // mean-reversion pull strength
    static constexpr uint16_t kStepSpan{81};        // xorshift % 81 -> [0,80], step = -40..+40
    static constexpr uint32_t kBlackoutMs{1500};    // G6 post-disengage silence window
    static constexpr uint8_t kDasUnseen{0xFF};

    // ── runtime state (fed by dashboard glue) ───────────────
    bool enabled{false};        // mirrors dashEpasNagEnabled (G2)
    bool canActive{false};      // mirrors canActive (G3)
    bool defenseEnabled{false}; // mirrors dashDefenseEnabled (G1)
    uint8_t dasHandsOnState{kDasUnseen};
    bool prevApActive{false};
    uint32_t apDisengageMs{0};  // 0 = no recent disengage
    uint16_t torqueWalk{kTorqueCenter};
    uint32_t prng{0xDEADBEEFu};
    DashEpasNagDiag diag{};

    // ── pure helpers ─────────────────────────────────────────

    // EPAS 0x370 additive checksum: byte7 = (sum(data[0..6]) + 0x73) & 0xFF
    // 0x73 = (0x370 & 0xFF) + (0x370 >> 8) = 0x70 + 0x03. Matches handlers.h:778.
    static uint8_t teslaChecksum0x370(const uint8_t d[7])
    {
        uint16_t sum = 0x70u + 0x03u;
        for (int i = 0; i < 7; ++i)
            sum += d[i];
        return static_cast<uint8_t>(sum & 0xFFu);
    }

    static uint16_t clampTorque(int32_t v)
    {
        if (v < static_cast<int32_t>(kTorqueMin))
            return kTorqueMin;
        if (v > static_cast<int32_t>(kTorqueMax))
            return kTorqueMax;
        return static_cast<uint16_t>(v);
    }

    uint32_t xorshift32()
    {
        uint32_t x = prng;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        prng = x;
        return x;
    }

    // Mean-reverting random walk centered on 0 Nm (raw 2050).
    uint16_t nextTorque()
    {
        int32_t step = static_cast<int32_t>(xorshift32() % kStepSpan) - 40; // ±40
        int32_t w = static_cast<int32_t>(torqueWalk) + step;
        w += (static_cast<int32_t>(kTorqueCenter) - w) / static_cast<int32_t>(kPullDivisor);
        torqueWalk = clampTorque(w);
        return torqueWalk;
    }

    // ── observation (called by dashboard glue) ───────────────
    void noteRuntime(bool enabled_, bool canActive_, bool defenseEnabled_)
    {
        enabled = enabled_;
        canActive = canActive_;
        defenseEnabled = defenseEnabled_;
    }

    // 0x399 / 0x39B DAS_status: hands-on state = byte5 bits[5:2]
    void onDasGate(const uint8_t data[8])
    {
        dasHandsOnState = static_cast<uint8_t>((data[5] >> 2) & 0x0Fu);
        diag.dasHandsOnState = dasHandsOnState;
    }

    // Track AP falling edge for the G6 blackout window.
    void noteApState(bool active, uint32_t nowMs)
    {
        if (prevApActive && !active)
            apDisengageMs = nowMs;   // disengage edge
        prevApActive = active;
    }

    // ── gate G1-G6 ───────────────────────────────────────────
    bool shouldInject(uint8_t handsOn, uint32_t nowMs) const
    {
        if (!enabled || !canActive || !defenseEnabled)          // G1 G2 G3
            return false;
        if (handsOn == 1)                                        // G4: real hand detected
            return false;
        const uint8_t das = dasHandsOnState;
        if (das == 0 || das == 8)                                // G5: NOT_REQD / SUSPENDED
            return false;                                        //     (0xFF unseen -> passes -> D5 inject)
        if (apDisengageMs != 0 && (nowMs - apDisengageMs) < kBlackoutMs) // G6
            return false;
        return true;
    }

    // ── echo builder ─────────────────────────────────────────
    // On a 0x370 frame: if gate open, write faithful echo to outData[8], return true.
    bool tryBuildEcho(const uint8_t inData[8], uint32_t nowMs, uint8_t outData[8])
    {
        const uint8_t handsOn = static_cast<uint8_t>((inData[4] >> 6) & 0x03u);
        diag.lastHandsOn = handsOn;

        if (!shouldInject(handsOn, nowMs))
        {
            diag.suppressCount++;
            diag.lastGate = "gated";
            return false;
        }

        const uint16_t torq = nextTorque();
        outData[0] = inData[0];
        outData[1] = inData[1];
        outData[2] = static_cast<uint8_t>((inData[2] & 0xF0u) | ((torq >> 8) & 0x0Fu));
        outData[3] = static_cast<uint8_t>(torq & 0xFFu);
        outData[4] = inData[4];                       // faithful: NEVER touch handsOnLevel
        outData[5] = inData[5];
        uint8_t cnt = static_cast<uint8_t>(inData[6] & 0x0Fu);
        cnt = static_cast<uint8_t>((cnt + 1) & 0x0Fu);
        outData[6] = static_cast<uint8_t>((inData[6] & 0xF0u) | cnt);
        outData[7] = teslaChecksum0x370(outData);     // sum(outData[0..6]) + 0x73

        diag.echoCount++;
        diag.lastGate = "inject";
        return true;
    }

    void reset()
    {
        torqueWalk = kTorqueCenter;
        prng = 0xDEADBEEFu;
        dasHandsOnState = kDasUnseen;
        diag.dasHandsOnState = kDasUnseen;
        prevApActive = false;
        apDisengageMs = 0;
    }
};
```

- [ ] **Step 2: 创建测试文件 `test/test_native_epas_nag/test_epas_nag.cpp`（完整，先写好全部测试）**

```cpp
#include <unity.h>
#include "dash_epas_nag.h"

// Build a 0x370 EPAS frame (8 bytes). handsOnLevel -> data[4] bits[7:6]; counter -> data[6] low nibble.
static void buildEpasFrame(uint8_t data[8], uint8_t handsOnLevel, uint8_t counter)
{
    for (int i = 0; i < 8; i++)
        data[i] = 0;
    data[4] = static_cast<uint8_t>((handsOnLevel & 0x03) << 6);
    data[6] = static_cast<uint8_t>(counter & 0x0F);
}

static DashEpasNagEngine makeArmedEngine()
{
    DashEpasNagEngine e;
    e.noteRuntime(true, true, true); // enabled + canActive + defense; dasHandsOnState stays 0xFF (unseen -> D5 inject)
    return e;
}

// ── constants ────────────────────────────────────────────────────────
void test_nag_constants()
{
    TEST_ASSERT_EQUAL_HEX32(880, DashEpasNagEngine::kCanId);
    TEST_ASSERT_EQUAL_UINT16(2050, DashEpasNagEngine::kTorqueCenter);
    TEST_ASSERT_EQUAL_UINT16(1780, DashEpasNagEngine::kTorqueMin);
    TEST_ASSERT_EQUAL_UINT16(2500, DashEpasNagEngine::kTorqueMax);
    TEST_ASSERT_EQUAL_UINT32(1500, DashEpasNagEngine::kBlackoutMs);
}

// ── checksum (additive, NOT crc) ─────────────────────────────────────
void test_checksum_additive_formula()
{
    uint8_t d[7];
    for (uint16_t v = 0; v < 256; v += 37)
    {
        for (int i = 0; i < 7; i++)
            d[i] = static_cast<uint8_t>(v + i * 7);
        uint16_t expected = 0x70u + 0x03u;
        for (int i = 0; i < 7; i++)
            expected += d[i];
        TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(expected & 0xFF), DashEpasNagEngine::teslaChecksum0x370(d));
    }
}

void test_checksum_counter_additive()
{
    // Incrementing the counter byte by 1 must increment the checksum by exactly 1
    // (this additive property is what distinguishes the EPAS checksum from a CRC).
    uint8_t d[7] = {0x10, 0x20, 0x08, 0x02, 0x40, 0x00, 0x05};
    uint8_t cs0 = DashEpasNagEngine::teslaChecksum0x370(d);
    d[6] = 0x06;
    uint8_t cs1 = DashEpasNagEngine::teslaChecksum0x370(d);
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>((cs0 + 1) & 0xFF), cs1);
}

// ── torque walk ──────────────────────────────────────────────────────
void test_torque_stays_in_bounds()
{
    DashEpasNagEngine e;
    for (int n = 0; n < 500; n++)
    {
        uint16_t t = e.nextTorque();
        TEST_ASSERT_TRUE(t >= DashEpasNagEngine::kTorqueMin);
        TEST_ASSERT_TRUE(t <= DashEpasNagEngine::kTorqueMax);
    }
}

void test_torque_reverts_toward_center()
{
    DashEpasNagEngine e;
    e.torqueWalk = DashEpasNagEngine::kTorqueMax; // start at +4.5 Nm rail
    uint32_t sum = 0;
    const int N = 300;
    for (int n = 0; n < N; n++)
        sum += e.nextTorque();
    uint32_t avg = sum / N;
    // mean-reversion pulls the long-run average near center 2050 (within ±1 Nm)
    TEST_ASSERT_TRUE(avg >= 1950 && avg <= 2150);
}

// ── gate G1-G6 truth table ───────────────────────────────────────────
void test_gate_disabled_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    e.noteRuntime(false, true, true);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_canactive_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    e.noteRuntime(true, false, true);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_defense_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    e.noteRuntime(true, true, false);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_real_hand_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    TEST_ASSERT_FALSE(e.shouldInject(1, 1000)); // handsOn=1 -> real hand
}
void test_gate_das_satisfied_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(0 << 2); // NOT_REQD
    e.onDasGate(das);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_das_suspended_blocks()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(8 << 2); // SUSPENDED
    e.onDasGate(das);
    TEST_ASSERT_FALSE(e.shouldInject(0, 1000));
}
void test_gate_das_unseen_injects_d5()
{
    DashEpasNagEngine e = makeArmedEngine();
    // dasHandsOnState stays default 0xFF -> D5: inject
    TEST_ASSERT_TRUE(e.shouldInject(0, 1000));
}
void test_gate_das_escalating_injects()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t das[8] = {0};
    das[5] = static_cast<uint8_t>(3 << 2); // escalation level 3
    e.onDasGate(das);
    TEST_ASSERT_TRUE(e.shouldInject(0, 1000));
}
void test_gate_blackout_blocks_then_clears()
{
    DashEpasNagEngine e = makeArmedEngine();
    e.noteApState(true, 1000);  // AP active
    e.noteApState(false, 2000); // disengage at t=2000
    TEST_ASSERT_FALSE(e.shouldInject(0, 2500));             // 500 ms < 1500 ms window
    TEST_ASSERT_TRUE(e.shouldInject(0, 2000 + 1500 + 1));   // after window
}

// ── echo builder ─────────────────────────────────────────────────────
void test_echo_gated_returns_false()
{
    DashEpasNagEngine e;
    e.noteRuntime(false, true, true); // disabled -> gated
    uint8_t in[8];
    buildEpasFrame(in, 0, 5);
    uint8_t out[8] = {0};
    TEST_ASSERT_FALSE(e.tryBuildEcho(in, 1000, out));
    TEST_ASSERT_EQUAL_UINT32(0, e.diag.echoCount);
}
void test_echo_faithful_preserves_handsonlevel()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 5); // handsOnLevel 0
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 1000, out));
    TEST_ASSERT_EQUAL_HEX8(in[4], out[4]); // untouched, NOT forced to 0x40
}
void test_echo_counter_increments_and_wraps()
{
    DashEpasNagEngine e = makeArmedEngine();
    for (uint8_t c = 0; c < 16; c++)
    {
        uint8_t in[8];
        buildEpasFrame(in, 0, c);
        uint8_t out[8];
        TEST_ASSERT_TRUE(e.tryBuildEcho(in, 1000 + c, out));
        uint8_t expected = static_cast<uint8_t>((c + 1) & 0x0F);
        TEST_ASSERT_EQUAL_HEX8(expected, out[6] & 0x0F);
    }
}
void test_echo_checksum_correct()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 9);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 1000, out));
    uint16_t expected = 0x70u + 0x03u;
    for (int i = 0; i < 7; i++)
        expected += out[i];
    TEST_ASSERT_EQUAL_HEX8(static_cast<uint8_t>(expected & 0xFF), out[7]);
}
void test_echo_torque_in_valid_range_data23()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 3);
    uint8_t out[8];
    TEST_ASSERT_TRUE(e.tryBuildEcho(in, 1000, out));
    uint16_t raw = static_cast<uint16_t>(((out[2] & 0x0F) << 8) | out[3]);
    TEST_ASSERT_TRUE(raw >= DashEpasNagEngine::kTorqueMin);
    TEST_ASSERT_TRUE(raw <= DashEpasNagEngine::kTorqueMax);
}
void test_echo_count_increments()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    e.tryBuildEcho(in, 1000, out);
    e.tryBuildEcho(in, 1010, out);
    e.tryBuildEcho(in, 1020, out);
    TEST_ASSERT_EQUAL_UINT32(3, e.diag.echoCount);
}
void test_reset_clears_state()
{
    DashEpasNagEngine e = makeArmedEngine();
    uint8_t in[8];
    buildEpasFrame(in, 0, 1);
    uint8_t out[8];
    e.tryBuildEcho(in, 1000, out);
    e.reset();
    TEST_ASSERT_EQUAL_UINT16(DashEpasNagEngine::kTorqueCenter, e.torqueWalk);
    TEST_ASSERT_EQUAL_UINT8(DashEpasNagEngine::kDasUnseen, e.dasHandsOnState);
    TEST_ASSERT_EQUAL_UINT32(0, e.apDisengageMs);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_nag_constants);
    RUN_TEST(test_checksum_additive_formula);
    RUN_TEST(test_checksum_counter_additive);
    RUN_TEST(test_torque_stays_in_bounds);
    RUN_TEST(test_torque_reverts_toward_center);
    RUN_TEST(test_gate_disabled_blocks);
    RUN_TEST(test_gate_canactive_blocks);
    RUN_TEST(test_gate_defense_blocks);
    RUN_TEST(test_gate_real_hand_blocks);
    RUN_TEST(test_gate_das_satisfied_blocks);
    RUN_TEST(test_gate_das_suspended_blocks);
    RUN_TEST(test_gate_das_unseen_injects_d5);
    RUN_TEST(test_gate_das_escalating_injects);
    RUN_TEST(test_gate_blackout_blocks_then_clears);
    RUN_TEST(test_echo_gated_returns_false);
    RUN_TEST(test_echo_faithful_preserves_handsonlevel);
    RUN_TEST(test_echo_counter_increments_and_wraps);
    RUN_TEST(test_echo_checksum_correct);
    RUN_TEST(test_echo_torque_in_valid_range_data23);
    RUN_TEST(test_echo_count_increments);
    RUN_TEST(test_reset_clears_state);
    return UNITY_END();
}
```

- [ ] **Step 3: 在 `platformio.ini` 增加 native env（镜像 `[env:native_wheel_dnd]`，位于 ~line 347 之后）**

在 `[env:native_wheel_dnd]` 块之后追加：

```ini
[env:native_epas_nag]
platform = native
extra_scripts = pre:scripts/platformio_native_env.py
build_flags = -std=c++17 -DNATIVE_BUILD
test_filter = test_native_epas_nag
```

- [ ] **Step 4: 把 `test_native_epas_nag` 加入 `[env:native]` 聚合测试列表（~line 253-262）**

在 `[env:native]` 的 `test_native_*` 列表末尾（`test_native_plugin_engine` 之后）追加一行 `test_native_epas_nag`。

- [ ] **Step 5: 跑测试，确认全绿**

Run: `pio test -e native_epas_nag`
Expected: 全部 21 个测试 PASS，0 FAIL。

- [ ] **Step 6: 提交**

```bash
git add include/dash_epas_nag.h test/test_native_epas_nag/test_epas_nag.cpp platformio.ini
git commit -m "feat(epas-nag): add DashEpasNagEngine pure struct + native unit tests"
```

---

## Task 2: 三个 handler filter 加入 0x370(+0x39B) + filter 契约测试

**Files:**
- Modify: `include/handlers.h`（LegacyHandler ~287、HW3Handler ~498、HW4Handler ~833）
- Test: `test/test_epas_nag_filters_contract.py`

- [ ] **Step 1: 写失败的契约测试 `test/test_epas_nag_filters_contract.py`**

```python
import unittest
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = (ROOT / "include" / "handlers.h").read_text(encoding="utf-8")


def handler_block(name):
    m = re.search(r"struct %s\s*:[^=]*?\n(?:.*?\n)*?\s*uint8_t filterIdCount\(\) const override \{ return \d+; \}" % name, H)
    return m.group(0) if m else ""


class FiltersContract(unittest.TestCase):
    def test_legacy_includes_0x370(self):
        self.assertIn("880", handler_block("LegacyHandler"))

    def test_hw3_includes_0x370_and_0x39b(self):
        b = handler_block("HW3Handler")
        self.assertIn("880", b)
        self.assertIn("923", b)

    def test_hw4_includes_0x370_and_0x39b(self):
        b = handler_block("HW4Handler")
        self.assertIn("880", b)
        self.assertIn("923", b)

    def test_filter_counts_match_array_entries(self):
        for name in ("LegacyHandler", "HW3Handler", "HW4Handler"):
            b = handler_block(name)
            self.assertTrue(b, "%s block missing" % name)
            arr = re.search(r"ids\[\]\s*=\s*\{([^}]*)\}", b)
            cnt = re.search(r"filterIdCount\(\) const override \{ return (\d+); \}", b)
            self.assertTrue(arr and cnt, "%s ids/count not found" % name)
            entries = [x for x in arr.group(1).replace(" ", "").split(",") if x]
            self.assertEqual(int(cnt.group(1)), len(entries), "%s count mismatch" % name)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 跑测试，确认它失败**

Run: `PYTHONPATH=test python3 -m unittest test_epas_nag_filters_contract -v`
Expected: FAIL（880/923 还没加进 filter）。

- [ ] **Step 3: 改 LegacyHandler filter（handlers.h:287-290）**

old:
```cpp
        static constexpr uint32_t ids[] = {69, 280, 390, 760, 920, 921, 1006, 1080, CAN_ID_OTA_STATUS};
        return ids;
    }
    uint8_t filterIdCount() const override { return 9; }
```
new:
```cpp
        // 880 (0x370 EPAS3P_sysStatus) added for EPAS-faithful nag engine.
        static constexpr uint32_t ids[] = {69, 280, 390, 760, 880, 920, 921, 1006, 1080, CAN_ID_OTA_STATUS};
        return ids;
    }
    uint8_t filterIdCount() const override { return 10; }
```

- [ ] **Step 4: 改 HW3 + HW4 filter（两处文本完全相同，用 `replace_all: true`）**

old（HW3 handlers.h:498 与 HW4 handlers.h:833 完全相同）:
```cpp
        static constexpr uint32_t ids[] = {280, 390, 920, 921, 1016, 1021, 2047, CAN_ID_OTA_STATUS};
        return ids;
    }
    uint8_t filterIdCount() const override { return 8; }
```
new（`replace_all: true`，同时改 HW3 和 HW4）:
```cpp
        // 880 (0x370 EPAS3P_sysStatus) + 923 (0x39B DAS_status Highland/HW4) added for EPAS-faithful nag engine.
        static constexpr uint32_t ids[] = {280, 390, 880, 920, 921, 923, 1016, 1021, 2047, CAN_ID_OTA_STATUS};
        return ids;
    }
    uint8_t filterIdCount() const override { return 10; }
```

- [ ] **Step 5: 跑测试，确认全绿**

Run: `PYTHONPATH=test python3 -m unittest test_epas_nag_filters_contract -v`
Expected: 4 个测试 PASS。

- [ ] **Step 6: 提交**

```bash
git add include/handlers.h test/test_epas_nag_filters_contract.py
git commit -m "feat(epas-nag): add 0x370(+0x39B) to Legacy/HW3/HW4 filters so EPAS frames reach the engine"
```

---

## Task 3: D7 — bit19 nag 门控 + CarManagerBase.epasNagActive

**Files:**
- Modify: `include/handlers.h`（CarManagerBase ~72、HW3 mux1 ~602、HW4 mux1 ~946）
- Test: `test/test_epas_nag_wiring_contract.py`（在 Task 4 一起建；本任务的断言放那里——见 Task 4 Step 1 的 `test_bit19_gated_by_epas_nag_active`）

- [ ] **Step 1: 给 CarManagerBase 加 `epasNagActive` 字段（handlers.h:72 之后）**

old:
```cpp
    Shared<bool> banShieldEnable{false};
```
new:
```cpp
    Shared<bool> banShieldEnable{false};
    Shared<bool> epasNagActive{false};  // EPAS-faithful nag engine active (D7): when true, skip bit19 nag
```

- [ ] **Step 2: 给 HW3 mux1 nag 加 `!epasNagActive` 门控（handlers.h:601-602）**

old:
```cpp
            // ── Mux 1: Nag suppression ─────────────────────────────────────
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
```
new:
```cpp
            // ── Mux 1: Nag suppression (skipped when EPAS-faithful nag active — D7) ──
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed() && !(bool)epasNagActive)
```

- [ ] **Step 3: 给 HW4 mux1 nag 加 `!epasNagActive` 门控（handlers.h:944-946）**

old:
```cpp
            // Mux 1: Nag suppression + FSD ready signal
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed())
```
new:
```cpp
            // Mux 1: Nag suppression + FSD ready signal (skipped when EPAS-faithful nag active — D7)
            if (index == 1 && (bool)fsdTriggered && injectionAllowed() && builtInFsdInjectionAllowed() && !(bool)epasNagActive)
```

> 注意：HW4 这段同时清 bit19 和置 bit47(FSD ready)。门控只跳过整个 mux1 块——当 faithful 接管 nag 时，FSD ready 信号也随之跳过。这是可接受的：faithful 开启是实验性 opt-in，且 FSD 激活本身由 mux0 负责（不受影响）。若评审要求 bit47 与 nag 解耦，单独再拆分；本期按 spec D7 整块跳过。

- [ ] **Step 4: 验证编译（先不提交，下一任务一起测）**

Run: `pio run -e native_hw3 -t 0 2>&1 | tail -5 || pio test -e native_hw3 2>&1 | tail -15`
Expected: 编译通过（`epasNagActive` 是 CarManagerBase 成员，HW3/HW4 可见）。若 native_hw3 测试因门控变化有断言失败，记录下来在 Task 6 全量回归里处理。

- [ ] **Step 5: 暂不提交，与 Task 4 一起提交（接线未完成前 `epasNagActive` 永远 false，行为不变）**

---

## Task 4: Dashboard 接线（include / 全局 / runtime 门控 / 胶水函数 / mcpDashOnFrame 钩子）+ 接线契约测试

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`（include ~48、全局 ~196、runtime ~1291、mcpDashOnFrame ~804、新增 `dashEpasNagOnFrame` 置于 mcpDashOnFrame 之前）
- Test: `test/test_epas_nag_wiring_contract.py`

- [ ] **Step 1: 写失败的接线+bit19 契约测试 `test/test_epas_nag_wiring_contract.py`**

```python
import unittest
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = (ROOT / "include" / "handlers.h").read_text(encoding="utf-8")
D = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(encoding="utf-8")


class WiringContract(unittest.TestCase):
    def test_include_dash_epas_nag_h(self):
        self.assertIn('#include "dash_epas_nag.h"', D)

    def test_globals_declared(self):
        self.assertIn("dashEpasNagEnabled", D)
        self.assertIn("dashEpasNagCtrl", D)
        self.assertIn("DashEpasNagEngine dashEpasNagCtrl", D)

    def test_glue_function_defined_and_called(self):
        self.assertIn("void dashEpasNagOnFrame(const CanFrame &f)", D)
        # called from mcpDashOnFrame
        self.assertRegex(D, r"static void mcpDashOnFrame\([^)]*\)\s*\{(?:(?!\}).)*dashEpasNagOnFrame\(f\)", D)

    def test_runtime_gate_set_in_apply_runtime_state(self):
        self.assertRegex(D, r"dashHandler->epasNagActive\s*=\s*dashEpasNagEnabled\s*&&\s*canActive\s*&&\s*dashDefenseEnabled", D)

    def test_epas_nag_active_field_on_car_manager_base(self):
        self.assertRegex(H, r"struct CarManagerBase(?:(?!\nstruct ).)*?Shared<bool> epasNagActive\{false\}", H, re.S)

    def test_bit19_gated_by_epas_nag_active(self):
        # both HW3 and HW4 mux1 nag blocks must include the !epasNagActive gate
        self.assertEqual(D.count("!(bool)epasNagActive"), 0)  # gating lives in handlers.h
        self.assertGreaterEqual(H.count("!(bool)epasNagActive"), 2)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 跑测试，确认它失败**

Run: `PYTHONPATH=test python3 -m unittest test_epas_nag_wiring_contract -v`
Expected: 多数 FAIL（接线还没做；`bit19_gated` 应已 PASS，因 Task 3 已改 handlers.h）。

- [ ] **Step 3: 加 include（mcp2515_dashboard.h:48 之后）**

old:
```cpp
#include "dash_wheel_dnd.h"
```
new:
```cpp
#include "dash_wheel_dnd.h"
#include "dash_epas_nag.h"
```

- [ ] **Step 4: 加全局变量（mcp2515_dashboard.h:196 `dashApEapCompatible` 之后）**

old:
```cpp
static bool dashApEapCompatible = true;
```
new:
```cpp
static bool dashApEapCompatible = true;
static bool dashEpasNagEnabled = false;   // EPAS-faithful nag (experimental, default off; D7)
static DashEpasNagEngine dashEpasNagCtrl; // unified EPAS nag engine instance
```

- [ ] **Step 5: 加 runtime 门控（mcp2515_dashboard.h:1291 `dashHandler->banShieldEnable = nvsBanShieldEnable;` 之后，在 `if (dashHandler)` 块内）**

old:
```cpp
        dashHandler->banShieldEnable = nvsBanShieldEnable;
```
new:
```cpp
        dashHandler->banShieldEnable = nvsBanShieldEnable;
        dashHandler->epasNagActive = dashEpasNagEnabled && canActive && dashDefenseEnabled;
```

- [ ] **Step 6: 在 `mcpDashOnFrame` 之前新增胶水函数 `dashEpasNagOnFrame`（mcp2515_dashboard.h:788 `static void mcpDashOnFrame` 之前插入）**

old:
```cpp
static void mcpDashOnFrame(const CanFrame &f)
{
```
new:
```cpp
// EPAS-faithful nag: observe DAS gate frames and echo 0x370 when the gate is open.
// Hooked from mcpDashOnFrame (universal per-frame callback on the active bus).
// Uses the global dashDriver to send — correct for the single-CAN Waveshare board
// (0x370/0x399/0x39B all arrive on the main TWAI bus).
static void dashEpasNagOnFrame(const CanFrame &f)
{
    if (!dashEpasNagEnabled)
        return; // cheap early-out; engine also checks, but skip work when off
    unsigned long now = millis();
    dashEpasNagCtrl.noteRuntime(dashEpasNagEnabled, canActive, dashDefenseEnabled);
    if (dashHandler)
        dashEpasNagCtrl.noteApState((bool)dashHandler->APActive, now);

    if (f.id == 921 || f.id == 923) // 0x399 / 0x39B DAS_status
    {
        if (f.dlc >= 6)
            dashEpasNagCtrl.onDasGate(f.data);
        return;
    }
    if (f.id == 880 && f.dlc >= 8) // 0x370 EPAS3P_sysStatus
    {
        uint8_t out[8];
        if (dashEpasNagCtrl.tryBuildEcho(f.data, now, out))
        {
            CanFrame echo;
            echo.id = 880;
            echo.dlc = 8;
            memcpy(echo.data, out, 8);
            echo.bus = f.bus;
            bool ok = dashDriver && dashDriver->send(echo);
            dashEpasNagCtrl.diag.tx.recordAttempt(echo.id, echo.bus, echo.dlc, echo.data, now);
            dashEpasNagCtrl.diag.tx.recordResult(ok);
            dashRecordCanFrame(echo, ok ? 'T' : 'E');
        }
    }
}

static void mcpDashOnFrame(const CanFrame &f)
{
```

> 说明：`dashRecordCanFrame`、`dashDriver`、`canActive`、`dashHandler` 都是在 mcpDashOnFrame(788) 之前已声明的全局/函数，可直接用。`memcpy` 已由 `dash_tx_evidence.h` 等链路带入（`<cstring>`）。胶水函数用 `millis()`——仅 ESP 端调用，引擎本身不调 millis（已被 Task 1 的 native 单测覆盖）。

- [ ] **Step 7: 在 `mcpDashOnFrame` 内调用胶水函数（mcp2515_dashboard.h:804 `dashRecordApRestoreFrame(f, now);` 之后）**

old:
```cpp
    dashRecordApRestoreFrame(f, now);
    dashRecordCanFrame(f, 'R');
```
new:
```cpp
    dashRecordApRestoreFrame(f, now);
    dashEpasNagOnFrame(f);
    dashRecordCanFrame(f, 'R');
```

- [ ] **Step 8: 跑接线契约测试 + native 引擎测试，确认全绿**

Run: `PYTHONPATH=test python3 -m unittest test_epas_nag_wiring_contract -v`
Expected: 全部 PASS。
Run: `pio test -e native_epas_nag`
Expected: 21 PASS（回归保护，引擎未变）。

- [ ] **Step 9: 提交（Task 3 + Task 4 合并提交）**

```bash
git add include/handlers.h include/web/mcp2515_dashboard.h test/test_epas_nag_wiring_contract.py
git commit -m "feat(epas-nag): wire DashEpasNagEngine into mcpDashOnFrame + gate bit19 nag via epasNagActive (D7)"
```

---

## Task 5: 持久化 / HTTP / /status JSON + 配置契约测试

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`（dashSavePrefs ~1348、dashLoadPrefs ~1550、handleDefenseConfig ~3018/& ~3059、dashDefenseConfigJson ~3003、handleStatus ~2463）
- Test: `test/test_epas_nag_config_contract.py`

- [ ] **Step 1: 写失败的配置契约测试 `test/test_epas_nag_config_contract.py`**

```python
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
D = (ROOT / "include" / "web" / "mcp2515_dashboard.h").read_text(encoding="utf-8")


class ConfigContract(unittest.TestCase):
    def test_persist_save_key(self):
        self.assertIn('putBool("def_epnag", dashEpasNagEnabled)', D)

    def test_persist_load_key(self):
        self.assertIn('dashEpasNagEnabled = prefs.getBool("def_epnag", false)', D)

    def test_http_handler_reads_epas_nag(self):
        self.assertIn('server.hasArg("epas_nag")', D)
        self.assertIn('dashEpasNagEnabled = dashArgTruthy(server.arg("epas_nag"))', D)

    def test_defense_config_json_has_field(self):
        self.assertIn('",\\"epas_nag\\":', D)

    def test_status_json_has_field(self):
        self.assertIn('",\\"epasNag\\":', D)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: 跑测试，确认它失败**

Run: `PYTHONPATH=test python3 -m unittest test_epas_nag_config_contract -v`
Expected: 全 FAIL。

- [ ] **Step 3: dashSavePrefs 加持久化（mcp2515_dashboard.h:1348 `def_apeap` 行之后）**

old:
```cpp
    prefs.putBool("def_apeap", dashApEapCompatible);
```
new:
```cpp
    prefs.putBool("def_apeap", dashApEapCompatible);
    prefs.putBool("def_epnag", dashEpasNagEnabled);
```

- [ ] **Step 4: dashLoadPrefs 加读取（mcp2515_dashboard.h:1550 `def_apeap` 行之后）**

old:
```cpp
    dashApEapCompatible = prefs.getBool("def_apeap", true);
```
new:
```cpp
    dashApEapCompatible = prefs.getBool("def_apeap", true);
    dashEpasNagEnabled = prefs.getBool("def_epnag", false);
```

- [ ] **Step 5: handleDefenseConfig 的 hasArg 条件加 `epas_nag`（mcp2515_dashboard.h:3018-3020）**

old:
```cpp
        server.hasArg("ap_eap_compatible") || server.hasArg("dnd_volume") ||
        server.hasArg("dnd_speed"))
    {
```
new:
```cpp
        server.hasArg("ap_eap_compatible") || server.hasArg("dnd_volume") ||
        server.hasArg("dnd_speed") || server.hasArg("epas_nag"))
    {
```

- [ ] **Step 6: handleDefenseConfig 加参数解析（mcp2515_dashboard.h:3059 `ap_eap_compatible` 解析块之后）**

old:
```cpp
        if (server.hasArg("ap_eap_compatible"))
            dashApEapCompatible = dashArgTruthy(server.arg("ap_eap_compatible"));
```
new:
```cpp
        if (server.hasArg("ap_eap_compatible"))
            dashApEapCompatible = dashArgTruthy(server.arg("ap_eap_compatible"));
        if (server.hasArg("epas_nag"))
            dashEpasNagEnabled = dashArgTruthy(server.arg("epas_nag"));
```

- [ ] **Step 7: dashDefenseConfigJson 加字段（mcp2515_dashboard.h:3003-3004 `ap_eap_compatible` 字段之后）**

old:
```cpp
    j += ",\"ap_eap_compatible\":";
    j += dashApEapCompatible ? "true" : "false";
```
new:
```cpp
    j += ",\"ap_eap_compatible\":";
    j += dashApEapCompatible ? "true" : "false";
    j += ",\"epas_nag\":";
    j += dashEpasNagEnabled ? "true" : "false";
```

- [ ] **Step 8: handleStatus 顶层加 `epasNag` 字段（mcp2515_dashboard.h:2462-2464 `dndSpeed` 之后、`appendCapabilitiesJson` 之前）**

old:
```cpp
    j += ",\"dndSpeed\":";
    j += dashDndSpeed ? "true" : "false";
    appendCapabilitiesJson(j, hwMode, effectiveHw);
```
new:
```cpp
    j += ",\"dndSpeed\":";
    j += dashDndSpeed ? "true" : "false";
    j += ",\"epasNag\":";
    j += dashEpasNagEnabled ? "true" : "false";
    appendCapabilitiesJson(j, hwMode, effectiveHw);
```

- [ ] **Step 9: 跑配置契约测试，确认全绿**

Run: `PYTHONPATH=test python3 -m unittest test_epas_nag_config_contract -v`
Expected: 5 个测试 PASS。

- [ ] **Step 10: 提交**

```bash
git add include/web/mcp2515_dashboard.h test/test_epas_nag_config_contract.py
git commit -m "feat(epas-nag): persist dashEpasNagEnabled + /defense_config epas_nag + /status epasNag"
```

---

## Task 6: 全量构建 + 回归 + 资产/烧录说明

**Files:** 无源码改动（验证 + 文档）

- [ ] **Step 1: 单 CAN 产物构建**

Run: `pio run -e waveshare_single_can_standalone`
Expected: `SUCCESS`（确认 `dash_epas_nag.h`、`epasNagActive`、胶水函数、HTTP/JSON 在产物里编译通过）。

- [ ] **Step 2: native 全量回归**

Run: `pio test -e native`
Expected: 所有 `test_native_*`（含新增 `test_native_epas_nag`、以及 `native_hw3`/`native_hw4`/`native_legacy`/`native_dashboard`/`native_single_can_dashboard`）全 PASS。
> 若 `native_hw3`/`native_hw4` 因 Task 3 的 bit19 门控出现断言失败：原因是已有测试未设置 `epasNagActive`（默认 false → 行为不变），**不应**失败。若失败，检查是否误改了 mux1 条件结构（应只追加 `&& !(bool)epasNagActive`，未动大括号/其它条件）。

- [ ] **Step 3: Python 契约全量回归**

Run:
```bash
PYTHONPATH=test python3 -m unittest test_epas_nag_filters_contract test_epas_nag_wiring_contract test_epas_nag_config_contract -v
PYTHONPATH=test python3 -m unittest test_dashboard_api_contract -v
```
Expected: 全 PASS（含既有 `test_dashboard_api_contract`，确保 `/status` JSON 结构未破坏）。

- [ ] **Step 4: UI 构建检查（gzip 存储）**

Run: `python3 scripts/minify_dashboard.py --check`（若脚本存在；否则按 memory 既有流程）
Expected: EXIT=0（UI 未改，仅后端；若有 UI 开关需后续另任务加，本计划不含 UI 改动）。

- [ ] **Step 5: 记录实车验证清单到 CHANGELOG / 提交**

在 `CHANGELOG.md` 追加一节 `## EPAS-faithful nag (experimental)`，列：
- 默认关闭；`/defense_config?epas_nag=1` 开启；`/status.epasNag` 反映状态。
- **头号实车验证项**：counter+1 回声竞速——台架先确认板子能稳定收/发 `0x370` 且回声能抢在真帧前。
- 首次接车 `epasNag` 保持关；确认 `/status` 正常后再手动开。
- 已知局限：HW4 Juniper（Bus6）部分支持。

```bash
git add CHANGELOG.md
git commit -m "docs(epas-nag): changelog + real-car validation checklist"
```

- [ ] **Step 6: 资产生成 + 烧录（待 Jordan 授权）**

按 memory 既有微雪单 CAN 流程：
```bash
# 干净重建（避免 version 标签缓存）
rm -rf .pio/build/waveshare_single_can_standalone
pio run -e waveshare_single_can_standalone --target build
# 资产打包到 firmware-assets/waveshare-single-can-standalone_<date>/
# 烧录（板 MAC 28:84:85:49:93:fc 或新板）：
# esptool.py --chip esp32s3 --port /dev/cu.usbmodemXXXX --baud 921600 erase_flash
# esptool.py ... write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB 0x0 full_merged.bin
```
**首次烧录后 `epasNag` 默认关；台架先验证 `/status.epasNag=false` 与板子能收 `0x370`，再决定是否手动开启 faithful。**

---

## Self-Review（写计划后自查）

**1. Spec 覆盖**：
- D1 faithful 引擎（不碰 handsOnLevel、均值回归扭矩）→ Task 1 `tryBuildEcho`（data[4] 透传、nextTorque）。✅
- D2 默认关闭 + 实验性开关 → `dashEpasNagEnabled=false`（Task 4）、HTTP `epas_nag`（Task 5）。✅
- D3 仅 nag 提示（隐含覆盖）→ 无 chime 逻辑（计划无 ISA chime 改动）。✅
- D4 统一可组合 NagEngine（dashboard 级，三 handler 转发）→ `DashEpasNagEngine` + `mcpDashOnFrame` 钩子。✅
- D5 未见 DAS 继续注入 → `shouldInject` 中 `0xFF` 不被 G5 拦截 + `test_gate_das_unseen_injects_d5`。✅
- D6 基线 `feature/ui-v11-redesign` → 计划头部声明，所有提交落当前分支。✅
- D7 过渡替代（faithful 开 → 跳过 bit19）→ `epasNagActive` + HW3/HW4 门控（Task 3/4）。✅
- 安全 G1-G6 → `shouldInject` 全覆盖 + 真值表测试。✅
- 测试四层（纯函数 native / 契约 python / 安全不变量 / 集成）→ Task 1 native 全量 + 三个契约测试；安全不变量（disabled/canactive/defense/blackout 等价"零发送"）由 `test_gate_*` 覆盖。✅

**2. 占位符扫描**：无 TBD/TODO/"适当处理"；每个 Step 有完整代码或确切命令。✅

**3. 类型/签名一致性**：
- `teslaChecksum0x370(const uint8_t d[7])` 在测试与实现一致。
- `tryBuildEcho(const uint8_t inData[8], uint32_t nowMs, uint8_t outData[8])` 实现/测试/胶水一致。
- `noteRuntime/onDasGate/noteApState/shouldInject` 签名跨 Task 1（定义）与 Task 4（胶水调用）一致。
- 全局名 `dashEpasNagEnabled`/`dashEpasNagCtrl`/`epasNagActive` 跨 Task 3/4/5 一致。
- NVS key `def_epnag` 跨 save/load 一致；HTTP arg `epas_nag`、JSON `epas_nag`/`epasNag` 跨 handler/json/status/契约一致。✅

**4. 已知风险/假设（实车前必读）**：
- 回声竞速依赖板子在 EPAS→DAS 通路（头号验证项）。
- `0x39B`(923) 仅 Highland/HW4 出现；legacy/HW3 靠 `0x399`(921)，引擎两者都听。
- HW4 Juniper Bus6 已知局限（spec §9）。
- 本计划**不含 UI 开关**（Dashboard 页面加 "EPAS 实验性" toggle）——HTTP/API 已就绪，UI 留作后续小任务或用 `/defense_config?epas_nag=1` 手动开启。

---

## Execution Handoff

计划已保存到 `docs/superpowers/plans/2026-06-17-epas-faithful-nag-unified.md`。
