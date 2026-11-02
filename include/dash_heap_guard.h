#pragma once
// dash_heap_guard.h — 堆腐巡逻绊网 v2（2026-11-02 实车崩溃取证第 3 轮）
//
// 三次实车 coredump 考古定案：mcause=1 / mepc=0 / mtval=0 = CPU 跳零；
// can_rt 在 appLoop 的驱动/处理器 vtable 间接调用点被脱轨（t0=0x4fc1a588、
// t1=0xf 跨两个构建完全一致）。HEAP_POISONING_COMPREHENSIVE canary 在自由
// 时从未触发 → 腐坏类 = freelist/边界标签或活块内容，canary 检查看不见。
// 本绊网三件套（台上证据链闭环，不依赖车内控制台）：
//   1) 静态指针哨兵：稳定槽（appDriver/appHandler/secondary/tertiary 创建
//      后永不变）+ 闭集槽（appActiveHandler/dashHandler 合法切换于
//      handlerPool[0..2] 三成员闭集）——指针/对象头字(vtable) 与开机快照
//      比对，can_rt 每圈查（~50 周期），巡逻任务 250ms 兜底；
//   2) 堆巡逻任务：heap_caps_check_integrity(MALLOC_CAP_INTERNAL) 每 250ms
//      走内部 DRAM 堆结构，结构性腐坏当场 abort（HEAP_TASK_TRACKING 使
//      coredump 里每块带主人任务名，离线指认凶手）；
//   3) 取证结构体 dashHeapGuardForensic 落 .bss：CAPTURE_DRAM 的 coredump
//      直接可见绊网原因/期望/实际值，腐坏瞬间 abort → coredump 全景定格。
//
// 槽位语义（决策核纯函数，原生测试钉死）：
//   稳定槽：未武装+空=OK；未武装+合法段指针=收养(迟到初始化)；武装后
//           指针变零/变值/头字变值 = 绊。
//   闭集槽：空=OK；已知成员头字变值=绊；未知指针（满 3 成员后或越段）=绊。
//   解引用纪律：仅当「已武装 且 cur==快照 且 在段内」或「收养时段内新指针」
//           才读对象头字——野指针永远先绊后读，哨兵自身不引发 load fault。
#include <stdint.h>

#define DASH_HG_MAX_MEMBERS 3u // handlerPool[0..2] 闭集上限

enum DashHgVerdict
{
    kDashHgOk = 0,
    kDashHgRearm = 1,      // 收养（迟到初始化/新池成员），不是绊网
    kDashHgTripNull = 2,   // 武装后指针变零
    kDashHgTripPointer = 3,// 指针偏离快照 / 闭集外未知指针
    kDashHgTripHead = 4,   // 对象头字（vtable）被改写 —— 精确脱轨形状
    kDashHgTripHeapWalk = 5, // 堆结构巡检失败（仅 ESP 巡逻任务）
};

struct DashHgMember
{
    uint32_t ptr; // 收养时的指针值
    uint32_t head; // 收养时的对象头字（vtable）
};

struct DashHgSlot
{
    const char *name;
    uint8_t stable; // 1=稳定槽（单成员，指针永不变）
    uint8_t count;  // 已收养成员数
    DashHgMember mem[DASH_HG_MAX_MEMBERS];
};

// 内部 DRAM 指针合法段（P4: SRAM 映射 0x4ff00000-0x4ffc0000）。
// 堆对象/任务栈全在此段；越段即野指针。
inline bool dashHgRangeOk(uint32_t v)
{
    return v >= 0x4ff00000u && v < 0x4ffc0000u;
}

inline DashHgSlot dashHgSlotStable(const char *name)
{
    DashHgSlot s = {};
    s.name = name;
    s.stable = 1;
    return s;
}

inline DashHgSlot dashHgSlotSet(const char *name)
{
    DashHgSlot s = {};
    s.name = name;
    s.stable = 0;
    return s;
}

// 解引用纪律：只有「已武装 且 cur==快照」才允许读对象头字。
inline bool dashHgDerefAllowed(const DashHgSlot *s, uint32_t cur)
{
    return s->count > 0 && s->mem[0].ptr == cur && dashHgRangeOk(cur);
}

