# Auto-Shift (Legacy Intel) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship auto gear shift (scroll + steering triggers, gated, SBW/0x228 injection on CanC) for Jordan's 2019 Model X on the Atlas-3CAN-P4 board, preceded by a zero-firmware recon trip that fills the unknown protocol constants.

**Architecture:** Recon first — an offline analysis toolkit digests a 15-minute field recording into a frozen protocol profile. The firmware then lands as ONE platform-free C++ core (`include/dash_autoshift.h`: profile + J1850 CRC + SBW composer + gate state machine + triggers) compiled into the P4 build behind `#if defined(DRIVER_P4_3CAN)`, wired to the CAN drains, DIAG console, NVS permit keys, and the dashboard TX queue in later tasks. T2CAN_DUAL stays byte-identical throughout.

**Tech Stack:** ESP-IDF (Arduino framework) on PlatformIO env `p4_3can`; pure-stdlib Python 3 analysis tooling; pytest contract tests (source-pinning, house style) plus a native C++ harness compiled by the test itself.

**Spec:** `docs/superpowers/specs/2026-11-04-autoshift-recon-and-v1-design.md` (amended 2026-11-05 — repo = T2CAN worktree, lightweight NVS permit, blocker-only rear posture, staged first injection).

## Global Constraints

- Build recipe: `IDF_MAINTAINER=1 ~/.platformio/penv/bin/pio run -e p4_3can`; pytest → pio build strictly serial, never parallel; after any pio build, restore/re-stamp `include/web/mcp2515_dashboard_ui.h` (two-stage stamp discipline).
- Every line of autoshift firmware sits inside `#if defined(DRIVER_P4_3CAN)` guards; `lilygo_t2can_dual` and all native builds must compile byte-identical to before.
- The engine core (profile, CRC, composer, gate machine, trigger logic) is platform-free C++ — no Arduino/ESP includes — so the native harness compiles it directly; platform wiring lives behind the guard in later tasks.
- OTA-only flashing (never `pio run -t upload`); any board action (flash, injection experiment) requires Jordan's explicit go-ahead first.
- English commit messages; never commit `.claude/scratch/` or `.superpowers/`; no `git push` without Jordan's explicit per-push approval.
- Recorder CSV columns: `ts_us,bus,dir,id,dlc,b0..b7`; `id` is DECIMAL (0x228 = 552, 0x118 = 280, 0x00E = 14).
- Analysis scripts run `python3 -I` with standard library only.

## Review Focus

1. **Stale SBW source** — injecting a modified copy of an old 0x228 after the real stalk moved. Expected: composer/gate refuses when source age exceeds the frozen budget. Pinned in Task 3 (composer refuses stale input) and Task 4 (gate BLOCK_STALE_SOURCE).
2. **Counter desync with the live SCCM stream** — MsgTxmtId must be source+1, never a locally rolling counter. Pinned in Task 3 (compose takes counter from the source frame).
3. **DI_gear SNA/INVALID false-confirm** — codes 0 (INVALID) and 7 (SNA) must never complete a request; only exact 1/2/4 confirm. Pinned in Task 4 (close-loop test with gear=0 and gear=7).
4. **Obstacle telemetry absent read as "clear"** — no fresh ooc/sdi within the window must BLOCK, not pass. Pinned in Task 4 (BLOCK_OBSTACLE on stale/absent telemetry).
5. **Scroll gesture burst = multiple requests** — one physical roll emits several 0x045 motion frames; exactly one request per gesture episode. Pinned in Task 5 (episode edge fires once).

---

### Task 1: Recon report toolkit (offline analysis)

**Files:**
- Create: `tools/autoshift_recon_report.py`
- Test: `test/test_autoshift_recon_report.py`

**Interfaces:**
- Consumes: recorder v2 session CSV(s) at arbitrary paths (CLI args).
- Produces: `analyze(paths: list[str], window_labels: list[tuple[int,int,str]]) -> dict` (also CLI entry printing markdown + JSON); report keys `sbw_states`, `id118_bytes`, `angle00e_bytes`, `obstacle_rates`, `profile_draft`.

- [ ] **Step 1: Write the failing tests** — fixture writer builds an in-memory CSV (real column layout, decimal ids): SBW section with two stalk states (`40 C0` ×16 counter values CRC-valid via J1850, plus state `41 C1` ×4) and one CRC-corrupt row; 0x118 rows with two byte patterns; 0x00E sweep rows; 0x20E/0x23E rows on bus 3. Assert: `sbw_states` clusters by (b0,b1) with counts, per-state CRC pass/fail tallies, counter-continuity fraction; `profile_draft` contains a `gear_codes` mapping skeleton keyed by observed state bytes; `obstacle_rates` reports per-bus Hz.

