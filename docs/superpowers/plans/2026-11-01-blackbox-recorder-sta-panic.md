# Blackbox Recorder + STA Panic Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Kill the in-car PANIC (esp_hosted NULL assert via `WiFi.status()`) and replace the SPIFFS flush recorder with a raw-partition blackbox (lock = sub-second, zero copy, protected sessions).

**Architecture:** A adds an event-maintained STA-association flag gating all 7 `esp_wifi_sta_get_ap_info` call sites through one safe helper. B2 swaps the PSRAM-ring+SPIFFS-flush engine inside `dash_recorder_v2.h` for a streaming writer into a dedicated 9MB `blackbox` flash partition: global 18B frame grid anchored at a 32B header, 2×64KB PSRAM staging double-buffer, erase-ahead writer task, per-block RAM summaries, block-level protection bitmap rebuilt from SPIFFS metas at boot. UI/API surface unchanged (idle/arming states only; "flushing" disappears).

**Tech Stack:** ESP-IDF 6.0.1 (espidf framework), PlatformIO env `p4_3can`, `esp_partition` API, Arduino-compat shim (`src/espidf_runtime.cpp`), pytest source-contract tests, host Python decoder.

**Spec:** `docs/superpowers/specs/2026-11-01-blackbox-recorder-and-sta-panic-fix-design.md`

## Global Constraints

- Gate order: pytest first, then `~/.platformio/penv/bin/pio run -e p4_3can` (and t2can env for isolation); never run native tests and builds in parallel.
- pytest = `/opt/homebrew/bin/python3.13 -m pytest test/ -q`.
- All recorder code stays macro-gated by `DASH_RECORDER_V2` (defined only in `[env:p4_3can]`, platformio.ini:277 area); p4 env never enters `default_envs` or CI.
- T2CAN zero behavior change: the STA flag check is compiled only under `DRIVER_P4_3CAN`; verify 0 new recorder symbols in a t2can build (Task 7).
- Commit messages / CHANGELOG / release notes: English only. `.claude/scratch/` never committed. Credentials never in repo.
- Never `pio run -t upload`. esptool entry = `~/.platformio/penv/bin/esptool`. Flash chain for this feature = table@0x8000 + otadata erase + app@0x20000 with read-backs (Task 7).
- No UI blob changes (`include/web/mcp2515_dashboard_ui.h` must stay untouched; if a build dirties it, `git checkout --` it). No new i18n labels.
- This is a git worktree: no bare `git stash`; list files explicitly in `git add`.

## Review Focus

1. **STA drops mid-download**: a session download streaming while STA drops must complete — reader uses only `esp_partition_read`, no lock shared with the writer. → Task 5 test `test_download_holds_no_writer_state`
2. **Lock while full**: when no unprotected block is free, `/ring_lock` returns 400-family error "no space", never silently overwrites. → Task 4 test `test_lock_refused_when_full`
3. **Partition absent** (board still on old table): recorder reports disabled, boot and APIs stay alive. → Task 2 test `test_absent_partition_disables_engine`
4. **Frame straddling a 64KB block boundary**: addressing is `32 + 18*k` across the whole partition, independent of block edges. → Task 2 test `test_grid_addressing_ignores_block_edges` + Task 6 decoder behavioral test `test_decode_frame_straddling_block_boundary`
5. **Reboot with protected sessions + stale NVS write pointer**: bitmap rebuild marks exactly the meta ranges; resume erases-ahead from the saved pointer. → Task 4 test `test_rebuild_marks_meta_ranges_only` + Task 7 bench reboot step.

---

### Task 1: STA association gate (panic fix, independent)

**Files:**
- Modify: `src/espidf_runtime.cpp` (flag near line 30; handler cases 111-124; sites 834/856/962; helper definition)
- Modify: `include/platform/espidf_runtime.h` (helper declaration, adjacent to `SPIFFSClass`)
- Modify: `include/web/dash_gateway.h:1444`, `include/web/mcp2515_dashboard.h:5512,6177,6554` (call sites)
- Modify: `include/web/mcp2515_dashboard.h` DIAG block (~6518 help + ~6767 dispatch): add bench-only `sta_drop`
- Test: `test/test_sta_association_guard.py` (new)

