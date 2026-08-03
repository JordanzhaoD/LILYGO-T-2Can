# LILYGO T-2CAN Repository Cleanup Design

Date: 2026-06-09

## Goal

Keep the current LILYGO T-2CAN repository safe and clean after the Atlas UI polish firmware build and device flash.

## Approved cleanup approach

Use **方案 A：安全收口**.

## Keep and commit

Commit the latest verified source and asset state on local `main`:

- `include/web/mcp2515_dashboard_ui.h`
- `firmware-assets/LILYGO-T-2CAN_latest_SHA256SUMS.txt`
- `firmware-assets/LILYGO-T-2CAN_latest_full_merged.bin`
- `firmware-assets/LILYGO-T-2CAN_latest_ota_app.bin`
- `firmware-assets/v4.1.0_20260609-1730/`
- `.gitignore` update for `.superpowers/`

These files represent the firmware that was generated and flashed to the connected device on 2026-06-09.

## Clean or archive

- Delete `.superpowers/brainstorm/` temporary session directories from the repository working tree.
- Move old untracked local artifacts outside the repository into an archive directory:
  - `firmware-assets/v4.1.0_20260608-2153/`
  - `docs/superpowers/specs/2026-06-02-fsd-dashboard-source-merge-checkpoint.md`

## Preserve

Keep `stash@{0}: pre-ui-polish-merge-20260609` for now as a safety backup. It does not affect `git status` and can be removed later after Jordan confirms there is no need for rollback.

## Verification

Before claiming cleanup is complete, run:

- `python3 scripts/minify_dashboard.py --check`
- `python3 -m pytest test/test_dashboard_api_contract.py -q`
- `shasum -a 256 -c firmware-assets/v4.1.0_20260609-1730/SHA256SUMS.txt`
- `git status -sb`

## Non-goals

- Do not push to `origin/main`.
- Do not resolve the `main...origin/main [ahead, behind]` divergence in this cleanup.
- Do not delete the safety stash.
- Do not delete harness-managed `.claude/worktrees/` worktrees.