- [ ] **Step 2: Run to verify failure**

Run: `python3 -m pytest test/test_autoshift_recon_report.py -v`
Expected: FAIL (module not found).

- [ ] **Step 3: Implement `tools/autoshift_recon_report.py`** — stdlib only (`csv`, `argparse`, `json`, `collections`). J1850 CRC8 helper duplicated here (poly 0x1D, init 0xFF, xorout 0xFF, MSB-first). SBW clustering keyed on (b0,b1) of bus-3 id-552 DLC-4 rows; 0x118 per-byte distinct-value/timeline summary; 0x00E per-byte min/max/monotonic-run summary; obstacle Hz per (id,bus). `--window ts_ms:ts_ms:label` (repeatable) annotates which window each 0x118/0x00E byte change falls in (that is how the stalk-sweep timeline disambiguates DI_gear vs DI_gearRequest).

- [ ] **Step 4: Run tests; verify pass**

Run: `python3 -m pytest test/test_autoshift_recon_report.py -v`
Expected: PASS.

- [ ] **Step 5: Commit** — `git add tools/autoshift_recon_report.py test/test_autoshift_recon_report.py && git commit -m "feat(p4): offline recon report toolkit for autoshift protocol discovery"`

### Task 2: Field recon session [human gate — no code]

**Files:** none (procedure; session files land in `~/Downloads` + MacServer).

**Interfaces:**
- Consumes: board 2 running 06d26b14 as-is; Task 1 toolkit.
- Produces: session CSVs + `python3 -I tools/autoshift_recon_report.py sess_*.csv --window ...` report → the frozen constants for Task 3.

- [ ] **Step 1: Prep** — car awake in P; DIAG `bus_enable 5`; reboot; confirm CanC traffic on the bus page; confirm recorder armed. TX master switch stays as-is (armed) — the trip doubles as TCB-trap bait; a trap trip (crash) OVERRIDES the session: stop, let the dump land (0x15E0000), recover it before continuing.
- [ ] **Step 2: Run the 8 segments** (spec Phase 1 list, each = one `rv2_lock` + export; phone video as backup): ① idle 30 s ② brake-hold stalk P→R→N→D→P slow ×3 + rapid ③ P-button presses ④ brake press/release ×10 ⑤ nose/tail-to-wall ⑥ scroll rolls both wheels ⑦ steering sweeps brake-held (full lock both ways, holds ~30°/60°/90°) ⑧ seatbelt buckle/unbuckle ×3.
- [ ] **Step 3: Freeze the profile** — run the report with windows annotated from the video; fill: gear codes per stalk state (P/R/N/D + P-button), 0x118 brake/speed/gearRequest field positions (stalk-sweep ordering: request field moves first, gear follows), 0x00E angle scale/offset/sign, obstacle freshness budget (frame period ×4), SBW source-freshness budget (max observed inter-frame gap ×4). Exit criteria: every constant named; crash-or-not at busmask 5 documented ≥15 min.

### Task 3: Engine core — profile freeze, CRC8, SBW composer

**Files:**
- Create: `include/dash_autoshift.h`
- Test: `test/test_autoshift_contract.py`, `test/test_native_autoshift_main.cpp` (compiled by the contract test via `clang++ -std=c++17 -DDRIVER_P4_3CAN -I include -x c++` — **the define is mandatory**: the header lives inside the P4 guard, so a native compile without it preprocesses to nothing and every assertion passes vacuously)

**Interfaces:**
- Consumes: Task 2 frozen constants.
- Produces (used by Tasks 4–6): `uint8_t dashAsCrc8J1850(const uint8_t *d, size_t n)`; `bool dashAsSbwCompose(const uint8_t src[4], uint32_t srcAgeMs, uint8_t gearCode, uint8_t out[4])` (false when `srcAgeMs > kAsSbwSrcFreshMs`); `struct DashAutoshiftProfile { uint8_t gearCode[5]; uint32_t sbwSrcFreshMs; uint8_t speedLimitKph; ... }` with `kAsProfile` constant instance.

