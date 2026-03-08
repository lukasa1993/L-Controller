---
phase: 01-foundation-refactor
plan: "03"
subsystem: infra
tags: [zephyr, nrf7002, validation, build, hardware-smoke]
requires:
  - phase: 01-02
    provides: composition-only main plus extracted bootstrap and network runtime ownership
provides:
  - build-first validation entrypoint delegating to `./scripts/build.sh`
  - documented split between automated refactor validation and blocking hardware smoke sign-off
  - approved device smoke record for `Wi-Fi connected`, `DHCP IPv4 address: ...`, `Reachability check passed`, and `APP_READY`
affects: [02-wi-fi-supervision, 03-recovery-watchdog, developer-workflow]
tech-stack:
  added: []
  patterns: [build-first validation, explicit hardware smoke gate, reusable shell helpers]
key-files:
  created: [.planning/phases/01-foundation-refactor/01-03-SUMMARY.md]
  modified: [scripts/common.sh, scripts/validate.sh, scripts/doctor.sh, README.md]
key-decisions:
  - "Phase 1 automation reuses `./scripts/build.sh` as the single canonical build signal via `./scripts/validate.sh`."
  - "Hardware sign-off stays explicit and blocking after `./scripts/flash.sh` and `./scripts/console.sh`, rather than being folded into the refactor loop."
  - "Phase 1 closes only after a human-approved device smoke confirms `Wi-Fi connected`, `DHCP IPv4 address: ...`, `Reachability check passed`, and `APP_READY`."
patterns-established:
  - "`./scripts/validate.sh` is the default refactor loop and only reaches for hardware-aware checks when explicitly asked via `--preflight`."
  - "Developer guidance should separate automated build feedback from manual device sign-off with concrete ready-state markers."
requirements-completed: [PLAT-01, PLAT-02, PLAT-03]
duration: 7 min
completed: 2026-03-08
---

# Phase 1 Plan 3: Foundation Refactor Summary

**Build-first Phase 1 validation via `./scripts/validate.sh` with a blocking real-device smoke gate recorded against the ready-state markers**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-08T18:10:12Z
- **Completed:** 2026-03-08T18:18:06Z
- **Tasks:** 4
- **Files modified:** 4

## Accomplishments

- Added a repo-owned validation entrypoint that reuses `./scripts/build.sh` and shares the expected ready-state markers through `scripts/common.sh`.
- Documented the split between automated refactor validation and manual hardware sign-off in `README.md`, with `scripts/doctor.sh` reinforcing the same build-first versus hardware-gate guidance.
- Re-ran `./scripts/validate.sh` successfully and recorded the approved hardware smoke checkpoint after the device reached `Wi-Fi connected`, `DHCP IPv4 address: ...`, `Reachability check passed`, and `APP_READY`.

## Task Commits

Each code task was committed atomically, and the final checkpoint was approved in continuation:

1. **Task 1: Create a validation entrypoint that reuses the build workflow** - `2c97dac` (feat)
2. **Task 2: Clarify build-first versus hardware sign-off guidance** - `1907f47` (chore)
3. **Task 3: Run the build-first validation path and stage the hardware smoke gate** - `1b6d90f` (chore)
4. **Task 4: Confirm hardware smoke on device** - Approved at the blocking human-verify checkpoint after all required ready-state markers were observed on hardware (no code commit required)

## Files Created/Modified

- `scripts/common.sh` - Added shared helpers for repo-script execution and the canonical ready-state marker list.
- `scripts/validate.sh` - Provides the build-first validation entrypoint, optional `--preflight`, and the blocking hardware smoke reminder.
- `scripts/doctor.sh` - Clarifies the USB/serial preflight and points operators back to the build-first versus hardware-gate split.
- `README.md` - Documents the Phase 1 validation workflow, the exact ready-state markers, and the final real-device sign-off steps.
- `.planning/phases/01-foundation-refactor/01-03-SUMMARY.md` - Captures the approved checkpoint and plan completion details.

## Decisions Made

- `./scripts/validate.sh` remains the default refactor feedback loop so repeated validation stays fast and hardware-independent.
- `./scripts/build.sh` remains the single canonical automated build signal; validation scaffolding wraps it instead of duplicating build logic.
- The hardware smoke gate stays explicit and blocking, and this plan is recorded complete only because the user approved a device run that reached every required ready-state marker.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- `shellcheck` is not installed in this environment, so the optional shell lint step was skipped.
- Zephyr still emits non-fatal MBEDTLS Kconfig warnings during `./scripts/validate.sh`; they remain visible but did not block the build or this plan.

## User Setup Required

None - no external service configuration required, and the blocking hardware smoke checkpoint has already been approved.

## Next Phase Readiness

- Phase 1 now has both a fast automated validation command and an explicit, recorded device-level bring-up sign-off.
- Later phases can rely on the documented baseline smoke markers and the build-first workflow instead of rediscovering the current ready-state path.
- Pre-existing non-fatal MBEDTLS warnings remain visible during configure output, but Phase 1 validation completed successfully.

## Self-Check: PASSED

FOUND: `.planning/phases/01-foundation-refactor/01-03-SUMMARY.md`
FOUND: `scripts/common.sh`
FOUND: `scripts/validate.sh`
FOUND: `scripts/doctor.sh`
FOUND: `README.md`
FOUND: `2c97dac`
FOUND: `1907f47`
FOUND: `1b6d90f`
