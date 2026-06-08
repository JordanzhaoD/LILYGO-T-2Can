# Legacy CAN-A FSD Stability Diagnostic Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `v4.1.1 Legacy CAN-A FSD Stability Diagnostic`, so LILYGO T-2CAN Legacy FSD failures on CAN-A / X197 Pin 13/14 can be classified from `/status` instead of guessed.

**Architecture:** Keep the existing Legacy Stable behavior as default. Add native-friendly diagnostic data structures, expose TWAI controller health, expand Legacy 0x3EE mux diagnostics, add a `legacy_tesla_parity` A/B mode, and add watchdog classification without automatic reboot. Behavior changes are limited to TWAI RX queue sizing, honest TX result recording, and the optional parity mode.

**Tech Stack:** ESP32-S3 / ESP-IDF TWAI, PlatformIO, C++17, ESP WebServer Dashboard, Unity native tests, Python API contract tests.

---

## File Structure

### New files

- `include/dash_twai_diag.h`
  - Native-friendly TWAI diagnostic structs, enum-to-string helpers, and watchdog-safe default values.
  - Does not include ESP-IDF headers.

### Modified files

- `include/drivers/twai_driver.h`
  - Increase default RX queue to 128.
  - Track TWAI recovery count, filter counters, drain-budget hits, and controller status snapshots.
  - Update shared diagnostic snapshot from `twai_get_status_info()` under ESP builds.

- `include/dash_fsd_diag.h`
  - Add Legacy policy value `TeslaParity`.
  - Add gate reason, health state, CAN bus/driver labels, first-seen timestamps, and auxiliary TX diagnostics for 760/1080.

- `include/handlers.h`
  - Keep Legacy Stable default unchanged.
  - Add `legacy_tesla_parity` handling for mux0/mux1.
  - Record true send results for Legacy 760 and 1080.
  - Record first 0x3EE mux0/mux1 RX and first Legacy FSD TX OK/FAIL.

- `include/web/mcp2515_dashboard.h`
  - Append TWAI diagnostics to `/status`.
  - Append expanded `fsdDiag` gate/health fields.
  - Accept and persist `legacy_tesla_parity` in `/legacy_fsd_config`.
  - Add Dashboard UI labels for health state and parity mode.

- `include/web/mcp2515_dashboard_ui.src.h`
  - Add Chinese UI labels and controls for Legacy CAN-A health and `Tesla Controller 对齐模式`.

- `include/web/mcp2515_dashboard_ui.h`
  - Regenerate from source using `scripts/minify_dashboard.py`.

- `test/test_native_legacy/test_legacy_handler.cpp`
  - Add parity-mode behavior tests.
  - Add TX-failure truth tests for mux0/760/1080.
  - Add gate/health classification tests where possible.

- `test/test_native_twai/test_twai_filter.cpp`
  - Add unit tests for diagnostic constants and filter counters using extracted helper logic.

- `test/test_dashboard_api_contract.py`
  - Add textual/API contract tests for new `/status` fields, policy strings, UI labels, and generated UI header.

---

## Task 1: Add native-friendly TWAI diagnostic model

**Files:**
- Create: `include/dash_twai_diag.h`
- Test: `test/test_native_twai/test_twai_filter.cpp`

- [ ] **Step 1: Write failing tests for TWAI diagnostic defaults and state names**

Append these tests to `test/test_native_twai/test_twai_filter.cpp` after the existing `test_twai_filter_empty_count_returns_zero()` function:

```cpp
#include "dash_twai_diag.h"

void test_twai_diag_default_snapshot_is_safe()
{
    DashTwaiDiag d;
    TEST_ASSERT_EQUAL(DashTwaiState::Unknown, d.state);
    TEST_ASSERT_EQUAL_UINT32(0, d.txFailed);
    TEST_ASSERT_EQUAL_UINT32(0, d.rxMissed);
    TEST_ASSERT_EQUAL_UINT32(0, d.recoveries);
    TEST_ASSERT_EQUAL_STRING("unknown", dashTwaiStateName(d.state));
}

void test_twai_diag_state_names_are_stable()
{
    TEST_ASSERT_EQUAL_STRING("running", dashTwaiStateName(DashTwaiState::Running));
    TEST_ASSERT_EQUAL_STRING("bus_off", dashTwaiStateName(DashTwaiState::BusOff));
    TEST_ASSERT_EQUAL_STRING("recovering", dashTwaiStateName(DashTwaiState::Recovering));
    TEST_ASSERT_EQUAL_STRING("stopped", dashTwaiStateName(DashTwaiState::Stopped));
    TEST_ASSERT_EQUAL_STRING("unknown", dashTwaiStateName(DashTwaiState::Unknown));
}
```

Add them to `main()` before `return UNITY_END();`:

```cpp
    RUN_TEST(test_twai_diag_default_snapshot_is_safe);
    RUN_TEST(test_twai_diag_state_names_are_stable);
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```bash
pio test -e native_twai
```

Expected: FAIL because `dash_twai_diag.h`, `DashTwaiDiag`, and `dashTwaiStateName()` do not exist yet.

- [ ] **Step 3: Create `include/dash_twai_diag.h`**

Create the file with exactly this content:

```cpp
#pragma once

#include <cstdint>

enum class DashTwaiState : uint8_t
{
    Unknown = 0,
    Running = 1,
    BusOff = 2,
    Recovering = 3,
    Stopped = 4,
};

static inline const char *dashTwaiStateName(DashTwaiState state)
{
    switch (state)
    {
    case DashTwaiState::Running:
        return "running";
    case DashTwaiState::BusOff:
        return "bus_off";
    case DashTwaiState::Recovering:
        return "recovering";
    case DashTwaiState::Stopped:
        return "stopped";
    case DashTwaiState::Unknown:
        return "unknown";
    }
    return "unknown";
}