// 稳定槽决策核。curHead 由调用者按 dashHgDerefAllowed 供给（不允许解引用
// 时传 0，此时核不会比较头字）。
inline int dashHgEvalStable(DashHgSlot *s, uint32_t cur, uint32_t curHead)
{
    if (s->count == 0)
    {
        if (cur == 0 || !dashHgRangeOk(cur))
            return kDashHgOk; // 未武装：空/野指针都不收养，继续观察
        s->mem[0].ptr = cur;
        s->mem[0].head = curHead;
        s->count = 1;
        return kDashHgRearm;
    }
    if (cur == 0)
        return kDashHgTripNull;
    if (cur != s->mem[0].ptr || !dashHgRangeOk(cur))
        return kDashHgTripPointer;
    if (curHead != s->mem[0].head)
    {
        if (s->mem[0].head == 0)
        {
            // 收养时调用方无法解引用（纪律禁读）传了 0：活对象 vtable 永不为 0，
            // 第二次读到的真值回填为基线（bench 假绊 #1 的根因修复）。
            s->mem[0].head = curHead;
            return kDashHgOk;
        }
        return kDashHgTripHead;
    }
    return kDashHgOk;
}

// 闭集槽决策核：合法值域 = 已收养成员（≤3）∪ {0}。
inline int dashHgEvalSet(DashHgSlot *s, uint32_t cur, uint32_t curHead)
{
    if (cur == 0)
        return kDashHgOk; // 空指针从来不是腐坏证据（调度处已有空判）
    for (uint8_t i = 0; i < s->count; ++i)
    {
        if (s->mem[i].ptr != cur)
            continue;
        if (curHead != s->mem[i].head)
        {
            if (s->mem[i].head == 0)
            {
                s->mem[i].head = curHead; // 0-基线回填（同稳定槽语义）
                return kDashHgOk;
            }
            return kDashHgTripHead;
        }
        return kDashHgOk;
    }
    if (!dashHgRangeOk(cur))
        return kDashHgTripPointer;
    if (s->count >= DASH_HG_MAX_MEMBERS)
        return kDashHgTripPointer; // 三成员已满：第四个指针=腐坏
    s->mem[s->count].ptr = cur;
    s->mem[s->count].head = curHead;
    s->count = s->count + 1;
    return kDashHgRearm;
}

// 绊网仅编入 P4 构建（T2CAN 黄金基线零行为差异）：p4_3can build_flags
// 定义 DASH_HEAP_GUARD_V2。纯决策核无条件编译（原生测试钉死语义）。
#if defined(ESP_PLATFORM) && !defined(NATIVE_BUILD) && defined(DASH_HEAP_GUARD_V2)
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 取证结构体：.bss 落位，CAPTURE_DRAM coredump 直接捞到。绊网瞬间填好
// 再 abort() —— 不依赖任何控制台输出（车内无监听者）。
struct DashHgForensic
{
    volatile uint32_t magic;   // 0x31444748 = "HGD1"
    volatile uint32_t reason;  // verdict | (slotIdx<<8) | (memberIdx<<12)
    volatile uint32_t expected;// 快照值（按 verdict：指针或头字）
    volatile uint32_t actual;  // 当前值
    volatile uint32_t tickMs;  // esp_timer 得到的绊网时刻
    volatile uint32_t armed;   // 已武装槽位数（健康侧写）
};
static DashHgForensic dashHeapGuardForensic = {0x31444748u, 0, 0, 0, 0, 0};

#define DASH_HG_MAX_SLOTS 8u
static DashHgSlot dashHgSlots[DASH_HG_MAX_SLOTS];
static void *const *dashHgSlotAddrs[DASH_HG_MAX_SLOTS]; // 槽位指针变量地址
static uint8_t dashHgSlotCount = 0;

// 注册一个哨兵槽。slotAddr 指向被监控的指针变量本身（如 (void* const*)
// &appDriver）。unique_ptr 单字布局下 reinterpret 合法（本工具链）。
inline bool dashHeapGuardRegisterSlot(const char *name, void *const *slotAddr, bool stable)
{
    if (dashHgSlotCount >= DASH_HG_MAX_SLOTS)
        return false;
    dashHgSlots[dashHgSlotCount] = stable ? dashHgSlotStable(name) : dashHgSlotSet(name);
    dashHgSlotAddrs[dashHgSlotCount] = slotAddr;
    dashHgSlotCount = dashHgSlotCount + 1;
    return true;
}

inline void dashHgTrip(uint32_t verdict, uint8_t slotIdx, uint8_t memberIdx,
                       uint32_t expected, uint32_t actual)
{
    dashHeapGuardForensic.reason = verdict | ((uint32_t)slotIdx << 8) | ((uint32_t)memberIdx << 12);
    dashHeapGuardForensic.expected = expected;
    dashHeapGuardForensic.actual = actual;
    dashHeapGuardForensic.tickMs = (uint32_t)(esp_timer_get_time() / 1000);
    dashHeapGuardForensic.armed = dashHgSlotCount;
    Serial.printf("[heap_guard] TRIP slot=%u member=%u verdict=%u expected=0x%08x actual=0x%08x\n",
                  slotIdx, memberIdx, verdict, expected, actual);
    abort(); // → panic → coredump（CAPTURE_DRAM 带走全景）
}

