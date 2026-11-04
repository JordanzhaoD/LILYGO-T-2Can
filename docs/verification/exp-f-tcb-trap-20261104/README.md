# Experiment F — hardware TCB write-trap: PMP dead, trigger watchpoint live (2026-11-04)

Follows `../exp-de-bisect-20261104/` (five bench arms, zero reproductions
of the httpd TCB `pxStackBase` +0x84 rewrite). Decision: stop
reproducing, trap the writer in hardware. Commits `ce13962` (v1, PMP),
`06d26b1` (v2, trigger) + stamps; final bench firmware stamped
`6.0.0-beta11-p4_3can-06d26b140832-2026-11-04T11:52:16Z`
(sha256 `b8f1129c…15917`), OTA'd to board 2 and standing by armed.

## v1: PMP TOR entries — killed by on-chip evidence

Two locked TOR entries (14 bound, 15 = L|TOR|R, W denied) over
[TCB+0x30, TCB+0x34), armed on both harts at guard adoption. First bench
run: banner claimed armed, DIAG `pmp_trap_test` store **passed through
unimpeded** (NO FAULT). CSR readback added and re-flashed; the numbers:

```
[heap_guard] PMP trap armed @0x4ff748a0 (entries 14/15)   <- our claim
[pmp] verify pmpcfg3=0x80000000 pmpaddr14=0x13fdd220 pmpaddr15=0x22000000
... 2.5s later (self-test moment) ...
[pmp] pmpcfg0=0x9d9d9b9b pmpcfg1=0x808b8d80
[pmp] pmpcfg2=0x8b808b8d pmpcfg3=0x80001d1d
[pmp] pmpaddr14=0x13fdd220 pmpaddr15=0x22000000 mstatus=0x00010089
```

Reading:

- **entry15**: we wrote cfg `0x89`, hardware kept `0x80` (L set, A=OFF,
  R cleared) and `pmpaddr15=0x22000000` is not our value — the entry was
  locked-empty at boot; L=1 entries reject even M-mode reconfiguration.
- **entry0-11** (`0x9b/0x9d/0x8b/0x8d` bytes): all locked by IDF startup
  memory-protection layout.
- **entry12-14** (`0x1d` = NAPOT|R|W|X, no L): borrowed at runtime by
  system code — entry14 read `0x00` right after arming and `0x1d` 2.5 s
  later. Borrowing them would corrupt system behavior.
- `pmpaddr14` also shows the P4 granularity: `0x4ff748a0>>2 = 0x13fdd228`
  read back `0x13fdd220` — addresses quantize to 64 bytes, so a 4-byte
  field cannot be fenced exactly even if an entry were free.

Conclusion: zero usable PMP entries on a running P4 IDF system; v1 dead
by evidence, kept in history (`ce13962`).

## v2: trigger module watchpoint — live and verified end-to-end

IDF's official `esp_cpu_set_watchpoint()` (esp_cpu.h; with no debugger
attached it goes straight to `rv_utils_set_watchpoint`, which drives
`tselect`/`tdata1`/`tdata2` + `tcontrol` MTE). P4 exposes
`SOC_CPU_WATCHPOINTS_NUM = 3` per hart; production firmware uses none
(the semihosting path only runs with a debugger). Slot 0, 4-byte NAPOT,
store-only, armed on both harts at guard adoption, gated on a WARL
readback (type==2, STORE and MACHINE bits kept, tdata2 = 4-byte NAPOT
pattern) verifying on **both** harts before arming is claimed — v1's
lesson (a CSR write accepted is not proof the trap is live) made law.

Bench transcript (board 2, v2 intermediate build):

```
[heap_guard] TCB write-trap armed @0x4ff7489c (trigger slot 0, both harts)
>> tcb_trap_test
[tcb_trap] tdata1=0x210000ca tdata2=0x4ff7489d
[tcb_trap] self-test store to 0x4ff7489c; breakpoint exception expected at this PC
Guru Meditation Error: Core  0 panic'ed (Breakpoint). Exception was unhandled.
MEPC    : 0x400243bc  RA      : 0x400243b2  SP      : 0x4ff45da0  GP      : 0x4ff32200
Backtrace: 0x400243bc:0x4ff45da0 0x400244a2:0x4ff45f00 0x400303d4:0x4ff45f50 ...
ELF file SHA256: af1733b9b
rst:0xc (SW_CPU_RESET),boot:0x30f (SPI_FAST_FLASH_BOOT)
```

`MEPC 0x400243bc` falls inside `dashSerialRunCommand`
(`0x4002303a + 0x13be`, the bench test is inlined there) — i.e. the
exception fired **at the store's own PC**, before the store committed.
Panic → coredump → clean reboot → trap re-armed. The instrument is
precise: whatever writes that word next stops the CPU with the writer's
PC in MEPC and lands in the coredump.

`tdata1=0x210000ca`: type=2 (mcontrol), match=NAPOT, M|U enable,
store-only, action=0 (breakpoint exception). `tdata2=0x4ff7489d` is the
4-byte NAPOT encoding of `0x4ff7489c`.

## Post-OTA freeze (one-off, resolved)

