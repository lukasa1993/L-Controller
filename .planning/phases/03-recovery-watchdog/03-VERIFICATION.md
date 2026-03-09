---
phase: 03-recovery-watchdog
verification: phase-goal
status: passed
score:
  satisfied: 18
  total: 18
  note: "All Phase 3 must-haves are satisfied in the current tree. Automated validation still emits the previously known non-blocking MBEDTLS Kconfig warnings during configuration, but the build and validation flow complete successfully."
requirements:
  passed: [REC-01, REC-02]
verified_on: 2026-03-09
automated_checks:
  - bash -n scripts/validate.sh scripts/common.sh
  - ./scripts/validate.sh
  - rg -n "APP_RECOVERY_|CONFIG_WATCHDOG|CONFIG_HWINFO|CONFIG_REBOOT" app/Kconfig app/prj.conf
  - rg -n "struct app_recovery_config|recovery_manager|recovery_reset_cause|stable_window|cooldown" app/src/app/app_config.h app/src/recovery/recovery.h app/src/recovery/recovery.c app/src/app/bootstrap.c
  - rg -n "recovery_manager_|startup_outcome_sem|last_failure|connectivity_state" app/src/app/bootstrap.c app/src/network/network_supervisor.c app/src/network/network_state.h app/src/network/network_supervisor.h
  - '! rg -n "sys_reboot|wdt_" app/src/network/network_supervisor.c'
  - rg -n "wdt_(install_timeout|setup|feed)|hwinfo_get_reset_cause|__noinit|cooldown|stable" app/src/recovery/recovery.c app/src/recovery/recovery.h app/src/app/bootstrap.c
  - rg -n "validate\.sh|build\.sh|flash\.sh|console\.sh|healthy stability|long-degraded escalation|anti-thrash" README.md scripts/validate.sh scripts/common.sh
human_verification_basis:
  source: .planning/phases/03-recovery-watchdog/03-02-SUMMARY.md
  approved_on: 2026-03-09
  markers:
    - Recovery watchdog armed timeout_ms=...
    - Recovery escalation trigger=confirmed-stuck state=... stage=... reason=...
    - Previous recovery reset=confirmed-stuck hw=0x... state=... stage=... reason=...
    - Previous recovery reset=watchdog-starvation hw=0x... state=... stage=... reason=...
    - Recovery incident cleared after stable window ... ms
recommendation: accept_phase_complete
---

# Phase 3 Verification

## Verdict

**Passed.** Phase 3 achieved its goal: the firmware now supervises boot and network liveness through a dedicated recovery manager, feeds the hardware watchdog only while supervised progress remains healthy enough, escalates only confirmed stuck or watchdog-starvation conditions to full reboot, and documents the blocking real-device sign-off path for healthy, degraded, recovery-reset, watchdog-reset, and anti-thrash scenarios.

## Concise Score / Summary

- **Overall:** 18/18 Phase 3 must-have checks are satisfied directly in the current tree.
- **Automated verification:** `bash -n scripts/validate.sh scripts/common.sh` passed, `./scripts/validate.sh` completed successfully on 2026-03-09, and the targeted grep checks confirmed recovery config, watchdog/reset-cause wiring, supervisor integration, and the absence of reboot/watchdog leakage back into `network_supervisor.c`.
- **Hardware verification basis:** `03-02-SUMMARY.md` records a human-approved device run covering healthy stability, long-degraded escalation, preserved reset-breadcrumb visibility, watchdog-reset behavior, and anti-thrash clearing.
- **Non-blocking variance:** Final automated validation still reports the previously known MBEDTLS Kconfig warnings during configuration. They do not prevent `./scripts/build.sh` or `./scripts/validate.sh` from passing and do not contradict the Phase 3 goal.

## Goal Coverage

| Goal clause | Result | Evidence |
|-------------|--------|----------|
| Critical execution paths are supervised by watchdog logic rather than a single generic feed loop | PASS | `recovery_manager_init()` arms the watchdog in `app/src/recovery/recovery.c`, `recovery_manager_startup_begin()` / `recovery_manager_startup_complete()` bracket boot progress, and `network_supervisor.c` reports runtime progress back through `recovery_manager_report_network_progress()` instead of owning watchdog feeds itself. |
| Minor or transient faults do not trigger device reset loops | PASS | Recovery policy keeps ordinary Wi-Fi churn inside the Phase 2 supervisor boundary, treats `NETWORK_CONNECTIVITY_HEALTHY` and `NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED` as stable enough to feed the watchdog, and uses cooldown/stable-window logic before clearing prior incidents or allowing repeat escalation. |
| Confirmed stuck states escalate through a defined recovery path to full restart when necessary | PASS | `recovery.c` logs `Recovery escalation trigger=...`, persists the latest breadcrumb in retained `.noinit` state plus `hwinfo_get_reset_cause()`, and calls `sys_reboot(SYS_REBOOT_COLD)` only after confirmed stuck or watchdog-starvation conditions. |

## Must-Have Coverage

### Plan 03-01 (`REC-01`, `REC-02`)

