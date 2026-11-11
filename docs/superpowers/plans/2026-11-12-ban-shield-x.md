# Ban Shield X (0x368 forge + tripwire) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the approved Ban Shield X design: a passive 0x368 template tripwire plus a strictly-budgeted 0x368 tier-bit forge experiment on CanC, with a "防封禁 Ban Shield" toggle on the FSD 防护 page.

**Architecture:** One new header-only module (`include/dash_ban_forge.h`, `DRIVER_P4_3CAN`-gated, `dash_autoshift.h` house style) holds the pure core: template check, five bit transforms, arm budget/cooldown state machine, tripwire with echo mute. Runtime glue in `src/main.cpp` (send helper + `wantTx` OR) and `include/web/mcp2515_dashboard.h` (sniff hook, NVS/defense_config, /status diag). UI adds one toggle + one arm selector to `pg-defense`.

**Tech Stack:** ESP32 Arduino/IDF (PlatformIO, envs `p4_3can` + `lilygo_t2can_dual`), clang++ native contract tests driven by pytest, ESP-IDF NVS Preferences, vanilla-JS dashboard UI.

**Spec:** `/Users/ziwind/my-vibe-project/Atlas-3CAN-P4/docs/ban-shield-0x368-forge-experiment-design-20261111.md` (v3 — the plan argues from it; executors read both)

## Global Constraints

- Default OFF, fail-closed: NVS `banForge` default false; runtime armed flag separate from preference; every arm auto-disarms on completion.
- Arm budget **hard-coded**: 3 injection attempts per arm; wallclock cap 600 ms measured from first injected frame; inter-arm cooldown ≥30 s. Not configurable.
- Every TX must traverse `mcpDashOnTxFrame` (T/E observability) — the tertiary driver already copies `dashDriver->onSendFrame` (`src/main.cpp:346` `t2canAsBindTertiary`).
- `wantTx` extension is OR-only: `(dashAsPermit.master && dashAsPermit.armed) || dashBanForgeActive()`. Never touch the backoff/LISTEN_ONLY-fallback path.
- Frame ID in code is decimal: 0x368 = 872. Template (hex) `51 31 59 0F 88 49 67 00`; DLC must be compared (memcmp-padding trap).
- Non-P4 builds byte-identical: all new code behind `#if defined(DRIVER_P4_3CAN)` (T2CAN red line).
- NVS keys ≤15 chars: `banForge` (8), `banForgeArm` (11).
- No `pio run -t upload` ever; builds via `IDF_MAINTAINER=1 ~/.platformio/penv/bin/pio run -e <env>` (wrapper watchdog trap); ui.h discipline: source commit first → `p4_3can` build last → commit regenerated `mcp2515_dashboard_ui.h` separately (dual-env stamp trap).
- Gate order serial, never parallel: native matrix → ESP builds → pytest.
- Commits English-only. Never commit `.claude/scratch/` or OTA credentials.

## Review Focus

1. **Echo self-pollution of the tripwire** — our injected frame returns on CanC with transformed bytes; a naive tripwire logs it as a template deviation. Expected: no tripwire hit while armed and until 2 s after the last injection. → Task 2 native test `tripwire_mutes_echo_window`.
2. **Arm budget stretched by trigger wait** — at ~10 Hz with 600 ms gaps, "3 frames" can span ~1 s wallclock. Expected: budget counted by injection attempts, wallclock cap measured from FIRST injected frame, not from arm time. → Task 2 `arm_wallclock_measured_from_first_injection`.
3. **DLC mismatch passes template check** — an 872 frame with dlc≠8 (or short data) must never be transformed. → Task 2 `template_requires_exact_dlc`.
4. **Forge armed while autoshift unarmed silently fails** — bus mode stays LISTEN_ONLY and every send fails. Expected: `wantTx` OR makes forge alone flip the mode; disarm restores LISTEN_ONLY via mode-dirty. → Task 3 glue + Task 5 bench A4.
5. **T2CAN build drift** — new dashboard-header code shifts non-P4 binary. Expected: `lilygo_t2can_dual` build byte-identical on untouched code paths; macro-gate totality pinned. → Task 5 guard test + build diff.

---

### Task 1: Fix v2 TX bus tagging (pre-existing bug)

**Files:**
- Modify: `include/web/mcp2515_dashboard.h:1367`
- Test: `test/test_ban_forge_contract.py` (token pin, created here; Task 2 extends it)

