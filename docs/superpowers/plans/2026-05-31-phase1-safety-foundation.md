# Phase 1: 安全基础 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 添加车辆OTA保护、功耗管理、CAN帧类型定义和状态字段扩展，零侵入不影响现有功能。

**Architecture:** 新增3个独立头文件模块（OTA guard、power management、fog light stub），扩展现有帧类型和状态输出。所有新功能通过配置开关控制，默认关闭。

**Tech Stack:** C++ (ESP-IDF/Arduino), PlatformIO, ESP32-S3

**设计文档:** `docs/superpowers/specs/2026-05-31-fsd-merge-design.md`

---

## 文件结构

| 操作 | 文件 | 职责 |
|------|------|------|
| Create | `include/dash_ota_guard.h` | 0x318 OTA检测，设置全局标志 |
| Create | `include/dash_power_mgmt.h` | 自动关机(5min无CAN) + WiFi自动关闭(5min无Web) |
| Modify | `include/can_frame_types.h` | 新增0x273/0x318/0x3C2帧常量和结构 |
| Modify | `include/web/mcp2515_dashboard.h` | 新增4个POST端点 + 1个GET端点，扩展status字段 |
| Modify | `include/web/mcp2515_dashboard_ui.src.h` | pg-overview页面新增状态显示和开关 |
| Modify | `src/main.cpp` | 初始化OTA guard + power mgmt定时器 |
| Modify | `test/test_dashboard_api_contract.py` | 新增端点契约测试 |

---

## Task 1: 扩展CAN帧类型定义

**Files:**
- Modify: `include/can_frame_types.h`

- [ ] **Step 1: 在文件末尾（namespace或文件级）添加新帧常量**

在 `can_frame_types.h` 末尾（`#pragma once` 保护内），现有帧定义之后添加：

```cpp
// ── 新增帧ID常量（Phase 1 合并） ──────────────────────────
namespace CanId {
    // 车辆OTA状态（CAN-A TWAI）
    static constexpr uint32_t OTA_STATUS      = 0x318;  // data[6]&0x03==2 → OTA进行中

    // 后雾灯控制（CAN-B MCP2515）
    static constexpr uint32_t REAR_FOG_LIGHT  = 0x273;  // data[2]=0x90(亮)/0x10(灭)

    // 方向盘滚轮按钮（CAN-B MCP2515）
    static constexpr uint32_t STEERING_WHEEL  = 0x3C2;  // data[2]音量, data[3]速度
}

// 0x273 后雾灯基础帧
struct FogLightFrame {
    static constexpr uint8_t BASE[8] = {0x81, 0xE1, 0x10, 0x40, 0x0B, 0x03, 0x30, 0x12};
    static constexpr uint8_t ON_BYTE  = 0x90;  // data[2] = 雾灯亮
    static constexpr uint8_t OFF_BYTE = 0x10;  // data[2] = 雾灯灭
};

// 0x3C2 方向盘滚轮命令值
namespace WheelCmd {
    static constexpr uint8_t VOL_UP      = 0x01;  // data[2] 音量上
    static constexpr uint8_t VOL_DOWN    = 0x3F;  // data[2] 音量下
    static constexpr uint8_t RELEASE     = 0x00;  // data[2/3] 回弹/释放
    static constexpr uint8_t SPEED_UP    = 0x01;  // data[3] 速度上
    static constexpr uint8_t SPEED_DOWN  = 0x3F;  // data[3] 速度下
}
```

- [ ] **Step 2: 编译验证**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e lilygo_t2can_dual 2>&1 | tail -5`
Expected: `SUCCESS`（仅添加常量，不影响编译）

- [ ] **Step 3: Commit**

```bash
git add include/can_frame_types.h
git commit -m "feat: add CAN frame constants for 0x318/0x273/0x3C2"
```

---

## Task 2: 创建OTA保护模块

**Files:**
- Create: `include/dash_ota_guard.h`

- [ ] **Step 1: 创建 dash_ota_guard.h**

```cpp
#pragma once
// dash_ota_guard.h — 车辆OTA保护（Phase 1）
// 监听0x318帧，检测车辆OTA状态，暂停FSD注入
// 零侵入：仅设置全局标志，不修改Handler内部逻辑

#include "can_frame_types.h"

// ── 全局状态 ──────────────────────────────────────────
static volatile bool vehicleOtaActive = false;  // true=车辆OTA进行中，暂停注入