After OTA'ing the final stamped build, the board's soft-reboot hung
early (serial stops right after `app_init`, no watchdog reset, HTTP
down for 60+ s; reproduced across two boots). An `esptool chip_id`
nudge (which hard-resets via RTS) brought it straight up: armed banner
at t+3.0 s, STA up, `uiBuildId …06d26b140832` confirmed. Firmware
itself is fine on a clean boot. If a future OTA reboot hangs the same
way, esptool nudge is the known unlock. (Not root-caused; candidate:
early-boot state left by the panic/coredump + OTA restart sequence.)

## Standing by

Board 2 now runs the armed build idle-stable. Next trip on that word —
bench, car, wherever — yields the writer's PC directly in the panic
backtrace / coredump, no reproduction recipe needed.

## Run F1 — first armed car trip (2026-11-04 22:28-23:02): clean negative

Firmware 06d26b14, CanA-only (route-A baseline busmask 0x01), board 2
USB on MacBook with full serial + HTTP + events capture
(`~/Desktop/car-runs/run-f1/` on MacServer: serial-f1.txt 165KB,
events-f1.md, poller-f1.jsonl, coredump-f1.raw, probe.bin).

| phase | duration | crashes | trap |
|---|---|---|---|
| static (car awake, CanA traffic) | ~3 min | 0 | armed, no trip |
| drive | ~10 min | 0 | armed, no trip |
| return | ~14 min (~2 min dark window) | 0 | armed, no trip |
| replug check | full boot capture | 0 | re-armed @0x4ff747f4 |

Hard results:

1. **~27 min CanA-only real-car, zero crashes** — vs. the same car on
   3 buses dying in 12-22 s. Route-A baseline (20efa75 lineage) holds
   in the field; ISR-pressure hypothesis gains another point. (Bar for
   "baseline met" was >=30 min; ~27 min observed — Jordan's call
   whether to extend or accept.)
2. **The pxStackBase writer never struck** — trap armed on every
   observed boot, zero trips, ledger gained no [4,x]. The writer's
   activity window plausibly needs the 3-bus pressure and/or polling
   storm that this baseline run deliberately lacks. Next ladder step
   (bus_enable 5 = +CanC) will show.

### Correction: coredump partition address

The bench brief above said coredump @ 0xFE0000 — that was the 16MB
partition table. p4_3can actually uses
`partitions_32mb_ota_8192k_nvs64.csv` (32MB flash): **coredump =
0x15E0000, size 0x100000 (1MB)**. The F1 probe found an ELF magic at
0x15E0000 — a dump IS present, almost certainly the bench self-test
Breakpoint panic. Two reversals follow:

- **Breakpoint panics DO write coredumps.** The earlier "partition
  empty after self-test" observation was a wrong-region read, not a
  write failure.
- Future no-MacBook trips keep a full evidence chain: any trip leaves
  a recoverable dump with the writer's MEPC, independent of serial.

(Pending when the board returns to bench: read 0x15E0000, confirm the
dump's embedded uiBuildId = 06d26b140832 and MEPC 0x400243bc.)

## Blackbox clips from F1 (+ Nov-2 sess_64): CanA-only field data profile

Recorder v2's first field outing — record, lock, export (bin + csv)
all worked. CSV reminder: `id` column is DECIMAL (69=0x045 bit us
again). Clips live in `~/Downloads` (and MacServer); analysis summary:

| clip | date | buses | fps | unique ids | gear(0x118 b3>>4) |
|---|---|---|---|---|---|
| sess_64 | Nov 2 (static, car awake) | 1+2+3 | 3087 | 364 | — |
| sess_70/71 | Nov 4 F1 (driving) | 1 | ~302 | 11 | 4 (=D) const |
| sess_68/69 | Nov 4 F1 (driving) | 1 | ~302 | 11 | 4 (=D) const |

All four F1 clips fall inside the driving window (ts 278-638 s of the
single crash-free boot). CanA-only traffic = 11 known IDs only
(0x118/0x186 @100 Hz, 0x488 @50, 0x370 @25, 0x045/0x318 @10, 0x3ee @4,
0x399 ~2, 0x398/0x2f8/0x438 @1). Scroll-wheel decode confirmed on real
input (2-4 motion frames per clip, matching physical scrolls; board
also TXed 2 mirrored 0x045 frames at the same moment).

**Quantified load delta**: CanA-only driving = 303 fps vs 3-bus static
= 3087 fps (bus2/BDY 1771 + bus3/PT 1245 contribute 97%, 356 IDs).
The 12-22 s family's required condition is "3 buses present" (L0
baseline crashed, CanA-only does not); whether the driver is raw
ISR/rx-path load or 3-bus-specific content stays open — the
bus_enable ladder (5 = +CanC ≈1.5 kfps, 7 = full ≈3.3+ kfps) is
exactly the instrument to separate those axes.

**TX was active the whole trip** (and already on Nov 2): 0x3EE bit46
refresh @~1.1 Hz, bit46 alternating set/clear (58/59), riding the
fresh-RX tail precisely (TX-after-RX gap median 0.2 ms, max 0.6 ms).
The car's own 0x3EE never carries bit46 (0/117 RX) — the board is the
only bit46 source, i.e. the whitelist-bridge core function ran in the
field for ~27 crash-free minutes alongside the baseline. For a
zero-TX observation baseline, disarm via the defense-page TX master
switch.
