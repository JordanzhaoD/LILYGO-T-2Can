# T2CAN → P4-3CAN 板型移植实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在本仓库（黄金基线 6.0.0-beta07，HEAD `eb9fd88`）新增第 11 个硬件 env `p4_3can`，把固件原封不动跑上 ESP32-P4（双原生 TWAI），UI 原子克隆仅改名 P4-3CAN。

**Architecture:** 方案 A = 只加板型不改功能。构建宏 `DRIVER_P4_3CAN` 在 main.cpp 里语义等同 `DRIVER_T2CAN_DUAL`（内部 `#define DRIVER_TWAI` 主总线 CanA + 拴第二驱动实例 CanB），唯一差别 = 第二实例从 SPI MCP2515 换成原生 TWAI_NUM_1。CanC（TWAI2）v1 完全不启动。功能引擎与 UI blob 零改动。

**Tech Stack:** PlatformIO espidf 框架 / platformio/espressif32@7.0.0（本机 file:// 钉包，`platformio_local.ini [espidf6]`）+ framework-espidf 4.60001.0（IDF 6.0.1，已验证含 esp32p4 target：`components/soc/esp32p4`、`esp_hal_twai/esp32p4` 在场）。

**Spec:** `docs/superpowers/specs/2026-10-31-p4-3can-board-port-design.md`（Task 1 入库；源=已批设计 `/Users/ziwind/.claude/plans/splendid-doodling-swan.md`，含全部定案 D1-D5 与建造者审查四决议）

## Global Constraints

- **零 diff 承诺**：改动仅限 `platformio.ini`、`boards/p4_3can.json`、`partitions_32mb_ota_8192k_nvs64.csv`、`sdkconfig.p4_3can*`、`src/main.cpp`（新增 `#if defined(DRIVER_P4_3CAN)` 块）、`include/drivers/p4_twai_secondary.h`（新文件）、`scripts/platformio_sync_profile.py` + `scripts/platformio_set_profile.py`（若涉及）、`platformio_profile_p4.example.h`、`.gitignore`、`docs/`、`CHANGELOG.md`、firmware-assets。**`include/dash_*.h`、`include/web/*`、其余 `src/`、`include/` 一律不动。**
- **引脚（实板观测，Atlas 仓库 p4_board_profile.h）**：CanA=TWAI0 TX `GPIO_NUM_2`/RX `GPIO_NUM_3`（车上 X179 13/14）；CanB=TWAI1 TX `GPIO_NUM_4`/RX `GPIO_NUM_5`（X179 9/10 滚轮总线）；CanC=TWAI2 TX26/RX27 **v1 不安装不启动**。`PIN_LED` 绝不 ∈ {2,3,4,5,26,27}。
- **总线身份不变**：`T2CAN_SECONDARY_BUS=CAN_BUS_PARTY` 照抄 dual 块；CanA=`CAN_BUS_DEFAULT=CAN_BUS_VEH`。
- **板名（Jordan 2026-10-31 定案）**：`-DDASH_BOARD_NAME=\"Atlas-P4-3CAN\"`（经宏流入 UI JSON module/board 字段，UI 零改动）。
- **AP（Jordan 2026-10-31 定案）**：AP SSID 默认 = `DASH_SSID`=`"Atlas-P4-3CAN"`（profile_p4 注入；UI/NVS `ap_ssid` 可覆盖=既有机制）；AP 网关/访问地址 = **100.100.1.1 已是固件硬编码现有行为**（mcp2515_dashboard.h:5240 `IPAddress apIp(100,100,1,1)`），零改动。
- **版本沿用 6.0.0-beta07 不 bump**（同一固件新板型）；CHANGELOG 条目纯英文（仓库铁律 + CJK 自检）。
- **整车黄金配置**（Jordan 已确认钉死）：车型=Legacy、ISA 提示音抑制=开、紧急车辆检测=开、EAP/NAG_KILLER=关。经 p4 专属 profile（`custom_profile_path`）注入；凭据走未跟踪本地 profile，不入库。
- **`p4_3can` 不进 `default_envs`**（当前为空，保持）、**不进 CI**（`release.yml` 是显式 env 矩阵，天然不涉及）。本机构建 + 按 beta07 资产惯例发产物。
- **红线**：上车测试前必须物理拔掉 T2CAN 板；首烧绝不单写 app（板上跑着别家分区表=boot 循环旧坑）；Atlas-3CAN-P4 仓库冻结只作资料源。
- **构建纪律**：native 测试矩阵勿与硬件构建并行跑；构建后若 `git status` 显示 `include/web/mcp2515_dashboard_ui.h` 被 `minify_dashboard.py` 重建弄脏 → `git checkout --` 还原，绝不 commit（UI 零改动承诺）。

