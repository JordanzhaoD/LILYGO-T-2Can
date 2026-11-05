#pragma once
// dash_heap_guard.h — 堆腐巡逻绊网 v2 + TCB 哨兵 v3
//
// v2（2026-11-02 实车崩溃取证第 3 轮）：三次实车 coredump 考古定案：
// mcause=1 / mepc=0 / mtval=0 = CPU 跳零；can_rt 在 appLoop 的驱动/处理器
// vtable 间接调用点被脱轨（t0=0x4fc1a588、t1=0xf 跨两个构建完全一致）。
// HEAP_POISONING_COMPREHENSIVE canary 在自由时从未触发 → 腐坏类 =
// freelist/边界标签或活块内容，canary 检查看不见。三件套：
//   1) 静态指针哨兵（稳定槽 + 闭集槽）——指针/对象头字(vtable) 与开机
//      快照比对，can_rt 每圈查，巡逻任务 250ms 兜底；
//   2) 堆巡逻任务：heap_caps_check_integrity(MALLOC_CAP_INTERNAL) 每 250ms
//      走内部 DRAM 堆结构，结构性腐坏当场 abort；
//   3) 取证结构体落 .bss：CAPTURE_DRAM 的 coredump 直接可见绊网
//      原因/期望/实际值，腐坏瞬间 abort → coredump 全景定格。
//
// v3（2026-11-03 第 4/5 轮定案）：六哨兵 + 堆巡逻在实车零触发——腐坏落在
// 哨兵间隙。第 5 轮铁证 = TCB 尸体：can_rt 的任务控制块躺在 web 任务
// 8KB 栈块内部、全 0xA5（FreeRTOS 新栈填充字节）。活结论是寄存器/上下文
// 保存区级污染；v3 直接看守每个活任务的 TCB 本体：
//   - 名字头 8 字节（pcTaskName 指进 TCB 内部——读它就是读 TCB）；
//   - pxStackBase（等值）；
//   - pxTopOfStack（TCB+0，内核保证的首成员）必须在 [pxStackBase, 内部
//     DRAM 顶) 内——栈向下长，低于基址或出段即被毒化的上下文指针。
// 任务句柄会被回收：xTaskNumber（每次创建唯一递增）区分「同址新任务」
// 与「活 TCB 被改写」——号变=重新收养；同号而名字/栈字段变=绊。
// 绊前二次确认（重枚举仍在场且仍失配才 abort）：删任务竞态（gw_dns
// 自删）只该退役基线，不该假绊。
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

// ── TCB 哨兵决策核（v3, 2026-11-03）──────────────────────────────────
// 纯函数：输入基线快照与当前观测，输出判决。原生测试钉死语义。

enum DashHgTcbVerdict
{
    kDashHgTcbOk = 0,
    kDashHgTcbAdopt = 1,         // 句柄未知或号变（回收再利用）：换基线
    kDashHgTcbTripName = 3,      // 同号而名字头 8 字节被改写
    kDashHgTcbTripStackBase = 4, // 同号而 pxStackBase 被改写
    kDashHgTcbTripTop = 5,       // pxTopOfStack 越出 [pxStackBase, dramEnd)
    kDashHgTcbAdoptCarve = 6,    // IDF 惰性协处理器 carve：换基线不绊（v4）
};

struct DashHgTcbBase
{
    uint32_t handle;
    uint32_t taskNumber; // xTaskNumber：每次任务创建唯一递增
    uint32_t nameA;      // 名字头 4 字节（小端打包）
    uint32_t nameB;      // 名字 5-8 字节
    uint32_t stackBase;  // pxStackBase 快照
};

// 打包名字头 8 字节；NUL 后补零——TCB 名字缓冲区尾部的相邻字段字节
// 不进快照（否则 volatile 邻居会造成假绊）。
inline void dashHgPackName8(const char *name, uint32_t *a, uint32_t *b)
{
    uint32_t va = 0, vb = 0;
    const char *p = name;
    for (int i = 0; i < 8; ++i)
    {
        uint32_t byte = 0;
        if (p && p[i] == '\0')
            p = nullptr; // 之后全补零
        if (p)
            byte = (uint32_t)(uint8_t)p[i];
        if (i < 4)
            va |= byte << (8 * i);
        else
            vb |= byte << (8 * (i - 4));
    }
    *a = va;
    *b = vb;
}