**Interfaces:**
- Produces: `bool espWifiStaApInfoSafe(wifi_ap_record_t *ap);` — true = `*ap` filled; false = not associated or API error. Declaration in `include/platform/espidf_runtime.h`; definition in `src/espidf_runtime.cpp`. Internally: `#if defined(DRIVER_P4_3CAN)` consults `static volatile bool wifiStaAssociated`; `#else` passes through to `esp_wifi_sta_get_ap_info`.
- Produces (bench): DIAG `sta_drop` → `WiFi.disconnect(false, false)` + print (A-fix repro trigger).

- [ ] **Step 1: Write failing tests** — `test/test_sta_association_guard.py`, house style (read source, assert substrings):
  - `test_flag_exists_and_starts_false`: `static volatile bool wifiStaAssociated = false;` present in espidf_runtime.cpp
  - `test_handler_sets_flag_on_both_events`: `WIFI_EVENT_STA_CONNECTED` branch contains `wifiStaAssociated = true;`; `WIFI_EVENT_STA_DISCONNECTED` branch contains `wifiStaAssociated = false;`
  - `test_helper_guards_under_p4_macro`: helper body contains `#if defined(DRIVER_P4_3CAN)` and `if (!wifiStaAssociated)` before calling the real API
  - `test_all_seven_sites_routed`: in the 5 files, exactly 1 remaining `esp_wifi_sta_get_ap_info(` outside the helper definition is the helper's own call; the 7 known sites now call `espWifiStaApInfoSafe`
  - `test_helper_declared_in_platform_header`: declaration present in `include/platform/espidf_runtime.h`
  - `test_sta_drop_diag_exists`: help line + dispatch branch for `sta_drop` in mcp2515_dashboard.h
- [ ] **Step 2: Run tests, verify FAIL** — `/opt/homebrew/bin/python3.13 -m pytest test/test_sta_association_guard.py -q` → errors (missing symbols)
- [ ] **Step 3: Implement** — flag + handler wiring; helper in espidf_runtime.cpp + decl in platform header; replace sites 834 (`begin`), 856 (`status`), 962 (`SSID`), dash_gateway.h:1444, dashboard 5512/6177/6554 (add `#include "platform/espidf_runtime.h"` to dash_gateway.h only if not already visible); add `sta_drop` DIAG (help line + dispatch, next to `rv2_status`).
- [ ] **Step 4: Run tests, verify PASS** — same command → all green
- [ ] **Step 5: Full gate + commit** — `/opt/homebrew/bin/python3.13 -m pytest test/ -q` (no regressions) then `~/.platformio/penv/bin/pio run -e p4_3can` green → `git add src/espidf_runtime.cpp include/platform/espidf_runtime.h include/web/dash_gateway.h include/web/mcp2515_dashboard.h test/test_sta_association_guard.py` → commit `fix(p4): gate STA AP-info RPCs on association state (esp_hosted NULL-assert panic)`

### Task 2: Partition table + blackbox skeleton (grid, header, lookup)

**Files:**
- Modify: `partitions_32mb_ota_8192k_nvs64.csv` (single line change + spiffs shrink)
- Modify: `include/web/dash_recorder_v2.h` (new partition block replacing ring storage decls; ring vars die in Task 6 — here we ADD the new block side-by-side)
- Test: `test/test_blackbox_recorder.py` (new)

**Interfaces:**
- Produces constants in `dash_recorder_v2.h`: `kRecBBPartLabel = "blackbox"`, `kRecBBHeaderSize = 32u`, `kRecBBBlockSize = 65536u`, `kRecBBBlockCount = 144u` (partition offset/size come from the found `esp_partition_t`, not constants)
- Produces: `struct RecBBHeader { char magic[4]; uint16_t version; uint16_t headerSize; uint8_t reserved[24]; }` + `static_assert(sizeof(RecBBHeader) == 32)`; magic `"BBX1"`, version 1
- Produces: `static const esp_partition_t *recBBPart = nullptr;` and `static bool recBBInitPartition();` — `esp_partition_find_first(ESP_PARTITION_TYPE_DATA, (esp_partition_subtype_t)0x40, kRecBBPartLabel)`; on first boot (magic mismatch at offset 0) erase block 0 and write the header; sets `recBBWriteOff = kRecBBHeaderSize` (or NVS pointer, Task 3)
- Produces grid helpers: `static uint32_t recBBFrameOff(uint32_t idx) { return kRecBBHeaderSize + idx * sizeof(RecRingFrame); }`