## Review Focus

1. **PIN_LED 落在 CAN/WiFi6 模组引脚 → 破坏 DAS 主总线或楔死 WiFi 芯片**。app.h:24 默认 PIN_LED=2=P4 的 CanA TX。钉住：Task 2 Step 1-3 证据程序（Waveshare wiki/Albert strings）+ 禁止清单 grep 验证。
2. **单写 app 到别家分区表 → boot 循环**（Atlas 新板旧坑实踩过）。钉住：Task 4 四段链步骤 + boot 横幅验证步骤 + 「此后升级=OTA 或单写 0x20000」注记。
3. **profile 互扰**：p4_3can 误用 T2CAN 的 `platformio_profile.h` → env+profile 双 `DRIVER_DEFINES` 触发 sync UserError 或错车型。钉住：Task 2 Step 4-6（`custom_profile_path` 指向独立文件 + 构建报错即失败 + DASH_DEFAULT_HW grep）。
4. **sdkconfig 目标错配**：`sdkconfig.defaults` 含 S3 专属行（`CONFIG_SPIRAM_MODE_OCT`、`CONFIG_ESP32_WIFI_TASK_PINNED_*`）→ P4 生成失败或 PSRAM 初始化挂。钉住：Task 1 Step 4 检查生成的 sdkconfig 里 `CONFIG_IDF_TARGET="esp32p4"` + Task 2 PSRAM 行调整。
5. **双总线身份错标**：P4 第二路帧未打 `CAN_BUS_PARTY` → bus2 计数/DND 证据/健康页全错。钉住：Task 3 Step 2（镜像 dual 块逐函数对照）+ Task 4 Step 6 台架计数检查。

---

### Task 1: D1 构建先导 `p4_3can_spark`（A/C 门 + 设计文档入库）

> 本任务回答「官方 espressif32@7.0.0 平台 + 仓库自带 board json 能否构建 esp32p4」。平台本身零 P4 知识（board json 全库无 p4、platform.py mcu 名单无 esp32p4），但 riscv 工具链已被 `platformio_local.ini [espidf6]` file:// 钉住、IDF 6.0.1 有完整 p4 target——理论可通，本任务实证。**失败（且 30 分钟内不可修）→ 停止，向 Jordan 报备走 C 方案（IDF 原生 CMake 预设），不硬闯。**

**Files:**
- Create: `boards/p4_3can.json`
- Create: `partitions_32mb_ota_8192k_nvs64.csv`
- Modify: `platformio.ini`（文件末尾追加 env）
- Create: `sdkconfig.p4_3can_spark`（首建由 CMake 生成后提交）
- Create: `docs/superpowers/specs/2026-10-31-p4-3can-board-port-design.md`（拷自已批设计 `/Users/ziwind/.claude/plans/splendid-doodling-swan.md`，原样入库）

**Interfaces:**
- Produces: board id `p4_3can`、env 名 `p4_3can_spark`（Task 2/3/4 依赖 board id；spark env 本任务后可删可留，留给 CI 诊断用建议保留）。
- Consumes: `platformio_local.ini [espidf6]` 的 platform/platform_packages 引用（既有）。

- [ ] **Step 1: 写 `boards/p4_3can.json`**

以 Atlas 仓库 `boards/atlas_p4_waveshare.json` 为底，官方平台键风格精简（去掉 pioarduino 专属的 `core`/`chip_variant`/`extra_flags`/`connectivity`），两处必须改：`offset_address` = `"0x20000"`（配新分区表，**Atlas 原值 0x10000 是旧坑**）；`partitions` 键去掉（分区表由 env 的 `board_build.partitions` 指定，单一来源）：

```json
{
  "build": {
    "mcu": "esp32p4",
    "f_cpu": "400000000L",
    "f_flash": "80000000L",
    "flash_mode": "dio",
    "bootloader_addr": "0x2000"
  },
  "name": "Atlas P4-3CAN 32MB",
  "frameworks": ["espidf"],
  "upload": {
    "flash_size": "32MB",
    "offset_address": "0x20000",
    "maximum_ram_size": 524288,
    "maximum_size": 33554432,
    "require_upload_port": true,
    "speed": 1500000
  },
  "debug": { "openocd_target": "esp32p4.cfg" },
  "url": "https://github.com/JordanzhaoD/my-vibe-project",
  "vendor": "Atlas"
}
```

