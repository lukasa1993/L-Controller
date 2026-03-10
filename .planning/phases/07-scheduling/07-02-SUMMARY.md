---
phase: 07-scheduling
plan: 02
subsystem: scheduler
tags: [zephyr, sntp, cron, relay, panel-status]
requires:
  - phase: 07-scheduling-01
    provides: UTC cron persistence, scheduler service boundary, and conservative boot semantics
provides:
  - trusted UTC clock acquisition with one-reset-then-degrade recovery ownership
  - minute-boundary scheduler runtime with future-only baseline recomputation
  - dispatcher-backed scheduled relay execution with serialization and no-op protection
  - locked scheduler runtime snapshots for panel status and boot logging
affects: [07-scheduling-03, panel, status, relay-control]
tech-stack:
  added: [Zephyr SNTP, CLOCK_REALTIME]
  patterns: [scheduler-owned trusted clock helper, minute-boundary delayable work, locked runtime snapshot reads]
key-files:
  created: []
  modified:
    - app/Kconfig
    - app/prj.conf
    - app/src/actions/actions.c
    - app/src/actions/actions.h
    - app/src/app/app_config.h
    - app/src/app/bootstrap.c
    - app/src/panel/panel_status.c
    - app/src/panel/panel_status.h
    - app/src/recovery/recovery.c
    - app/src/recovery/recovery.h
    - app/src/scheduler/scheduler.c
    - app/src/scheduler/scheduler.h
key-decisions:
  - "Trusted time comes from a scheduler-owned Zephyr SNTP helper that prefers DHCP-provided NTP and falls back to an app-configured server."
  - "The live scheduler runs from one delayable minute-boundary work item and recomputes from the current UTC minute instead of backfilling missed work."
  - "Manual and scheduled commands share one dispatcher mutex and skip already-applied desired-state writes to avoid race windows and flash churn."
patterns-established:
  - "Trusted-clock recovery: first acquisition failure requests one clock-specific recovery reset, subsequent boot stays degraded without reboot looping."
  - "Scheduler status reads use one locked snapshot so panel payloads and boot logs report coherent runtime truth."
requirements-completed: [SCHED-03, SCHED-04]
duration: 13min
completed: 2026-03-10
---

# Phase 7 Plan 2: Live Scheduling Runtime Summary

**Trusted UTC clock acquisition with one-reset degrade recovery, minute-boundary scheduler execution, and live scheduler status snapshots**

## Performance

- **Duration:** 13 min
- **Started:** 2026-03-10T10:44:09+04:00
- **Completed:** 2026-03-10T06:56:31Z
- **Tasks:** 4
- **Files modified:** 12

## Accomplishments
- Added scheduler-owned SNTP time acquisition that sets `CLOCK_REALTIME`, requests one dedicated clock recovery reset on initial failure, and degrades cleanly on the next boot.
- Turned the scheduler into a minute-boundary runtime that retries trust acquisition, tracks future-only baselines, and recomputes next runs after trust or correction events.
- Routed scheduled relay commands through the existing dispatcher with mutex serialization and true no-op protection.
- Replaced the status placeholder with a live scheduler snapshot carrying clock trust, automation state, next run, last result, and recent problems.

## Task Commits

Each task was committed atomically:

1. **Task 1: Acquire trusted UTC time and implement the one-reset then degrade recovery path** - `239b9cf` (feat)
2. **Task 2: Implement trusted-minute schedule evaluation and next-run bookkeeping** - `1a9f8a7` (feat)
3. **Task 3: Dispatch scheduled jobs through the existing action engine with serialization and no-op protection** - `2f697e9` (feat)
4. **Task 4: Publish truthful scheduler runtime status and degraded clock visibility** - `980a071` (feat)

