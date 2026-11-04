# Albert-mechanism port — bench E2E on board 2 (2026-11-04)

Firmware under test: `firmware-bc1d106.bin` (receipt:
`../p4-twai-node-hotpath-20261104/README.md`), OTA'd to board 2 at
192.168.1.146 (uiBuildId `bc1d10615e26` verified via /status; boot banner
shows the three `[BUS]` lines with bus_enable=0x1 unchanged).

## What ran

1. DIAG session 1 (`p4_diag_session1.log`): bus_enable get (0x1) →
   can_status baseline → `exp_b 200 10 1` → can_status → rv2_resets.
2. Flood session 1 (`p4_flood_session.log`): 30 s `exp_b 200 30 1`;
   captures panic #1 (which fired ~7 min after session 1, at idle) and
   the fresh boot's ELF SHA line.
3. Flood session 2 (`p4_flood_session2.log`): 30 s `exp_b 200 30 1` with
   interleaved /system_status probes (`p4_probe2.jsonl`); captures the
   reboot of panic #2 and its queue-full tail.
4. Coredump pulled from flash after panic #2
   (`p4_core_bc1d106_raw.bin` → strip 12-byte header → `core.elf`),
   decoded against `.pio/build/p4_3can/firmware.elf`
   (ELF SHA256 prefix `0e079e5` matches the on-boot log).

## Finding 1 — exp_b self-flood TX stall (expected physical limit)

`tx=17 fail=1983/3983/…`, `tx queue full` ×5359/5903, `rx=0`.

- 17 accepted = 1 mounted + 16 queued (tx_queue_depth); **zero TX
  completions in 30 s**.
- Driver-level diag after: `state=running, tec=0, rec=0, busError=0,
  msgsToTx=16(=empty), framesRead=0`.
- `busError` increments only inside the ISR (BUS_ERR/ARB_LOST paths) —
  zero across all sessions ⇒ **the TWAI ISR never executed on the bench**.
- Consistent with round-9's conclusion (exp-b-20261103/README.md:38):
  no bus-less path to the real ISR chain on this silicon (P4
  `twai_ll_set_mode` ignores the loopback argument; self-test + self-rx
  still needs a real differential bus to complete). **Not a port
  regression by evidence, but the node-API RX path remains unvalidated
  until wired or in-car.**

## Finding 2 — httpd TCB corruption reproduced on bc1d106 (the big one)

Two `abort()` panics, both `dashHgTcbScan → dashHgTcbTrip(verdict=4
kDashHgTcbTripStackBase)`, crashed task `heappatrol`, core 1.

Panic #1 (log, session-1 boot at idle ~t445 s): backtrace symbolized —
`panic_abort ← esp_vApplicationTickHook ← __assert_func ← dashHgTcbScan
← dashHeapGuardCheckTcbs ← vPortTaskWrapper`.

Panic #2 (coredump, `tickMs=50156` ≈ 2 s after the 30 s flood window):

| field | round-8 trip #1 (82697ba) | today (bc1d106) |
|---|---|---|
| task | httpd #17 | httpd #18 |
| field | TCB+0x30 pxStackBase | TCB+0x30 pxStackBase |
| expected | 0x4ff75040 | 0x4ff97a64 |
| actual | 0x4ff750c4 | 0x4ff97ae8 |
| **delta** | **+0x84 (132 B)** | **+0x84 (132 B)** |
| trigger context | 1 Hz /system_status poll storm (browser tab) | 6 s /system_status probes during flood |
| TWAI ISR activity | zero CAN traffic | busError=0 ⇒ ISR never ran |

Same writer signature (field, task, +0x84 into the /status deepest-frame
residue region), same trigger (system_status polling), across old and
new firmware. The full-IRAM node-API hot path did not prevent it — and
with the ISR provably idle, **CAN interrupt pressure is not required for
this corruption family**.

## Interpretation

- The Albert-mechanism port addressed the flash-cache/ISR-storm theory.
  The bench now shows the surgical-TCB-rewrite family occurring with
  zero TWAI interrupts and only mundane HTTP load, on both driver
  generations ⇒ the hot-path placement is exonerated *for this family*;
  suspicion consolidates on the HTTP/hosted/SDIO/platform layer (the
  round-8/9 "below app layer" verdict, now with a cross-firmware
  reproduction).
- In-car implication: the 12-22 s family may share the writer; an
  iPad-dashboard-open (1 Hz /system_status polling) car session is now
  the pointed control (FIELD-GUIDE experiment C).
- `+0x84` identical across builds ⇒ deterministic writer, value derived
  from the httpd stack area. Candidate directions: misdirected context
  save, hosted SDIO DMA stray write, pmu/plic path (unchanged verdict).

## Ledger

`/ring_status` resets (newest first) after both panics:
`[1,0](esptool RTS) [4,0] [4,0] [3,0] [3,0] [1,0] [3,0] [1,0]` — the two
`[4,x]` are today's tripwire aborts. Board stable at idle since.

## Not done / next

- No push of this evidence commit yet (awaits Jordan).
- rx/ISR validation needs wiring (CanA↔CanB jumper) or the car.
- Experiment C (HTTP polling A/B) is now the sharpest bench experiment;
  the poll trigger is confirmed twice.
