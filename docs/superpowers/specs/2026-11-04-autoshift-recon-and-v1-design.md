# Auto-Shift on the Legacy Intel Model X — dictionary-first recon + v1

Date: 2026-11-04. Approved route: Jordan picked "A·侦察优先" over quick-wins and
crash-first, then approved 方案二 (recon firmware with boot-auto-armed buses +
auto-restarting recorder). Two facts verified afterwards — busmask is
NVS-persisted across reboots (stability-baseline design) and recorder v2
ingests all buses un-gated from boot until power-off into the 9 MB flash grid
(dash_recorder_v2.h header) — collapse the recon phase to **zero firmware
change**: the armed trap build 06d26b14 goes to the car as-is.

2026-11-05: Jordan approved the design. The builder check raised four
issues, all resolved: (1) the original text referenced the Atlas repo's
TX admission chain / Soft Glass UI, which do not exist in this worktree —
**v1 lives in THIS (T2CAN) worktree** with a lightweight gate (house arm
pattern + dedicated autoshift permit + master switch; the Atlas admission
subsystem is not ported); (2) a steering-sweep segment was missing from
Phase 1 — added; (3) the first live injection is now staged (no-op probe,
then real command — each separately approved by Jordan); (4) rear-obstacle
posture decided by Jordan: **blocker-only, no extra R confirmation**
(residual risk of weaker rear sensing on busmask 5 accepted).

## Intent

Bring the flagship feature of the reference firmware (Albert P4 RE — auto
gear shift via scroll wheel / steering angle / obstacle assist) to Jordan's
2019 Model X (Legacy, Intel era) on the Atlas-3CAN-P4 board, using Tesla's
own signal dictionary instead of blind frame discovery. The same busmask 5
(CanA+CanC) doubles as the next rung of the crash-family ladder
(2026-11-03-p4-stability-baseline-design.md).

## Evidence base (all verified 2026-11-04, desk-side)

Bus identities locked via the VAPI dictionary
(`tesla_can_explorer_local/datasets/modelsx_intel/raw/`, extracted from
libQtCarVAPI 2026.2.3 MCU2-SX: 1728 frames, signal names + enums, no bit
offsets) cross-checked against the sess_64 tri-bus capture:

- **CanA = CH, CanB = BDY, CanC = PT** (GPIO-for-GPIO same board family as
  Albert; e.g. 0x045 STW_ACTN_RQ, 0x292 BDY mirror, 0x228 split below).
- **Albert's HW4 protocol table does not transplant**: 9/27 IDs matched; the
  two V12 candidates are charger frames here — CanC 0x229 =
  CHGPH2_HVStatus, CanC 0x249 = CHGPH2_thermalStatus2 (V12 checksum law
  0/371 and 0/360 fail; payloads are analog values).
- **Gear command frame found and cracked**: `SBW_RQ_SCCM` (shift-by-wire
  request from the stalk module) = **CanC 0x228, DLC4, ~98 Hz**. b2 high
  nibble = MsgTxmtId (16-count), b3 = **CRC8 SAE J1850 (poly 0x1D, init
  0xFF, xorout 0xFF) over b0..b2 — 16/16 static vectors pass**. Stalk-state
  bytes b0/b1 constant `40 C0` while parked in P with nothing pressed. The
  dictionary CH copy (0x6D) never appears in our captures. (CanB 0x228 =
  MCU_windowCommands, DLC8 idle zeros — same ID, different frame, both
  verified.)
- **Gate/feedback frame**: `DI_torque2` = CanA 0x118 @100 Hz carrying
  DI_gear (position known: b3>>4), DI_gearRequest, DI_brakePedal,
  DI_vehicleSpeed, torque2 checksum+counter (bit layouts for all but gear
  unknown — Phase 2 output).
- **Obstacle suite flows while parked**: 0x20E sdiFront (CanB+CanC), 0x22E
  sdiRear (CanB), 0x23E oocStatus (CanB+CanC), 0x21E pscStatus (CanB),
  0x2BF ussRaw7 (CanC), 0x31E PARK_status (both).
