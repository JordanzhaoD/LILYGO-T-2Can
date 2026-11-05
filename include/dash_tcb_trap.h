// ── 实验 F（崩溃取证，2026-11-04）：TCB 写陷阱 v2 —— trigger watchpoint ──
// 三次已证实的 TCB 腐写（round-8 + 2026-11-04×2）全部是 httpd 任务的
// pxStackBase 字段（TCB+0x30）被改写 +0x84，且台架五臂（实验 C/D/E/E3，
// ~1055 次请求 × 10 个 boot）无法按需复现。与其继续猜条件，不如让写者
// 一下笔就被硬件抓住。
//
// 第一版用 RISC-V PMP（TOR 表项圈 4 字节），台架实机判死：P4 启动后
// 读回 pmpcfg0..3 = 0x9d9d9b9b / 0x808b8d80 / 0x8b808b8d / 0x80001d1d
// —— entry0-11 全部 L=1 锁死（IDF 启动内存保护布局），entry15 被 L=1
// 锁成空置（写入被硬件拒绝，读回只剩 IDF 原值），entry12-14 无 L 但被
// 系统运行时动态占用（武装 2.5s 后 entry14 变 NAPOT|RWX）。无可用表项。
//
// v2 换 trigger module（调试器硬件断点那套，与 PMP 独立）：官方 API
// esp_cpu_set_watchpoint()（无调试器连接时直达 rv_utils_set_watchpoint
// —— tselect/tdata1/tdata2 + tcontrol MTE，IDF esp_cpu.h 公开入口）。
// slot 0、4 字节 NAPOT、只盯 store、双 hart 各自武装（trigger CSR 每
// hart 独立）。命中 → breakpoint exception（写前触发，腐写永不落地）
// → panic → backtrace/coredump 直接给出写者 MEPC。
//
// 安全性：watchpoint 只在命中时产生异常，不改变任何访存语义；误伤面
// = 没有合法写者碰这个字段（pxStackBase 仅任务创建时写一次，武装发生
// 在哨兵收养=创建之后；上下文切换写 TCB+0 不越界；httpd 从不删除）。
// P4 每核 3 个 watchpoint（SOC_CPU_WATCHPOINTS_NUM=3），生产固件不
// 占用（semihosting 分支仅在调试器连接时走，板上无调试器）。
//
// 自验（DIAG tcb_trap_test）：故意 store 该字，期望当场 breakpoint
// exception 在自验代码自己的 PC——端到端证明陷阱已武装且精确。
//
// 与绊网同门（DASH_HEAP_GUARD_V2 = p4_3can build_flags）：T2CAN 黄金
// 基线零行为差异。
#pragma once

#if defined(ESP_PLATFORM) && !defined(NATIVE_BUILD) && defined(DASH_HEAP_GUARD_V2)

#include "esp_cpu.h"
#include "esp_err.h"
#include "esp_ipc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "riscv/csr.h"
#include "riscv/rv_utils.h"

// trigger 槽位。P4 每核 3 个（SOC_CPU_WATCHPOINTS_NUM），IDF 生产代码
// 仅在调试器 semihosting 场景使用；板上无调试器 → slot 0/1 空闲。
#define DASH_TCB_TRAP_SLOT 0   // httpd pxStackBase（三次腐写受害者）
#define DASH_TCB_TRAP_SLOT2 1  // can_rt pxStackBase（2026-11-06 早场受害者）

// 已武装的受保护字段地址（0 = 未武装）。自验命令只打 slot0 这一个字。
static volatile uint32_t dashHgTrapField = 0;  // slot 0（httpd）
static volatile uint32_t dashHgTrapField2 = 0; // slot 1（can_rt）
// 重布防计数（诊断用：httpd 每次重启换 TCB +1）。
static volatile uint32_t dashHgTrapRearms = 0;

// 每 hart 的武装验证位图（bit0=core0, bit1=core1；全 1 才宣称 armed）。
static volatile uint32_t dashHgTrapOkMask = 0;

// 在【当前 hart】武装 + WARL 回读验证。回读不过（type≠mcontrol、
// store/machine 位被清、tdata2 编码不符）= 硬件不支持该配置，绝不宣称
// 武装成功（PMP 版教训：写入成功 ≠ 生效）。
// arg 低 2 位编码槽位（字段地址 4 对齐，低 2 位恒 0）。
static void dashTcbTrapArmCore(void *arg)
{
    const uint32_t packed = (uint32_t)(uintptr_t)arg;
    const uint32_t slot = packed & 3u;
    const uint32_t addr = packed & ~3u;
    if (esp_cpu_set_watchpoint(slot, (const void *)(uintptr_t)addr, 4,
                               ESP_CPU_WATCHPOINT_STORE) != ESP_OK)
        return;
    RV_WRITE_CSR(tselect, slot);
    const uint32_t t1 = RV_READ_CSR(tdata1);
    const uint32_t t2 = RV_READ_CSR(tdata2);
    // 期望：type=2（mcontrol）、store 匹配、M-mode 生效、tdata2 为
    // 4 字节 NAPOT 编码（addr 对齐 | 0b01）。
    if (((t1 >> 28) == 2u) && (t1 & TDATA1_STORE) && (t1 & TDATA1_MACHINE) &&
        (t2 == (addr | 1u)))
        dashHgTrapOkMask |= (1u << (uint32_t)xPortGetCoreID());
}