- [ ] **Step 2: 写 `partitions_32mb_ota_8192k_nvs64.csv`**

复制 `partitions_16mb_ota_4096k_nvs64.csv` 的全部行（名称/Type/SubType/Flags 一字不动），只按此表换 Offset/Size（布局=16MB 表平移放大，app 槽 8192k）：

| name | offset | size |
|---|---|---|
| nvs | 0x9000 | 0x10000 |
| otadata | 0x19000 | 0x2000 |
| app0 | 0x20000 | 0x800000 |
| app1 | 0x820000 | 0x800000 |
| spiffs | 0x1020000 | 0x1FC0000 |
| coredump | 0x2FE0000 | 0x20000 |

- [ ] **Step 3: 追加 spark env 到 platformio.ini 末尾**

```ini
; ── P4-3CAN port (2026-10-31): D1 build spark — proves espressif32@7.0.0
;    + local file:// toolchains can build esp32p4/espidf. Do NOT add to
;    default_envs (empty on purpose) or the CI matrix.
[env:p4_3can_spark]
board = p4_3can
framework = espidf
platform = ${espidf6.platform}
platform_packages = ${espidf6.platform_packages}
board_build.partitions = partitions_32mb_ota_8192k_nvs64.csv
board_build.flash_size = 32MB
build_flags =
    -DDRIVER_TWAI
    -DTWAI_TX_PIN=GPIO_NUM_2
    -DTWAI_RX_PIN=GPIO_NUM_3
```

引脚宏书写形式对照 `[env:lilygo_t2can_dual]`（platformio.ini:218 起）既有写法照抄；`DRIVER_TWAI` = 既有单总线路径（beta07 主总线同款），spark 阶段零新驱动代码即最大覆盖面（整个 dashboard+功能引擎为 p4 目标编译一遍）。

- [ ] **Step 4: 构建并验证目标**

Run: `pio run -e p4_3can_spark`
Expected: `SUCCESS`，产出 `.pio/build/p4_3can_spark/firmware.bin`。
生成 `sdkconfig.p4_3can_spark`（PIO espidf 构建器约定：`SDKCONFIG_PATH` 默认 = 项目根 `sdkconfig.<env>`，espidf.py:106-109）。验证两行：

```bash
grep 'CONFIG_IDF_TARGET=' sdkconfig.p4_3can_spark   # 期望 "esp32p4"
grep 'CONFIG_SPIRAM=' sdkconfig.p4_3can_spark       # 记录值（S3 defaults 带来的行）
```

首建失败常见点（按序自查，>30 分钟不可修 → 停，报 Jordan 走 C）：① 请求联网装 15.2.0 工具链 → `platformio_local.ini` 引用漏了；② IDF 目标不识别 → 检查 framework-espidf 解析到 4.60001.0；③ 链接器/分区表错 → `board_build.flash_size` 未生效。

- [ ] **Step 5: 入库设计文档 + 提交**

把已批设计 `/Users/ziwind/.claude/plans/splendid-doodling-swan.md` 原样拷为 `docs/superpowers/specs/2026-10-31-p4-3can-board-port-design.md`。

```bash
git add boards/p4_3can.json partitions_32mb_ota_8192k_nvs64.csv platformio.ini sdkconfig.p4_3can_spark docs/superpowers/specs/2026-10-31-p4-3can-board-port-design.md
git status   # 确认无 mcp2515_dashboard_ui.h 等意外文件；有则 checkout -- 还原后再 add
git commit -m "build(p4): D1 spark env proves espressif32@7.0.0 builds esp32p4 target"
```

---

### Task 2: 正式板型 env `p4_3can`（板名/黄金配置/profile 隔离/PIN_LED 证据）