struct DashTwaiDiag
{
    DashTwaiState state = DashTwaiState::Unknown;
    uint32_t txErrorCounter = 0;
    uint32_t rxErrorCounter = 0;
    uint32_t msgsToTx = 0;
    uint32_t msgsToRx = 0;
    uint32_t txFailed = 0;
    uint32_t rxMissed = 0;
    uint32_t rxOverrun = 0;
    uint32_t arbLost = 0;
    uint32_t busError = 0;
    uint32_t recoveries = 0;
    uint32_t lastRecoveryMs = 0;
    uint32_t framesRead = 0;
    uint32_t framesAccepted = 0;
    uint32_t framesDropped = 0;
    uint32_t readDrainBudgetHits = 0;
};

static inline uint32_t dashAgeMs(uint32_t nowMs, uint32_t eventMs)
{
    return eventMs == 0 ? static_cast<uint32_t>(0xFFFFFFFFUL) : nowMs - eventMs;
}
```

- [ ] **Step 4: Run TWAI native test**

Run:

```bash
pio test -e native_twai
```

Expected: PASS.

- [ ] **Step 5: Commit Task 1**

```bash
git add include/dash_twai_diag.h test/test_native_twai/test_twai_filter.cpp
git commit -m "test: add TWAI diagnostic model"
```

---

## Task 2: Wire TWAI diagnostics into the LILYGO TWAI driver

**Files:**
- Modify: `include/drivers/twai_driver.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add contract test for TWAI queue and status fields**

Append this method to `DashboardApiContractTests` in `test/test_dashboard_api_contract.py`:

```python
    def test_twai_diagnostics_are_exposed_for_legacy_can_a(self) -> None:
        twai = (ROOT / "include" / "drivers" / "twai_driver.h").read_text(encoding="utf-8")
        diag = (ROOT / "include" / "dash_twai_diag.h").read_text(encoding="utf-8")
        for token in [
            '#define TWAI_RX_QUEUE_LEN 128',
            'DashTwaiDiag',
            'snapshotDiag()',
            'twai_get_status_info',
            'readDrainBudgetHits',
            'framesAccepted',
            'framesDropped',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, twai + diag)
        for token in [
            '"twai"',
            '"state"',
            '"tec"',
            '"rec"',
            '"txFailed"',
            '"rxMissed"',
            '"readDrainBudgetHits"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)
```

- [ ] **Step 2: Run contract test to verify it fails**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py -k twai_diagnostics -q
```

Expected: FAIL because TWAI diagnostics are not wired yet.

- [ ] **Step 3: Modify `include/drivers/twai_driver.h` includes and queue default**

At the top of `include/drivers/twai_driver.h`, after `#include "can_driver.h"`, add:

```cpp
#include "../dash_twai_diag.h"
```

Change:

```cpp
#ifndef TWAI_RX_QUEUE_LEN
#define TWAI_RX_QUEUE_LEN 32
#endif
```

to:

```cpp
#ifndef TWAI_RX_QUEUE_LEN
#define TWAI_RX_QUEUE_LEN 128
#endif
```

- [ ] **Step 4: Add diagnostic members and public accessor**

Inside class `TWAIDriver`, after `bool enableInterrupt(...) override { return false; }`, add:

```cpp
    DashTwaiDiag snapshotDiag()
    {
        lock();
        updateDiagLocked();
        DashTwaiDiag copy = diag_;
        unlock();
        return copy;
    }
```

Inside the private section, before `gpio_num_t txPin_;`, add:

```cpp
    DashTwaiDiag diag_;
```

- [ ] **Step 5: Update counters in `read()`**

In `read(CanFrame &frame)`, after a successful `twai_receive`, before `bool accepted = exactFilterMatchesLocked(msg.identifier);`, add:

```cpp
            diag_.framesRead++;
```

After `bool accepted = exactFilterMatchesLocked(msg.identifier);`, add:

```cpp
            if (accepted)
                diag_.framesAccepted++;
            else
                diag_.framesDropped++;
```

Before the final `return false;` at the end of the function, add:

```cpp
        lock();
        diag_.readDrainBudgetHits++;
        unlock();
```

- [ ] **Step 6: Update recovery counters**

Inside `recoverWithCooldown()`, after `lastRecovery_ = now;`, add:

```cpp
        diag_.recoveries++;
        diag_.lastRecoveryMs = now;
        diag_.state = DashTwaiState::Recovering;
```

Inside `tryRecover()`, after `lastRecovery_ = now;`, add the same three lines.

- [ ] **Step 7: Add `updateDiagLocked()` helper**

Add this private method before `bool exactFilterMatchesLocked(...) const`:

```cpp
    void updateDiagLocked()
    {
        if (!driverInstalled_)
        {
            diag_.state = DashTwaiState::Stopped;
            return;
        }
#ifndef NATIVE_BUILD
        twai_status_info_t status;
        if (twai_get_status_info(&status) != ESP_OK)
        {
            diag_.state = DashTwaiState::Unknown;
            return;
        }
        switch (status.state)
        {
        case TWAI_STATE_RUNNING:
            diag_.state = DashTwaiState::Running;
            break;
        case TWAI_STATE_BUS_OFF:
            diag_.state = DashTwaiState::BusOff;
            break;
        case TWAI_STATE_RECOVERING:
            diag_.state = DashTwaiState::Recovering;
            break;
        case TWAI_STATE_STOPPED:
            diag_.state = DashTwaiState::Stopped;
            break;
        default:
            diag_.state = DashTwaiState::Unknown;
            break;
        }
        diag_.txErrorCounter = status.tx_error_counter;
        diag_.rxErrorCounter = status.rx_error_counter;
        diag_.msgsToTx = status.msgs_to_tx;
        diag_.msgsToRx = status.msgs_to_rx;
        diag_.txFailed = status.tx_failed_count;
        diag_.rxMissed = status.rx_missed_count;
        diag_.rxOverrun = status.rx_overrun_count;
        diag_.arbLost = status.arb_lost_count;
        diag_.busError = status.bus_error_count;
#endif
    }
```