// 在【当前 hart】解除槽位（tdata1=0 关闭 trigger）。
static void dashTcbTrapClearCore(void *arg)
{
    RV_WRITE_CSR(tselect, (uint32_t)(uintptr_t)arg);
    RV_WRITE_CSR(tdata1, 0);
}

// 全 hart 解除某槽位（任务退役时调用，防被释放/复用内存的合法写误伤）。
static void dashHgTcbTrapClear(uint32_t slot, volatile uint32_t &fieldVar)
{
    if (fieldVar == 0)
        return;
    dashTcbTrapClearCore((void *)(uintptr_t)slot);
    const uint32_t self = (uint32_t)xPortGetCoreID();
    for (uint32_t c = 0; c < (uint32_t)portNUM_PROCESSORS; ++c)
        if (c != self)
            esp_ipc_call_blocking(c, dashTcbTrapClearCore, (void *)(uintptr_t)slot);
    fieldVar = 0;
}

// 全 hart 武装（可重布防）。返回受保护字段地址；偏移自检失败（字段当前
// 值 ≠ pxStackBase）或任一 hart 回读验证失败返回 0 并拒绝宣称武装——绝不
// 圈一个身份不明的字、也绝不半武装了还报成功。
//
// 2026-11-06 v3（晚场事故后）：首版一次性布防（dashHgTrapField!=0 早退）
// 在 httpd 网络弹跳重启换 TCB 后失效——陷阱永远盯着首个 TCB 的尸体，晚场
// 崩溃（受害者 httpd#2）由此漏网。现改为：地址变了就重布防（同址幂等）。
// 任务删除由 dashHgTcbTrapClear 解除，消掉复用内存误伤窗口。
static uint32_t dashHgTcbTrapArm(uint32_t tcb, uint32_t stackBase,
                                 uint32_t slot, volatile uint32_t &fieldVar)
{
    if (tcb == 0)
        return 0;
    const uint32_t field = tcb + 0x30; // pxStackBase（round-8 coredump 验证）
    if (fieldVar == field)
        return field; // 同址已武装：幂等快路径
    if (*(volatile uint32_t *)(uintptr_t)field != stackBase)
        return 0;
    dashHgTrapOkMask = 0;
    dashTcbTrapArmCore((void *)(uintptr_t)(field | (slot & 3u))); // 当前核
    const uint32_t self = (uint32_t)xPortGetCoreID();
    for (uint32_t c = 0; c < (uint32_t)portNUM_PROCESSORS; ++c)
        if (c != self)
            esp_ipc_call_blocking(c, dashTcbTrapArmCore,
                                  (void *)(uintptr_t)(field | (slot & 3u)));
    const uint32_t all = (1u << (uint32_t)portNUM_PROCESSORS) - 1u;
    if (dashHgTrapOkMask != all)
        return 0;
    if (fieldVar != 0)
        dashHgTrapRearms = dashHgTrapRearms + 1;
    fieldVar = field;
    return field;
}

// 台架自验（DIAG tcb_trap_test，仅台架）：对受保护字故意 store。陷阱
// 生效时 store 提交前即 breakpoint exception，MEPC = 下方这条 store
// 自己的 PC（本函数内）——coredump 里写者=自验代码即证明「武装+精确」
// 双成立。能打印出第二行 = 陷阱未生效（armed 但没拦住 = 配置 bug，
// 必须修）。
static void dashHgTrapBenchTest()
{
    const uint32_t f = dashHgTrapField;
    if (f == 0)
    {
        Serial.println("[tcb_trap] trap not armed (httpd not adopted yet?)");
        return;
    }
    RV_WRITE_CSR(tselect, DASH_TCB_TRAP_SLOT);
    Serial.printf("[tcb_trap] tdata1=0x%08x tdata2=0x%08x\n",
                  (unsigned)RV_READ_CSR(tdata1), (unsigned)RV_READ_CSR(tdata2));
    Serial.printf("[tcb_trap] self-test store to 0x%08x; breakpoint exception expected at this PC\n",
                  (unsigned)f);
    *(volatile uint32_t *)(uintptr_t)f = *(volatile uint32_t *)(uintptr_t)f;
    Serial.println("[tcb_trap] NO FAULT - watchpoint armed but ineffective!");
}

#endif // gate
