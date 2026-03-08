---
phase: 03-recovery-watchdog
plan: "01"
subsystem: infra
tags: [zephyr, watchdog, hwinfo, recovery, wifi]

requires:
  - phase: 02-network-supervision
    provides: network supervisor connectivity states, retry policy, and last_failure reporting
provides:
  - recovery manager with hardware-watchdog liveness supervision
  - boot and runtime progress reporting wired through bootstrap and network supervision
  - retained reset breadcrumb handling with cooldown and stable-window clearing
affects: [03-02-validation, ota-lifecycle, local-panel-status]

tech-stack:
  added: [Zephyr watchdog API, Zephyr HWINFO reset-cause API]
  patterns: [recovery-manager-owned watchdog feeding, retained .noinit recovery breadcrumbs]

key-files:
  created: [app/src/recovery/recovery.c]
  modified: [app/CMakeLists.txt, app/Kconfig, app/prj.conf, app/src/app/app_config.h, app/src/app/app_context.h, app/src/app/bootstrap.c, app/src/network/network_state.h, app/src/network/network_supervisor.c, app/src/network/network_supervisor.h, app/src/recovery/recovery.h]

key-decisions:
  - "Keep watchdog ownership inside recovery_manager and feed it only after evaluating boot/runtime liveness windows."
  - "Treat LAN-up upstream degradation as acceptable runtime stability while requiring patience windows before confirmed stuck escalation."
  - "Persist only the latest recovery breadcrumb via .noinit plus hwinfo reset-cause data instead of introducing a broader persistence layer."

patterns-established:
  - "Recovery manager owns watchdog setup, reset breadcrumbing, and final reboot authority."
  - "Network supervisor reports progress into recovery policy without calling sys_reboot() or watchdog APIs."

requirements-completed: [REC-01, REC-02]

duration: 5m
completed: 2026-03-09
---

# Phase 3 Plan 1: Recovery Watchdog Supervision Summary

**Recovery-managed boot and network liveness with hardware watchdog gating, retained reset breadcrumbs, and anti-thrash cooldown policy.**

## Performance

- **Duration:** 5m
- **Started:** 2026-03-08T20:48:39Z
- **Completed:** 2026-03-08T20:53:44Z
- **Tasks:** 3
- **Files modified:** 11

## Accomplishments

- Added a real `recovery_manager` module that owns watchdog setup, liveness evaluation, and reboot escalation.
- Wired boot and network supervision to report meaningful progress through recovery without leaking reboot policy into networking.
- Preserved the latest recovery-reset breadcrumb across reboot and added cooldown/stable-window logic to avoid immediate reset thrash.

## Task Commits

Each task was committed atomically:

1. **Task 1: Add the recovery manager contract and build-time watchdog policy** - `d09d990` (feat)
2. **Task 2: Wire supervised progress and escalation boundaries through boot and network supervision** - `3fb3848` (feat)
3. **Task 3: Add retained reset-cause handling and anti-thrash stable-window behavior** - `991ebd9` (feat)

**Plan metadata:** pending final docs commit at summary creation time

## Files Created/Modified

- `app/src/recovery/recovery.c` - Implements watchdog arming, liveness evaluation, retained reset breadcrumbs, and cooldown/stable-window clearing.
- `app/src/recovery/recovery.h` - Declares the recovery-manager contract and retained reset-cause surface.
- `app/src/app/bootstrap.c` - Loads recovery policy, initializes recovery before startup supervision, and logs preserved reset cause details.
- `app/src/app/app_context.h` - Extends the shared application context with recovery-manager ownership.
- `app/src/network/network_state.h` - Carries the recovery-manager pointer inside network runtime state.
- `app/src/network/network_supervisor.c` - Reports connectivity/failure/startup progress into recovery while staying free of reboot/watchdog calls.
- `app/src/network/network_supervisor.h` - Updates supervisor initialization to accept recovery wiring.
- `app/src/app/app_config.h` - Adds the recovery/watchdog policy structure to the build-time app configuration.
- `app/Kconfig` - Adds watchdog timeout, degraded patience, stable-window, and cooldown knobs.
- `app/prj.conf` - Enables watchdog, reboot, and reset-cause support required by the recovery path.
- `app/CMakeLists.txt` - Compiles the new recovery module.

## Decisions Made

- Used hardware watchdog supervision inside `recovery_manager` instead of any idle-loop feed pattern in `main.c`.
- Kept Phase 2 retry/reachability behavior inside the network supervisor and limited recovery integration to progress reporting and escalation boundaries.
- Considered `NETWORK_CONNECTIVITY_HEALTHY` and `NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED` acceptable stable states so upstream churn alone does not trigger resets.
- Used a `.noinit` breadcrumb plus `hwinfo_get_reset_cause()` to retain only the most recent recovery-reset explanation across reboot.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking Issue] Enabled reboot support for recovery escalation**
- **Found during:** Task 2 (Wire supervised progress and escalation boundaries through boot and network supervision)
- **Issue:** Linking failed once boot/runtime wiring made `sys_reboot()` reachable because `CONFIG_REBOOT` was not enabled.
- **Fix:** Added `CONFIG_REBOOT=y` to `app/prj.conf` so the recovery manager can perform cold reboots.
- **Files modified:** `app/prj.conf`
- **Verification:** `./scripts/build.sh`
- **Committed in:** `3fb3848` (part of task commit)

---

**Total deviations:** 1 auto-fixed (1x Rule 3)
**Impact on plan:** Required for correctness once recovery escalation became live. No scope creep.

## Issues Encountered

- The repo-level `./scripts/validate.sh` automated path passed, but its manual follow-up text still reflects the prior Phase 2 device checklist. That documentation gap remains scoped for `03-02` validation work.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Recovery/watchdog policy is now implemented behind dedicated subsystem boundaries and ready for device-level validation.
- `03-02` can focus on validation surfaces, manual stuck-state scenarios, and documenting the healthy/degraded/reset verification flow on hardware.

## Self-Check

PASSED - verified recovery files exist and task commits `d09d990`, `3fb3848`, and `991ebd9` resolve successfully.

---
*Phase: 03-recovery-watchdog*
*Completed: 2026-03-09*