**Files:**
- Modify: `platformio.ini`（追加 `[env:p4_3can]`）
- Create: `sdkconfig.p4_3can`（生成后调整 PSRAM 再提交）
- Create: `platformio_profile_p4.example.h`（入库模板）
- Create: `platformio_profile_p4.h`（**不入库**，本地黄金配置+凭据）
- Modify: `.gitignore`（加 `platformio_profile_p4.h`）
- Modify: `scripts/platformio_sync_profile.py` + `scripts/platformio_set_profile.py`（`DRIVER_DEFINES` 元组加 `"DRIVER_P4_3CAN"`；先 grep 两脚本所有 `DRIVER_T2CAN_DUAL` 出现点逐处对照）

**Interfaces:**
- Produces: env 名 `p4_3can`（Task 3 构建、Task 4 烧录、Task 5 资产用）；构建宏 `DRIVER_P4_3CAN`（Task 3 的 main.cpp 块与驱动头都包在它下面）；profile 选项 `custom_profile_path = platformio_profile_p4.h`。
- Consumes: Task 1 的 board id `p4_3can`、分区 CSV；`scripts/platformio_sync_profile.py` 既有机制（`custom_profile_path` 项目选项、`_pick_one` 校验、`_set_cppdefine(DASH_DEFAULT_HW)` 注入）。

- [ ] **Step 1: 查证据定 PIN_LED**

按序取证（写进 commit message）：① Waveshare 官网该板 Wiki（板名 `waveshare-p4-wifi6-sn65hvd230-x3`，即 Waveshare ESP32-P4 WiFi6 开发板）查板载 LED/WS2812 引脚与 32.768k 晶振有无；② grep 本地 Albert 参考固件反编译 strings（Atlas-3CAN-P4 仓库参考资料目录）里 LED GPIO 痕迹。得证 → 用该脚；无证据 → `GPIO_NUM_8`（P4 上通常悬空；即使接 32k 晶振也无破坏性电流，LED 不亮可接受=Jordan 已批）。**硬约束：绝不 ∈ {2,3,4,5,26,27}，不与 WiFi6 模组 SDIO/UART、USB、TF 卡引脚重叠。**（T2CAN 侧 PIN_LED 用途=app.h:80-94 可寻址 RGB + 184/242 普通开关，纯视觉。）

- [ ] **Step 2: 追加正式 env**

```ini
[env:p4_3can]
board = p4_3can
framework = espidf
platform = ${espidf6.platform}
platform_packages = ${espidf6.platform_packages}
board_build.partitions = partitions_32mb_ota_8192k_nvs64.csv
board_build.flash_size = 32MB
custom_profile_path = platformio_profile_p4.h
build_flags =
    -DDRIVER_P4_3CAN
    -DTWAI_TX_PIN=GPIO_NUM_2
    -DTWAI_RX_PIN=GPIO_NUM_3
    -DDASH_BOARD_NAME=\"Atlas-P4-3CAN\"
    -DPIN_LED=<Step 1 定的脚，形如 GPIO_NUM_8>
```

其余 dual env 有而本 env 需要对照的键（`DASH_FSD_252_COMPAT`、`DASH_STA_AP_GATEWAY`、`ESP_IDF_LWIP_HOOK_FILENAME`、`-Wno-*`）：逐项对照 `[env:lilygo_t2can_dual]`（platformio.ini:218-260）抄与板无关的通用项（功能/UI 零差异=build_flags 面也应对齐；SPI/MCP 专属键除外）。MCP/SPI 键（`PIN_CAN_CS`、`MCP2515_RST_PIN`、SPI 引脚）**不抄**。

- [ ] **Step 3: 写 profile 模板 + 本地体 + .gitignore**

`platformio_profile_p4.example.h`：打开本机未跟踪 `platformio_profile.h`（黄金源：Legacy+ISA+EMERGENCY 开、EAP/NAG_KILLER 关）→ 拷其结构，驱动 define 用 `DRIVER_P4_3CAN`（**必须恰好一个** driver define，否则 sync 脚本 `_pick_one` UserError），凭据占位符化（`your-wifi-ssid` 风格，照 `platformio_profile.example.h` 既有占位惯例），**`DASH_SSID` = `\"Atlas-P4-3CAN\"`**（Jordan 定案；AP 密码沿用黄金 profile 的 DASH_PASS 值）。本地 `platformio_profile_p4.h` = 同内容但凭据用真值（从 `platformio_profile.h` 拷）。`.gitignore` 加一行 `platformio_profile_p4.h`。

- [ ] **Step 4: 改 sync/set_profile 脚本**

