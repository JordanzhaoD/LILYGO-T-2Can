# Experiment B — bench NO_ACK self-flood + TCB trip #1 forensics (2026-11-03)

Firmware: `6.0.0-beta11-p4_3can-a72f22b8167d` (exp_b command, commits a72f22b + f6c5309, pushed).
Board: bench board 2 (USB `/dev/cu.usbmodem5CF71553661`, LAN 192.168.1.146), no physical CAN bus.
Round-7 context: in-car 12–22 s crash family verdict = register-file group zeroing under TWAI
interrupt pressure, below the app layer. Experiment B was built to drive the REAL TWAI ISR path
on the bench (NO_ACK + `TWAI_MSG_FLAG_SELF` self-reception flood, 800 fps/lane, 300 s, mask 3).

## Files

| file | what |
|---|---|
| `b1_run1.txt` | blast 1 serial log (command 19:12:18 → done 19:17:28; trip ~19:18:15, panic NOT captured — runner grace was 45 s) |
| `b1_run2.txt` | blast 2 serial log (19:34:13 → done 19:39:13, grace 150 s; **no trip**) |
| `coredump_raw.bin` | 1 MB flash coredump @0x15E0000, pulled after trip #1 reboot |
| `core.elf` | ELF-wrapped coredump (12-byte flash header stripped) |
| `idle_watch1.txt` | 35-min passive watch after reboot (empty — zero output, zero trips) |

## Blast verdict (B1): INVALID as an ISR-pressure test — and why no firmware can fix it alone

Both runs: `tx=17 frozen, fail≈240k, rx=0`. **Register-level post-mortem (IDF 6.0.2 headers):**

- `TWAI_MSG_FLAG_SELF` maps to the SJA1000 **self-reception-request command** — transmit onto
  the bus AND receive the own frame back (`esp_twai.c:747` `.config.loopback = message->self` →
  `twai_hal_v1.c:391` → `twai_ll_set_cmd_self_rx_request`). It still **drives the physical bus**.
- `TWAI_MODE_NO_ACK` maps to `mode.self_test_mode` (`twai.c:565`) — suppresses only the
  *missing-acknowledge* error, **not bit errors**.