- [ ] **Step 8: Run focused contract test**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py -k twai_diagnostics -q
```

Expected: still FAIL until `/status` output is added in the next task. The driver-specific tokens should now pass.

- [ ] **Step 9: Do not commit yet**

Do not commit Task 2 alone. It requires Task 3 to expose the data through `/status` before it is useful.

---

## Task 3: Expose TWAI diagnostics through `/status`

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add include for TWAI diag model**

Near the other dashboard includes in `include/web/mcp2515_dashboard.h`, add:

```cpp
#include "dash_twai_diag.h"
```

- [ ] **Step 2: Add a dashboard helper to append TWAI JSON**

After `appendTxEvidenceJson(...)`, add:

```cpp
static void appendTwaiDiagJson(String &j, unsigned long now)
{
    DashTwaiDiag d{};
#if defined(ESP_PLATFORM) && !defined(NATIVE_BUILD)
    if (dashDriver)
    {
        // The active LILYGO CAN-A driver is TWAIDriver in this build. Keep this
        // helper narrow and diagnostic-only; other driver types keep defaults.
        TWAIDriver *twai = dynamic_cast<TWAIDriver *>(dashDriver);
        if (twai)
            d = twai->snapshotDiag();
    }
#endif
    j += R"JSON(,"twai":{"state":")JSON";
    j += dashTwaiStateName(d.state);
    j += R"JSON(","tec":)JSON";
    j += d.txErrorCounter;
    j += R"JSON(,"rec":)JSON";
    j += d.rxErrorCounter;
    j += R"JSON(,"msgsToTx":)JSON";
    j += d.msgsToTx;
    j += R"JSON(,"msgsToRx":)JSON";
    j += d.msgsToRx;
    j += R"JSON(,"txFailed":)JSON";
    j += d.txFailed;
    j += R"JSON(,"rxMissed":)JSON";
    j += d.rxMissed;
    j += R"JSON(,"rxOverrun":)JSON";
    j += d.rxOverrun;
    j += R"JSON(,"arbLost":)JSON";
    j += d.arbLost;
    j += R"JSON(,"busError":)JSON";
    j += d.busError;
    j += R"JSON(,"recoveries":)JSON";
    j += d.recoveries;
    j += R"JSON(,"lastRecoveryAgeMs":)JSON";
    j += d.lastRecoveryMs ? String(now - d.lastRecoveryMs) : String(-1);
    j += R"JSON(,"framesRead":)JSON";
    j += d.framesRead;
    j += R"JSON(,"framesAccepted":)JSON";
    j += d.framesAccepted;
    j += R"JSON(,"framesDropped":)JSON";
    j += d.framesDropped;
    j += R"JSON(,"readDrainBudgetHits":)JSON";
    j += d.readDrainBudgetHits;
    j += "}";
}
```

If `dynamic_cast` causes compile trouble because RTTI is disabled, replace the cast with a narrow build-specific global accessor in `twai_driver.h` and call that accessor instead. Keep the JSON field names unchanged.

- [ ] **Step 3: Call helper in `handleStatus()`**

In `handleStatus()`, after the existing CAN stats are appended and before `appendFsdDiagJson(j, now);`, add:

```cpp
    appendTwaiDiagJson(j, now);
```

- [ ] **Step 4: Run focused contract test**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py -k twai_diagnostics -q
```

Expected: PASS.

- [ ] **Step 5: Run build check for dashboard native environment**

Run:

```bash
pio test -e native_dashboard
```

Expected: PASS. If it fails because `TWAIDriver` is unknown in native dashboard, guard the dynamic cast with `#if defined(ESP_PLATFORM) && !defined(NATIVE_BUILD)` exactly as shown.

- [ ] **Step 6: Commit Tasks 2-3**

```bash
git add include/drivers/twai_driver.h include/web/mcp2515_dashboard.h test/test_dashboard_api_contract.py
git commit -m "feat: expose TWAI diagnostics for Legacy CAN-A"
```

---

## Task 4: Expand Legacy FSD diag with gate reason, health, bus, and first-seen timing

**Files:**
- Modify: `include/dash_fsd_diag.h`
- Modify: `include/handlers.h`
- Modify: `include/web/mcp2515_dashboard.h`
- Test: `test/test_native_legacy/test_legacy_handler.cpp`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add native tests for first-seen and bus/driver labels**

Append these tests to `test/test_native_legacy/test_legacy_handler.cpp` before `int main()`:

```cpp
void test_legacy_mux0_records_can_a_twai_labels()
{
    CanFrame f = {.id = 1006};
    f.bus = CAN_BUS_VEH;
    f.data[0] = 0x00;
    f.data[4] = 0x40;

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(FsdDiagBus::CanA, handler.legacyFsdDiag.mux0.bus);
    TEST_ASSERT_EQUAL(FsdDiagDriver::Twai, handler.legacyFsdDiag.mux0.driver);
    TEST_ASSERT_NOT_EQUAL(0, handler.legacyFsdDiag.firstMux0RxMs);
    TEST_ASSERT_NOT_EQUAL(0, handler.legacyFsdDiag.firstTxOkMs);
}

void test_legacy_gate_block_records_gate_blocked_health()
{
    handler.checkAD = denyAD;
    CanFrame f = {.id = 1006};
    f.bus = CAN_BUS_VEH;
    f.data[0] = 0x00;
    f.data[4] = 0x40;

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(FsdSkipReason::GateBlocked, handler.legacyFsdDiag.mux0.lastSkip);
    TEST_ASSERT_EQUAL(FsdHealthState::GateBlocked, handler.legacyFsdDiag.health);
}
```

Add them to `main()` after the existing Legacy mux tests:

```cpp
    RUN_TEST(test_legacy_mux0_records_can_a_twai_labels);
    RUN_TEST(test_legacy_gate_block_records_gate_blocked_health);
```

- [ ] **Step 2: Add contract test for expanded `fsdDiag` JSON**

Append this method to `DashboardApiContractTests`:

```python
    def test_legacy_can_a_fsd_diag_expanded_fields_are_exposed(self) -> None:
        for token in [
            'FsdHealthState',
            'FsdGateBlockReason',
            'FsdDiagBus',
            'FsdDiagDriver',
            'fsdHealthStateName',
            'fsdGateBlockReasonName',
            'firstMux0RxMs',
            'firstTxOkMs',
            'firstTxFailMs',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.handlers + (ROOT / "include" / "dash_fsd_diag.h").read_text(encoding="utf-8"))
        for token in [
            '"health"',
            '"gateReason"',
            '"lastBlockedBy"',
            '"bus"',
            '"driver"',
            '"firstMux0RxAgeMs"',
            '"firstTxOkAgeMs"',
            '"firstTxFailAgeMs"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.dash)
```

- [ ] **Step 3: Run tests to verify failure**

Run:

```bash
pio test -e native_legacy
python3 -m pytest test/test_dashboard_api_contract.py -k legacy_can_a_fsd_diag_expanded -q
```

Expected: both FAIL because the new diag fields do not exist.

- [ ] **Step 4: Extend `include/dash_fsd_diag.h` enums**

Add these enums after `FsdSkipReason`:

```cpp
enum class FsdGateBlockReason : uint8_t
{
    None = 0,
    CheckAd = 1,
    CanActive = 2,
    Ota = 3,
    ApGate = 4,
    CompileGate = 5,
};

enum class FsdHealthState : uint8_t
{
    Unknown = 0,
    Healthy = 1,
    NoSourceFrame = 2,
    NotTriggered = 3,
    GateBlocked = 4,
    TxDegraded = 5,
    TxOkVehicleUnconfirmed = 6,
};

enum class FsdDiagBus : uint8_t
{
    Unknown = 0,
    CanA = 1,
    CanB = 2,
};

enum class FsdDiagDriver : uint8_t
{
    Unknown = 0,
    Twai = 1,
    Mcp2515 = 2,
};
```

Add string helpers:

```cpp
static inline const char *fsdGateBlockReasonName(FsdGateBlockReason reason)
{
    switch (reason)
    {
    case FsdGateBlockReason::None: return "none";
    case FsdGateBlockReason::CheckAd: return "check_ad";
    case FsdGateBlockReason::CanActive: return "can_active";
    case FsdGateBlockReason::Ota: return "ota";
    case FsdGateBlockReason::ApGate: return "ap_gate";
    case FsdGateBlockReason::CompileGate: return "compile_gate";
    }
    return "none";
}

static inline const char *fsdHealthStateName(FsdHealthState state)
{
    switch (state)
    {
    case FsdHealthState::Healthy: return "healthy";
    case FsdHealthState::NoSourceFrame: return "no_source_frame";
    case FsdHealthState::NotTriggered: return "not_triggered";
    case FsdHealthState::GateBlocked: return "gate_blocked";
    case FsdHealthState::TxDegraded: return "tx_degraded";
    case FsdHealthState::TxOkVehicleUnconfirmed: return "tx_ok_vehicle_unconfirmed";
    case FsdHealthState::Unknown: return "unknown";
    }
    return "unknown";
}

static inline const char *fsdDiagBusName(FsdDiagBus bus)
{
    switch (bus)
    {
    case FsdDiagBus::CanA: return "can_a";
    case FsdDiagBus::CanB: return "can_b";
    case FsdDiagBus::Unknown: return "unknown";
    }
    return "unknown";
}

static inline const char *fsdDiagDriverName(FsdDiagDriver driver)
{
    switch (driver)
    {
    case FsdDiagDriver::Twai: return "twai";
    case FsdDiagDriver::Mcp2515: return "mcp2515";
    case FsdDiagDriver::Unknown: return "unknown";
    }
    return "unknown";
}
```

Update `FsdSkipReason` by replacing `CheckAdFalse = 5` with:

```cpp
    GateBlocked = 5,
```

Update `fsdSkipReasonName()` so case 5 returns `"gate_blocked"`.

- [ ] **Step 5: Extend `FsdMuxDiag` and `LegacyFsdDiag`**

In `FsdMuxDiag`, add fields:

```cpp
    FsdDiagBus bus = FsdDiagBus::Unknown;
    FsdDiagDriver driver = FsdDiagDriver::Unknown;
```

Add method:

```cpp
    void recordPath(FsdDiagBus frameBus, FsdDiagDriver frameDriver)
    {
        bus = frameBus;
        driver = frameDriver;
    }
```

In `LegacyFsdDiag`, add:

```cpp
    FsdHealthState health = FsdHealthState::Unknown;
    FsdGateBlockReason lastBlockedBy = FsdGateBlockReason::None;
    uint32_t firstMux0RxMs = 0;
    uint32_t firstMux1RxMs = 0;
    uint32_t firstTxOkMs = 0;
    uint32_t firstTxFailMs = 0;
    FsdMuxDiag aux760;
    FsdMuxDiag aux1080;
```

Add helper methods inside `LegacyFsdDiag`:

```cpp
    void markMux0Rx(uint32_t nowMs)
    {
        if (firstMux0RxMs == 0) firstMux0RxMs = nowMs;
    }

    void markMux1Rx(uint32_t nowMs)
    {
        if (firstMux1RxMs == 0) firstMux1RxMs = nowMs;
    }

    void markTxResult(bool ok, uint32_t nowMs)
    {
        if (ok)
        {
            if (firstTxOkMs == 0) firstTxOkMs = nowMs;
            health = FsdHealthState::Healthy;
            lastBlockedBy = FsdGateBlockReason::None;
        }
        else
        {
            if (firstTxFailMs == 0) firstTxFailMs = nowMs;
            health = FsdHealthState::TxDegraded;
        }
    }
```

- [ ] **Step 6: Update Legacy handler to record path and health**

In `LegacyHandler::handleMessage()`:

For mux0 after `legacyFsdDiag.mux0.recordRx(nowMs);`, add:

```cpp
                legacyFsdDiag.markMux0Rx(nowMs);
                legacyFsdDiag.mux0.recordPath(FsdDiagBus::CanA, FsdDiagDriver::Twai);
```

When not triggered, add:

```cpp
                    legacyFsdDiag.health = FsdHealthState::NotTriggered;
```

Replace mux0 gate-block branch:

```cpp
                    legacyFsdDiag.mux0.lastSkip = FsdSkipReason::CheckAdFalse;
```

with:

```cpp
                    legacyFsdDiag.mux0.lastSkip = FsdSkipReason::GateBlocked;
                    legacyFsdDiag.health = FsdHealthState::GateBlocked;
                    legacyFsdDiag.lastBlockedBy = FsdGateBlockReason::CheckAd;
```

After `legacyFsdDiag.mux0.recordTx(ok, nowMs);`, add:

```cpp
                legacyFsdDiag.markTxResult(ok, nowMs);
```

For mux1 after `legacyFsdDiag.mux1.recordRx(nowMs);`, add:

```cpp
                legacyFsdDiag.markMux1Rx(nowMs);
                legacyFsdDiag.mux1.recordPath(FsdDiagBus::CanA, FsdDiagDriver::Twai);
```

Replace mux1 gate-block skip in the same way as mux0.

After `legacyFsdDiag.mux1.recordTx(ok, nowMs);`, add:

```cpp
                legacyFsdDiag.markTxResult(ok, nowMs);
```

- [ ] **Step 7: Expand Dashboard JSON**

In `appendFsdMuxDiagJson(...)`, after `"lastSkip"`, append bus and driver:

```cpp
    j += R"JSON(","bus":")JSON";
    j += fsdDiagBusName(mux.bus);
    j += R"JSON(","driver":")JSON";
    j += fsdDiagDriverName(mux.driver);
```

In `appendFsdDiagJson(...)`, after `forceRuntime`, append:

```cpp
    j += R"JSON(,"health":")JSON";
    j += fsdHealthStateName(diag.health);
    j += R"JSON(","firstMux0RxAgeMs":)JSON";
    j += diag.firstMux0RxMs ? String(now - diag.firstMux0RxMs) : String(-1);
    j += R"JSON(,"firstMux1RxAgeMs":)JSON";
    j += diag.firstMux1RxMs ? String(now - diag.firstMux1RxMs) : String(-1);
    j += R"JSON(,"firstTxOkAgeMs":)JSON";
    j += diag.firstTxOkMs ? String(now - diag.firstTxOkMs) : String(-1);
    j += R"JSON(,"firstTxFailAgeMs":)JSON";
    j += diag.firstTxFailMs ? String(now - diag.firstTxFailMs) : String(-1);
```

Inside the existing `"gate"` object, append:

```cpp
    j += R"JSON(,"finalAllowed":)JSON";
    j += (canActive && dashOtaGuardAllowInjection() && dashApInjectionAllowed()) ? "true" : "false";
    j += R"JSON(,"lastBlockedBy":")JSON";
    j += fsdGateBlockReasonName(diag.lastBlockedBy);
    j += "\"";
```

- [ ] **Step 8: Run tests**

Run:

```bash
pio test -e native_legacy
python3 -m pytest test/test_dashboard_api_contract.py -k legacy_can_a_fsd_diag_expanded -q
```

Expected: PASS.

- [ ] **Step 9: Commit Task 4**

```bash
git add include/dash_fsd_diag.h include/handlers.h include/web/mcp2515_dashboard.h test/test_native_legacy/test_legacy_handler.cpp test/test_dashboard_api_contract.py
git commit -m "feat: expand Legacy CAN-A FSD diagnostics"
```

---

## Task 5: Add `legacy_tesla_parity` policy for controlled A/B testing

**Files:**
- Modify: `include/dash_fsd_diag.h`
- Modify: `include/handlers.h`
- Modify: `include/web/mcp2515_dashboard.h`
- Test: `test/test_native_legacy/test_legacy_handler.cpp`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add native tests for parity behavior**

Append these tests to `test/test_native_legacy/test_legacy_handler.cpp` before `int main()`:

```cpp
void test_legacy_tesla_parity_mux0_writes_speed_profile()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::TeslaParity;
    handler.speedProfile = 2;

    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    f.data[6] = 0x00;

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40);
    TEST_ASSERT_EQUAL_HEX8(0x04, mock.sent[0].data[6] & 0x06);
}

void test_legacy_tesla_parity_mux1_clears_bit19()
{
    handler.legacyFsdDiag.policy = LegacyFsdPolicy::TeslaParity;

    CanFrame f0 = {.id = 1006};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);

    CanFrame f1 = {.id = 1006};
    f1.data[0] = 0x01;
    setBit(f1, 19, true);
    handler.handleMessage(f1, mock);

    TEST_ASSERT_EQUAL(2, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[1].data[2] >> 3) & 0x01);
    TEST_ASSERT_EQUAL(FsdSkipReason::Sent, handler.legacyFsdDiag.mux1.lastSkip);
}
```

Add to `main()`:

```cpp
    RUN_TEST(test_legacy_tesla_parity_mux0_writes_speed_profile);
    RUN_TEST(test_legacy_tesla_parity_mux1_clears_bit19);
```

- [ ] **Step 2: Add contract tests for policy string and persistence**

Append to `DashboardApiContractTests`:

```python
    def test_legacy_tesla_parity_policy_is_wired(self) -> None:
        diag = (ROOT / "include" / "dash_fsd_diag.h").read_text(encoding="utf-8")
        for token in [
            'TeslaParity',
            '"legacy_tesla_parity"',
            'p == "legacy_tesla_parity"',
            'prefs.putUChar("lfsd_pol"',
            'prefs.getUChar("lfsd_pol"',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, diag + self.dash)
        self.assertIn('Tesla Controller 对齐模式', self.ui)
```

- [ ] **Step 3: Run tests to verify failure**

Run:

```bash
pio test -e native_legacy
python3 -m pytest test/test_dashboard_api_contract.py -k legacy_tesla_parity -q
```

Expected: FAIL because the parity policy does not exist.

- [ ] **Step 4: Extend `LegacyFsdPolicy`**

In `include/dash_fsd_diag.h`, change enum to:

```cpp
enum class LegacyFsdPolicy : uint8_t
{
    Stable = 0,
    Experimental = 1,
    TeslaParity = 2,
};
```

