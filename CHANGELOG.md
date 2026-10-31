# Changelog

## [Unreleased]

## [6.0.0-beta10] - 2026-11-01

### Fixed (P4)

- **P4: board no longer reboots every ~13 s when the C6 WiFi slave is down.**
  The arduino-factory default `ESP_HOSTED_TRANSPORT_RESTART_ON_FAILURE` was
  inherited into the P4 sdkconfig; with a dead or missing slave the host
  restarted itself in a loop, which would kill the CAN defenses while
  driving. The board now degrades to CAN-only instead. Field-proven on the
  C6-failure bench board: zero restarts over a 5-minute window with the
  slave link permanently down. Transport stays on streaming RX (packet
  mode was a diagnostic experiment only).

## [6.0.0-beta09] - 2026-11-01

### Fixed

- **P4-3CAN: desktop drive-status CAN3 card.** The phone UI showed the
  CAN C lane but the desktop Drive Status metric row did not (beta08
  shipped only the overview stat and the mobile card). The cockpit row
  now gains a CAN3 card next to CAN1/CAN2 behind `can3Available`,
  stretching the row to six columns; all other boards render exactly as
  before.
- **P4-3CAN: chip temperature now reads the on-chip sensor.** The tsens
  driver gate covered ESP32-S2/S3 only, so ESP32-P4 reported no
  temperature and the dashboard showed `--`. P4 joins the gate — its
  tsens attribute table covers the same -10..80 °C range the existing
  config requests.
- **Every env without an explicit product flag compiles again.** The
  `DASH_PRODUCT_NAME` fallback `#define` sat after its first
  string-concat use, so builds like `lilygo_t2can_dual` (no `-D`
  override) failed to compile; the beta08 golden gate had run before
  that commit landed and masked it. The fallback now precedes the first
  use and a contract test pins the ordering.

## [6.0.0-beta08] - 2026-11-01

### Added

- **P4-3CAN board: ESP32-P4 with dual native TWAI.** The firmware now
  builds for the Atlas-P4-3CAN board (Waveshare ESP32-P4 WiFi6 with the
  3x SN65HVD230 CAN transceiver board): CanA (X197 13/14) on TWAI0 as
  the primary vehicle bus, and CanB (X197 9/10) on a second native TWAI
  controller where the T-2CAN board uses its MCP2515 over SPI. Feature
  and UI parity with the T-2CAN dual build; the board name is the only
  visible difference.
- **P4-3CAN: CAN C observation lane - the third bus is live.** CanC
  (X179 18/19, PT powertrain bus) now runs on TWAI2 in listen-only mode:
  it never transmits and never ACKs, its frames never reach the vehicle
  handlers or evidence counters, and it exists so upcoming features
  (Ban Shield, autoshift) can observe the powertrain bus. On the P4
  board the overview gains a CAN3 status card, and the diagnostics
  live-frame view tags every frame with its bus (CAN1/CAN2/CAN3, also
  matched by the filter box). All other boards report
  `can3Available:false` and render exactly as before.
- **Recorder captures all three buses.** CAN C frames enter the
  recorder alongside CAN A/B with bus tag 3 in the CSV export; PT-bus
  records are excluded from the 0x399/0x045/0x3C2 evidence counters so a
  gateway mirror on the powertrain bus cannot inflate vehicle evidence.
- **Board-supplied product name.** `/system_status` now carries
  `product`; the UI brand follows it when present. The firmware default
  is the exact legacy "Atlas T-2CAN" string, so every existing board's
  UI is unchanged - the P4-3CAN board introduces itself as
  "Atlas-P4-3CAN" (was: hardcoded T-2CAN brand on every board).

## [6.0.0-beta07] - 2026-10-30

### Changed

- **DND reworked to one frame per action with per-episode direction
  alternation (Plan D).** Same-episode +1/-1 pairing is gone: every frame
  in a warning episode pushes the same way, and the direction flips only
  after an episode actually put a frame on the bus, so consecutive
  delivered episodes cancel (+n then -n) and the drift is bounded by one
  episode's frame budget instead of stranding half of a pair. All beta02
  safety rails stay (echo window, counter-conflict abort, physical-input
  abort, 300 ms release timeout, TX-failure auto-disarm); the 300 ms
  ceiling and the 3-frame budget are pinned by native tests against a
  repeat of the beta05 P0. WebUI diagnostics switch from pair bookkeeping
  (pending compensation / balanced pairs) to direction bookkeeping
  (episode direction, next direction, alternation count).

## [6.0.0-beta06] - 2026-10-29

### Changed

- **DND (wheel 0x045 volume compensation) rolled back to the beta02
  implementation.** Three field rounds (beta03 atomic pairing, beta04
  20 ms gap, beta05 600 ms gap + 1200 ms release timeout) all failed to
  stop the volume drift, and beta05 regressed P0: with the FSD gate
  enabled, activation stopped landing. DND now behaves exactly as in
  6.0.0-beta02 - the drift bug is known to be present and the rework
  continues on this baseline.
- **Flash burst retained.** The 0x292 STW_ACTN_RQ port, bus-off
  hardening, and NVS persistence (keys bk..bn) shipped inside the same
  beta03 chain but are re-ported unchanged: beta04 field evidence (FSD
  activation healthy with the burst port present) clears them as the
  P0 regression source.

### Fixed

- **P0 restored: FSD activation with the FSD gate enabled.** The beta05
  DND session could hold the 0x045 channel for up to 1.2 s (release
  timeout), overlapping the activation landing edge window; the rollback
  removes that hold entirely.

## [6.0.0-beta02] - 2026-10-25

### Fixed

