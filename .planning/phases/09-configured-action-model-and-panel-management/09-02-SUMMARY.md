---
phase: 09-configured-action-model-and-panel-management
plan: "02"
subsystem: api
tags: [zephyr, panel, http, persistence, actions]
requires:
  - phase: 09-01
    provides: "Configured relay action records, approved output validation, and legacy-compatible action resolution"
  - phase: 05-local-control-panel
    provides: "Authenticated exact-route HTTP patterns and panel shell infrastructure"
provides:
  - "Authenticated /api/actions snapshot with action catalog truth, output choices, command choices, and usability metadata"
  - "Configured action create and update handlers with device-generated operator-safe IDs and field-specific validation errors"
  - "Action mutation save-and-reload flow that persists through the atomic store and refreshes scheduler runtime truth"
affects: [09-03, 10-shared-execution-and-schedule-integration, panel, schedules]
tech-stack:
  added: []
  patterns: [server-truth action snapshots, exact-route action mutations, JSON-safe user-authored action serialization]
key-files:
  created: []
  modified:
    - app/src/panel/panel_status.h
    - app/src/panel/panel_status.c
    - app/src/panel/panel_http.c
key-decisions:
  - "Configured action IDs are generated on create and stay stable on edit so schedule references and future cross-surface links do not churn when the display name changes."
  - "The /api/actions snapshot publishes both approved output choices and relay command choices so the panel stays server-truth-first and does not invent hardware options locally."
  - "Action snapshot JSON escapes user-authored display names and summaries before serialization so operator-authored names cannot corrupt the response body."
patterns-established:
  - "Authenticated action mutations follow the same exact-route POST pattern as schedules while keeping action-specific validation and error fields explicit."
  - "Action saves reload the scheduler immediately so any runtime view that depends on action command semantics sees persisted truth on the next fetch."
requirements-completed: [ACT-03, ACT-04, ACT-05, BIND-01, BIND-02]
duration: 12 min
completed: 2026-03-11
---

# Phase 9 Plan 02: Configured Action API Summary

**Authenticated configured-action snapshot and create/edit routes with device-generated IDs, approved-output validation, and truthful usability metadata**

## Performance

- **Duration:** 12 min
- **Started:** 2026-03-11T08:59:30Z
- **Completed:** 2026-03-11T09:11:44Z
- **Tasks:** 3
- **Files modified:** 3

## Accomplishments

- Added the authenticated exact `/api/actions`, `/api/actions/create`, and `/api/actions/update` routes so the panel can fetch, create, and edit configured actions through one server-owned contract.
- Serialized the configured-action catalog with output choices, relay command choices, action IDs, output summaries, and short `Ready` / `Disabled` / `Needs attention` usability truth.
- Routed accepted action mutations through `persistence_store_save_actions()` and immediately reloaded scheduler runtime state so later fetches see persisted catalog truth instead of stale in-memory assumptions.

## Task Commits

The HTTP contract, mutation handlers, and reload wiring landed together because they share one route module and needed to stay buildable as a unit:

1. **Task 1: Add a configured-action snapshot endpoint with truthful operator-facing status** - `7b8044c` (feat)
2. **Task 2: Implement authenticated create and edit handlers with server-owned ID generation** - `7b8044c` (feat)
3. **Task 3: Preserve atomic save-and-reload behavior for configured-action mutations** - `7b8044c` (feat)

## Files Created/Modified

- `app/src/panel/panel_status.h` - Declares the configured-action snapshot renderer used by the new API surface.
- `app/src/panel/panel_status.c` - Serializes the configured-action catalog, approved output choices, relay command choices, usability labels, and JSON-safe operator-authored strings.
- `app/src/panel/panel_http.c` - Registers the exact action routes, validates action payloads, generates stable action IDs on create, and persists or reloads the catalog.

## Decisions Made

- Create requests generate the operator-safe `actionId` on the device, while update requests preserve the existing ID so later schedule references and cross-surface links stay stable.
- The action snapshot includes output and command choices as part of the same response so the Actions page can remain server-truth-first instead of hard-coding relay options.
- User-authored action names and summaries are escaped during JSON serialization because phase 9 introduces the first operator-authored strings in this API surface.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Escaped user-authored action strings in the snapshot serializer**
- **Found during:** Task 1 (Add a configured-action snapshot endpoint with truthful operator-facing status)
- **Issue:** The existing panel serializers built JSON with raw `%s` insertion, which would have made operator-authored display names unsafe once configured actions became user-managed records.
- **Fix:** Added explicit JSON string escaping in `panel_status.c` and used it for action IDs, display names, output labels, summaries, and usability details in the `/api/actions` snapshot.
- **Files modified:** `app/src/panel/panel_status.c`
- **Verification:** `./scripts/build.sh`
- **Committed in:** `7b8044c`

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** The fix tightened the action snapshot contract without changing scope. It was required for safe serialization of operator-authored action names.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase `09-03` can render the real Actions management surface directly from `/api/actions` and post create or edit mutations without inventing local output or command catalogs.
- The backend now returns stable action IDs, output summaries, and short usability labels that the Actions page can show verbatim.
- Schedule create/edit remains intentionally deferred to the legacy catalog until Phase 10 wires the shared action catalog into that surface.

## Self-Check: PASSED
- Found summary file: `.planning/phases/09-configured-action-model-and-panel-management/09-02-SUMMARY.md`
- Found task commit: `7b8044c`

---
*Phase: 09-configured-action-model-and-panel-management*
*Completed: 2026-03-11*