`grep -n "DRIVER_T2CAN_DUAL" scripts/*.py` 找全触点；两脚本的 `DRIVER_DEFINES` 元组各加 `"DRIVER_P4_3CAN"`。除元组外不添逻辑（p4 走既有 `custom_profile_path` 分支）。注意 dashboard 构建下车型/ISA/EMERGENCY/EAP/NAG_KILLER 本就=运行时 UI 开关（can_helpers.h:7-31 编译期 no-op），黄金配置里真正编译期生效的只有车型→`DASH_DEFAULT_HW` 注入——验证脚本对 p4 profile 走了这条路径。

- [ ] **Step 5: 构建验证三件套**

```bash
pio run -e p4_3can        # SUCCESS
strings .pio/build/p4_3can/firmware.bin | grep -c "Atlas-P4-3CAN"   # ≥2（板名+AP SSID 都进固件）
pio run -e lilygo_t2can_dual    # 仍 SUCCESS（黄金 env 无回归）
```

同时检查 `sdkconfig.p4_3can`（首建生成）：`CONFIG_IDF_TARGET="esp32p4"`；PSRAM 行——S3 defaults 带来的 `CONFIG_SPIRAM_MODE_OCT` 在 P4 上若报错或 PSRAM 初始化失败（构建期报/上电日志报），删 SPIRAM 族行改为不启用 PSRAM（P4 片上 768KB SRAM > S3 512KB，dashboard 无 PSRAM 也过；台架 Task 4 Step 7 再看堆余量定夺）。PSRAM 决策写进 commit message。

- [ ] **Step 6: 提交**

```bash
git add platformio.ini sdkconfig.p4_3can platformio_profile_p4.example.h .gitignore scripts/platformio_sync_profile.py scripts/platformio_set_profile.py
git commit -m "build(p4): full p4_3can env with isolated profile and golden vehicle config

LED pin evidence: <来源/P4板上悬空判定>"
```

（`platformio_profile_p4.h` 已被 .gitignore 挡住，确认 `git status` 不见它。）

---

### Task 3: 第二路 TWAI 驱动 + main.cpp P4 块 + 零 diff 审计 + 全回归

**Files:**
- Create: `include/drivers/p4_twai_secondary.h`
- Modify: `src/main.cpp`（新增 `#if defined(DRIVER_P4_3CAN)` 块，与 `DRIVER_T2CAN_DUAL` 块并列）

**Interfaces:**
- Consumes: `twai_driver.h` 的类模式与 IDF6 `twai_general_config_t.controller_id`；dual 块的全部接缝（main.cpp:97 起 `appDriverSecondary` 用法族：`t2canSetupSecondary` / `t2canSendSecondary` / `t2canDrainSecondary`（budget 16 读循环、`t2canRecordBus2`、`appActiveHandler->handleMessage(f, *appDriverSecondary)`）/ tick 调用族 / setup:758 `onSendFrame` 挂接）；`T2CAN_SECONDARY_BUS=CAN_BUS_PARTY`。
- Produces: `class P4TwaiSecondaryDriver`，方法面 = dual 块对第二驱动实际调用的全集（对照 ESP32_MCP2515Driver 公开面：`init()` / `send(const CanFrame&)` / `read(CanFrame&)` / `setReceiveAllMode()` / `checkBusOff()` / `tryRecover()` / `onSendFrame` 回调；以 grep `appDriverSecondary->` 的真实调用清单为准，一一实现，不多不少）。

- [ ] **Step 1: 写 `include/drivers/p4_twai_secondary.h`**

独立新头（不改共享的 `twai_driver.h`——零 diff 承诺）。镜像 `twai_driver.h` 类结构，三处不同：`g_config_.controller_id = TWAI_NUM_1`（=1）、TX=`GPIO_NUM_4` RX=`GPIO_NUM_5`、构造即 accept-all 过滤（dual 块初始化后调 `setReceiveAllMode()` 的语义在此等价成立，方法保留空实现或直接配置，以接口对齐为准）。500kbps、BUSOFF 冷却等常量照抄 `twai_driver.h`（BUSOFF_COOLDOWN_MS=1000）。

- [ ] **Step 2: 写 main.cpp 的 `#if defined(DRIVER_P4_3CAN)` 块**

