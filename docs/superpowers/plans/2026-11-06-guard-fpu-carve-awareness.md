# Heap Guard FPU-Carve Awareness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 教 heappatrol 识别 ESP-IDF 合法的惰性协处理器栈 carve（TCB pxStackBase 前移），不再误伤自崩；拆除武装在同一字段上的 trap v3 watchpoint；carve 事件以日志+计数留证。

**Architecture:** 判决核心 `dashHgTcbEval` 保持纯函数，新增判决码 `kDashHgTcbAdoptCarve`，用 IDF 自己的栈顶账本（`RvCoprocSaveArea`）互锁判定：`sa_allocator==新值 && 前向 && sa_tcbstack≤基线`。ESP 侧薄胶水在 `dashHgTcbScan` 里范围门控地读账本并传给纯函数；收养即更新基线+计数+串口一行。陷阱武装从 `dashHgTcbAdopt` 删除（合法写者存在 → watchpoint 必假阳性），陷阱机器本体保留编译。

**Tech Stack:** C++（ESP-IDF FreeRTOS，PlatformIO 工程）；native Unity 测试（`native_heap_guard` env）；Python pytest 源码合同测试。

**Spec:** `docs/superpowers/specs/2026-11-05-guard-fpu-carve-awareness-design.md`（计划与规格同行，执行者两份都读）

## Global Constraints

- 工作目录：worktree `/Users/ziwind/my-vibe-project/LILYGO-T-2Can-4.5.0-beta01`，分支 `p4-3can-board-port`。会话 cwd 会漂移——每条命令显式 `cd` 进去。
- 门禁顺序（串行，绝不并行）：pytest 全量 → native 矩阵 → `p4_3can` + `lilygo_t2can_dual` 双构建。
- pio 入口 `~/.platformio/penv/bin/pio`，ESP 构建需 `IDF_MAINTAINER=1`。
- `DASH_HEAP_GUARD_V2` 仅 P4 编入；`lilygo_t2can_dual` 必须零行为差异（只要求构建绿）。
- pio 构建后若 `include/web/mcp2515_dashboard_ui.h` 被 stamp 重写：`git checkout --` 还原（固件 bin 已带 uiBuildId，无需入库）。
- 禁止 `pio run -t upload`；烧录只用 OTA 裸 body `POST /update`（+ `X-File-Name` 头），**且烧前必须问 Jordan**。
- commit 信息纯英文；`.claude/scratch/`、`.superpowers/`、`.agent*` 绝不 commit；push 前必须问 Jordan（每次）。
- 不 bump VERSION（保持 `6.0.0-beta11`）——uiBuildId=commitSHA+构建时间戳已足够区分。

## Review Focus

规格未点名、但最可能咬人的五类输入（每行后括号内为钉住它的测试与所属任务）：

1. **TCB+0x48（pxEndOfStack）本身被腐写成野指针** —— scan 不得解引用 DRAM 段外的 sa；越界时传 (0,0) 让分类器自然判 trip，巡逻队自己不能崩。（Task 2 合同钉范围门 `0x4ff00000`/`0x4ffc0000` + Task 1 native `test_tcb_carve_without_ledger_trips`）
2. **回退形"carve"**（基址下移、账本看似自洽）—— 必须照旧 trip，只有前向被接受。（Task 1 `test_tcb_backward_carve_trips`）
3. **同名窗口内改名 + carve 并存** —— 名字判定在前：即使账本完美，改名仍 TripName，不许 carve 分支遮蔽身份校验。（Task 1 `test_tcb_name_rewrite_beats_valid_carve`）
4. **收养后真溢出**（sp 落进新基线之下 = carve 区被栈写穿）—— 下一圈必须 TripTop，收养不得放宽溢出检查。（Task 1 `test_tcb_top_below_adopted_base_trips`）
5. **账本缺失/过期**（allocator==0 或 ≠ 新值）—— 没有免费通行证，照旧 TripStackBase。（Task 1 `test_tcb_carve_without_ledger_trips` + `test_tcb_carve_ledger_mismatch_trips`）

