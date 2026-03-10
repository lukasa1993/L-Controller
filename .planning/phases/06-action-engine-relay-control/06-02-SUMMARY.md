---
phase: 06-action-engine-relay-control
plan: "02"
subsystem: relay-control
tags: [zephyr, nrf, relay, actions, persistence]
requires:
  - phase: 06-action-engine-relay-control
    provides: Relay runtime ownership, live status, and startup safe-state policy from 06-01
provides:
  - Generic action_dispatcher contract with source and result types
  - Reserved built-in relay on and relay off action IDs seeded into the durable catalog
  - Shared action_id execution path that actuates relay_service and persists remembered desired state
affects: [06-03, phase-07-scheduling, panel-http, scheduler]
tech-stack:
  added: []
  patterns: [generic action dispatch boundary, relay rollback on persistence failure]
key-files:
  created: [app/src/actions/actions.c]
  modified: [app/CMakeLists.txt, app/src/app/app_context.h, app/src/app/bootstrap.c, app/src/actions/actions.h, app/src/relay/relay.h, app/src/relay/relay.c]
key-decisions:
  - Reserve stable relay built-ins as relay0.off and relay0.on and seed them during dispatcher startup.
  - Keep HTTP and future scheduler callers on action_id execution while the dispatcher owns relay actuation and persistence writes.
  - Roll back relay runtime state if desired-state persistence fails so accepted commands remain durable and truthful.
patterns-established:
  - Action execution flows through action_dispatcher_execute(action_id, source, result) instead of direct GPIO or persistence calls.
  - Relay runtime source metadata distinguishes manual-panel, scheduler, and safety-policy callers for later status rendering.
requirements-completed: [ACT-01, ACT-02]
duration: 4m
completed: 2026-03-10
---

# Phase 06 Plan 02: Generic Action Model and Execution Pipeline Summary

**Generic action dispatcher with seeded relay on/off actions and durable relay execution through `relay_service`.**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-10T07:50:40+04:00
- **Completed:** 2026-03-10T07:54:04+04:00
- **Tasks:** 3
- **Files modified:** 7

## Accomplishments
- Added an app-owned `action_dispatcher` boundary with generic source and result types and boot-time composition after relay startup.
- Seeded reserved `relay0.off` and `relay0.on` actions into persisted config without clobbering non-reserved catalog entries.
- Implemented one shared `action_id` execution path that validates actions, actuates the relay runtime, records source metadata, and persists remembered desired state.

## Task Commits

Each task was committed atomically:

1. **Task 1: Define the dispatcher contract and compose it into app ownership** - `123b331` (feat)
2. **Task 2: Seed reserved built-in relay actions for clean-device control** - `b53b1d0` (feat)
3. **Task 3: Implement the shared execution path and desired-state persistence** - `25702d4` (feat)

**Plan metadata:** Included in the final docs commit for this plan.

## Files Created/Modified
- `app/src/actions/actions.c` - Implements dispatcher initialization, built-in action seeding, action lookup, execution, and persistence rollback handling.
- `app/src/actions/actions.h` - Defines generic action execution source and result contracts plus built-in relay action ID access.
- `app/src/relay/relay.h` - Exposes relay command and rollback APIs plus richer runtime source metadata.
- `app/src/relay/relay.c` - Applies relay commands, restores runtime state on rollback, and renders source text for later status/UI work.
- `app/src/app/app_context.h` - Adds dispatcher ownership to the app context.
- `app/src/app/bootstrap.c` - Initializes the dispatcher after relay runtime startup.
- `app/CMakeLists.txt` - Adds the new action dispatcher source to the firmware build.

## Decisions Made
- Reserved relay actions use stable `relay0.off` and `relay0.on` IDs so future panel and scheduler callers can share durable references without exposing action administration in the UI.
- The dispatcher, not HTTP handlers or future scheduler code, owns action resolution, relay actuation, and relay persistence writes.
- Relay status source values now distinguish `manual-panel`, `scheduler`, and `safety-policy` so later status rendering can explain why the current relay state changed.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added relay rollback when desired-state persistence fails**
- **Found during:** Task 3 (Implement the shared execution path and desired-state persistence)
- **Issue:** Applying the relay command before persisting `last_desired_state` could leave the live relay changed while durable desired state stayed stale if the save failed.
- **Fix:** Snapshotted the prior relay runtime status, restored it on `persistence_store_save_relay` failure, and returned a failed dispatch result.
- **Files modified:** `app/src/actions/actions.c`, `app/src/relay/relay.c`, `app/src/relay/relay.h`
- **Verification:** `./scripts/build.sh && rg -n "action_dispatcher_execute|enabled|last_desired_state|source|relay_service_" app/src/actions/actions.c app/src/actions/actions.h app/src/relay/relay.c app/src/relay/relay.h`
- **Committed in:** `25702d4` (part of Task 3 commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Necessary for durable command correctness. No scope creep.

## Issues Encountered
None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- Phase 06-03 can translate desired relay state to `action_dispatcher_builtin_relay_action_id(...)` and call `action_dispatcher_execute(...)` instead of touching relay or persistence directly.
- Phase 07 scheduling can execute stored `action_id` values through the same dispatcher boundary.
- Zephyr still emits the pre-existing non-fatal MBEDTLS Kconfig warnings during build, but this plan's build completed successfully.

---
*Phase: 06-action-engine-relay-control*
*Completed: 2026-03-10*

## Self-Check: PASSED

- Verified summary file exists.
- Verified commit `123b331` exists.
- Verified commit `b53b1d0` exists.
- Verified commit `25702d4` exists.
