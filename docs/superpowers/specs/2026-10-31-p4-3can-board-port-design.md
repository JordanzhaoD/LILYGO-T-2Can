# T2CAN → P4-3CAN 原封移植（方案 A：T2CAN 仓库加第 10 个板型）

> 取代已作废的「0.4.0 P4 原生架构修复」计划（2026-10-30 Jordan 定夺：P4 原生架构整体偏离目标，废弃）。

## Context（意图与为什么）

P4 原生开发数周产出被判废品（界面/功能不可用、偏离目标）。Jordan 定夺：把车上稳定运行的 T2CAN 固件**原封不动**移植到 P4 硬件——UI 原子克隆、仅改名 P4-3CAN；P4 的 3 路 CAN 与高性能留给后续新功能（Ban Shield、自动换档）。先满足「能用」，零行为差异。

**黄金基线（Jordan 确认）**：车上 T2CAN 板运行 **6.0.0-beta07** = 仓库 `/Users/ziwind/my-vibe-project/LILYGO-T-2Can-4.5.0-beta01` 的 HEAD `eb9fd88`（树干净）。移植树一字不动，只加板型。

## 关键事实（本人已核验）

1. **T2CAN 仓库本就是多板型固件**：PlatformIO 9 个板型 env，框架=espidf（非 Arduino），平台 `platformio/espressif32@7.0.0` + `framework-espidf 4.60001.0`（IDF 6.0.x，支持 ESP32-P4），本地工具链经 `platformio_local.ini` file:// 钉包（riscv 工具链已在机——P4 是 RISC-V）。
2. **驱动接缝现成**：构建宏选驱动。`DRIVER_T2CAN_DUAL`（main.cpp:14-29）= 主总线走 `DRIVER_TWAI`（单 TWAI 实例，引脚由 TWAI_TX_PIN/RX_PIN 宏给）+ 其上拴第二驱动实例（原板=SPI MCP2515，`include/drivers/esp32_mcp2515_driver.h`），总线身份用位掩码 `CAN_BUS_CH/VEH/PARTY`（can_frame_types.h:8-11），`T2CAN_SECONDARY_BUS=CAN_BUS_PARTY`。
3. **P4 板=三路原生 TWAI**（旧仓库 include/platform/p4/p4_board_profile.h 实板观测）：TWAI0 TX2/RX3=CanA（X179 13/14 DAS 主线，同 T2CAN 主总线）、TWAI1 TX4/RX5=CanB（X179 9/10 滚轮总线，同 T2CAN 第二总线）、TWAI2 TX26/RX27=CanC（18/19 PT 动力总线）。32MB flash，PSRAM。板型 json 在旧仓库 `boards/atlas_p4_waveshare.json`（frameworks 含 espidf），分区表 `partitions/atlas_p4_32mb.csv`。
4. **UI 原子克隆天然成立**：UI 内嵌固件（include/web/mcp2515_dashboard_ui.h），板名经构建宏 `DASH_BOARD_NAME`（mcp2515_dashboard.h:6023-6105，流入 JSON 的 module/board 字段）——改名=一个宏，UI 零改动。
5. **接线（Jordan 确认）**：CanA=13/14、CanB=9/10、CanC=18/19 PT。与 T2CAN 双总线角色一一对位，P4 双原生 TWAI 替代原板「TWAI+MCP2515」组合，硬件反而更简单。

## 方案（已裁）

- **A（选定）**：T2CAN 仓库加 `[env:p4_3can]` + 双原生 TWAI 驱动变体。功能/UI 字节不动；T2CAN 后续升级 P4 免费跟进。
- B 拷贝独立仓库——升级流断裂，违背「原封不动最快」，否。
- C P4 走 IDF 原生构建（idf.py）——仅当 A 的构建先导验证失败时启用（备用门，写入设计）。

## 设计定案

> 建造者审查（2026-10-31）四项决议已并入：①整车配置钉死（Jordan 确认）②分区表与首烧方案修正（原文 D2/D5 自相矛盾已改）③PIN_LED 换安全空脚（Jordan 确认板上无灯/不确定）④CI 与资产策略。