- **Steering angle**: 0x00E STW_ANGLHP_STAT on CanC @~100 Hz.
- **Failure feedback**: PT 0x366 DI_alertMatrix5 carries DI_u002_shiftDenied
  (event frame — absent while parked, expected).
- Albert's auto-shift gate semantics (master / speed ≤ 8 km/h / brake /
  seatbelt / count + 1000 ms cooldown / steering thresholds / scroll
  mappings / runtime block reasons) adopted as the behavioral reference.

Features killed by evidence (do not re-propose without new data): **seats**
(MSMD/MSMP live only on BFT + ETH; BFT is not wired to X179), **ambient
RGB** (no CAN frame on Intel), **Albert's exit-parking chain** (0x293 /
0x3A1 / 0x3FD absent from the Intel dictionary — HW4-only). Deferred:
service mode (0x339 DAS_bodyControls mirror observed on CanB), rear fog
(LHC family, CH 0x205 area) — both CanB-era candidates, not this design.

## Goals

1. **Phase 1 — recon (no firmware change)**: one ~15 min static session on
   busmask 5 capturing stalk codes, brake/speed field positions, obstacle
   behavior near a wall, and 2-bus crash-family stability data.
2. **Phase 2 — offline analysis**: produce the SBW encoding table
   (TSL_RND_Posn_StW per gear + P-button), 0x118 field map
   (brake/speed/gearRequest — note can_helpers.h:86 currently documents
   DI_gear as b2 bits 5-7 while our own field recon proved b3>>4; the
   stalk-sweep timeline disambiguates the two fields), 0x00E steering
   field map (scale/offset/sign), obstacle thresholds, and the SBW TX
   timing budget (source-freshness window) — all from the recon capture.
3. **Phase 3 — auto-shift v1 firmware**: gate chain + scroll/steering
   triggers + SBW injection on CanC through the existing TX authorization
   chain, DI_gear confirm/close loop, block-reason reporting, bilingual UI.

## Non-goals (v1)

