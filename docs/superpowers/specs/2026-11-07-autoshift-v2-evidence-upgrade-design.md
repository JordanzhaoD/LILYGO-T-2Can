# Auto-shift v2 — ka750 evidence upgrade (sensors, sequence composer, P<->D)

Date: 2026-11-07. Status: approved for implementation (Jordan, same day).
Supersedes the sensor/composer half of
`2026-11-04-autoshift-recon-and-v1-design.md` Phase 3; the permit gate,
CRC, snapshot and fail-closed posture carry over unchanged.

## What the ka750 field round changed

The 2026-11-06/07 ka750 evidence round closed three of the four v1 freeze
gaps and corrected two v1 assumptions:

1. **Shift commands are native to this car and fully observed**
   (`~/atlas-field-kit/evidence/ka750/sess_s4_gears.csv`): every gear
   transition rides a CanC 0x228 command burst — the old "末三换挡零脉冲"
   gap was a recorder lock-window artifact. Commands are multi-frame
   PHASES at a 10 ms cadence inside the gateway-owned ~100 Hz idle
   stream (idle b1=0xC0, DLC4, b2 = MsgTxmtId walking +0x10 per frame
   across idle AND command frames). Gear (CanA 0x118 b3>>4) flips 0-30 ms
   after burst start, worst observed 520 ms after a short burst. Single
   frames CAN shift (observed twice), but every clean observation is a
   4-26 frame burst. Phase table (observed):
   - P->N: C4 x8-12 (sweep lead-in x4)
   - N->D: C8 x13-26
   - D->N: C2 x6-26
   - N->R: C2 x6-9 then C1 x19-23 (two-phase; recon2-era "C1C2" order
     was wrong)
   - ->P: D0 x12 (from R via stalk; from D via P button — recon2)
   - P->D one sweep: C4 x4 then C8 x26, no gap (s4 @209.789)
2. **Speed sentinel closed**: 0x106 (CanC, ~100 Hz) b5:b6 big-endian,
   4 mm/s per tick — 3-point calibration 797/1354/2104 tick =
   11.5/19.5/30.3 km/h. The v1 guess (0x118 b4/b5 vehicle speed) is
   disproven (constant-offset counter pair).
3. **Brake sentinel closed**: 0x116 (CanC, ~100 Hz) b1 bit7 — one
   continuous 41.8 s hold across the whole shifting test, exact.