### D1 构建先导（第一个执行步，半天，结果决定 A/C）
在仓库加最小 `[env:p4_3can_spark]`（板 json 先拷入 boards/、espidf 框架、仅验证链路：espressif32@7.0.0 + esp32p4 目标 + riscv file:// 工具链能否解析构建）。**通过→走 A；失败→C（同树加 IDF 原生 CMake 预设），此时再向 Jordan 报备。**

### D2 新板型 `[env:p4_3can]`
- `boards/p4_3can.json`：从旧仓库 `atlas_p4_waveshare.json` 改（name=Atlas P4-3CAN 32MB；**upload.offset_address 改 0x20000** 与新分区表一致）。
- `partitions_32mb_ota_8192k_nvs64.csv`：按 16MB 表布局平移放大（nvs@0x9000/0x10000 → otadata@0x19000 → **app0@0x20000** → app1 → 余量），不自创偏移。
- build_flags：`-DDRIVER_P4_3CAN`（在 main.cpp 里语义同 T2CAN_DUAL：内部 `#define DRIVER_TWAI` + 拴第二实例）、`-DTWAI_TX_PIN=GPIO_NUM_2 -DTWAI_RX_PIN=GPIO_NUM_3`（主=CanA）、`-DDASH_BOARD_NAME=\"P4-3CAN\"`、**整车配置钉死**（Jordan 确认的车载配置：车型=Legacy、ISA 提示音抑制+紧急车辆检测=开、ENHANCED_AUTOPILOT+NAG_KILLER=关——功能旗标全部进 tracked 的 build_flags，可复现）；AP/OTA **凭据不走入库通道**，沿用既有 platformio_profile.h 本地未跟踪机制（行为可复现、密钥不泄漏）。
- **`-DPIN_LED=<安全空脚>`**：默认值 GPIO2 在 P4=CanA 主总线 TX，必须覆盖；板上无灯/不确定（Jordan 确认）→ 建造者按微雪 P4 载板引脚图选一确认悬空脚（候选 GPIO 8/9，绝不碰 2/3/4/5/26/27），灯功能允许实际不亮（纯装饰）。
- `sdkconfig.p4_3can`：按既有 per-board sdkconfig 机制（scripts/platformio_sync_profile.py + platformio_profile.h）新增，PSRAM 开启。
- **不进 `default_envs`、不进 CI 矩阵**：其他机器无 file:// 工具链钉包；CI 用 HW4 示例 profile 覆盖=按定义非车载配置。资产走本机构建 + 按仓库惯例提交 firmware-assets 行（beta07「three-env」模式照抄为含 p4_3can 的资产行）。

### D3 第二总线驱动：原生 TWAI 实例
- 新 `include/drivers/p4_twai_secondary.h`：复用 `twai_driver.h` 的类模式，起 **TWAI_NUM_1** 实例（TX4/RX5=CanB），暴露与 dual 路径所用的第二驱动相同接口（init/send/readFrame/diag 计数）。IDF 6 的 twai_general_config_t 带 controller_id 字段。
- main.cpp 内 `#if defined(DRIVER_P4_3CAN)` 与 `DRIVER_T2CAN_DUAL` 块并列（MCP 特有逻辑—RST 脚/中断脚/SPI 初始化—替换为第二 TWAI 安装），`T2CAN_SECONDARY_BUS=CAN_BUS_PARTY` 不变，全部 secondary 计数/证据路径复用。
- **CanC（TWAI2）v1 完全不启动**：不安装、不进 UI（原子克隆=界面就是 T2CAN 双总线世界观）。

### D4 行为零差异承诺
- include/ 下功能引擎（dash_*.h 全家：FSD 中继/DND/防甩/速度偏移/jitter/nag/abort guard/爆闪…）与 UI blob **零改动**；diff 仅限：platformio.ini、boards/、partitions CSV、sdkconfig、main.cpp 板型块、新第二驱动头、（如需）platformio_profile.h。
- 版本号沿用 6.0.0-beta07（同一固件，板名区分）。CHANGELOG 若记，英文条目（仓库铁律）。