- [ ] **Step 1: Write the failing tests** — contract test pins: header gate is `#if defined(DRIVER_P4_3CAN)`; the 16 static-verified CRC vectors literal in the native main (`40 C0 00 B2`, `10 7F`, `20 35`, `30 F8`, `40 A1`, `50 6C`, `60 26`, `70 EB`, `80 94`, `90 59`, `A0 13`, `B0 DE`, `C0 87`, `D0 4A`, `E0 00`, `F0 CD`); compose takes the counter from `src[2]` high nibble +1 and recomputes byte 3; compose returns false past the freshness budget; profile gear codes equal the Task-2 frozen values (test fails with "PROFILE UNFROZEN" sentinel until Task 2 lands).
- [ ] **Step 2: Run to verify failure** — `python3 -m pytest test/test_autoshift_contract.py -v` → FAIL (file missing).
- [ ] **Step 3: Implement `include/dash_autoshift.h`** — platform-free core; profile constants = `{TASK2_FROZEN}`; CRC8 MSB-first poly 0x1D init 0xFF xorout 0xFF; compose copies src, writes the frozen stalk-state byte pair for `gearCode`, counter nibble from src, CRC recompute.
- [ ] **Step 4: Verify** — pytest PASS (including the native-compile-and-run assertions).
- [ ] **Step 5: Commit** — `feat(p4): autoshift engine core - frozen profile, J1850 crc8, sbw composer`.

### Task 4: Gate chain state machine

**Files:**
- Modify: `include/dash_autoshift.h`
- Test: extend `test/test_autoshift_contract.py`, native main matrix cases.

**Interfaces:**
- Consumes: Task 3 composer/profile.
- Produces: `enum DashAsVerdict { AS_SUBMIT, AS_BLOCK_MASTER, AS_BLOCK_STALE_118, AS_BLOCK_SPEED, AS_BLOCK_BRAKE, AS_BLOCK_OBSTACLE, AS_BLOCK_COOLDOWN, AS_BLOCK_COUNT, AS_BLOCK_PENDING, AS_ALREADY_TARGET }`; `DashAsVerdict dashAsGateEval(uint32_t nowMs, const DashAsInputs &in)` with `struct DashAsInputs { bool master; bool torque2Fresh; uint8_t speedKph; bool brakeApplied; uint8_t gearNow, gearTarget; bool obstacleFreshAndClear; uint32_t lastShiftMs, nowCount; }`; close-loop helpers `dashAsConfirmGear(uint8_t gearNow)` returning COMPLETE only for gearNow == pending target ∈ {1,2,4}.

- [ ] **Step 1: Failing tests** — native matrix: master=false → AS_BLOCK_MASTER (checked before anything else); stale 0x118 → AS_BLOCK_STALE_118; speed 9 kph → AS_BLOCK_SPEED (8 passes); brake released → AS_BLOCK_BRAKE; obstacle stale OR blocked → AS_BLOCK_OBSTACLE (absent telemetry is NOT clear); within cooldown → AS_BLOCK_COOLDOWN; pending request outstanding → AS_BLOCK_PENDING; gear==target → AS_ALREADY_TARGET; happy path → AS_SUBMIT. Close loop: gear 0/7 never confirm; gear==target confirms; deadline expiry increments the retry count, at `kAsRetryMax{2}` the request is abandoned with a block reason. Contract test pins evaluation order (master→stale→speed→brake→cooldown→count→obstacle→pending→target→submit) by source ordering.
- [ ] **Step 2: Verify failure** → pytest FAIL.
- [ ] **Step 3: Implement** — pure state machine, fail-closed on every freshness input; single pending slot `{target, deadlineMs}`; constants `kAsSpeedLimitKph{8}` default, `kAsCooldownMs{1000}`, `kAsConfirmTimeoutMs` from Task 2 budget.
- [ ] **Step 4: Verify pass; commit** — `feat(p4): autoshift gate chain - fail-closed, single in-flight, exact-code confirm`.

### Task 5: Decoders, triggers, close-loop wiring

**Files:**
- Modify: `include/dash_autoshift.h` (pure decode/trigger helpers), `src/main.cpp` (drain-path feed inside the P4 guard).

**Interfaces:**
- Consumes: gate machine (Task 4); wheel episode semantics from `include/dash_legacy_wheel_045_dnd.h` (episode edge = one gesture); Task 2 field maps.
- Produces: `void dashAsOnTorque2(const uint8_t d[8], uint32_t nowMs)`, `void dashAsOnAngle00E(const uint8_t d[8], uint32_t nowMs)`, `void dashAsOnObstacle(uint32_t id, const uint8_t d[8], uint32_t nowMs)` (ooc/sdi extraction), `DashAsTrigger dashAsPollTrigger(uint32_t nowMs)` returning `{NONE, SCROLL(side,dir), STEER(sign)}`; steering thresholds `kAsSteerParkDeg{30}`, `kAsSteerDriveReverseDeg{60}`.