// ── 内部计数器 ────────────────────────────────────────
static volatile uint8_t otaConfirmCount = 0;     // 连续检测到OTA的次数
static volatile uint8_t otaClearCount   = 0;     // 连续未检测到OTA的次数

// ── 配置 ─────────────────────────────────────────────
static constexpr uint8_t OTA_CONFIRM_THRESHOLD = 3;   // 连续3次确认OTA
static constexpr uint8_t OTA_CLEAR_THRESHOLD   = 6;   // 连续6次确认OTA结束

// ── NVS持久化键 ──────────────────────────────────────
static constexpr const char* NVS_OTA_GUARD = "ota_guard";

// ── 核心检测函数 ─────────────────────────────────────
// 在CAN-A帧回调中调用，传入0x318帧
inline void dashOtaGuardProcessFrame(const CanFrame& frame) {
    if (frame.id != CanId::OTA_STATUS) return;
    if (frame.dlc < 7) return;

    bool otaFlag = (frame.data[6] & 0x03) == 0x02;

    if (otaFlag) {
        otaConfirmCount++;
        otaClearCount = 0;
        if (otaConfirmCount >= OTA_CONFIRM_THRESHOLD && !vehicleOtaActive) {
            vehicleOtaActive = true;
        }
    } else {
        otaClearCount++;
        otaConfirmCount = 0;
        if (otaClearCount >= OTA_CLEAR_THRESHOLD && vehicleOtaActive) {
            vehicleOtaActive = false;
        }
    }
}

// ── 注入门禁检查 ─────────────────────────────────────
// 在 dashInjectionActive() 中调用
// 返回true=允许注入，false=OTA进行中应暂停
inline bool dashOtaGuardAllowInjection() {
    return !vehicleOtaActive;
}

// ── 状态JSON输出 ─────────────────────────────────────
inline bool dashOtaGuardStatus() { return vehicleOtaActive; }
```

- [ ] **Step 2: 编译验证**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e lilygo_t2can_dual 2>&1 | tail -5`
Expected: `SUCCESS`（头文件未被include前不影响编译，但应能被成功编译）

- [ ] **Step 3: Commit**

```bash
git add include/dash_ota_guard.h
git commit -m "feat: add vehicle OTA guard module (0x318 detection)"
```

---

## Task 3: 创建功耗管理模块

**Files:**
- Create: `include/dash_power_mgmt.h`

- [ ] **Step 1: 创建 dash_power_mgmt.h**

```cpp
#pragma once
// dash_power_mgmt.h — 功耗管理（Phase 1）
// 自动关机：5分钟无CAN数据 → deep sleep，TWAI_RX唤醒
// WiFi自动关闭：5分钟无Web请求 → 仅关STA保AP

#include <esp_sleep.h>
#include <Arduino.h>

// ── 配置参数 ─────────────────────────────────────────
static constexpr uint32_t CAN_TIMEOUT_MS      = 300000;  // 5分钟
static constexpr uint32_t WEB_TIMEOUT_MS      = 300000;  // 5分钟
static constexpr const char* NVS_AUTO_SHUTDOWN = "auto_shutdown";
static constexpr const char* NVS_WIFI_AUTO_OFF = "wifi_auto_off";

// ── 全局状态 ─────────────────────────────────────────
static volatile bool autoShutdownEnabled = false;  // 自动关机开关
static volatile bool wifiAutoOffEnabled  = false;  // WiFi自动关闭开关
static volatile uint32_t lastCanActivityMs  = 0;   // 最后CAN活动时间
static volatile uint32_t lastWebActivityMs  = 0;   // 最后Web请求时间
static volatile bool wifiStaDisabled = false;       // STA已关闭标记

// ── 初始化 ───────────────────────────────────────────
// 在setup()中调用，配置deep sleep唤醒源
inline void dashPowerMgmtInit() {
    if (autoShutdownEnabled) {
        // TWAI_RX (GPIO6) 作为EXT0唤醒源，CAN活动时自动唤醒
        esp_sleep_enable_ext0_wakeup(GPIO_NUM_6, 1);
    }
    lastCanActivityMs = millis();
    lastWebActivityMs = millis();
}

// ── CAN活动更新 ──────────────────────────────────────
// 在每次收到CAN帧时调用
inline void dashPowerMgmtTouchCan() {
    lastCanActivityMs = millis();
}

// ── Web活动更新 ──────────────────────────────────────
// 在每个WebServer请求处理开始时调用
inline void dashPowerMgmtTouchWeb() {
    lastWebActivityMs = millis();
    // 如果STA被关闭了，恢复它
    if (wifiStaDisabled) {
        WiFi.mode(WIFI_AP_STA);
        wifiStaDisabled = false;
    }
}

// ── 主循环检查 ───────────────────────────────────────
// 在loop()中周期调用
// 返回true表示设备即将进入deep sleep
inline bool dashPowerMgmtTick() {
    uint32_t now = millis();

    // WiFi自动关闭：仅关闭STA，保留AP
    if (wifiAutoOffEnabled && !wifiStaDisabled &&
        (now - lastWebActivityMs) > WEB_TIMEOUT_MS) {
        WiFi.mode(WIFI_AP);  // 保留AP，关闭STA
        wifiStaDisabled = true;
    }

    // 自动关机：deep sleep
    if (autoShutdownEnabled &&
        (now - lastCanActivityMs) > CAN_TIMEOUT_MS) {
        // 进入deep sleep，TWAI_RX唤醒
        esp_deep_sleep_start();
        return true;  // 不会执行到这里，但保留
    }

    return false;
}

// ── NVS读写 ──────────────────────────────────────────
inline void dashPowerMgmtLoadNVS(Preferences& prefs) {
    autoShutdownEnabled = prefs.getBool(NVS_AUTO_SHUTDOWN, false);
    wifiAutoOffEnabled  = prefs.getBool(NVS_WIFI_AUTO_OFF, false);
}

inline void dashPowerMgmtSaveNVS(Preferences& prefs, bool autoShutdown, bool wifiAutoOff) {
    autoShutdownEnabled = autoShutdown;
    wifiAutoOffEnabled  = wifiAutoOff;
    prefs.putBool(NVS_AUTO_SHUTDOWN, autoShutdown);
    prefs.putBool(NVS_WIFI_AUTO_OFF, wifiAutoOff);
}
```