// dramEnd = 内部 DRAM 段顶（P4: 0x4ffc0000，与 dashHgRangeOk 同段）。
// saTcbstack/saAllocator = 栈顶 RvCoprocSaveArea 账本（调用方读好传入，
// 保持纯函数）：sa_tcbstack 只在 allocator==0 时写一次（=最初基址）、
// sa_allocator = carve 后新基址（游标逐次前移）。
inline int dashHgTcbEval(const DashHgTcbBase *base, uint32_t curNumber,
                         uint32_t curNameA, uint32_t curNameB,
                         uint32_t curStackBase, uint32_t curTopOfStack,
                         uint32_t dramEnd,
                         uint32_t saTcbstack, uint32_t saAllocator)
{
    if (curNumber != base->taskNumber)
        return kDashHgTcbAdopt; // 同址新任务（句柄回收）：重新收养
    if (curNameA != base->nameA || curNameB != base->nameB)
        return kDashHgTcbTripName;
    if (curStackBase != base->stackBase)
    {
        // IDF 惰性协处理器 carve：sa 账本互锁（pxPortGetCoprocArea 语义）。
        // 注意 sa_tcbstack 只在 allocator==0 时写一次（=最初基址）——连环
        // carve（先 FPU 后 PIE）时它低于已更新的基线，故判据是 ≤ 而非 ==。
        if (saAllocator != 0 && saAllocator == curStackBase
            && curStackBase > base->stackBase
            && saTcbstack != 0 && saTcbstack <= base->stackBase)
            return kDashHgTcbAdoptCarve;
        return kDashHgTcbTripStackBase;
    }
    if (curTopOfStack < curStackBase || curTopOfStack >= dramEnd)
        return kDashHgTcbTripTop;
    return kDashHgTcbOk;
}

// 绊网仅编入 P4 构建（T2CAN 黄金基线零行为差异）：p4_3can build_flags
// 定义 DASH_HEAP_GUARD_V2。纯决策核无条件编译（原生测试钉死语义）。
#if defined(ESP_PLATFORM) && !defined(NATIVE_BUILD) && defined(DASH_HEAP_GUARD_V2)
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "dash_tcb_trap.h"

// 取证结构体：.bss 落位，CAPTURE_DRAM coredump 直接捞到。绊网瞬间填好
// 再 abort() —— 不依赖任何控制台输出（车内无监听者）。
// v3 起魔数 "HGD2"（0x32444748）：离线判读器按魔数区分结构版本，尾部
// 新增 tcb 三字仅供 TCB 绊网路径填写。
struct DashHgForensic
{
    volatile uint32_t magic;   // 0x32444748 = "HGD2"
    volatile uint32_t reason;  // verdict | (slotIdx<<8) | (memberIdx<<12)
    volatile uint32_t expected;// 快照值（按 verdict：指针或头字）
    volatile uint32_t actual;  // 当前值
    volatile uint32_t tickMs;  // esp_timer 得到的绊网时刻
    volatile uint32_t armed;   // 已武装槽位数（健康侧写）
    volatile uint32_t tcbNameA;     // v3：绊网任务名字头（当前观测值）
    volatile uint32_t tcbNameB;
    volatile uint32_t tcbTaskNumber;
};
static DashHgForensic dashHeapGuardForensic = {0x32444748u, 0, 0, 0, 0, 0, 0, 0, 0};

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

// ── TCB 哨兵（v3, ESP 侧）───────────────────────────────────────────
// 每 250ms 枚举全任务（uxTaskGetSystemState：sdkconfig 须开
// USE_TRACE_FACILITY）。基线 = 句柄 → {xTaskNumber, 名字头8, pxStackBase}。
// 判决见 dashHgTcbEval。绊前二次确认：重枚举仍在场且仍失配才 abort——
// 删任务竞态（gw_dns 自删后快照悬空）只退役基线，不假绊。真腐坏
// （0xA5 尸体）在两次枚举间不会消失，确认即绊。
#define DASH_HG_MAX_TASKS 24u
static DashHgTcbBase dashHgTcbBaseline[DASH_HG_MAX_TASKS];
static uint8_t dashHgTcbBaselineCount = 0;
static TaskStatus_t dashHgTcbStats[DASH_HG_MAX_TASKS]; // .bss：巡逻栈放不下
static uint16_t dashHgTcbCarves = 0; // v4：已收养的合法协处理器 carve 数

inline int dashHgTcbIndexOf(uint32_t handle)
{
    for (uint8_t i = 0; i < dashHgTcbBaselineCount; ++i)
        if (dashHgTcbBaseline[i].handle == handle)
            return i;
    return -1;
}

