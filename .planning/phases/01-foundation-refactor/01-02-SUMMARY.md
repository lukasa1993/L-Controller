---
phase: 01-foundation-refactor
plan: "02"
subsystem: infra
tags: [zephyr, nrf7002, wifi, bootstrap, refactor]
requires:
  - phase: 01-01
    provides: typed app/network contracts and explicit multi-file build scaffolding
provides:
  - bootstrap-owned Wi-Fi bring-up sequencing through `app_boot`
  - subsystem-owned Wi-Fi lifecycle callbacks, connect flow, and DHCP wait state
  - extracted reachability probe and composition-only `main.c`
  - placeholder admin Kconfig symbols that keep the local secrets overlay buildable
affects: [01-03, 02-wi-fi-supervision, 03-recovery-watchdog, 05-local-control-panel]
tech-stack:
  added: []
  patterns: [composition-root main, bootstrap-owned orchestration, subsystem-owned network runtime state]
key-files:
  created: [app/src/network/wifi_lifecycle.c, app/src/app/bootstrap.c, app/src/network/reachability.c, .planning/phases/01-foundation-refactor/01-02-SUMMARY.md]
  modified: [app/src/network/wifi_lifecycle.h, app/src/app/bootstrap.h, app/src/main.c, app/CMakeLists.txt, app/Kconfig, .planning/phases/01-foundation-refactor/deferred-items.md]
key-decisions:
  - "Bootstrap now owns config loading, interface discovery, and the ready-path orchestration behind `app_boot`."
  - "Wi-Fi callback and DHCP state stay inside `struct network_runtime_state` and are manipulated through lifecycle APIs instead of `main.c` globals."
  - "The existing local secrets overlay stays valid by defining placeholder admin Kconfig symbols now, rather than editing user-specific secret files."
patterns-established:
  - "`main.c` should stay a composition root that boots subsystems and then idles."
  - "Runtime networking behavior should flow through bootstrap/lifecycle/reachability modules operating on typed shared context."
requirements-completed: [PLAT-02, PLAT-03]
duration: 6 min
completed: 2026-03-08
---

# Phase 1 Plan 2: Foundation Refactor Summary

**Bootstrap-owned Wi-Fi bring-up with extracted lifecycle and reachability modules behind a composition-only `main.c`**

## Performance

- **Duration:** 6 min
- **Started:** 2026-03-08T17:55:19Z
- **Completed:** 2026-03-08T18:00:56Z
- **Tasks:** 3
- **Files modified:** 9

## Accomplishments

- Moved Wi-Fi callback handling, connect/disconnect requests, DHCP waiting, and timeout management into `app/src/network/wifi_lifecycle.c`.
- Added `app/src/app/bootstrap.c` and `app/src/network/reachability.c` so the ready path flows through explicit subsystem entrypoints instead of `main.c`.
- Reduced `app/src/main.c` to firmware composition only and updated `app/CMakeLists.txt` so the extracted modules build in the normal Zephyr path.

## Task Commits

Each task was committed atomically:

1. **Task 1: Move Wi-Fi lifecycle ownership into the networking subsystem** - `b054608` (feat)
2. **Task 2: Extract reachability and bootstrap sequencing** - `411656f` (feat)
3. **Task 3: Reduce `main.c` to composition root and restore the build** - `540316d` (refactor)

## Files Created/Modified

- `app/src/network/wifi_lifecycle.c` - Owns Wi-Fi readiness callbacks, connect/disconnect flow, DHCP binding, and wait sequencing.
- `app/src/network/wifi_lifecycle.h` - Exposes lifecycle init, security parsing, connect, and wait APIs to bootstrap.
- `app/src/app/bootstrap.c` - Loads app config from Kconfig and orchestrates interface discovery through `APP_READY`.
- `app/src/app/bootstrap.h` - Declares the single bootstrap entrypoint used by the composition root.
- `app/src/network/reachability.c` - Runs the post-connect DNS/TCP reachability probe.
- `app/src/main.c` - Now boots the app context and idles without direct networking logic.
- `app/CMakeLists.txt` - Compiles the extracted bootstrap and networking source files.
- `app/Kconfig` - Defines placeholder admin symbols so the existing secrets overlay no longer aborts builds.
- `.planning/phases/01-foundation-refactor/deferred-items.md` - Marks the earlier plan’s build blocker as resolved in `01-02`.

## Decisions Made

- `app_boot()` owns Kconfig-derived configuration loading so `main.c` does not re-acquire subsystem knowledge.
- `struct network_runtime_state` remains the single mutable container for Wi-Fi association, disconnect, and DHCP readiness state.
- The local secrets overlay mismatch is fixed in tracked Kconfig rather than by mutating `app/wifi.secrets.conf`, which keeps user-specific secrets untouched.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking Issue] Restored Kconfig validation for the local secrets overlay**
- **Found during:** Task 1 (Move Wi-Fi lifecycle ownership into the networking subsystem)
- **Issue:** `./scripts/build.sh` aborted because `app/wifi.secrets.conf` assigned `APP_ADMIN_USERNAME` and `APP_ADMIN_PASSWORD`, but `app/Kconfig` did not define those symbols.
- **Fix:** Added placeholder string Kconfig definitions so the existing secrets overlay remains valid until later auth phases consume those settings.
- **Files modified:** `app/Kconfig`
- **Verification:** `./scripts/build.sh`
- **Committed in:** `b054608` (part of task commit)

---

**Total deviations:** 1 auto-fixed (1 Rule 3 build blocker)
**Impact on plan:** Required to satisfy the plan’s build verification gate; no behavior or scope expansion beyond restoring the existing build path.

## Issues Encountered

- Zephyr still emits non-fatal MBEDTLS Kconfig warnings during configuration, but the app configures, builds, and links successfully. Those warnings were pre-existing and did not block this plan after the secrets-symbol fix.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- `main.c` is now a composition-only firmware root, so the next plan can focus on validation and hardware sign-off instead of structural extraction.
- Bootstrap, Wi-Fi lifecycle, and reachability behavior now compile as separate modules behind typed headers and shared context.
- The previous deferred build blocker is resolved; only non-fatal MBEDTLS warnings remain visible during build output.

## Self-Check: PASSED

FOUND: `.planning/phases/01-foundation-refactor/01-02-SUMMARY.md`
FOUND: `app/src/network/wifi_lifecycle.c`
FOUND: `app/src/app/bootstrap.c`
FOUND: `app/src/network/reachability.c`
FOUND: `app/src/main.c`
FOUND: `app/CMakeLists.txt`
FOUND: `app/Kconfig`
FOUND: `b054608`
FOUND: `411656f`
FOUND: `540316d`