- [ ] **Step 2: 编译验证**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e lilygo_t2can_dual 2>&1 | tail -5`
Expected: `SUCCESS`

- [ ] **Step 3: Commit**

```bash
git add include/dash_power_mgmt.h
git commit -m "feat: add power management module (auto-shutdown + WiFi auto-off)"
```

---

## Task 4: 集成OTA Guard到帧处理流程

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`（在handleStatus中输出OTA状态）
- Modify: `src/main.cpp`（在CAN帧处理中调用OTA guard）

- [ ] **Step 1: 在 mcp2515_dashboard.h 顶部添加include**

在现有 `#include` 区域添加：
```cpp
#include "dash_ota_guard.h"
#include "dash_power_mgmt.h"
```

- [ ] **Step 2: 在 handleStatus() JSON输出中添加新字段**

在 `handleStatus()` 函数的 JSON 输出中（现有字段之后）添加：
```cpp
    // Phase 1 新增状态字段
    j.set("vehicleOta", vehicleOtaActive);
    j.set("autoShutdown", autoShutdownEnabled);
    j.set("wifiAutoOff", wifiAutoOffEnabled);
    j.set("fogStrategy", (int)dashRearFogStrategy);  // 0=off,1=strobe,2=pilot
    j.set("strobeCont", false);  // Phase 4 实现
    j.set("dndVolume", dashDndVolume);   // Phase 3 使用
    j.set("dndSpeed", dashSpeedNoDisturb);
```

注意：`dashDndVolume` 如果现有代码中没有这个变量名（现有是 `isaChimeSuppress`），需要在 dashboard 全局变量区声明：
```cpp
static bool dashDndVolume = false;  // 音量消除DND（Phase 3实现执行逻辑）
```

- [ ] **Step 3: 新增API端点处理函数**

在 `mcp2515_dashboard.h` 的 handler 函数区添加：

