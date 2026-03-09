---
phase: 3
slug: recovery-watchdog
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-09
---

# Phase 3 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | other — build/script smoke plus targeted static checks |
| **Config file** | none — existing scripts + Zephyr app config |
| **Quick run command** | `./scripts/build.sh` |
| **Full suite command** | `./scripts/validate.sh` |
| **Estimated runtime** | ~180 seconds |

---

## Sampling Rate

- **After every task commit:** Run `./scripts/build.sh`
- **After every plan wave:** Run `./scripts/validate.sh`
- **Before `$gsd-verify-work`:** Full automated validation must be green, then complete manual device scenarios for healthy stability, long-degraded escalation, watchdog reset, preserved reset-cause reporting, and anti-thrash behavior
- **Max feedback latency:** 180 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | REC-01, REC-02 | structure + config | `test -f app/src/recovery/recovery.c && rg -n "APP_RECOVERY_|CONFIG_WATCHDOG|CONFIG_HWINFO" app/Kconfig app/prj.conf && rg -n "struct app_recovery_config|recovery_manager" app/src/app/app_config.h app/src/recovery/recovery.h app/src/recovery/recovery.c` | ❌ planned | ⬜ pending |
| 03-01-02 | 01 | 1 | REC-01 | integration | `rg -n "recovery_manager_" app/src/app/bootstrap.c app/src/network/network_supervisor.c app/src/recovery/recovery.c && rg -n "startup_outcome_sem|last_failure|connectivity_state" app/src/network/network_state.h app/src/network/network_supervisor.c && ! rg -n "sys_reboot|wdt_" app/src/network/network_supervisor.c` | ❌ planned | ⬜ pending |
| 03-01-03 | 01 | 1 | REC-02 | reset semantics | `rg -n "wdt_(install_timeout|setup|feed)|hwinfo_get_reset_cause|__noinit|stable|cooldown" app/src/recovery/recovery.c app/src/recovery/recovery.h app/src/app/bootstrap.c` | ❌ planned | ⬜ pending |
| 03-02-01 | 02 | 2 | REC-01, REC-02 | docs + shell | `bash -n scripts/validate.sh scripts/common.sh && rg -n "watchdog|recovery|reset cause|validate\.sh|flash\.sh|console\.sh" README.md scripts/common.sh scripts/validate.sh` | ✅ existing | ⬜ pending |
| 03-02-02 | 02 | 2 | REC-01, REC-02 | docs + checklist | `bash -n scripts/validate.sh scripts/common.sh && rg -n "build\.sh|stable|degraded|reset|watchdog" README.md scripts/validate.sh scripts/common.sh` | ✅ existing | ⬜ pending |
| 03-02-03 | 02 | 2 | REC-01, REC-02 | manual checkpoint | `./scripts/flash.sh` then `./scripts/console.sh` and confirm healthy, long-degraded escalation, reboot breadcrumb, watchdog-reset, and anti-thrash markers | ✅ existing | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing `./scripts/build.sh` and `./scripts/validate.sh` already provide the automated feedback loop for this phase
- [x] No new framework install is required before execution begins
- [x] No `MISSING` automated references remain in the phase task map
- [x] Existing `./scripts/flash.sh` and `./scripts/console.sh` cover the hardware validation gate

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Healthy boot does not trigger false recovery escalation | REC-01, REC-02 | Requires real board timing, Wi-Fi environment, and serial observation over a stable interval | Flash with `./scripts/flash.sh`, open `./scripts/console.sh`, confirm `APP_READY`, healthy-state logs, watchdog/recovery initialization logs, and no unexpected recovery-triggered reset during the configured stable window. |
| Long degraded behavior escalates only after the patience window is truly exceeded | REC-02 | Requires real network disruption over time and observation of retry behavior before escalation | Keep the device in a degraded network condition long enough to exceed the configured patience threshold; confirm normal retries happen first and a whole-device reset only occurs after the recovery policy decides progress has stopped or stayed degraded too long. |
| Preserved recovery-reset cause is visible on the next boot | REC-02 | Requires a real reboot and retained memory / reset-cause handling | After a recovery-triggered reset, reconnect to the console and confirm the next boot logs the stored recovery-reset breadcrumb clearly enough to explain the reset path and reason. |
| Watchdog starvation resets the device when feeds stop | REC-01, REC-02 | Requires real watchdog hardware behavior; build logs alone cannot prove expiry semantics | Use the chosen lab method for the phase implementation (for example intentional feed suppression or a controlled halt of the supervised path) and confirm the device resets under watchdog control rather than silently hanging. |
| Repeated persistent faults do not cause rapid reboot loops | REC-02 | Requires cross-boot observation under a persistent fault condition | Recreate or maintain the same fault after the first recovery-triggered reset and confirm the system respects the configured cooldown / stable-window policy instead of thrashing through immediate repeated resets. |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or declared manual sign-off
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 180s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
