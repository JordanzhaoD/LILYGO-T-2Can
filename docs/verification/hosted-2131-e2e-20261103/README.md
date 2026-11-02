# Bench E2E — fixes A-D (d5b5d28), esp_hosted 2.13.1 — board 2

Date: 2026-11-03 (local night run, autonomous)
Board: 2 (field board), LAN 192.168.1.146, USB `/dev/cu.usbmodem5CF71553661`
Firmware: `d5b5d28` build `6.0.0-beta11-p4_3can-82697baff518-2026-11-02T18:34:36Z`
(stamp hash = last UI commit 82697ba; build timestamp = this build)
Artifact: `.pio/build/p4_3can/firmware.bin` sha1 `0e6683f81aa53788f07a966008c1450d558f1b48`

## Checklist

| # | Item | Result |
|---|------|--------|
| 1 | Push 5 commits (b574a7e..d5b5d28) | DONE |
| 2 | OTA flash via `/update` (raw body + X-File-Name) | 200 OK, 1,390,800 B, 10.8 s |
| 3 | Board return after OTA | UP ~15 s |
| 4 | Build marker = new build | PASS (stamp 18:34:36Z) |
| 5 | hosted 2.13.1: STA assoc + IP + RSSI | PASS |
| 6 | AP up, channel aligned, napt | PASS |
| 7 | gw_dns task active, zero timeouts | PASS |
| 8 | Fix D artifact name via /update_check | NOT OBSERVABLE (GitHub unreachable from bench LAN; release has no P4 asset yet) |
| 9 | 10-min soak (60 polls), resets ledger flat | PASS — 0 anomalies; rssi -30..-37; fwd_hook 161->79294 (data plane loaded); ledger += only the OTA [3,0], zero during soak |
| 10 | `heap_guard_test tcb` trip → abort → coredump | PASS — verdict=3 TripName, expected=0xd5c4c0cd (flipped), actual=0x70616568 "heap"; confirm-then-trip <=500ms; abort core 1 (patrol); dump saved 704KB @0x15E0000; clean reboot |
| 11 | Coredump decode: magic HGD2 + TCB fields | PASS — magic=0x32444748, reason=3, armed=6, tcbNameA/B=d5c4c0cd/"patr", taskNumber=0x13; serial print == dump struct byte-for-byte |

## Notes

- Pre-flash reset ledger baseline: `[[1,0]×6, [4,0], [4,0]]` (round-4 field crash remnants).
- OTA reboot adds one expected ledger entry; soak must add none.
- The bench trip test intentionally produces a `[4,0]` ledger entry and a
  test coredump at 0x15E0000 — a later field crash overwrites both; forensic
  hygiene noted for the next vehicle session.
- Fix C heal paths are passive on bench (no AP client joins → watch never
  arms); active validation requires an AP-side client (field/iPad).
- The trip fired on a board that had been up continuously since the OTA
  reboot (tickMs=0xc1c8b = 793 s): P4 native USB CDC does NOT reset on
  port open; the boot banner at the head of trip_serial.txt is a stale
  device-buffer fragment from tonight's power-on. The small I() stamps
  inside the panic block are the known coredump-writer timer re-base
  artifact; tickMs is the only true clock.
- Baseline[0] task = "heappatrol" (the patrol itself, dash_heap_guard.h:453)
  — the bench hook flips its own task's name baseline, matching native
  test test_tcb_name_rewrite_trips semantics.
- Post-trip recovery: STA re-associated to JordanAP (same IP 192.168.1.146),
  AP channel aligned, nat=on, DNS active, HTTP 200 — esp_hosted 2.13.1
  survived a full panic->reboot->re-sync cycle.
- Bench artifacts kept on disk, not committed: coredump_raw.bin (1MB flash
  image @0x15E0000), core.elf (ELF @offset 12). The board's coredump
  partition holds this TEST dump until the next crash overwrites it.
- Reset ledger after E2E (newest first): [[4,0]=bench trip, [3,0]=OTA,
  [1,0]x6, [4,0]=round-4 field remnant].
- Known flake: esptool read-flash failed twice at 921600/460800 (USB CDC
  noise, "Invalid head of packet (0xFF)"); succeeded at 115200 after a
  chip-id nudge — the documented workaround.