文件头 driver 选择区（main.cpp:14-29）加：`#elif defined(DRIVER_P4_3CAN)` → `#define DRIVER_TWAI` + `#define T2CAN_SECONDARY_BUS CAN_BUS_PARTY`（与 dual 分支并排）。正文每处 `#if defined(DRIVER_T2CAN_DUAL)` 旁加 `#if defined(DRIVER_T2CAN_DUAL) || defined(DRIVER_P4_3CAN)`（共用骨架），P4 特有差异点只在第二实例构造/安装处分叉：MCP 的 SPI 初始化+RST 时序+中断脚（`t2canSetupSecondary` 内）换成 `P4TwaiSecondaryDriver::init()`（TWAI 安装+过滤器），类型名替换，其余逻辑（drain/record/handleMessage/tick 族/health）一字不动共用。`#include "drivers/p4_twai_secondary.h"` 同样包进 p4 条件。CanC/TWAI2 零代码出现。

- [ ] **Step 3: 构建 + 零 diff 审计**

```bash
pio run -e p4_3can          # SUCCESS
git diff --stat eb9fd88..HEAD && git status --porcelain
```

Expected: 改动文件清单 ⊆ Global Constraints 白名单；`include/web/`、`include/dash_*.h` 零出现（构建弄脏的 ui.h → `git checkout --` 还原）。

- [ ] **Step 4: 全回归（逐条跑，勿并行）**

```bash
pio run -e lilygo_t2can_dual     # 门槛 env，必须绿
pio run -e esp32_twai && pio run -e esp32s2_twai && pio run -e esp32_ext_mcp2515
pio run -e waveshare_single_can_standalone && pio run -e waveshare_ESP32_S3_RS485_CAN
pio run -e m5stack-atomic-can-base && pio run -e m5stack-atoms3-mini-can-base
pio run -e lilygo_tcan485_hw3 && pio run -e esp32_feather_v2_mcp2515
```

10 个既有硬件 env 全绿为佳；出现与本次 diff 无关的既有失败 → 记录进任务笔记，门槛只认 `lilygo_t2can_dual`+`p4_3can`+`p4_3can_spark`，不阻塞但如实上报。native 族测试（platformio.ini:261-407 全部 `native*` env）照 beta07 发版矩阵跑 `pio test -e <name>`——新代码全在 p4 条件内且 main.cpp 不进 native 构建，期望全绿。

- [ ] **Step 5: 提交**

```bash
git add include/drivers/p4_twai_secondary.h src/main.cpp
git commit -m "feat(p4): native TWAI_NUM_1 secondary driver and P4-3CAN board block in main"
```

---

### Task 4: 四段链首烧 + 台架冒烟（需 P4 板在台架 + USB）

**Files:**
- Create: `docs/verification/p4-3can-bench-2026-11-XX.md`（冒烟记录：boot 横幅、IP、UI 截图、计数器、堆余量）

**Interfaces:**
- Consumes: `.pio/build/p4_3can/{bootloader.bin, partitions.bin, firmware.bin}`；Task 2 profile 里的 WiFi 凭据。

- [ ] **Step 1: 擦 + 四段链首烧（esptool 走 PIO 自带 esptoolpy）**

板上现在是 Atlas 固件+其分区表 → 全链替换。`<PORT>` = 板的 USB 串口（macOS `/dev/cu.usbmodem*`）：

```bash
python3 ~/.platformio/packages/tool-esptoolpy/esptool/esptool/__init__.py --chip esp32p4 -p <PORT> -b 921600 \
  erase_region 0x9000 0x12000            # nvs + otadata 一次清干净
python3 ~/.platformio/packages/tool-esptoolpy/esptool/esptool/__init__.py --chip esp32p4 -p <PORT> -b 921600 \
  write_flash 0x2000  .pio/build/p4_3can/bootloader.bin \
                0x8000 .pio/build/p4_3can/partitions.bin \
                0x20000 .pio/build/p4_3can/firmware.bin
```

（esptool 入口路径以 `pio run -e p4_3can -t envdump` 或包目录实际为准；bootloader.bin 缺失 → 先 `pio run -e p4_3can -t bootloader`。）**此后升级 = WebUI OTA 或单写 app@0x20000，永远不再四段链（除非分区表再变）。**

- [ ] **Step 2: boot 验证**

`screen <PORT> 115200`（或 pio device monitor）：看 boot 横幅无 boot-loop、无 `SPIRAM` 报错（若 Task 2 保留了 PSRAM 行）、TWAI 初始化日志（主总线 + "CAN B"（TWAI_NUM_1）ready @ 500k 字样）。AP 起来后（CoreWLAN 按名扫描验证，SSID 来自 profile）。

