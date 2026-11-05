# CRASH-1710 addr2line 判读定案（2026-11-05，判读者 = MacServer Claude）

ELF = .pio/build/p4_3can/firmware.elf（SHA256 ff4890c171ff… 与现场一致 = e2e3688 精确构建）
core = /tmp/core_1710.elf（由 coredump_1710_tcbtrip.bin 经 esp_coredump 转出，718,300B）

## 1. Backtrace 定位（addr2line 实测）

| PC | 解析 | 角色 |
|---|---|---|
| 0x400d6c82 | `vPortTaskWrapper` port.c:262 | 守卫巡逻任务自己的包装器（**非写者**——推翻"末位=写者调用点"假设） |
| 0x4000481e | `dashHeapGuardCheckTcbs` ← `dashHeapGuardPatrolTask` :461 | 检测入口 |
| 0x40004778 | `dashHgTcbEval` :190/200（内联进 Scan） | 判决 |
| 0x40004542 | `dashHgTcbScan` :376 | 复核 |
| 0x4000453f | `dashHgTcbTrip` :370 → abort() | 绊网引爆 |

**结论：整条 backtrace = 守卫的确定性检测链。** 腐写是异步发生的，巡逻在下一个扫描周期
才发现——引爆时刻写者上下文早已消失，本 dump 不含写者。这是绊网设计的预期行为（保住
确定性的 abort 与干净 coredump，代价是写者现场）。

## 2. 寄存器现场 = 正常 panic 路径（无劫持）

- MTVEC 0x4ff20003 = `_vector_table`：P4 的 RISC-V 向量表本来就在 .iram0.text 头部（0x4ff20000），低 2 位 03 = 模式位——**合法，未被改写**
- MEPC 0x4ff2b0c4 = `panic_abort`（panic.c:473）：IDF abort→panic 的标准入口（intentional
  illegal instruction），MCAUSE=2 由此而来——**非控制流劫持**
- 0x4ff2xxxx 波段全部落在 `.iram0.text`（0x4ff20000–0x4ff319c8，可执行段）——不是"DRAM 乱跳"
- T0–T6 连续 ASCII = TRIP 行 printf 格式化的临时寄存器残影——良性

## 3. 被腐写的真身（GDB 直读 core + .bss 取证结构体）

```
dashHeapGuardForensic = {magic="HGD2", reason=0x304, expected=0x4ff7fe6c,
  actual=0x4ff7fef0, tickMs=0x39c197(≈63min), armed=6,
  tcbNameA="can_", tcbNameB="rt", tcbTaskNumber=21}
reason 解码：verdict=4(kDashHgTcbTripStackBase) | slot=3 | member=0
baseline[3] = {handle=0x4ff816f0, taskNumber=21, name="can_rt", stackBase=0x4ff7fe6c}
```

**verdict 4 = `kDashHgTcbTripStackBase`：`can_rt`（CAN 运行时任务）TCB 的 `pxStackBase`
字段 0x4ff7fe6c → 0x4ff7fef0（+0x84）。**

关键物证：x/s 0x4ff7fef0 → **0xA5 重复填充**（IDF 栈填充 = 从未触碰的栈区）。即：

- 新值的形状**恰好是该任务自己的合法 pxTopOfStack**（栈深恰好用 0x84 字节时的保存栈指针）
- 写者行为模式 = **"把该写进 pxTopOfStack 的值，写进了 pxStackBase 字段"** ——同结构
  相邻指针字段的一次错位写入，非随机垃圾
- 0x84 指纹第 N 次复现（bench httpd idx=12 +0x84 → 本次 can_rt idx=3 +0x84），跨固件稳定

## 4. 上下文与定性

- 崩溃前串口 `[CAN] Bus OFFLINE` 循环 = 车打盹、总线超时——与 round-1「网络/事件边沿邻接」线索同向
- can_rt 当时 idle（最深 0x84 字节以下全是 0xA5）——腐写发生在任务静止窗口
- 运行时 ≈63 分钟（tickMs 0x39c197）——台架 83min 同量级的"长时闲置后"画像
- **定性：九轮取证家族（控制流/寄存器污染）的 TCB 字段变体，首次在实车现场被完整捕获；
  检测链（baseline→trip→forensic→coredump→干净重启）全链路按设计工作。**
- 写者身份不在本 dump 中；继续追 = 已定夺停止的取证战役（产品路线优先）。若未来要追，
  金钥匙 = 这份 core（写者若是周期性存在，下一次可在 can_rt TCB 周边下 write-watchpoint）。

## 5. 对产品路线的影响

- v1 功能路径与本次腐写无交互（autoshift 全程 off；can_rt 的 SBW 快照在 LISTEN_ONLY 下工作正常）
- 收工时刻崩溃 = 断电拆线前，八段证据全部有效，无数据损失
- 台账终态 [4,0] PANIC 与本判读一致；下次上车照常（崩溃非指令性、非必现）