| Must-have | Result | Evidence |
|-----------|--------|----------|
| Critical execution paths report progress into a dedicated recovery manager, and the watchdog is only fed when those paths remain within allowed progress windows | PASS | `bootstrap.c` initializes and brackets startup through `recovery_manager_*()`, `network_supervisor.c` reports runtime progress into the recovery manager, and `recovery.c` owns `wdt_feed()` behind policy evaluation instead of feeding from idle/bootstrap code. |
| Confirmed long-lived degraded or stalled network supervision is treated as unrecoverable only after explicit patience thresholds, not on every transient Wi-Fi failure | PASS | `recovery.c` evaluates degraded patience and last-progress timestamps before escalation, while `network_supervisor.c` continues to own retry/reachability handling for transient network churn. |
| The device preserves the most recent recovery-reset cause across reboot and avoids immediate reset thrashing before returning to normal supervision | PASS | `recovery.c` stores retained breadcrumb state in `__noinit`, reads hardware reset cause via `hwinfo_get_reset_cause()`, and applies cooldown plus stable-window clearing before considering a prior incident resolved. |
| `app/src/recovery/recovery.c` provides the recovery manager implementation with watchdog and retained reset handling | PASS | File exists at 441 lines, includes `wdt_install_timeout`, `wdt_setup`, `wdt_feed`, retained reset state, and escalation logic. |
| `app/src/recovery/recovery.h` provides the recovery manager contract for init, progress reporting, and reset-cause access | PASS | File exists at 54 lines and declares `recovery_manager_init`, startup/reporting hooks, and `recovery_manager_last_reset_cause`. |
| `app/Kconfig` provides explicit recovery/watchdog timing configuration | PASS | `app/Kconfig` defines `CONFIG_APP_RECOVERY_WATCHDOG_TIMEOUT_MS`, `CONFIG_APP_RECOVERY_DEGRADED_PATIENCE_MS`, `CONFIG_APP_RECOVERY_STABLE_WINDOW_MS`, and `CONFIG_APP_RECOVERY_COOLDOWN_MS`. |
| `app/prj.conf` enables Zephyr features required for watchdog and reset-cause support | PASS | `app/prj.conf` enables `CONFIG_WATCHDOG`, `CONFIG_HWINFO`, and `CONFIG_REBOOT`. |
| Boot initializes recovery before long-lived supervision and logs preserved reset cause | PASS | `bootstrap.c` calls `recovery_manager_init()` before `network_supervisor_init()`, logs prior reset breadcrumbs through `recovery_manager_last_reset_cause()`, and starts startup supervision before network bring-up. |
| Network supervision reports forward progress and degraded incidents into recovery policy | PASS | `network_supervisor.c` routes progress/failure state into `recovery_manager_report_network_progress()` and remains free of direct `sys_reboot()` or watchdog API calls. |
| Recovery policy reads shared runtime state and classifies stuck versus progressing network conditions | PASS | `recovery.c` consumes `network_runtime_state`, `connectivity_state`, and `last_failure` to distinguish healthy/degraded progress from confirmed-stuck escalation. |

### Plan 03-02 (`REC-01`, `REC-02`)

| Must-have | Result | Evidence |
|-----------|--------|----------|
| The repo keeps a single automated validation entrypoint for fast build feedback while clearly separating device-only recovery/watchdog sign-off | PASS | `scripts/validate.sh` still delegates to `./scripts/build.sh` and explicitly prints the separate hardware sign-off steps instead of attempting to simulate them automatically. |
| Phase 3 validation explicitly covers healthy stability, long-degraded escalation, preserved reset-cause reporting, watchdog reset semantics, and anti-thrash behavior | PASS | `README.md`, `scripts/common.sh`, and `scripts/validate.sh` all enumerate the healthy, long-degraded, reboot-breadcrumb, watchdog-reset, and anti-thrash scenarios as required checkpoints. |
| Final phase approval depends on a human-confirmed device run that records the observed recovery-reset and watchdog behavior | PASS | `03-02-SUMMARY.md` records the blocking human checkpoint as approved on 2026-03-09, with the required scenario markers listed as the approval basis. |
| `README.md` documents the Phase 3 validation flow and expected recovery/watchdog scenarios | PASS | `README.md` contains the Phase 3 automated/device split, the required scenario list, and the explicit hardware checklist. |
| `scripts/validate.sh` remains the phase-safe automated validation entrypoint | PASS | `scripts/validate.sh` is 73 lines long, calls `scripts/build.sh`, and keeps device verification as a post-build blocking sign-off. |
| `scripts/common.sh` provides shared markers and reusable device-checklist helpers for recovery sign-off | PASS | `scripts/common.sh` exports the recovery/watchdog marker list and the full Phase 3 device checklist used by both docs and CLI output. |
| `scripts/validate.sh` links the automated path to `scripts/build.sh` | PASS | Validation delegates directly to `run_repo_script scripts/build.sh`, preserving the canonical automated build signal. |
| `README.md` links the documented device validation sequence to `scripts/flash.sh` and `scripts/console.sh` | PASS | The README device-only sign-off section instructs operators to use `./scripts/flash.sh` and `./scripts/console.sh` for the blocking real-hardware run. |

## Requirement Traceability

| Requirement | Result | Evidence |
|-------------|--------|----------|
| `REC-01` — Device supervises critical execution paths with watchdog logic | PASS | Recovery manager owns watchdog setup/feed policy, startup boundaries, and runtime progress integration, while `REQUIREMENTS.md` now marks `REC-01` complete. |
| `REC-02` — Device triggers a full restart only after confirmed unrecoverable subsystem failure or watchdog starvation | PASS | Recovery escalation only occurs after confirmed stuck-state/watchdog conditions, reset breadcrumbs persist across reboot, and the approved hardware checklist explicitly covers recovery-reset and watchdog-reset outcomes. |

## Recommendation

Accept Phase 3 as complete. The roadmap, state, and requirements traceability already reflect completion; this verification report confirms the code and documented device-validation path meet the Phase 3 goal without surfacing unresolved blockers.