- **DNS filter rules silently vanished in the car (UI only; storage was
  fine).** Field report on 6.0.0-beta01: opening Network Settings in-car
  showed an empty rules box with the aggressive profile card highlighted,
  reading as "config not saved / filtering dead". Bench verification of
  the very board proved the storage chain intact (12/4 rules persisted
  across reboot, dig queries blocked telemetry domains with 0.0.0.0), so
  the regression was a visibility gap: a single failed `GET /dns_rules`
  hit the early return in `loadGatewayDns`, and `fetchJson` only toasts
  after 3 consecutive misses while any successful poll resets the
  counter — a transient AP-side fetch failure (ESP32 single-radio STA
  reconnect scans in-car) left the HTML defaults on screen with zero
  feedback. Fixes: retry once after 500 ms; on persistent failure set
  the DNS status triplet to an error state ("Load failed / tap Refresh /
  rules not loaded") and clear profile-card highlights; the HTML default
  profile card is now conservative-active to match factory auto-fill
  (was aggressive-active). Three new i18n entries. No firmware-C++,
  driving-engine, or persistence changes of any kind.

## [6.0.0-beta01] - 2026-10-22

### Fixed

- **JITTER cancel loop killed the FSD landings it produced (Plan A).**
  With the anti-jerk procedure armed, field data from 2026-09-28
  (recordings (7)/(8)) showed FSD survival near zero: after a successful
  0x42 re-engagement the 5 s deadline expiry zeroed `cycles_`, so the
  car's own s2->s3 re-engage (stalk held, no human input) opened a fresh
  period and the s6 lane capture was cancelled again — 14 of 25 captures
  killed 1-19 ms in, one self-driven loop every 10-13 s. A 3/6 landing
  this drive session (`landedThisSession_`) now keeps the cancel gate
  spent across the timeout; only a session boundary (Park edge, permit
  loss, gate off, switch off, apError) re-arms it. Park re-arms even
  with no cycle running. One cancel + one re-request per session, the
  original LittleGong intent; the 45-degree abort and carrier lock are
  untouched. (This fix plus the UI work below ship together as the 6.0
  line because the driving-engine change resets the minor-version
  baseline.)

### Removed

- **Light stunts page (Option A, conservative).** The pg-strobe page, its
  sidebar/mobile navigation entries, the fog-light strategy backend
  (`/fog_light`, `/strobe_cont`, `handleFogLight`, `handleStrobeCont`,
  `dash_fog_light.h`, `t2canFogLightTick`) and the `lt_fog` NVS keys are
  gone. Everything on the CAN2 control page — flash burst, stalk test,
  service mode — is preserved unchanged.
- **Power management module.** `dash_power_mgmt.h`, `/power_mgmt`, the
  hardware-page power card, auto-shutdown / Wi-Fi auto-off state, and the
  backup `power` block are removed from firmware and UI.
- **Legacy 0x370 NAG main-path UI.** The defense-page card, toggle,
  runtime-status grid, `/status` legacyNag JSON blob, and the
  `bionic_steering` branch of `/defense_config` are removed; the engine
  (handlers, bionic steering, v4 continuous-absolute) stays compiled but
  dormant with `bionicSteering` defaulting to false.
- Emergency controls consolidated: the overview-page card moves into the
  desktop home and the mobile field-remote page (no duplicate card).

### Changed

- **UI restyle rolled back to the beta03 look.** An interim
  square-corner "Tactical Telemetry" theme was rejected in review; the
  rounded radius scale (xl 28px / lg 20px), the purple `#7c3aed`
  accent with cyan/indigo gradients, and the translucent glass panels
  with backdrop blur are restored. Monospaced data values and
  micro-label tracking are kept; semantic reds (danger buttons,
  hold-to-arm, error states) remain untouched.
- **Daylight mode rebuilt.** Theme switching now toggles a root `light`
  class driving two complete CSS variable sets (including the cockpit
  layer, previously stuck dark); 26 hardcoded dark rgba values became
  variables. The light palette keeps paper surfaces (`#F4F4F0`) with
  carbon ink, now carrying the same purple accent as dark mode.
- **Bilingual switching rebuilt.** Static copy carries `data-i18n`
  attributes (~490 nodes), dynamic strings route through `T()`, the I18N
  dictionary grew to ~600 entries, and a language switch re-renders the
  cached status data. English mode now leaves zero Chinese residue on all
  ten pages (pytest guards included).

### Added

- Overview cockpit gains a CAN2 status block (5-column row on dual-CAN,
  hidden on single-CAN) and font-size auto-fit so metric values never
  wrap to two lines.
- Speed strategy page: the Legacy smart speed-offset card moved above the
  mode selector. CAN2 page: Service Mode moved to the top with a live
  status line.

## [5.0.0-beta03] - 2026-09-29

### Added

- **EAP-landing bit46 one-shot upgrades a restalk to FSD in place.**
  Field data from two instrumented drives (E7/E8) showed every human
  restalk during the disengage window landed EAP (13/13) — the state-3
  acquisition window saw no bit46 because the paused base path is the
  procedure's own doing — and the vehicle upgraded to FSD within
  14-104 ms whenever a bit46 frame arrived while engaged (11/11). The
  state-3 landing edge now queues one clone (same fail-closed template
  freshness gate as the arming shot) so the driver no longer waits out
  the 5 s deadline tail for the base path to resume. New diag counter
  `bit46LandingShots` (expected to track `reengagedEap` in the field);
  `bit46Shots` and `bit46Refreshes` stay split.

### Fixed

- **Parked no longer rises on a single corrupted gear frame.** One
  glitched 0x118/0x186 frame decoded as Park mid-drive (observed in
  the E7 recording while the car was rolling) reset a running JITTER
  cycle, swallowed its re-request, cleared the session budget, and
  spuriously opened the AP injection gate. Parked now rises only after
  3 consecutive P readings shared across both gear sources (280 and
  390); any non-P gear clears it instantly, and a Parked that is
  already true (the boot default while the DI is asleep) stays true.

## [5.0.0-beta02] - 2026-09-20

### Fixed

- **Power management switches can now actually be toggled.** The auto
  shutdown and Wi-Fi auto-off switches passed a pre-concatenated string
  to the `postForm` helper, which expects an object: `for...in` over a
  string iterates character indices, so the request body degenerated to
  `0=t&1=r&2=u&3=e` garbage, the backend never saw the `autoShutdown` /
  `wifiAutoOff` arguments, and the poll loop snapped the checkbox back
  off. Both toggles (and the HW3 slew-rate toggle, which had the same
  latent bug) now post proper object payloads. This bug dated back to
  the Phase 1 power UI and is why the switches appeared dead in
  addition to the runtime fixes in beta01.

### Changed

- **Power management card moved to the hardware page.** The card lived
  inside the desktop-only overview column, so on phones the two power
  switches were unreachable. It now sits on the hardware page — which
  both the phone "more → hardware" tab and the desktop sidebar reach —
  with element ids renamed `ov-*` → `hw-*` (`hw-auto-shutdown`,
  `hw-wifi-auto-off`, `hw-pwr-idle`). The read-only idle line moved
  with it; behavior and backend contract are unchanged.

## [5.0.0-beta01] - 2026-09-20

### Changed

- **Overview page slimmed down.** The "Legacy runtime status" card, the
  "Legacy CAN-A diagnostics" card, and the five experimental switches inside
  the "FSD injection policy" card (mux1 injection, profile write, vision
  limit clear, legacy speed offset, speed limit override) are removed from
  the UI. The three policy cards and four policy status tiles remain.
  Backend endpoints and `/status` JSON keys are unchanged.
- **AP injection safety block relocated.** The overview-page AP injection
  safety card now lives on the FSD defense page, directly under the AP gate
  controls. All inner element ids are preserved, so rendering is unchanged.
- **Emergency controls promoted to the overview page.** The "Emergency
  controls" card (device reboot, counter reset) moved from the drive style
  page to the bottom of the overview page and is now visible on both the
  in-car and phone layouts. The phone remote page keeps only field-safe
  operations.
- **Hardware page AP restore toggle consolidated.** The AP state restore
  card was removed from the hardware page; the equivalent toggle on the FSD
  defense page (`ap-auto-restore-tgl`) remains the single control point for
  the same backend setting.
- **Power management card observability.** A read-only line in the power
  management card now shows CAN idle seconds, web idle seconds, and whether
  STA was auto-disabled, fed by new `/status` keys `canIdleMs`,
  `webIdleMs`, and `wifiStaDisabled`.

### Removed

- **Five legacy experimental switches are also reset once in NVS.** A
  one-time migration (marker `ui_rst_50b1`) clears the persisted values for
  mux1 injection, profile write, vision limit clear, legacy speed offset,
  and speed limit override on first boot of this firmware, so previously
  stored experimental states can no longer take effect silently.

### Fixed

- **Wi-Fi auto-off actually sticks now.** Previously the dashboard's STA
  reconnect loop forced `WIFI_AP_STA` back within seconds of the idle
  shutdown, and opening the web panel again cleared the disabled flag with
  no restore path. The reconnect loop now stays suppressed while STA is
  idle-disabled, and web activity (opening the panel) restores STA within
  about a second.
- **Auto shutdown wake polarity corrected.** The deep-sleep ext0 wake on
  the CAN RX pin was configured for level 1, which is the recessive
  (idle) bus level, so the device woke immediately and auto shutdown never
  held. Wake is now armed on level 0 (dominant, i.e. real bus traffic),
  and a serial log line is emitted before entering deep sleep.

## [4.5.0-beta15] - 2026-09-15

### Removed

- **8.3.6 防甩协调器家族整体移除（约 4,300 行）**，由单 CAN v1.19.2 实车冻结的
  JITTER 程序逐字移植取代（见 Added）。路线 B 论证：beta13 实车 30 轮 70% 成功、
  9 次失败，beta14 的计数器容差/重分类/轮内重试三方案全是给协调器续命的生命
  维持——而单 CAN 同车同总线同拨杆的 JITTER 已「开启上百次、0 例甩方向」
  （v1.19.1 起机制冻结），继续修协调器等于在已验证方案旁边维护一套未验证方案：
  - `include/dash_ap_rerequest_activation.h`（1183 行，beta01..beta14 协调器本体
    含 2026-09-10/11 实车标定 profile 与 beta13/14 全部容差补丁）
  - `include/dash_ap_blackbox.h`（229 行，beta10 NVS 黑匣子）
  - `scripts/analyze_rerequest_045_calibration.py`（442 行）+ 其 5 例测试
  - `test_native_ap_rerequest`（68 例）/`test_native_ap_blackbox`（21 例）与
    platformio.ini 对应两个 env
  - JSON `apReRequest` 对象（含 beta10 permitInputs/tickHeartbeat/blackbox 观测层）、
    /defense_config 平键 `ap_re_request`、NVS `apr_on`（载入时一次性迁移清除）与
    `aprbbx` 命名空间（一次性清空）、`FsdGateBlockReason::ReRequestHandshake`（值 9
    留空有注释）、UI 12 格 ap-rr-* 诊断面
  - 两份 AP-REREQUEST 流程文档顶部加「已被取代」注记（留史）

### Added

- **8.3.6防甩机制（JITTER 程序）**：`include/dash_jitter_procedure.h` 逐字移植自
  单 CAN v1.19.2（593 行，实车验证：开启上百次、0 例甩方向；CRC8-J1850、
  0x399 事件驱动、carrier-lock 0x045 泵、bit46 武装单发 + 重挂前刷新、
  FSD/EAP 落地拆分计数、失败静默复位无重试无锁定）+ 39 例 native 测试
  （`native_jitter` env，逐字）。接线适配全部在包装层：
  - **四路观测全部 VEH 总线过滤 + DLC 卫**（0x399/0x3EE/0x045/0x488）——PARTY
    副总线镜像帧不得驱动机器（单 CAN 无此维度）；0x045 短帧不得克隆进泵帧
  - **0x488 转向角三位形态** `((b2>>5)&0x07)==2`（单 CAN 实车验证形态）；
    LegacyHandler 0x370-NAG 的两位解码是另一条路径，两处注释声明差异有意
  - bit46 暂停：`dashLegacyFsdActivationAllowed` 头部 `procedureActive()` 检查，
    在 `!apInjectionGate` 早真**之前**（顺序钉）；暂停报 LegacyFsdSettle
  - Barrier A：`jitterInhibit3ee` + `blockedReason="jitterProcedure"`（mux0/mux1）
  - 滚轮 DND 仲裁：`procedureActive()` 期间 JITTER 独占 0x045
    （`StalkOwnedByReRequest`→`StalkOwnedByJitter`，窗口宽度不变）
  - 诊断：`legacyInjectionSafety.jitter` 22 键（requested/effective/apGateOpen/
    permit/phase/reason/apState/cycles/cancels/requests/reengagedFsd/reengagedEap/
    bit46Shots/bit46Refreshes/pumpFrames/txOk/txFail/steerAborts/steerResets/
    apErrorResets/timeoutResets/failedCycles），链式引号规则逐字移植；
    UI 9 格诊断面 + jt 原因/阶段映射；模拟器 22 键静态镜像
  - NVS `jtr_on`（默认关，fail-closed）+ `apr_on` 迁移 + `aprbbx` 清理
- 模拟器修正：`otaWatch`/`carrier3ee`/`txAccounting` 原误嵌在 apReRequest 对象
  内，现提升为 `legacyInjectionSafety` 直接子键——与固件发射形态一致
- `reconstruct_status_json.py` 加固两处：`extract_function` 现识别 C++ 原始字符串
  字面量（beta14 的引号奇偶「碰巧通过」被消除——本次即抓获一处真实链式引号
  缺陷，第六次 JSON 事故防于未发）；FUNCS 白名单补 `appendJitterDiagJson`
- CI native 矩阵重建：16 条 stale（含 2 条不存在 env）→ 22 条与 platformio.ini
  逐项对账一致
- **clang-format 棘轮清零（独立 style commit）**：beta01 起的手写代码从未
  过格式化（2026-08-09 起已知漂移、此前靠手改绕行），Phase 0 全量入库后
  CI lint 会红（29 文件 717 处，本地 v22 与 CI 同款 v18 判定一致）。批量
  格式化为纯空白/折行改动；冻结件 dash_jitter_procedure.h 与
  test_jitter.cpp 原本就干净、未动一字；6 处合约钉随新格式更新
  （语义零变化）

### Caveats

- **双 CAN 上独立 0x3EE bit46 TX 属新行为**（协调器只改活帧、从不独立发 0x3EE；
  单 CAN 同车同总线已实车验证同款独立 TX）。首场实车读 bit46Shots/Refreshes 与
  FSD/EAP 落地计数；急停杆 = 关开关（立即 fullReset("off")）
- 无 own-echo TX 指纹（单 CAN 实车验证过无指纹形态，逐字移植决策）——列入实车
  观察项：若 pumpFrames 异常膨胀再加回指纹补丁
- 「不甩」结论为单车小样本表述（同一辆 CN 2026.8.3.6 车辆、同一驾驶人），
  不作跨车推广
- 双 CAN 本机全部验证为构建/native/pytest 级；实车未验证

## [4.5.0-beta14] - 2026-09-14

### Changed

- **8.3.6 防甩协调器实车健壮性三方案（beta13 实车分析定案；2026-09-13
  四份 CSV、~30 轮 70% 成功、9 失败全为车辆侧瞬态 + 1 自检误判，一次
  修到位；预期失败率 31%→<10%）**：
  - **计数器 +2 容差（方案一）**：取消 release TX 后原生模块广播暂停
    213-290ms、恢复时跳一号，被旧「∈{lastTx, lastTx+1}」检查误判为
    counterConflict 硬锁（104 手势 3 例 +2 跳变全良性）。现环形比较
    `ahead=(native-lastTx)&0x0F`：+2 容忍并计 `counterSkips`（诊断新键）；
    冲突=原生落后（ahead=15，beta04 真冲突形态）或跳前超 2，仍硬锁。
  - **车辆侧失败重分类（方案二）**：请求被拒（`apExitedDuringRequest`，
    重试预算耗尽后）与车无视取消（`cancelIgnored`，state3 持续到证据
    超时）纳入 `vehicleSideRearmReason` 自动重武装集合（beta06 四成员
    扩为六成员，`kVehicleAutoRearmMax=3` 上限不变）——实车「报错退出」
    硬锁体感消失。证据超时分类精化：`cancelEvidenceDeadlineLocked()`
    超时时车仍 active → `cancelIgnored`（重武装）；否则维持
    `cancelEvidenceTimeout` 硬锁。
  - **轮内重试（方案三）**：①请求重试——请求被拒（2→1）回等 state2
    （车需 ~940ms 回可用态）自动重发请求手势，每轮 ≤2 次（profile
    `requestRetries`，预算按已用计，0=beta13 行为严格不变）；覆盖
    RequestSequence 与 WaitQualification 两相位（0x399 2Hz 量化使拒绝
    多在 release 后才被检出）。②取消重发——取消 700ms
    （`cancelRetryDelayMs`）无证据且车仍 state3 → 重发一次取消手势
    （`kCancelRetryMax=1`），证据超时延至 2200ms
    （`cancelRetryTimeoutMs`）。LittleGong 固件的「稳定」本质是人肉
    重试，这三项把那份耐心装进自动化。
  - **安全不变式全部保持**：Barrier A、物理拨杆锁、8/9 故障态锁、
    permit 链零改动；profile 新字段默认 0 严格等于 beta13 行为。
- **profile 结构体去填充（memcmp 保留路径守卫）**：新增 retry 字段在
  原布局打开对齐填充孔，`configure()` 的同 profile memcmp 比较读到
  旧栈字节 → 意外全重置（native 测试显形）。字段按 4→2→1 字节降序
  重排为无填充布局，`static_assert(sizeof(ApReRequestProfile)==40)` 钉死。

### Added

- **重试诊断三键**（`/status` 与 `/defense_config` 的 apReRequest 块）：
  `counterSkips`（良性 +2 跳变计数）、`cancelRetries`（取消重发已用
  次数）、`requestRetries`（请求轮内重试已用次数）；UI rrPhaseMap 补
  `requestRetryWait`（请求被拒·轮内重试等待）、`cancelRetryArmed`
  （取消被无视·重发已武装）、`cancelIgnored`（车辆无视取消·已自动
  重武装）三条中文条目；simulator 镜像同步。验证规程判读表新增对应
  四行并区分 `cancelIgnored` 与 `cancelEvidenceTimeout`。

## [4.5.0-beta13] - 2026-09-13

### Fixed

- **permitInputs 缺失闭合花括号（第五次 JSON 破损事故；beta12 实车
  「未连接 + 配置全灰 + RX/TX 0/0」的根因）**：beta10 观测层新增的
  `,"permitInputs":{` 五输入分解对象从未闭合——legacyInjectionSafety
  永不闭合、root `}` 被错位消费，`/status` 与 `/defense_config` 从
  首次 boot 起整体不可解析（页面顶栏「未连接」、配置复选框全回 HTML
  默认灰 OFF、RX/TX 停 0/0——静态页走 gzip rodata 通道正常送达，故
  页面能开而数据全死；症状与车/CAN/WiFi 无关，模块插不插车一样）。
  beta10 实车时发现的 reason 收尾引号（beta11 单字符热修）是同一
  增量内的**另一个** bug，修完它本 bug 仍在并随 beta12 发布。修复 =
  在（双驱动构建独有的）`driver` 键后补 `j += '}'` 并拆分
  `#ifdef DRIVER_T2CAN_DUAL`（非双驱动构建 4 输入同样闭合）；
  permitBits/tickHeartbeat/blackbox/otaWatch 维持 apReRequest 层的
  兄弟键位置（与模拟器镜像及 UI 读取一致）。**8.3.6 防甩语义零改动，
  LittleGong 四项移植本体无恙（全部经重建器机械验证）。**

### Added

- **JSON 发射机械重建验证器 + pytest 常驻守卫（此类事故的结构闭合）**：
  `scripts/reconstruct_status_json.py` 按源序逐语句重建
  handleStatus / dashDefenseConfigJson / handleVehicleOtaStatus（含全部
  内联 builder、宏三明治/顶层三目/String() 调用占位建模）并对字面量
  骨架做 json.loads 整体验析——本 bug 修前在两处精确报出 EOF 截断，
  修后 ALL PARSE CLEAN（93/21/6 顶层键，permitInputs 恰五输入、
  四观测块兄弟形状与意图对齐）。合约新增
  `test_beta13_permitinputs_close_reconstruction_guard`（闭合位置
  order-pin + 模拟器兄弟形状 + 子进程跑重建器断言退出码）。dashboard
  头 native 不编译，此前无任何自动化整解析门。

## [4.5.0-beta12] - 2026-09-13

### Changed

- **OTA 守卫三路径改造（LittleGong 逆向移植 1；beta08 症状 2/3 头号
  嫌疑的行为修复）**：原清除条件是「连续 6 帧非 OTA 的 0x318」——若
  车辆侧 0x318 停发（bus 静默 / 网关降级），标志永久卡 true、permit
  静默死亡、仅拔插可恢复。三条清除路径（permit 表达式**逐字不动**，
  全部实现在守卫状态机内部 vehicleOtaActive 的清除条件上）：A. 原
  6 帧清除路径保留；B. **停流超时降级**——置位持续满 10 分钟且期间
  零 0x318 帧（任意 dlc 都算载波存活）自动清除 + `staleClears` 计数
  + CAN 任务日志；C. **用户覆盖开关**——`/defense_config` POST
  `ota_guard_override`（NVS 短键 `otg_ovr`）+ UI「忽略车辆 OTA 守卫」
  （诊断徽标），开启=立即清除并暂停置位，关闭=干净重来（真 OTA 仍会
  被 3 帧确认重新捕获）。`otaWatch` 与 `/vehicle_ota_status` 扩
  `staleClears` / `override` / `blockedMs` 三键。
- **黑匣子条目 JSON 发射挪入纯函数（LittleGong 移植 5，结构加固）**：
  条目 JSON 由 dashboard 头内联 String 拼接（native 测试不编译该头，
  beta06 / beta07 / beta10 三次同类引号事故全部从这里漏网）改为
  `dash_ap_blackbox.h` 内 `dashApBbEntryToJson` 纯函数发射，native
  golden-string 测试逐字符钉死全部 7 种 kind × 有/无 reason 的精确
  输出；buffer 不足返回 0（条目跳过，绝不产生截断 JSON）。合约改钉
  dashboard 调用形式并负向钉死旧内联形式。

### Added

- **0x3EE 载波节奏观测（LittleGong 移植 2；症状 1「只出现 EAP」= 资格
  窗内载波缺失的判读依据）**：`/status` 新增 `carrier3ee` 块
  （lastGapMs / maxGapMs / longGaps / total / lastMs / ageMs），过滤
  口径与资格注入看到的流完全一致（0x3EE mux0 CAN_BUS_VEH），
  longGap 阈值 2000ms 严格大于。纯观测，批准形态=只 JSON 不加 UI 格。
  新增 native `3ee_cadence` 测试环境（12 用例：过滤口径 + 间隔统计 +
  2000ms 边界）。
- **注入 TX 会计（LittleGong 移植 3；症状 3「长时间运行后无法注入」
  的鉴别数据）**：`/status` 新增 `txAccounting` 块（attempts / ok /
  fails / consecutiveFails / lastOkAgeMs），账本 = legacyFsdDiag.mux0
  （recordMuxTx 仅在 activation-allowed 路径调用，即 bit46 注入帧专用，
  速度偏移-only 写入另计不混入）。
- native `ota_guard` 测试环境（20 用例：三路径 + 599999/600000 边界 +
  任意 dlc 盖章 + override 全行为），`ap_blackbox` 扩 6 个 golden-string
  用例；模拟器镜像全部新键与 override 清 vehicle_ota 行为；合约新增
  `test_beta12_littlegong_port_ota_guard_carrier_tx` 双向钉（permit
  逐字钉保持 + 新键裸形式正向 / 游离引号负向 + NVS/POST/UI/模拟器全
  链）。

## [4.5.0-beta11] - 2026-09-13

### Fixed

- **黑匣子 reason 字符串收尾引号热修（实车 beta10 配置保存与页面显示
  全面异常的根因）**：beta10 新增的 NVS 黑匣子条目发射段中，
  `"reason":"` 打开后拼接 `e.reason` 便直接连接 `'}'`——每条 entry 产出
  `"reason":"xxx}` 的未闭合字符串，吞掉其后所有 JSON 内容；boot 条目
  开机即写入、黑匣子永不为空，故**首次 boot 起 /status 与
  /defense_config 整体不可解析**（保存链本身未坏，与 beta06→beta07
  同签名）。修复 = 收尾块补一行 `j += '"';`（单字符级改动，8.3.6
  语义与 beta10 观测层零改动）。合约新增 5b「字符串值收尾纪律」双向
  钉测：kind 值经 `","ms":` 字面量收尾、reason 值经显式 `j += '"';`
  收尾的精确形式正向钉 + `e.reason;` 直连 `'}'` 的坏形式负向钉——
  钉测盲区（历来的键前缀钉从未覆盖字符串值**收尾**）在第三次复发
  （beta06 散引号 / beta07 数值后字符串收尾形 / beta10 缺收尾引号）
  后终于闭合。

## [4.5.0-beta10] - 2026-09-13

### Added

- **P1 观测层（纯观测，8.3.6 防甩语义零改动，permit 表达式逐字钉死）**：
  2026-09-13 实车反馈三症状中，症状 2/3（强退 FSD 后 / 长时间运行后
  注入失效）开关循环无效、仅拔插断电可恢复——挂死态在协调器**之外**
  的 RAM 里，重启恰好销毁证据。beta10 只加观测不加行为，四处埋点：
  ① `/status` 诊断新增 `permitInputs`（canActive / legacyHandler /
  otaGuardClear / abortGuardOk / driver 五输入逐项分解）与
  `permitBits` 位图——下次再「无法注入」，一眼看出五个输入哪个断了；
  ② 新增 `tickHeartbeat`（count / lastMs / ageMs）——协调器 tick
  心跳，停滞即说明 CAN 任务层饿死；③ 新增 `otaWatch`
  （seen318 / last318Ms / confirm / clear）——观测 OTA 守卫的 0x318
  输入流。**重要更正**：OTA 守卫并非此前误报的"死代码"，它实际接线
  于 CAN 帧分发器（dashOtaGuardProcessFrame），且其清除逻辑要求连续
  6 帧非 OTA 的 0x318——若置位后 0x318 停发则守卫**永久卡 true**、
  permit 静默死亡、无任何 UI 提示，与症状 2/3 完美吻合，列为头号
  嫌疑（现有 `/vehicle_ota_status` 端点即可查证）；
  ④ **NVS 黑匣子**（`blackbox[8]` 环形，跨断电存活）：记录
  Boot / 心跳(5min) / RoundStart / RoundEnd / Lock / PermitChange /
  IntentChange（flappy 种类 5s 限速），每条含 seq / 时刻 / tick /
  phase / permitBits / apState / reason；开机 NVS 恢复并打 Boot 标记
  ——意外断电后第一次 `/status` 即显示断电前最后的证据。
  UI 防护页诊断网格新增「许可链」「心跳」两格（许可链断时直接中文
  点名断的输入）。协调器模块 dash_ap_rerequest_activation.h 零改动
  （合约钉死无 Blackbox 字样）；新增 native 套件
  test_native_ap_blackbox（15 用例：栈序/容量/限速/往返/截断/续号）。

## [4.5.0-beta09] - 2026-09-12

### Added

- **「车辆响应」观测（P1，纯观测零注入改动）**：第二份 Tier 1 CSV
  （162535.907）定位「有时开启失败」的根因为车侧拒激活——一次原生
  RWD 按杆后车 6ms 内翻起 flag1、2.3s 才回 state2、永不进 state3，
  设备全程正确静默（无激活边沿=无轮次），但驾驶者无从区分"车拒绝
  了"与"盒子坏了"。现在 WaitDriverIntent 期观测到原生 RWD 按压
  边沿后 2.5s 内无 state3-6 激活边沿，UI 诊断网格新增的「车辆响应」
  格显示「未接受激活 · 车侧拒绝」（等待期显示「等待车辆响应…」），
  `/status` 诊断新增 `vehicleRefusal` / `rwdPressPending` /
  `vehicleRefusals` / `rwdPressMs`。按住保持期间的 10Hz 滚计数器帧
  只计一次按压；FWD 不追踪（取消总是可观测的 6→1）；轮次进行中的
  按杆走既有 physicalInput 路径不触发本观测；下一次按压或激活边沿
  清除拒绝态。不改任何门控/超时/注入逻辑——8.3.6 防甩语义零改动。
- **0x399 高半字节 flag 位计数（P2，纯观测）**：`/status` 诊断新增
  `apFlagCounts[16]` 直方图 + `lastApFlag` / `lastApFlagMs`，统计
  0x399 byte0 高半字节每个值的帧数（此前在 dashboard 层被剥掉丢弃）。
  跨两份 Tier 1 CSV 的假说：flag1=驾驶员手力/扭矩、flag4=停车/不可
  用、flag5=剧烈捕捉后瞬态——语义未破译，计数为后续实车 CSV 对照
  破译铺路。Disabled（含门控关）也计数。

## [4.5.0-beta08] - 2026-09-12

### Fixed

- **手机底部标签栏滚动后消失（iOS Safari）**：两条底部标签栏
  （`#mob-tabs` / `#mob-tabs-dual`）与两个"更多"面板是
  `position:fixed` 元素，却嵌套在 `.content`（`overflow-y:auto`
  滚动容器）内。iOS Safari 在长页面惯性滚动期间不重绘滚动容器内
  的 fixed 元素，滚动后标签栏渲染空白——即现场"标签栏消失"
  （截图 4376/4377 滚动到底部时栏消失、4378 浅位置正常）。
  移植单 CAN v1.0.7 的 `hoistMobileChrome()` 修复：DOM 加载时把
  4 个 fixed chrome 重新挂到 `<body>` 直接子级，`position:fixed`
  稳定锚定视口。`#mob-tabs` 改为自带 `data-dual-hide`（原包装
  div 删除），产品模式显隐行为不变。布局逐像素零变化
  （430×932 探针：栏位置/尺寸/层级/预留与修复前一致）。

## [4.5.0-beta07] - 2026-09-12

### Fixed

- **`/status` and `/defense_config` JSON was unparseable** (beta06
  regression, confirmed on hardware): the new `autoRearms` diag field is
  numeric, but the following `round` literal kept the string-close form
  `","round":` — one stray quote emitted `..."autoRearms":0","round":...`,
  breaking JSON parsing device-wide. UI impact matched the field reports:
  every switch rendered OFF after re-entering a page (`loadDefenseConfig`
  bailed on the parse, leaving checkboxes at their HTML defaults) and the
  top-bar badge stuck on 未连接 (`poll()` got null from `/status`).
  Config saves themselves were never broken — the device RAM/NVS state was
  correct the whole time. Fix: bare `,"round":` after the numeric value;
  contract test now pins both boundary forms (string-close after string
  values, bare after numeric/bool) so the substring-shaped blind spot that
  let this ship green cannot recur.

## [4.5.0-beta06] - 2026-09-12

### Changed

- **Qualification window widened 1200 → 2400 ms** (beta05 Tier 1 CSV,
  f2 round A): the only in-window mux0 carrier landed inside the
  press→release gap and the next carrier arrived 2000 ms after t_request —
  the 1200 ms window still expired on ~12% of rounds. 2400 ms covers the
  observed worst case with margin; AP-exit races inside the longer window
  stay fenced by the existing `apExitedDuringWindow` guard. Window
  semantics (half-open, from t_request) unchanged.
- **Vehicle-side locks auto re-arm** (fix 1): `windowExpired` /
  `apExitedDuringWindow` / `apActiveDuringWait` / `apFaultState` now
  re-arm to WaitDriverIntent with the same fence a completed round uses
  (full AP exit + driver intent + activation edge before a new round) —
  no more manual 8.3.6-switch cycling after every transient vehicle
  hiccup. A cap of 3 consecutive vehicle-side locks without a completed
  round between them degrades to a real FailedLocked
  (`reason="autoRearmLimit"`) so a persistently unhappy vehicle cannot
  loop the cancel/re-request chain forever. Hardware-class reasons
  (`counterConflict`/`txFailed`/`permitLost`/`physicalInput`/
  `invalidNative045`/`dasStale`/timeouts) still lock fail-closed.
- **configure() preserves an ActiveInjection round** (fix 3):
  dashApplyRuntimeState re-applies coordinator state on every /config
  POST; the old unconditional reset killed the bit46 chain mid-injection
  (prime suspect for the beta05 rounds where injection stopped while the
  car still held state 6). Preservation is strict — same enable + same
  gate + byte-identical valid profile, ActiveInjection phase only; every
  real mode change still takes the full reset.
- **AP-settle delay chain deleted end to end** (user request): the
  0/1/2/3 s "延迟注入" select existed only to damp the wheel jerk on the
  old direct path; with the 8.3.6 coordinator owning injection timing it
  is dead weight. Removed from the UI (select + settling state + stable
  timer readouts), `/config` POST param `ap_delay_ms`, NVS key `ap_dly`
  (actively removed on load — one-way migration), the settle timer, the
  `legacy_ap_state_delay_gate` variant, and the simulator mirror. With
  the gate open + 8.3.6 switch off, the legacy direct path now allows
  immediately once 0x399 confirms AP active.

### Fixed

- **Stale cancel-evidence latch could false-lock round 2+** (barrier B):
  startRound did not clear the previous round's latch, so a stale seen=true
  plus an active state observed during round 2's WaitCancelEvidence
  (observed 22-526 ms before the cancel takes effect) fired a false
  `apActiveDuringWait` lock. Each round now starts with a fresh latch.
- Module State/Diag compile fix: the beta06 diagnostic fields were added
  to Diag but State lacked `reason`/`lastEndedReason`/`lastAction`
  initializers.

### Added

- **Diagnostics**: `lastEndedReason` (survives re-arms and state changes —
  the field answer to "record the reason before touching any switch" from
  the beta05 Tier 1 lesson) and `autoRearms` counters in /status +
  /defense_config, the simulator, and two new UI diag cells
  (上次结束原因 / 自动重武装); the four vehicle-side reasons localize with
  a "已自动重武装" suffix and `autoRearmLimit` gets its own row.
- Eight native tests: window-expired re-arm + round-2 full fence,
  re-arm cap → autoRearmLimit, completed-round budget reset,
  hardware-class locks still lock, round-2 latch freshness (barrier B),
  configure preservation / reset conditions / handshake-phase
  non-preservation.
- Contract pins for the whole beta06 surface (module re-arm set + cap +
  memcmp preservation, dashboard JSON keys, UI cells, simulator mirror)
  and anti-regression pins for the deleted delay chain.

## [4.5.0-beta05] - 2026-09-11

### Fixed

- **Stale last-TX anchor fixed** (beta04 Tier 1 CSV, round 2): the ring
  comparison from beta04 had no time gating, so the previous round's
  request-release counter (83 s old) still "led" the fresh native template by
  1 on the 4-bit ring and the round-2 press skipped the counter the car was
  waiting for (device sent 7 while the car echoed 6) — the car ignored the
  frame and the await-native check locked the round `counterConflict` with
  the release never sent. That ring relationship hits with ~56% probability
  per subsequent round. The anchor is now a **time-recency rule**: anchor own
  last TX only when it is newer than the last native template refresh (the
  beta03 1 ms-collision shape, where the native bus has not spoken since our
  TX); otherwise always anchor template+1 — the car accepts its own current
  counter +1 (round 1's press/release/request echoes proved it three times).
  `ringAhead` is deleted; time separates stale from current, the ring cannot.
- New native regression: full round 1 completes, 83 s of native traffic, round
  2's template ring-lags the stale last TX by 1 — the press must anchor
  template+1, the car's echo must not read as a conflict, and the cancel
  release (which the CSV round 2 never sent) must follow.

## [4.5.0-beta04] - 2026-09-11

### Changed

- **Re-request now waits for the available state** (Tier 1 CSV root cause A):
  the first real-vehicle round fired the request press 1 ms after the cancel
  release while the car was still in DAS state 1 — it holds state 1 for
  ~936-950 ms before returning to state 2, and all 17 calibrated successful
  RWD engagements started from state 2. New profile fields
  `requestApMask` (compiled `{2}`) + `availableWaitTimeoutMs` (3000 ms ≈ 3×
  the observed recovery): the cancel evidence only latches the wait
  (`reason="waitAvailable"`); the request gesture is emitted once the car
  reports an available state. Locks fail-closed on
  `availableWaitTimeout` (never came back available) and
  `apActiveDuringWait` (cancel landed, then AP re-engaged on its own — the
  re-request is pointless; pre-evidence state 3 still just means the cancel
  has not taken effect and keeps waiting). The cancel timing itself is
  unchanged (accepted by the vehicle at 22/31 ms in both rounds).
- **Rolling-counter collision fixed** (root cause B): a fresh press anchored
  its counter on the native template alone, so when the native bus cloned
  our cancel press inside the release gap the request press repeated the
  release counter (two frames 1 ms apart with the same counter, in both
  failed rounds). A fresh press now anchors on whichever of
  (template, own last TX) is ahead on the 4-bit ring (`ringAhead`), so our
  own frames always advance.

### Added

- `rrPhaseMap`: the coordinator status row now localizes every
  `phase/reason` into Chinese for Tier 1 field triage (including the three
  new reasons above).
- Four native tests: wait-for-available gating, `availableWaitTimeout`
  half-open boundary, `apActiveDuringWait` (with the pre-evidence state-3
  keep-waiting contrast), and the counter-collision regression.

## [4.5.0-beta03] - 2026-09-11

### Changed

- **AP injection gate is now the coordinator's arm-level precondition**
  (user-defined framework of 2026-09-11): gate closed → the 8.3.6防甩机制
  coordinator is completely silent (`phase=disabled`, `reason="apGateOff"`,
  zero `0x045` TX, no interception) and the Legacy **direct path is the legal
  normal mode** for non-8.3.6 vehicles (force activation works, no jerk).
  Gate open + switch on → coordinator remains the sole activation authority.
  Closing/reopening the gate is a **mode switch, not a fault**: reopening
  re-arms automatically via `configure()` (`WaitDriverIntent`), never
  `FailedLocked`, no need to re-toggle the 8.3.6 switch. `permit` is
  deliberately **not** gate-dependent (gate toggling must never trigger
  `permitLost`).
- **FSD 防护 page reordered** (zero id churn): FSD 防护总开关 (master, Ban
  Shield only — does not govern the coordinator) → AP 注入门控/延迟/自动恢复
  → 8.3.6防甩机制 + its diagnostics grid. Overview copy updated to the same
  hierarchy.

### Added

- `ApReRequestDiag.apGateOpen` + `"apGateOpen"` JSON key +
  `unavailableReason="apGateOff"` (profile errors keep priority); the 8.3.6
  switch stays toggleable with the gate closed (remembers the choice, status
  row shows 「不可用 · AP 门控未开启」, auto re-arms when the gate reopens).
- Triple independent defense lines inside the coordinator (`active()`,
  `activationAllowed()`/`inhibitDevice3ee()`, `ownsStalk()` /
  `nextStalkFrame()` guard / `startRound()` edge) each check `apGateOpen` —
  any single missed edit still fails closed.
- Six native tests (gate-closed silence, reopen re-arm without lock,
  mid-round clean reset, diag exposure, permit immunity, wiring-seam
  denial) and contract assertions pinning the three-arg `configure()` call
  site, the defense-page toggle order, and the sole-authority branch order
  in `dashLegacyFsdActivationAllowed()`.
- WebUI simulator: AP gate trio state (`apGateEnabled`/`apDelayMs`/
  `apAutoRestore`/`apInjectionState`), POST `/config` parsing for
  `apg`/`ap_delay_ms`/`ap_auto_restore`, and gate-aware `apReRequest`
  diagnostics.

## [4.5.0-beta02] - 2026-09-11

### Changed

- **UI rename**: the coordinator switch is now labeled **"8.3.6防甩机制"**
  (was "AP 取消重请求"). Same backend key (`ap_re_request`), same wiring —
  rename only; contract tests guard against the old label returning.

### Removed

- **#108 steer-jerk defense family deleted**: Instant Engage, Minimal Inject
  and Soft Engage (`dash_legacy_steer_defense.h`, `dash_minimal_inject.h`,
  `dashSoftEngageRelease`, UI cards, NVS keys, JSON objects, POST params,
  `native_legacy_steer_defense` test env). Vehicle evidence showed they never
  eliminated the 2026.8.3.6 activation jerk; the AP cancel + re-request
  coordinator is the replacement and the sole activation authority.
- **Abort Guard is retained on purpose**: it is not a jerk defense — it is the
  safety fence that latches after `0x399` AP state 8/9 (driver abort) and
  blocks every Legacy injection path (including the coordinator's) until a
  clean disengage (<2). Deleting it would not remove any jerk logic.

### Moved

- **AP gate controls consolidated onto the FSD 防护 page** (top of the status
  card): AP 注入门控 toggle, 延迟注入 (0/1000/2000/3000 ms) and AP 自动恢复
  now live next to the 8.3.6防甩机制 switch and save through one
  `saveApGateControls()` call. The hardware page keeps only the AP 状态恢复
  card; the status page points to the new location. Old `ap-gate-tgl` id and
  `saveApGate()`/`updateApGateControl()` helpers are gone (anti-regression
  assertions added).

### Fixed

- Qualification window frozen at **1200 ms** (was a 500 ms candidate; 1 Hz
  carrier evidence on the capture showed single-frame confirmation is not
  guaranteed inside 500 ms — user approved 1200 on 2026-09-11).
- WebUI simulator `/defense_config` aligned with the firmware response:
  `minimal_inject` removed, `ap_re_request` + `legacyInjectionSafety.apReRequest`
  added, so the renamed switch is testable offline.

## [4.5.0-beta01] - 2026-09-10

### Added

- **AP cancel + re-request coordinator** (`include/dash_ap_rerequest_activation.h`)
  for the Model X / 2026.8.3.6 activation steering-jerk hypothesis. When the
  driver activates AP with FSD intent, the device first replays a synthetic
  `0x045` stalk **cancel** (FWD) gesture, waits for DAS evidence that AP
  actually ended, then replays a **request** (RWD) gesture; within the
  qualification window after the request press (profile-driven; calibrated
  1200 ms — see the calibration entry below), the first native `0x3EE mux0`
  frame with the
  FSD intent bit while `DAS_autopilotState ∈ {2 AVAILABLE, 3 ACTIVE_NOMINAL}`
  (membership, no 2→3 edge) is modified to carry `bit46` **in that same
  handler event**. User-frozen design contract of 2026-09-10; the
  qualification-set membership, window semantics (half-open
  `[t_request, t_request+window)`), and FWD/RWD mapping are frozen; the
  window VALUE is profile-driven (frozen candidate was 500 ms).
- Three hard barriers around the handshake: (A) **all** device `0x3EE` writes
  (activation bit46, speed offset, mux1) are blocked while a round runs;
  (B) a cancel *request* is never treated as cancel *completion* — only fresh
  DAS evidence advances the round; (C) qualification requires the frozen AP
  set + fresh DAS + driver intent bit on a native mux0 frame.
- Safety rails: physical stalk input, counter conflicts, TX rejection, permit
  loss (CAN off / OTA / abort-guard latch), fault states and window expiry all
  lock the round `FailedLocked` until the mode is reconfigured; a completed
  round re-arms only after a fresh full AP exit.
- While the coordinator is armed it **replaces** (never chains after) the old
  settle/defense/APActive(3..6) activation chain in
  `dashLegacyFsdActivationAllowed()`, so the frozen AP=2 qualification path is
  reachable. Wheel-DND `0x045` ownership is arbitrated
  (`StalkOwnedByReRequest`) so both features never share the frame/counter.
- Exposed via the `ap_re_request` toggle (NVS `apr_on`, `/defense_config`,
  dashboard "AP 取消重请求" switch) and full diagnostics under
  `legacyInjectionSafety.apReRequest` (phase/step/reason/round/epoch/gesture
  counters/echo+conflict counters). **Default off.** The compiled profile
  now carries the real-vehicle calibration below; before calibration it was
  deliberately empty so the switch could be requested but never arm
  (fail-closed), and an invalid profile still fails closed the same way.
- **Calibrated the profile from real-vehicle 0x045 captures (2026-09-10,
  Model X, 2026.8.3.6; four recorder CSVs — two static gesture passes, two
  driving AP sessions; 3202/3202 CRC-valid frames, zero device 0x045 TX):**
  the stalk field is **byte0 bits 0-1** (idle 0x40 → 0x41 FWD / 0x42 RWD) —
  the wheel-DND byte3 guess is falsified (byte3 constant 0x30 throughout).
  The frozen value mapping is confirmed on-vehicle: all 14 FWD presses
  canceled (6→1, 161-526 ms) and all 17 RWD presses engaged (AP 2→3,
  155-236 ms). Timings filled from observation: `cancelEvidenceTimeoutMs`
  1500 (~3× observed max), `requestSeqTimeoutMs` 1000, `stalkReleaseGapMs`
  100 (presses held 1-3 frames). AP masks: cancel evidence {1,2}, fault
  {8,9} (observed 6→8→9 cascade), post-qualification continuation
  {2,3,4,5,6} (observed 2→3→6). **Documented deviation:** the native
  `0x3EE mux0` carrier is 1 Hz (gap median 1000 ms; 100% of observed gaps
  exceed the frozen 500 ms candidate, only 2.6% exceed 1200 ms), so
  `qualificationWindowMs` is compiled as **1200 ms instead of the frozen
  candidate 500 ms** — window semantics (half-open from `t_request`) stay
  frozen; the 500 ms candidate value is one profile edit away if rejected.
- Added the calibration tooling for that profile:
  `scripts/analyze_rerequest_045_calibration.py` derives the stalk
  byte/mask/FWD/RWD values, release-gap timing, and AP-state sets from a
  recorder CSV — assumption-free idle-baseline diffing plus 0x399
  event correlation (window truncated at the next press so a rise after
  the driver's next gesture is never misattributed; AP=2→3 request edges
  classify as rearward) that separately tests the frozen
  FWD=cancel/RWD=request mapping and the legacy wheel-DND byte-3
  enumeration guess — with the capture procedure in
  `docs/AP-REREQUEST-0X045-CALIBRATION-PROCEDURE.md` (existing recorder
  unchanged; default NAG filter already covers 0x045/0x399/0x3EE).
- 35 native tests (`native_ap_rerequest`): state-machine edges, 499/500 ms
  window boundary, uint32 wrap, echo/counter-conflict, plus wiring-level tests
  driving the real `LegacyHandler` + `MockDriver` through the dashboard seams
  (barrier A, same-event injection, replace-not-chain at AP=2, deny when
  inactive/out-of-set).

### 未验证（Unverified）

- The cancel/re-request hypothesis is **not validated on a real vehicle**. No
  flash, no bench, no road test has been run; "stable, no steering jerk" on
  2026.8.3.6 remains an open hypothesis. The 2026-09-10 captures validate the
  stalk ENCODING and the cancel/engage CAUSALITY of the physical stalk only —
  they say nothing about whether the coordinator's synthetic replay prevents
  the steering jerk. The stalk field position was never exported by any known
  DBC; it is now calibrated from those captures (byte0 bits 0-1) instead of
  guessed.

## [4.3.0-beta.27] - 2026-08-04

### Added

- Ported **Soft Engage** (upstream flipper-tesla-fsd v2.16-beta.10, #108
  sibling) to the Legacy `0x3EE mux0` activation gate. With the settle/edge
  verdict already allowing the frame, it holds `bit46` off until the steering
  wheel nears centre (`|angle| <= ±5.0°` and `steerValidity == 0`), or a 5 s
  long-curve timeout elapses so the driver is never stranded. Once released per
  engagement episode it stops gating. Implemented as the pure, native-tested
  `dashSoftEngageRelease()` helper (in `can_helpers.h`) wired into
  `dashLegacyFsdActivationAllowed()` after the steer-jerk defense verdict, using
  the existing `apRestoreState` steering parse. Disabled by default
  (zero-regression); the single-CAN firmware defaults it on, dual-CAN keeps it
  opt-in like the other experimental gates. Exposed via the `soft_engage`
  toggle (NVS `def_se`, `/defense_config`, dashboard switch) with 7 new
  `dashSoftEngageRelease` unit tests.

## [4.3.0-beta.26] - 2026-08-04

### Added

- Ported the `DashLegacySteerDefense` deep module from the single-CAN firmware,
  aligning Legacy `0x3EE mux0` activation with upstream flipper-tesla-fsd #108
  (v2.16-beta.16/.17/.19 parity; the core Abort Guard mechanism is unchanged
  through beta.24, whose later releases only touched adjacent HW detection and
  diagnostics). One module now owns engagement-edge detection, Instant Engage,
  the Minimal Inject burst, and the settle fallback on the Legacy mux0 path. It
  replaces the prior level-triggered `DashMinimalInject` quota on mux0; the base
  module is retained but no longer gates Legacy mux0. All controls default off,
  so continuous activation behavior is unchanged until explicitly armed.
- Added Legacy-only experimental **Instant Engage** (`ap_first_edge`), disabled
  by default: injects at the real AP engagement edge (state `2→3`), bypassing
  the settle wait so activation lands away from the later `6→8/9` abort edge.
- The **Minimal Inject** toggle now drives the steer defense edge burst: at
  each engagement edge it admits only five `0x3EE mux0` activation frames, then
  hard-stops until the next disengage→re-engage. Toggling it on mid-episode does
  not open the burst window (beta.17 edge semantics).
- Exposed `steerDefense` runtime diagnostics (engagement edges, Instant fires,
  Minimal burst budget/used/blocks) under `/defense_config` and a dashboard
  "Steer Defense" status tile. Ported the `test_native_legacy_steer_defense`
  unit suite (15 cases) covering edge detection, Instant one-shot, the 5-frame
  hard stop, mid-episode toggle, and uint32 wrap.

### Changed

- Abort Guard latch release threshold aligned to upstream `DAS_APSTATE_ENGAGED`
  (beta.15 parity): the latch now clears when AP drops below state `3`
  (disengaged `0/1` or available `2`), matching the engagement threshold used by
  AP-First and the steer defense. Previously it cleared only below state `2`.

## [4.3.0-beta.25] - 2026-07-30

### Added

- Added Legacy-only experimental `Abort Guard`, disabled by default. AP state
  `8/9` latches all Legacy writes (`0x3EE mux0/mux1`, `0x438`, `0x370`, and
  left-wheel DND) until a clean AP exit reports state `<2`.
- Added Legacy-only experimental `Minimal Inject`, disabled by default. Each
  `0x399` AP engagement permits exactly five successful `0x3EE mux0`
  activation writes, then blocks only further activation writes for that
  engagement. Source observation and speed-only output remain available.
- Added persisted `/defense_config` controls, dashboard toggles, and runtime
  diagnostics for latch state, AP state, blocked paths, quota, successful TX,
  and engagement count.

## [4.3.0-beta.24] - 2026-07-30

### Fixed

- Decoupled Legacy smart speed offset from the shared HW3/HW4 three-mode
  strategy. Legacy now persists and applies its native four modes: `off`,
  `manual`, `auto`, and `custom`.
- Added independent Legacy manual offset (`0..33 km/h`) and four custom bands
  (`0..63%`). The values drive only the verified `0x3EE mux0` offset path;
  `0x2F8` remains read-only limit telemetry and `Auto` still falls back to
  fresh `0x399` fused limit data.

## [4.3.0-beta.23] - 2026-07-29

### Fixed

- Restored the persisted Legacy AP settle delay. With AP gate enabled, Legacy
  now waits for `0x399` AP state `3..6` to remain continuous for the configured
  `0/1000/2000/3000 ms` before `0x3EE mux0` activation or speed writes proceed.
- AP exit, CAN disable, OTA guard, or gate disable clears the timer. `/status`
  now reports `settling`, the elapsed `apStableMs`, configured `requiredStableMs`,
  and `legacy_ap_state_delay_gate` while the delay is in effect.

## [4.3.0-beta.22] - 2026-07-29

### Fixed

- Fixed the Legacy AP injection gate. With the gate enabled, `0x3EE mux0`
  injection now waits for Legacy `0x399` AP state `3..6`; it no longer treats
  the generic `Parked` or `Summoning` state as a Legacy AP admission.
- Unified the Legacy activation callback, injection/runtime status, plugin
  context, and diagnostics around that same strict AP decision. `/status`
  reports `waiting_ap`, `legacy_ap_state_gate`, and `ApGate` while blocked.

### Compatibility

- With AP gate disabled, the established Legacy continuous activation path is
  unchanged. Enabling the experimental gate is fail-closed: it cannot bootstrap
  an initial AP state by sending bit46 before `0x399` reports AP active.

## [4.3.0-beta.21] - 2026-07-27

### Fixed

- Corrected the beta.20 Legacy Scroll DND volume-up side effect. Vehicle
  captures show that a warning normally needs two positive `0x045` actions:
  the first changes media volume and the second clears the warning.
- Added result-gated balancing. If the warning is still active when the next
  native idle arrives, beta.21 sends one inverse action to neutralize that
  unsuccessful positive attempt before retrying. If `0x399` clears first, the
  inverse is cancelled because the positive action was consumed by the prompt.
- Every inverse action follows a fresh native CAN1 `0x045` rolling counter and
  recalculates CRC-8/J1850. Driver input or any existing safety gate aborts it.

### Diagnostics

- Added per-episode positive count, pending compensation count, and completed
  volume-neutral episode count to the existing Scroll DND diagnostics.

### Preserved

- Legacy `0x3EE` activation, smart speed offset, `0x370`, recorder, network,
  NVS, and mobile navigation paths are unchanged.

## [4.3.0-beta.20] - 2026-07-27

### Fixed

- Removed the inverse `-1` left-wheel action after a successful Legacy NAG
  dismissal. Vehicle capture proved the `+1` action cleared `0x399` hands-on
  state before the inverse action, so the inverse was applied to audio as one
  volume-down step.
- Changed the `0x045` runtime to send one positive action, wait for a native
  idle/counter release and the warning result, then stop immediately when the
  warning clears. A still-active warning may retry after 800 ms, up to three
  positive attempts per episode.

### Preserved

- Retained the `legacy_scroll_dnd` NVS/API contract and deprecated diagnostic
  keys `negativeTx` and `balancedPairs`; they remain readable but no longer
  increase.
- Legacy `0x3EE` activation, smart speed offset, `0x370`, recorder, network,
  NVS, and mobile navigation paths are unchanged.

## [4.3.0-beta.19] - 2026-07-27

### Added

- Added a dedicated Legacy Model X left-scroll DND engine using CAN1 `0x045`
  and the vehicle-confirmed `0x399` hands-on warning trigger.
- Added native-frame-derived `+1 -> native idle -> -1 -> native idle` balancing,
  CRC-8/J1850 generation, rolling-counter following, expected-duplicate
  classification, Own-Echo rejection, and CAN2 `0x292` mirror diagnostics.
- Added runtime diagnostics for source freshness, generated counter/CRC,
  positive/negative TX, balanced pairs, retries, physical input, warning-clear
  latency, counter conflicts, TX failures, and runtime auto-disarm.

### Changed

- Legacy Scroll DND now transmits only CAN1 `0x045`; CAN2 `0x292` and `0x238`
  remain observation-only and the newer `0x3C2` implementation is isolated.
- Removed the Legacy Scroll DND dependency on the defense aggregate switch,
  slew-rate setting, D gear, and CAN2 online state. The persisted
  `legacy_scroll_dnd` switch is the independent feature control.
- Removed the obsolete Legacy speed-scroll `0x3C2` UI control while retaining
  its stored API field for backward compatibility.

### Safety

- Generated frames preserve all native fields except the left tick nibble,
  rolling counter, and CRC. Physical wheel input, OTA, AP exit, stale source,
  counter conflict, TWAI failure, or mode change aborts the active sequence.
- At most three balanced pairs are emitted in one uninterrupted warning
  episode; two consecutive TX failures auto-disarm runtime transmission.
- Existing Legacy activation, smart speed offset, and `0x370` paths are not
  modified by this release.

## [4.3.0-beta.18] - 2026-07-27

### Added

- Added receive-only Legacy Model S/X steering-wheel observation for
  `STW_ACTN_RQ`, prioritizing chassis/BFT ID `0x045` and also recording the
  gateway aliases `0x238` (powertrain) and `0x292` (body).
- Added per-bus `0x045` RX/TX/error counters and decoded observation counters
  for `StW_Lvr_Stat`, `StW_Cond_Psd`, switch activity, rolling counter, and CRC.
- Added a read-only CSV analyzer and an on-car capture procedure for mapping the
  2019 Model X volume wheel before any transmit implementation is considered.

### Safety

- This release does not transmit `0x045`, `0x238`, or `0x292`. Existing
  `0x3C2` Scroll DND remains fail-closed and is not remapped to a Legacy ID.

## [4.3.0-beta.17] - 2026-07-26

### Fixed

- Removed the circular Legacy activation dependency that required `APActive`
  and a settle delay before allowing `0x3EE mux0` bit46 injection. Bit46 is
  itself required to establish the AP/FSD state, which caused intermittent
  activation failures until stale vehicle state or a module restart changed
  the gate result.
- Aligned Legacy activation with `tesla-fsd-controller`: after the explicit
  force/UI trigger, only the CAN master, OTA safety guard, and existing plugin
  ownership check gate the built-in `0x3EE` send path.
- Kept the AP injection gate behavior for HW3/HW4 and auxiliary paths.

### Diagnostics

- Added `activationGateMode=explicit_trigger_can_ota` and
  `legacyIgnoresApGate=true`; Legacy now reports `armed` instead of incorrectly
  displaying `waiting_ap` before the first native `0x3EE mux0` source frame.

## [4.3.0-beta.16] - 2026-07-26

### Changed

- Replaced the directionless beta.7 random walk with a one-sided Mode-C model driven by receive-only `0x488 DAS_steeringControl`.
- Applies torque opposite the valid type-1 steering request, retains the last direction through the center deadband, and fails closed when `0x488` is missing, invalid, or older than 250 ms.
- Delays state-2 support for 2 seconds, delays states 3-5 escalation for 1 second, then uses bounded 0.8-1.8 Nm support or a 2.1 Nm ramp with 0.2 Nm/frame output slew.

### Preserved

- Keeps raw 2050 as zero, preserves native `0x370` byte 4, derives counter+1 and additive checksum from each native frame, and retains Own-Echo, counter-collision, and TX-failure protection.
- Does not inject `0x488`, write `0x399`, depend on unavailable `0x129`, or change Legacy FSD activation and smart speed-offset paths.

### Diagnostics

- Added `0x488` RX count, control type, decoded steering angle, source bus, age, selected episode direction, direction changes, and target torque to the Legacy NAG API.
- Vehicle effectiveness remains unverified pending beta.16 real-vehicle testing.

## [4.3.0-beta.15] - 2026-07-26

### Changed

- Replaced the Legacy `0x370` deterministic alternating waveform with the upstream `v2.16-beta.7` EPAS-faithful mean-reverting random walk.
- Uses raw 2050 as zero, a per-native-frame xorshift step of +/-40 raw units, 1/16 mean reversion, and the upstream asymmetric raw clamps 1780-2500 (-2.7 to +4.5 Nm).
- Removed steering-angle direction dependency; the random walk has no fixed polarity and does not require `0x129`.
- Stops output for DAS hands states 0 and 8 and skips native `handsOnLevel=1`, matching the upstream beta.7 admission behavior.

### Preserved

- Preserves the complete native `0x370` byte 4, including `handsOnLevel`.
- Continues to derive output from the native frame with counter+1, Tesla additive checksum, learned same-source bus, Own-Echo fingerprinting, collision degradation, and TX-failure auto-disarm.
- Existing Legacy FSD activation, smart speed offset, Scroll DND, UI, API, and NVS contracts are unchanged.

### Diagnostics

- Added `torqueModel=beta7_mean_reverting_random_walk`, `directionSource=none`, and `randomWalkSteps` to Legacy NAG diagnostics.

## [4.3.0-beta.14] - 2026-07-25

### Fixed

- Fixed `recorder queue allocation failed` on ESP32-S3 after Wi-Fi/WebUI startup.
- Allocated the 2,048-record queue payload from PSRAM first, with internal-heap fallback, and kept the FreeRTOS queue control block static.
- Added `queue_psram`, `queue_depth`, and heap diagnostics to `/rec_status` and allocation errors.

### Validation

- Recording remains queue-backed and non-blocking for CAN producers; the queue is initialized once and is not reallocated on every recording session.

## [4.3.0-beta.13] - 2026-07-25

### Fixed

- Aligned Legacy Continuous Absolute with upstream v2.16-beta.10 EPAS-faithful evidence by preserving native `0x370` byte 4, including `handsOnLevel`, instead of forcing level 1.
- Retained the vehicle-established 2050 raw zero point, existing +/-1.8 Nm limit, follower scheduling, checksum, and rolling-counter behavior.

### Diagnostics

- Added `0x129` steering angle and `0x5D9` hands-on-reason candidate traffic to the default 120-second recorder profile for direction and warning-cause correlation.

## [4.3.0-beta.12] - 2026-07-25

### Fixed

- Classified valid same-counter native `0x370` frames arriving 20-100 ms after TX as expected vehicle followers instead of Own-Echo or immediate collisions.
- Allowed expected native followers to drive the next rolling-counter overlay, restoring continuous Legacy torque output instead of degrading after 3-13 frames.
- Retained exact full-payload Own-Echo suppression and three-strike degradation for immediate same-counter conflicts or invalid-checksum followers.

## [4.3.0-beta.11] - 2026-07-25

### Added

- Admitted receive-only `0x3C2` evidence through the CAN1 TWAI filter in Legacy, HW3, and HW4 modes while retaining CAN2 MCP2515 receive-all monitoring.
- Added independent CAN1/CAN2 `0x3C2` RX, TX, and TX-error recorder counters to the API and dashboard.

## [4.3.0-beta.10] - 2026-07-25

### Fixed

- Decoupled the FSD defense master, Legacy 0x370 NAG, and HW3 slew limiter persisted controls.
- Replaced counter-only 0x370 own-echo detection with bus, DLC, counter, and full-payload fingerprints.
- Added explicit same-counter/different-payload collision diagnostics and automatic degradation after three collisions.
- Simplified the Legacy 0x370 runtime gates to CAN, Legacy handler, AP context, native frame, bus lock, and TX health.

## [4.3.0-beta.9] - 2026-07-25

- Aligned Legacy wheel DND with the reference firmware's `right_scroll` action model: `0x399` warning transitions now select the configured wheel action instead of starting speed DND on the UI switch edge.
- Added the native `0x3C2 mux1` reactive right-scroll sequence `01 -> 00 -> 3F -> 00` at 100 ms intervals; when both wheel actions are enabled, right-scroll is selected explicitly and reported in diagnostics.
- Added `dndActionType` to `wheelDndDiag` and kept the fail-closed native-frame, CAN2, OTA, cooldown, TX-failure, and auto-disarm gates. No synthetic `0x3C2` template or fixed-frame fallback was added.

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),

## [Unreleased]

## [4.3.0-beta.8] - 2026-07-25

### Fixed

- Process Legacy Scroll DND `0x399` context from its native source controller instead of excluding the MCP2515/PARTY channel. Vehicle captures 77-79 contain all `0x399` traffic on that logical channel, so the previous gate left diagnostics at `ap_inactive` despite AP state 6 and hands-warning states 3/4 being present.
- Added `0x399` source, age, raw AP state and hands state to `wheelDndDiag` and the dashboard.

### Safety

- Captures 77-79 contain no native `0x3C2` on either recorded logical bus across 360 seconds. Synthetic `0x3C2` fallback remains disabled; Scroll DND fails closed with `no_native_mux1` until the second channel exposes a verified native mux-1 frame.

## [4.3.0-beta.7] - 2026-07-25

### Fixed

- Restored source-driver-following for Legacy `0x3EE`: whichever controller receives the native frame handles and retransmits it through that same controller, matching `tesla-fsd-controller` and the last vehicle-working beta baseline.
- Removed the beta.6 static TWAI/CAN1 rejection that caused native `0x3EE` RX to increase while all Legacy activation TX remained zero on harnesses where physical CAN1 reaches the MCP2515 controller.
- Preserved secondary RX recorder single-write behavior through the handler `onFrame` callback.

### Reliability

- Retained bounded MCP2515 recovery after five consecutive TX failures or bus-off, including restoration of bitrate, Normal mode, filters or receive-all mode.
- Legacy payload, mux handling, activation bit 46, profile writes, AP gate and speed-offset calculations are unchanged.

## [4.3.0-beta.6] - 2026-07-24

### Added

- Added CAN2 native `0x3C2` capture diagnostics for total RX, mux-1, mux-1 idle, manual volume up/down, unexpected speed-wheel activity, injected TX, failed TX, and CAN1 sightings.
- Added the active recorder filter and a `wheel_3c2` evidence object to the recorder metadata export.
- Added live `0x3C2` evidence counters to the CAN recorder dashboard.
- Added CAN2 MCP2515 recovery diagnostics for recovery count, consecutive TX failures, and rejected mirrored `0x3EE` frames.

### Changed

- Expanded the default 120-second Legacy DND capture profile to `118,186,370,389,399,3C2,3EE,3FD,47D,47F,488` so native wheel traffic, warning context, camera-warning matrices, and steering-control context are retained together.
- Bound Legacy `0x3EE` to the internal TWAI channel. Vehicle testing showed this mapping was incorrect for the installed dual-CAN harness; this build is revoked and replaced by beta.7.
- Added bounded CAN2 MCP2515 recovery after five consecutive TX failures or bus-off, preserving the receive-all recorder mode after reset.

### Safety

- Recorder changes are observation-only. They do not arm Scroll DND, alter `0x3C2`, or change any CAN transmission gate.
- A CAN2 RX count proves frame visibility only. Vehicle acceptance still requires paired baseline/action captures and an observable vehicle response.

## [4.3.0-beta.5] - 2026-07-24

### Added

- Added an independent Legacy Scroll DND path driven by CAN1 `0x399` hands-warning transitions and a fresh native CAN2 `0x3C2` mux-1 idle frame.
- Added warning episode, hands state, native-frame age, action count, cooldown, TX failure, auto-disarm, and block-reason diagnostics to `wheelDndDiag`.
- Added `legacy_scroll_dnd` as an explicit `/defense_config` alias while preserving the existing `dnd_volume` API and `def_dv` NVS key.

### Changed

- Changed the reactive wheel sequence to `01 -> 00 -> 3F -> 00` at 100 ms intervals and limited it to three actions per warning episode with a two-second cooldown.
- Reactive Scroll DND now changes only `0x3C2 data[2]`; it preserves the native counter/checksum bytes and never starts the speed-wheel path.
- The dashboard switch now waits for a real `0x399` warning instead of transmitting immediately when enabled.

### Fixed

- Fixed the dual-CAN mobile More menu remaining open after navigation to FSD Defense.
- Removed misleading UI coupling between ISA sound suppression and wheel/camera behavior.

### Safety

- Legacy Scroll DND remains default-off and requires CAN write, defense master, effective Legacy mode, active AP state 3 through 6, CAN2 availability, no vehicle OTA, and a native idle `0x3C2` mux-1 frame no older than 1000 ms.
- Hands-warning states 9 and 10 are observation-only. Two consecutive transmission failures automatically disarm the feature until the user toggles it off and on.
- Scroll DND does not modify cabin-camera fields, `0x3FD`, or continuous `0x370` NAG behavior. Vehicle effectiveness remains pending controlled validation.

## [4.3.0-beta.4] - 2026-07-24

### Fixed

- Fixed Legacy NAG being permanently blocked when real `0x399` frames report AP status 6. Legacy now consistently treats statuses 3 through 6 as active context without changing HW3/HW4 AP semantics.
- Removed the Legacy NAG D-gear gate after captures 68/69 proved the current `0x118` gear bit definition is incompatible with the target vehicle and causes rapidly changing false gear values.

### Added

- Added a native regression using the captured status-6/no-gear scenario and requiring a same-bus, counter+1, checksummed `0x370` TX frame.
- Added `gearGateRequired=false` and Legacy AP context limits to the NAG runtime diagnostics.

### Safety

- Legacy NAG remains default-off and still requires CAN write, the defense master, Legacy mode, fresh `0x399` status 3..6 context, same-bus learning, a native `0x370` frame, accepted native hands level, and successful bounded transmission.
- The D-gear gate is removed only from Legacy NAG. Lighting, fog, shifting, HW3/HW4, and other vehicle-write safety gates are unchanged.

## [4.3.0-beta.3] - 2026-07-24

### Added

- Added a 120-second queue-backed CAN recorder that streams compact 19-byte records to SPIFFS from a low-priority writer task.
- Added 60/90/120-second selection, editable hexadecimal CAN ID filtering, and a default Legacy NAG diagnostic filter (`118,186,370,389,399,3EE`).
- Added recorder progress, written-frame, dropped-frame, file-size, storage-full, finalizing, and stop-reason diagnostics.
- Added streamed binary-to-CSV download conversion so stopping a recording no longer serializes a large in-memory buffer.

### Changed

- Removed the 2000-frame RAM capture ceiling and synchronous `/rec_stop` CSV save path.
- Recorder producers now use non-blocking queue writes and count dropped records instead of blocking CAN processing.

### Safety

- A 120-second capture is storage-guaranteed only with a suitably narrow filter. Unfiltered full dual-CAN traffic stops explicitly on storage exhaustion and reports `storage_full` rather than silently truncating.

## [4.3.0-beta.2] - 2026-07-24

### Added

- Added the `Legacy Continuous Absolute v4` engine using raw torque zero point 2050, a hard `+/-1.8 Nm` limit, and hands-on level 1.
- Added continuous same-source `0x370` responses on the learned native CAN bus, one bounded retry, and automatic runtime shutdown after three consecutive logical TX failures.
- Added v4 state, coverage, silence, retry, torque, target-hands, and degradation diagnostics to `/status` and `/defense_config`.

### Changed

- Replaced the Legacy Reactive v2 burst algorithm with Continuous Absolute v4 while preserving the beta1 `bionic_steering` API, `def_bio` NVS key, dashboard switch, and page initialization logic.
- Native `0x370` target hands levels 0 and 1 are accepted; levels 2 and 3 remain blocked.

### Safety

- Legacy NAG remains default-off and requires the existing CAN-write master, defense master, Legacy mode, fresh Drive/AP context, learned bus affinity, and a live native `0x370` frame.
- This implementation is host-tested and firmware-build verified but remains pending controlled vehicle validation.

## [4.3.0-beta.1] - 2026-07-23

### Added

- Added the Waveshare-verified Legacy smart speed-offset engine with fixed, automatic, and four-zone custom percentage strategies.
- Added 3-second `0x2F8` map-limit priority with fresh `0x399` fused-limit fallback and default 5 km/h/s downward smoothing.
- Added automatic CAN1/CAN2 `0x3EE mux0` route learning, mirrored-frame ambiguity blocking, manual bus override, and same-driver transmission.
- Added `/legacy_speed_config`, `legacySmartSpeed` runtime diagnostics, NVS persistence, and desktop/mobile controls for route and smoothing.

### Changed

- Changed Legacy `0x2F8` handling to read-only telemetry; calculated offset requests now modify the native `0x3EE mux0` frame.
- Mapped the existing `/speed_strategy` and `/speed_custom` contracts to Legacy without changing HW3/HW4 behavior.
- Replaced hard-coded Legacy CAN-A/TWAI diagnostics with the actual frame bus and corresponding dual-CAN driver labels.

### Safety

- Legacy smart speed fails closed on stale/unknown limits, route ambiguity, bus mismatch, disabled CAN write, OTA/AP gate rejection, plugin ownership, or an unchanged output frame.
- Automatic routing requires three consistent native `0x3EE mux0` samples; mirrored frames require explicit CAN1/CAN2 selection.

## [4.2.0-beta.1] - 2026-07-23

### Added

- Added user-verified Legacy Reactive NAG v2 as an optional Legacy subfeature, reusing the existing defense/NVS contract.
- Added proactive and reactive `0x370` handling from real source frames with bounded torque adjustment, counter/checksum regeneration, and own-echo rejection.
- Added automatic CAN1/CAN2 target-bus learning and same-driver transmission to prevent dual-bus duplicate injection.
- Added `/status` and `/defense_config` Legacy NAG diagnostics for mode, selected bus, AP/hands-on state, block reason, RX/TX counts, failures, and frame ages.

### Changed

- Replaced the active `0x370` prohibition with positive runtime contracts after user confirmation on the target vehicle.
- Renamed the dashboard control from experimental bionic steering to verified `Legacy NAG免打扰`, while preserving the `bionic_steering` API and `def_bio` NVS key.

### Safety

- Legacy NAG remains default-off and requires Legacy mode, CAN write, defense master, D gear, engaged AP state, fresh DAS context, a learned target bus, and a live native `0x370` frame.

## [4.1.1] - 2026-07-23

### Added

- Added `waveshare_single_can_standalone` profile for Waveshare ESP32-S3-RS485-CAN single-CAN deployments.
- Added Dashboard capabilities for single-CAN UI/API pruning: CAN2, Bus2, Service Mode, Stalk/highbeam, and lighting-stunt surfaces are hidden or unregistered in standalone builds (controlled via the `DASH_SINGLE_CAN_STANDALONE` build flag).
- Added AP Injection Gate safety parity with upstream `ev-open-can-tools v3.0.2-beta.3`: Legacy `0x3EE mux0` requires AP active for 2 seconds when the gate is enabled, and live unknown gear fails closed.
- Added official JSON plugin manager for Waveshare Single CAN standalone: URL install, `.json` upload, paste JSON, disabled-by-default installs, per-plugin enable/priority/remove/detail, GTW2047 replay count, and SPIFFS persistence (`/plugins_state.json`) restored on boot.
- Added AP Gate delayed-injection controls and AP injection state to the standalone driving status UI (desktop + mobile).

### Changed

- Refreshed the desktop and mobile dashboard with the cockpit design system while preserving the LILYGO dual-CAN controls and APIs.
- Restored persisted FSD runtime controls through `GET /config`, including Legacy offset and speed-limit override state.
- Extended Legacy vision-limit clearing, HW3 ISA chime suppression, and HW4 CAN 923 ISA speed-limit rewriting.
- Cleaned standalone desktop and mobile UI to remove CAN2/T-2CAN/Auto Shift product surfaces while preserving dual-CAN builds.

### Security

- Physically removed the experimental EPAS-faithful NAG injection engine and its CAN `0x370` write path. Existing passive/handler behavior is retained; the removed engine must not be reintroduced without independent safety review and bench validation.

## [4.0.2] - 2026-05-31

### Added
- LILYGO T-2CAN dual-bus firmware is now included in CI/release workflows as `firmware-lilygo-t2can-dual.bin`.
- Auto-shift placeholder page now displays read-only gear-assist telemetry from `/gear_assist_status`.

### Changed
- FSD/CAN injection safety now gates handler-level injection through the OTA guard and keeps LILYGO release builds injection-off on boot.
- HW3 speed-offset SNA/NONE handling now preserves incoming mux-2 payload and clears offset telemetry.
- Fog/strobe control now sends a final OFF frame from the CAN task when stopped or when gear is unsafe/stale.
- Wheel DND settings are persisted and re-arm once when the defense gate opens.

### Fixed
- Power-management partial updates preserve omitted settings and use configurable wake pin selection.
- Bionic steering status/re-enable path now reads and resets the active handler state.
- Dashboard drive cards preserve Auto/Sloth/MAX state instead of collapsing to legacy speed profiles.
- Chip temperature display uses the red error style above 60°C.

## [3.0.4-beta.1] - 2026-05-28

### Changed (Breaking)
- FSD detection bit corrected from 37 to 38 (matches verified tesla-fsd-controller)
- All compile-time feature flags (ISA_SPEED_CHIME_SUPPRESS, EMERGENCY_VEHICLE_DETECTION,
  ENHANCED_AUTOPILOT, BYPASS_TLSSC_REQUIREMENT, DASH_FSD_252_COMPAT) replaced with
  runtime switches accessible from Dashboard UI
- LegacyHandler CAN 760: replaced MPP bucket table with simple offset write
- HW3Handler mux 2: complete rewrite with auto target, custom buckets, high-speed mode

### Added
- Auto hardware detection mode (hwMode=3): detects HW3/HW4 from CAN 920
- Ban Shield: CAN 2047 snapshot protection (opt-in)
- TLSSC bypass option (bit 38) for HW3+HW4
- HW4 speed offset support (hw4OffsetRaw) on mux 2
- Legacy CAN 1080 visionSpeedSlider override
- Legacy CAN 1006 mux 1 removeVisionSpeedLimit option (bit 48)
- HW3 auto speed targeting: <60->64kph, =60->100kph, 60-79->85kph, >=80->passthrough
- 15 new CarManagerBase state fields for runtime FSD configuration

and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.0.0-beta.5] - 2026-05-04

### Added

- RGB status LED now reflects runtime state at a glance: solid green = injecting + at least one client connected, blinking green = injecting with no client connected, solid red = injection stopped + connected, blinking red = injection stopped + no client. Solid blue indicates an in-progress OTA firmware update. "Connected" is true when any station is associated with the AP or when the STA uplink is up.

## [3.0.0-beta.4] - 2026-05-04

### Added

- Manual firmware upload now has a dashboard button to clear saved OTA username/password credentials from browser local storage.

## [3.0.0-beta.3] - 2026-05-04

### Added

- Plugin rules can now include an additional byte-mask match (`match_byte`, `match_mask`, `match_val`) so plugins can target specific bit states without adding dedicated firmware toggles.
- Added an example `0x370` duplicate-counter plugin that matches byte 4 bits 7:6 clear, forces byte 3 to `0xB6`, sets byte 4 bit 6, increments byte 6 low-nibble counter, and recomputes byte 7 checksum.

### Fixed

- ESP-IDF WiFi Internet saves now defer reconnect until after the HTTP response, preventing the dashboard request from being dropped while AP+STA mode changes channels.
- Switching saved WiFi networks now disconnects any existing STA association before connecting to the new SSID, avoiding repeated `sta is connected` / deauth log spam.
- WiFi scan now prepares AP+STA mode before scanning, so scans work when the device is otherwise in AP-only mode.
- ESP-IDF WiFi/httpd/netif component logs are reduced to warning/error levels to keep dashboard serial logs readable.

## [3.0.0-beta.2] - 2026-05-04

### Changed

- Pin ESP-IDF builds to ESP-IDF v6.0.1 through PlatformIO `platformio/espressif32` 7.0.0 and keep legacy Arduino boards in `legacy-arduino`.
- Build all supported ESP-IDF and legacy Arduino targets in GitHub Actions, including release artifacts for AtomS3 Mini CAN Base and Waveshare ESP32-S3 RS485/CAN.

### Fixed

- Release and test workflows now run legacy Arduino board builds from `legacy-arduino`, so RP2040 and Feather M4 CI no longer look for removed root Arduino environments.
- CI installs the Python package needed by ESP-IDF 6.0.1 tooling and skips clang-format on the generated dashboard payload header.

## [3.0.0-beta.1] - 2026-05-03

### Added

- ESP-IDF 5.5 native build path replacing the Arduino framework on supported targets (M5Stack AtomS3 Lite verified). Adds `src/espidf_runtime.cpp` plus `include/platform/espidf_runtime.h` providing Arduino-compatible shims (`String`, `WiFi`, `WebServer`, `Preferences`, `Update`, `HTTPClient`, `SPIFFS`, etc.) on top of ESP-IDF APIs.
- ESP-IDF `led_strip` (RMT) implementation of the on-board RGB status LED so the AtomS3 Lite indicator works under IDF (green = injecting, red = idle).
- "Status LED Brightness" subsection in the Configuration card with a slider and number input (0–255), persisted in NVS (`led_b`) and applied live via `/led_brightness`.
- Multi-SSID WiFi support — up to 4 saved networks. The device tries each in turn until one connects (e.g. home + phone hotspot). New endpoints `/wifi_networks`, `/wifi_delete`; `/wifi_config` accepts an `idx` argument to update a specific slot. Legacy single-SSID NVS keys auto-migrate to slot 0 on first boot.
- `scripts/minify_dashboard.py` — minifies the dashboard HTML/CSS/JS (csscompressor + terser + htmlmin) and emits a gzipped `DASH_HTML_GZ[]` payload served with `Content-Encoding: gzip`. Dashboard source-of-truth moved to `include/web/mcp2515_dashboard_ui.src.h`.

### Changed

- Flash usage on `m5stack-atoms3-mini-can-base` reduced from 79.0 % (1,243,217 B) to 64.8 % (1,019,835 B) — about 223 KB saved with no feature loss. Drivers:
  - `CONFIG_COMPILER_OPTIMIZATION_SIZE=y` (was `OPTIMIZATION_DEBUG`),
  - `CONFIG_NEWLIB_NANO_FORMAT=y` plus integer formatting for the FPS field,
  - `CONFIG_ESP_ERR_TO_NAME_LOOKUP=n`,
  - `CONFIG_BOOTLOADER_LOG_LEVEL_ERROR=y`,
  - gzipped dashboard HTML (≈120 KB raw rodata → ≈30 KB compressed).
- `WebServer::send_P` no longer copies the body into a `String` (which OOM'd on the 130 KB dashboard HTML and aborted the httpd task). It now streams large responses in 4 KB chunks via `httpd_resp_send_chunk` directly from the source pointer (`sendRaw`).
- `WebServer::begin()` bumps the IDF httpd task stack to 16 KB and moves the per-request URL-query buffer to the heap, fixing a stack overflow on the first dashboard hit.

### Fixed

- `httpd` task stack overflow on the first client connection on ESP-IDF builds.
- `bad_alloc` / `terminate` reboot when serving the root dashboard page over the soft-AP on ESP-IDF builds.

## [2.6.0-beta.1] - 2026-04-30

### Added

- Added MCP2515 recovery regression coverage for the RP2040 CAN driver.
- Added `platformio_profile.example.h` as the committed build-config template.

### Changed

- `platformio_profile.h` is now treated as a local-only build config and ignored by git. Copy `platformio_profile.example.h` to `platformio_profile.h` before building, then keep board choices, credentials, and keys in the local file.
- Build documentation and helper-script errors now describe `platformio_profile.h` as the local build config and point fresh checkouts to the example file.

### Fixed

- RP2040 MCP2515 builds now recover the CAN controller after repeated TX failures or MCP2515 bus-off (`EFLG_TXBO`) instead of staying silent until power-cycled.
- CI test and release builds now create their local `platformio_profile.h` from `platformio_profile.example.h` before applying per-board build profiles.

## [2.5.2] - 2026-04-29

Stable release bundling the AP Injection Gate Smart Summon fixes from `2.5.2-beta.1` through `2.5.2-beta.6`.

### Fixed

- AP Injection Gate no longer drops during Smart Summon launches that report `DI_autonomyControlActive` while still in Park before an immediate turn. Definite Park frames now clear the summon latch only when ACA is inactive, so the earlier non-zero `UI_selfParkRequest` survives the shift out of Park even when no fresh request frame appears after the shift.

## [2.5.2-beta.6] - 2026-04-29

### Fixed

- AP Injection Gate no longer drops during Smart Summon launches that report `DI_autonomyControlActive` while still in Park before an immediate turn. Definite Park frames now clear the summon latch only when ACA is inactive, so the earlier non-zero `UI_selfParkRequest` survives the shift out of Park even when no fresh request frame appears after the shift.

## [2.5.2-beta.5] - 2026-04-28

### Fixed

- AP Injection Gate no longer stays open for 3-5 s after Autopilot is disengaged, and no longer treats plain TACC as an open-gate condition. `DI_autonomyControlActive` (ACA) on CAN 280 is set during AP, TACC, *and* Smart Summon, so it cannot drive Summoning by itself. Summoning now requires both `ACA=1` *and* a `UI_selfParkRequest` non-zero command observed in the current autonomy episode. ACA falling edge resets the spr-seen flag, so a subsequent TACC engagement does not re-latch the gate. The 5 s ACA-only timeout is removed: AP disengage drops ACA immediately and the gate closes immediately, restoring instant AP / TACC re-engagement.

## [2.5.2-beta.4] - 2026-04-28

### Fixed

- AP Injection Gate now opens on a freshly-booted module when the car is asleep / locked with Sentry. While the DI is asleep, CAN ID 280 (`DI_systemStatus`) is not broadcast at all (only DAS / autopilot ECU IDs 921/1016/1021/2047 keep transmitting), so the previous boot-time `Parked=false` default left the gate stuck at `Waiting AP` until the driver pressed the brake to wake the DI. `Parked` now defaults to `true` at boot; the first `DI_systemStatus` frame with a driving gear (R/N/D) flips it to false. If the DI never reports, the car is asleep / parked and the gate remains open by design.

## [2.5.2-beta.3] - 2026-04-28

### Fixed

- AP Injection Gate now stays open for the full duration of a Smart Summon / Smart Park session. Detection now uses `DI_autonomyControlActive` (CAN ID 280, bit 50 / byte 6 bit 2) as the primary "summon active" signal in addition to `UI_selfParkRequest` on CAN 1016. The DI bit is held high for the entire time the car is being driven by an autonomy stack, so the 5 s spr-only timeout no longer expires mid-summon when the UI command pulse drops to 0 while the car keeps driving itself.

## [2.5.2-beta.2] - 2026-04-28

### Fixed

- AP Injection Gate no longer drops while Summon shifts the vehicle to Reverse. `clearSummonOnPark()` now fires only on a definitive `DI_gear == 1 (P)` value, not on the permissive `isVehicleParked` set that also includes `0=INVALID` and `7=SNA`. SNA can blip during gear transitions (e.g. P->R under Summon control), and the previous logic would clear `Summoning` on that blip and close the gate mid-summon. Shift to Drive was unaffected because `selfParkRequest` stayed non-zero long enough to re-latch `Summoning`; Reverse was reaching gear faster than the latch could recover.

## [2.5.2-beta.1] - 2026-04-28

### Fixed

- AP Injection Gate now opens while the car is asleep / locked with Sentry. `isVehicleParked` now treats `DI_gear` values `0=INVALID` and `7=SNA` as parked in addition to `1=P`. When the DI is asleep it reports SNA on CAN ID 280, which previously left `Parked=false` and kept the gate stuck at `Waiting AP` until the driver pressed the brake to wake the DI. Driving states (R=2, N=3, D=4) are still not parked, so the gate behavior on a moving car is unchanged.

## [2.5.1] - 2026-04-28

Stable release bundling all changes from 2.4.2 onwards (`2.5.0-beta.5` through `2.5.0-beta.12`). Notably, the `Start after AP` dashboard toggle now gates plugin injection on AP, Park, *and* Summon / Smart Park, which makes the firmware compatible with vehicles running Tesla software release **2026.14.3** and newer — these versions reject the always-on injection used by earlier dashboards, so the toggle must be enabled to keep injection working on those vehicles.

### Added

- Dashboard speed profiles now include an `Auto` mode. Auto follows the vehicle follow-distance selection, while a manual dashboard profile stays locked and is injected instead of being overwritten by the car.

### Fixed

- Legacy speed profile selection is now written back into outgoing CAN ID `1006` mux `0` frames so the selected profile actually takes effect on vehicle behavior instead of only changing the observed internal state. (rolled up from 2.4.2-beta.1)
- AP Injection Gate now detects active AP from recorded HW3 `1021` mux `0` frames by reading the observed AD bit, so plugin injection no longer stays stuck at `Waiting AP` after Autopilot is engaged.
- AP Injection Gate now waits for DAS `AutopilotStatus` active states instead of the 1021 UI/config bit, preventing plugin injection from switching to Active when AP is not engaged.
- OTA update finalize now passes `true` to `Update.end()` to force completion regardless of residual byte count, fixing the `Update finalize failed` error that could occur after a successful download via GitHub S3 redirects.
- OTA error paths now log `Update.errorString()` at every failure point (begin, write, finalize) so the root cause is visible in the dashboard log instead of a generic message.
- AP Injection Gate now also opens while the vehicle is in Park, so Summon unlock injection can run while parked and stops again after shifting to Drive.
- Dashboard manual profile selection now persists in firmware state and is applied to Legacy, HW3, and HW4 injection paths.
- AP Injection Gate park detection now also reads `DI_systemStatus` (CAN ID 280) `DI_gear`, so the gate reopens when the vehicle is shifted to Park on Chassis-bus connections that do not carry `DIF_torque`/`DIR_torque` (CAN ID 390). MCP2515 hardware filter slots updated for Legacy, HW3, and HW4 modes to admit ID 280.
- AP Injection Gate now also stays open while Summon / Smart Park is active. HW3 and HW4 handlers parse `UI_driverAssistControl` (CAN ID 1016) `UI_selfParkRequest` (byte 3 bits 4-7); when the request is non-zero (4=PRIME, 5=PAUSE, 7/8=AUTO_SUMMON_FWD/REV, 11=SMART_SUMMON), `Summoning` is asserted and held for 5 s after the last activity, so injection keeps running once the vehicle shifts out of Park into Drive/Reverse under Summon control.
- AP Injection Gate `Summoning` flag is force-cleared whenever the vehicle returns to Park, so a manual P->D shift after a completed summon correctly waits for AP again instead of latching the gate open until reboot.

## [2.5.0-beta.12] - 2026-04-27

### Fixed

- AP Injection Gate Summon detection no longer latches `Summoning` permanently after the first summon use. `UI_summonHeartbeat` is no longer used for activity tracking because it keeps cycling 0..3 indefinitely once summon has run, which prevented the gate from ever closing again. Detection now relies solely on `UI_selfParkRequest` (CAN 1016, byte 3 bits 4-7); the flag holds for 5 s after the last non-zero command and is also force-cleared whenever the vehicle returns to Park, so a manual P->D shift after a completed summon correctly waits for AP again.

## [2.5.0-beta.11] - 2026-04-27

### Fixed

- AP Injection Gate now also stays open while Summon / Smart Park is active. HW3 and HW4 handlers parse `UI_driverAssistControl` (CAN ID 1016) `UI_summonHeartbeat` (byte 0 bits 2-3) and `UI_selfParkRequest` (byte 3 bits 4-7); when either is non-zero, `Summoning` is asserted and held for 1500 ms after the last activity, so injection keeps running once the vehicle shifts out of Park into Drive/Reverse under Summon control.

## [2.5.0-beta.10] - 2026-04-27

### Fixed

- AP Injection Gate park detection now also reads `DI_systemStatus` (CAN ID 280) `DI_gear`, so the "Start after AP" gate reopens when the vehicle is shifted to Park on Chassis-bus connections that do not carry `DIF_torque`/`DIR_torque` (CAN ID 390). MCP2515 hardware filter slots updated for Legacy, HW3, and HW4 modes to admit ID 280.

## [2.5.0-beta.9] - 2026-04-27

### Added

- Dashboard speed profiles now include an `Auto` mode. Auto follows the vehicle follow-distance selection, while a manual dashboard profile stays locked and is injected instead of being overwritten by the car.

### Fixed

- Dashboard manual profile selection now persists in firmware state and is applied to Legacy, HW3, and HW4 injection paths.

## [2.5.0-beta.8] - 2026-04-27

### Fixed

- AP Injection Gate now also opens while the vehicle is in Park, so Summon unlock injection can run while parked and stops again after shifting to Drive.

## [2.5.0-beta.7] - 2026-04-25

### Fixed

- OTA update finalize now passes `true` to `Update.end()` to force completion regardless of residual byte count, fixing the `Update finalize failed` error that could occur after a successful download via GitHub S3 redirects.
- OTA error paths now log `Update.errorString()` at every failure point (begin, write, finalize) so the root cause is visible in the dashboard log instead of a generic message.

## [2.5.0-beta.6] - 2026-04-24

### Fixed

- AP Injection Gate now waits for DAS `AutopilotStatus` active states instead of the 1021 UI/config bit, preventing plugin injection from switching to Active when AP is not engaged.

## [2.5.0-beta.5] - 2026-04-24

### Fixed

- AP Injection Gate now detects active AP from recorded HW3 `1021` mux `0` frames by reading the observed AD bit, so plugin injection no longer stays stuck at `Waiting AP` after Autopilot is engaged.

## [2.5.0] - 2026-04-24

First stable release of the 2.5 series. Bundles all changes from 2.5.0-beta.1 through 2.5.0-beta.4.

### Added

- Dashboard configuration now includes a GTW 2047 plugin replay control, and settings backup/import now preserves plugin replay preferences.
- Dashboard configuration now includes an AP Injection Gate toggle that arms plugin injection but waits until AP/NoA is observed active before sending plugin frames.
- Plugin rules now support `counter` fields and `emit_periodic` for cached GTW mux 3 broadcasts, including editor support and updated plugin documentation.
- Plugin rules now support a `bus` field (`CH`, `VEH`, `PARTY`, comma-separated string, bitmask, or array) to restrict matching to specific CAN bus pins; frames with unknown bus still match for backwards compatibility.
- Plugin rules now support a `mux_mask` field (alias `muxMask`) to control which bits of byte 0 are compared for mux matching; values 0-7 default to low-3-bit mask, values 8-255 default to full-byte mask, enabling low-nibble and full-byte DBC mux styles.
- Plugin list API now includes `bus` and `mux_mask` per rule so the dashboard UI and support exports reflect full rule configuration.
- Plugin editor gained bus and mux-mask input fields per rule, and the rule label, summary, and conflict panel now show bus pin and mux/mask.
- GTW periodic emit can now optionally try to silence native gateway broadcasts through a UDS diagnostic sequence using extended session, SecurityAccess, `CommunicationControl`, and `TesterPresent`.
- HW3 dashboard builds now expose an optional offset slew limiter for plugin-driven mux 2 offset changes.
- Added the shared `INJECTION_AFTER_AP` build option for behaviour-option builds; `ENHANCED_AUTOPILOT` mux 1 injection now waits for AP to be active when this option is enabled.
- Added a WiFi dashboard regression test that covers the WiFi settings UI, backend routes, status payload, and backup fields.
- Added native PlatformIO test environments `native_plugin_engine` and `native_plugin_engine_custom_key` for running plugin engine unit tests without hardware.

### Changed

- Plugin rules now allow up to 16 operations per rule instead of 8.
- `PLUGIN_FILTER_IDS_MAX` raised to 32 (was equal to `PLUGIN_RULES_MAX` = 16) and made overridable at build time.
- Replayed GTW frames, periodic emits, and repeated Rule Test sends now advance counter fields and refresh checksums between sends.
- Dashboard plugin details, validation, support exports, and docs now describe the new replay, counter, and periodic emit behavior.
- `gtw_silent: true` is now silently treated as disabled at parse time unless `PLUGIN_GTW_UDS_CUSTOM_KEY` is defined at build time; the periodic emit still works but no UDS sequence is started and `0x684`/`0x685` filter IDs are not injected.
- UDS request frames sent by the GTW silencing state machine are now tagged with the bus of the frame that seeded the periodic emit cache.
- Incoming frames with `CAN_BUS_ANY` are now normalized to `CAN_BUS_DEFAULT` in the main app loop before being passed to the plugin engine.
- Plugin rule mux matching and test-rule matching refactored into shared `pluginRuleMatchesBus` / `pluginRuleMatchesMux` helpers used by both the engine and the dashboard.
- Plugin editor mux input range extended to -1-255 (was -1-7) to support full-byte DBC mux values.

### Fixed

- GTW silent-mode plugin rules now add the required UDS request/response CAN IDs to the active filter set so the diagnostic state machine can observe replies.
- The CAN analyzer now labels UDS `0x28 CommunicationControl` requests by name.
- WiFi Internet status now follows the live STA connection state reliably after connect attempts and page refreshes instead of getting stuck on `Connecting to ...` or `Not configured`.
- Corrupted WiFi SSID fragments are now ignored in saved settings and filtered from `/wifi_status` responses.
- The WiFi settings form no longer overwrites the SSID field while it is being edited, and the status header no longer depends on optional labels being present.
- Dashboard CAN sniffer and recorder buffers now clamp incoming frame DLC before copying frame data.
- Plugin mux matching now ignores zero-DLC frames instead of treating them as mux 0.
- Plugin conflict detection and detail panel now correctly account for bus mask and mux mask when determining whether two rules can affect the same frame.
- Custom-key plugin engine native tests now validate output against the configured `PLUGIN_GTW_UDS_KEY_READY` value instead of a hard-coded key byte.

## [2.5.0-beta.4] - 2026-04-24

### Added

- Dashboard configuration now includes an AP Injection Gate toggle that arms plugin injection but waits until AP/NoA is observed active before sending plugin frames.
- Added the shared `INJECTION_AFTER_AP` build option for behaviour-option builds; `ENHANCED_AUTOPILOT` mux 1 injection now waits for AP to be active when this option is enabled.

### Fixed

- Custom-key plugin engine native tests now validate output against the configured `PLUGIN_GTW_UDS_KEY_READY` value instead of a hard-coded key byte.

## [2.5.0-beta.3] - 2026-04-24

### Added

- Plugin rules now support a `bus` field (`CH`, `VEH`, `PARTY`, comma-separated string, bitmask, or array) to restrict matching to specific CAN bus pins; frames with unknown bus still match for backwards compatibility.
- Plugin rules now support a `mux_mask` field (alias `muxMask`) to control which bits of byte 0 are compared for mux matching; values 0–7 default to low-3-bit mask, values 8–255 default to full-byte mask, enabling low-nibble and full-byte DBC mux styles.
- Plugin list API now includes `bus` and `mux_mask` per rule so the dashboard UI and support exports reflect full rule configuration.
- Plugin editor gained bus and mux-mask input fields per rule, and the rule label, summary, and conflict panel now show bus pin and mux/mask.
- `PLUGIN_FILTER_IDS_MAX` raised to 32 (was equal to `PLUGIN_RULES_MAX` = 16) and made overridable at build time.
- Added native PlatformIO test environments `native_plugin_engine` and `native_plugin_engine_custom_key` for running plugin engine unit tests without hardware.

### Changed

- `gtw_silent: true` is now silently treated as disabled at parse time unless `PLUGIN_GTW_UDS_CUSTOM_KEY` is defined at build time; the periodic emit still works but no UDS sequence is started and `0x684`/`0x685` filter IDs are not injected.
- UDS request frames sent by the GTW silencing state machine are now tagged with the bus of the frame that seeded the periodic emit cache.
- Incoming frames with `CAN_BUS_ANY` are now normalized to `CAN_BUS_DEFAULT` in the main app loop before being passed to the plugin engine.
- Plugin rule mux matching and test-rule matching refactored into shared `pluginRuleMatchesBus` / `pluginRuleMatchesMux` helpers used by both the engine and the dashboard.
- Plugin editor mux input range extended to −1–255 (was −1–7) to support full-byte DBC mux values.

### Fixed

- Plugin conflict detection and detail panel now correctly account for bus mask and mux mask when determining whether two rules can affect the same frame.

## [2.5.0-beta.2] - 2026-04-24

### Fixed
- Dashboard CAN sniffer and recorder buffers now clamp incoming frame DLC before copying frame data.
- Plugin mux matching now ignores zero-DLC frames instead of treating them as mux 0.

## [2.5.0-beta.1] - 2026-04-23

### Added
- Dashboard configuration now includes a GTW 2047 plugin replay control, and settings backup/import now preserves plugin replay preferences.
- Plugin rules now support `counter` fields and `emit_periodic` for cached GTW mux 3 broadcasts, including editor support and updated plugin documentation.
- GTW periodic emit can now optionally try to silence native gateway broadcasts through a UDS diagnostic sequence using extended session, SecurityAccess, `CommunicationControl`, and `TesterPresent`.
- HW3 dashboard builds now expose an optional offset slew limiter for plugin-driven mux 2 offset changes.
- Added a WiFi dashboard regression test that covers the WiFi settings UI, backend routes, status payload, and backup fields.

### Changed
- Plugin rules now allow up to 16 operations per rule instead of 8.
- Replayed GTW frames, periodic emits, and repeated Rule Test sends now advance counter fields and refresh checksums between sends.
- Dashboard plugin details, validation, support exports, and docs now describe the new replay, counter, and periodic emit behavior.

### Fixed
- GTW silent-mode plugin rules now add the required UDS request/response CAN IDs to the active filter set so the diagnostic state machine can observe replies.
- The CAN analyzer now labels UDS `0x28 CommunicationControl` requests by name.
- WiFi Internet status now follows the live STA connection state reliably after connect attempts and page refreshes instead of getting stuck on `Connecting to ...` or `Not configured`.
- Corrupted WiFi SSID fragments are now ignored in saved settings and filtered from `/wifi_status` responses.
- The WiFi settings form no longer overwrites the SSID field while it is being edited, and the status header no longer depends on optional labels being present.

## [2.4.2-beta.1] - 2026-04-23

### Fixed
- Legacy speed profile selection is now written back into outgoing CAN ID `1006` mux `0` frames so the selected profile actually takes effect on vehicle behavior instead of only changing the observed internal state.
- Added a native Legacy regression test that verifies the selected speed profile bits are injected into the transmitted mux `0` frame.

## [2.4.1] - 2026-04-22

### Added
- Dashboard header now shows the latest observed `GTW_autopilot` state next to the selected hardware mode.
- Native dashboard regression tests now assert that dashboard handlers do not send frames, increment `framesSent`, or fire `onSend`.
- README and dashboard footer now include the support and gift information from `main`.

### Changed
- Dashboard builds no longer compile automatic CAN injection paths from Legacy, HW3, or HW4 handlers; enabled plugins are the automatic injection path.
- HW3 no longer listens for or injects Track Mode request frames.
- ESP32 dashboard builds now use a 4MB OTA partition layout with larger app slots while preserving SPIFFS storage.
- Plugin documentation now describes the dashboard's plugin-only automatic injection behavior instead of firmware overlap warnings.
- Rule Test count and interval controls now use visible labels with `1` and `100` as placeholders, while empty fields still default to one injection every 100 ms.
- Support issue flow now copies the dashboard support report to the clipboard and opens the General Issue form without prefilled title or body text.

### Fixed
- ESP32 dashboard hotspot startup is more reliable by starting the AP earlier, disabling WiFi sleep, validating saved AP/STA credentials, and falling back to AP-only mode when STA connection attempts time out.

## [2.4.1-beta.3] - 2026-04-22

### Fixed
- ESP32 dashboard hotspot startup is more reliable by starting the AP earlier, disabling WiFi sleep, validating saved AP/STA credentials, and falling back to AP-only mode when STA connection attempts time out.

## [2.4.1-beta.2] - 2026-04-20

### Changed
- Rule Test count and interval controls now use visible labels with `1` and `100` as placeholders, while empty fields still default to one injection every 100 ms.
- Support issue flow now copies the dashboard support report to the clipboard and opens the General Issue form without prefilled title or body text.

## [2.4.1-beta.1] - 2026-04-20

### Added
- Dashboard header now shows the latest observed `GTW_autopilot` state next to the selected hardware mode.
- Native dashboard regression tests now assert that dashboard handlers do not send frames, increment `framesSent`, or fire `onSend`.

### Changed
- Dashboard builds no longer compile automatic CAN injection paths from Legacy, HW3, or HW4 handlers; enabled plugins are the automatic injection path.
- HW3 no longer listens for or injects Track Mode request frames.
- ESP32 dashboard builds now use a 4MB OTA partition layout with larger app slots while preserving SPIFFS storage.
- Plugin documentation now describes the dashboard's plugin-only automatic injection behavior instead of firmware overlap warnings.

## [2.4.0] - 2026-04-19

## [2.4.0-beta.1] - 2026-04-19

### Added
- Dashboard plugin priority controls now let users choose which enabled plugin wins overlapping bit writes
- Dashboard plugin conflict warnings now show firmware overlaps, plugin priority overlaps, and the first enabled injection priority

### Changed
- Plugin installs now start disabled so users can review priority and conflicts before enabling them
- Rule Test now waits for the next matching live CAN frame, applies the selected rule to that frame, and injects the captured result with the chosen count and interval
- Dashboard polling now keeps `/status` as the connection gate and backs off non-critical polls during reconnects

### Fixed
- Plugin rules targeting the same CAN ID and mux are merged into one injected frame per incoming frame, preventing contradictory duplicate plugin sends in the same cycle
- TWAI dashboard filtering now drops sparse-mask false positives in software, reducing dashboard load when plugin CAN IDs widen the hardware mask
- Dashboard status rendering no longer breaks when optional status-grid elements are removed or rearranged
- Dashboard remains responsive under heavier CAN/plugin load by limiting per-loop frame draining and giving the web task more scheduling priority

## [2.3.2-beta.2] - 2026-04-18

### Changed
- Dashboard no longer exposes or persists speed profile control; follow distance and the derived profile remain visible as read-only status
- Dashboard profile-related boot logging and legacy NVS cleanup now reflect the new plugin-managed model without stale `SP` or profile-lock state

### Fixed
- Dashboard core no longer injects speed profile or speed offset back onto CAN for Legacy, HW3, or HW4 handlers; those values are now observational unless a plugin explicitly modifies those frames
- Dashboard plugin toggles now apply immediately and batch their persistence, avoiding repeated Wi-Fi stalls while enabling or disabling multiple plugins

## [2.3.2-beta.1] - 2026-04-18

### Changed
- Dashboard builds now ignore all `BEHAVIOUR OPTIONS` from `platformio_profile.h`; these overrides are plugin-managed and are no longer compiled into firmware when `ESP32_DASHBOARD` is enabled

### Fixed
- Dashboard injection stop now also blocks plugin-based frame injection instead of letting enabled plugins keep sending
- TWAI dashboard profile syncing no longer forces commented-out behavior options into the build
- Dashboard boot/runtime state no longer reports legacy built-in ISA, emergency vehicle detection, TLSSC bypass, or nag handling as active in plugin-managed builds
- Plugin cards no longer show the obsolete built-in conflict warning badge and message

## [2.3.1] - 2026-04-18

### Fixed
- Dashboard hardware defaults now follow `platformio_profile.h` reliably for dashboard builds, even when older `DASH_DEFAULT_HW` values still exist in the selected PlatformIO environment
- Reflashing a dashboard build with a new default hardware mode now migrates stale stored hardware defaults from NVS without overwriting an explicit hardware choice made later in the web UI

## [2.3.0] - 2026-04-18

### Added
- Dedicated documentation pages for build and flash setup, dashboard usage, and a docs index for GitHub Pages
- Hardware-specific example plugins for HW3 and HW4 feature replacements, including AD activation, TLSSC bypass, nag suppression, Summon unlock, ISA chime suppression, emergency vehicle detection, and HW4 speed offsets

### Changed
- Dashboard Features card now only exposes Enable Logging; the other vehicle overrides are no longer shown there
- GitHub Actions are now split into separate workflows for tests, releases, and GitHub Pages deployment
- Dashboard and README documentation now reflect the plugin-based override flow and current Pages structure

## [2.2.0] - 2026-04-18

First stable release of the 2.2 series. Bundles all changes from 2.2.0-beta.1 through 2.2.0-beta.14.

### Added
- Plugin Editor UI with live JSON preview, duplicate-name detection, download/export, install support, loading of installed plugins, and a rule test tool for sending generated CAN frames
- CAN Sniffer support for switching between on-wire 11-bit IDs and prefixed DBC JSON IDs, with filtering for both formats
- Dashboard improvements including plugin capacity visibility, default CAN pin hints, a hidden SSID option for the WiFi hotspot, and an Atom S3 Mini injection toggle on the built-in button

### Changed
- Dashboard defaults now follow the selected build flags, including injection-on-boot behavior and vehicle-aware web UI defaults for TWAI and MCP2515 targets
- `esp32_feather_v2_mcp2515` now uses the new MCP2515 driver and web UI
- Reinstalling an existing plugin now preserves its enabled state and works cleanly when the plugin list is already full

### Fixed
- Plugin enable/disable and remove state is now persisted correctly across reboot, and install/remove/toggle actions refresh the dashboard immediately without falling back to a misleading connection error
- Dashboard polling, confirmation flows, and plugin detail panels now behave reliably across reconnects and Chrome on iOS
- WiFi STA/AP persistence, optional NVS reads, injection persistence, and runtime AD gating now behave consistently across reboots and firmware updates
- Atom and Atom S3 builds now use the correct RGB LED pins and a release-safe ESP32 LED API path across the supported CI toolchains

## [2.2.0-beta.14] - 2026-04-18

### Fixed
- Plugin install, remove and enable/disable actions in the dashboard now refresh the plugin list immediately instead of waiting for a manual page refresh
- Dashboard plugin installs no longer fall back to a misleading "Connection error" when the plugin was already applied and only the response body was interrupted
- Atom and Atom S3 dashboard builds now use an ESP32 RGB LED API path that stays compatible across the release CI toolchains

## [2.2.0-beta.13] - 2026-04-18

### Added
- Atom S3 Mini builds can now toggle injection with the built-in button on GPIO41, with the state saved so it persists across reboot

### Fixed
- Atom and Atom S3 dashboard builds now drive the built-in RGB status LED from the correct board pins so injection-off shows red and injection-on shows green reliably

## [2.2.0-beta.12] - 2026-04-17

### Fixed
- Plugin detail panels in the dashboard now stay open across the background plugin-list refresh instead of collapsing unexpectedly
- Dashboard confirmation prompts now use an in-page modal so reboot and other confirm actions work reliably in Chrome on iOS

## [2.2.0-beta.11] - 2026-04-17

### Added
- Plugin Editor can now load installed plugins for in-place editing
- Plugin Editor now includes a rule test tool that can send a generated CAN frame multiple times at a chosen interval

### Changed
- Reinstalling an existing plugin by name now preserves its enabled/disabled state and still works when the plugin list is already full

### Fixed
- Atom S3 and Atom Lite dashboard builds can now save CAN pin settings that use GPIO 6-11 instead of being blocked by the generic ESP32 flash-pin restriction

## [2.2.0-beta.10] - 2026-04-17

### Fixed
- Plugin enabled/disabled state is now persisted across reboot instead of defaulting back to enabled on startup
- Removing a plugin now also clears its persisted enabled/disabled state
- Dashboard background polling now stops cleanly after repeated connection failures and points users to the STA IP after WiFi handoff instead of continuously spamming timeout errors

## [2.2.0-beta.9] - 2026-04-17

### Added
- CAN Sniffer now has a toggle to switch between on-wire 11-bit CAN IDs and DBC JSON IDs with the current bus prefix
- The sniffer filter now accepts both on-wire IDs and prefixed DBC JSON IDs

### Changed
- Migrated `esp32_feather_v2_mcp2515` to the new MCP2515 driver and web UI
- Dashboard feature and injection defaults now follow the selected build flags, and `DASH_INJECTION_ON_BOOT` can be used to start injecting automatically after boot
- Dashboard grid inputs now size correctly in narrower layouts, and `platformio_profile.h` no longer ships with `DRIVER_TWAI` and `HW3` preselected by default

### Fixed
- "Stop Injecting" now persists across reboot instead of silently re-enabling injection on startup
- Runtime AD gating is now applied consistently across Legacy, HW3, and HW4 handlers so blocked mux paths no longer keep injecting

## [2.2.0-beta.8] - 2026-04-16

### Added
- Plugins card now shows the maximum plugin capacity, current usage, and a clearer message when the plugin limit has been reached
- `/plugins` now returns `maxPlugins`, and plugin install errors include the configured maximum

### Fixed
- `DRIVER_TWAI` dashboard builds now treat the vehicle selection in `platformio_profile.h` as the default web UI hardware only; if none is selected, `HW3` is used by default
- WiFi STA credentials are now loaded correctly from NVS on boot after being saved through the dashboard
- Optional AP and WiFi preference reads no longer spam `Preferences` `NOT_FOUND` errors when keys have not been stored yet
- Added the option to show default can pins in webdashboard

## [2.2.0-beta.7] - 2026-04-15

### Added
- Hidden SSID option in the WiFi Hotspot card. When enabled, the access point does not broadcast its SSID — clients have to enter the name manually. Setting is persisted in NVS and included in the settings backup/restore JSON (`ap.hidden`)
- `/ap_config` endpoint accepts a new `hidden` parameter; `/ap_status` returns the current `hidden` flag

## [2.2.0-beta.6] - 2026-04-15

### Changed
- Dashboard layout: the two separate Firmware Update cards (GitHub OTA and manual .bin upload) have been merged into a single card. Manual .bin upload is now a collapsible section under the primary update controls
- WiFi Internet moved into its own top-level card (previously nested inside the Plugins card) — it is used for both firmware updates and plugin downloads, so it deserves its own slot

## [2.2.0-beta.5] - 2026-04-15

### Fixed
- Firmware Update check: no longer offers older versions as "updates". A proper semantic-version comparison is now used (major.minor.patch plus alpha/beta/rc pre-release ranking), so a device on `2.2.0-beta.4` will not be prompted to "update" to `2.0.0` or `2.1.0`
- Auto-Update on Boot uses the same comparison (older releases are skipped)
- CI release job: firmware binaries are now reliably attached to GitHub releases. The workflow first creates the release as a draft, uploads all assets, and then publishes it, which works around the repository's "immutable releases" setting that previously blocked asset uploads after publish

## [2.2.0-beta.4] - 2026-04-15

### Changed
- Default dashboard credentials (`changeme`) are now allowed at build time. Users are expected to change the WiFi AP password and OTA credentials at runtime via the dashboard WiFi Hotspot card (persisted in NVS, OTA-safe)
- Build no longer fails when `DASH_PASS` / `DASH_OTA_PASS` are left at the default `changeme` placeholder

### Removed
- Nag Killer toggle removed from the dashboard Features card. The underlying `NAG_KILLER` build flag remains available for advanced users who want to compile it in

## [2.2.0-beta.3] - 2026-04-15

### Fixed
- Firmware Update check: "JSON parse error" when Beta Channel is enabled (reduced GitHub API response size by using `per_page=1` instead of `per_page=5`, avoids ArduinoJson heap overflow on ESP32)
- CI: firmware artifacts are now correctly attached to GitHub releases. Previous releases failed to attach binaries due to an "immutable release" error when the release was published before the upload step ran

### Changed
- CI release notes: workflow now extracts the matching section from `CHANGELOG.md` for each tag instead of using the whole file, and auto-detects prerelease based on the tag name (`beta`/`alpha`/`rc`)

## [2.2.0-beta.2] - 2026-04-15

### Added
- Auto-Update on Boot: optional toggle in the Firmware Update card. When enabled, the device checks GitHub for a newer release ~15 seconds after the WiFi Internet connection comes up and installs it automatically
- Respects the Beta Channel toggle (only installs prereleases when that is on)
- Setting persisted in NVS so it survives firmware updates
- New endpoints: `/auto_update` (GET/POST)

## [2.2.0-beta.1] - 2026-04-15

### Added
- Plugin Editor card: create plugins via a form UI without writing JSON manually
- Support for all plugin ops: set_bit, set_byte, or_byte, and_byte, checksum
- Per-rule configuration of CAN ID, optional mux value, and send flag
- Live JSON preview updating as you edit, with collapsible rule sections
- Client-side validation (ID, mux, bit 0-63, byte 0-7, value 0-255, hex input `0xFF` supported)
- One-click Install via existing `/plugin_upload` endpoint (no backend changes)
- Download generated plugin as a standalone `.json` file for sharing or backup
- Duplicate-name detection against existing installed plugins

## [2.1.0] - 2026-04-15

First stable release of the 2.1 series. Bundles all changes from 2.1.0-beta.1 through 2.1.0-beta.5.

### Added
- **Rebrand & UX:** renamed UI from "ADUnlock" to "ev-open-can-tools"; dynamic footer with firmware version and device IP; GitHub and Discord links in the footer
- **Plugins:** info icon next to "Plugins" with inline explanation and link to plugin documentation with examples
- **CAN Pins:** runtime-configurable CAN TX/RX GPIO pins via the dashboard, persisted in NVS so custom pin configurations survive OTA firmware updates; validation (GPIO 0-39, TX != RX, GPIO 6-11 blocked for SPI flash)
- **WiFi Internet:** network scanner with RSSI and channel info; static IP configuration (IP, gateway, subnet, DNS); dedicated status and scan endpoints
- **WiFi Hotspot:** change AP name and password via the dashboard; credentials stored in NVS and survive firmware updates
- **OTA firmware updates from GitHub releases:** check for updates and install directly from the dashboard, with beta channel toggle
- **Status badges** ("saved" / "firmware default") on WiFi Hotspot and WiFi Internet cards, plus an info icon explaining NVS persistence
- **Settings Backup / Restore:** export all persistent settings (AP, WiFi Internet, CAN pins, beta flag) as JSON and restore in one go — disaster recovery for full-erase or cross-device migration

### Note
- WiFi credentials have been OTA-safe since the original 2.1 series; the badges and backup feature make that explicit and add recovery paths for full-flash-erase scenarios.

## [2.1.0-beta.5] - 2026-04-15

### Added
- Visible "saved" / "firmware default" status badge on the WiFi Hotspot and WiFi Internet cards
- Info icon on WiFi Hotspot card explaining that credentials are stored in NVS and survive firmware updates
- Settings Backup card: export all persistent settings (AP, WiFi Internet, CAN pins, beta flag) as JSON for safekeeping or migration to another device
- Settings Restore: upload a previously exported JSON to restore all persistent settings in one go
- New endpoints: /settings_export (GET), /settings_import (POST)

### Note
- WiFi credentials were already OTA-safe in prior versions; these changes make the persistence explicit in the UI and add disaster-recovery via backup file

## [2.1.0-beta.4] - 2026-04-15

### Added
- Runtime-configurable CAN TX/RX pins: configure GPIO pins for the TWAI transceiver directly from the dashboard
- Pin configuration is persisted in NVS so it survives OTA firmware updates
- New "CAN Pins" dashboard card with validation (GPIO 0-39, TX != RX, GPIO 6-11 blocked for SPI flash) and reboot flow
- New endpoints: /can_pins (GET/POST)

### Fixed
- OTA updates no longer risk breaking CAN communication on boards with custom pin configurations — once pins are configured via the dashboard they persist across updates

## [2.1.0-beta.3] - 2026-04-15

### Added
- Plugin info icon: click the (i) next to "Plugins" to see an inline explanation of what plugins are and how they work, with a link to the documentation and examples
- Footer community links: GitHub repository and Discord invite are now linked in the dashboard footer

## [2.1.0-beta.2] - 2026-04-15

### Changed
- Rebrand: renamed all UI references from "ADUnlock" to "ev-open-can-tools"
- Footer now shows current firmware version and dynamically adapts to the device IP
- Removed hardcoded hardware references from footer

### Added
- WiFi network scanner: scan and display available networks in the dashboard, select by clicking
- Signal strength indicators (RSSI) and channel info for each scanned network
- Static IP configuration: optionally set IP, gateway, subnet mask and DNS server
- OTA firmware update from GitHub releases: check for updates and install directly from the dashboard
- Beta channel toggle: switch between stable and pre-release firmware versions
- Firmware version auto-injected from VERSION file at build time
- Dedicated WiFi status endpoint (/wifi_status) and scan endpoint (/wifi_scan)
- WiFi hotspot configuration: change AP name and password via the dashboard
- New update endpoints: /update_check, /update_install, /update_beta
- New AP endpoints: /ap_config, /ap_status

## [2.0.0] - 2026-04-14

### Added
- Plugin system: install CAN frame modification rules as JSON files via web dashboard
- Plugin Manager UI card with install from URL, file upload, enable/disable and remove
- Paste JSON (offline): install plugins by pasting JSON directly into the dashboard — no internet or file picker needed
- Plugin detail view: expandable rule inspector showing CAN IDs, mux values and all operations per plugin
- Conflict detection: warns with a visual indicator when plugin CAN IDs overlap with base firmware handlers
- WiFi STA mode (AP+STA) for internet access to download plugins
- Plugin engine with mux-aware matching and operations: set_bit, set_byte, or_byte, and_byte, checksum
- Automatic CAN filter merging for plugin-required IDs
- ArduinoJson v7 dependency for all ESP32 dashboard environments
- New API endpoints: /plugins, /plugin_upload, /plugin_install, /plugin_toggle, /plugin_remove, /wifi_config
- Plugin documentation with format reference, examples and CAN ID table (docs/plugins.md)

### Fixed
- Credential placeholder check in build script now matches platformio_profile.h defaults
- CI release job: firmware artifacts renamed to unique filenames to prevent upload conflicts

## [1.0.0] - 2026-04-10

First release