- [ ] **Step 1: Write failing tests** in `test/test_blackbox_recorder.py`:
  - `test_partition_table_rows`: CSV contains `blackbox,      data, 0x40,   0x16E0000,0x900000,`; spiffs row now `0x1020000,0x6C0000`; coredump row unchanged at `0x1FE0000,0x20000`; offsets are contiguous (0x1020000+0x6C0000==0x16E0000, 0x16E0000+0x900000==0x1FE0000) — assert with int math
  - `test_header_struct_frozen`: `static_assert(sizeof(RecBBHeader) == 32` and magic/version constants
  - `test_grid_addressing_ignores_block_edges`: compute in Python — for k in {3640, 3641, 3642} (block-0/1 boundary: (65536-32)/18 = 3638.9), frame offsets span the boundary; assert the C++ source defines addressing as `kRecBBHeaderSize + idx * sizeof(RecRingFrame)` (substring) — continuity is grid-anchored, not block-anchored
  - `test_block_constants`: 144 blocks × 65536 == 0x900000 (assert in Python from the constants found in source)
  - `test_absent_partition_disables_engine`: `recBBInitPartition` sets a `recBBEnabled=false`-style path when `esp_partition_find_first` returns nullptr; `dashRecV2LockRequest` still refuses with "recorder v2 disabled"; `dashRecV2Init` prints `[REC2] blackbox partition missing — recorder disabled`
- [ ] **Step 2: Run → FAIL**
- [ ] **Step 3: Implement** — CSV edit; engine block in dash_recorder_v2.h (constants, header struct, lookup+first-boot header write); wire `recBBInitPartition()` into `dashRecV2Init()` before the ledger boot call; keep ring code untouched this task (dead but compiling — `#if 0` NOT used; it still compiles until Task 6 removes it).
- [ ] **Step 4: Run → PASS**; plus `/opt/homebrew/bin/python3.13 -m pytest test/test_dash_recorder_v2.py -q` still green (old pins untouched)
- [ ] **Step 5: Build gate** — `pio run -e p4_3can` green
- [ ] **Step 6: Commit** — `feat(p4): blackbox partition table and frame-grid skeleton`

### Task 3: Streaming writer (staging, erase-ahead, summaries, stop-when-full)

**Files:**
- Modify: `include/web/dash_recorder_v2.h` (writer core; `dashRecV2Submit` rewired to staging)
- Test: `test/test_blackbox_recorder.py` (extend)

**Interfaces:**
- Produces: `struct RecBBBlockSum { uint32_t firstTsUs, lastTsUs, cntA, cntB, cntC, cntTx; }` + `static_assert(sizeof(RecBBBlockSum) == 24)`; `static RecBBBlockSum recBBSums[kRecBBBlockCount];` (RAM-only, zeroed at boot)
- Produces: staging — `static uint8_t *recBBStage[2];` (each `kRecBBBlockSize`, PSRAM via `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`, fallback disabled), `static volatile uint32_t recBBStageLen[2];`, `static volatile uint8_t recBBStageFillIdx;`; producer = `dashRecV2Submit` under `recV2Mux` (append 18B frame + bump per-bus count in `recBBSums[current]`); consumer = writer task
- Produces: `static void dashRecV2WriterTask(void *);` — task `recbb_write`, stack 6144, prio 1, core 1; waits on semaphore (given by producer when a buffer fills) with 250ms timeout; flushes full buffer via `esp_partition_write`, then erase-ahead: when write pointer crosses into block N+1, close `recBBSums[N]` (freeze; open N+1), erase N+1 **iff** its protection bit is clear and it is not already the erased-ahead block; skip protected blocks (advance until unprotected); if no unprotected block ahead → set `static volatile bool recBBPaused = true;` (staging keeps absorbing until full → then frames drop + `ringOverflow` counter increments — badge semantics preserved)
- Produces: NVS pointer — key `"rv2_wptr"` (u32 byte offset), written every 8 closed blocks and on lock; read at init (after header check) as resume point
- Consumes: `dashRecV2Submit(const CanFrame &f, uint8_t busTag, char dir)` signature unchanged; fps accumulators unchanged

