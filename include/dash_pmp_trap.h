// ── 实验 F（崩溃取证，2026-11-04）：PMP 硬件写陷阱 ─────────────────────
// 三次已证实的 TCB 腐写（round-8 + 2026-11-04×2）全部是 httpd 任务的
// pxStackBase 字段（TCB+0x30）被改写 +0x84，且台架五臂（实验 C/D/E/E3，
// ~1055 次请求 × 10 个 boot）无法按需复现。与其继续猜条件，不如让写者
// 一下笔就被硬件抓住：
//
//   两个 RISC-V PMP TOR 表项（14=下界、15=TOR|L|R，无 W）圈住
//   [tcb+0x30, tcb+0x34) 共 4 字节，双 hart 各自武装。任务创建完成
//   之后没有任何合法代码写这个字段 → 任何 store = 被抓的写者，
//   store access fault 精确异常，MEPC = 写者指令 PC，panic + coredump
//   直接给出写者身份（ISR？hosted 回调？哪段代码）。
//
// 合法性边界（为何只圈这 4 字节是安全的）：
// - pxStackBase 仅在任务创建时写一次；武装发生在哨兵收养（创建之后）。
// - 调度器上下文切换写的是 TCB+0（pxTopOfStack），不越界。
// - 哨兵巡逻每 250ms 读该字段——R 允许。
// - 若 httpd 任务被删除（从未发生）：free() 会写已释放块——那也会被
//   陷阱抓住，而"删除 httpd"本身就是异常事件，抓到不冤。
// - 自验（DIAG pmp_trap_test）：故意 store 该字，期望当场 fault 在
//   自验代码自己的 PC——端到端证明陷阱已武装且异常精确。
//
// 与绊网同门（DASH_HEAP_GUARD_V2 = p4_3can build_flags）：T2CAN 黄金
// 基线零行为差异。IDF 6.0.1 的 P4 无公开 PMP API——CSR 直写是 IDF
// 自己 sleep 代码（C6 等）的同款模式；表项选 14/15（P4 固件内无人
// 占用的最高两号）。
#pragma once

#if defined(ESP_PLATFORM) && !defined(NATIVE_BUILD) && defined(DASH_HEAP_GUARD_V2)

#include "esp_ipc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "riscv/csr.h"
#include "riscv/encoding.h"

// 已武装的受保护字段地址（0 = 未武装）。自验命令只打这一个字。
static volatile uint32_t dashHgPmpField = 0;

// 在【当前 hart】武装：14 号表项 A=OFF 只存下界地址，15 号 TOR|L|R
// （W 拒绝）保护 [addr, addr+4)。先写地址寄存器、后写 cfg（手册顺序：
// 避免 cfg 生效瞬间地址陈旧保护错区间）。
static void dashPmpArmCore(void *arg)
{
    const uint32_t addr = (uint32_t)(uintptr_t)arg;
    RV_WRITE_CSR(pmpaddr14, addr >> 2);
    RV_WRITE_CSR(pmpaddr15, (addr + 4) >> 2);
    uint32_t cfg3 = RV_READ_CSR(pmpcfg3);
    cfg3 &= ~((uint32_t)0xFFu << 16); // entry 14：A=OFF（仅当地址下界）
    cfg3 &= ~((uint32_t)0xFFu << 24); // entry 15：清后设
    cfg3 |= ((uint32_t)(PMP_L | PMP_TOR | PMP_R)) << 24;
    RV_WRITE_CSR(pmpcfg3, cfg3);
    __asm__ volatile("fence iorw, iorw" ::: "memory");
}

// 全 hart 武装。返回受保护字段地址；偏移自检失败（字段当前值 ≠
// pxStackBase）返回 0 并拒绝武装——绝不圈一个身份不明的字。
static uint32_t dashHgPmpArm(uint32_t tcb, uint32_t stackBase)
{
    if (tcb == 0 || dashHgPmpField != 0)
        return dashHgPmpField;
    const uint32_t field = tcb + 0x30; // pxStackBase（round-8 coredump 验证）
    if (*(volatile uint32_t *)(uintptr_t)field != stackBase)
        return 0;
    dashPmpArmCore((void *)(uintptr_t)field); // 当前核
    const uint32_t self = (uint32_t)xPortGetCoreID();
    for (uint32_t c = 0; c < (uint32_t)portNUM_PROCESSORS; ++c)
        if (c != self)
            esp_ipc_call_blocking(c, dashPmpArmCore, (void *)(uintptr_t)field);
    dashHgPmpField = field;
    return field;
}

// 台架自验（DIAG pmp_trap_test，仅台架）：对受保护字故意 store。陷阱
// 生效时 store 提交前即 fault，MEPC = 下方这条 store 自己的 PC（本函数
// 内）——coredump 里写者=自验代码即证明「武装+精确」双成立。能打印出
// 第二行 = 陷阱未生效（armed 但没拦住 = 配置 bug，必须修）。
static void dashHgPmpBenchTest()
{
    const uint32_t f = dashHgPmpField;
    if (f == 0)
    {
        Serial.println("[pmp] trap not armed (httpd not adopted yet?)");
        return;
    }
    Serial.printf("[pmp] self-test store to 0x%08x; fault expected at this PC\n",
                  (unsigned)f);
    *(volatile uint32_t *)(uintptr_t)f = *(volatile uint32_t *)(uintptr_t)f;
    Serial.println("[pmp] NO FAULT - trap armed but ineffective!");
}

#endif // gate