// 检查全部槽位。can_rt 每圈调用 + 巡逻任务 250ms 兜底。任何非 Ok/Rearm
// 即绊。头字解引用严格遵循 dashHgDerefAllowed。
inline void dashHeapGuardCheckStatics()
{
    for (uint8_t i = 0; i < dashHgSlotCount; ++i)
    {
        DashHgSlot *s = &dashHgSlots[i];
        uint32_t cur = (uint32_t)(uintptr_t)*dashHgSlotAddrs[i];
        uint32_t head = 0;
        if (s->stable)
        {
            // 解引用纪律：武装后仅「快照指针+段内」可读；未武装收养时对
            // 「段内新指针」读一次头字（rangeOk 即安全门，与闭集收养同标
            // 准）——否则快照头字永远收养成 0，下一圈必假绊（bench 教训）。
            if (dashHgDerefAllowed(s, cur) || (s->count == 0 && dashHgRangeOk(cur)))
                head = *(volatile uint32_t *)(uintptr_t)cur;
            int v = dashHgEvalStable(s, cur, head);
            if (v == kDashHgTripNull || v == kDashHgTripPointer)
                dashHgTrip(v, i, 0, s->mem[0].ptr, cur);
            if (v == kDashHgTripHead)
                dashHgTrip(v, i, 0, s->mem[0].head, head);
        }
        else
        {
            // 闭集槽：解引用前先确认成员资格（野指针永远先绊后读）；
            // 未知但段内且未满员 = 收养候选，读一次头字入快照。
            int known = -1;
            for (uint8_t m = 0; m < s->count; ++m)
                if (s->mem[m].ptr == cur)
                    known = m;
            if (known >= 0 || (dashHgRangeOk(cur) && s->count < DASH_HG_MAX_MEMBERS))
                head = *(volatile uint32_t *)(uintptr_t)cur;
            int v = dashHgEvalSet(s, cur, head);
            if (v == kDashHgTripPointer)
                dashHgTrip(v, i, 0, 0, cur);
            if (v == kDashHgTripHead)
                dashHgTrip(v, i, (uint8_t)known, s->mem[known].head, head);
        }
    }
}

static void dashHeapGuardPatrolTask(void *)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(250));
        // 只走内部 DRAM 堆（~600KB）：证据链全在内部段，PSRAM 20MB 环
        // 每圈全走太热。结构性腐坏（freelist/边界标签）当场返回 false。
        if (!heap_caps_check_integrity(MALLOC_CAP_INTERNAL, true))
            dashHgTrip(kDashHgTripHeapWalk, 0xff, 0xff, 0, 0);
        dashHeapGuardCheckStatics();
    }
}

// 启动巡逻任务（can_rt 在核 0 → 巡逻钉核 1，can_rt 卡死也能巡）。
inline void dashHeapGuardStart()
{
    xTaskCreatePinnedToCore(dashHeapGuardPatrolTask, "heappatrol", 4096, nullptr,
                            1, nullptr, 1);
}

// ── 台架验证钩子（DIAG heap_guard_test，仅台架）──────────────────────────
// static:  直接绊一次静态哨兵路径（复刻「对象头字被改写」形状）。
// heap:    malloc→free→改写已自由块载荷（砸 COMPREHENSIVE canary）→
//          巡逻堆走检当场绊。两者证明「检测→abort→coredump」全链。
inline void dashHeapGuardBenchTripStatic()
{
    // 找第一个已武装稳定槽，直击其对象头字比较路径：改写快照头字制造
    // 真实 TripHead（不碰活对象本身——被监控指针与对象零改动，绊网纯
    // 靠快照失配触发，与真腐坏同一条代码路径）。
    for (uint8_t i = 0; i < dashHgSlotCount; ++i)
    {
        DashHgSlot *s = &dashHgSlots[i];
        if (s->stable && s->count > 0)
        {
            s->mem[0].head ^= 0xa5a5a5a5u; // 快照失配 = 头字被改写的镜像
            return;
        }
    }
}

inline void dashHeapGuardBenchTripHeap()
{
    // Bench lesson (2026-11-02): a FREED block's canary can be reused away
    // within the 250ms patrol gap (small blocks churn fast) — the smash then
    // lands on live data the walk never verifies and nothing trips. A LIVE
    // block's poison tail is durable: leak the block deliberately (bench
    // only) and corrupt its tail region → the walk fails deterministically.
    static uint8_t *benchBlock = nullptr;
    benchBlock = (uint8_t *)heap_caps_malloc(200, MALLOC_CAP_INTERNAL);
    if (!benchBlock)
        return;
    for (int i = 200; i < 216; ++i)
        benchBlock[i] ^= 0xa5; // poison tail (+ possibly next header) → 250ms 内绊
}
#endif // ESP_PLATFORM
