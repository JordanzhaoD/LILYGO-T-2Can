# P4 CAN hot path on the new TWAI node API — Albert-P4 mechanism port

Date: 2026-11-04 · Status: approved by Jordan ("按照Albert-P4固件实现机制重做一版")
Predecessor analysis: `docs/verification/albert-p4-reference-comparison-20261104.md`
Baseline it builds on: bus_enable staged rollout (route A, `2026-11-03-p4-stability-baseline-design.md`) — unchanged.

## Goal

Rebuild the P4-3CAN CAN hot path following the reference firmware's
implementation mechanism, on our current stack. The reference (Albert-P4,
same Waveshare P4 board, same 3× SN65HVD230 @500k + esp_hosted SDIO) is
stable in-car; our legacy-driver build dies in 12–39 s there. The mechanism
items we port:

1. New TWAI node API (`twai_new_node_onchip`) instead of the legacy queue driver.
2. Whole CAN hot path IRAM/cache-safe: `CONFIG_TWAI_ISR_IN_IRAM=y`,
   `CONFIG_TWAI_IO_FUNC_IN_IRAM=y`, `CONFIG_TWAI_ISR_CACHE_SAFE=y`
   (the last selects `TWAI_OBJ_CACHE_SAFE`: driver objects land in internal RAM).
3. RX serviced in the ISR callback with a frame budget (queue-free), hot
   state in internal RAM.
4. `can_rt` on core 1 (reference runs its CAN task on core 1, away from
   hosted/lwIP on core 0).

Not ported (documented why): IDF 5.5.5 downgrade (would churn the espidf
shim, esp_hosted 2.13.1 port and recorder v2 for no additional mechanism
value — the new node API exists in our IDF 6.0.1) and Albert's private
FreeRTOS/SDIO/TWAI patches (content unknown; only their existence is
documented).

## What changes

### 1. `sdkconfig.p4_3can` (P4 vehicle env only)

Add the three flags above. Prereq verified: `SPI_FLASH_AUTO_SUSPEND` unset,
so `TWAI_ISR_CACHE_SAFE` is visible. T2CAN sdkconfig untouched.

### 2. New `include/drivers/p4_twai_node.h` — `P4TwaiNodeCore`

One embeddable core used by both driver classes, compiled only under
`DRIVER_P4_3CAN && !NATIVE_BUILD`:

- **init**: `twai_new_node_onchip` — io pins from ctor, `bit_timing =
  {500000}` (basic config), `tx_queue_depth = 16`, `intr_priority = 0`,
  flags `enable_listen_only` for CanC; then
  `twai_node_register_event_callbacks(.on_rx_done = coreRxDone, user_ctx = this)`,
  then `twai_node_enable()`.
