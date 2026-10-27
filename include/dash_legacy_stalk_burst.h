#pragma once

// ── Legacy Model X (MCU2 Intel) 0x292 STW_ACTN_RQ 拨杆爆闪 ──────────────
//
// Fresh 平台（Model 3 / S-X MCU3）的灯光爆闪走 0x249 SCCM_leftStalk；但在 Legacy
// Model X 上 0x249 是 DAS_visualDebug（AP 调试遥测），注入不会闪灯。Legacy 的真
// 实拨杆帧是 0x292 STW_ACTN_RQ（BDY 车身总线）——见 tesla_can_explorer_local 的
// datasets/modelsx_intel 权威定义（2026.2.3 MCU2-SX 反编译）。
//
// 信号 bit 布局取自 opendbc 的 STW_ACTN_RQ（tesla_powertrain.dbc 等，三份一致），
// 全部 Intel 小端：
//   HiBmLvr_Stat    : data[2] bit 2-3   (0=IDLE,1=HIBM_ON_PSD,2=HIBM_FLSH_ON_PSD,3=SNA)
//   TurnIndLvr_Stat : data[2] bit 0-1   (0=IDLE,1=LEFT,2=RIGHT,3=SNA)
//   MC_STW_ACTN_RQ  : data[6] bit 4-7   (4-bit rolling counter)
//   CRC_STW_ACTN_RQ : data[7] 整字节     (8-bit CRC，覆盖 data[0..6])
//
// ⚠ 台架待验证项（编译期可调）：
//   1. CRC 多项式：社区答案不一，默认用最常见的车规 SAE J1850 CRC8
//      (poly 0x1D / init 0xFF / xorout 0xFF)。若台架实帧不符，改下方三个 constexpr。
//   2. 仲裁 ID：opendbc 把 STW_ACTN_RQ 标在 0x238，explorer 标在 0x292——以 explorer
//      为准，台架用真实帧复验。
//
// 纯逻辑、无 CAN 依赖，可在 native 单测中锁定 bit 语义与 CRC 行为。

#include <cstdint>
#include <cstring>

// Legacy 0x292 HiBmLvr_Stat（远光拨杆）
enum LegacyHibmStatus : uint8_t
{
    kLegacyHibmIdle = 0,  // IDLE
    kLegacyHibmOnPsd = 1, // HIBM_ON_PSD（常亮远光）
    kLegacyHibmFlash = 2, // HIBM_FLSH_ON_PSD（远光闪/超车闪 —— 爆闪用这个）
    // 3 = SNA（不可用，保留）
};

// Legacy 0x292 TurnIndLvr_Stat（转向灯拨杆）
enum LegacyTurnStatus : uint8_t
{
    kLegacyTurnIdle = 0,  // IDLE
    kLegacyTurnLeft = 1,  // LEFT
    kLegacyTurnRight = 2, // RIGHT
    // 3 = SNA
};

// ── CRC8 参数（默认 SAE J1850，台架不符则改这里）──────────────────
#ifndef LEGACY_STALK_CRC_POLY
#define LEGACY_STALK_CRC_POLY 0x1D
#endif
#ifndef LEGACY_STALK_CRC_INIT
#define LEGACY_STALK_CRC_INIT 0xFF
#endif
#ifndef LEGACY_STALK_CRC_XOROUT
#define LEGACY_STALK_CRC_XOROUT 0xFF
#endif

namespace legacy_stalk
{
// 8-bit CRC over data[0..6]，写入 data[7]。MSB-first（非反射），SAE J1850 默认。
inline uint8_t crc8(const uint8_t data[7])
{
    uint8_t crc = LEGACY_STALK_CRC_INIT;
    for (uint8_t i = 0; i < 7; ++i)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b)
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ LEGACY_STALK_CRC_POLY)
                               : static_cast<uint8_t>(crc << 1);
    }
    return static_cast<uint8_t>(crc ^ LEGACY_STALK_CRC_XOROUT);
}

