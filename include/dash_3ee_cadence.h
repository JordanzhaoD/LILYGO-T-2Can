#pragma once
// dash_3ee_cadence.h — 4.5.0-beta12 (LittleGong 逆向移植 2): 0x3EE mux0
// 载波节奏观测。纯观测、零行为改动（批准形态=只 JSON 不加 UI 格）。
//
// 为什么（beta08 症状 1「只出现 EAP」= 资格窗内载波缺失）：协调器的资格
// 注入依赖 [t_request, +window) 内出现原生 0x3EE mux0 载波；Tier 1 CSV
// （beta05 f2 轮 A）实测该载波有 ~1.05s 全静默间隙、mux0→mux0 最长 2s，
// 单一窗口仍可能整个落在空窗里。本模块把这条载波的真实节奏显形进
// /status（carrier3ee 块）：lastGapMs / maxGapMs / longGaps（>2s 间隙
// 计数）/ total，现场判读「windowExpired 是否载波节奏问题」不再靠猜。
//
// 过滤口径：id 0x3EE（=1006）、mux0（data[0]&0x07==0）、协调器标定的
// 源总线 CAN_BUS_VEH——与资格注入看的是同一条流。
#include "can_frame_types.h"

static constexpr uint32_t kDash3eeLongGapMs = 2000; // >2s 计一次 longGap

struct Dash3eeCadence
{
    uint32_t total = 0;      // 观测到的 mux0 帧总数
    uint32_t lastMs = 0;     // 最近一帧时刻（0=开机以来从未见过）
    uint32_t lastGapMs = 0;  // 最近一次相邻帧间隔
    uint32_t maxGapMs = 0;   // 最大相邻帧间隔
    uint32_t longGaps = 0;   // 超 kDash3eeLongGapMs 的间隙累计次数

    void observe(const CanFrame &frame, uint32_t nowMs)
    {
        if (frame.id != 0x3EE || frame.dlc == 0) return;
        if ((frame.data[0] & 0x07) != 0) return; // mux0 only
        if (frame.bus != CAN_BUS_VEH) return;    // coordinator source bus
        ++total;
        if (lastMs != 0 && nowMs > lastMs)
        {
            const uint32_t gap = nowMs - lastMs;
            lastGapMs = gap;
            if (gap > maxGapMs) maxGapMs = gap;
            if (gap > kDash3eeLongGapMs) ++longGaps;
        }
        lastMs = nowMs;
    }
};
