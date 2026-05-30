# Phase 5B: 集成验证与发布 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** 全量回归测试、API契约验证、构建发布固件。

**Architecture:** 测试驱动验证，确保5个Phase的所有新功能与现有功能协同正常。

**Tech Stack:** PlatformIO, Python测试

**前置依赖:** Phase 1-5A 全部完成

---

## Task 1: API契约全量测试

- [ ] **Step 1: 验证所有旧端点仍然正常注册**
- [ ] **Step 2: 验证所有新端点已注册**
- [ ] **Step 3: 验证JS中所有fetchJson/postForm调用都有对应路由**
- [ ] **Step 4: Commit**

---

## Task 2: 功能回归测试

- [ ] **Step 1: 现有9页功能验证（无回归）**
- [ ] **Step 2: 新增2页功能验证（pg-strobe, pg-shift）**
- [ ] **Step 3: 速度偏移3模式测试**
- [ ] **Step 4: 防御系统5开关测试**
- [ ] **Step 5: 功耗管理开关测试**
- [ ] **Step 6: OTA guard状态测试**

---

## Task 3: 构建发布固件

- [ ] **Step 1: 完整编译（clean build）**
- [ ] **Step 2: 检查固件大小（Flash分区余量）**
- [ ] **Step 3: 生成OTA .bin 和 合并.bin**
- [ ] **Step 4: 复制到 firmware-assets/ 目录**
- [ ] **Step 5: 更新VERSION文件**
- [ ] **Step 6: git tag v4.02**
- [ ] **Step 7: git push origin main --tags**