---

### Task 1: 纯函数判决码 `kDashHgTcbAdoptCarve`（native TDD）

**Files:**
- Modify: `include/dash_heap_guard.h`（枚举 ~151-156；`dashHgTcbEval` ~190-204；ESP 侧 scan 调用点 ~418）
- Test: `test/test_native_heap_guard/test_heap_guard.cpp`

**Interfaces:**
- Consumes: 现有 `DashHgTcbBase{handle,taskNumber,nameA,nameB,stackBase}`、helper `tcbBaseline(name,number,stackBase)`、`kDramEnd=0x4ffc0000u`。
- Produces: 枚举值 `kDashHgTcbAdoptCarve = 6`；新签名
  `int dashHgTcbEval(const DashHgTcbBase *base, uint32_t curNumber, uint32_t curNameA, uint32_t curNameB, uint32_t curStackBase, uint32_t curTopOfStack, uint32_t dramEnd, uint32_t saTcbstack, uint32_t saAllocator)`
  （Task 2 的胶水与 Task 3 的遥测都依赖此签名与判决码）。

- [ ] **Step 1: 写失败测试**

新增 7 个测试 + 既有 6 个 eval 调用点各补 `, 0u, 0u`。数值用实案地址（can_rt 实测 0x4ff7fe6c→0x4ff7fef0，+132=0x84）：

```cpp
void test_tcb_fpu_carve_accepted(void) {
    DashHgTcbBase b = tcbBaseline("can_rt", 12, 0x4ff7fe6cu);
    TEST_ASSERT_EQUAL_INT(kDashHgTcbAdoptCarve, dashHgTcbEval(
        &b, b.taskNumber, b.nameA, b.nameB,
        0x4ff7fef0u /* cur = base+0x84 (RvFPUSaveArea=132) */,
        0x4ff81000u /* top */, kDramEnd,
        0x4ff7fe6cu /* saTcbstack == 最初基址 */, 0x4ff7fef0u /* saAllocator == 新值 */));
}
```

其余 6 个（同构，只列判据）：
- `test_tcb_cumulative_carve_accepted`：baseline=0x4ff7fef0（FPU 后），cur=0x4ff7ff2c（PIE 形状 +60），saTcb=0x4ff7fe6c（**最初**基址 < 基线），saAllo=0x4ff7ff2c → `kDashHgTcbAdoptCarve`。钉连环 carve 的 `≤` 语义。
- `test_tcb_carve_without_ledger_trips`：FPU 同形但 saAllocator=0 → `kDashHgTcbTripStackBase`。
- `test_tcb_carve_ledger_mismatch_trips`：saAllocator=cur+4 → `kDashHgTcbTripStackBase`。
- `test_tcb_backward_carve_trips`：cur=base−0x50，saTcb=saAllo=cur（账本自洽但回退）→ `kDashHgTcbTripStackBase`。
- `test_tcb_name_rewrite_beats_valid_carve`：完美 carve 值 + curNameA 改一字 → `kDashHgTcbTripName`。
- `test_tcb_top_below_adopted_base_trips`：base==cur==0x4ff7fef0、top=0x4ff7fe00（落在新基线下）→ `kDashHgTcbTripTop`。

- [ ] **Step 2: 跑测试确认失败**

```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-4.5.0-beta01 && \
IDF_MAINTAINER=1 ~/.platformio/penv/bin/pio test -e native_heap_guard
```
Expected: FAIL —— 编译错误 `no matching function for call to 'dashHgTcbEval(...)'`（红即是红，编译错也算）。

- [ ] **Step 3: 实现**

