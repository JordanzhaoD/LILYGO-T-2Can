# Phase 5A: UI收尾 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** 完成驾驶模式UI重构、自动换挡占位页面、模块配置页完善。

**Architecture:** 纯UI层变更，不涉及固件逻辑。pg-drive改为6模式卡片，pg-shift为只读占位。

**Tech Stack:** HTML/CSS/JS (内嵌SPA)

**设计文档:** `docs/superpowers/specs/2026-05-31-fsd-merge-design.md` 第3.3节、第3.7节

**前置依赖:** Phase 1-4 完成

---

## 文件结构

| 操作 | 文件 | 职责 |
|------|------|------|
| Modify | `include/web/mcp2515_dashboard_ui.src.h` | pg-drive重构 + pg-shift新增 + pg-overview完善 |

---

## Task 1: pg-drive 驾驶模式UI重构

- [ ] **Step 1: 替换现有驾驶模式UI为3x2卡片网格**
- [ ] **Step 2: 每卡片含图标+名称+描述文字**
- [ ] **Step 3: 选中状态样式**
- [ ] **Step 4: Commit**

---

## Task 2: pg-shift 自动换挡占位页

- [ ] **Step 1: 新增sidebar导航项（最后）**
- [ ] **Step 2: 创建pg-shift页面HTML**
- [ ] **Step 3: "储备功能未开放"警告卡片**
- [ ] **Step 4: 遥测只读展示（车速/刹车/挡位）**
- [ ] **Step 5: 更新bottom-nav**
- [ ] **Step 6: Commit**

---

## Task 3: pg-overview 状态显示完善

- [ ] **Step 1: 温度显示增加颜色阈值（>60红色）**
- [ ] **Step 2: CAN状态描述文字优化**
- [ ] **Step 3: 软件版本字段显示**
- [ ] **Step 4: Commit**

---

## Task 4: minify + gzip + 最终UI验证

- [ ] **Step 1: 运行minify脚本**
- [ ] **Step 2: 编译完整固件**
- [ ] **Step 3: 验证所有11个页面可导航**
- [ ] **Step 4: git tag phase5a-ui-polish**
