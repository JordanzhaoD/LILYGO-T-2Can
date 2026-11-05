#pragma once
// dash_3ee_keepalive.h — 0x3EE 载波静默保活（2026-11-05/06 实车根因的修复）。
//
// 为什么：车机 UI 计算机对 0x3EE 是事件驱动+休眠节律——行驶中连续 2Hz
// （2026-09-16 T2CAN 实测最大间隙 1958ms；2026-11-05 P4 行驶段 964ms），
// 但库内机动/停车收尾段转入 2-5s 静默（11-05 采集 91-118s 停车段 6 个间隙；
// 11-06 库内侦查段 30-62% 墙钟静默）。尾流 TX 只能骑原生帧，静默期无帧可
// 骑 → 车机消费端老化 → bit46 + 速度偏移同时回退 = FSD 界面闪烁。
// 已排除：同总线（id 目录两代重合）、recorder 丢帧（0x488 同总线 41Hz 全程
// 零 >500ms 间隙）、我们 TX 引发退避（9 月 196 帧尾流与连续载波共存）。
//
// 本模块只管纯时序策略（静默触发/节奏/模板过期/原生回归让位），门链镜像
// 与 payload 补丁在 LegacyHandler::keepAliveTick（见 handlers.h）。模板三
// 个月实测逐字节静态（b6 是 mux 页标识非计数器、无 CRC 字段）——自由发送
// 无计数器错位风险，这与 0x045（滚动计数器+CRC8）是两种世界。
#include <cstring>
#include "can_frame_types.h"

struct Dash3eeKeepAlivePage
{
    uint32_t lastNativeMs = 0;    // 最近一帧原生（0=从未见过，无模板）
    uint32_t lastKeepAliveMs = 0; // 最近一次保活签发（0=尚未签发）
    uint32_t keepAlives = 0;      // 签发计数（诊断用）
    uint8_t tmpl[8] = {0};        // 最近原生模板（逐字节）
};

struct Dash3eeKeepAlive
{
    // 1800ms：大于行驶期正常间隙（964ms 实测、1958ms 历史），小于车机消费端
    // 老化（实测扛过 1958ms 不丢功能）——保活帧总能在消费端饿死前进场。
    static constexpr uint32_t kSilenceTriggerMs = 1800;
    // 900ms：贴原生每页 ~1Hz 节奏，略密以留余量。
    static constexpr uint32_t kCadenceMs = 900;
    // 10s 无任何原生 = 车机上下文已变（屏幕灭/功能无关），停止刷存在感。
    static constexpr uint32_t kTemplateMaxAgeMs = 10000;

    Dash3eeKeepAlivePage mux0{};
    Dash3eeKeepAlivePage mux1{};
    uint8_t mux0NativeBus = 0;
    uint8_t mux1NativeBus = 0;

    // 预门缓存：RX 路径在任何 gate 判定之前喂（镜像 jitterObserve3ee 的
    // 纪律——门吃了帧，模板也必须保持新鲜）。
    void observeMux0(const uint8_t data[8], uint8_t bus, uint32_t nowMs)
    {
        memcpy(tmpl0(), data, 8);
        mux0.lastNativeMs = nowMs;
        mux0NativeBus = bus;
    }

    void observeMux1(const uint8_t data[8], uint8_t bus, uint32_t nowMs)
    {
        memcpy(tmpl1(), data, 8);
        mux1.lastNativeMs = nowMs;
        mux1NativeBus = bus;
    }

    enum class Action
    {
        None,
        SendMux0,
        SendMux1,
    };

    // 每次调用至多签发一页（mux0 优先）；签发即记 cadence——门关闭的页
    // 完全跳过（不消耗 cadence），门重开立即可发。
    Action tick(uint32_t nowMs, bool mux0GatesOpen, bool mux1GatesOpen)
    {
        if (pageDue(mux0, nowMs) && mux0GatesOpen)
        {
            mux0.lastKeepAliveMs = nowMs;
            ++mux0.keepAlives;
            return Action::SendMux0;
        }
        if (pageDue(mux1, nowMs) && mux1GatesOpen)
        {
            mux1.lastKeepAliveMs = nowMs;
            ++mux1.keepAlives;
            return Action::SendMux1;
        }
        return Action::None;
    }

    bool copyMux0Template(uint8_t out[8]) const
    {
        if (mux0.lastNativeMs == 0)
            return false;
        memcpy(out, mux0.tmpl, 8);
        return true;
    }

    bool copyMux1Template(uint8_t out[8]) const
    {
        if (mux1.lastNativeMs == 0)
            return false;
        memcpy(out, mux1.tmpl, 8);
        return true;
    }

  private:
    uint8_t *tmpl0() { return mux0.tmpl; }
    uint8_t *tmpl1() { return mux1.tmpl; }

    static bool pageDue(const Dash3eeKeepAlivePage &page, uint32_t nowMs)
    {
        if (page.lastNativeMs == 0)
            return false; // 无模板
        const uint32_t silence = nowMs - page.lastNativeMs;
        // 无符号回绕安全：时钟倒跳产生的巨大值被过期门拦截（fail-closed）。
        if (silence < kSilenceTriggerMs)
            return false; // 载波还在（或刚回）
        if (silence >= kTemplateMaxAgeMs)
            return false; // 模板过期 fail-closed
        if (page.lastKeepAliveMs != 0 && nowMs - page.lastKeepAliveMs < kCadenceMs)
            return false; // 上一次保活还在节奏内
        return true;
    }
};
