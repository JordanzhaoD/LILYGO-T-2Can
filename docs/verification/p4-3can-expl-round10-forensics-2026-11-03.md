# Experiment L Round 10 — dump forensics, identity audit, fingerprint unification

Date: 2026-11-03 (late night field ladder + same-night MacServer dump analysis)
Scope: read-only forensics over the evidence pulled from the MacBook field session
(`~/l-field-results-20261103/`). No board code changed. Ladder result table lives in
`FIELD-GUIDE-filled.md` (filled by the field session).

## 1. Evidence identity audit (before anything else)

The five expL `core-*.elf` files were labeled by the rung under which they were
*retrieved*. Their **contents** say otherwise. Embedded `uiBuildId` strings:

| Dump file | uiBuildId found inside | Real firmware of that crash |
|---|---|---|
| core-L0-crash2.elf | `6.0.0-beta11-p4_3can-a72f22b8167d-2026-11-03T11:08:31Z` | a72f22b (pre-ladder board firmware) |
| core-L1-last.elf | same a72f22b string | a72f22b (stale) |
| core-L2-last.elf | same a72f22b string | a72f22b (stale) |
| core-L4-last.elf | `6.0.0-beta11-p4_3can-82697baff518-2026-11-03T12:31:35Z` | 82697ba ✓ its own |
| core-L5-tonight.elf | (no version string captured) | a72f22b layout, fresh (md5 ≠ L4) |

Morning `run-001..009/core.elf`: no version string captured, but poller
`uiBuildId` had already pinned the morning firmware as
`82697baff518-2026-11-02T18:34:36Z` (an earlier build of the same commit as L4).

**Conclusion:** crashes under 535d03d / 247cc2b / b574a7e (L0/L1/L2) never
overwrote the coredump partition — the "L0/L1/L2 dumps" are one stale pre-ladder
a72f22b crash, read three times. The dividing line for "did the crash write its
dump" is exactly 82697ba, which is also the commit that changed
`CONFIG_ESP_SYSTEM_PANIC_REBOOT_DELAY_SECONDS` from 0 → 10 (sdkconfig.p4_3can).
All symbol reads below are attributed only to dumps whose firmware is confirmed.

ELF note: per-rung ELFs were rebuilt locally; the earlier "bin_fresh == bin_kit"
check compared sizes only. Panic-printed ELF sha prefixes do not match rebuilt
ELFs (build-timestamp artifacts), but address resolution was verified exact
(line-level hits in `memcpy.c`, `tasks.c`), so symbolization stands. The kit
a72f22b ELF is byte-exact (`deded3b9f` matches the L5 panic print).

## 2. Fingerprint unification — one phenomenon, four views

Serial-side classes (from the field serials, USB CDC survives resets):

1. **Instr access fault, MEPC=0 / RA=0 / MTVAL=0** — jump target zeroed → fetch at 0.
2. **L4 crash1: Store access fault, MTVAL=0x1, MEPC=`memcpy+172` (IRAM), RA=`app_can_task+1060`
   = src/main.cpp:215 at 82697ba** — the inlined `P4TwaiSecondaryDriver::read()` frame
   copy; the destination pointer register was zeroed to 0x1 (odd address → memcpy's
   byte-alignment loop at memcpy.c:56 → store to 0x1). Data structures were NOT corrupted;
   a register died mid-instruction.
3. **L2 crash1: Illegal instruction, MEPC=0x40017200 = RA, MTVAL=0x4a412a47.**
   Disassembly: 0x400171fe is a 4-byte `addi a5,a5,676` whose immediate bytes at
   0x40017200-0x40017203 are exactly `47 2a 41 4a`. So 0x40017200 is the **middle of a
   valid instruction** — a corrupted jump target that happens to land mid-instruction;
   the "garbage instruction" is real flash bytes.

Dump-side class (all 14 dumps, spanning two builds of 82697ba and two a72f22b
crashes — five distinct code layouts):

4. **ESP_PANIC_DETAILS note = `abort() was called at PC 0x40004521 on core 1`** in every
   dump. `abort()` reports `__builtin_return_address(0) - 3`; 0x40004521 is an **odd**
   address, which cannot be a call site (RISC-V compressed insns are 2-byte aligned),
   and 0x40004524 resolves to *different functions* in different builds. A constant,
   impossible call-site value across five layouts means **the ra register itself was
   poisoned (to 0x40004524) before abort() ran** — corruption first, abort second.

Unified statement: **the crash family is control-flow / register corruption —
jump targets, return addresses, and pointer registers get zeroed or poisoned with
fixed garbage while the CPU is running normally.** It is not heap trampling
(L4's heap poison was silent), not TCB surgery (tripwire v2 was silent), not
bus-off recovery churn, and not recorder v2 (L1 ≈ L0 delays).

## 3. Interrupt-pressure telemetry at the moment of the L4 crash

Read from core memory (`appDriver` → TWAIDriver::diag_ on the CanA primary):

- state = **Running**, tx/rx error counters = 0, busError = 0, **recoveries = 0**
  → no bus-off, no recovery churn — that hypothesis is dead for CanA.
- **rxOverrun = 14857, rxMissed = 1148** in ~71 s of uptime ≈ 200 RX-queue overruns/s.
- framesRead 36194, framesAccepted 2025 → 94 % of frames are hardware-mask false
  positives, software-dropped after the ISR already paid for them.
- CanC (listen-only PT lane): rx 35682, missed 220, tx/rx error 0 → healthy.

## 4. Hypothesis status after round 10

- **Regression hypothesis: collapsed.** beta10 (535d03d) crashes in-car (81–833 s);
  the 33-commit range modulates crash *delay* only (blackbox engine ≈ 10× faster,
  hosted 2.13.1 ≈ second-level). Recorder v2 is neutral (L1 ≈ L0).
- **Standing hypothesis:** register/control-flow corruption under RX interrupt
  storm on ESP32-P4 v3.2 (ECO7, ROM `esp32p4-eco7-20260109`), with crash delay
  inversely proportional to ISR + PSRAM/cache load. Bench never reproduces because
  its buses are silent. ECO7 has documented Guru-class quirks (esptool release
  notes: "Fix ECO7 early guru meditation errors").
- **Open:** the corruption source (TWAI ISR path vs esp_hosted SPI path vs
  silicon). All four rungs that crash have both active; no rung isolates them.

## 5. Next experiments (proposals, none implemented)

- **C (zero-firmware, FIELD-GUIDE §7 already):** in-car with hosted traffic removed
  (keep the car head unit off the board AP / close the dashboard) — isolates the
  hosted contribution with the exact same L5 firmware.
- **M (firmware change, needs approval):** per-bus enable mask (CanA/B/C) flashed
  per run — the clean mechanism discriminator for the TWAI ISR path.
- **P (firmware change, needs approval):** panic-forensics hardening — abort-hook
  backtrace into the coredump details, and one PRINT_HALT run to capture the full
  assert text that USB CDC keeps eating. Also fix the coredump-write gap for
  pre-82697ba-era configs if we ever ladder through old rungs again.

## 6. Evidence pointers

- Dumps + serials + filled ladder table: `~/l-field-results-20261103/` (not committed).
- gdb extraction scripts: `~/.claude/jobs/1f666ffd/tmp/gdb_pass.sh`.
- Rebuilt per-rung ELFs: `~/l-field-results-20261103/elFs/firmware-{535d03d,247cc2b,b574a7e,82697ba,a72f22b}.elf`.
- Chip: ESP32-P4 v3.2 / ECO7 (from field serial banner `ESP-ROM:esp32p4-eco7-20260109`).
