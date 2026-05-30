# Phase 2: 速度偏移重构 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** 采用源代码(DouyinFSD v3.68)的3模式+4区间分段查表算法完全重写速度偏移系统。

**Architecture:** 重写 `dash_hw3_speed.h`，用源代码的分段查表替代现有桶式映射。保留Legacy路径不动。编译开关 `USE_NEW_SPEED_ALGO` 控制新旧切换。

**Tech Stack:** C++ (ESP-IDF), PlatformIO

**设计文档:** `docs/superpowers/specs/2026-05-31-fsd-merge-design.md` 第4.6节

**前置依赖:** Phase 1 完成

---

## 文件结构

| 操作 | 文件 | 职责 |
|------|------|------|
| Rewrite | `include/dash_hw3_speed.h` | 新3模式+4区间+平滑降速算法 |
| Modify | `include/web/mcp2515_dashboard.h` | 新增 POST /speed_custom, 扩展 /speed_strategy |
| Modify | `include/web/mcp2515_dashboard_ui.src.h` | pg-speed UI 3选项卡+实时偏移+算法说明 |
| Modify | `test/test_dashboard_api_contract.py` | 新端点契约测试 |

---

## Task 1: 重写 dash_hw3_speed.h 核心算法

**Files:**
- Rewrite: `include/dash_hw3_speed.h`

- [ ] **Step 1: 添加编译开关和新变量**

在文件顶部添加：
```cpp
#ifndef USE_NEW_SPEED_ALGO
#define USE_NEW_SPEED_ALGO 1  // 1=源代码算法, 0=旧桶式映射
#endif
```

新全局变量：
```cpp
// ── 新速度偏移系统（源代码算法） ─────────────────
static volatile uint8_t offsetMode = 1;        // 0=固定, 1=自动(默认), 2=自定义
static volatile uint8_t manualOffsetPct = 0;   // 固定模式: 0/10/20/30/40/50%
static volatile uint8_t customPct[4] = {30,20,10,10};  // 4区间自定义百分比
static volatile float smoothedOffset = 0.0f;   // 平滑降速跟踪器
static volatile float actualOffset = 0.0f;     // 当前实际偏移(km/h)
static constexpr float SMOOTH_RATE = 5.0f;     // 降速平滑速率 km/h/s
```

- [ ] **Step 2: 实现自动偏移分段查表**

```cpp
inline float dashComputeAutoTarget(float limitKph) {
    if (limitKph <= 40) return min(60.0f, limitKph * 1.5f);
    if (limitKph <= 60) return min(90.0f, limitKph * 1.5f);
    if (limitKph <= 90) return min(117.0f, limitKph * 1.3f);
    if (limitKph <= 110) return min(132.0f, limitKph * 1.2f);
    return min(132.0f, limitKph * 1.1f);
}
```

- [ ] **Step 3: 实现自定义偏移4区间查表**

```cpp
inline float dashComputeCustomTarget(float limitKph) {
    uint8_t pct;
    if (limitKph <= 50) pct = customPct[0];
    else if (limitKph <= 70) pct = customPct[1];
    else if (limitKph <= 100) pct = customPct[2];
    else pct = customPct[3];
    return limitKph * (1.0f + pct / 100.0f);
}
```

- [ ] **Step 4: 实现统一偏移计算 + 平滑降速引擎**

```cpp
inline float dashComputeOffset(float limitKph, float dt) {
    float target;
    switch (offsetMode) {
        case 0: target = limitKph * (1.0f + manualOffsetPct / 100.0f); break;
        case 1: target = dashComputeAutoTarget(limitKph); break;
        case 2: target = dashComputeCustomTarget(limitKph); break;
        default: target = limitKph; break;
    }
    float rawOffset = target - limitKph;

    // 平滑降速引擎
    if (rawOffset < smoothedOffset) {
        smoothedOffset = max(rawOffset, smoothedOffset - SMOOTH_RATE * dt);
    } else {
        smoothedOffset = rawOffset;  // 升速瞬间跟随
    }
    actualOffset = smoothedOffset;
    return smoothedOffset;
}
```

- [ ] **Step 5: 保留旧代码路径（USE_NEW_SPEED_ALGO=0时）**

用 `#if USE_NEW_SPEED_ALGO` 包裹新代码，`#else` 保留旧函数体不变。

- [ ] **Step 6: 编译验证**
- [ ] **Step 7: Commit**

---

## Task 2: 新增 POST /speed_custom 端点

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`

- [ ] **Step 1: 添加 handleSpeedCustom() 处理函数**
- [ ] **Step 2: 注册路由 server.on("/speed_custom", ...)**
- [ ] **Step 3: 在 handleStatus() 中输出 actOffset 和 speedLimit**
- [ ] **Step 4: Commit**

---

## Task 3: 重构 pg-speed UI

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`

- [ ] **Step 1: 替换现有速度偏移UI为3选项卡布局**
- [ ] **Step 2: 添加实时偏移显示区域**
- [ ] **Step 3: 添加算法说明卡片（自动模式分段表）**
- [ ] **Step 4: 添加自定义4区间输入框**
- [ ] **Step 5: 更新JS连接逻辑**
- [ ] **Step 6: minify + gzip**
- [ ] **Step 7: Commit**

---

## Task 4: Phase 2 回归验证

- [ ] **Step 1: 编译测试**
- [ ] **Step 2: API契约测试**
- [ ] **Step 3: 设备验证（可选）**
- [ ] **Step 4: git tag phase2-speed-offset**