```cpp
// ── POST /power_mgmt ──────────────────────────────
static void handlePowerMgmt() {
    if (server.method() == HTTP_POST) {
        bool autoShutdown = server.hasArg("autoShutdown") && server.arg("autoShutdown") == "true";
        bool wifiAutoOff  = server.hasArg("wifiAutoOff")  && server.arg("wifiAutoOff") == "true";
        dashPowerMgmtSaveNVS(prefs, autoShutdown, wifiAutoOff);
        server.send(200, "application/json", "{\"ok\":true}");
    } else {
        String json = "{\"autoShutdown\":";
        json += autoShutdownEnabled ? "true" : "false";
        json += ",\"wifiAutoOff\":";
        json += wifiAutoOffEnabled ? "true" : "false";
        json += "}";
        server.send(200, "application/json", json);
    }
}

// ── GET /vehicle_ota_status ────────────────────────
static void handleVehicleOtaStatus() {
    String json = "{\"vehicleOta\":";
    json += vehicleOtaActive ? "true" : "false";
    json += ",\"otaConfirmCount\":";
    json += String(otaConfirmCount);
    json += ",\"otaClearCount\":";
    json += String(otaClearCount);
    json += "}";
    server.send(200, "application/json", json);
}

// ── POST /fog_light ────────────────────────────────
static void handleFogLight() {
    if (server.method() == HTTP_POST) {
        int strategy = server.hasArg("fogStrategy") ? server.arg("fogStrategy").toInt() : 0;
        if (strategy < 0 || strategy > 2) strategy = 0;
        dashRearFogStrategy = strategy;
        prefs.putUChar("lt_fog", strategy);
        server.send(200, "application/json", "{\"ok\":true,\"fogStrategy\":" + String(strategy) + "}");
    } else {
        server.send(200, "application/json", "{\"fogStrategy\":" + String(dashRearFogStrategy) + "}");
    }
}

// ── POST /strobe_cont ──────────────────────────────
static void handleStrobeCont() {
    // Phase 4 实现完整逻辑，Phase 1 仅保存配置
    server.send(200, "application/json", "{\"ok\":true,\"note\":\"Phase 4\"}");
}
```

- [ ] **Step 4: 注册新路由**

在 `mcpDashboardSetup()` 函数的路由注册区域（现有 `server.on()` 调用附近）添加：

```cpp
    server.on("/power_mgmt",       HTTP_GET,  handlePowerMgmt);
    server.on("/power_mgmt",       HTTP_POST, handlePowerMgmt);
    server.on("/vehicle_ota_status", HTTP_GET, handleVehicleOtaStatus);
    server.on("/fog_light",        HTTP_GET,  handleFogLight);
    server.on("/fog_light",        HTTP_POST, handleFogLight);
    server.on("/strobe_cont",      HTTP_POST, handleStrobeCont);
```

- [ ] **Step 5: 在NVS加载区初始化功耗管理**

在现有的 NVS 配置加载函数中添加：
```cpp
    dashPowerMgmtLoadNVS(prefs);
    dashPowerMgmtInit();
```

- [ ] **Step 6: 在 main.cpp 的 CAN帧处理循环中集成 OTA guard**

在 `appLoop()` 或 CAN帧处理回调中，在现有帧处理之后添加：
```cpp
    // OTA guard：检测0x318帧
    dashOtaGuardProcessFrame(frame);
    // 功耗管理：记录CAN活动
    dashPowerMgmtTouchCan();
```

- [ ] **Step 7: 在 dashInjectionActive() 中添加OTA门禁**

找到 `dashInjectionActive()` 或等效的注入检查函数，在返回前添加条件：
```cpp
    // OTA保护：车辆OTA进行中时暂停注入
    if (!dashOtaGuardAllowInjection()) return false;
```

- [ ] **Step 8: 在 WebServer 请求钩子中更新 Web 活动时间**

在 `mcpDashboardSetup()` 中，或现有的请求拦截中添加：
```cpp
    // 所有Web请求更新功耗管理计时器
    server.onNotFound([]() {
        dashPowerMgmtTouchWeb();
        server.send(404, "text/plain", "Not Found");
    });
```

同时在每个 handler 函数开头可以统一添加 `dashPowerMgmtTouchWeb();`，或更优雅地在全局before-handler中处理。

- [ ] **Step 9: 在主循环中调用功耗管理tick**

在 `mcpDashboardLoop()` 末尾添加：
```cpp
    dashPowerMgmtTick();
```

