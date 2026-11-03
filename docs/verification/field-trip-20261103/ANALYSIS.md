# 实车十轮取证判读 — 2026-11-03（板 2 / 固件 d5b5d28 / esp_hosted 2.13.1）

数据源：MacBook `~/Desktop/car-runs/`（car_diag.py 自动采集），已回传 MacServer `/Users/ziwind/car-runs/`。
判读工具：firmware-d5b5d28.elf + riscv32-esp-elf-gdb（与板上固件同版本）。

## 一、总况

| 轮次 | 崩溃 | 首崩时机(上电后) | 首崩死点 | 第二崩死点 | STA | AP clients |
|---|---|---|---|---|---|---|
| 1 | ✅×2 | ~12s | app_can_task+1016 (main.cpp:367) | 跳NULL | 无 | 0 |
| 2 | ✅×2 | ~22s | 跳NULL | hosted transport assert (0xA5吃掉消息) | 无 | 1 |
| 3 | ✅×2 | ~15s | dashStartAccessPoint String析构 | 跳NULL | 无 | 0 |
| 4 | ✅×2 | ~12s | app_can_task+1016 | 跳NULL | 无 | 0 |
| 5 | ✅×2 | ~12s | app_can_task+1016 | (bt空) | 无 | 0 |
| 6 | ✅×1 | ~22s | 跳NULL | — | 无 | 3 |
| 7 | ✅×2 | ~13s | app_can_task+1016 | 跳NULL | 有 | 0 |
| 8 | ✅×2 | 早 | 跳NULL | 跳NULL | 有 | 0 |
| 9 | ✅×1 | ~16s | dashStartAccessPoint String析构 | — | 有 | 0 |
| 10 | 无（提前拔板收尾，无数据） | | | | | |

- **每轮=双崩循环**：首崩→panic→dump落盘→SW_CPU_RESET→二次boot→二次崩（dump=最后一崩）
- 崩溃后 DIAG 命令无应答；二次崩的 panic 打印常为乱码（assert 消息被 0xA5 吞掉，dump 里才可见）
- run2 出现「无 Guru 打印但 dumpSaved」= 静默崩溃只被 dump 抓到 —— **dump > 串口文本**

## 二、判读矩阵更新（对照上轮两嫌疑）

| 假设 | 判定 | 证据 |
|---|---|---|
| 电源轨（车内杂波） | ❌ **排除** | 9/9 在 MacBook 干净 USB 电下照样崩，节奏不变（12-22s） |
| STA 关联/WiFi 流量 | ❌ **排除** | runs 1-6 未关联、clients=0 也崩 |
| EMI 位翻转 | ⬇️ 降权 | 三总线无 bus-off/错误计数日志；TCB 哨兵六字段 × ~18 崩全零触发（随机位翻转做不到如此偏食）；腐坏模式=整片清零/0xA5 填充=软件写穿特征 |
| **CAN 数据链路软件腐坏** | ⬆️ **头号嫌疑** | 见下 |

## 三、头号嫌疑证据链

1. **两个精确复现死亡点都在 CAN/网络处理链上**：
   - `src/main.cpp:367` `appActiveHandler->handleMessage(f, *appDriverSecondary)` —— 虚调用 + DND 滚轮帧处理（RA=dash_wheel_dnd.h:178 recordNative3c2），同一 PC 五次命中
   - `include/web/mcp2515_dashboard.h:5395` dashStartAccessPoint String 析构 `this=0xffffffc4`（腐坏对象）
2. **受害者全随机但类型一致**：vtable/函数指针（6×跳NULL, MEPC=0 RA=0）、C++ String 对象、hosted 通道句柄 —— = 堆上 C++ 对象被整片写穿，TCB/任务栈毫发无损（哨兵监控区外）
3. **0xA5 铁证重现**：run2 dump 里 assert 消息本身被 184×0xA5（FreeRTOS 栈填充值）覆盖
4. **首崩时机 ~12s 高度确定**（runs 1,4,5,7 全在 I(11.7s) 附近）——不像随机命中，像某个启动后定时事件制造腐坏，随后最先碰到腐坏对象的路径倒下（CAN 任务碰帧最频繁所以常当替死鬼；runs 3,9 则是 STA 触发 AP 弹跳时 String 阵亡）
5. **台架从未执行过这条链**：台架 E2E（OTA+浸泡+绊网）三总线无流量；TWAI ISR 洪流 / app_can_task 派发 / handleMessage / DND / recorder ring 写 —— 全部只在实车跑过。这正是「台架注入绕开 ISR」结构差的延伸：不止 ISR，整条帧处理链台架都是零覆盖
6. forensic 结构 magic=HGD2 活、字段全 0 = dump 新鲜（非台架陈旧 dump）、哨兵确实在岗但没逮到（腐坏落区在哨兵间隙——第六轮再次确认第四轮结论）

## 四、第七轮判读（2026-11-03 晚，寄存器级复核）— 修正第三节结论

> 本节修正第三节"CAN 数据链路软件腐坏"的表述，并证伪一份外部竞态理论。