- [ ] **Step 1: Failing tests** — native: a scroll episode of 3 motion frames yields exactly ONE SCROLL trigger (burst=one request); steering crossing 30° with sign → STEER; below threshold → NONE; 0x118 decoder extracts brake/speed/gear using frozen field map; obstacle decoder sets/clears freshness. Contract: main.cpp feed calls sit inside the P4 guard and call the decoders for ids 280/14/0x20E/0x23E on the right buses.
- [ ] **Step 2: Verify failure** → pytest FAIL.
- [ ] **Step 3: Implement** — decoders per frozen map; triggers edge/episode-based; P/R/N/D mapping table for scroll actions (defaults: left+ = D, left− = R, right+ = P, right− = D/R toggle).
- [ ] **Step 4: Verify pass; commit** — `feat(p4): autoshift decoders and one-shot triggers wired into can drains`.

### Task 6: Permit gate, TX submit, DIAG, runtime status

**Files:**
- Modify: `include/dash_autoshift.h`, `include/web/mcp2515_dashboard.h` (DIAG + status JSON), `src/main.cpp` (NVS keys, arm flow).

**Interfaces:**
- Consumes: composer (Task 3), gate + close loop (Tasks 4–5), dashboard TX queue (the same path the 0x292 gateway mirror uses, `kGatewayMirrorId` precedent in dash_legacy_wheel_045_dnd.h).
- Produces: NVS namespace "t2can" keys `autoshift_master` (u8, default 0), `autoshift_armed` (u8, default 0); DIAG commands `autoshift_status`, `autoshift_arm`, `autoshift_disarm`, `autoshift_master 0|1`, `autoshift_test` (compose-and-print, NEVER transmits); `DashAsRuntime` JSON {state, verdict, lastBlock, pending, counts, sourceAgeMs}.

- [ ] **Step 1: Failing tests** — contract: TX submit call is reachable ONLY inside `master && armed && verdict == AS_SUBMIT` (source-pin the guard wraps the submit); arm is two-step house pattern (first `autoshift_arm` opens a confirm window, a second `autoshift_arm` within `kAsArmWindowMs{5000}` enables — no single-call TX enablement); `autoshift_test` contains no submit call; DIAG registrations inside the P4 block with help lines; NVS keys with defaults 0.
- [ ] **Step 2: Verify failure** → pytest FAIL.
- [ ] **Step 3: Implement** — permit struct + NVS persistence; submit = compose from freshest 0x228 snapshot → dashboard TX queue; self-echo suppression window per `kOwnEchoWindowMs` precedent; runtime status assembled for DIAG + Web.
- [ ] **Step 4: Verify pass; commit** — `feat(p4): autoshift permit gate, authorized sbw submit, diag and runtime status`.

### Task 7: Web UI, full gates, bench evidence, OTA request

**Files:**
- Modify: `include/web/mcp2515_dashboard_ui.src.h` (bilingual drive-page section, house style), regenerate `mcp2515_dashboard_ui.h` per stamp discipline.
- Create: `docs/verification/autoshift-v1-20261105/README.md` (evidence).

**Interfaces:**
- Consumes: `DashAsRuntime` (Task 6).
- Produces: shipped candidate build + evidence pack; staged car experiment plan (A no-op probe / B real command).

- [ ] **Step 1: Failing contract test** — UI contains the autoshift section tokens (bilingual labels, master toggle bound to `autoshift_master`, status fields); src/ui sync rule respected (edit `.src.h`, regenerate `ui.h`).
- [ ] **Step 2: Implement UI section; regenerate ui.h** (two-stage stamp).
- [ ] **Step 3: Full gates, serial** — `python3 -m pytest test/ -q` all green → `IDF_MAINTAINER=1 ~/.platformio/penv/bin/pio run -e p4_3can` clean → `pio run -e lilygo_t2can_dual` byte-identical check vs pre-change artifact → restore/re-stamp ui.h.
- [ ] **Step 4: Bench evidence** — flash nothing without Jordan; on bench approval: OTA, then DIAG `autoshift_status` (gate states), `autoshift_test` (composer output vs hand-computed), gate simulation via crafted frames; record transcript in the evidence README.
- [ ] **Step 5: Commit evidence + request** — `evidence(p4): autoshift v1 bench verification`; then present Jordan the staged car-experiment plan (A then B, each separately approved).