- No CanB enable; no seatbelt gate (frame unreachable on our buses — the
  car enforces shift preconditions itself); obstacle acts as a **blocker**,
  not a suggester (no auto P→D/R on proximity); no USS-fusion suggestion
  mode (Albert's V12 path — future candidate); no exit-parking.
- No changes to busmask defaults (0x01 stays; 5 is set per-trip via DIAG),
  no changes to the TCB trap / heap guard / recorder.

## Anti-goals

- No TX outside the autoshift permit gate (NVS master + arm, two-step);
  no autonomous driving behaviors; no bypass of the pytest → build
  serial gate; nothing that touches T2CAN_DUAL behavior; no porting of
  the Atlas admission subsystem into this worktree.

## Phase 1 — field recon procedure (board as-is, 06d26b14)

Static, car in P, brake available, ~15 min: DIAG `bus_enable 5` + reboot,
confirm CanC traffic on the bus page, then lock+export one session per
segment (rv2_lock via Web/DIAG), phone video as alignment backup:

1. baseline idle 30 s;
2. brake hold + stalk P→R→N→D→P slow ×3, then rapid cycles;
3. P-button presses (stalk-end button);
4. brake press/release cycles ×10;
5. nose-to-wall and tail-to-wall approaches (obstacle context);
6. scroll rolls (both wheels, both directions);
7. steering sweeps with brake held (full lock both ways, holds at
   ~30°/60°/90°) — feeds the 0x00E field map (no decoder exists anywhere;
   the dictionary has names but no bit offsets);
8. seatbelt buckle/unbuckle ×3 (long-shot: any observable frame).

Recorder v2 keeps recording across crash-reboots automatically; if the
board crashes (2-bus is an untested ladder rung), note the time, let it
reboot, continue — that IS the ladder data. Coredump lands at 0x15E0000 if
the TCB writer strikes (trip = priority over recon; recover the dump
before further action).

## Phase 3 — auto-shift v1 design

- **Gates (Albert semantics)**: master switch → fresh DI_torque2 →
  DI_vehicleSpeed ≤ limit (default 8 km/h) → DI_brakePedal applied →
  cooldown elapsed → per-session count under limit → obstacle clear
  (0x23E oocStatus + 0x20E sdiFront on CanC, freshness fail-closed;
  rear coverage is weaker on busmask 5 — blocker-only, no extra R
  confirmation, per Jordan's 2026-11-05 decision) → submit.
- **Triggers**: scroll-wheel mappings (per-side pos/neg → P/R/D/D-R
  toggle, reusing the 0x045 decoder) and steering-angle thresholds (P:
  infer D/R from sign; D/R: toggle; defaults 30°/60°, configurable).
- **Repo + TX gate (2026-11-05 decision)**: v1 lives in this T2CAN
  worktree. Lightweight gate — NVS-persisted `autoshift_master` +
  `autoshift_armed` (DIAG two-step arm, house pattern), no Atlas
  admission port. TX submits through the same dashboard TX queue used by
  the 0x292 gateway mirror.
- **TX**: copy the freshest CanC 0x228 RX inside the source-freshness
  window (budget from Phase 2), set TSL_RND_Posn_StW to the target code,
  MsgTxmtId = source counter + 1 (never a local rolling count — the live
  SCCM stream keeps its own sequence), recompute J1850 CRC, submit.
  Single in-flight request; deadline; retry cap.
- **First live injection is staged**: (A) no-op probe — re-inject the
  current P-state frame while parked in P (semantically identical to the
  live stream; validates CRC/counter/acceptance machinery), then (B) a
  real command. Each stage requires Jordan's explicit approval.
- **Close loop**: DI_gear reaches target → complete + cooldown count; on
  mismatch/timeout → cancel, publish block reason (runtime status +
  optional 0x366 shiftDenied readout).
- **UI**: drive-page section, bilingual, Soft Glass system; per-feature
  master toggle follows the existing defense-page pattern.
- **Contract tests**: the 16 known SBW CRC vectors pinned in pytest; CRC
  implementation; gate ordering; single-in-flight; TX rides only through
  the authorization path; T2CAN_DUAL zero-diff.

## Approaches considered

- A quick-wins first (CanB body features): rejected — CanB is the heaviest
  bus (1771 fps) and nothing on it is needed for the flagship.
- B recon firmware with boot-auto-arm + recorder autostart: superseded —
  both properties already exist in production firmware (NVS busmask,
  always-on recorder); no OTA needed before recon.
- C (chosen) dictionary-first recon on the armed build → offline analysis
  → v1 on busmask 5: every v1 input lives on CanA+CanC; merges the
  feature route with the pre-planned ladder rung (+CanC ≈1.5 kfps); keeps
  the TCB trap baited during the session.

## Constraints

- Build recipe IDF_MAINTAINER=1 / penv62 / pinned platformio_local.ini;
  ui.h two-stage stamp discipline; pytest → pio build strictly serial.
- OTA-only flashing (never `pio run -t upload`); every board action
  (flash, TX experiment) requires Jordan's explicit go-ahead.
- Commit/release notes in English; `.claude/scratch/` and `.superpowers/`
  never committed; push only with per-push approval.

## Verification

- Phase 1 exit: encoding table complete (4 gears + P-button), 0x118 field
  map pinned, obstacle behavior characterized, session files archived
  (Downloads + MacServer), crash-or-not at busmask 5 documented ≥15 min.
- Phase 3 gate: full pytest suite green, pio build clean for p4_3can and
  lilygo_t2can_dual, bench simulation of the gate chain + CRC (16
  vectors), staged first injection in the car (A no-op probe, B real
  command) with Jordan approving each stage explicitly, evidence pack
  per repo norms.

## Left to the builder

Exact bit positions and gear-code values (Phase 2 output), SBW
source-freshness budget, obstacle threshold defaults, UI copy details,
runtime-status JSON shape, and the bench harness for the close loop.
