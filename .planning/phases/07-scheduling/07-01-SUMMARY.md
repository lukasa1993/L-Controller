---
phase: 07-scheduling
plan: 01
subsystem: scheduling
tags: [zephyr, cron, utc, nvs, scheduler]

requires:
  - phase: 04-persistent-configuration
    provides: NVS-backed sectioned persistence and layout-versioned validation
  - phase: 06-action-engine-relay-control
    provides: Relay action dispatcher and stable relay0.on/off action IDs
provides:
  - Trusted-clock-gated scheduler service with runtime status and future-only baseline helpers
  - UTC 5-field cron schedule persistence contract with layout version 2
  - Pre-persistence conflict rejection for enabled opposite-state same-minute relay schedules
affects: [07-02, 07-03, panel, api]

tech-stack:
  added: []
  patterns:
    - UTC-only 5-field cron persistence
    - Scheduler-domain validation reused by persistence
    - Boot-owned trusted-clock gating with future-only baselining

key-files:
  created:
    - app/src/scheduler/scheduler.c
  modified:
    - app/CMakeLists.txt
    - app/Kconfig
    - app/prj.conf
    - app/src/app/app_config.h
    - app/src/app/app_context.h
    - app/src/app/bootstrap.c
    - app/src/persistence/persistence.c
    - app/src/persistence/persistence_types.h
    - app/src/scheduler/scheduler.h

key-decisions:
  - "Schedules persist as explicit UTC 5-field cron expressions and the persistence layout is bumped to version 2."
  - "Enabled opposite-state same-minute overlaps are rejected before save, while same-state overlaps remain allowed."
  - "Scheduler automation stays inactive until trusted time is explicitly acquired, and trust/correction events baseline the current minute, skip missed jobs, and compute only future runs."

patterns-established:
  - "Scheduler validation and cron parsing live in the scheduler module and are reused by persistence load/save."
  - "Boot composes the scheduler after recovery, relay, and action initialization so later status surfaces can rely on scheduler truth."
  - "Trusted-clock loss or absence is visible as inactive automation with degraded reason rather than guessed readiness."

requirements-completed: [SCHED-01, SCHED-02, SCHED-04]

duration: 10min
completed: 2026-03-10
---

# Phase 07 Plan 01: Scheduling Summary

**UTC cron-backed scheduler foundation with trusted-clock gating, conflict-safe persistence, and boot-owned future-only baselining**

## Performance

- **Duration:** 10 min
- **Started:** 2026-03-10T06:15:49Z
- **Completed:** 2026-03-10T06:26:07Z
- **Tasks:** 3
- **Files modified:** 10

## Accomplishments
- Added a real `scheduler.c` service with trusted-clock state, degraded reasons, next-run status, last-result visibility, and future-only baseline helpers.
- Replaced the legacy weekday/minute schedule shape with a UTC cron expression contract and bumped persistence layout handling to version 2.
- Reused scheduler-side cron parsing and exact opposite-state conflict validation from persistence load/save, then composed scheduler ownership into boot after actions are ready.

## Task Commits

Each task was committed atomically:

1. **Task 1: Add the scheduler build surface and explicit trusted-clock contract** - `f1c1770` (feat)
2. **Task 2: Migrate persisted schedules to a canonical cron contract with conservative validation** - `6be6341` (feat)
3. **Task 3: Compose scheduler ownership into boot with skip-missed and future-only semantics** - `4f4f05e` (feat)

## Files Created/Modified
- `app/src/scheduler/scheduler.c` - Scheduler runtime service, cron parser/evaluator, validation, next-run computation, and trusted-clock baseline logic.
- `app/src/scheduler/scheduler.h` - Scheduler runtime contracts, status structs, cron matcher types, and service APIs.
- `app/src/persistence/persistence_types.h` - Canonical UTC cron persistence record replacing `days_of_week_mask` and `minute_of_day`.
- `app/src/persistence/persistence.c` - Scheduler-backed cron validation, conflict rejection, and layout-aware schedule load/save path.
- `app/src/app/bootstrap.c` - Boot-time scheduler composition and conservative status logging.
- `app/src/app/app_context.h` - Embedded scheduler service ownership in `app_context`.
- `app/CMakeLists.txt` - Registered the scheduler compilation unit.
- `app/Kconfig` - Added scheduler cadence and trusted-clock knobs and bumped persistence layout default.
- `app/prj.conf` - Set the Phase 7 scheduler defaults.
- `app/src/app/app_config.h` - Added scheduler config wiring.

## Decisions Made
- Kept the supported cron grammar numeric and 5-field only: `minute hour day-of-month month day-of-week` with `*`, lists, ranges, and steps.
- Treated day-of-month/day-of-week with standard OR semantics and ignored disabled schedules during opposite-state overlap checks.
- Kept automation inactive until trusted time is provided explicitly instead of inferring trust from boot readiness or current wall clock state.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Repaired planning metadata after tooling parse failures**
- **Found during:** Finalization
- **Issue:** `state advance-plan` could not parse the pre-phase-7 `Current Plan: Not started` marker, and `roadmap update-plan-progress` reported success without changing the Phase 7 checklist row.
- **Fix:** Kept the automated requirements and session updates, then manually repaired `.planning/STATE.md` and `.planning/ROADMAP.md` to reflect plan `2 of 3`, `Ready to execute next plan`, `89%` overall progress, and `07-01` checked off in the roadmap.
- **Files modified:** `.planning/STATE.md`, `.planning/ROADMAP.md`
- **Verification:** Re-read the `Current Position` block in `.planning/STATE.md` and the Phase 7 checklist/progress row in `.planning/ROADMAP.md`.
- **Committed in:** final docs commit

---

**Total deviations:** 1 auto-fixed (1 Rule 3 - Blocking)
**Impact on plan:** No product scope change; this only repaired planning metadata so the next-plan handoff stays accurate.

## Issues Encountered
- `./scripts/build.sh` succeeds, but Zephyr still emits the pre-existing MBEDTLS Kconfig warnings already tracked in `.planning/STATE.md`.
- A scheduler compile warning about an uninitialized local candidate hour appeared during implementation and was fixed before the final task commit.
- The state/roadmap helper commands left stale Phase 7 progress markers, so `.planning/STATE.md` and `.planning/ROADMAP.md` required a manual repair during finalization.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Plan `07-02` can now add live trusted-time acquisition, minute work scheduling, and dispatch execution on top of the boot-owned scheduler boundary.
- Plan `07-03` can build API/UI flows against the stable UTC cron persistence contract and scheduler runtime status.
- No new blockers were introduced by this plan.

## Self-Check: PASSED
- Verified `.planning/phases/07-scheduling/07-01-SUMMARY.md` exists.
- Verified task commits `f1c1770`, `6be6341`, and `4f4f05e` exist in `git log`.
