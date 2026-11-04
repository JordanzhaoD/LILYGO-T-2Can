# Autoshift v1 — Bench Verification Evidence (2026-11-05)

Status: **BENCH COMPLETE** (Jordan approved 2026-11-05: "批准台架烧录").
Board 2, USB `/dev/cu.usbmodem5CF71553661`, STA `192.168.1.146`.

Firmware under test: final-review fix pass `2c5f832` + bench-found NVS key
fix `e2e3688` (branch `p4-3can-board-port`), built
`6.0.0-beta11-p4_3can-e2e3688ea2de`. Suite: 478 passed + 754 subtests;
`p4_3can` build clean. Flashed twice via OTA bare-body POST `/update`
(`X-File-Name: firmware.bin`), http=200 each.

## Must-shows → actual results (Task-6 ruling)

1. **Capability surface** — `/status` reports
   `autoshiftAvailable":true` and the matching `uiBuildId` (transcript 03).
2. **Mode flip** — armed via `autoshift_arm` ×2 within 5 s: log prints
   `[AS] CanC now NORMAL (autoshift armed)`; `autoshift_disarm` and the
   master-off cascade both print `[AS] CanC now LISTEN_ONLY`
   (transcripts 04/05/07). The flip, not just the JSON state.
3. **Gate states** — `autoshift_status` JSON across the chain:
   `off` → `master_on` → `armed` → `master_on` (disarm) → `off`
   (master 0, cascade + I3 parity). Full JSON in transcripts 04/05/07.
4. **Composer vs hand-computed** — on unfrozen v1 the bench composer is
   fail-closed BY DESIGN: `autoshift_test 1` refuses even with a fresh
   synthetic snapshot (fed via `rv2_frame 3 228 4 40 C0 35 00`; the same
   feed moved `sourceAgeMs` −1 → 12642, proving the snapshot pipeline).
   The 16 pinned CRC vectors are proven by the native suite
   (`test_native_autoshift_main.cpp`, 478/478). Compose-and-print with a
   frozen pair arrives with Task 2.
5. **Gate-chain simulation** — busmask-5 posture covered by the snapshot
   feed above plus `state:"armed"` polling; per-gate crafted-frame sweeps
   (0x045/0x00E/0x23E/0x20E) are Task-2 territory — their maps are
   sentinel-unfrozen on v1, so triggers and obstacle gates read blocked
   by construction.

## Bench-found bug: NVS key exceeded the 15-char limit

`autoshift_master 1` printed `NVS write failed` on `2c5f832` (transcript
04). Root cause: the key `"autoshift_master"` is 16 chars; ESP-IDF caps
NVS keys at 15 (`NVS_KEY_NAME_MAX_SIZE`) and `nvs_set_u8` rejects it.
Fixed in `e2e3688`: keys renamed `as_master`/`as_armed`, plus a contract
test that fails on any future NVS key over 15 chars (RED→GREEN,
suite 478/478). This is the bench doing its job.

## Persistence across a real reboot (the definitive proof)

Serial-open alone does NOT reset this board (no boot lines in plain
sessions). Proof used a WiFi OTA of the SAME binary as an unconditional
reboot with the serial line held open (transcript 06):

- pre-OTA: `autoshift_master 1` + `autoshift_arm` ×2 → ARMED, NORMAL
- OTA http=200 → reboot
- boot log: `[BUS] bus_enable=0x5` + `CAN B (TWAI1) disabled` +
  `[AS] permit restored: master=1 armed=1 (bus mode applies on can_rt)` +
  `[AS] CanC now NORMAL (autoshift armed)` — twice (OTA double boot),
  identical both times
- post-boot `autoshift_status`: `{"state":"armed",...}`

## Jordan's CAN B+C directive (2026-11-05)

"台架后进入task2 之前要打开CAN B CAN C 总线测试无崩溃稳定后执行侦查录制"

Board now runs busmask `0x7` (all three CAN controllers constructed at
boot — transcript 07: `[BUS] bus_enable=0x7`, no "disabled" lines,
autoshift `off` = safe posture). A 60-minute idle serial soak
(08-soak-3bus.txt) gates entry into the Task-2 recon recording.

## Baseline forensics note (pre-existing, preserved)

`01-baseline-boot.txt` / `coredump-20261105-bench-tcb84.bin` capture the
TCB +0x84 watchpoint trip that fired on the OLD firmware (`20efa75`) at
~83 min idle uptime, zero traffic. Preserved per the standing ruling
(stop forensics, product route); not investigated further.

## Staged car experiment (requires separate explicit approvals)

- **Stage A — no-op probe**: armed + master on, trigger suppressed;
  observe 0x228 bus health and zero gear change. Jordan approves this
  stage explicitly.
- **Stage B — real command**: one gated submit at standstill, brake
  held, P↔D only. Jordan approves this stage explicitly, after reading
  Stage A's transcript.

## Transcripts

- `01-baseline-boot.txt` — old firmware 20efa75, TCB TRIP abort (preserved)
- `02-baseline-ip.txt` — netif probe, STA IP 192.168.1.146
- `03-ota-fixbuild.txt` — OTA of 2c5f832 + /status stamp/capability probe
- `04-permit-chain-nvs-bug.txt` — full permit chain on 2c5f832; catches
  the NVS key bug (master refused, chain otherwise green)
- `05-permit-chain-fixed.txt` — same chain on e2e3688, NVS write silent
- `06-ota-reboot-persistence.txt` — OTA-forced reboot with live boot
  capture: busmask 5 banner + permit restored + armed after reboot
- `07-bus7-posture.txt` — master-off cascade, bus_enable 7 stored+applied,
  safe posture confirmed after reboot
- `08-soak-3bus.txt` — 60-min three-bus idle soak (crash markers summary)