- [ ] **Step 1: Failing tests** (extend the blackbox test file):
  - `test_submit_paths_through_staging`: `dashRecV2Submit` body contains the staging append under `portENTER_CRITICAL(&recV2Mux)` and no `ring[` access
  - `test_writer_task_pinned_core1_low_prio`: `xTaskCreatePinnedToCore(dashRecV2WriterTask` with `"recbb_write"` and `6144`
  - `test_erase_ahead_skips_protected_and_stops`: writer body contains the protection skip (`recBBProt` bit test) and the pause set (`recBBPaused = true`) when no unprotected block remains
  - `test_block_summaries_ram_only`: `RecBBBlockSum recBBSums` array declared; no NVS/flash write of summaries (assert absence of `putBytes("rv2_sum` etc.)
  - `test_wptr_nvs_cadence`: `rv2_wptr` written on 8-block boundary and at lock (two call sites)
  - `test_pacing_and_yield`: writer flushes whole staging buffer in one `esp_partition_write` per buffer (no per-frame flash calls; assert loop structure by substring `esp_partition_write(recBBPart`)
- [ ] **Step 2: Run → FAIL**
- [ ] **Step 3: Implement** per interfaces. Erase-ahead initial state at boot: erase the block containing the resume pointer (covers stale-pointer re-erase safety), then proceed.
- [ ] **Step 4: Run → PASS** + full pytest suite green
- [ ] **Step 5: Build + commit** — `feat(p4): streaming blackbox writer with erase-ahead and block summaries`

### Task 4: Lock, meta, protection bitmap, session lifecycle

**Files:**
- Modify: `include/web/dash_recorder_v2.h` (lock state machine, meta build, rebuild, remove)
- Test: `test/test_blackbox_recorder.py` (extend); `test/test_dash_recorder_v2.py` (update stale flush-era pins)

**Interfaces:**
- Produces: `static uint8_t recBBProt[kRecBBBlockCount / 8];` + `static void recBBMarkRange(uint32_t startOff, uint32_t endOff);` (sets bits for blocks covering `[startOff,endOff)`, wrap-safe)
- Produces: `static void recBBRebuildProtection();` — walks `recV2ScanSessions`, reads each meta's `bb_start_off`/`bb_end_off`, marks bits
- Produces: `static void recBBFinishLock();` — called from `dashRecV2Loop` when `millis() - recV2LockStartMs >= recV2PostMs` while arming: computes `tCutUs = lastTs - preMs*1000` clamped to oldest summary `firstTsUs`; window counts = sum over touched summaries (incl. current open block running counts); `start_boot_us = tCutUs`, `end_boot_us = last submit ts`; marks bitmap; builds meta (existing schema + new `"bb_start_off"`,`"bb_end_off"` block-rounded byte offsets); writes meta file; `recV2PreMsAct = preMs actually honored`; seq via `recV2AllocateSeq()`; state → idle
- Meta keys preserved: `seq, boot_ms, trigger, wall?, pre_ms_req, pre_ms_act, post_ms, bus_mask, frames{a,b,c,tx}, ring_overflow, ring_overflow_in_window, twai_missed_a/b/c, start_boot_us, end_boot_us` + new `bb_start_off, bb_end_off`
- `recV2RemoveSession(seq)` → clear bitmap range from meta + delete meta file (no bin to delete)
- Lock refusals: `err = "no space"` when `recBBPaused`; keep "recorder busy"/"bus_mask" validations
- Ledger: `rv2_flushing` NVS flag now set at arming start, cleared in `recBBFinishLock` after meta write (diedMidFlush=1 now means died during arming; JSON shape `resets` unchanged)

- [ ] **Step 1: Failing tests**:
  - `test_lock_refused_when_full`
  - `test_finish_lock_summaries_and_meta`: meta builder emits `bb_start_off`/`bb_end_off` and all preserved keys (assert each key substring)
  - `test_rebuild_marks_meta_ranges_only`: rebuild reads `bb_start_off`/`bb_end_off` per meta; bits outside ranges stay clear
  - `test_delete_clears_protection`: remove calls the unmark path then meta delete
  - `test_ledger_flag_brackets_arming`: `rv2_flushing` set in `dashRecV2LockRequest`, cleared in `recBBFinishLock`
  - Update `test/test_dash_recorder_v2.py`: delete `DashRecorderV2LockContractTests.test_flush_failure_cleans_partial_file`, `test_shrink_keeps_recent`, `test_wraparound_window_extraction` (flush-era); update `test_meta_field_set_complete` to the new key list
