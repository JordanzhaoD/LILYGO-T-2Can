# Phase 3: FSD防御系统重构 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** 升级防御系统为仿生扭矩(0x370正弦波)+0x3C2滚轮DND消除架构，替代现有固定echo和ISA抑制。

**Architecture:** 在NagHandler内部增加仿生模式切换，新建dash_wheel_dnd.h处理0x3C2四步序列。保留fallback机制。

**Tech Stack:** C++ (ESP-IDF), PlatformIO

**设计文档:** `docs/superpowers/specs/2026-05-31-fsd-merge-design.md` 第3.5节、第4.2-4.3节

**前置依赖:** Phase 1 完成

---

## 文件结构

| 操作 | 文件 | 职责 |
|------|------|------|
| Create | `include/dash_bionic_steer.h` | xorshift32 + 正弦波扭矩计算 |
| Create | `include/dash_wheel_dnd.h` | 0x3C2音量/速度四步序列 |
| Modify | `include/handlers.h` | NagHandler增加仿生模式分支 |
| Modify | `include/web/mcp2515_dashboard.h` | 扩展 /defense_config 参数 |
| Modify | `include/web/mcp2515_dashboard_ui.src.h` | pg-defense UI 5开关+描述 |
| Modify | `test/test_native_nag/` | NagHandler仿生模式测试 |

---

## Task 1: 创建仿生方向盘模块

**Files:**
- Create: `include/dash_bionic_steer.h`

核心逻辑：
- xorshift32 伪随机数生成器
- 正弦波扭矩计算：base + amplitude * sin(phase)
- 随机参数：振幅30-55, 方向正/负, 持续350-500ms
- 安全限制：扭矩绝对值不超过60原始单位
- 运行时异常保护：连续3次帧格式异常→回退echo

- [ ] **Step 1: 实现xorshift32**
- [ ] **Step 2: 实现正弦波扭矩计算**
- [ ] **Step 3: 实现异常保护计数器**
- [ ] **Step 4: Commit**

---

## Task 2: 创建滚轮DND模块

**Files:**
- Create: `include/dash_wheel_dnd.h`

核心逻辑：
- 0x3C2音量四步序列：data[2]=0x01→0x00→0x3F→0x00，每步50ms
- 0x3C2速度四步序列：data[3]=0x01→0x00→0x3F→0x00，每步50ms
- 通过CAN-B (MCP2515) 发送
- counter递增 + Tesla checksum重算
- 受防御总开关+dashDndVolume/dashSpeedNoDisturb控制

- [ ] **Step 1: 实现Tesla CAN checksum计算**
- [ ] **Step 2: 实现四步序列状态机**
- [ ] **Step 3: 实现CAN-B发送逻辑**
- [ ] **Step 4: Commit**

---

## Task 3: NagHandler升级

**Files:**
- Modify: `include/handlers.h`

- [ ] **Step 1: include dash_bionic_steer.h**
- [ ] **Step 2: 在 NagHandler::handleMessage() 中增加仿生模式分支**
- [ ] **Step 3: dashBionicSteering=true时调用仿生扭矩，false时保留原echo**
- [ ] **Step 4: 异常保护：仿生失败3次自动回退echo**
- [ ] **Step 5: Commit**

---

## Task 4: 扩展防御API + UI

**Files:**
- Modify: `include/web/mcp2515_dashboard.h`
- Modify: `include/web/mcp2515_dashboard_ui.src.h`

- [ ] **Step 1: /defense_config 新增参数：dnd_volume, dnd_speed**
- [ ] **Step 2: NVS迁移：旧键isaChimeSuppress→新键def_dvol**
- [ ] **Step 3: pg-defense UI重构为5开关+描述文字**
- [ ] **Step 4: 更新poll()函数JS逻辑**
- [ ] **Step 5: minify + gzip**
- [ ] **Step 6: Commit**

---

## Task 5: Phase 3 回归验证

- [ ] **Step 1: 编译测试**
- [ ] **Step 2: NagHandler单元测试（仿生模式+fallback）**
- [ ] **Step 3: API契约测试**
- [ ] **Step 4: git tag phase3-fsd-defense**
