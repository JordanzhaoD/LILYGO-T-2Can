# FSD Activation Logic Migration Design

**Date:** 2026-05-28
**Status:** Draft
**Scope:** Migrate proven FSD activation logic from tesla-fsd-controller into LILYGO T-2CAN firmware handlers

## Context

The LILYGO T-2CAN firmware (`LILYGO-T-2Can-firmware/`) has three CAN handlers (Legacy, HW3, HW4) for FSD activation, but these have **not been field-verified**. The `tesla-fsd-controller/` project contains proven, field-tested activation logic. This spec covers migrating that verified logic into T-2CAN's handler architecture.

**Approach:** Handler-internal rewrite (Approach A). Preserve T-2CAN architecture (CarManagerBase, Shared\<T\>, Dashboard, dual CAN), replace activation frame processing with verified logic.

## Source of Truth

- **Verified project:** `/Users/ziwind/my-vibe-project/tesla-fsd-controller/`
- **Key source file:** `include/mod_fsd.h` — handleLegacy(), handleHW3(), handleHW4()
- **Config reference:** `include/fsd_config.h` — FSDConfig struct
- **Helper reference:** `include/can_helpers.h` — isFSDSelectedInUI(), setBit(), etc.

## Section 1: Core Helper Calibration

### 1.1 Critical Bug Fix — isADSelectedInUI()

**Current (T-2CAN):**
```cpp
inline bool isADSelectedInUI(const CanFrame &frame) {
    if (bypassTlsscRequirementRuntime)
        return true;
    return (frame.data[4] >> 5) & 0x01;  // byte 4 bit 5 = CAN bit 37
}
```

**Verified (tesla-fsd-controller):**
```cpp
inline bool isFSDSelectedInUI(const CanFrame& frame) {
    return (frame.data[4] >> 6) & 0x01;  // byte 4 bit 6 = CAN bit 38
}
```

**Action:** Replace `isADSelectedInUI()` with `isFSDSelectedInUI()` checking bit 38 (`data[4] >> 6`). Remove `bypassTlsscRequirementRuntime` short-circuit (use `forceActivateRuntime` for force-activate functionality).

### 1.2 setSpeedProfileV12V13() — Add Profile Clamping

**Current (T-2CAN):** No upper bound check. Passing profile > 2 corrupts byte 6.

**Verified:** `if (profile > 2) profile = 2;` before mask operation.

**Action:** Add clamping line.

### 1.3 No Other Helper Changes Needed

`computeVehicleChecksum()`, `setBit()`, `readMuxID()` — all functionally identical between projects.

---

## Section 2: LegacyHandler Rewrite

Target file: `include/handlers.h` — `struct LegacyHandler`

### 2.1 Preserved Logic (State Tracking)