## Files Created/Modified
- `app/src/scheduler/scheduler.c` - Adds SNTP clock acquisition, minute-boundary runtime work, dispatcher-backed due execution, problem tracking, and locked snapshot export.
- `app/src/scheduler/scheduler.h` - Expands runtime state, problem codes, snapshot API, and service fields for live scheduling.
- `app/src/recovery/recovery.h` - Adds the trusted-clock-specific recovery reset trigger and request API.
- `app/src/recovery/recovery.c` - Persists clock-reset breadcrumbs through the recovery manager and exposes the trigger text.
- `app/src/actions/actions.c` - Serializes all dispatcher callers with a mutex and skips true no-op relay writes.
- `app/src/actions/actions.h` - Stores dispatcher lock state alongside the shared action execution surface.
- `app/src/panel/panel_status.c` - Builds a live scheduler status payload with clock trust, next run, last result, and recent problems.
- `app/src/panel/panel_status.h` - Increases the status payload budget for the richer scheduler runtime snapshot.
- `app/src/app/bootstrap.c` - Starts trusted-clock acquisition after network startup and logs one coherent scheduler snapshot.
- `app/src/app/app_config.h` - Wires trusted-clock server config into the app scheduler settings.
- `app/Kconfig` - Adds the fallback trusted-clock SNTP server config.
- `app/prj.conf` - Enables SNTP and DHCP NTP option support for the scheduler-owned trusted clock helper.

## Decisions Made
- Preferred DHCP-provided NTP configuration first, with an app-owned fallback SNTP server string, to keep trusted-clock ownership inside the firmware rather than a broader Zephyr auto-init path.
- Evaluated scheduler work only at trusted UTC minute boundaries and treated any backward jump or minute-changing correction as a future-only baseline reset.
- Kept `/api/status` authenticated and truthful by exporting action labels, not raw relay action IDs, while still preserving internal scheduler/action IDs in runtime state.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added missing network supervisor include for trusted-clock startup**
- **Found during:** Task 1 (Acquire trusted UTC time and implement the one-reset then degrade recovery path)
- **Issue:** The new trusted-clock sync helper used `network_supervisor_status` and `network_supervisor_get_status()` without including the corresponding header, which broke the build.
- **Fix:** Added `network/network_supervisor.h` to `app/src/scheduler/scheduler.c` and rebuilt successfully.
- **Files modified:** `app/src/scheduler/scheduler.c`
- **Verification:** `./scripts/build.sh`
- **Committed in:** `239b9cf`

**2. [Rule 3 - Blocking] Repaired C99 helper ordering after inserting runtime and status helpers**
- **Found during:** Tasks 2-4
- **Issue:** New scheduler helpers referenced internal static functions before their definitions, causing implicit-declaration and conflicting-static-definition build failures.
- **Fix:** Added the required forward declarations and consolidated last-result/problem helper usage so the scheduler compiles cleanly under the existing C99 build.
- **Files modified:** `app/src/scheduler/scheduler.c`
- **Verification:** `./scripts/build.sh`
- **Committed in:** `980a071`

**3. [Rule 3 - Blocking] Repaired stale planning state and roadmap progress after helper updates**
- **Found during:** Finalization
- **Issue:** `gsd-tools` advanced the plan counters, but `STATE.md` and `ROADMAP.md` kept stale human-readable 07-01 progress text and a `1/3` Phase 7 roadmap row.
- **Fix:** Manually updated `.planning/STATE.md` and `.planning/ROADMAP.md` so the human-readable status matches the completed `07-02` summary and the next executable plan is `07-03`.
- **Files modified:** `.planning/STATE.md`, `.planning/ROADMAP.md`
- **Verification:** Re-read the current-position block in `.planning/STATE.md` and the Phase 7 checklist/progress row in `.planning/ROADMAP.md`.
- **Committed in:** final docs commit

---

**Total deviations:** 3 auto-fixed (3 Rule 3 - Blocking)
**Impact on plan:** All three fixes were required to keep the runtime buildable and the planning handoff accurate. They did not change the intended product scope.

## Issues Encountered
- `./scripts/build.sh` still emits the pre-existing MBEDTLS Kconfig warnings already tracked in `.planning/STATE.md`, but the firmware build completes successfully.

## User Setup Required

None - no external service configuration required beyond the existing firmware build secrets.

## Next Phase Readiness
- Plan `07-03` can build schedule-management API and UI flows on top of a live scheduler engine, stable dispatcher seam, and truthful runtime status payload.
- The panel and logs now expose clock trust, next run, last result, and recent problems, so operator-facing scheduling work can consume live state instead of placeholders.
- No new blockers were introduced by this plan.

## Self-Check: PASSED
- Verified `.planning/phases/07-scheduling/07-02-SUMMARY.md` exists.
- Verified task commits `239b9cf`, `1a9f8a7`, `2f697e9`, and `980a071` exist in `git`.
