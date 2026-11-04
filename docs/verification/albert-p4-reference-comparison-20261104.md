# Reference-firmware comparison: why Albert-P4 is stable in-car and we crash

Date: 2026-11-04 · Read-only research, no code changed.
Ask: compare the stable in-car P4 3CAN reference firmware against ours and find
the crash cause. Reference material: `/Users/ziwind/Codex/DouyinFSD /esp_tsl-main/Albert/`
(firmware identity, decompiled task table, reverse-engineering workspace) and
`Albert-P4-P1.1.4/` / `P1.6.4-V3.2` images.

## Executive summary

The reference firmware ("Albert-P4", Waveshare P4 + WiFi6 esp_hosted SDIO +
3× SN65HVD230 @500 kbit — **the same board design and pin map as ours**) is
stable in the exact environment where our firmware dies in 12–39 s. It differs
from us on every layer of the CAN hot path **simultaneously**:

| Axis | Albert-P4 (stable) | Ours (crashes in-car) |
|---|---|---|
| IDF / framework | **ESP-IDF v5.5.5 + Arduino 3.3.7** | ESP-IDF 6.0.1 (espidf PIO) |
| TWAI API | new node API (`twai_new_node_onchip`) | legacy queue API (`twai_driver_install`) |
| TWAI ISR/IO in IRAM | **yes — link-time proof**: 106 `_ram_` symbols incl. `twai_node_receive_from_isr_ram_`, `twai_node_config_mask_filter_ram_` | **no** — `CONFIG_TWAI_ISR_IN_IRAM`, `_IO_FUNC_IN_IRAM`, `_ISR_CACHE_SAFE`, `_ISR_IN_IRAM_LEGACY` all unset in `sdkconfig.p4_3can`; `intr_flags = 0` from `TWAI_GENERAL_CONFIG_DEFAULT` (`include/drivers/twai_driver.h:36`, `include/drivers/p4_twai_secondary.h:322`) |
| CAN hot state | **internal RAM** (101,792 B tracked) | driver objects default-caps; recorder v2 ring in PSRAM (HEX @80 M) |
| Per-frame RX | serviced **inside the ISR callback** with frame-count/time budgets, queue-free | legacy RX queue + `twai_receive` polling in `can_rt` |
| Task placement | `fsd_p0` prio 24 core1, `can_rt` prio 20 core1 (8 KB) | `can_rt` (+TWAI ISRs) on core0 alongside hosted/lwIP |
| OTA | quiesces CAN before flashing | (orthogonal) |
| Extras | **custom FreeRTOS / SDIO / TWAI patches** ("打补丁" per their README) | stock IDF 6.0.1 |

## The mechanism class this points at

IDF's own Kconfig help states the hazard verbatim
(`components/esp_driver_twai/Kconfig`, verified in our local IDF 6.0.1):

> `TWAI_ISR_CACHE_SAFE` — Allow TWAI works **under Cache disabled (such as when
> writing to SPI Flash)**.

Chain of facts:

1. Our three in-car CAN controllers install the legacy driver with
   `intr_flags = 0`, so the ISR is allocated **without** `ESP_INTR_FLAG_IRAM`
   and lives in **flash cache**, as does the whole legacy driver + HAL object
   (`TWAI_MALLOC_CAPS = MALLOC_CAP_DEFAULT` with `TWAI_OBJ_CACHE_SAFE` unset).
2. Flash/cache-disabled windows exist at runtime: every `nvs_commit`
   (`src/espidf_runtime.cpp:421–495`, gateway persistence
   `include/web/dash_gateway.h:1223`, boot ledger writes), flash erases from the
   blackbox engine, and cache maintenance on the PSRAM/HEX path.
3. IDF's designed protection during flash ops is to **mask non-IRAM interrupts**
   (`esp_intr_noniram_disable`, `esp_hw_support/intr_alloc.c:960`) — a
   software, per-CPU, IPC-coordinated dance. Any hole or race in that dance on
   the P4/RISC-V port (Albert found enough of them to patch FreeRTOS/SDIO/TWAI
   themselves) means a TWAI RX ISR is fetched/executed while the cache is off →
   garbage execution.
4. That failure signature is exactly our rounds 1–10 fingerprint family:
   jump-to-0, jump mid-instruction, register-file groups zeroed, `ra` poisoned
   to constant odd values — *non-linear control-flow corruption below the
   application layer*.

Why each environment behaves as observed:

- **Bench never crashes**: buses are silent → no RX completions → the TWAI RX
  ISR path is never exercised at rate (round 8/9 established the bench cannot
  drive the real ISR chain — no silicon loopback). Round-8's single bench
  corruption (TCB trip #1, zero CAN) shows the corruption vector exists even
  without TWAI, but at bench probability; in-car it is deterministic.
- **In-car always crashes, delay ∝ load**: 3 buses ≈ thousands of RX ISR/s
  (CanA rxOverrun ~200/s alone) × flash/cache windows ⇒ near-certain hit
  within seconds. L2 (blackbox, more flash+PSRAM traffic) crashed 10× faster —
  the round-10 "delay scales with ISR+PSRAM/cache load" law.
- **T2CAN months-stable**: 1×TWAI at ~500 fps (≈1/6 our ISR rate), the second
  bus is SPI-polled MCP2515 (no per-frame ISR), no esp_hosted, no PSRAM ring,
  and an Arduino-core IDF vintage — i.e. the same latent defect at a hazard
  rate orders of magnitude lower.
- **Albert stable**: the entire CAN hot path is IRAM-resident and
  internal-RAM-backed, and they run a patched 5.5.5 — both ends of the hazard
  (cache windows × flash-resident ISR) are neutralized.

## Proposed fix (config-level, P4 build only — **pending Jordan's approval**)

Mirror the reference firmware's IRAM posture with the legacy driver's
supported path (contract at `components/driver/twai/twai.c:493` — passing the
IRAM flag is *rejected* unless the Kconfig is on):

1. `sdkconfig.p4_3can`: set `CONFIG_TWAI_ISR_IN_IRAM_LEGACY=y`
   (compiles legacy ISR + HAL `noflash`, per `components/esp_driver_twai/linker.lf`).
2. In the three `TWAI_GENERAL_CONFIG_DEFAULT` blocks (main bus
   `include/drivers/twai_driver.h`, secondary/third `include/drivers/p4_twai_secondary.h`),
   set `g_config_.intr_flags = ESP_INTR_FLAG_IRAM` under `#ifdef DRIVER_P4_3CAN`
   so the interrupt is allocated IRAM-safe. T2CAN build path unchanged.
3. Cost: a few KB of IRAM (ISR + HAL frame functions), no behavior change on
   silent buses; rejection check stays satisfied because the Kconfig is on.

Predictive power (falsifiable tomorrow):
- If the Route A CanA-only baseline (already on board 2) survives or crashes
  markedly later, the ISR-storm theory gains weight and the IRAM fix is the
  enabler for lighting CanB/CanC back on.
- If CanA-only still dies at 12–39 s, TWAI-IRAM loses primacy to the
  hosted/platform line (decision tree in FIELD-GUIDE §七 already encodes this).
- The IRAM fix firmware, once approved and flashed, is the direct experiment:
  stable in-car ⇒ mechanism confirmed.

Not adopted now (kept as fallbacks): downgrade to IDF 5.5.5/Arduino 3.3.7 to
match Albert exactly; moving `can_rt` to core1; pinning per-frame allocations
to internal RAM. One variable at a time.

## Evidence trail

- Reference identity/tasks/RAM: `esp_tsl-main/Albert/reverse/README.md`,
  `analysis/board_profile.json`, `skills/albert-p4-can-analysis/references/can-runtime.md`.
- `_ram_` symbol saturation incl. `twai_node_receive_from_isr_ram_`:
  Albert image symbol tables (106 refs).
- Our config gaps: `sdkconfig.p4_3can` (TWAI IRAM/cache-safe flags all off;
  `SPIRAM_MODE_HEX=y`; `FLASHMODE="dio"` matches Albert — flash mode excluded).
- Legacy driver IRAM contract: `components/driver/twai/Kconfig.twai`,
  `twai.c:420` (`g_config->intr_flags | ESP_INTR_FLAG_INTRDISABLED`),
  `twai.c:493` (flag rejected unless `TWAI_ISR_IN_IRAM[_LEGACY]`).
- Linker proof of IRAM placement: `components/esp_driver_twai/linker.lf`
  (`TWAI_ISR_IN_IRAM=y` → ISR/HAL entries `noflash`; the `_ram_` variants).
- Masking mechanism: `components/esp_hw_support/intr_alloc.c:960`
  (`esp_intr_noniram_disable`), cross-core via the spi_flash OS layer.