These CAN handlers are T-2CAN-specific (not present in tesla-fsd-controller's mod_fsd.h) and must be preserved unchanged:

- **CAN 280 (DI_systemStatus):** DI_gear -> Parked, summon state tracking
- **CAN 390 (Vehicle status):** DIF_gear -> Parked
- **CAN 921 (DAS_status):** APActive tracking
- **CAN 69 (stalk position):** speedProfile mapping — logic matches verified version

### 2.2 CAN 760 (UI_gpsVehicleSpeed) — Replace

**Current:** Complex MPP bucket table override via `dash_legacy_speed.h` (raises speed limit via bucket lookup). This is a different feature from the verified offset write.

**Verified:** Simple offset write (user speed offset in kph):
```cpp
if (legacyOffset == 0) return;
uint8_t raw = (uint8_t)(legacyOffset + 30);
frame.data[5] = (frame.data[5] & 0xC0) | (raw & 0x3F);
driver.send(frame);
```

**Decision:** Replace T-2CAN's MPP bucket table with verified offset write. `dash_legacy_speed.h` file is kept for reference but LegacyHandler no longer calls it.

### 2.3 CAN 1080 (UI_driverAssistAnonDebugParams) — New

Not present in current T-2CAN. Verified logic:
```cpp
if (frame.id == 1080 && overrideSpeedLimit) {
    frame.data[7] = (frame.data[7] & 0x80) | 100;  // visionSpeedSlider = 100%
    driver.send(frame);
}
```

**Action:** Add CAN 1080 to filter IDs (change from `{69,280,390,760,921,1006}` to `{69,280,390,760,921,1006,1080}`).

### 2.4 CAN 1006 mux 0 — Rewrite

**Verified logic:**
```cpp
fsdTriggered = forceActivateRuntime || isFSDSelectedInUI(frame);
if (fsdTriggered && checkAD()) {
    setBit(frame, 46, true);
    setSpeedProfileV12V13(frame, speedProfile);
    send(frame);
}
```

**Changes from current:**
- Use `isFSDSelectedInUI()` (bit 38) instead of `isADSelectedInUI()` (bit 37)
- Remove `DASH_FSD_252_COMPAT` branch — unified path
- Remove bit 40/41 setting (not in verified code)
- Remove Dashboard-specific skip

### 2.5 CAN 1006 mux 1 — Rewrite

**Verified logic:**
```cpp
if (fsdTriggered && checkAD()) {
    setBit(frame, 19, false);  // nag suppression
    if (removeVisionSpeedLimit) setBit(frame, 48, false);
    driver.send(frame);  // not counted as modification
}
```

**Changes from current:**
- Remove `ESP32_DASHBOARD` compile-time gate
- Remove `checkNag` function pointer gate
- Add optional bit 48 clear (`removeVisionSpeedLimit`)

### 2.6 New CarManagerBase Fields

```cpp
Shared<bool> fsdTriggered{false};
Shared<bool> removeVisionSpeedLimit{true};
Shared<int> legacyOffset{0};          // 0=off, 1-33 kph
Shared<bool> overrideSpeedLimit{false}; // visionSpeedSlider override
```

---

## Section 3: HW3Handler Rewrite

Target file: `include/handlers.h` — `struct HW3Handler`

### 3.1 Preserved Logic

- **CAN 280/390:** gear tracking, summon state — unchanged
- **CAN 1016:** follow distance -> speedProfile, summon SPR tracking — unchanged
- **CAN 921:** APActive, fusedSpeedLimitRaw capture — unchanged
- **CAN 2047:** GTW_autopilot diagnostic — unchanged (Ban Shield added later)

### 3.2 CAN 1021 mux 0 — Rewrite

**Verified logic:**
```cpp
fsdTriggered = forceActivateRuntime || isFSDSelectedInUI(frame);
if (fsdTriggered && checkAD()) {
    hw3SpeedOffset = clamp(((d3>>1)&0x3F - 30)*5, 0, 100);
    setBit(frame, 46, true);
    if (tlsscBypass) setBit(frame, 38, true);
    setSpeedProfileV12V13(frame, speedProfile);
    send(frame);
}
```

**Changes from current:**
- Remove `DASH_FSD_252_COMPAT` branch
- Add bit 38 TLSSC bypass option
- Stock offset capture formula already matches

### 3.3 CAN 1021 mux 1 — Rewrite

**Verified logic:**
```cpp
if (fsdTriggered && checkAD()) {
    setBit(frame, 19, false);
    driver.send(frame);  // nag only, not counted as modification
}
```

**Changes from current:**
- Remove `DASH_FSD_252_COMPAT` branch
- Remove `ENHANCED_AUTOPILOT` conditional
- Remove Dashboard-specific skip
- Use `fsdTriggered` instead of `ADEnabled`

### 3.4 CAN 1021 mux 2 — Full Rewrite

**Verified three-layer logic (from tesla-fsd-controller mod_fsd.h lines 279-371):**

```
Layer 1: Start from stock offset raw
Layer 2: <80kph fused limit:
    - hw3AutoSpeed -> computeHW3MinimumTargetSpeedKph()
      (<60→64, =60→100, 60-79→85, >=80→passthrough)
    - hw3CustomSpeed -> computeHW3CustomTargetSpeedKph()
      (5-bucket table: 30/40/50/60/70 kph)
    - encode via encodeHW3Offset(kph, fusedLimitKph)
Layer 3: >=80kph fused limit:
    - hw3HighSpeedEnable -> per-bucket pct table (80/90/100/110/120)
    - encode via encodeHW3OffsetFromPct(pct, fusedLimitKph)
Layer 4: Slew limiter (downward only, configurable pct/sec)
Layer 5: Write to data[0][7:6] + data[1][5:0]
```

**Action:** Port the complete mux 2 logic from tesla-fsd-controller `handleHW3()` lines 279-371. Inline all encode functions, policy constants, and slew limiter.

### 3.5 New CarManagerBase Fields

```cpp
Shared<int> hw3SpeedOffset{0};         // stock offset from mux 0
Shared<bool> hw3AutoSpeed{true};       // auto target mode
Shared<bool> hw3CustomSpeed{false};    // custom bucket mode
Shared<bool> hw3HighSpeedEnable{false}; // >=80kph custom offset
Shared<bool> tlsscBypass{false};       // HW3+HW4 TLSSC bypass
Shared<uint8_t> hw3WireEncoding{1};    // 0=KPH5, 1=PCT4
Shared<bool> hw3OffsetSlew{false};     // slew limiter toggle
Shared<uint8_t> hw3SlewRatePctPerSec{5}; // slew rate
Shared<uint8_t> hw3OffsetLastRaw{0};   // slew state
Shared<uint32_t> hw3OffsetLastSentMs{0};
Shared<uint8_t> hw3OffsetTargetRaw{0}; // diag
Shared<uint32_t> hw3OffsetSlewCount{0};
```

### 3.6 Policy Constants (inline in handlers.h)

```cpp
// Auto targets
kHw3AutoTargetBelow60Kph = 64
kHw3AutoTargetAt60Kph = 100
kHw3AutoTargetForVisible80Kph = 85
kHw3StockOffsetCutoverKph = 80

// Custom buckets
kHw3CustomBucketBaseKph = 30, step=10, count=5

// High-speed buckets
kHw3HighSpeedBucketBaseKph = 80, step=10, count=5

// Wire encoding
kHw3SpeedOffsetMaxPct = 50
kHw3EncKph5MaxKph = 40, scale=5
kHw3EncPct4Scale = 4

// Slew limiter
kHw3SlewRateMin = 1, Max = 25, Default = 5
```

### 3.7 dash_hw3_speed.h

Keep the file for Dashboard state queries but HW3Handler no longer calls its functions. The encode/bucket logic will be self-contained in the handler.

---

## Section 4: HW4Handler Rewrite

Target file: `include/handlers.h` — `struct HW4Handler`

### 4.1 Preserved Logic

- **CAN 280/390:** gear tracking, summon state — unchanged
- **CAN 1016:** HW4 5-position stalk mapping (profile 0-4) — unchanged
- **CAN 921:** APActive, fusedSpeedLimitRaw — unchanged
- **CAN 2047:** GTW_autopilot diagnostic — unchanged

### 4.2 CAN 921 (ISA Chime) — Rewrite

**Verified logic:**
```cpp
if (isaChimeSuppress && dlc >= 8) {
    frame.data[1] |= 0x20;
    frame.data[7] = computeVehicleChecksum(frame);
    send(frame);
}
```

**Changes from current:**
- Remove `ISA_SPEED_CHIME_SUPPRESS` compile-time gate
- Remove `!ESP32_DASHBOARD` restriction
- Use `computeVehicleChecksum()` instead of manual sum
- Gate on runtime `isaChimeSuppress` instead of compile flag

### 4.3 CAN 1021 mux 0 — Rewrite

**Verified logic:**
```cpp
fsdTriggered = forceActivateRuntime || isFSDSelectedInUI(frame);
if (fsdTriggered && checkAD()) {
    setBit(frame, 46, true);
    setBit(frame, 60, true);
    if (emergencyVehicleDetection) setBit(frame, 59, true);
    if (tlsscBypass) setBit(frame, 38, true);
    send(frame);
}
```

**Changes from current:**
- Add bit 38 TLSSC bypass
- Remove `EMERGENCY_VEHICLE_DETECTION` compile gate, use runtime flag

### 4.4 CAN 1021 mux 1 — Rewrite

**Verified logic:**
```cpp
if (fsdTriggered && checkAD()) {
    setBit(frame, 19, false);  // nag suppression
    setBit(frame, 47, true);   // HW4 FSD ready — critical for activation
    send(frame);  // counted as modification (unlike HW3)
}
```

**Changes from current:**
- Remove `ADEnabled` gate, use `fsdTriggered`
- Count as modification (matches verified behavior)

### 4.5 CAN 1021 mux 2 — Rewrite

**Verified logic:**
```cpp
if (fsdTriggered && checkAD()) {
    // Speed profile
    frame.data[7] &= ~(0x07 << 4);
    frame.data[7] |= (speedProfile & 0x07) << 4;
    // Offset
    if (hw4OffsetRaw > 0)
        frame.data[1] = (frame.data[1] & 0xC0) | (hw4OffsetRaw & 0x3F);
    send(frame);
}
```

**Changes from current:**
- Remove `!speedProfileAuto` condition — always inject profile
- Add `hw4OffsetRaw` offset write (new feature)

### 4.6 Ban Shield (CAN 2047) — New Optional Feature

Port from tesla-fsd-controller mod_fsd.h `handleBanShield2047()`:

```
1. Per-mux (0-7) baseline snapshot (8 bytes each)
2. First occurrence: save snapshot, mark valid
3. Subsequent: compare payload, if changed -> re-send baseline (block detection)
4. Runtime toggle: banShieldEnable
```

Add to both HW3Handler and HW4Handler's CAN 2047 processing, after existing GTW_autopilot diagnostic.

### 4.7 New CarManagerBase Fields

```cpp
Shared<bool> emergencyVehicleDetection{true};
Shared<bool> isaChimeSuppress{false};
Shared<uint8_t> hw4OffsetRaw{0};       // 0-21, raw ~ mph_offset*1.4
Shared<bool> banShieldEnable{false};
Shared<uint32_t banShieldBlocks{0};
// Ban Shield state: snapshot arrays stored as member vars (non-atomic, CAN task only)
uint8_t banShieldSnapshot[8][8] = {};
bool banShieldValid[8] = {};
```

---

## Section 5: Auto Hardware Detection Mode

### 5.1 CAN 0x398 (920) Detection

All handlers get CAN 920 added to their filter IDs. On receipt:

```cpp
if (frame.id == 920 && dlc >= 1) {
    uint8_t das_hw = (frame.data[0] >> 6) & 0x03;
    if (das_hw == 2) hwDetected = 1;   // HW3
    else if (das_hw == 3) hwDetected = 2; // HW4
    return;
}
```

### 5.2 Auto Switch Logic

In `main.cpp` CAN loop, after handler processes each frame:

```cpp
if (autoModeEnabled && hwDetected > 0 && currentHwMode != hwDetected) {
    dashSwapHandler(hwDetected);
}
```

### 5.3 Dashboard UI

Current 3-button selector (Legacy/HW3/HW4) becomes 4-option:
- Legacy (0), HW3 (1), HW4 (2), Auto (3)

Status JSON adds:
```json
{"hwMode": 3, "hwDetected": 2, "hwDetectedName": "HW4"}
```

### 5.4 New CarManagerBase Fields

```cpp
Shared<uint8_t> hwDetected{0};      // 0=unknown, 1=HW3, 2=HW4
Shared<bool> autoModeEnabled{false}; // auto-switch toggle
```

### 5.5 Edge Cases

| Scenario | Behavior |
|----------|----------|
| No 0x398 received | hwDetected=0, no switch |
| Vehicle sleeps/wakes | hwDetected resets to 0, waits for new 0x398 |
| CAN 920 not on Party bus | No switch, Dashboard shows "Unknown" |
| User selects Auto but no detection | Stays on default handler (HW3 per platformio_profile.h) |

---

## Implementation Scope

### Files Modified

| File | Changes |
|------|---------|
| `include/handlers.h` | Rewrite all 3 handler activation logic, add CarManagerBase fields, add Ban Shield |
| `include/can_helpers.h` | Fix isFSDSelectedInUI bit, add profile clamping, remove compile-time gates |
| `src/main.cpp` | Add auto-mode switch logic in CAN loop |
| `include/web/mcp2515_dashboard_ui.src.h` | Add Auto option to HW mode selector |
| `include/web/mcp2515_dashboard.h` | Expose new state fields in JSON, NVS persistence |

### Files NOT Modified

| File | Reason |
|------|--------|
| `include/dash_hw3_speed.h` | Kept for Dashboard queries, handler no longer calls it |
| `include/dash_legacy_speed.h` | Kept for reference, LegacyHandler no longer calls it |
| `include/can_frame_types.h` | No changes needed |
| `include/drivers/*` | No changes needed |
| `platformio.ini` | No new build flags (all runtime now) |

### Build Flags Removed

These compile-time gates become runtime switches:
- `DASH_FSD_252_COMPAT` — removed, unified path
- `ISA_SPEED_CHIME_SUPPRESS` — removed, runtime `isaChimeSuppress`
- `EMERGENCY_VEHICLE_DETECTION` — removed, runtime `emergencyVehicleDetection`
- `ENHANCED_AUTOPILOT` — removed, simplified mux 1
- `BYPASS_TLSSC_REQUIREMENT` — removed, use `forceActivateRuntime` + `tlsscBypass`

### Test Impact

- Existing 142 native tests may need updates for:
  - `isADSelectedInUI()` -> `isFSDSelectedInUI()` bit change
  - Removed compile flags
  - New `fsdTriggered` field usage
- New tests needed for:
  - Auto mode detection and switching
  - Ban Shield snapshot/blocking
  - HW3 mux 2 speed offset layers
  - HW4 mux 2 offset write

### Risk Assessment

| Risk | Mitigation |
|------|-----------|
| Bit 38 fix changes activation for all users | This is a bug fix — verified code uses bit 38 |
| Removing DASH_FSD_252_COMPAT changes Dashboard behavior | Verified logic replaces it; Dashboard post-processing no longer needed |
| HW3 mux 2 rewrite changes speed offset behavior | Using proven auto+custom+high-speed logic |
| Ban Shield adds complexity | Runtime-disabled by default, opt-in |
| Auto mode may misdetect | Only activates on definitive 0x398 values (2 or 3), fallback to manual |