- [ ] **Step 10: 编译验证**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e lilygo_t2can_dual 2>&1 | tail -5`
Expected: `SUCCESS`

- [ ] **Step 11: Commit**

```bash
git add include/web/mcp2515_dashboard.h src/main.cpp
git commit -m "feat: integrate OTA guard + power mgmt into dashboard and main loop"
```

---

## Task 5: 更新API契约测试

**Files:**
- Modify: `test/test_dashboard_api_contract.py`

- [ ] **Step 1: 在契约测试中添加新端点验证**

在 `test_dashboard_api_contract.py` 中添加测试用例：

```python
def test_phase1_new_endpoints():
    """Phase 1 新增端点应在固件路由中注册"""
    # 这些端点在UI的JS中会被调用，固件必须有对应路由
    new_endpoints = {
        "GET  /power_mgmt": "handlePowerMgmt",
        "POST /power_mgmt": "handlePowerMgmt",
        "GET  /vehicle_ota_status": "handleVehicleOtaStatus",
        "GET  /fog_light": "handleFogLight",
        "POST /fog_light": "handleFogLight",
        "POST /strobe_cont": "handleStrobeCont",
    }
    # 验证固件头文件中有对应的 server.on() 注册
    dashboard_h = Path("include/web/mcp2515_dashboard.h").read_text()
    for endpoint, handler in new_endpoints.items():
        method, path = endpoint.split()
        assert f'"{path}"' in dashboard_h, f"Missing route: {endpoint}"
        assert handler in dashboard_h, f"Missing handler: {handler}"
```

- [ ] **Step 2: 运行契约测试**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python test/test_dashboard_api_contract.py`
Expected: `PASS`

- [ ] **Step 3: Commit**

```bash
git add test/test_dashboard_api_contract.py
git commit -m "test: add Phase 1 API contract tests for new endpoints"
```

---

## Task 6: 更新pg-overview UI — 状态显示和开关

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`

- [ ] **Step 1: 在sidebar导航项中确认pg-overview位置**

现有导航项中 `pg-overview` 已存在，无需修改导航。

- [ ] **Step 2: 在 pg-overview 页面HTML中添加状态详情卡片**

找到 `pg-overview` 的 `<div class="page" id="pg-overview">` 区域。在现有的stats grid之后添加状态详情区域：

```html
<!-- 模块状态详情 -->
<div class="card" style="margin-top:16px">
  <h3>模块状态</h3>
  <div class="status-grid" id="module-status-grid">
    <div class="status-item">
      <span class="status-label">FSD防封</span>
      <span class="status-value" id="st-firewall">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">车机AI电脑</span>
      <span class="status-value" id="st-hw-detect">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">CAN状态A</span>
      <span class="status-value" id="st-can-a">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">CAN状态B</span>
      <span class="status-value" id="st-can-b">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">模块温度</span>
      <span class="status-value" id="st-temp">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">当前挡位</span>
      <span class="status-value" id="st-gear">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">车辆OTA</span>
      <span class="status-value" id="st-vota">--</span>
    </div>
    <div class="status-item">
      <span class="status-label">软件版本</span>
      <span class="status-value" id="st-version">--</span>
    </div>
  </div>
</div>
```

- [ ] **Step 3: 添加新开关（自动关机 + WiFi关闭）**

在现有开关区域之后添加：

```html
<!-- 功耗管理开关 -->
<div class="card" style="margin-top:16px">
  <h3>功耗管理</h3>
  <div class="toggle-row">
    <span>自动关机<span class="toggle-hint">5分钟无CAN数据自动休眠</span></span>
    <label class="switch"><input type="checkbox" id="tog-auto-shutdown" onchange="toggleAutoShutdown(this.checked)"><span class="slider"></span></label>
  </div>
  <div class="toggle-row">
    <span>关闭WiFi<span class="toggle-hint">5分钟无操作关闭网络中转</span></span>
    <label class="switch"><input type="checkbox" id="tog-wifi-auto-off" onchange="toggleWifiAutoOff(this.checked)"><span class="slider"></span></label>
  </div>