- **ISR side** (`static IRAM_ATTR bool coreRxDone(node, edata, ctx)`):
  the driver's own ISR drains the hardware FIFO (`while
  twai_hal_get_rx_msg_count`) and invokes `on_rx_done` **once per frame**
  (verified in `esp_twai_onchip.c:187` `_node_isr_main`; overrun is also
  handled driver-side), so the callback does exactly one
  `twai_node_receive_from_isr` into a stack `twai_frame_t` (buffer = local
  `uint8_t[8]`), pushes it into the SPSC ring, on full ring increments
  `rxRingDrops` (no blocking call ever), then
  `vTaskNotifyGiveFromISR(consumerTask, &woken)`; returns woken.
- **Ring**: `kRingFrames = 128` slots × 16 B (`id`, `dlc`, `data[8]` +
  padding), `heap_caps_calloc(..., MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`
  at init — internal RAM, allocated only when the lane is constructed, so
  the bus_enable gating keeps its "disabled lane installs no ISR" property.
  SPSC discipline: head (ISR-only writes), tail (can_rt-only writes),
  `volatile` indices; overflow = drop + count, never block.
- **Task side**: `read(CanFrame&)` pops the ring (non-blocking, same
  contract as today's `twai_receive(…, 0)` path); `send()` builds a
  `twai_frame_t` and calls `twai_node_transmit(node, &f, 2)` (2 ms timeout,
  same policy as today).
- **TX staging pool (lifetime requirement)**: `_node_queue_tx` mounts
  immediately only when the hardware is idle (payload copied into the TX
  buffer registers synchronously); when busy it **queues the caller's
  pointer** (`esp_twai_onchip.c:567` `tx_mount_queue`), so the frame and its
  buffer must outlive the async completion. The core therefore transmits
  from its own pool — `kTxSlots = 18` slot structs (`twai_frame_t` +
  `uint8_t[8]`, internal RAM, allocated with the ring), tracks occupancy
  with a `__atomic` bitmask (task allocates, `on_tx_done` frees by pointer
  match), and reports pool exhaustion as a TX failure (feeds the existing
  `consecutiveTxFailures` guard).
- **Bus-off**: state from `twai_node_get_info` (`twai_error_state_t`);
  recovery via `twai_node_recover()` with the existing 1 s cooldown shape.
- **Mask filter**: `twai_node_config_mask_filter(node, 0, …)` requires the
  disabled state → apply = `twai_node_disable` → config → `twai_node_enable`.
  ⚠️ polarity inversion vs legacy: new-API mask bit **1 = must match**,
  legacy acceptance_mask bit 1 = don't-care. `setFilters` keeps computing
  `base`/`differ` and passes `mask = ~differ & 0x7FF` (std). Accept-all =
  `id = 0, mask = 0`. The software exact-ID re-check stays (parity with
  today: hardware mask filter + exact list).
- **Bench hooks** (`benchSetMode`/`benchSendSelf`): mapped onto node flags
  `enable_self_test` / `enable_loopback` + node re-init; kept because the
  round-9 jumper plan (CanA↔CanB wiring) needs a working self-flood TX path.

### 3. Driver classes keep their public surface

`TWAIDriver` (twai_driver.h) and `P4TwaiSecondaryDriver`
(p4_twai_secondary.h): under the P4 gate their private layer delegates to
`P4TwaiNodeCore`; mutex, exact-filter bookkeeping, `DashTwaiDiag`,
`canBenchInjectPop` hooks, MCP-facing accessors
(`setReceiveAllMode`/`recoveryCount`/`consecutiveTxFailures`/`lastError`)
and `statusInfo(twai_status_info_t&)` survive — `statusInfo` now
synthesizes the legacy struct from node info so the two `main.cpp`
consumers (:266 can3 status, :414 recorder-meta missed counter) stay
byte-identical. Field mapping:

| Legacy field | New source |
|---|---|
| `state` | `twai_node_status_t.state` mapped 1:1 (error-state enum → legacy state names) |
| `tx_error_counter` / `rx_error_counter` | same-named node status fields |
| `msgs_to_tx` | `tx_queue_remaining` |
| `rx_missed_count` | core `rxRingDrops` (frames lost to consumer keep-up — closest true meaning; legacy counted queue-full losses) |
| `tx_failed_count` / `arb_lost_count` | 0 (not exposed by the new API) |
| `bus_error_count` | `twai_node_record_t.bus_err_num` |
| `rx_overrun_count` (primary diag) | core `rxRingDrops` |

### 4. `can_rt` → core 1 (P4 only)

`main.cpp` `APP_CAN_TASK_CORE` becomes `#ifdef DRIVER_P4_3CAN → 1` else the
current default 0. Heap-guard v3 TCB sentinels adopt by task number, so the
move is invisible to them; the stale "can_rt lives on core 0" comments get
updated.

### 5. Not touched

bus_enable NVS gating and boot logs; recorder v2/blackbox; hosted 2.13.1
fixes; T2CAN_DUAL paths (compile invariance is a gate); native test build
(`NATIVE_BUILD` keeps compiling the existing legacy-shaped stubs);
dashboard/diag UI contracts (DIAG `bus_enable` etc.).

## Why this is expected to kill the crash family

The field fingerprints (rounds 1–10) are control-flow/register corruption
under in-car-only interrupt load. Every reproducible mechanism item that
differs between Albert-P4 and us is removed: after this change the TWAI ISR,
its call chain and the frame path are `noflash` (linker.lf-gated), driver
objects and the frame ring are internal-RAM, and the per-frame ISR work no
longer depends on cache-resident code or PSRAM. The theory stays
falsifiable: if a fully-IRAM build still crashes in-car, the cache/ISR line
is dead and investigation moves to hosted/silicon.

## Test plan (TDD, contract tests first)

New `test/test_p4_twai_node_contract.py` (source-contract style, mirrors
`test_bus_enable_contract.py`):

1. sdkconfig pins: the three flags =y in `sdkconfig.p4_3can`.
2. `p4_twai_node.h`: callback has `IRAM_ATTR`; ring and TX pool allocated
   with `MALLOC_CAP_INTERNAL`; ISR bodies contain no blocking calls
   (forbidden-name scan: `xSemaphoreTake`, `malloc`, `Serial`, `printf`,
   `vTaskDelay`); the TX pool free path goes through `on_tx_done` pointer
   matching.
3. P4-gated regions of both driver headers contain no legacy
   `twai_driver_install`/`twai_receive(`/`twai_transmit(`/`twai_get_status_info`
   calls; the non-P4 regions still do (T2CAN invariance).
4. `main.cpp`: `APP_CAN_TASK_CORE` resolves to 1 under `DRIVER_P4_3CAN`.
5. Mask-polarity contract: the P4 filter path passes `~differ & 0x7FF`, not
   legacy `differ << 21` semantics.
6. Existing bus_enable tests (18) still green — gating shape unchanged.

Gates (in order, serial): full pytest → `pio run -e p4_3can` →
`pio run -e lilygo_t2can_dual` → clang-format dry-run (no new violations) →
ui.h two-phase commit discipline (commit sources, rebuild, commit stamped
`mcp2515_dashboard_ui.h`).

## Rollout

1. Bench E2E on board 2 (LAN): boot banner, `bus_enable` DIAG unchanged,
   three-lane traffic via jumper if available, `/status` diag fields sane,
   recorder v2 lock/unlock + session download, 30 min soak.
2. Flash to the car only after Jordan's route-A baseline field result is
   in, with explicit approval — the two experiments are kept separable so
   field data stays attributable.

## Risks

- **Mask-polarity bug class** (1=match vs 1=don't-care): caught by contract
  test 5 + bench traffic compare against the known catalog.
- **IRAM budget**: noflash additions ≈ a few KB; P4 HP IRAM has ample
  headroom (verify in build map).
- **rx_missed semantics change** (ring drops vs driver queue losses):
  documented; recorder meta keeps a true consumer-keep-up metric instead.
- **can_rt on core 1**: new timing interactions are bench-soaked before
  the car sees it.