**Interfaces:**
- Produces: correct v2 TX rows for CanC (bus=3), prerequisite for trustworthy Stage-B evidence.

- [ ] **Step 1: Write the failing pin** — create `test/test_ban_forge_contract.py` with `test_v2_tx_submit_maps_pt_bus` asserting the source contains `frame.bus == CAN_BUS_PT ? kRecV2BusC` inside the `mcpDashOnTxFrame` v2 submit (it currently doesn't).
- [ ] **Step 2: Run** `python3 -m pytest test/test_ban_forge_contract.py -q` → FAIL.
- [ ] **Step 3: Fix** the submit at :1367 to the 3-way form matching the v1 export (line 5120): `frame.bus == CAN_BUS_PT ? kRecV2BusC : ((frame.bus & CAN_BUS_PARTY) ? kRecV2BusB : kRecV2BusA)`.
- [ ] **Step 4: Re-run** → PASS.
- [ ] **Step 5: Commit** `fix(recorder): tag CanC TX rows as bus C in the v2 blackbox submit`.

### Task 2: Pure core — `dash_ban_forge.h` + native tests

**Files:**
- Create: `include/dash_ban_forge.h`
- Create: `test/test_native_ban_forge_main.cpp`
- Modify: `test/test_ban_forge_contract.py` (add clang++ compile-and-run, mirror `test_autoshift_contract.py` mechanics: `subprocess` clang++ `-DDRIVER_P4_3CAN -std=c++17`, run binary, expect exit 0)

**Interfaces:**
- Produces (all `static inline`, header-only, inside `#if defined(DRIVER_P4_3CAN)`; an `#else` empty translation unit keeps non-P4 builds identical):
  - `struct DashBanForgeDiag { uint32_t tripwireHits; uint32_t injected; uint32_t armsRun; uint8_t lastDeviant[8]; uint32_t lastDeviantMs; uint8_t lastArm; bool armed; };`
  - `void dashBanForgeSetPref(bool on); bool dashBanForgePref(void);`
  - `bool dashBanForgeRequestArm(uint8_t arm);` — arm 0=disarm, 1-5=B1-B5, 6=additive-checksum fallback, 7=whole-b6 probe; rejects while cooldown active or pref off.
  - `bool dashBanForgeActive(void);` — true while an arm is armed (drives `wantTx` OR).
  - `bool dashBanForgeOnFrame(uint32_t id, uint8_t dlc, const uint8_t data[8], uint32_t nowMs, uint8_t out[8]);` — RX hook: returns true + fills `out` when this received frame should be answered with an injection (template match + arm live + budget remains). Also runs the tripwire check (deviant 872 payload → diag counter + `lastDeviant` snapshot + mute during echo window).
  - `void dashBanForgeOnInjectResult(bool ok, uint32_t nowMs);` — counts attempts; first attempt starts the 600 ms wallclock window; budget/cap exhaustion auto-disarms (armed=false) and starts the 30 s cooldown.
  - `const DashBanForgeDiag &dashBanForgeDiag(void);`
  - Constants: template `{0x51,0x31,0x59,0x0F,0x88,0x49,0x67,0x00}`, `kBanForgeId=872`, `kBanForgeMaxAttempts=3`, `kBanForgeWindowMs=600`, `kBanForgeCooldownMs=30000`, `kBanForgeEchoMuteMs=2000`.
  - Arm transforms (template-relative, value 3→2 unless stated): B1 `b6 bits7:5`, B2 `b6 bits3:1`, B3 `b1 bits6:4`, B4 `b2 bits5:3`, B5 `b3 bits4:2`, B6 = B1 + `b7 = (sum(b0..b6)) & 0xFF`, B7 = whole `b6 = 0x00`. Only the target bits change; all other bytes copied from the received frame. (Spec §6-A2's XOR/two's-complement checksum forms are deliberately collapsed into B6's additive form — field evidence says b7 is a reserved zero byte, and the 0x238 country_override precedent uses additive; if B6 shows checksum-like response, a two's-complement variant is a one-line follow-up arm.)

- [ ] **Step 1: Write `test/test_native_ban_forge_main.cpp`** with `main()` returning nonzero on first failure, asserting: template accepts id 872/dlc 8 exact bytes; rejects wrong id, wrong dlc, deviant payload (`template_requires_exact_dlc`, `template_rejects_deviant`); each of B1-B7 transform expected bytes on the field template (e.g. B1 output `51 31 59 0F 88 49 47 00`); arm flow: requestArm(1) → three template frames each yield one injection → 4th yields none, armed=false, cooldown blocks immediate re-arm (`arm_budget_three_attempts_then_disarm`); wallclock: simulate 3 attempts with first at t0, frame arriving at t0+700 ms yields none even though attempts<3 (`arm_wallclock_measured_from_first_injection`); tripwire: deviant 872 at rest → hit+snapshot; deviant at armed or ≤2 s after last inject → no hit (`tripwire_mutes_echo_window`); pref off → requestArm rejected, OnFrame never injects (`pref_off_fail_closed`).
- [ ] **Step 2: Add to contract test** the clang++ compile-and-run (mirror `test_autoshift_contract.py::test_native_main_compiles_and_passes_with_p4_define`, header `include/dash_ban_forge.h`, binary exit 0) + `test_header_is_gated_by_p4_guard`.
- [ ] **Step 3: Run** `python3 -m pytest test/test_ban_forge_contract.py -q` → FAIL (no header).
- [ ] **Step 4: Implement `include/dash_ban_forge.h`** minimal to pass. State machine pattern after `dash_legacy_wheel_045_dnd.h` (attempt-based budget, cooldown, auto-disarm). No heap, no Serial in the core; plain functions + file-scope statics like `dash_autoshift.h`.
- [ ] **Step 5: Re-run** → PASS.
- [ ] **Step 6: Commit** `feat(banforge): pure core with template check, seven arm transforms, and echo-muted tripwire`.

### Task 3: Runtime glue — CanC send path, bus mode, NVS, defense_config, /status

**Files:**
- Modify: `src/main.cpp` (send helper ~:406 pattern; `wantTx` at :358; sniff call site via `mcpDashSniffCan3`; boot NVS load ~:1163 area)
- Modify: `include/web/mcp2515_dashboard.h` (`mcpDashSniffCan3` :1138 P4 block; defense_config parse ~:8098 & apply ~:7655/4346; `/status` diag ~:2710 pattern; dashLog events)

**Interfaces:**
- Consumes: Task 2 core API verbatim.
- Produces: `static bool t2canBanForgeSendCan3(const uint8_t d[8])` (id 872, dlc 8, bus `CAN_BUS_PT`, tertiary driver send — mirrors `t2canAutoshiftSendCan3` at `src/main.cpp:406`); `wantTx = (dashAsPermit.master && dashAsPermit.armed) || dashBanForgeActive();`; diag JSON `"banForgeDiag":{"pref":..,"armed":..,"arm":..,"injected":..,"armsRun":..,"tripwireHits":..,"lastDeviantMs":..}`.

- [ ] **Step 1: Wire the sniff hook** — in the `DRIVER_P4_3CAN` block of `mcpDashSniffCan3`, after the autoshift sensors: `uint8_t out[8]; if (dashBanForgeOnFrame(tagged.id, tagged.dlc, tagged.data, millis(), out)) { if (t2canBanForgeSendCan3(out)) dashBanForgeOnInjectResult(true, millis()); else dashBanForgeOnInjectResult(false, millis()); }` (send failure still consumes budget — attempt semantics). Declare the helper in main.cpp above the include or via a small fwd-decl header as house style allows; keep call order stable.
- [ ] **Step 2: Bus mode + disarm path** — extend `wantTx` (OR only). On every defense_config save with `banForge` false or arm 0, call `dashBanForgeRequestArm(0)` then `t2canAutoshiftModeDirty()` to restore LISTEN_ONLY when autoshift is also idle.
- [ ] **Step 3: NVS + defense_config** — POST parse: `defense["banForge"]`→`p.putBool("banForge",..)`, `defense["banForgeArm"]`→`p.putUChar("banForgeArm", 0..7)` (clamp, then `dashBanForgeSetPref`/`dashBanForgeRequestArm` at apply time, mirroring the `def_en` flow at :4421). Boot: load both prefs; `dashBanForgeSetPref(pref)`; **armed stays false at boot** (fail-closed; re-arm explicitly). GET `/defense_config` returns current values.
- [ ] **Step 4: /status diag + dashLog** — append `banForgeDiag` JSON after `wheelDndDiag` (:2710 pattern); dashLog `[BANFORGE]` lines on arm start, arm complete (attempts used), tripwire hit (with hex payload), and pref change.
- [ ] **Step 5: Build check** `IDF_MAINTAINER=1 ~/.platformio/penv/bin/pio run -e p4_3can` → SUCCESS (no upload).
- [ ] **Step 6: Commit** `feat(banforge): canc send path, or-gated bus mode, nvs config, and status diagnostics`.

### Task 4: UI — 防封禁 Ban Shield 开关 on the FSD 防护 page

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h` (settings card in `pg-defense`, toggle rows ~:1272-1385 pattern; `saveDefenseConfig` JS :3105; refresh/status JS; EN map ~:2029-2120)
- Regenerate: `include/web/mcp2515_dashboard_ui.h` per build discipline (Task 5 runs the `p4_3can` build that stamps it)

**Interfaces:**
- Consumes: Task 3 `/defense_config` keys `defense.banForge` (bool), `defense.banForgeArm` (int 0-7) and `/status` `banForgeDiag`.
- Produces: checkbox `def-ban-shield-tgl` + select `def-ban-shield-arm` (options 0 关闭 / 1-5 臂 B1-B5 / 6 校验和兜底 / 7 整字节探针), status chips `st-banforge-*` (开关状态 / 当前臂 / 已注入 / tripwire 命中).

- [ ] **Step 1: Markup** — one setting row in the FSD 防护 card after `def-master-tgl`: name `防封禁 Ban Shield`, desc `0x368 档位遮罩实验：默认关闭；开启后选择实验臂，每次臂注入≤3帧后自动停止`（描述里写明这是实验功能）+ the toggle; second compact row for the arm select. Follow the existing `.setting-row` structure exactly; static nodes only (data-i18n on static markup, none on dynamic text — i18n trap).
- [ ] **Step 2: JS** — extend `saveDefenseConfig()` to include both fields; extend the defense status refresh to populate the chips from `/status` `banForgeDiag`.
- [ ] **Step 3: i18n** — add the new zh strings to the EN map (~:2120 pattern).
- [ ] **Step 4: pytest UI contract** — extend the UI-contract test (house `test_ui_contract`) expectations for the new ids if it asserts page inventories; run `python3 -m pytest test/test_ui_contract.py test/test_ui_browser.py -q` (browser harness if available).
- [ ] **Step 5: Commit (source only)** `feat(ui): ban shield toggle and arm selector on the fsd defense page`.

### Task 5: Gates — native matrix, dual build, ui.h stamp, pytest, version

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.h` (regenerated), `platformio.ini` (only if version env), `CHANGELOG.md`, version header per house bump discipline (VERSION → `6.0.0-beta15`).

- [ ] **Step 1: Native matrix** — full run per house runner; all green including new `test_ban_forge_contract.py`.
- [ ] **Step 2: Dual build serial** — `p4_3can` then `lilygo_t2can_dual` via `IDF_MAINTAINER=1` direct pio; T2CAN image must not change behavior (macro-gate; if the repo pins build hashes for t2can, verify unchanged or document why changed — ui.h is shared, stamp-only).
- [ ] **Step 3: ui.h stamp discipline** — after source commits are in, run `p4_3can` build LAST, commit regenerated `mcp2515_dashboard_ui.h` separately with the env-stamped build id in the message.
- [ ] **Step 4: Full pytest** — `python3 -m pytest -q` all green.
- [ ] **Step 5: Version + CHANGELOG** — bump to `6.0.0-beta15`, English CHANGELOG entry (Ban Shield X: tripwire, forge experiment gated default-off, UI toggle, v2 TX bus-tag fix).
- [ ] **Step 6: Commit** `chore(release): 6.0.0-beta15 ban shield x`. Ask Jordan before any push or OTA.

### Task 6: Bench Stage A + vehicle Stage B runbooks (documentation)

**Files:**
- Create: `docs/ban-shield-stage-ab-runbook-20261112.md`

- [ ] **Step 1: Stage A runbook** — bench top: simulated 0x368 source (~10 Hz, template), toggle on, arm 1: verify T-row id 872 bus C in v1 recorder + E-row zero, `banForgeDiag.injected==3`, auto-disarm, LISTEN_ONLY restored (autoshift idle), tripwire silent during echo window. A4 kill: toggle off mid-arm stops injections immediately.
- [ ] **Step 2: Stage B runbook** — vehicle protocol per spec §7: pin alignMode/legacy activation/0x3EE keepalive OFF (NVS-reversible), clean baseline capture, arms B1→B5 sequentially with ≥30 s observation, v1 session recorder three-bus, `recbb_stream` stays disarmed, screen recording, abort conditions.
- [ ] **Step 3: Commit** `docs: ban shield stage a/b runbooks`.
