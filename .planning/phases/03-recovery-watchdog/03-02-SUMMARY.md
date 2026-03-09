---
phase: 03-recovery-watchdog
plan: "02"
subsystem: testing
tags: [validation, watchdog, recovery, hardware, bash]

requires:
  - phase: 03-01
    provides: recovery-manager watchdog policy, retained reset breadcrumbs, and anti-thrash behavior to verify on hardware
provides:
  - phase-safe validation guidance that separates automated build checks from device-only recovery sign-off
  - shared ready, supervisor, and recovery/watchdog markers plus a reusable Phase 3 hardware checklist
  - recorded human approval for healthy, degraded, recovery-reset, watchdog-reset, and anti-thrash scenarios
affects: [phase-03-complete, operator-validation, ota-lifecycle]

tech-stack:
  added: []
  patterns: [canonical build-only validate.sh entrypoint, blocking device verification checklist in shared shell helpers]

key-files:
  created: []
  modified: [README.md, scripts/common.sh, scripts/validate.sh]

key-decisions:
  - "Keep `./scripts/validate.sh` as the canonical automated validation command and delegate only to `./scripts/build.sh` for fast iteration."
  - "Treat real-hardware recovery/watchdog verification as a blocking human checkpoint with explicit healthy, degraded, reset-cause, watchdog, and anti-thrash coverage before Phase 3 completion."
  - "Centralize validation markers and checklist text in `scripts/common.sh` so README guidance and CLI output stay aligned."

patterns-established:
  - "Fast automated validation never pretends to verify watchdog recovery; it points operators to the blocking hardware checklist instead."
  - "Phase completion requires scenario-level device confirmation for healthy stability, degraded patience, preserved reset breadcrumbs, watchdog reset semantics, and anti-thrash behavior."

requirements-completed: [REC-01, REC-02]

duration: 9h 23m
completed: 2026-03-09
---

# Phase 3 Plan 2: Recovery Validation Summary

**Phase-safe validation flow that keeps build feedback fast while requiring explicit device approval for recovery resets, watchdog semantics, and anti-thrash behavior.**

## Performance

- **Duration:** 9h 23m
- **Started:** 2026-03-08T21:01:48Z
- **Completed:** 2026-03-09T06:25:33Z
- **Tasks:** 3
- **Files modified:** 3

## Accomplishments

- Documented the full Phase 3 validation flow in `README.md` while preserving one fast automated validation entrypoint for normal development loops.
- Centralized the ready-state, supervisor-state, and recovery/watchdog markers plus the blocking device checklist in shared shell helpers and `./scripts/validate.sh` output.
- Recorded approved real-hardware sign-off covering healthy stability, degraded patience, recovery-reset breadcrumbs, watchdog reset semantics, and anti-thrash behavior.

## Task Commits

Each task was committed atomically where code or docs changed:

1. **Task 1: Update validation guidance for recovery/watchdog markers and scenarios** - `94a78cc` (feat)
2. **Task 2: Stage the device checklist and preserve the automated build path** - `744d8dd` (feat)
3. **Task 3: Confirm recovery and watchdog behavior on real hardware** - approved by user on 2026-03-09 (human checkpoint, no code commit)

**Plan metadata:** pending final docs commit at summary creation time

## Files Created/Modified

- `README.md` - Documents the automated-versus-device validation split, the blocking hardware checklist, and the required Phase 3 scenarios.
- `scripts/common.sh` - Publishes the shared ready, supervisor, and recovery/watchdog markers along with the reusable Phase 3 checklist helpers.
- `scripts/validate.sh` - Keeps automated validation on `./scripts/build.sh` and prints the blocking device-only checklist after a successful build.

## Decisions Made

- Kept `./scripts/validate.sh` as the canonical automated validation command so wave-level feedback stays fast and consistent.
- Required real-device sign-off for the recovery/watchdog scenarios instead of trying to approximate watchdog semantics in the automated path.
- Reused shared marker and checklist helpers so validation guidance stays synchronized between docs and CLI output.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- None. The blocked checkpoint resumed with an explicit `approved` response after the device run.

## User Setup Required

None - the blocking hardware verification for this plan is complete.

## Self-Check: PASSED

- Verified `.planning/phases/03-recovery-watchdog/03-02-SUMMARY.md` exists.
- Verified prior task commits `94a78cc` and `744d8dd` exist in git history.
