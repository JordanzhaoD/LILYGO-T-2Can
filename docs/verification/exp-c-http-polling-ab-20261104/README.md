# Experiment C — pure HTTP polling A/B/C on a flood-free boot (2026-11-04)

Firmware: `firmware-bc1d106.bin` (same build as the bench E2E earlier today).
Board 2, bench, no CAN wiring change, zero `exp_b` on any arm's boot.

## Protocol

Three arms, each on its own fresh boot (HTTP `GET /reboot` separator, 90 s
blind settle), serial open once for the whole run, ~65 KB captured
(`p4_expC_serial.log`, `p4_expC_events.jsonl`, `p4_expC.py`).

| arm | stimulus | duration | mac-errors seen |
|---|---|---|---|
| A | silent (script makes zero HTTP) | 10 min | **129** at ~1 Hz, t=395.7→524.9 s |
| B | `GET /status` @ 1 Hz | 10 min | 0 |
| C | `GET /system_status` @ 1 Hz | 10 min | **533** at ~1.11 s cadence |

## Result: NO corruption in any arm

- Zero `abort()`, zero `[heap_guard] TCB TRIP`, zero Guru lines.
- `/ring_status` ledger after all arms: `[3,0]x3` (the three separator
  reboots) then the pre-existing entries — **no new `[4,x]`**.
- Arm C delivered 533 `/system_status` hits; every one executed
  `handleSystemStatus` → failing `esp_read_mac` (log line
  `system_api: 0 mac type is incorrect`) → no trip.
- Arm A was contaminated by an **external** ~1 Hz `/system_status` poller
  for 129 s (a dashboard page open on the LAN; the script made no HTTP in
  that arm) — still no trip.

Conclusion: **pure `/system_status` polling is inert on a boot that never
ran `exp_b`.** Round-8's "1 Hz /system_status poll storm" trigger is
therefore incomplete as a recipe.

## Re-reading today's two trips with this control

- **Panic #1** (`p4_flood_session.log`, first 4 lines, corrected reading —
  the earlier note mis-attributed panic #2's numbers to it):
  `t445803 esp_read_mac fail` → `t445835 temperature_sensor init` (both
  calls live inside `handleSystemStatus`, `mcp2515_dashboard.h:6331` and
  `:6357`) → `[heap_guard] TCB TRIP idx=8 verdict=4
  expected=0x4ff42de4 actual=0x4ff42e68` (**+0x84, third instance**).
  The write landed inside the handler's own execution window (detection
  lag ≤ one 250 ms patrol scan). Same boot had run `exp_b` ~7 min earlier.
- **Panic #2** (coredump tickMs=50156): last `/system_status` mac-error at
  t50020 with `tx queue full` spam still flowing (flood active) → trip
  ~136 ms later. Abort banner itself was lost to UART saturation; the
  coredump (idx=11, verdict=4) is the record.

Refined recipe across all three trips (round-8, today #1, today #2):
**`handleSystemStatus` executing on a boot primed by `exp_b` (active or
minutes earlier) → TCB+0x30 rewritten +0x84 within the handler window.**
Polling alone (662 clean hits today) does nothing; flood alone is
untested-as-controlled. The mac-error adjacency makes `esp_read_mac`'s
error path or the first-use `temperature_sensor` init the two in-handler
suspects; both run in every arm-C hit too, so the primer is what's
missing, not the handler code path.

Coredump limitation checked: the tripped httpd TCB (~0x4ff97xxx on the
panic-#2 boot) is not inside the captured regions (dump ends at
0x4ff8486c), so TCB-neighborhood forensics is not possible from this
dump.

## Next (proposed, needs approval — intentionally crashes the board)

- D1 minimal reproducer: fresh boot → `exp_b 200 10 1` → 10 s → ONE
  `/system_status` GET → expect trip within seconds.
- D2 handler bisect: same priming → ONE `/status` GET instead.
  D1-positive + D2-negative pins the writer inside `handleSystemStatus`.