### D5 验证（台架→实车）
1. 回归网：既有 9 env 全部构建绿 + `[env:native]` 测试绿（新代码全在 #ifdef 内）。
2. p4_3can 构建绿 → USB esptool **四段链首烧**：bootloader@0x2000 + 分区表@0x8000 + otadata 区擦除 + app@0x20000（板上现有 Atlas 固件+分区表全被替换——预期行为；Atlas 固件冻结仓库可随时重建，板上 NVS（WiFi 配置）清空后走 AP 重新配网）。⚠️绝不「单写 app」：板上跑的是别家分区表（旧坑：firmware-only=boot 循环）。
3. 台架冒烟：STA/AP 起、LAN 打开 UI、板名显示 P4-3CAN、双总线计数动、OTA 页可用（此后升级走 WebUI OTA 或单写 app@**0x20000**）。
4. **实车（Jordan 拔 T2CAN 板后）**：parity 清单——武装/FSD 中继、DND、防甩、速度偏移等逐项与 T2CAN 行为一致。
5. 仓库发版惯例照走（firmware-assets 产物、release(assets) commit；uiBuildId 先 commit 再产资产的既有纪律）。

### 约束与红线
- **上车测试前必须物理拔掉 T2CAN 板**（两个可 TX 设备同总线=事故）。
- 旧仓库 Atlas-3CAN-P4 冻结归档，不再投入；其 P4 硬件事实（引脚/分区/烧录配方）只作移植资料。
- 不做任何「顺手改进」：一切以与 beta07 行为逐字节一致为准。

### 留给建造者
- twai_driver.h 类能否双实例化（若含全局态则第二实例用独立封装）；ISR/互斥策略照抄主实例。
- platformio_profile.h 与 sync_profile 脚本对第 10 板型的适配点。
- IDF 6 下 esp32p4 的 sdkconfig 差异项（PSRAM/flash dio/USB CDC）。
- D1 失败时的 C 方案落点（idf.py 预设、产物路径并入 firmware-assets 惯例）。

## 执行阶段（批出后按 writing-plans 展开）
P0 D1 先导（A/C 门）→ P1 板型骨架+改名+构建绿 → P2 第二 TWAI 驱动+双总线通 → P3 台架冒烟 → P4 实车 parity（Jordan 配合拔板/试驾）→ P5 资产发版。

---

## 增补定案（执行期）

**Jordan 2026-10-31 三项**：板名=`Atlas-P4-3CAN`（DASH_BOARD_NAME）；AP SSID 默认=`Atlas-P4-3CAN`（经 p4 profile 的 DASH_SSID）；AP 访问地址=`100.100.1.1`——**后者本就是固件硬编码现有行为**（mcp2515_dashboard.h:5240），零改动。

**D1 实施发现（2026-10-31）**：
1. 官方 espressif32@7.0.0 平台对 esp32p4 零知识：builder 派生 `xtensa-esp32p4-` 前缀。修法=本机平台包 4 处 mcu 元组加 `esp32p4`（builder/main.py×2、builder/frameworks/espidf.py、platform.py）。**平台补丁在本机 `~/.platformio/platforms/espressif32@7.0.0/`，仓库外**——p4_3can 本就本机专属构建（不入 CI），代价=平台重装后须重打（本节即备忘）。
2. IDF 6.0.1 对 esp32p4 要求 riscv 工具链 `esp-15.2.0_20251204`（14.2 不认 p4 march 的 zaamo/zalrsc）。修法=下载官方 aarch64-apple-darwin 工具链入 `~/.platformio/tools/toolchain-riscv32-esp-15.2.0`，`platformio_local.ini` 的 riscv 钉包改指新目录（其它 env 全是 xtensa，不受影响）。版本对上后 `IDF_MAINTAINER=1` 不再需要。
