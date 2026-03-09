---
phase: 04-persistent-configuration
plan: "02"
subsystem: infra
tags: [zephyr, nvs, persistence, auth, relay, scheduler, validation]
requires:
  - phase: 04-01
    provides: NVS-backed persistence store, versioned section contracts, and boot-owned snapshot loading
provides:
  - snapshot-aware persistence save APIs with typed request shapes
  - auth reseed enforcement for blank or invalid stored credentials
  - staged action and schedule validation with unique-ID and reference checks
affects: [04-03, panel_auth, actions, relay, scheduler]
tech-stack:
  added: [typed save requests, staged validation, snapshot-aware repository writes]
  patterns: [request-level persistence staging, snapshot-owned save APIs, boot-visible snapshot summary]
key-files:
  created:
    - .planning/phases/04-persistent-configuration/04-02-SUMMARY.md
  modified:
    - app/src/app/bootstrap.c
    - app/src/persistence/persistence.c
    - app/src/persistence/persistence.h
    - app/src/persistence/persistence_types.h
key-decisions:
  - "Future writers save through request-shaped APIs tied to `persisted_config`, so callers never author raw schema-versioned NVS blobs."
  - "Persistence stages candidate auth/action/relay/schedule updates in RAM, validates unique IDs and schedule references, then commits only the requested sections."
  - "Boot logs summarize the resolved snapshot with action/schedule counts and relay intent without exposing credentials."
patterns-established:
  - "Snapshot-aware repositories: save helpers update durable storage and the in-memory `persisted_config` together."
  - "Request-level validation: action and schedule updates are checked for self-consistent references before any write."
  - "Auth reseed guardrail: blank or invalid stored credentials are treated as invalid and reseeded from build-time config."
requirements-completed: [CFG-01, CFG-02, CFG-03, CFG-04]
duration: 6 min
completed: 2026-03-09
---

# Phase 4 Plan 2: Persistent Repositories Summary

**Typed persistence save APIs with auth reseed enforcement, staged action/schedule validation, and boot-visible resolved snapshot reporting**

## Performance

- **Duration:** 6 min (elapsed from the first `04-02` task commit to plan completion)
- **Started:** 2026-03-09T08:31:37Z
- **Completed:** 2026-03-09T08:38:02Z
- **Tasks:** 3
- **Files modified:** 4

## Accomplishments

- Fixed the auth repository so blank stored credentials no longer bypass the configured reseed path.
- Added typed save request contracts plus staged validation that rejects duplicate IDs and dangling schedule-to-action references before section writes.
- Kept persistence centralized by updating the in-memory `persisted_config` snapshot alongside validated saves and logging a concise boot summary of the resolved snapshot.

## Task Commits

Each task was committed atomically:

1. **Task 1: Implement the auth repository and known-credential reseed path** - `18aa8b9` (`fix`)
2. **Task 2: Implement relay/action and schedule repositories with staged validation** - `837d64b` (`feat`)
3. **Task 3: Load the resolved snapshot into app context and prove the build** - `bc34c35` (`feat`)

## Files Created/Modified

- `app/src/app/bootstrap.c` - Logs explicit auth reseed outcomes and a structured summary of the resolved persisted snapshot.
- `app/src/persistence/persistence.h` - Exposes snapshot-aware load/save APIs built around typed request shapes instead of raw section blobs.
- `app/src/persistence/persistence.c` - Stages save requests against the current snapshot, validates cross-section references, and keeps the in-memory snapshot synchronized after writes.
- `app/src/persistence/persistence_types.h` - Defines save request shapes for auth, actions, relay, and schedules separately from the persisted storage schema.

## Decisions Made

- Save APIs now accept request shapes without `schema_version` fields so canonicalization stays inside the persistence repository.
- Action and schedule records must have unique IDs so future references remain deterministic.
- Failed writes trigger a snapshot reload so `app_context` stays synchronized with durable storage.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Blank stored auth credentials blocked the reseed path**
- **Found during:** Task 1 (Implement the auth repository and known-credential reseed path)
- **Issue:** Persisted auth blobs with empty username/password fields were treated as valid, so the configured operator credentials would not be restored.
- **Fix:** Required non-empty credential strings and made boot logs identify configured-credential reseeds explicitly.
- **Files modified:** `app/src/persistence/persistence.c`, `app/src/app/bootstrap.c`
- **Verification:** Task 1 `rg` check and final `./scripts/build.sh`
- **Committed in:** `18aa8b9`

**2. [Rule 2 - Missing Critical Functionality] Save requests now reject ambiguous IDs and dangling schedule references before section writes**
- **Found during:** Task 2 (Implement relay/action and schedule repositories with staged validation)
- **Issue:** Raw section saves could persist duplicate action/schedule IDs or accept schedule writes without validating against the active action catalog.
- **Fix:** Added typed request shapes, staged validation against the current snapshot, unique-ID checks, and schedule reference validation before writes.
- **Files modified:** `app/src/persistence/persistence.h`, `app/src/persistence/persistence.c`, `app/src/persistence/persistence_types.h`
- **Verification:** Task 2 `rg` check and final `./scripts/build.sh`
- **Committed in:** `837d64b`

---

**Total deviations:** 2 auto-fixed (1 Rule 1 bug, 1 Rule 2 missing critical functionality)
**Impact on plan:** All auto-fixes tightened correctness inside the planned repository work. No scope creep was introduced.

## Issues Encountered

- `./scripts/build.sh` still emits the pre-existing MBEDTLS Kconfig warnings already logged in `.planning/phases/04-persistent-configuration/deferred-items.md`; the build completed successfully.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `04-03` can now exercise corrupt/incompatible section fallbacks against save APIs that preserve reference integrity.
- Future auth, action, relay, and scheduler phases can call snapshot-aware `persistence_store_save_*` entrypoints without manipulating raw schema versions or NVS blobs.
- No new blocker was introduced by `04-02`; the remaining MBEDTLS warnings are unchanged and remain out of scope for this plan.

## Self-Check

**PASSED**

- FOUND: `.planning/phases/04-persistent-configuration/04-02-SUMMARY.md`
- FOUND: `18aa8b9`
- FOUND: `837d64b`
- FOUND: `bc34c35`