- [ ] **Step 2: Run → FAIL**
- [ ] **Step 3: Implement**; `dashRecV2Loop` drives arming countdown (existing pattern) then calls `recBBFinishLock`
- [ ] **Step 4: Run → PASS** (both recorder test files) + full suite
- [ ] **Step 5: Build + commit** — `feat(p4): zero-copy lock with block protection and meta sessions`

### Task 5: Downloads + status semantics

**Files:**
- Modify: `include/web/mcp2515_dashboard.h:8163-8330` (handleSessionDownload; handleSessions list source), `dash_recorder_v2.h` (`dashRecV2StatusJson`)
- Test: `test/test_blackbox_recorder.py` (extend); `test/test_dash_recorder_v2.py` (download/CSV pins update)

**Interfaces:**
- Produces: `handleSessionDownload` bin mode — streams from `bb_start_off` to `bb_end_off + kRecBBBlockSize` (window may straddle), 4KB chunks via `esp_partition_read`, per-frame filter `tsUs >= start_boot_us && tsUs <= end_boot_us && (busBit & bus_mask)`, `Content-Length` = meta frames total × 18; `?fmt=csv` — same loop, emits the existing exact CSV header then rows (same columns as today: `ts_ms,dir,bus,id,dlc,b0..b7` style — keep byte-identical header string)
- Produces: `dashRecV2StatusJson` — states `idle`/`arming` only (drop `flushing` branch); adds `"paused":true|false`; `sessions_space_free` = clear-protection bits × `kRecBBBlockSize`; `span_s` from oldest live summary firstTs to now; `seq`, `resets`, `fps`, `enabled` unchanged
- Produces: `static bool recBBReadFrame(uint32_t off, RecRingFrame &out);` — the single read+filter primitive both endpoints share

- [ ] **Step 1: Failing tests**:
  - `test_download_streams_partition_not_spiffs`: handler uses `esp_partition_read` (substring) and no `SPIFFS.open` for bin mode; chunk loop 4096
  - `test_download_filter_anchored_to_meta_window`: filter condition includes `start_boot_us` and `end_boot_us` and bus mask; alignment guard `!= 0` on `size % sizeof(RecRingFrame)` retained
  - `test_download_holds_no_writer_state`: download path takes no mutex shared with `dashRecV2Submit`/writer (assert absence of `recV2Mux` in the download handler slice)
  - `test_csv_header_unchanged`: header string identical to pre-change (copy the pinned string from the existing test)
  - `test_status_json_new_semantics`: `"paused"` key present; no `flushing` string in the state-name builder; `sessions_space_free` computed from protection bits
  - Update stale pins in `test_dash_recorder_v2.py` (`test_status_json_shape`, `test_download_validates_frame_alignment` → point at new handler slice)
- [ ] **Step 2: Run → FAIL** → **Step 3: Implement** → **Step 4: Run → PASS** + full suite
- [ ] **Step 5: Build + commit** — `feat(p4): streaming session download and post-flush status semantics`

### Task 6: Dead code removal, decoder tool, version bump

**Files:**
- Modify: `include/web/dash_recorder_v2.h` (delete ring/flush/reclaim/reserve/sweep/orphan machinery + `kRecV2SpaceReserve` + `recV2FlushTask` + `recV2CountWindow` + `recV2SweepOrphans` + `recV2FileSize` bin usage)
- Modify: `src/espidf_runtime.cpp` (nothing expected; verify)
- Create: `scripts/decode_blackbox.py`
- Modify: `VERSION` (→ `6.0.0-beta11`), `CHANGELOG.md` (English entry), `test/test_dashboard_api_contract.py:2298` (version pin)
- Test: `test/test_blackbox_recorder.py` + `test/test_decode_blackbox.py` (new)

**Interfaces:**
- Produces: `scripts/decode_blackbox.py` — CLI `decode_blackbox.py IN.bin [-o OUT.csv]`; rejects input whose length % 18 != 0; emits CSV rows `ts_ms,dir,bus,id,dlc,b0..b7` byte-compatible with board CSV (dir from flags bits 2-3: 0=R,1=T,2=E; bus from flags bits 0-1: 1=A,2=B,3=C)

