---
phase: 01-foundation-refactor
plan: "01"
subsystem: infra
tags: [zephyr, nrf7002, cmake, wifi, module-layout]
requires: []
provides:
  - typed app config and application context contracts
  - explicit networking runtime state and Wi-Fi lifecycle interfaces
  - placeholder subsystem header homes for future firmware responsibilities
  - Zephyr build wiring for `app/src` include-based multi-file growth
affects: [02-wi-fi-supervision, 03-recovery-watchdog, 04-persistent-configuration, 05-local-control-panel, 06-action-engine-relay-control, 07-scheduling, 08-ota-lifecycle]
tech-stack:
  added: []
  patterns: [responsibility-first subsystem folders, typed shared firmware contracts, explicit CMake source lists]
key-files:
  created: [app/src/app/app_context.h, app/src/network/network_state.h, app/src/network/wifi_lifecycle.h, app/src/recovery/recovery.h, app/src/panel/panel_http.h, .planning/phases/01-foundation-refactor/deferred-items.md]
  modified: [app/CMakeLists.txt]
key-decisions:
  - "The top-level app context owns configuration and networking runtime state by value."
  - "Future subsystem homes stay header-only until their own phases add real behavior."
  - "Build growth uses explicit source and include registration instead of source discovery."
patterns-established:
  - "Subsystem folders under app/src map directly to long-term firmware ownership boundaries."
  - "Cross-module interaction should flow through typed headers instead of file-local globals."
requirements-completed: [PLAT-01, PLAT-03]
duration: 3 min
completed: 2026-03-08
---

# Phase 1 Plan 1: Foundation Refactor Summary

**Typed application and networking contracts with explicit subsystem header homes and multi-file Zephyr build wiring**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-08T17:44:48Z
- **Completed:** 2026-03-08T17:48:00Z
- **Tasks:** 3
- **Files modified:** 15

## Accomplishments

- Defined typed contracts for app config, app context, network runtime state, Wi-Fi lifecycle, and reachability ownership.
- Created clear header homes for recovery, panel/auth, actions, scheduler, persistence, OTA, and relay work without inventing runtime behavior.
- Updated the app build definition to use an explicit `src` root for future extracted firmware modules and shared headers.

## Task Commits

Each task was committed atomically:

1. **Task 1: Create typed app and networking contracts** - `5086e51` (feat)
2. **Task 2: Establish future subsystem homes without fake implementation** - `2ff616b` (feat)
3. **Task 3: Update build wiring for multi-file growth** - `fd03289` (chore)

## Files Created/Modified

- `app/src/app/app_config.h` - Defines immutable Wi-Fi and reachability configuration structs.
- `app/src/app/app_context.h` - Composes the top-level application context from config and network runtime state.
- `app/src/app/bootstrap.h` - Declares the bootstrap boundary that will replace `main.c` ownership in later plans.
- `app/src/network/network_state.h` - Defines subsystem-owned Wi-Fi/runtime state instead of hidden file-scope globals.
- `app/src/network/wifi_lifecycle.h` - Declares the Phase 1 Wi-Fi lifecycle API surface.
- `app/src/network/reachability.h` - Declares the reachability check contract.
- `app/src/recovery/recovery.h` - Reserves the recovery subsystem boundary.
- `app/src/panel/panel_http.h` - Reserves the HTTP panel delivery boundary.
- `app/src/panel/panel_auth.h` - Reserves the panel authentication boundary.
- `app/src/actions/actions.h` - Reserves the action dispatch boundary.
- `app/src/scheduler/scheduler.h` - Reserves the scheduler boundary.
- `app/src/persistence/persistence.h` - Reserves the persistence boundary.
- `app/src/ota/ota.h` - Reserves the OTA lifecycle boundary.
- `app/src/relay/relay.h` - Reserves the relay control boundary.
- `app/CMakeLists.txt` - Moves build wiring to explicit source-root and include-root registration.
- `.planning/phases/01-foundation-refactor/deferred-items.md` - Logs the unrelated pre-existing build blocker discovered during verification.

## Decisions Made

- `struct app_context` owns `struct app_config` and `struct network_runtime_state` directly so future extraction keeps shared state explicit.
- Later subsystems are represented as header-only boundaries for now to avoid speculative scaffolding before their phases land real code.
- `app/CMakeLists.txt` uses an explicit source list and `src` include root so extracted `.c` files can be added incrementally without discovery logic.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

- `./scripts/build.sh` is currently blocked by a pre-existing Kconfig validation failure: `app/wifi.secrets.conf` assigns `APP_ADMIN_USERNAME` and `APP_ADMIN_PASSWORD`, but those symbols are undefined in `app/Kconfig`. This plan did not touch either file, so the issue was logged in `.planning/phases/01-foundation-refactor/deferred-items.md` instead of being fixed here.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- The firmware tree now has durable homes for current and future subsystem responsibilities under `app/src`.
- Shared app and networking contracts are explicit enough for the next plan to move runtime behavior out of `main.c`.
- Full Zephyr build verification still needs the pre-existing Kconfig/secrets mismatch resolved before compilation can proceed end-to-end.

## Self-Check: PASSED

FOUND: `app/src/app/app_context.h`
FOUND: `app/src/network/network_state.h`
FOUND: `app/src/network/wifi_lifecycle.h`
FOUND: `app/src/recovery/recovery.h`
FOUND: `app/src/panel/panel_http.h`
FOUND: `.planning/phases/01-foundation-refactor/deferred-items.md`
FOUND: `app/CMakeLists.txt`
FOUND: `5086e51`
FOUND: `2ff616b`
FOUND: `fd03289`
