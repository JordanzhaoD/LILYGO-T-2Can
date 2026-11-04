# P4 CAN hot path — Albert-mechanism port build receipt (2026-11-04)

Jordan-approved redo ("按照Albert-P4固件实现机制重做一版"). Design:
`docs/superpowers/specs/2026-11-04-p4-twai-node-hotpath-design.md`.
Root-cause analysis that motivated it:
`docs/verification/albert-p4-reference-comparison-20261104.md`.

## Firmware

| Item | Value |
|---|---|
| File | `firmware-bc1d106.bin` |
| Commit | `bc1d106` (impl) + `f681a14` (ui build-id sync) |
| Embedded uiBuildId | `6.0.0-beta11-p4_3can-bc1d10615e26-2026-11-04T04:50:50Z` |
| sha256 | `93132e851bf2a2467289481c53f4f28ff4e3a88bc2231a9ba6a39d4175a89dd9` |
| Env | `p4_3can` (SDIO hosted golden vehicle config) |
| Mechanism | new TWAI node API, `TWAI_ISR_IN_IRAM`/`IO_FUNC_IN_IRAM`/`ISR_CACHE_SAFE=y`, internal-RAM RX ring + TX pool, can_rt core 1 |

## Map verification (the mechanism is real, not declared)

`ev_open_can_tools.map`: `_node_isr_main`, `_node_parse_rx`,
`_node_start_trans`, `twai_node_receive_from_isr` (0x4ff23938) and
`twai_node_transmit` (0x4ff23a1a) all resolve in internal RAM (0x4ff2xxxx)
— noflash sections per `esp_driver_twai/linker.lf`, matching the
`_ram_`-saturated Albert-P4 image. Generated config carries all four
macros incl. the selected `CONFIG_TWAI_OBJ_CACHE_SAFE`.

## Gates run (all green)

- pytest full suite: 418 passed + 754 subtests (incl. 20 new
  `test_p4_twai_node_contract.py` pins and the exp_b contract updated to
  the node-core mechanism).
- `pio run -e p4_3can` and `pio run -e lilygo_t2can_dual`
  (legacy-path invariance; T2CAN paths untouched).
- clang-format --dry-run --Werror clean on all touched files.

## Rollout state (deliberately NOT flashed)

Board 2 currently runs the route-A baseline (20efa75,
`bus_enable=0x01`) awaiting its ≥30 min field validation. Per the design,
this firmware flashes only AFTER that result and with Jordan's explicit
approval, so the two field experiments stay attributable:

- baseline survives → this build is the candidate for lighting CanB/CanC
  (`bus_enable 5/7` levels ride on the new hot path);
- baseline still crashes → this build becomes round-11's prime test
  (fully-IRAM path; a crash here kills the cache/ISR theory and moves
  suspicion to hosted/silicon).

## Bench E2E checklist (before any car run)

- [ ] OTA flash; boot shows the three `[BUS] bus_enable` lines (gating unchanged)
- [ ] DIAG `bus_enable` get/reject checks unchanged
- [ ] `/status` diag fields sane (state=Running, counters zero at rest)
- [ ] Lane traffic via CanA↔CanB jumper (round-9 plan): frames flow, ring
      counters advance, `rxRingDrops` stays 0 at bench rates
- [ ] exp_b self-flood (NO_ACK=self-test+loopback): `rx` advances (B1's
      rx=0 failure mode must not recur), zero panics
- [ ] recorder v2 lock/session download round-trip
- [ ] 30 min soak, `rv2_resets` ledger clean
