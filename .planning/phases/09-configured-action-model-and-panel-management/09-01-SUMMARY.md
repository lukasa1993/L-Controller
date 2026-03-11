---
phase: 09-configured-action-model-and-panel-management
plan: "01"
subsystem: infra
tags: [zephyr, persistence, relay, scheduler, migration]
requires:
  - phase: 04-persistent-configuration
    provides: "Atomic sectioned persistence saves and load-time section validation"
  - phase: 06-action-engine-relay-control
    provides: "Canonical action dispatch path and relay runtime ownership"
  - phase: 07-scheduling
    provides: "Persisted schedule tables keyed by canonical action_id values"
provides:
  - "Configured relay action records with display_name, output_key, enabled state, and explicit command metadata"
  - "Relay-owned approved output registry and bound-command execution helpers"
  - "Legacy built-in action normalization plus scheduler-safe compatibility for relay0.on and relay0.off"
affects: [09-02, 09-03, 10-shared-execution-and-schedule-integration, schedules]
tech-stack:
  added: []
  patterns: [command-centric action records, section-local legacy normalization, firmware-owned approved output validation]
key-files:
  created: []
  modified:
    - app/src/persistence/persistence_types.h
    - app/src/actions/actions.c
    - app/src/actions/actions.h
    - app/src/persistence/persistence.c
    - app/src/relay/relay.c
    - app/src/relay/relay.h
    - app/src/scheduler/scheduler.c
    - app/src/scheduler/scheduler.h
    - app/src/app/bootstrap.c
key-decisions:
  - "One configured action now represents one executable relay command rather than a hidden built-in boolean toggle."
  - "Legacy relay0.on and relay0.off survive only as explicit compatibility IDs; they are no longer seeded into the persisted operator catalog."
  - "Scheduler validation resolves legacy IDs explicitly while conflict checks compare output binding plus command semantics."
patterns-established:
  - "Relay module owns the approved output registry and command-to-state translation for configured actions."
  - "Persistence can normalize a single section's legacy blob shape without bumping the global layout version or resetting unrelated sections."
requirements-completed: [BIND-01, BIND-02, MIGR-01, MIGR-03]
duration: 7 min
completed: 2026-03-11
---

# Phase 9 Plan 01: Configured Action Schema Summary

**Configured relay action records with approved-output bindings, explicit legacy built-in normalization, and schedule-safe compatibility for legacy relay action IDs**

## Performance

- **Duration:** 7 min
- **Started:** 2026-03-11T08:39:06Z
- **Completed:** 2026-03-11T08:46:00Z
- **Tasks:** 3
- **Files modified:** 9

## Accomplishments
- Replaced the persisted action shape with a command-centric configured-action record carrying `display_name`, `output_key`, `enabled`, `type`, and `command`.
- Added a relay-owned approved output contract so actions validate against firmware-defined outputs instead of raw GPIO assumptions.
- Removed unconditional built-in action seeding, normalized legacy action blobs explicitly, and preserved legacy schedule references through explicit compatibility resolution.

## Task Commits

Each task was committed atomically:

1. **Task 1: Redefine the action catalog around configured relay records and approved outputs** - `4e7bb64` (feat)
2. **Task 2: Replace unconditional built-in seeding with explicit legacy normalization** - `f697f52` (fix)
3. **Task 3: Keep configured-action saves atomic and preserve legacy schedule references safely** - `2a2da92` (fix)

## Files Created/Modified
- `app/src/persistence/persistence_types.h` - Expanded persisted action schema and migration action enum.
- `app/src/actions/actions.h` - Added configured-action validation, ID generation, and compatibility helper APIs.
- `app/src/actions/actions.c` - Switched dispatcher logic to output-bound commands and explicit legacy compatibility actions.
- `app/src/relay/relay.h` - Declared the approved output registry and bound-command helpers.
- `app/src/relay/relay.c` - Implemented firmware-owned approved output lookup and command-to-state translation.
- `app/src/persistence/persistence.c` - Added legacy action normalization and strict configured-action save validation.
- `app/src/scheduler/scheduler.h` - Updated runtime entry shape to carry output binding and command semantics.
- `app/src/scheduler/scheduler.c` - Resolved legacy action IDs explicitly during schedule validation and runtime compilation.
- `app/src/app/bootstrap.c` - Surfaced non-default persistence migration actions in boot logs.

## Decisions Made

- The configured action catalog is command-centric: one record maps to one executable relay command, not to a parent output with hidden subcommands.
- Approved output truth now belongs to the relay subsystem, so persistence and dispatcher validation share one firmware-owned binding contract.
- Legacy `relay0.on` and `relay0.off` IDs remain compatibility-only runtime truths for schedules and legacy callers, never operator-facing catalog entries.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Tightened action save validation to the full configured-action schema**
- **Found during:** Task 3 (Keep configured-action saves atomic and preserve legacy schedule references safely)
- **Issue:** The persistence save path still validated actions by unique `action_id` only, which would have allowed invalid bindings or incomplete configured records to persist.
- **Fix:** Routed `persisted_action_catalog_valid()` through `action_dispatcher_action_record_valid()` so saves now enforce non-empty strings, approved outputs, valid commands, and the configured-action contract.
- **Files modified:** app/src/actions/actions.c, app/src/persistence/persistence.c
- **Verification:** `./scripts/build.sh`
- **Committed in:** `2a2da92`

**2. [Rule 3 - Blocking] Pulled relay binding definitions into persistence migration code**
- **Found during:** Task 2 (Replace unconditional built-in seeding with explicit legacy normalization)
- **Issue:** The new legacy normalization helper in `persistence.c` referenced the relay output key contract without including the relay header, which broke the build.
- **Fix:** Included `relay/relay.h` so the migration logic uses the same approved-output constant as the runtime code.
- **Files modified:** app/src/persistence/persistence.c
- **Verification:** `./scripts/build.sh`
- **Committed in:** `f697f52`

---

**Total deviations:** 2 auto-fixed (1 missing critical, 1 blocking)
**Impact on plan:** Both fixes were required for correctness. They tightened the configured-action contract without expanding scope beyond the plan.

## Issues Encountered

- The first Task 2 build failed because the new legacy normalization helper lacked the relay output key definition. The follow-up include fix restored a green build immediately.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase `09-02` can build authenticated `/api/actions` routes directly on the configured-action schema, relay-owned output registry, and server-side ID generation helpers.
- Phase `09-03` can render the operator catalog from persisted configured actions without inheriting hidden built-in relay state.
- Schedule create/edit UX still uses legacy public keys until later work switches it onto the shared configured-action catalog in Phase 10.

## Self-Check: PASSED
- Found summary file: `.planning/phases/09-configured-action-model-and-panel-management/09-01-SUMMARY.md`
- Found task commit: `4e7bb64`
- Found task commit: `f697f52`
- Found task commit: `2a2da92`

---
*Phase: 09-configured-action-model-and-panel-management*
*Completed: 2026-03-11*