inline void dashHgTcbRetire(uint8_t idx)
{
    // 被退役的若是受陷阱保护的任务：先解除槽位——被释放的 TCB 块可能被
    // 堆复用，复用后的合法写会撞上 watchpoint 造成假阳性崩溃。
    const uint32_t rn = dashHgTcbBaseline[idx].handle + 0x30;
    if (rn == dashHgTrapField)
        dashHgTcbTrapClear(DASH_TCB_TRAP_SLOT, dashHgTrapField);
    else if (rn == dashHgTrapField2)
        dashHgTcbTrapClear(DASH_TCB_TRAP_SLOT2, dashHgTrapField2);
    // swap-with-last，保持数组紧凑
    dashHgTcbBaseline[idx] = dashHgTcbBaseline[dashHgTcbBaselineCount - 1];
    dashHgTcbBaselineCount = dashHgTcbBaselineCount - 1;
}

inline void dashHgTcbAdopt(uint32_t handle, uint32_t number,
                           uint32_t nameA, uint32_t nameB, uint32_t stackBase)
{
    if (dashHgTcbBaselineCount >= DASH_HG_MAX_TASKS)
        return; // 满员：跳过收养（不绊——任务数超上限不是腐坏证据）
    DashHgTcbBase &b = dashHgTcbBaseline[dashHgTcbBaselineCount];
    b.handle = handle;
    b.taskNumber = number;
    b.nameA = nameA;
    b.nameB = nameB;
    b.stackBase = stackBase;
    dashHgTcbBaselineCount = dashHgTcbBaselineCount + 1;
    // v4（2026-11-05 P0 定案）：此处不再武装 TCB 写陷阱。被盯字段
    // （pxStackBase）存在合法写者——IDF 惰性协处理器上下文保存
    // （pxPortGetCoprocArea 在栈底 carve 并上推基址；实车 can_rt 首个
    // 0x488 浮点、httpd 的 fps JSON 都会触发）。watchpoint 对它必然
    // 假阳性 → 实车秒级自崩。守卫改为 carve 感知（见 dashHgTcbEval /
    // dashHgTcbScan）；陷阱机器保留编译（历史 + 未来复用），退役解除
    // 路径保留（对零值是无害空操作）。
}

inline void dashHgTcbTrip(int verdict, uint8_t idx, uint32_t expected, uint32_t actual)
{
    dashHeapGuardForensic.reason = verdict | ((uint32_t)idx << 8);
    dashHeapGuardForensic.expected = expected;
    dashHeapGuardForensic.actual = actual;
    dashHeapGuardForensic.tickMs = (uint32_t)(esp_timer_get_time() / 1000);
    dashHeapGuardForensic.armed = dashHgSlotCount;
    dashHeapGuardForensic.tcbNameA = dashHgTcbBaseline[idx].nameA;
    dashHeapGuardForensic.tcbNameB = dashHgTcbBaseline[idx].nameB;
    dashHeapGuardForensic.tcbTaskNumber = dashHgTcbBaseline[idx].taskNumber;
    Serial.printf("[heap_guard] TCB TRIP idx=%u verdict=%u expected=0x%08x actual=0x%08x\n",
                  idx, verdict, expected, actual);
    abort(); // → panic → coredump（CAPTURE_DRAM 带走全景）
}

