---
phase: 7
slug: scheduling
status: draft
nyquist_compliant: true
wave_0_complete: true
created: 2026-03-10
---

# Phase 7 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | other — build-first Zephyr validation plus blocking browser/curl/device scheduler sign-off |
| **Config file** | none — existing scripts, board overlay, and app config |
| **Quick run command** | `./scripts/build.sh` |
| **Full suite command** | `./scripts/validate.sh` |
| **Estimated runtime** | ~30 seconds |

---

## Sampling Rate

- **After every task commit:** Run `./scripts/build.sh`
- **After every plan wave:** Run `./scripts/validate.sh`
- **Before `$gsd-verify-work`:** Full automated validation must be green, then complete the blocking browser/curl/device scheduling scenarios on real hardware
- **Max feedback latency:** 45 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 07-01-01 | 01 | 1 | SCHED-04 | build wiring | `rg -n "scheduler\.c|scheduler_service|clock|trusted|degraded" app/CMakeLists.txt app/Kconfig app/prj.conf app/src/app/app_config.h app/src/scheduler/scheduler.h app/src/scheduler/scheduler.c app/src/app/app_context.h app/src/app/bootstrap.c` | ❌ planned | ⬜ pending |
| 07-01-02 | 01 | 1 | SCHED-01, SCHED-02, SCHED-04 | schema + validation | `rg -n "cron|expression|schedule_id|action_id|enabled|conflict|UTC|day_of_month|day_of_week|layout|relay-on|relay-off" app/src/persistence/persistence_types.h app/src/persistence/persistence.h app/src/persistence/persistence.c app/src/scheduler/scheduler.c` | ❌ planned | ⬜ pending |
| 07-01-03 | 01 | 1 | SCHED-04 | startup semantics | `./scripts/build.sh && rg -n "trusted|baseline|skip missed|future run|problem|automation|UTC|backward|normalized" app/src/scheduler/scheduler.c app/src/app/bootstrap.c app/src/app/app_context.h` | ❌ planned | ⬜ pending |
| 07-02-01 | 02 | 2 | SCHED-04 | time sync + recovery | `rg -n "SNTP|CLOCK_REALTIME|clock|trusted|degraded|RECOVERY|reset trigger|UTC" app/src/scheduler/scheduler.c app/src/scheduler/scheduler.h app/src/app/bootstrap.c app/src/recovery/recovery.c app/src/recovery/recovery.h` | ❌ planned | ⬜ pending |
| 07-02-02 | 02 | 2 | SCHED-03, SCHED-04 | runtime engine | `test -f app/src/scheduler/scheduler.c && rg -n "next_run|evaluate|minute|recompute|clock|history|UTC|normalized" app/src/scheduler/scheduler.c app/src/scheduler/scheduler.h app/src/app/app_context.h app/src/app/bootstrap.c` | ❌ planned | ⬜ pending |
| 07-02-03 | 02 | 2 | SCHED-03 | shared execution path | `rg -n "ACTION_DISPATCH_SOURCE_SCHEDULER|action_dispatcher_execute|last_result|scheduled|mutex|lock|desired" app/src/scheduler/scheduler.c app/src/actions/actions.c app/src/actions/actions.h && ! rg -n "gpio_pin_(set|configure)" app/src/scheduler/scheduler.c` | ❌ planned | ⬜ pending |
| 07-02-04 | 02 | 2 | SCHED-03, SCHED-04 | status + history | `./scripts/build.sh && rg -n "clockTrusted|automationActive|nextRun|lastResult|problem|implemented|scheduler|degraded|UTC" app/src/panel/panel_status.c app/src/panel/panel_status.h app/src/scheduler/scheduler.c app/src/app/bootstrap.c` | ❌ planned | ⬜ pending |
| 07-03-01 | 03 | 3 | SCHED-01, SCHED-02 | API routes | `rg -n "/api/schedules|schedule|cron|conflict|friendly|action" app/src/panel/panel_http.c app/src/panel/panel_status.c app/src/panel/panel_status.h app/src/actions/actions.c app/src/actions/actions.h` | ❌ planned | ⬜ pending |
| 07-03-02 | 03 | 3 | SCHED-01, SCHED-02, SCHED-04 | frontend | `rg -n "scheduler|cron|next run|last result|clock|history|create|edit|disable|delete" app/src/panel/assets/index.html app/src/panel/assets/main.js && ! rg -n "action_id" app/src/panel/assets/index.html app/src/panel/assets/main.js` | ✅ existing | ⬜ pending |
| 07-03-03 | 03 | 3 | SCHED-03, SCHED-04 | docs + validation | `bash -n scripts/validate.sh scripts/common.sh scripts/build.sh && rg -n "Phase 7|schedule|cron|clock|missed|resync|curl" README.md scripts/common.sh scripts/validate.sh` | ✅ existing | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [x] Existing script-based validation remains the canonical automated signal
- [x] No new external test framework is required before Phase 7 execution begins
- [x] Every planned task has a concrete structural or build-first automated check
- [x] Final approval remains blocked on a real-device schedule-management and runtime-behavior checkpoint

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Scheduler stays inactive until time is trustworthy and exposes degraded clock state clearly | SCHED-04 | Requires live boot behavior and trusted-time acquisition timing on a real device | Boot the flashed firmware, inspect console and panel status before time trust is established, and confirm automation is inactive while the degraded or syncing state is visible. |
| One failed initial time-sync attempt causes a single recovery reset, then scheduling comes back degraded instead of reboot-looping | SCHED-04 | Requires controlled time-sync failure across two boots and retained recovery breadcrumbs | Deliberately block the initial trusted-time source, confirm one recovery reset occurs, then confirm the next boot leaves scheduling inactive and visibly degraded while the panel remains usable. |
| Operator can create a cron schedule for a relay action and immediately see the next run | SCHED-01 | Requires live panel/API behavior plus device scheduler status | Use the Phase 7 panel or curl flow to create a schedule a few minutes ahead, then confirm the next-run field updates and raw internal action IDs stay hidden from the normal UI. |
| Operator can edit, enable, disable, and delete schedules and keep those changes across reboot | SCHED-02 | Requires persisted state plus authenticated end-to-end flows | Create a schedule, edit it, disable and re-enable it, reboot the device, reopen the schedule view or call `GET /api/schedules`, and confirm edited, disabled, enabled, and deleted state is still correct after the reboot. |
| A manual relay command does not disarm scheduling and the next scheduled run still fires | SCHED-03, SCHED-04 | Requires a real relay actuation path plus a live pending schedule | Create a near-future schedule, issue one manual relay command before the due minute, and confirm the next scheduled run still executes normally and records scheduler as the source for that scheduled transition. |
| A scheduled job fires through the shared action path and updates scheduler last-result visibility | SCHED-03 | Requires a real relay actuation path, live status refresh, and trusted time | Create a near-future schedule for relay on or off, wait for execution, then confirm the physical result, relay status payload, and scheduler last-result or last-run fields all agree. |
| Missed jobs are skipped across reboot or time unavailability and major clock correction only recomputes future runs | SCHED-04 | Requires device clock transitions and reboot timing that build checks cannot simulate faithfully | Reboot the device or force time to become untrusted around a scheduled minute, then restore trusted time and confirm the missed run is not replayed and only future runs are rescheduled. |
| Conflicting schedules are rejected and recent scheduler problems remain visible | SCHED-01, SCHED-02, SCHED-04 | Requires operator-visible validation and status rendering, not just internal return codes | Attempt to save two enabled schedules that would drive opposing relay states in the same minute, confirm the save is rejected, and confirm the scheduler problem or validation feedback is visible in the panel or status output. |

*If none: "All phase behaviors have automated verification."*

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or existing Wave 0 infrastructure
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all missing framework dependencies
- [x] No watch-mode flags
- [x] Feedback latency < 45s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** approved on 2026-03-10
