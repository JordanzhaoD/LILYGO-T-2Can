# bus_enable staged-rollout baseline — build receipt (2026-11-03)

Route A deliverable: the first car-usable P4 stability baseline. Design:
`docs/superpowers/specs/2026-11-03-p4-stability-baseline-design.md`.

## Firmware

| Item | Value |
|---|---|
| File | `firmware-20efa75.bin` |
| Commit | `20efa75` (impl) + `2f64dad` (ui build-id sync) |
| Embedded uiBuildId | `6.0.0-beta11-p4_3can-20efa758bcfe-2026-11-03T15:44:05Z` |
| sha256 | `d149c11ccfe29b7d2bed0aa5f6657f3b87032e86f4db201125db6281cd1c3d32` |
| Env | `p4_3can` (SDIO hosted golden vehicle config) |
| Boot default | `busmask=0x01` → CanA only; CanB/CanC never constructed |

## Gates run (all green)

- pytest full suite: 398 passed + 754 subtests (incl. 18 new
  `test_bus_enable_contract.py` pins).
- `pio run -e p4_3can` and `pio run -e lilygo_t2can_dual` (shared-code
  invariance; T2CAN_DUAL path untouched).
- clang-format dry-run: no new violations (dashboard pre-existing drift
  unchanged).
- Note: `pio run -e p4_3can_spark` fails to link on esp_hosted SDIO symbols —
  pre-existing: that SPI bring-up env never had the SDIO transport; unrelated
  to this change (verified the missing symbols are `#if H_TRANSPORT_IN_USE ==
  H_TRANSPORT_SDIO`-gated in the component).

## Field validation plan (this firmware)

1. Flash via OTA (bare-body POST /update + X-File-Name).
2. Boot log must show: `[BUS] bus_enable=0x1 (bit1=CanB bit2=CanC)` and both
   `[BUS] CAN B (TWAI1) disabled by bus_enable` /
   `[BUS] CAN C (TWAI2) disabled by bus_enable`.
3. DIAG `bus_enable` (no arg) prints `bus_enable=0x1 ...`; `bus_enable 0` and
   `bus_enable 9` are rejected (`value must be 1..7`).
4. Success = ≥30 min drive, zero crash, `rv2_resets` ledger clean, CanA
   driving features + CanA-lane recording working.
5. Level-up ladder (one car run each, only after a clean baseline run):
   `bus_enable 5` (+CanC) → `bus_enable 7` (full tri-bus).

## Verification record

- [ ] Boot log lines captured (serial)
- [ ] DIAG get/reject checks
- [ ] ≥30 min drive, zero crash, resets ledger clean
- [ ] Level 0x05 car run
- [ ] Level 0x07 car run