### 4.1 外部理论证伪："t2canDrainSecondary check-then-use 竞态"

有分析指认 `main.cpp:359-367` 指针 check-then-use 竞态（并发方=dashSwapHandler 无锁换靶）。逐条对寄存器证据复核后**证伪**：

1. 崩溃点 `+1016`（0x40016544）执行 `lw a2,812(s3)`——**s3 是全局页基址寄存器**（0x4ff3e000，函数 +278 `lui s3,0x4ff3e` 设一次，此后全函数零写入）。崩溃时 s3=0：指针竞态改不了这个寄存器。
2. "check-then-use"链条完好：appActiveHandler 判空通过、vtable 加载成功（现场 A5=0x4010cd2c=LegacyHandler 合法虚表）。倒下的是下一步那只好好的 s3。
3. 现场组合不可能来自正常执行：S0~S5 全零（**s5 在全函数从未被写过**、s0 仅开头写一次）；**RA=0x40016500=+948 是一条 store 指令的地址**——全函数 39 个调用点枚举完毕，没有任何调用能产生这个返回地址。
4. 十轮串口日志零 swap/920 切换打印——"运行时平台切换"并发方在数据里无身影。

### 4.2 崩溃家族统一机制：寄存器文件成组归零

- **SP 散射铁证**：同一崩溃点 +1016，跨轮 SP=0x4ff7d9c0 / 0x4ff7da50 / 0x4ff7da40（函数合法 sp=0x4ff7d9c0）；跳NULL族 SP=0x4ff7d920/9c0/da60。同一指令三种 SP=**SP 本身也是受害者**。
- **非线性踩踏判死**：现场 a0-a7 槽全活（A1=A3=A4=0xa0=160 表容量、A6=4、A7=3 都是活值）而 s0-s5 全零。任何**线性内存清零**必然连 a 槽一起清掉——造不出这个模式。**零落在寄存器文件本身**，中断保存帧只是忠实记录了"已经归零的寄存器"。
- 异常帧基址 0x4ff7d880（由 mstatus@0x4ff7d900=base+128 精确反推，mcause=1=instr access fault + mtval=0=跳NULL实证）。
- **B 签名同机制统一**：String 析构 this=0xffffffc4 = **-60 = 归零基址寄存器 + 字段偏移**（lw 字段偏移-60 以零基址求址）。
- RA=+948 无法由任何调用产生，最可能的来源=**中断打断时刻的 MEPC 残影**（打断点恰在 bus2 诊断表 store——洪流期最热代码块）。
- 三类死亡点由此统一为一个机制：被归零的寄存器组随后被最先使用它的路径消费——can_rt 用 s3 寻址→Load fault；跳NULL族用 ra→PC=0；dash 任务 String 用基址→0xffffffc4。

### 4.3 上游排查（IDF 6.0.2/6.0.3 无现成修复）

- v6.0.1→v6.0.3 全部中断路径文件路径级扫描：vectors.S 仅 812c1895c（Zcmp workaround=C5/C61 睡眠重构、`#if __riscv_zcmp` 门控，与本固件无关）；panic 两条=调试器断点化妆；P4 port 三条=PMP/PSRAM/电源。**无寄存器腐坏类修复，升级无解可捡**。
- 本固件 arch 无 zcmp、全镜像 0 条 cm.push（ZCMP 理论排除），但 soc_caps.h 三条 ZCMP errata + vectors.S"mie=0 时仍触发中断"注释 = **P4 官方承认存在中断时序窗口类硅缺陷先例**。

### 4.4 结论与下一步

应用层无法解释也无法防御该机制（四轮应用层修复全失败的原因）。报告提议的"快照到局部+临界区"只能作防御加固、**不可当主修**。决胜实验=第五节实验 B：台架灌流若复现=拿到无限离线复现循环（最小化→报 espressif→试 IDF 6.1）；数小时全绿=车内特有（线束耦合）。

## 五、下一步（待 Jordan 批）

**实验 B（NO_ACK 自发自收洪流回放）正是决胜局**——上轮挂起的理由是先等实验 A（充电宝对照）出结果；A 已出（干净电照样崩）：
- 台架让板 2 三路 TWAI 自发自收灌满真实帧率 → 若 12-22s 家族在台架复现 = **软件 bug 实锤 + 无限复现循环可离线归零**
- 若台架数小时全绿 = 车内特有 → 回头查线束耦合
- 实验纯 RX 模拟，不碰车辆，零风险

## 附：本次判读命令备忘

```bash
GDB=~/.platformio/packages/tool-riscv32-esp-elf-gdb/bin/riscv32-esp-elf-gdb
ELF=~/atlas-field-kit/firmware-d5b5d28.elf
$GDB --batch -ex "file $ELF" -ex "target core run-XXX/core.elf" -ex "bt 12" -ex "p/x dashHeapGuardForensic"
# 串口日志必须 grep -a（二进制乱码字节会让普通 grep 静默漏检 panic 签名!）
```