4. **Wheel trigger mechanism cracked** (s3 + session_log): event pages
   0x514 (left wheel) / 0x51B (right wheel) on **CanB**, DLC8, ~1 Hz;
   b0=0x10 press / 0x11 release arriving as pairs ("10 01 05"/"11 00
   04"). The v1 decoder target (0x045 scroll episodes) decodes neither
   press nor direction — dead end, removed.
5. **Still open**: obstacle discrimination (0x3BA/0x388 await the
   long-pole box-push test) -> obstacle gate stays sentinel fail-closed.
   Steering-angle scale awaits video calibration -> angle stays sentinel.

## Jordan's decisions (2026-11-07)

- **Trigger mapping: P<->D only.** Left-wheel press (press+release
  pair) requests P->D when in P, D->P when in D. Both TX sequences are
  field-proven end to end; no unobserved sequence is ever composed.
  Requests from N/R/invalid gear are dropped at trigger time.
- **CanB posture: master ON auto-enables CanB RX.** The v1 anti-goal
  ("no CanB enable") predates knowing the wheel pages live there.
  Implemented as a pure runtime OR — effective mask =
  operator NVS mask | (master ? CanB : 0). The operator's stored mask is
  never rewritten; master off (or boot with master off) returns CanB to
  the operator's own posture. RX only, never TX.
- Standing P0/P1/P2 discipline (2026-11-07): autoshift is P1. Its TX
  path is the existing direct CanC driver send (no dashboard queue hop),
  which already satisfies "P1 must not queue behind P2 (WebUI/logs)";
  P2 deferral semantics (delay != failure, state kept, retry) match the
  engine's retry-not-abandon behavior inside the retry budget.

## v2 design

### Profile (frozen from evidence; sentinels remain for angle/obstacle)

- Sequence table: P->D = [C4 x4, C8 x25]; D->P = [D0 x12];
  `frameCadenceMs = 10`; counter rides +1 (nibble) per frame.
- `speedFreshMs = 300` (0x106 @100 Hz), `brakeFreshMs = 300` (0x116),
  `t2FreshMs = 300` (0x118), `sbwSrcFreshMs = 100` (native 10 ms
  cadence), `wheelPressTimeoutMs = 2000`, `confirmTimeoutMs = 1000`
  (covers 300 ms burst + 520 ms worst observed lag).
- Deleted (disproven): `stalkB0/stalkB1` per-gear table, 0x118
  brake/speed byte maps, 0x045 wheel map, `scrollEpisodeGapMs`.

### Engine (single owner of the in-flight request)

`dashAsRequestShift(target, nowMs)`: valid only for P<->D from a fresh
decoded 0x118 gear; refuses when busy. Arms the burst state machine.
`dashAsEngineTick(nowMs, out)`: emits one frame per 10 ms while
bursting — first frame rides the freshest native 0x228 counter +1
(source freshness gate), later frames walk our own counter — recomputes
the J1850 CRC per frame. Burst end -> confirming until 0x118 shows the
exact target gear. Timeout -> auto-retry (re-ride the freshest source)
up to `retryMax`, then abandon with reason. A stale source at retry
time abandons (fail-closed). Disarm cancels any in-flight request.

### Gate chain (order re-pinned)

master -> stale 0x118 -> stale speed -> speed > limit -> stale brake ->
brake not applied -> cooldown -> session count -> obstacle (sentinel
fail-closed) -> pending -> already-target -> submit. Two new verdicts
(`AS_BLOCK_STALE_SPEED`, `AS_BLOCK_STALE_BRAKE`) make the new sensor
latches diagnosable in /autoshift_status.

### Wiring

- CanC drain: + 0x106 (dlc>=7) -> speed decoder; + 0x116 (dlc>=2) ->
  brake decoder. 0x228 snapshot unchanged (own-echo suppression now
  also covers the burst cadence).
- CanB drain: + 0x514/0x51B (dlc>=3) -> wheel event decoder (left page
  only consumes; press+release pair completes one gesture).
- CanA drain: 0x118 stays (gear + freshness + close loop); 0x045
  autoshift decoder removed (0x045 keeps its legacy DND consumers).
- `t2canAutoshiftTick` becomes the engine pump (compose + send + pace),
  still gated behind master+armed and the CanC LISTEN_ONLY->NORMAL flip.
- `dashAsSetMaster(true)` ORs CanB into the effective bus mask (see
  posture above); no NVS writes.

### Non-goals (v2)

- No new trigger surfaces (steering angle, roll direction, right wheel,
  phone buttons) until their evidence lands.
- No obstacle gating change (sentinel fail-closed until the box-push
  adjudication); no N/R targets; no sequence other than the two frozen
  above; no changes to T2CAN_DUAL behavior.
- First live injection stays staged exactly as v1 pinned it: Stage A
  no-op probe (re-inject current idle frame, counter+1) then Stage B
  real command — each requires Jordan's explicit approval. Everything
  in this change is bench-verifiable without bus TX.

## Test plan (TDD)

Native suite (test_native_autoshift_main.cpp), each RED first:
0x106 decode at all three calibration points + rounding + freshness
fail-closed; 0x116 brake decode + freshness; wheel pair completes /
press-only never fires / stale release drops / right page ignored /
N-and-R gear context drops; P<->D sequence emission (codes, counts,
counter ride first-and-later frames, CRC per frame, 10 ms pacing);
burst end -> confirm window; exact-gear confirm -> complete + stats;
timeout -> auto-retry with re-ridden fresher counter; retry exhaustion
-> abandoned; stale source at request/retry -> refuse/abandon; gate
order with the two new stale verdicts. Contract suite: drain wiring
pins (0x106/0x116/0x514), profile frozen-value pins replacing the
UNFROZEN sentinel test, gate-order pin update, busmask OR semantics,
status JSON fields. Full gate chain serial: native -> pytest -> pio
build (wrapper) -> ui.h restore.