</div>
```

- [ ] **Step 4: 添加对应的JavaScript函数**

在 `<script>` 区域添加：

```javascript
async function toggleAutoShutdown(on) {
  await postForm('/power_mgmt', 'autoShutdown=' + on);
}
async function toggleWifiAutoOff(on) {
  await postForm('/power_mgmt', 'wifiAutoOff=' + on);
}
```

- [ ] **Step 5: 在 poll() 函数中更新新UI元素**

在现有 `poll()` 函数的JSON解析区域添加：

```javascript
    // Phase 1 新增状态
    const gearMap = {1:'P', 2:'R', 3:'N', 4:'D'};
    const gear = d.gear || 0;
    const el = id => document.getElementById(id);

    // CAN-A状态
    if (el('st-can-a')) {
      const fps = d.fps || 0;
      if (!d.can || d.can === 0) el('st-can-a').innerHTML = '<span class="red">异常脱机 (检查连线)</span>';
      else if (fps === 0) el('st-can-a').innerHTML = '<span class="red">异常 (无数据)</span>';
      else el('st-can-a').innerHTML = '<span class="green">正常 (' + fps + ' fps)</span>';
    }

    // CAN-B状态
    if (el('st-can-b')) {
      const eflg = d.eflg || 0;
      if (eflg > 0) el('st-can-b').innerHTML = '<span class="red">异常脱机 (检查连线)</span>';
      else el('st-can-b').innerHTML = '<span class="green">正常</span>';
    }

    // 温度
    if (el('st-temp')) {
      fetchJson('/system_status').then(s => {
        const t = s.chipTemp || s.temp || '--';
        const hot = parseFloat(t) > 60;
        el('st-temp').innerHTML = hot ? '<span class="red">' + t + ' °C</span>' : t + ' °C';
      }).catch(() => {});
    }

    // 挡位
    if (el('st-gear')) {
      el('st-gear').textContent = gearMap[gear] || '--';
    }

    // 车辆OTA
    if (el('st-vota')) {
      const vota = d.vehicleOta || false;
      el('st-vota').innerHTML = vota ? '<span class="red">OTA进行中</span>' : '<span class="green">正常</span>';
    }

    // 功耗管理开关
    if (el('tog-auto-shutdown')) el('tog-auto-shutdown').checked = d.autoShutdown || false;
    if (el('tog-wifi-auto-off')) el('tog-wifi-auto-off').checked = d.wifiAutoOff || false;
```

- [ ] **Step 6: 在 `/status` 端点中添加 gear 字段**

在 `handleStatus()` 的JSON输出中添加gear字段。gear需要从CAN 0x118帧解码。在dashboard全局变量区添加：
```cpp
static volatile uint8_t currentGear = 3;  // 默认N挡，1=P,2=R,3=N,4=D
```

在CAN帧处理中（0x118帧回调）更新：
```cpp
    if (frame.id == 0x118 && frame.dlc >= 3) {
        currentGear = (frame.data[2] >> 5) & 0x07;
    }
```

在handleStatus()输出中添加：
```cpp
    j.set("gear", (int)currentGear);
```

- [ ] **Step 7: 编译验证**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e lilygo_t2can_dual 2>&1 | tail -5`
Expected: `SUCCESS`

- [ ] **Step 8: 运行 minify 脚本更新 gzip 版本**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python scripts/minify_dashboard.py`
Expected: 生成新的 `mcp2515_dashboard_ui.h` (gzip compressed)

- [ ] **Step 9: Commit**

```bash
git add include/web/mcp2515_dashboard_ui.src.h include/web/mcp2515_dashboard_ui.h include/web/mcp2515_dashboard.h
git commit -m "feat: update pg-overview with status details, gear, power management toggles"
```

---

## Task 7: Phase 1 回归验证

**Files:**
- All modified files

- [ ] **Step 1: 完整编译**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && pio run -e lilygo_t2can_dual 2>&1 | tail -10`
Expected: `SUCCESS`

- [ ] **Step 2: 运行现有测试确保未破坏**

Run: `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-firmware && python test/test_dashboard_api_contract.py`
Expected: 所有测试通过

- [ ] **Step 3: Flash到设备验证（可选）**

如果有T-2CAN设备连接：
```bash
pio run -e lilygo_t2can_dual -t upload
```

验证：
- [ ] Web dashboard 正常加载
- [ ] pg-overview 新状态显示正常
- [ ] 新开关可切换且配置持久化
- [ ] 其他8个页面功能不受影响
- [ ] OTA guard状态在 /vehicle_ota_status 可查看

- [ ] **Step 4: Phase 1 完成标记**

```bash
git tag phase1-safety-foundation
git push origin main --tags
```

---

## Phase 1 完成标准

- [x] `dash_ota_guard.h` 创建并集成
- [x] `dash_power_mgmt.h` 创建并集成
- [x] CAN帧类型扩展（0x318/0x273/0x3C2）
- [x] 5个新API端点（/power_mgmt GET+POST, /vehicle_ota_status, /fog_light GET+POST, /strobe_cont）
- [x] /status 新增字段（vehicleOta, autoShutdown, wifiAutoOff, gear, fogStrategy等）
- [x] pg-overview UI 更新（状态详情+功耗开关）
- [x] API契约测试通过
- [x] 现有功能不受影响
