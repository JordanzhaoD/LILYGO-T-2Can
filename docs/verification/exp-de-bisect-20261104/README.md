# Experiments D / E / E3 — bench bisect attempts, all negative (2026-11-04)

Firmware `firmware-bc1d106.bin`, board 2, bench. Continuation of
`../exp-c-http-polling-ab-20261104/` (pure polling arms, also negative).
Goal: lock an on-demand reproducer for the httpd TCB `pxStackBase` +0x84
rewrite. Result: five arms, zero trips — and the pivot that follows.

## Arms

| arm | recipe (each on a fresh boot) | requests | trip |
|---|---|---|---|
| D1 | 90 s settle → `exp_b 200 10 1` → 10 s → 1× `/system_status` → 60 s → 30× @1 Hz | 31 | no |
| D2 | same, `/status` instead | 31 | no |
| E1 | 90 s settle → `exp_b 200 30 1` with `/system_status` every 6 s during flood + 60 s after (panic-#2 concurrency shape) | 17 | no |
| E2 | 90 s settle → `exp_b 200 10 1` → 18-shot rapid burst → 420 s silent → 1× `/system_status` (panic-#1 boot history) | 19 | no |
| E3 | **17 s settle (early boot)** → `exp_b 200 30 1` → dual poll `/status`+`/system_status` every ~1.5 s through flood + 60 s | 154 | no |

Flood counters identical across arms (`tx=17 fail=1983` for 10 s,
queue-full spam) — the primer reproduced deterministically; the handler
paths verifiably executed (mac-error counts match request counts,
temperature first-use line present). Ledger after all arms: only `[3,0]`
soft resets, no new `[4,x]`.

## Reading

Cumulative with experiment C: ~870 `/system_status` and ~185 `/status`
hits across ten fresh boots, five flood timings (early boot, late boot,
concurrent, post-flood, post-idle), single/burst/dual-client patterns —
zero reproductions. The two trips this morning and round-8 remain
unreproducible on demand; some condition we have not identified (or a
combination: real browser client with keep-alive connections and asset
loads, the DIAG command sequence, OTA recency, esptool-reset boot
pedigree) is still missing.

Stack-geometry note: `pxStackBase` = low end of the stack; a value of
`base + 0x84` is a pointer to the near-bottom (deepest) frame area —
consistent with round-8's "/status deepest-frame residue" reading.

## Pivot: stop reproducing, trap the writer in hardware

Proposed experiment F (firmware change): arm two RISC-V PMP TOR entries
(locked, R ok, W deny) around the httpd TCB `pxStackBase` field
(TCB+0x30..+0x34) on both harts, from the heap-guard adoption point.
Nothing legitimately writes that field after task creation, so the next
rewrite faults precisely at the writer's PC (store access fault, MEPC),
which the panic handler prints and the coredump captures. Self-verify
with a DIAG command that deliberately writes the field and expects a
fault at its own PC. IDF 6.0.1 exposes no public PMP API on P4, but the
CSRs are writable directly (pattern used by IDF's own sleep code on
other RISC-V targets).

## Files

- `p4_expD*.py/.jsonl/.log` — D1/D2
- `p4_expE*.py/.jsonl/.log` — E1/E2
- `p4_expE3*.py/.jsonl/.log` — E3
