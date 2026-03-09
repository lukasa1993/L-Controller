---
phase: 05-local-control-panel
plan: "02"
subsystem: auth
tags: [zephyr, http-server, auth, session, json, status-api]
requires:
  - phase: 05-01
    provides: Zephyr HTTP panel shell, static asset delivery, and app-owned panel startup wiring
  - phase: 04
    provides: persisted single-user credential source plus relay and schedule status data
  - phase: 03
    provides: recovery reset-cause reporting used by the protected status payload
provides:
  - mutex-guarded RAM-only sid session service with cooldown-backed wrong-password throttling
  - exact login, logout, and session-check routes with cookie issuance and stale-cookie cleanup
  - protected aggregate /api/status payload for device, network, recovery, relay, scheduler, and update surfaces
affects: [phase-05-03, phase-06, phase-07, phase-08, panel-ux]
tech-stack:
  added: [Zephyr JSON parsing, RAM session table, protected aggregate status API]
  patterns: [exact-path auth routes, server-owned cookie sessions, single aggregate read-only panel status endpoint]
key-files:
  created:
    - .planning/phases/05-local-control-panel/05-02-SUMMARY.md
    - app/src/panel/panel_auth.c
    - app/src/panel/panel_status.c
    - app/src/panel/panel_status.h
  modified:
    - app/CMakeLists.txt
    - app/Kconfig
    - app/prj.conf
    - app/src/app/app_config.h
    - app/src/app/app_context.h
    - app/src/app/bootstrap.c
    - app/src/panel/panel_auth.h
    - app/src/panel/panel_http.c
key-decisions:
  - "Panel auth uses a fixed-size mutex-guarded RAM session table with opaque sid cookies so sessions survive refresh/navigation but are invalidated on reboot."
  - "Phase 5 exposes only the exact auth trio plus one protected aggregate /api/status endpoint; control, configuration, scheduling, and update routes stay intentionally unavailable."
  - "Unauthorized session and status requests clear stale sid cookies so browsers recover cleanly after reboot or manual logout."
patterns-established:
  - "Auth boundary: panel_http owns exact route callbacks while panel_auth owns credential validation, cooldown policy, and session storage."
  - "Protected operator data flows through one read-only aggregate status payload instead of multiple micro-endpoints."
  - "New panel modules must be registered explicitly in app/CMakeLists.txt to keep the Zephyr build surface in sync with subsystem additions."
requirements-completed: [AUTH-01, AUTH-02, AUTH-03, PANEL-01, PANEL-02, PANEL-03]
duration: 5 min
completed: 2026-03-09
---

# Phase 5 Plan 2: Auth and Status API Summary

**Single-user RAM session auth with exact login/logout/session routes and a protected aggregate status API for the Phase 5 operator panel.**

## Performance

- **Duration:** 5 min (elapsed from the first `05-02` task commit to summary creation)
- **Started:** 2026-03-09T15:52:50Z
- **Completed:** 2026-03-09T15:58:33Z
- **Tasks:** 3
- **Files modified:** 11

## Accomplishments

- Added the `panel_auth` service with opaque `sid` tokens, concurrent RAM-backed sessions, reboot-bounded invalidation, and a wrong-password cooldown.
- Exposed only the exact Phase 5 auth routes—`POST /api/auth/login`, `POST /api/auth/logout`, and `GET /api/auth/session`—with `HttpOnly` and `SameSite=Strict` cookies.
- Added the protected read-only `GET /api/status` endpoint so the frontend can render current device, network, recovery, relay, scheduler, and update state from one guarded payload.

## Task Commits

Each task was completed atomically:

1. **Task 1: Implement the panel auth service and reboot-bounded RAM session policy** - `b273865` (`feat`)
2. **Task 2: Add exact auth routes, cookie handling, and protected-route gating** - `f434172` (`feat`)
3. **Task 3: Implement the protected aggregate status API and keep future surfaces read-only** - `24a5ad0` (`feat`)

**Plan metadata:** pending final docs commit at summary creation time