- The ESP32 TWAI IP has **no loop-back mode register bit at all** — classic ESP32/S3
  (`twai_struct.h`: rm/lom/stm/afm, "MOD.4 Sleep Mode not supported") and P4/C6/H2
  (`hw_ver3/twai_struct.h` bits 0..3 = reset/listen_only/self_test/acceptance_filter).
  `TWAI_MODE_LOOP_BACK` **does not exist in any IDF**; the new-API `flags.enable_loopback` is
  just "SRR on every frame" (`esp_twai_onchip.c:179`). The MCP2515 (T2CAN's secondary) does
  have a true internal loopback — the ESP32 native controller does not.
- So with a floating bus: recessive bits read garbage → bit/stuff errors → infinite
  retransmit → TX buffer never frees → tx queue fills at 17 → every later `twai_transmit`
  fails (240k), no frame ever completes → rx=0, the ISR chain never flows.

Conclusion: **there is no bus-less path to the real ISR chain on this silicon.** Any valid
bench ISR-pressure test needs a physical differential bus, however small.

## Trip #1 — what the TCB tripwire caught

Patrol (250 ms, `dashHgTcbScan`) tripped `TripStackBase` on task `httpd` (#17):
`expected=0x4ff75040 actual=0x4ff750c4`, tickMs=565,536 (≈47 s after blast done, 90 s after the
last HTTP request), two-scan confirmed then `abort()`.

### What it is NOT (each ruled out with evidence)

1. **Not task recreation.** `uxTCBNumber` still 17, name intact `httpd`, `pxEndOfStack`
   unchanged (0x4ff79030). A recreated task would carry a new monotonically-increasing task
   number (board had 20 tasks).
2. **Not our patrol writing TCBs.** `dashHgTcbScan`/eval is read-only vs TCBs (Adopt paths
   write only the baseline array).
3. **Not stack overflow.** TCB sits 12,792 B *below* the stack base; everything between TCB and
   stack (heap block headers, 0xABBA1234 canaries, 0xfe free fill) is intact. An overflow deep
   enough to reach TCB+0x30 would have wrecked all of it.
4. **Not the blast.** Blast 2 (same firmware, same command, no trip within 150 s) and the 35-min
   idle watch (zero trips) both clean. The trip correlates with the **HTTP poll-storm session**,
   not the CAN flood.
5. **Not heap corruption.** Heap walk clean; block header below the stack intact
   (canary + size 0x4000 = the live 16 KB stack block).

### What actually happened

- **The pxStack field of the httpd TCB was surgically rewritten in place** (single word,
  TCB+0x30, 0x4ff75040 → 0x4ff750c4) at ≈19:18:15 — board idle, blast over, patrol healthy all
  along. Same *phenomenon class* as the in-car round-7 signature: TCB fields touched surgically,
  everything adjacent intact. First time caught live on the bench.
- The 168-byte "blob" at the stack base `[0x4ff75040, 0x4ff750e8)` is **benign deepest-frame
  residue**: it contains the exact three-float fingerprint of the status-JSON fps expression
  (`fps*10.0f+0.5f` with fps=37.1: 0x42146666 / 0x3f000000 / 0x43b9c000 at matching offsets) —
  the `/status` JSON handler (mcp2515_dashboard.h:3228) ran on the httpd task and once pushed
  its Arduino-String call chain to within 168 B of the 16 KB stack floor. Not a write target.
- The new pxStack value 0x4ff750c4 points *into* that residue region — the writer's value is
  consistent with data derived from the stack area, not with zeros (differs from the in-car
  zeroing detail).
- **The 1 Hz mac-error loop** (`system_api: 0 mac type is incorrect`) = an external HTTP client
  polling `/system_status` (the only `esp_read_mac(ESP_MAC_WIFI_STA)` caller, dashboard
  handleSystemStatus). Blast 1: 59 requests at exactly 1000 ms (19:12:48–19:13:45) = a browser
  tab on the dashboard UI; blast 2: a single ping 14 s after done (first-of-boot: the
  temperature_sensor lazy-init print proves it). Idle watch: none.

### Where this leaves round 7

- The bench trip #1 does **not** reproduce the blast→crash chain; it independently re-demonstrates
  the in-car corruption class (surgical single-word TCB edit) occurring on a bench board under
  mundane HTTP load. Root writer still unidentified — below app layer, consistent with the
  register-file/pmu/interrupt-path verdict family.
- Tripwire design lesson (v4 backlog): a pxStack change with unchanged taskNumber/name is
  currently classified TripStackBase; that verdict fired correctly, but the confirm logic cannot
  distinguish "field rewritten in place" from "legit task lifecycle event" — it doesn't need to
  (both are worth tripping on), just noting the semantics.

## Why the T2CAN firmware survives the car and the P4 dies in 12–22 s (Jordan's question)

The same dashboard/dispatch/DND feature line, the same IDF 6.0.1, the same file — and the
T2CAN has run the real car for months without a single crash. The differences that survive
scrutiny:

| dimension | T2CAN (field, no crash) | P4-3CAN (field, crashes) |
|---|---|---|
| silicon | ESP32-S3 (Xtensa LX7) | ESP32-P4 (RISC-V) — IDF vectors.S carries a "interrupt taken with mie=0" errata note; soc_caps has ZCMP erratas. Espressif's own precedent for P4 interrupt-timing windows |
| primary bus (CanA, X197 13/14) | **native TWAI — same `TWAIDriver` class** | native TWAI |
| secondary bus (CanB, X197 9/10) | **MCP2515 over SPI — polled task-context reads, no per-frame driver ISR on the ESP32** | native TWAI controller #2 — full driver ISR stream |
| third bus | none | native TWAI controller #3 (CanC, listen-only — real PT traffic still drives its RX ISR) |
| WiFi | S3 built-in radio | esp_hosted/C6 over SDIO (its own host ISR + rpc/sdio task farm) |
| can_rt | core 0, prio 18, same constants (`APP_CAN_TASK_CORE=0` both builds) | same |

So in the car the interrupt budget is: T2CAN = **one** native-TWAI driver ISR stream; P4 =
**three** concurrent native-TWAI ISR streams + the SDIO host ISR. Everything above the ISR
(dispatch, discovered-ID table, handlers, recorder ring) is byte-for-byte the same code on both
boards — and rounds 2–7 already burned the app layer down (full session-content replays r2–r5:
zero crashes; register-file group zeroing saved faithfully in interrupt frames is not an
app-level write pattern).

Conclusion: the surviving discriminators are (a) **P4 silicon** (RISC-V interrupt path with
documented timing-window precedents) and (b) **native-TWAI ISR stream count under real load**
(1 vs 3, plus SDIO). The bench trip #1 above (TCB field rewritten under plain HTTP load, zero
TWAI involvement) leans toward (a): P4 corrupting context-adjacent state under *ordinary*
interrupt pressure.

This directly shapes the experiment ladder — with one physical jumper the EXISTING beta11
firmware runs it unchanged (no variant needed; `exp_b` already does NO_ACK + self-reception):

- **Harness**: jumper CanA H/L ↔ CanB H/L with two dupont wires (a real differential bus, two
  transceivers; optionally CanC joined for 3-lane). The receiving lane left in NORMAL mode even
  ACKs, making lane-0 TX bit-identical to a real bus.
- **L1 = `exp_b 800 300 1`** (jumper A↔B): lane 0 self-receives its 800 fps through the real
  bus — T2CAN-equivalent native-TWAI ISR load on P4 silicon. If this trips/corrupts, P4 chokes
  at loads the S3 shrugs off for months → silicon case, escalate to Espressif.
- **L2 = `exp_b 800 300 7`** (jumper A↔B↔C): three transmitters arbitration-sharing one wire;
  every controller's RX ISR sees the ~2400 fps aggregate (≈ sess_64, i.e. harsher per
  controller than the car's ~800). If only L2 fails, it's an interrupt-pressure threshold,
  still P4-specific.
- Alternative harness: one 120 Ω resistor across H/L per blasted bus (official single-node
  self-test shape) — works too, but the jumper exercises real differential receive on a second
  controller as well.

## Pulling the next coredump (recipe)

`esptool` @115200 + chip-id nudge (same as before), `read_flash 0x15E0000 0x100000`; strip the
12-byte flash header for the ELF. gdb: `file .pio/build/p4_3can/firmware.elf` then
`target core core.elf`.