`include/dash_heap_guard.h`：枚举加 `kDashHgTcbAdoptCarve = 6`（先核对现有相邻值 Ok=0/Adopt=1/TripName=3/TripStackBase=4/TripTop=5，取下一个空位，设计钉 6）；`dashHgTcbEval` 签名追加 `uint32_t saTcbstack, uint32_t saAllocator`；stackBase 失配分支改为 spec §1 的 carve 分支（逐字采用已批准设计的代码块，注释一并带上）。同时把 ESP gate 内 `dashHgTcbScan` 的 eval 调用点补传 `0u, 0u`（保 ESP 可编译；真胶水 Task 2 做）——native env 不编这段，正确性由 Task 4 双构建兜底。

- [ ] **Step 4: 跑测试确认通过**

同 Step 2 命令。Expected: PASS（既有测试 + 新 7 个全绿）。

- [ ] **Step 5: Commit**

```bash
git add include/dash_heap_guard.h test/test_native_heap_guard/test_heap_guard.cpp
git commit -m "feat(p4): teach heap guard evaluator the IDF lazy-coproc carve verdict"
```

### Task 2: scan 胶水——读 sa 账本 + 收养路径 + 计数/日志（pytest 合同先行）

**Files:**
- Modify: `include/dash_heap_guard.h`（`dashHgTcbScan` ~394-460，ESP gate 内；计数器放 gate 内、与 `dashHgTrapRearms` 同款 static）
- Test: `test/test_tcb_trap_contract.py`

**Interfaces:**
- Consumes: Task 1 的 9 参 `dashHgTcbEval` + `kDashHgTcbAdoptCarve`。
- Produces: `dashHgTcbCarves`（uint16_t 计数，Task 3 的 `heap_guard_status` 读它）；carve 日志行（格式见 Step 3）。

- [ ] **Step 1: 写失败合同测试**

`test/test_tcb_trap_contract.py` 新增 3 个源码钉（读 `include/dash_heap_guard.h` 文本）：
- `test_scan_reads_ledger_with_range_gate`：`dashHgTcbScan` 函数体内存在 `0x4ff00000` 与 `0x4ffc0000` 范围门，且 sa 读值传入 eval（钉 Review Focus #1：越界不解引用）。
- `test_scan_has_carve_adoption_branch`：`kDashHgTcbAdoptCarve` 在 scan 内被处理，分支含 `dashHgTcbCarves` 自增与 `base->stackBase` 更新。
- `test_carve_log_line_format`：源文件含 `"TCB carve accepted"` 与 `"(IDF lazy coproc save)"` 字符串。

运行 `cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-4.5.0-beta01 && python3 -m pytest test/test_tcb_trap_contract.py -q` → 3 个新测试 FAIL（字符串缺失）。

- [ ] **Step 2: 实现胶水**

`dashHgTcbScan` 每任务循环内、eval 调用前插 spec §2 的代码块（逐字采用：读 `handle+0x48` 得 pxEndOfStack → `sa=(end-24)&~15` → 范围门 `(sa & 0xfff00000u) == 0x4ff00000u && sa + 12u <= 0x4ffc0000u` 过了才解引用 sa+4/sa+8，否则 (0,0)；`+0x48` 偏移注释钉 IDF 反汇编证据）。eval 返回值处理新增：

```cpp
if (v == kDashHgTcbAdoptCarve) {
    Serial.printf("[heap_guard] TCB carve accepted %s 0x%08x -> 0x%08x (IDF lazy coproc save)\n",
                  st.pcTaskName, (unsigned)base->stackBase, (unsigned)stackBase);
    base->stackBase = stackBase;      // 先打旧值再换基线
    ++dashHgTcbCarves;
    continue;                          // 不进 suspicious/confirm——sa 互锁即确认
}
```

（基线更新后同一 carve 不会重打日志；第二次 carve = 新事件，理应再打一行——这是"每任务一行"裁量权的自然实现。）

- [ ] **Step 3: 跑合同测试确认通过**

同 Step 1 命令 → PASS（新 3 + 既有全绿）。

- [ ] **Step 4: Commit**

```bash
git add include/dash_heap_guard.h test/test_tcb_trap_contract.py
git commit -m "feat(p4): validate carve via stack-top ledger in the patrol scan"
```

### Task 3: 拆陷阱武装 + boot 行 + `carves=` 遥测（pytest 合同先行）