## Files Created/Modified

- `app/CMakeLists.txt` - Registers the new panel auth and status modules in the Zephyr build as the panel subsystem grows.
- `app/Kconfig` - Adds the panel session-capacity, failure-limit, and cooldown configuration knobs.
- `app/prj.conf` - Enables Zephyr JSON parsing for the login request body while preserving the panel HTTP server envelope.
- `app/src/app/app_config.h` - Extends the panel config contract with auth-session policy settings.
- `app/src/app/app_context.h` - Adds owned `panel_auth_service` state to the app context.
- `app/src/app/bootstrap.c` - Initializes panel auth immediately after the persisted config loads.
- `app/src/panel/panel_auth.h` - Defines the RAM session and login-result contracts used by HTTP routing.
- `app/src/panel/panel_auth.c` - Implements session allocation, credential validation, cooldown logic, and logout/session lookup.
- `app/src/panel/panel_http.c` - Adds the exact auth trio plus the protected `/api/status` route and cookie-response helpers.
- `app/src/panel/panel_status.h` - Declares the status translation contract.
- `app/src/panel/panel_status.c` - Renders the aggregate read-only JSON payload from app-owned network, recovery, relay, scheduler, and update state.

## Decisions Made

- Kept the auth/session model server-owned and RAM-only so browser refresh/navigation remains predictable without introducing persistent tokens or local storage.
- Limited Phase 5 API scope to the exact auth trio plus a single aggregate status endpoint, keeping relay, scheduler, update, and config control routes unavailable until later phases.
- Cleared stale `sid` cookies on unauthorized protected requests to make reboot-bounded invalidation recover cleanly in browsers.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Registered `panel_auth.c` in the build surface**
- **Found during:** Task 1 (Implement the panel auth service and reboot-bounded RAM session policy)
- **Issue:** The plan added a new `panel_auth.c` module, but `app/CMakeLists.txt` still compiled only the existing panel source set.
- **Fix:** Added `app/src/panel/panel_auth.c` to `APP_SOURCES` so the new auth subsystem was part of the firmware build.
- **Files modified:** `app/CMakeLists.txt`
- **Verification:** `./scripts/build.sh` completed successfully after the auth service landed.
- **Committed in:** `b273865` (Task 1 commit)

**2. [Rule 3 - Blocking] Registered `panel_status.c` in the build surface**
- **Found during:** Task 3 (Implement the protected aggregate status API and keep future surfaces read-only)
- **Issue:** The new `panel_status.c` translation layer would not compile or link without explicit registration in the app source list.
- **Fix:** Added `app/src/panel/panel_status.c` to `APP_SOURCES` when wiring the protected `/api/status` route.
- **Files modified:** `app/CMakeLists.txt`
- **Verification:** `./scripts/build.sh` completed successfully with the new status module and route.
- **Committed in:** `24a5ad0` (Task 3 commit)

---

**Total deviations:** 2 auto-fixed (2 Rule 3 blocking fixes)
**Impact on plan:** No scope creep. Both fixes were required to compile the planned auth and status modules.

## Issues Encountered

- `./scripts/build.sh` still emits the pre-existing MBEDTLS Kconfig warnings already tracked in project state, but the Phase 5 auth/status build completed successfully.

## User Setup Required

None - the auth/session/status implementation for this plan is fully automated.

## Next Phase Readiness

- `05-03` can now build the login-first frontend against stable server contracts: `GET /api/auth/session`, `POST /api/auth/login`, `POST /api/auth/logout`, and `GET /api/status`.
- Relay, scheduler, and update cards can consume the aggregate payload immediately while staying read-only or placeholder-only in the UI.
- No new blocker was introduced by `05-02`; the only remaining Phase 5 blocker is the final browser/device checkpoint in `05-03`.

## Self-Check: PASSED

- Verified `.planning/phases/05-local-control-panel/05-02-SUMMARY.md` exists.
- Verified prior task commits `b273865`, `f434172`, and `24a5ad0` exist in git history.