Update `legacyFsdPolicyName()`:

```cpp
    case LegacyFsdPolicy::TeslaParity:
        return "legacy_tesla_parity";
```

- [ ] **Step 5: Implement parity mux0/mux1 behavior**

In `LegacyHandler` mux0 block, replace:

```cpp
                if (legacyFsdDiag.policy == LegacyFsdPolicy::Experimental && legacyFsdDiag.profileWriteEnable)
                    setSpeedProfileV12V13(frame, speedProfile);
```

with:

```cpp
                if (legacyFsdDiag.policy == LegacyFsdPolicy::TeslaParity ||
                    (legacyFsdDiag.policy == LegacyFsdPolicy::Experimental && legacyFsdDiag.profileWriteEnable))
                    setSpeedProfileV12V13(frame, speedProfile);
```

In mux1 block, replace Stable-only disable:

```cpp
                if (legacyFsdDiag.policy == LegacyFsdPolicy::Stable)
```

with:

```cpp
                if (legacyFsdDiag.policy == LegacyFsdPolicy::Stable)
```

Keep this condition unchanged, then replace the mux1 enable check:

```cpp
                if (!legacyFsdDiag.mux1Enable)
```

with:

```cpp
                if (legacyFsdDiag.policy != LegacyFsdPolicy::TeslaParity && !legacyFsdDiag.mux1Enable)
```

Replace the vision-limit clear branch:

```cpp
                if (legacyFsdDiag.visionLimitClearEnable)
                    setBit(frame, 48, false);
```

with:

```cpp
                if (legacyFsdDiag.visionLimitClearEnable)
                    setBit(frame, 48, false);
```

This line stays as-is: parity clears bit19 but only clears bit48 when the existing `removeVisionSpeedLimit`/vision flag is enabled through policy config.

- [ ] **Step 6: Update dashboard persistence**

In `dashSavePrefs()`, replace:

```cpp
    prefs.putUChar("lfsd_pol", dashLegacyFsdPolicy == LegacyFsdPolicy::Experimental ? 1 : 0);
```

with:

```cpp
    prefs.putUChar("lfsd_pol", dashLegacyFsdPolicy == LegacyFsdPolicy::TeslaParity ? 2 : (dashLegacyFsdPolicy == LegacyFsdPolicy::Experimental ? 1 : 0));
```

In `dashLoadPrefs()`, replace:

```cpp
    dashLegacyFsdPolicy = prefs.getUChar("lfsd_pol", 0) == 1 ? LegacyFsdPolicy::Experimental : LegacyFsdPolicy::Stable;
```

with:

```cpp
    {
        uint8_t p = prefs.getUChar("lfsd_pol", 0);
        dashLegacyFsdPolicy = p == 2 ? LegacyFsdPolicy::TeslaParity : (p == 1 ? LegacyFsdPolicy::Experimental : LegacyFsdPolicy::Stable);
    }
```

- [ ] **Step 7: Update `/legacy_fsd_config` parser**

In `handleLegacyFsdConfig()`, before experimental check, add:

```cpp
            if (p == "legacy_tesla_parity" || p == "tesla_parity" || p == "2")
                dashLegacyFsdPolicy = LegacyFsdPolicy::TeslaParity;
            else if (p == "experimental" || p == "legacy_experimental" || p == "1")
                dashLegacyFsdPolicy = LegacyFsdPolicy::Experimental;
```

Remove the old first `if (p == "experimental"...)` branch to avoid duplicate logic.

- [ ] **Step 8: Add UI label**

In `include/web/mcp2515_dashboard_ui.src.h`, find the Legacy FSD strategy card and add a selectable option whose visible label is:

```text
Tesla Controller 对齐模式
```

The option must submit:

```text
policy=legacy_tesla_parity
```

Use existing `postForm('/legacy_fsd_config', ...)` style from the current card. Do not create a new API route.

- [ ] **Step 9: Regenerate dashboard UI**

Run:

```bash
python3 scripts/minify_dashboard.py
python3 scripts/minify_dashboard.py --check
```

Expected: `--check` PASS and `include/web/mcp2515_dashboard_ui.h` updated.

- [ ] **Step 10: Run tests**

Run:

```bash
pio test -e native_legacy
python3 -m pytest test/test_dashboard_api_contract.py -k legacy_tesla_parity -q
```

Expected: PASS.

- [ ] **Step 11: Commit Task 5**

```bash
git add include/dash_fsd_diag.h include/handlers.h include/web/mcp2515_dashboard.h include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h test/test_native_legacy/test_legacy_handler.cpp test/test_dashboard_api_contract.py
git commit -m "feat: add Legacy Tesla parity diagnostic mode"
```

---

## Task 6: Make Legacy 760 and 1080 TX evidence honest

**Files:**
- Modify: `include/handlers.h`
- Modify: `include/web/mcp2515_dashboard.h`
- Modify: `test/test_native_legacy/test_legacy_handler.cpp`

- [ ] **Step 1: Extend `MockDriver` to simulate send failure**

Modify `test/drivers/mock_driver.h` by adding a field inside `MockDriver`:

```cpp
    bool sendOk = true;
```

Change `send()` to:

```cpp
    bool send(const CanFrame &frame) override
    {
        sent.push_back(frame);
        if (onSendFrame)
            onSendFrame(frame, sendOk);
        return sendOk;
    }
```

Change `reset()` to:

```cpp
    void reset()
    {
        sent.clear();
        sendOk = true;
    }
```

- [ ] **Step 2: Add failing tests for 760/1080 send failure**

Append to `test/test_native_legacy/test_legacy_handler.cpp` before `int main()`:

```cpp
void test_legacy_can760_records_tx_fail()
{
    handler.legacyOffset = 10;
    mock.sendOk = false;

    CanFrame f = {.id = 760};
    f.dlc = 8;
    f.data[5] = 0xC0;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyFsdDiag.aux760.err);
    TEST_ASSERT_EQUAL(FsdSkipReason::TxFail, handler.legacyFsdDiag.aux760.lastSkip);
}

void test_legacy_can1080_records_tx_fail()
{
    handler.overrideSpeedLimit = true;
    mock.sendOk = false;

    CanFrame f = {.id = 1080};
    f.dlc = 8;
    f.data[7] = 0x80;
    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(1, handler.legacyFsdDiag.aux1080.err);
    TEST_ASSERT_EQUAL(FsdSkipReason::TxFail, handler.legacyFsdDiag.aux1080.lastSkip);
}
```

