# Design: Heap Guard FPU-Carve Awareness (守卫知法)

**Date**: 2026-11-05 · **Status**: 已批准（Jordan 2026-11-05；2026-11-06 判据修订：连环 carve 时 `sa_tcbstack` 保持最初基址，互锁由 `==` 改 `≤`）· **Size**: small change
**Repo**: LILYGO-T-2Can-4.5.0-beta01 (branch p4-3can-board-port)

## Intent & Why

2026-11-05 P0 定案（见 memory `fpu-lazy-save-guard-false-trip-20261105`）：追查五案的
"TCB pxStackBase(+0x30) := base+0x84" 不是腐写，而是 ESP-IDF RISC-V 端口的 **FPU 惰性
上下文保存**：任务生涯第一次执行浮点指令时，`pxPortGetCoprocArea()`（`portable/riscv/port.c:787`）
在任务栈底 carve `sizeof(RvFPUSaveArea)=132` 字节并**合法地**把 pxStackBase 上推
（让 FreeRTOS 溢出检查跳过 FPU 上下文区）。can_rt 的首浮点 = 0x488 方向盘角度
`((raw&0x7FFF)-16384)*0.1f`（`mcp2515_dashboard.h:1307`，需真实 CAN 数据 → 实车独有）；
httpd 的首浮点 = fps/temp JSON 计算——所以**台架永不复现、实车必触发**。

heappatrol（250ms 圈）把该合法写入当腐写 → panic → **自伤**。1710/2044/台架trip#1
三次"崩溃"全是这个机制。当前板上 20fa320 的 trap v3 武装在同一字段上——实车开车后
秒级必然自崩，**任何下次实车前必须处置**。

方案 A（Jordan 已选）：教守卫识别 IDF 的合法 carve，其余真腐写照旧绊网。

## Goals

1. heappatrol 对合法 FPU（及未来 DSP/PIE/HWLP）carve：识别 → 更新基线 → 计数，
   **不绊、不崩**。
2. 拆除 TCB+0x30 上的 watchpoint 武装（合法写者存在，watchpoint 必假阳性）。
3. carve 事件留现场证据（串口一行 + 计数），让下次实车**用日志而非崩溃**动态确认定案。
4. 名字改写 / pxTopOfStack 越界 / 非 carve 形状的 pxStackBase 改写：**照旧真绊**。

## Non-Goals

- 不改 IDF、不改任务栈尺寸、不动浮点代码路径。
- 不追认任何"栈基址回退/乱跳"——只有 sa 交叉验证通过的**前向** carve 被接受。
- 39s 家族（50fc467，守卫诞生前）与 rounds 6/7 旧 dump 复核：另行任务，不在本改。

## Constraints

- 门禁顺序：pytest → native 矩阵 → p4_3can + lilygo_t2can_dual 双构建（串行勿并行）。
- `DASH_HEAP_GUARD_V2` 仍仅 P4 编入；T2CAN 黄金基线零行为差异必须保持。
- 判决核心保持纯函数（`dashHgTcbEval`），native 测试直接钉语义。
- pio 构建后须还原/重戳 `include/web/mcp2515_dashboard_ui.h`。
- 烧录：OTA 裸 body POST `/update`，烧前问 Jordan。

## Approach（已选 A；否掉的备选）

- **A（选）守卫知法**：pxStackBase 失配时先用 IDF 自己的账本（栈顶
  `RvCoprocSaveArea`）交叉验证，通过则收养，不过则照旧绊。
- B 拆除 TCB 绊网：最简单，但放弃对真腐写的全部哨兵能力——被否。
- C 不改先上车拿 watchpoint 写者 PC：动态铁证最硬，但烧掉一趟 KA-750 任务——被否
  （A 的 carve 日志本身就是无伤亡的动态确认）。

## Design

### 1. 新判决码 + 纯函数分类器（`include/dash_heap_guard.h`）

```c
enum { ..., kDashHgTcbAdoptCarve = 6 };  // 合法协处理器 carve：换基线不绊
```

`dashHgTcbEval` 签名追加两个值参数（由调用方读好传入，保持纯函数）：

```c
inline int dashHgTcbEval(const DashHgTcbBase *base, uint32_t curNumber,
                         uint32_t curNameA, uint32_t curNameB,
                         uint32_t curStackBase, uint32_t curTopOfStack,
                         uint32_t dramEnd,
                         uint32_t saTcbstack, uint32_t saAllocator)   // 新
{
    if (curNumber != base->taskNumber) return kDashHgTcbAdopt;
    if (curNameA != base->nameA || curNameB != base->nameB) return kDashHgTcbTripName;
    if (curStackBase != base->stackBase) {
        // IDF 惰性协处理器 carve：sa 账本互锁（IDF pxPortGetCoprocArea 语义）。
        // 注意 sa_tcbstack 只在 allocator==0 时写一次（=最初基址）——连环 carve
        // （先 FPU 后 PIE）时它低于已更新的基线，故判据是 ≤ 而非 ==。
        if (saAllocator != 0 && saAllocator == curStackBase
            && curStackBase > base->stackBase
            && saTcbstack != 0 && saTcbstack <= base->stackBase)
            return kDashHgTcbAdoptCarve;
        return kDashHgTcbTripStackBase;
    }
    if (curTopOfStack < curStackBase || curTopOfStack >= dramEnd) return kDashHgTcbTripTop;
    return kDashHgTcbOk;
}
```

