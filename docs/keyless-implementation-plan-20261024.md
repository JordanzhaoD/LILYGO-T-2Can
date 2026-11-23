# 无钥匙进入（Keyless Entry）实施方案 —— Atlas-T2CAN 双CAN 固件

> 日期：2026-10-24 ｜ 状态：**待 Jordan 批准（HARD-GATE，批准后动代码）**
> 依据：四案裁决（0x205 b2 bit4 锁态锚 / 0x248 命令镜像实测）+ Intel MCU 字典 + fob 链时序。
> 决策记录：自动换档挂起（等 3CAN 硬件）；无钥匙落 T-2CAN（bus2=BDY 已接，**零接线改动**）。

---

## 一、落点与基线

| 项 | 值 |
|---|---|
| 仓库/分支 | `LILYGO-T-2Can-firmware` @ `dev/4.5.0-beta15-baseline`（3a0e9ed）→ 开 `feat/keyless-entry` |
| 硬件 | T-2CAN（esp32s3box，ESP32-S3 + MCP2515×2），bus2=BDY |
| 版本 | 6.1.0-keyless-beta01（M1+M2）→ beta02（M3 自动模式） |
| 新增文件 | `include/dash_keyless.h`（自包含模块，零改动现有防御引擎） |

## 二、功能架构（三层）

```
┌─ 检测层 keylessPresenceGet() ──────────────────────┐
│  V1 WiFi：手机热点，模块 STA 关联=在场（零手机端开发）│
│  V2 BLE ：NimBLE peripheral + 手机连接态（S3 BLE5）  │
├─ 决策层 KeylessState 状态机 ────────────────────────┤
│  去抖/冷却/跳闸/手动自动统一入口                      │
├─ 执行层 0x248 注入器 + 0x205 确认 ──────────────────┤
│  克隆-改-发（bit46 同族范式）→ 等 b2 bit4 翻转确认   │
└───────────────────────────────────────────────────┘
```

## 三、核心状态机（dash_keyless.h）

```
IDLE ──在场(去抖3s)──→ PRESENT_ARMED
PRESENT_ARMED ──触发──→ INJECT_UNLOCK ──bit4:1→0 (2s内)──→ UNLOCKED ✓
UNLOCKED ──离场(断连去抖20s)──→ INJECT_LOCK ──bit4:0→1──→ LOCKED ✓
注入后 2s 无确认 → RETRY(≤2次) → FAULT_LATCHED（自动禁用+UI红，t2canServiceLatched 同款）
```

- 真相源唯一：`0x205 b2 bit4`（1=LOCKED/0=UNLOCKED，fob 链双向实测）；0x203 b3 bit1（布防）仅旁证显示
- 手动（现场遥控页按钮）与自动走**同一条注入链**——手动即自动的调试入口
- 安全阀：注入间隔 ≥2s；连续 3 次未确认→跳闸；bus2 静默 >2s（车睡眠）不注入

## 四、0x248 注入器（bit46 同族范式）

1. **模板缓存**：bus2 RX hook 拦 0x248 → 缓存最近帧（新鲜度 ≤300ms 才可用）
2. **注入**：克隆模板 → 改 `MCU_lockRequest` 目标位 → 立即经 bus2 发出（<10ms，MCU 周期 ~1.3s 竞争窗口小）
3. **位布局参数化**：实测 b3 值域 {1,4,5,6} 超出枚举域(0-4)=复合字段——**注入位/掩码做成编译期参数，M0 补录后锁定**（见 §八）
4. **白名单纪律**：仅 unlock/lock 两个目标值；绝不碰 0x433（Summon）/0x213（SC 认证域）

## 五、在场检测双路线

| | V1 WiFi（首发标配） | V2 BLE（二期） |
|---|---|---|
| 原理 | 手机开热点，模块 STA 连接=在场；断连+去抖=离场 | 模块 NimBLE peripheral，手机 app 维持 GATT 连接；断连=离场 |
| 手机端 | **零开发**（iPhone 热点一键） | 需轻量 app（iOS 无 Web Bluetooth；Flutter 单页或快捷指令，二期定） |
| 耗电 | 热点常开 ~5%/h | BLE 连接 ~1-2%/h |
| 时延 | 关联 ~3-8s；断连去抖 20s | 连接 ~1-2s；断连去抖 20s |
| 模块侧 | STA 代码已有（P4 LAN 验证过可平移） | NimBLE 栈新增（上游 ev-open-can-tools v4.0.0-beta.2 passkey+NVS 范式可参考） |
| 风险 | 车库无信号热点仍可用（点对点）；模块连热点期间不连家里 WiFi | iOS 后台 BLE 连接被系统杀（需 background bluetooth mode） |