Add to `main()`:

```cpp
    RUN_TEST(test_legacy_can760_records_tx_fail);
    RUN_TEST(test_legacy_can1080_records_tx_fail);
```

- [ ] **Step 3: Run test to verify failure**

Run:

```bash
pio test -e native_legacy
```

Expected: FAIL because 760/1080 aux diagnostics are not recorded yet.

- [ ] **Step 4: Update Legacy 760 send path**

In `LegacyHandler` 760 block, replace:

```cpp
            framesSent++;
            driver.send(frame);
            if (onSend) onSend(0, true);
```

with:

```cpp
            legacyFsdDiag.aux760.recordBefore(frame.data);
            legacyFsdDiag.aux760.recordAfter(frame.data);
            legacyFsdDiag.aux760.recordPath(FsdDiagBus::CanA, FsdDiagDriver::Twai);
            framesSent++;
            bool ok = driver.send(frame);
            legacyFsdDiag.aux760.recordTx(ok, dashDiagNowMs());
            if (onSend) onSend(0, ok);
```

- [ ] **Step 5: Update Legacy 1080 send path**

In `LegacyHandler` 1080 block, replace:

```cpp
            framesSent++;
            driver.send(frame);
            if (onSend) onSend(0, true);
```

with:

```cpp
            legacyFsdDiag.aux1080.recordBefore(frame.data);
            legacyFsdDiag.aux1080.recordAfter(frame.data);
            legacyFsdDiag.aux1080.recordPath(FsdDiagBus::CanA, FsdDiagDriver::Twai);
            framesSent++;
            bool ok = driver.send(frame);
            legacyFsdDiag.aux1080.recordTx(ok, dashDiagNowMs());
            if (onSend) onSend(0, ok);
```

- [ ] **Step 6: Expose aux diagnostics in `/status`**

In `appendFsdDiagJson(...)`, after mux1 JSON, add:

```cpp
    appendFsdMuxDiagJson(j, "aux760", diag.aux760, true, now);
    appendFsdMuxDiagJson(j, "aux1080", diag.aux1080, true, now);
```

- [ ] **Step 7: Add contract tokens**

In `test_legacy_can_a_fsd_diag_expanded_fields_are_exposed`, add tokens:

```python
            '"aux760"',
            '"aux1080"',
```

- [ ] **Step 8: Run tests**

Run:

```bash
pio test -e native_legacy
python3 -m pytest test/test_dashboard_api_contract.py -k legacy_can_a_fsd_diag_expanded -q
```

Expected: PASS.

- [ ] **Step 9: Commit Task 6**

```bash
git add test/drivers/mock_driver.h include/handlers.h include/web/mcp2515_dashboard.h test/test_native_legacy/test_legacy_handler.cpp test/test_dashboard_api_contract.py
git commit -m "fix: record Legacy auxiliary TX failures"
```

---

## Task 7: Add Legacy watchdog health classification

**Files:**
- Modify: `include/dash_fsd_diag.h`
- Modify: `include/web/mcp2515_dashboard.h`
- Test: `test/test_native_legacy/test_legacy_handler.cpp`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add native tests for health classifier helper**

Append to `test/test_native_legacy/test_legacy_handler.cpp` before `int main()`:

```cpp
void test_legacy_health_no_source_when_mux0_stale()
{
    LegacyFsdDiag d;
    d.mux0.lastRxMs = 1000;
    FsdHealthState s = classifyLegacyFsdHealth(d, 7000, true);
    TEST_ASSERT_EQUAL(FsdHealthState::NoSourceFrame, s);
}

void test_legacy_health_tx_degraded_when_errors_increase()
{
    LegacyFsdDiag d;
    d.mux0.lastRxMs = 6500;
    d.mux0.err = 1;
    FsdHealthState s = classifyLegacyFsdHealth(d, 7000, true);
    TEST_ASSERT_EQUAL(FsdHealthState::TxDegraded, s);
}

void test_legacy_health_gate_blocked_when_last_blocked()
{
    LegacyFsdDiag d;
    d.mux0.lastRxMs = 6500;
    d.health = FsdHealthState::GateBlocked;
    FsdHealthState s = classifyLegacyFsdHealth(d, 7000, true);
    TEST_ASSERT_EQUAL(FsdHealthState::GateBlocked, s);
}
```

Add to `main()`:

```cpp
    RUN_TEST(test_legacy_health_no_source_when_mux0_stale);
    RUN_TEST(test_legacy_health_tx_degraded_when_errors_increase);
    RUN_TEST(test_legacy_health_gate_blocked_when_last_blocked);
```

- [ ] **Step 2: Run native test to verify failure**

Run:

```bash
pio test -e native_legacy
```

Expected: FAIL because `classifyLegacyFsdHealth()` does not exist.

- [ ] **Step 3: Add classifier helper to `dash_fsd_diag.h`**

Add after `dashDiagNowMs()`:

```cpp
static constexpr uint32_t kLegacyFsdSourceStaleMs = 5000;

static inline FsdHealthState classifyLegacyFsdHealth(const LegacyFsdDiag &diag, uint32_t nowMs, bool enabled)
{
    if (!enabled)
        return FsdHealthState::Unknown;
    if (diag.mux0.lastRxMs == 0 || nowMs - diag.mux0.lastRxMs > kLegacyFsdSourceStaleMs)
        return FsdHealthState::NoSourceFrame;
    if (diag.mux0.err > 0 || diag.mux1.err > 0 || diag.aux760.err > 0 || diag.aux1080.err > 0)
        return FsdHealthState::TxDegraded;
    if (diag.health == FsdHealthState::GateBlocked)
        return FsdHealthState::GateBlocked;
    if (diag.health == FsdHealthState::NotTriggered)
        return FsdHealthState::NotTriggered;
    if (diag.mux0.tx > 0)
        return FsdHealthState::Healthy;
    return diag.health;
}
```