**为什么用 sa 交叉验证而不是硬编码 +132**：FPU=132 只是三协处理器之一（P4
`SOC_CPU_COPROC_NUM=3`；PIE=56/align16 等）。IDF 在每个任务栈顶放
`RvCoprocSaveArea{enable, sa_tcbstack, sa_allocator, sa_coprocs[3]}`（24B，
`align16(pxEndOfStack-24)`）。carve 的不变式是：`sa_tcbstack` = **最初**的栈基址
（只在 allocator==0 时写一次，连环 carve 时不再变）、`sa_allocator` = carve 后的
新基址（游标逐次前移）。账本互锁（游标==新值 && 前向 && 原始基址≤基线）能一致
通过的改写就是 IDF 本人所为；攻击者要骗过它得同时改写 TCB+0x30、TCB+0x48 和
栈顶账本——超出本威胁模型。尺寸无关 = 对任何协处理器序列（FPU 后再 PIE）都
成立，基线逐次前移即可。

### 2. ESP 侧薄胶水（`dashHgTcbScan`）

stackBase 失配判定前，读 sa 并传给纯函数：

```c
uint32_t end = handle ? *(volatile uint32_t *)(uintptr_t)(handle + 0x48) : 0;  // pxEndOfStack
uint32_t sa  = (end - 24u) & ~15u;
uint32_t saTcb = 0, saAllo = 0;
if (end > stackBase && (sa & 0xfff00000u) == 0x4ff00000u   // 内部 DRAM 段才解引用
    && sa + 12u <= 0x4ffc0000u) {
    saTcb  = *(volatile uint32_t *)(uintptr_t)(sa + 4);
    saAllo = *(volatile uint32_t *)(uintptr_t)(sa + 8);
}
```

- `+0x48` = pxEndOfStack：与 IDF 自身反汇编一致（`vPortTCBPreDeleteHook` 内联块
  `lw 72(s1)`），注释里钉证据。
- 指针越界不解引用（防 TCB+0x48 本身被腐写的二次伤害）→ saTcb/saAllo 留 0 →
  分类器自然判 trip。
- 扫描循环里 `v == kDashHgTcbAdoptCarve`：`base->stackBase = curStackBase`、
  `dashHgTcbCarves++`、**首个 carve 串口一行**：
  `[heap_guard] TCB carve accepted <name> 0x<old> -> 0x<new> (IDF lazy coproc save)`
  然后 `continue`（不 suspicious、不需二次确认——sa 互锁本身就是确认）。

### 3. 拆陷阱武装（同文件）

`dashHgTcbAdopt()` 里两段 `dashHgTcbTrapArm`（httpd/can_rt）删除；退役路径
`dashHgTcbTrapClear` 保留（无害空操作）；`dash_tcb_trap.h` 机器保留编译（历史 +
未来复用）。`dashHgTrapRearms` 计数保留（恒 0）。boot 不再打 "TCB write-trap armed"
两行，改为打一行：
`[heap_guard] TCB sentinel carve-aware (IDF lazy coproc save exempt), patrol 250ms`。

### 4. 遥测

`heap_guard_status`（串口，`mcp2515_dashboard.h:7327`）追加 `carves=<n>`。
carve 日志行即实车动态确认载体：开车后应见 can_rt 一行（0x488 首浮点）、
iPad 连上后应见 httpd 一行（fps 首浮点）——**看到日志 + 零崩 = 定案闭环**。

### 5. 测试计划

- **native**（`test/test_native_heap_guard/test_heap_guard.cpp`）新增：
  carve 互锁通过 → `kDashHgTcbAdoptCarve`（FPU +132 与假想 PIE 尺寸各一例）；
  sa_tcbstack≠旧值 / sa_allocator≠新值 / allocator=0 / 新值回退 → 照旧
  `kDashHgTcbTripStackBase`；既有 6 断言随新签名补两参（合法场景传 0,0 语义不变）。
- **合同**（`test/test_tcb_trap_contract.py`）：改钉"收养不再武装陷阱/boot 打
  carve-aware 行"；carve 收养路径新钉。
- 门禁：pytest 全量 → native 矩阵 → 双构建。

### 6. 发布

commit（英文）→ Jordan 批 → OTA 板 2 → 台架验证：`heap_guard_test tcb` 仍真绊
（人工腐写非 carve 形状）+ 串口 carve-aware 行在场 → 上车。任务书 §5"陷阱 v3
观察哨"段落改为"carve 日志观察哨"（atlas-field-kit 文档随行更新）。

## 留给实现者的裁量

- carve 日志是否限流（每任务一行 vs 每次）——默认每任务一行。
- `dashHgTcbCarves` 溢出语义——uint16 截断即可。