// ── 字段读取（小端 bit 解析，全部掩码到位）─────────────────────────
inline uint8_t hibmStatus(const uint8_t f[8]) { return (f[2] >> 2) & 0x03; }
inline uint8_t turnStatus(const uint8_t f[8]) { return f[2] & 0x03; }
inline uint8_t counter(const uint8_t f[8]) { return (f[6] >> 4) & 0x0F; }

// ── 字段写入（仅改本字段，保留其它位）─────────────────────────────
inline void setHibm(uint8_t f[8], uint8_t hibm)
{
    f[2] = static_cast<uint8_t>((f[2] & 0xF3) | ((hibm & 0x03) << 2));
}
inline void setTurn(uint8_t f[8], uint8_t turn)
{
    f[2] = static_cast<uint8_t>((f[2] & 0xFC) | (turn & 0x03));
}
inline void setCounter(uint8_t f[8], uint8_t cnt)
{
    f[6] = static_cast<uint8_t>((f[6] & 0x0F) | ((cnt & 0x0F) << 4));
}
inline void updateCrc(uint8_t f[8]) { f[7] = crc8(f); }

// 端到端：从最近捕获的真实帧模板构造一帧注入。
//   tmpl  : 最近真实 0x292（保留雨刷/巡航/16 按键等位，避免副作用）
//   out   : 输出帧（8 字节）
//   cnt   : 计数器（一般 = 上一个真实帧 counter + 1）
//   hibm  : 远光拨杆状态（爆闪激活=kLegacyHibmFlash，释放=kLegacyHibmIdle）
//   turn  : 转向灯拨杆状态（一般保留 kLegacyTurnIdle，除非要同时打转向）
// 拷贝模板 → 写 hibm/turn/counter → 重算 CRC。
inline void buildFrame(const uint8_t tmpl[8], uint8_t out[8], uint8_t cnt,
                       uint8_t hibm, uint8_t turn)
{
    memcpy(out, tmpl, 8);
    setHibm(out, hibm);
    setTurn(out, turn);
    setCounter(out, cnt);
    updateCrc(out);
}
} // namespace legacy_stalk

// 最近真实 0x292 模板捕获（main.cpp 在每帧 RX 时刷新）。新鲜度由调用方传入 nowMs
// 判定，模块本身不持有时间依赖，便于 native 单测。
struct LegacyStalkTemplate
{
    uint8_t frame[8] = {};
    uint32_t lastMs = 0;
    bool valid = false;

    void capture(const uint8_t f[8], uint32_t nowMs)
    {
        memcpy(frame, f, 8);
        lastMs = nowMs;
        valid = true;
    }
    // 模板是否在 maxAgeMs 内（参照 V8.2 LIGHTING_TEMPLATE_MAX_AGE_MS=300ms）。
    bool isFresh(uint32_t nowMs, uint32_t maxAgeMs) const
    {
        return valid && lastMs != 0 && (nowMs - lastMs) <= maxAgeMs;
    }
    // 是否处于真实帧的"尾流窗口"内：即上一帧真实帧后 [wakeMinMs, wakeMaxMs] 之间。
    // 用于"骑尾流注入"——在此窗口注入可避开下一个真实帧（同 ID 不同数据会位错误→TEC→bus-off）。
    bool inWake(uint32_t nowMs, uint32_t wakeMinMs, uint32_t wakeMaxMs) const
    {
        if (!valid || lastMs == 0)
            return false;
        uint32_t age = nowMs - lastMs;
        return age >= wakeMinMs && age <= wakeMaxMs;
    }
    uint32_t ageMs(uint32_t nowMs) const { return valid && lastMs != 0 ? (nowMs - lastMs) : 0xFFFFFFFFu; }
    void invalidate()
    {
        valid = false;
        lastMs = 0;
    }
};