- [ ] **Step 4: Use classifier in `/status`**

In `appendFsdDiagJson(...)`, before appending `"health"`, compute:

```cpp
    FsdHealthState health = classifyLegacyFsdHealth(diag, now, canActive);
```

Then output `health` instead of `diag.health`:

```cpp
    j += fsdHealthStateName(health);
```

- [ ] **Step 5: Add Dashboard contract tokens**

In the existing expanded diag contract test, add:

```python
            'classifyLegacyFsdHealth',
            'kLegacyFsdSourceStaleMs',
```

- [ ] **Step 6: Run tests**

Run:

```bash
pio test -e native_legacy
python3 -m pytest test/test_dashboard_api_contract.py -k legacy_can_a_fsd_diag_expanded -q
```

Expected: PASS.

- [ ] **Step 7: Commit Task 7**

```bash
git add include/dash_fsd_diag.h include/web/mcp2515_dashboard.h test/test_native_legacy/test_legacy_handler.cpp test/test_dashboard_api_contract.py
git commit -m "feat: classify Legacy FSD diagnostic health"
```

---

## Task 8: Add Dashboard UI labels for Legacy CAN-A health

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`
- Modify: `include/web/mcp2515_dashboard_ui.h`
- Test: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: Add UI contract test**

Append to `DashboardApiContractTests`:

```python
    def test_legacy_can_a_health_ui_labels_are_present(self) -> None:
        for token in [
            'Legacy CAN-A 诊断',
            '0x3EE mux0',
            '0x3EE mux1',
            'TWAI 状态',
            'TEC/REC',
            'TX失败',
            'RX丢失',
            '健康状态',
            '最后阻止原因',
            'Tesla Controller 对齐模式',
        ]:
            with self.subTest(token=token):
                self.assertIn(token, self.ui)
                self.assertIn(token, self.ui_gen)
```

- [ ] **Step 2: Run test to verify failure**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py -k legacy_can_a_health_ui_labels -q
```

Expected: FAIL because UI labels are not present.

- [ ] **Step 3: Update UI source**

In `include/web/mcp2515_dashboard_ui.src.h`, find the module/overview Legacy diagnostic card. Add a compact card section that reads these `/status` paths:

```javascript
const fd=d.fsdDiag||{};
const tw=d.twai||{};
```

The visible labels must include exactly:

```text
Legacy CAN-A 诊断
0x3EE mux0
0x3EE mux1
TWAI 状态
TEC/REC
TX失败
RX丢失
健康状态
最后阻止原因
Tesla Controller 对齐模式
```

Use existing `card`, `stats`, `setting-row`, and `tgl` styles. Do not introduce a new CSS system.

- [ ] **Step 4: Regenerate UI**

Run:

```bash
python3 scripts/minify_dashboard.py
python3 scripts/minify_dashboard.py --check
```

Expected: PASS.

- [ ] **Step 5: Run UI contract test**

Run:

```bash
python3 -m pytest test/test_dashboard_api_contract.py -k legacy_can_a_health_ui_labels -q
```

Expected: PASS.

- [ ] **Step 6: Commit Task 8**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h test/test_dashboard_api_contract.py
git commit -m "feat: show Legacy CAN-A diagnostic health in dashboard"
```

---

## Task 9: Full verification and diagnostic release readiness

**Files:**
- Modify only if previous tests expose issues.
- No new code expected.

- [ ] **Step 1: Run dashboard minify check**

```bash
python3 scripts/minify_dashboard.py --check
```

Expected: PASS.

- [ ] **Step 2: Run Python API contract tests**

```bash
python3 -m pytest test/test_dashboard_api_contract.py
```

Expected: PASS.

- [ ] **Step 3: Run native Legacy tests**

```bash
pio test -e native_legacy
```

Expected: PASS.

- [ ] **Step 4: Run native TWAI tests**

```bash
pio test -e native_twai
```

Expected: PASS.

- [ ] **Step 5: Run native dashboard tests**

```bash
pio test -e native_dashboard
```

Expected: PASS.

- [ ] **Step 6: Build LILYGO firmware**

```bash
pio run -e lilygo_t2can_dual
```

Expected: SUCCESS.

- [ ] **Step 7: Inspect working tree**

```bash
git status --short
```

Expected: only intended files changed. No `.pio/`, generated temporary files, or accidental logs staged.

- [ ] **Step 8: Commit verification adjustments if needed**

If Step 1-7 required code fixes, commit them:

```bash
git add <fixed-files>
git commit -m "fix: complete Legacy CAN-A diagnostic verification"
```

If no fixes were needed, do not create an empty commit.

---

## Self-Review

### Spec coverage

- TWAI diagnostic visibility: Task 1, Task 2, Task 3.
- TWAI RX queue alignment: Task 2.
- Filter/read counters: Task 2.
- Legacy gate/failure reason expansion: Task 4.
- Legacy 760/1080 TX truth: Task 6.
- Boot timing and first TX timestamps: Task 4. Full 1 Hz persistent log migration is intentionally represented through `/status` first; add serial/SPIFFS log lines only if verification shows the status data is insufficient for field testing.
- `legacy_tesla_parity` A/B mode: Task 5.
- Watchdog health classification: Task 7.
- Dashboard UI labels: Task 8.
- Verification: Task 9.

### Placeholder scan

This plan contains no unresolved placeholders or unspecified implementation steps. Every code-changing task includes target files, code snippets, commands, and expected results.

### Type consistency

The plan consistently uses:

- `DashTwaiDiag`
- `DashTwaiState`
- `LegacyFsdPolicy::TeslaParity`
- `FsdHealthState`
- `FsdGateBlockReason`
- `FsdDiagBus`
- `FsdDiagDriver`
- `classifyLegacyFsdHealth()`

These names are introduced before later tasks use them.