- [ ] **Step 1: Failing tests**:
  - `test_no_dead_engine_symbols`: source contains none of `recV2FlushTask`, `kRecV2SpaceReserve`, `recV2ReclaimSpace`, `recV2SweepOrphans`, `recV2CountWindow`, `ringHead`
  - decoder tests (behavioral, pure Python): `test_roundtrip_synthetic_frames` (build N frames incl. one straddling a synthetic 64KB boundary — just decode a stream, boundaries irrelevant to correctness, but assert a frame whose bytes span two 4096-chunk reads decodes — feed via a chunked reader), `test_rejects_misaligned_input`, `test_csv_columns_exact`
- [ ] **Step 2: Run → FAIL** → **Step 3: Implement** (deletions; decoder script; version/CHANGELOG/test pin) → **Step 4: Run → PASS** full suite
- [ ] **Step 5: Build + commit** — `feat(p4): remove flush engine, add blackbox decoder, bump 6.0.0-beta11`

### Task 7: Flash + bench verification (on-hardware)

**Files:**
- Modify: `docs/verification/p4-3can-bench-2026-11-01.md` (append section E: blackbox + panic-fix bench record); evidence artifacts under `docs/verification/` per house convention

- [ ] **Step 1: Build gates** — full pytest; `pio run -e p4_3can`; `pio run -e lilygo_t2can_dual` (or the repo's canonical t2can env) + `nm`/`strings` on its ELF asserting 0 occurrences of `recBB`/`rec2_flush` symbols
- [ ] **Step 2: Pre-flash read-back** — `esptool ... read_flash 0x8000 0x1000 /tmp/old_table.bin` (keep); confirm old table matches current committed CSV's previous version
- [ ] **Step 3: Flash 3-piece chain** — `esptool --port /dev/cu.usbmodem5CF71553661 --baud 921600 --chip esp32-p4 write_flash 0x8000 partitions.bin 0x20000 firmware.bin` + `esptool ... erase_region 0x19000 0x2000` (otadata); re-read 0x8000 and diff against the built table → identical
- [ ] **Step 4: Boot health** — board joins LAN (JordanAP); `/system_status` shows `6.0.0-beta11`, uptime climbing; `/ring_status` shows `"enabled":true`, `"paused":false`, `resets` ledger intact
- [ ] **Step 5: A-fix repro (the crash that wasn't)** — connect a client to the board AP polling `/ring_status` 1/s; serial DIAG `sta_drop`; expected: no reboot, status degrades to disconnected state, AP client keeps polling, ledger gains no PANIC entry (verify `/ring_status` resets unchanged)
- [ ] **Step 6: B2 round trip** — DIAG `rv2_flood 6000 120 7` during which HTTP `POST /ring_lock {pre_s:60, post_s:5, bus_mask:7}`; lock completes ≤2s after post window (state arming→idle, no flushing); `/sessions` shows the session with exact frames; download bin → `python3 scripts/decode_blackbox.py` → row count == meta frames total; download csv → byte-compare header + row count
- [ ] **Step 7: Protection & full** — second lock immediately (overlapping window) succeeds; delete one session → its range unprotected; fill protection (flood + many locks with large pre_s or manual bitmap pressure via repeated locks until `paused:true` or lock refused "no space") → verify refusal message, no overwrite (re-download first session intact)
- [ ] **Step 8: Reboot rebuild** — power-cycle; metas survive, `/sessions` unchanged, protection honored (writer does not clobber: flood 60s + check first session bytes unchanged via re-download hash)
- [ ] **Step 9: Evidence + docs** — serial log + HTTP transcripts into `docs/verification/`; append bench section; commit `test(p4): blackbox + sta-panic bench evidence` (English)

---

## Execution Notes

- Task 1 is independently field-shippable (flash single app@0x20000 only — no table change needed for A alone; if shipped solo, skip Task 7's table steps).
- Task ordering 2→3→4→5 is strict (interfaces build on each other); Task 6 after 5; Task 7 last.
- If `esp_partition_write` latency under WiFi load surprises (>250ms stalls visible in staging overflow), do NOT tune silently — surface it: the erase-ahead depth (1 block) can go to 2, but that's a spec delta to report.
