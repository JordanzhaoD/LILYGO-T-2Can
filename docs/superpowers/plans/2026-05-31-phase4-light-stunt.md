# Phase 4: 灯光特技系统 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** 实现独立灯光特技页面(pg-strobe)，包含后雾灯爆闪(0x273)、F1领航灯、持续爆闪。

**Architecture:** 新建dash_fog_light.h处理0x273帧，从pg-bus2分离灯光控件到新页面pg-strobe。

**Tech Stack:** C++ (ESP-IDF), PlatformIO

**设计文档:** `docs/superpowers/specs/2026-05-31-fsd-merge-design.md` 第3.6节、第4.3节

**前置依赖:** Phase 1 完成

---

## 文件结构

| 操作 | 文件 | 职责 |
|------|------|------|
| Create | `include/dash_fog_light.h` | 0x273后雾灯爆闪+F1领航灯控制器 |
| Modify | `include/web/mcp2515_dashboard.h` | 扩展 /fog_light + /strobe_cont 执行逻辑 |
| Modify | `include/web/mcp2515_dashboard_ui.src.h` | 新增pg-strobe页面 + pg-bus2清理 |
| Modify | `src/main.cpp` | fog light定时器集成 |

---

## Task 1: 创建后雾灯控制器

**Files:**
- Create: `include/dash_fog_light.h`

核心逻辑：
- 基础帧：{0x81, 0xE1, 0x10, 0x40, 0x0B, 0x03, 0x30, checksum}
- 爆闪模式：data[2]在0x90/0x10间切换，50ms间隔
- F1领航：3连闪135ms + 1500ms停顿循环
- 安全条件：仅D挡激活（检测apRestoreState.gearRaw==4）
- 通过CAN-B (MCP2515) 发送

- [ ] **Step 1: 实现FogLightController类/结构体**
- [ ] **Step 2: 实现爆闪模式tick()**
- [ ] **Step 3: 实现F1领航模式tick()**
- [ ] **Step 4: 实现安全退出条件**
- [ ] **Step 5: Commit**

---

## Task 2: 实现持续爆闪逻辑

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`

- [ ] **Step 1: 扩展handleStrobeCont()为完整逻辑**
- [ ] **Step 2: 在stalk inject tick中添加无限循环模式**
- [ ] **Step 3: 安全退出：非D挡/CAN离线/WiFi断开60s自动停止**
- [ ] **Step 4: Commit**

---

## Task 3: 创建pg-strobe页面 + pg-bus2清理

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h`

- [ ] **Step 1: 在sidebar导航添加"灯光特技"项（pg-bus2和pg-defense之间）**
- [ ] **Step 2: 创建pg-strobe页面HTML（5个区域）**
- [ ] **Step 3: 添加JS连接逻辑（toggleStrobe, toggleFogStrobe等）**
- [ ] **Step 4: 从pg-bus2移除已迁出控件（爆闪开关/次数/频率/雾灯）**
- [ ] **Step 5: 保留pg-bus2中的stalk测试+ID发现+Service Mode**
- [ ] **Step 6: 更新bottom-nav移动端导航**
- [ ] **Step 7: minify + gzip**
- [ ] **Step 8: Commit**

---

## Task 4: Phase 4 回归验证

- [ ] **Step 1: 编译测试**
- [ ] **Step 2: API契约测试（新页面端点验证）**
- [ ] **Step 3: pg-bus2功能确认（stalk/ID/service mode仍正常）**
- [ ] **Step 4: git tag phase4-light-stunt**