// 单次枚举内做一轮「观测 + 判决」；confirm=true 时仅在二次确认枚举里
// 允许真正绊网（首轮发现失配先返回 true 让调用方重枚举确认）。
inline bool dashHgTcbScan(bool confirm)
{
    UBaseType_t n = uxTaskGetSystemState(dashHgTcbStats, DASH_HG_MAX_TASKS, nullptr);
    if (n == 0)
        return false;
    if (n > DASH_HG_MAX_TASKS)
        n = DASH_HG_MAX_TASKS;

    bool suspicious = false;
    for (UBaseType_t i = 0; i < n; ++i)
    {
        TaskStatus_t &st = dashHgTcbStats[i];
        uint32_t handle = (uint32_t)(uintptr_t)st.xHandle;
        uint32_t nameA = 0, nameB = 0;
        dashHgPackName8(st.pcTaskName, &nameA, &nameB);
        uint32_t stackBase = (uint32_t)(uintptr_t)st.pxStackBase;
        // TCB+0 = pxTopOfStack（内核保证首成员）：直接读控制块首字
        uint32_t top = handle ? *(volatile uint32_t *)(uintptr_t)handle : 0;
        int idx = dashHgTcbIndexOf(handle);
        if (idx < 0)
        {
            dashHgTcbAdopt(handle, st.xTaskNumber, nameA, nameB, stackBase);
            continue;
        }
        // v4 sa 账本（RvCoprocSaveArea）：end 读自 TCB+0x48（pxEndOfStack，
        // 与 IDF vPortTCBPreDeleteHook 内联 lw 72(s1) 一致）。账本位于
        // align16(end-24)：{enable, sa_tcbstack, sa_allocator, coprocs[3]}。
        // 只有 end/sa 落在内部 DRAM 段才解引用——TCB+0x48 本身被腐写成
        // 野指针时不给巡逻队二次伤害（越界 → 0,0 → 分类器自然判 trip）。
        uint32_t saTcb = 0, saAllo = 0;
        uint32_t end = handle ? *(volatile uint32_t *)(uintptr_t)(handle + 0x48u) : 0;
        uint32_t sa = (end - 24u) & ~15u;
        if (end > stackBase && (sa & 0xfff00000u) == 0x4ff00000u
            && sa + 12u <= 0x4ffc0000u)
        {
            saTcb = *(volatile uint32_t *)(uintptr_t)(sa + 4u);
            saAllo = *(volatile uint32_t *)(uintptr_t)(sa + 8u);
        }
        int v = dashHgTcbEval(&dashHgTcbBaseline[idx], st.xTaskNumber,
                              nameA, nameB, stackBase, top, 0x4ffc0000u,
                              saTcb, saAllo);
        if (v == kDashHgTcbAdoptCarve)
        {
            // IDF 惰性协处理器 carve（首个浮点/DSP/PIE 指令）：先留证据
            // 再换基线。基线更新后同一 carve 不会重报；后续新 carve =
            // 新事件，理应再报一行（can_rt=开车后 0x488 首浮点、httpd=
            // 客户端连上后 fps 首浮点——日志即实车定案闭环）。sa 互锁
            // 本身就是确认：不进 suspicious/二次确认。
            Serial.printf("[heap_guard] TCB carve accepted %s 0x%08x -> 0x%08x (IDF lazy coproc save)\n",
                          st.pcTaskName,
                          (unsigned)dashHgTcbBaseline[idx].stackBase,
                          (unsigned)stackBase);
            dashHgTcbBaseline[idx].stackBase = stackBase;
            ++dashHgTcbCarves;
            continue;
        }
        if (v == kDashHgTcbAdopt)
        {
            // 句柄回收再利用：整槽换新基线
            DashHgTcbBase &b = dashHgTcbBaseline[idx];
            b.taskNumber = st.xTaskNumber;
            b.nameA = nameA;
            b.nameB = nameB;
            b.stackBase = stackBase;
            continue;
        }
        if (v == kDashHgTcbOk)
            continue;
        if (!confirm)
        {
            suspicious = true; // 首轮发现：留给二次确认
            continue;
        }
        // 二次确认仍在场且仍失配 → 真绊
        if (v == kDashHgTcbTripName)
            dashHgTcbTrip(v, (uint8_t)idx, dashHgTcbBaseline[idx].nameA, nameA);
        else if (v == kDashHgTcbTripStackBase)
            dashHgTcbTrip(v, (uint8_t)idx, dashHgTcbBaseline[idx].stackBase, stackBase);
        else
            dashHgTcbTrip(v, (uint8_t)idx, dashHgTcbBaseline[idx].stackBase, top);
    }

    // 退役：本轮枚举里消失的基线条目（任务已删；句柄回收由收养路径兜）
    for (uint8_t i = dashHgTcbBaselineCount; i > 0; --i)
    {
        uint8_t idx = i - 1;
        uint32_t h = dashHgTcbBaseline[idx].handle;
        bool seen = false;
        for (UBaseType_t j = 0; j < n; ++j)
            if ((uint32_t)(uintptr_t)dashHgTcbStats[j].xHandle == h)
            {
                seen = true;
                break;
            }
        if (!seen)
            dashHgTcbRetire(idx);
    }
    return suspicious;
}

inline void dashHeapGuardCheckTcbs()
{
    if (dashHgTcbScan(false))
        dashHgTcbScan(true); // 首轮有失配：重枚举确认后才允许 abort
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
        dashHeapGuardCheckTcbs();
    }
}

// 启动巡逻任务（can_rt 在核 0 → 巡逻钉核 1，can_rt 卡死也能巡）。
inline void dashHeapGuardStart()
{
    xTaskCreatePinnedToCore(dashHeapGuardPatrolTask, "heappatrol", 4096, nullptr,
                            1, nullptr, 1);
    Serial.println("[heap_guard] TCB sentinel carve-aware (IDF lazy coproc save exempt), patrol 250ms");
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

inline void dashHeapGuardBenchTripTcb()
{
    // v3 bench hook: flip the FIRST armed TCB baseline's name head — the
    // live TCB is untouched; the eval mismatch walks the exact same
    // detect → confirm → trip path a real TCB rewrite takes (forensics
    // carry the baseline name + task number for the coredump reader).
    if (dashHgTcbBaselineCount == 0)
    {
        Serial.println("[heap_guard] tcb baseline not armed yet; retry after first patrol tick");
        return;
    }
    dashHgTcbBaseline[0].nameA ^= 0xa5a5a5a5u;
}
#endif // ESP_PLATFORM