- [ ] **Step 3: 配网 + LAN UI 冒烟**

AP 配网流程与 T2CAN 板完全同法（DASH_STA_AP_GATEWAY 既有流程）。LAN 打开 dashboard，逐项核：① 板名/JSON `module`/`board` 字段 = **Atlas-P4-3CAN**；⑥ AP 模式 SSID=**Atlas-P4-3CAN**、访问地址 **http://100.100.1.1**（硬编码既有行为）；② 页面与导航 = T2CAN 原样（原子克隆）；③ 双总线页在、bus2 计数器在（台架无流量=0，正常）；④ OTA 页可用（后续升级通道）；⑤ 堆余量记录（`/api` status heap 字段）——决定 PSRAM 去留在下一版定案。

- [ ] **Step 4: 记录 + 提交**

冒烟记录写入 `docs/verification/p4-3can-bench-2026-11-XX.md`（含证据截图），commit `docs(p4): bench smoke evidence for first four-stage flash`。

---

### Task 5: 资产发版（版本不 bump + 四 env 资产行）

**Files:**
- Modify: `CHANGELOG.md`（英文条目）
- Modify/新增: firmware-assets 产物（照 `eb9fd88 release(assets): 6.0.0-beta07 three-env firmware assets` 的文件布局，加 p4_3can 一路变四路）

**Interfaces:**
- Consumes: `.pio/build/p4_3can/firmware.bin`（含 bootloader/partitions 如 beta07 资产惯例）。

- [ ] **Step 1: CHANGELOG 英文条目**（挂在 6.0.0-beta07 段或 Unreleased，镜像既有结构）：`Add P4-3CAN board: ESP32-P4 with dual native TWAI (CanA=13/14, CanB=9/10); feature/UI parity with 6.0.0-beta07.` CJK 正则自检通过。

- [ ] **Step 2: 资产行 + 元数据校验**

照 beta07 资产 commit 布局加 p4_3can 产物；`python3 scripts/check_release_metadata.py` 绿。**纪律：uiBuildId 不 bump**（UI 零改动）；若脚本要求源码 commit 先于资产 → 照 beta07 顺序（源码 commit → 资产 commit）。

- [ ] **Step 3: 提交 + 问 Jordan 是否 push**

```bash
git add CHANGELOG.md <firmware-assets 路径>
git commit -m "release(assets): 6.0.0-beta07 four-env firmware assets (adds P4-3CAN)"
```

push 前向 Jordan 确认（本仓库直推 origin 惯例，但发版类 commit 一贯过目后推）。

---

### Task 6: 实车 parity 清单（Jordan 配合，拔 T2CAN 板红线）

**Files:**
- Create: `docs/verification/p4-3can-field-parity-2026-11-XX.md`

**Interfaces:**
- Consumes: Task 4 烧好的板、车上 X179 13/14（CanA）+ 9/10（CanB）接线（Jordan 已确认现状即此）。

- [ ] **Step 1: 写 parity 清单并入库**

清单项逐一对齐 T2CAN 行为：武装拨动、FSD 中继（Legacy 线）、DND 单帧+方向交替、防甩、速度偏移、bus2 健康页计数随滚轮动作增长、OTA 从车内 WiFi 可用。**红线置顶：上车前物理拔掉 T2CAN 板（两个可 TX 设备同总线=事故）。** commit `docs(p4): field parity checklist`。

- [ ] **Step 2: Jordan 试驾回填**

Jordan 正常使用逐项试；异常项记拒因码/现象回填清单。全部 ✅ → 移交后续新功能线（Ban Shield、自动换档——这才是 P4 平台的真正用途），本移植收口。

---

## 执行备注（给 executor）

- 全程在 worktree `/Users/ziwind/my-vibe-project/LILYGO-T-2Can-4.5.0-beta01` 根目录跑命令；不用 `-c` 剪枝、不并行 native 测试与硬件构建。
- 构建后 `git status` 出现 `include/web/mcp2515_dashboard_ui.h` 变脏 → `git checkout --` 还原，**绝不 commit**。
- D1 spark 若失败且 30 分钟不可修 = 停止并报备（C 方案门），不要自行改平台版本或动 toolchain 钉包。
- 任何「顺手改进」冲动（重命名、格式化、删死代码）都违反零 diff 承诺——不批。