**Files:**
- Modify: `include/dash_heap_guard.h`（`dashHgTcbAdopt` ~337-375 删两段 `dashHgTcbTrapArm`；`dashHeapGuardStart` ~487 加 boot 行）
- Modify: `include/web/mcp2515_dashboard.h:7327-7331`（`heap_guard_status` 追加 `carves=<n>`）
- Test: `test/test_tcb_trap_contract.py`

**Interfaces:**
- Consumes: Task 2 的 `dashHgTcbCarves`。
- Produces: 无武装的 adopt 路径；boot 行字符串（Task 5 台架验证 grep 目标）；`heap_guard_status` 的 `carves=` 字段。

- [ ] **Step 1: 改合同测试**

`test/test_tcb_trap_contract.py`：
- 删除 `test_arming_keys_on_httpd_name`、`test_second_slot_arms_on_can_rt_name`（钉"武装存在"的两个测试，本次拆的就是武装）。
- 新增 `test_adopt_no_longer_arms_trap`：`dashHgTcbAdopt` 函数体（`inline void dashHgTcbAdopt` 到下一个 `inline` 之间）不含 `dashHgTcbTrapArm(`。
- 新增 `test_boot_line_is_carve_aware`：`dash_heap_guard.h` 含 `"carve-aware (IDF lazy coproc save exempt)"`。
- 新增 `test_status_reports_carves`：`mcp2515_dashboard.h` 的 heap_guard_status 段含 `carves=`。
- 保留：header gate、official API/store-only、slot 打包、readback 校验、地址变更重布防、retire 解除（`dashHgTcbTrapClear` 仍在）、all-harts、WARL、selftest、`tcb_trap_test` DIAG 注册——陷阱机器本体不动，只是不再上膛。
- 文件 docstring 补 v4 一句：TCB+0x30 的合法写者=IDF `pxPortGetCoprocArea` 惰性协处理器 carve（2026-11-05 P0 定案），watchpoint 对它必假阳性，故拆武装、守卫改 carve 感知。

- [ ] **Step 2: 跑合同测试确认失败**

`python3 -m pytest test/test_tcb_trap_contract.py -q` → 新 3 个 FAIL（armed 字样仍在/新字符串缺失）。

- [ ] **Step 3: 实现**

- `dashHgTcbAdopt` 删两段武装（httpd `0x70747468/0x00000064`→slot0；can_rt `0x5F6E6163/0x00007472`→slot1）；退役路径 `dashHgTcbTrapClear` 保留。
- `dashHeapGuardStart()` 在 xTaskCreatePinnedToCore 成功后打一行：
  `Serial.println("[heap_guard] TCB sentinel carve-aware (IDF lazy coproc save exempt), patrol 250ms");`
- `heap_guard_status` 打印追加 `carves=%u`（读 `dashHgTcbCarves`）。

- [ ] **Step 4: 跑合同测试确认通过** → PASS。

- [ ] **Step 5: Commit**

```bash
git add include/dash_heap_guard.h include/web/mcp2515_dashboard.h test/test_tcb_trap_contract.py
git commit -m "feat(p4): disarm the TCB write-trap and report carve telemetry"
```

### Task 4: 全量门禁（串行）

**Files:** 无新改动；验证 Task 1-3 的合并结果。

**Interfaces:**
- Consumes: 前三任务的全部产出。
- Produces: 门禁绿 + 可交付固件 bin（`.pio/build/p4_3can/firmware.bin`）。

- [ ] **Step 1: pytest 全量**

```bash
cd /Users/ziwind/my-vibe-project/LILYGO-T-2Can-4.5.0-beta01 && python3 -m pytest test/ -q
```
Expected: 全绿（基线 484+754 subtests，无新增失败）。

- [ ] **Step 2: native 矩阵（串行）**

```bash
for e in $(awk -F'[][]' '/^\[env:native/{print $2}' platformio.ini); do
  echo "== $e ==" && IDF_MAINTAINER=1 ~/.platformio/penv/bin/pio test -e "$e" || exit 1
done
```
Expected: 每个 env 全绿。

