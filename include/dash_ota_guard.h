#pragma once
// dash_ota_guard.h — 车辆OTA保护（Phase 1；4.5.0-beta12 三路径改造）
// 监听0x318帧，检测车辆OTA状态，暂停FSD注入
// 零侵入：仅设置全局标志，不修改Handler内部逻辑
//
// 4.5.0-beta12（LittleGong 逆向移植 1）：原清除条件是「连续 6 帧非 OTA 的
// 0x318」——若车辆侧 0x318 停发（bus 静默/网关降级），标志永久卡 true，
// permit 静默死亡、仅拔插可恢复（beta08 实车症状 2/3 头号嫌疑）。三条清除
// 路径（permit 表达式逐字不动，全部实现在本状态机内部 vehicleOtaActive
// 的清除条件上）：
//   A. 原路径保留：连续 OTA_CLEAR_THRESHOLD 帧非 OTA 0x318 → 清除；
//   B. 停流超时降级：置位持续满 kOtaStaleTimeoutMs 且期间零 0x318 帧
//      （0x318 还在流且报 OTA 则继续封锁）→ 自动清除 + staleClears 计数，
//      调用方（main.cpp tick）记日志；
//   C. 用户覆盖：dashOtaGuardSetOverride(true)（defense_config
//      ota_guard_override / NVS otg_ovr / UI「忽略车辆 OTA 守卫」）→
//      立即清除并暂停置位，关掉后干净重来（真 OTA 会被 3 帧确认重新捕获）。
#include "can_frame_types.h"
static volatile bool vehicleOtaActive = false; // true=车辆OTA进行中，暂停注入
static volatile uint8_t otaConfirmCount = 0;   // 连续检测到OTA的次数
static volatile uint8_t otaClearCount = 0;     // 连续未检测到OTA的次数
// beta12 观测/降级状态（同 TU static，otaWatch 与 /vehicle_ota_status 发射）
static volatile bool otaGuardOverride = false; // 用户覆盖：忽略守卫（诊断用）
static volatile uint32_t otaLastSeenMs = 0;    // 最近一帧 0x318（任意 dlc）的时刻
static volatile uint32_t otaActiveSinceMs = 0; // 本轮置位起点（0=未置位）
static volatile uint32_t otaStaleClears = 0;   // 停流超时降级清除累计次数
#define OTA_CONFIRM_THRESHOLD 3                // 连续3次确认OTA
#define OTA_CLEAR_THRESHOLD 6                  // 连续6次确认OTA结束
#define OTA_STALE_TIMEOUT_MS 600000UL          // 停流超时降级：10 分钟零 0x318 → 清除

inline void dashOtaGuardProcessFrame(const CanFrame &frame, uint32_t nowMs)
{
    if (frame.id != CAN_ID_OTA_STATUS)
        return;
    // 任意 dlc 的 0x318 都证明载波存活（dlc 门仅约束 OTA 标志位判读）
    otaLastSeenMs = nowMs;
    if (otaGuardOverride)
    {
        // 覆盖期间：不置位、不计数（保持干净，关闭覆盖后从零确认）
        vehicleOtaActive = false;
        otaConfirmCount = 0;
        otaClearCount = 0;
        otaActiveSinceMs = 0;
        return;
    }
    if (frame.dlc < 7)
        return;
    bool otaFlag = (frame.data[6] & 0x03) == 0x02;
    if (otaFlag)
    {
        otaConfirmCount = otaConfirmCount + 1;
        otaClearCount = 0;
        if (otaConfirmCount >= OTA_CONFIRM_THRESHOLD && !vehicleOtaActive)
        {
            vehicleOtaActive = true;
            otaActiveSinceMs = nowMs;
        }
    }
    else
    {
        otaClearCount = otaClearCount + 1;
        otaConfirmCount = 0;
        if (otaClearCount >= OTA_CLEAR_THRESHOLD && vehicleOtaActive)
        {
            vehicleOtaActive = false;
            otaActiveSinceMs = 0;
        }
    }
}

// 停流超时降级（路径 B）。由 main.cpp CAN 任务周期调用；nowMs 必须显式传入
// （native 测试驱动离散时钟）。返回 true = 本次调用执行了降级清除（调用方
// 记日志；staleClears 已自增）。
inline bool dashOtaGuardTick(uint32_t nowMs)
{
    if (!vehicleOtaActive || otaGuardOverride)
        return false;
    const uint32_t lastSeen = otaLastSeenMs;
    if (lastSeen == 0)
        return false; // 开机以来从未见过 0x318：无从判定停流
    if ((uint32_t)(nowMs - lastSeen) < OTA_STALE_TIMEOUT_MS)
        return false;
    vehicleOtaActive = false;
    otaActiveSinceMs = 0;
    otaConfirmCount = 0;
    otaClearCount = 0;
    otaStaleClears = otaStaleClears + 1;
    return true;
}

// 用户覆盖（路径 C）：开=立即清除并暂停置位；关=干净重来。
inline void dashOtaGuardSetOverride(bool v)
{
    otaGuardOverride = v;
    if (v)
    {
        vehicleOtaActive = false;
        otaActiveSinceMs = 0;
        otaConfirmCount = 0;
        otaClearCount = 0;
    }
}

inline bool dashOtaGuardAllowInjection() { return !vehicleOtaActive; }
inline bool dashOtaGuardStatus() { return vehicleOtaActive; }
inline bool dashOtaGuardOverrideEnabled() { return otaGuardOverride; }
// 置位已持续时长（ms）；未置位返回 0。发射侧用 now-lastSince 计算。
inline uint32_t dashOtaGuardActiveSinceMs() { return otaActiveSinceMs; }
inline uint32_t dashOtaGuardStaleClears() { return otaStaleClears; }
inline uint32_t dashOtaGuardLastSeenMs() { return otaLastSeenMs; }