**建议**：V1 先行（体验立即可用），V2 评估后二期。检测源抽象成接口，`kl_presence_src` 配置切换，可叠加（OR）。

## 六、UI（沿用 5.0 UI 纪律：sel-cards/Stepper/i18n）

- **CAN2 控制页**新增「无钥匙进入」卡：总开关（off/manual/auto 三段 Stepper）+ 状态行（在场/离场/已解锁/已锁/跳闸）+ 最后动作时间戳 + 注入/确认计数
- **手机「现场遥控」页**新增「解锁 / 锁车」两按钮（manual 模式入口，M2 起可用）
- 全部文案 data-i18n 中英双语；状态色沿用 v-ok/v-err 语义

## 七、配置与 NVS

```
kl_enable(u8) 0=off/1=manual/2=auto   kl_presence_src(u8) 0=wifi/1=ble
kl_unlock_deb_s(u16) 默认3            kl_lock_deb_s(u16) 默认20
kl_inject_min_gap_s(u16) 默认2        （备份块 keyless 纳入 /backup）
```

## 八、M0 数据缺口（写注入器前必须补录，一次车边 8 分钟）

1. **S6 车机/App 解锁**（2 分钟）：屏幕解锁×3 + App 解锁×2（间隔≥10s，App 限速）
   → 锁定 0x248 b3 中 `MCU_LOCK/UNLOCK/REMOTE_*` 的落位与脉冲形态（值 1/2/3/4 如何映射到 b3 复合字段）
2. **S5 睡眠唤醒**（6 分钟）：fob 锁车→走开 5 分钟→回来录制→fob 解锁
   → bus2 静默→唤醒的帧序列（0x358 chBusState?），决定睡眠车「先唤醒再注入」时序或「不注入等 fob」兜底

## 九、里程碑

| 阶段 | 内容 | 依赖 |
|---|---|---|
| **M0** | 补录 S5+S6（Jordan 车边 8 分钟） | 无 |
| **M1** | 固件骨架：dash_keyless 模块 + 0x205 监听状态机 + 0x248 模板缓存 + UI 卡 + 状态显示（**注入 dry-run 预览帧不实发**） | 无（与 M0 并行）|
| **M2** | 注入器实装（位布局按 S6 锁定）+ 手动按钮激活 + 台架/车边验证（注入 1 帧看 0x205 响应） | M0 |
| **M3** | 自动模式：V1 WiFi 在场 + 全状态机 + 实车验收（走近自动解锁/走开自动锁） | M2 |
| **M4** | 睡眠适配（S5 时序）+ V2 BLE 评估 | M0/M3 |

**顺带同批（待批）**：幻影 Park 修复（0x118 档位源 b2→b3>>4，一行级+native 测试）与 M1 合并——已裁决定案，详见 adjudication-report-20261024。

## 十、测试纪律（5.0/6.0 惯例不变）

- `native_keyless` 测试 env：状态机全转移 / 去抖边界 / 跳闸 / 模板新鲜度 / 位布局参数化注入
- 合约测试：/keyless_config 端点 + /status 字段 + 备份块 + 版本钉
- UI 守卫：data-i18n 词条完整；`pio test` 逐 env（禁裸跑）
- 3 env 构建（lilygo_t2can_dual + waveshare_ESP32_S3_RS485 + waveshare_single_can_standalone）
- 资产+CHANGELOG 英文+CJK 自检；**不 push 不烧录不 OTA——待批**
- 实车验收四级：只读观察 → dry-run → 手动注入 → 自动模式

## 十一、安全边界（复述）

- 解锁≠开走：DST80 防盗独立加密，CAN 伪造骗不过——模块被盗也开不走车
- 仅信任绑定设备（V1 热点凭据 / V2 BLE 配对密钥 NVS）
- 走开去抖 20s 防误锁；注入白名单仅 0x248 两值；跳闸阀自动熔断

## 十二、待 Jordan 批准

1. 本方案整体（M0-M4 路线 + V1 WiFi 先行）
2. M0 补录时间（8 分钟车边）
3. 幻影 Park b3>>4 修复是否同批 M1
4. 版本号 6.1.0-keyless-beta01 认可