- [ ] **Step 3: 双构建（串行，勿并行）**

```bash
IDF_MAINTAINER=1 ~/.platformio/penv/bin/pio run -e p4_3can && \
IDF_MAINTAINER=1 ~/.platformio/penv/bin/pio run -e lilygo_t2can_dual
```
Expected: 双绿（T2CAN 黄金基线零行为差异=构建零告警新增）。

- [ ] **Step 4: 还原 ui.h stamp**

```bash
git status --short include/web/mcp2515_dashboard_ui.h && \
git checkout -- include/web/mcp2515_dashboard_ui.h
```
（若本就干净，checkout 无害。）

### Task 5: OTA 板 2 + 台架验证 + 文档/证据（每步都有 Jordan 门）

**Files:**
- Create: `evidence/fpu-carve/`（串口日志 + 验证记录）
- Modify: `atlas-field-kit/missions/` 任务书 §5、`atlas-field-kit/FIELD-GUIDE`（trap v3 观察哨段落 → carve 日志观察哨；boot 行参考更新）

**Interfaces:**
- Consumes: Task 4 产出的 `firmware.bin`（uiBuildId=新 commit SHA 前缀+时间戳）。
- Produces: 板 2 运行 carve-aware 固件的台架证据；更新后的现场文档。

- [ ] **Step 1: 问 Jordan 批准 OTA**（不做任何板上操作直到获批）

- [ ] **Step 2: OTA 烧录**（凭证走 shell 变量提取自 FIELD-GUIDE，绝不内联/入库）：裸 body `POST /update` + `X-File-Name` 头。

- [ ] **Step 3: 台架验证序列**（串口全程 `grep -a`）：
  1. boot 行 `[heap_guard] TCB sentinel carve-aware...` 在场；`uiBuildId` 前缀=本次 commit。
  2. curl 仪表盘 `/system_status` 数次（触发 httpd fps 首浮点）→ 期望 `[heap_guard] TCB carve accepted httpd ...` 一行 + 板子存活。
  3. `heap_guard_status` → `carves>=1`。
  4. 最后跑 `heap_guard_test tcb`（人工改名形状）→ 仍真绊 abort（证明真腐写照旧拦截）；板子重启属预期，故放最后。

- [ ] **Step 4: 文档更新**：任务书 §5 "陷阱 v3 观察哨" → "carve 日志观察哨"（can_rt 一行=开车后 0x488 首浮点、httpd 一行=客户端连上后 fps 首浮点；看到日志+零崩=定案闭环）；FIELD-GUIDE boot 行参考同步。

- [ ] **Step 5: 证据 commit + 记忆更新**

固件仓：
```bash
git add evidence/fpu-carve/
git commit -m "evidence(p4): carve-aware guard bench verification"
```
任务书/FIELD-GUIDE 按其所在仓库单独 commit（先 `git -C <该目录> status` 确认位置，勿跨仓 add）。
并更新会话记忆（`fpu-lazy-save-guard-false-trip-20261105` 追加"方案 A 已实施+台架闭环"；MEMORY.md 索引行同步）。

- [ ] **Step 6: push 前问 Jordan**（每次都问，不默认沿用旧批准）。

---

## 执行备注（给实现者）

- 顺序不可换：Task 1 的签名是 2/3 的依赖；Task 4 必须在 5 之前。
- Task 1 Step 3 里 scan 调用点补 `0u, 0u` 是保 ESP 可编译的临时占位，Task 2 换成真值——两个任务都碰同一调用点，review 时注意 Task 2 diff 必须覆盖它。
- carve 分支的判定顺序（number → name → stackBase/carve → top）是刻意的：身份校验优先于收养（Review Focus #3）。
- 若 `dashHgTcbScan` 现有局部变量名与 spec §2 代码块不一致（如 `stackBase`/`st`），以现有命名为准适配，语义不变。
