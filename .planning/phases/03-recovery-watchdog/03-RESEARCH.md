# Phase 3: Recovery & Watchdog - Research

**Researched:** 2026-03-09
**Status:** Ready for planning

<focus>
## Research Goal

Determine the cleanest way to add conservative recovery supervision on top of the existing Phase 2 network supervisor so the firmware supervises the real boot/runtime critical paths with watchdog logic, distinguishes ordinary degraded connectivity from true no-progress conditions, and escalates to a full-device restart only after confirmed unrecoverable failure or watchdog starvation.

</focus>

<findings>
## Key Findings

### Phase 3 should supervise the existing network-supervisor path, not invent a second recovery state machine

- `app/src/app/bootstrap.c` already owns startup composition and blocks inside `network_supervisor_start()` until the supervisor reaches a bounded startup outcome.
- `app/src/network/network_supervisor.c` already owns retry windows, bring-up deadlines, reachability checks, connectivity-state transitions, and structured `last_failure` recording.
- `app/src/network/wifi_lifecycle.c` already keeps low-level callback work lightweight by updating state, releasing semaphores, and rescheduling `supervisor_retry_work` instead of embedding policy.
- That means the current product's real critical execution path is already explicit: boot through `app_boot()`, bounded startup in `network_supervisor_start()`, then runtime supervision through `supervisor_retry_work` plus NET_MGMT callback edges.
- Phase 3 should supervise that path. It should not create a second network-policy layer inside `app/src/recovery/`.

### `main.c` is no longer a valid watchdog owner

- `app/src/main.c` now calls `app_boot()` once and then spends normal runtime in `k_sleep(K_SECONDS(1))`.
- Feeding a watchdog from this idle loop would violate both REC-01 and the roadmap success criterion that critical execution paths are supervised rather than a single generic feed loop.
- Any Phase 3 design that puts `task_wdt_feed()` into `main()` would be a regression back toward pseudo-global, non-specific liveness logic.

### The repo already exposes a useful status contract, but not yet a liveness contract

- `app/src/network/network_supervisor.h` exposes `network_supervisor_get_status()`, which already publishes connectivity state, Wi-Fi/DHCP status, reachability outcome, leased IPv4 address, and `last_failure`.
- `app/src/network/network_state.h` already carries retry timing, startup timing, structured failure state, and the runtime booleans that explain what the network layer is doing.
- What is still missing for REC-01 and REC-02 is recovery-specific progress memory: last meaningful progress time, degraded-incident timing, stable-since timing, watchdog channel ownership, reboot-suppression state after a recovery-triggered reset, and the preserved last recovery-reset cause.
- Phase 3 should add only the minimum extra fields needed for liveness and escalation. It should not replace the Phase 2 connectivity/failure contract with a second ad hoc status model.

### The cleanest ownership boundary is a real `recovery_manager`, not reboot logic inside networking

- `app/src/recovery/recovery.h` already reserves the recovery subsystem seam, while `app/CMakeLists.txt` does not yet compile any recovery source file.
- `app/src/app/app_context.h` currently owns configuration plus network runtime state by value, making it the natural place to add recovery ownership without reopening the Phase 1 boundary decision.
- `app/src/network/network_supervisor.c` already uses "recovery" to mean self-healing retry through `network_supervisor_schedule_recovery()` and `network_supervisor_start_recovery_bringup()`, so full-device reset policy should stay outside the network module to avoid conflating retry with escalation.
- The practical split is: networking keeps self-healing and status publication; recovery keeps liveness judgment, reboot breadcrumbing, watchdog ownership, and the final restart decision.

### The pinned Zephyr/NCS stack already supports the layered watchdog model this phase needs

- The local SDK exposes Zephyr task watchdog APIs in `.ncs/v3.2.1/zephyr/include/zephyr/task_wdt/task_wdt.h`, including `task_wdt_init()`, `task_wdt_add()`, `task_wdt_feed()`, and `task_wdt_delete()`.
- `.ncs/v3.2.1/zephyr/samples/subsys/task_wdt/src/main.c` shows the intended layered pattern: task watchdog first, hardware watchdog as fallback, and explicit `sys_reboot(SYS_REBOOT_COLD)` when a timeout is genuinely unrecoverable.
- `.ncs/v3.2.1/zephyr/include/zephyr/drivers/hwinfo.h` exposes reset-cause APIs, and `.ncs/v3.2.1/zephyr/tests/boards/nrf/hwinfo/reset_cause/src/main.c` demonstrates a small `.noinit` breadcrumb plus reset-cause inspection on Nordic targets.
- `app/prj.conf` already enables `CONFIG_STACK_SENTINEL=y` and `CONFIG_INIT_STACKS=y`, so the repo already has defensive runtime diagnostics; Phase 3 can add watchdog supervision without inventing a custom fault-detection foundation first.

### `app/prj.conf` already contains low-level Wi-Fi resilience, so Phase 3 must stay above that layer

- `app/prj.conf` already enables `CONFIG_NRF_WIFI_RPU_RECOVERY=y`.
- That means some Wi-Fi/RPU faults are already being recovered below the application.
- Phase 3 should therefore treat repeated degraded network behavior as evidence for an application-level no-progress policy, not as an automatic reboot trigger by itself.
- Ordinary Wi-Fi disconnects, DHCP churn, and reachability failure remain Phase 2 self-healing behavior unless they become a long-enough no-progress condition.

### Some repo planning docs lag the actual post-Phase-2 codebase

- `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/CONVENTIONS.md`, `.planning/codebase/STACK.md`, and `.planning/codebase/STRUCTURE.md` still describe the older single-file `main.c` app.
- The actual repo now has `app/src/app/bootstrap.c`, `app/src/network/network_supervisor.c`, `app/src/network/wifi_lifecycle.c`, and `app/src/network/reachability.c`, and `app/CMakeLists.txt` already compiles them.
- For Phase 3 seam mapping, the actual source tree plus the Phase 1/2 research/state artifacts are the reliable source of truth.

### Phase 3 must stop before panel and persistence-era work

- REC-01 and REC-02 do not require new HTTP endpoints, operator-facing status pages, flash-backed incident history, or runtime-tunable recovery policy.
- The context only asks for the most recent recovery-reset cause across reboot, not a general persistence layer or reboot-streak database.
- The clean boundary is one minimal preserved recovery breadcrumb and runtime liveness logic only.

</findings>

<recommendation>
## Recommended Implementation Shape

### Recommended module responsibilities

- `app/src/recovery/recovery.{h,c}`
  - own task watchdog initialization and hardware watchdog fallback selection
  - own recovery evaluation work and the startup/runtime supervision channels
  - own the preserved last-recovery-reset breadcrumb and boot-time inspection/logging
  - own the final `sys_reboot(SYS_REBOOT_COLD)` call after confirmed no-progress or watchdog starvation

- `app/src/app/bootstrap.c`
  - initialize the recovery manager after network state initialization
  - arm a startup guard around `network_supervisor_start()`
  - hand off from startup supervision to runtime supervision once startup settles
  - log any preserved last recovery-reset cause before treating the new boot as normal

- `app/src/app/app_context.h`
  - extend the shared application context with recovery ownership by value

- `app/src/network/network_supervisor.{h,c}`
  - remain the owner of connectivity retry, reachability decisions, and structured network failure reporting
  - expose status snapshots and any minimal extra progress signal needed by recovery
  - stay free of direct reboot or watchdog policy

- `app/src/network/wifi_lifecycle.c`
  - stay callback-light and low-level
  - continue to surface edges and reschedule work instead of feeding watchdogs or deciding escalations from NET_MGMT context

### Recommended supervision model

- Use Zephyr task watchdog as the Phase 3 software supervisor and configure hardware watchdog fallback underneath it.
- Register at least two recovery-owned channels:
  1. a startup guard channel that wraps the bounded `network_supervisor_start()` window
  2. a runtime network-progress channel that supervises post-boot liveness
- Feed channels from recovery-owned checkpoints, not from `main()` and not directly from Wi-Fi callbacks.
- Define meaningful progress as any of:
  - connectivity-state change
  - Wi-Fi ready/connect/disconnect edge
  - DHCP bind/loss edge
  - reachability result change
  - completion of the bounded startup contract
  - surviving in an acceptable stable state long enough to satisfy the stability window
- Treat these states as non-stuck by themselves:
  - `NETWORK_CONNECTIVITY_HEALTHY`
  - `NETWORK_CONNECTIVITY_LAN_UP_UPSTREAM_DEGRADED`
  - short-lived `NETWORK_CONNECTIVITY_DEGRADED_RETRYING` while inside the patience window
- Treat a condition as confirmed stuck only when the device fails to make meaningful progress for the configured patience window, or when watchdog starvation itself fires.
- When a condition is confirmed stuck, record the recovery breadcrumb, log it, and call `sys_reboot(SYS_REBOOT_COLD)`.

### Recommended recovery and breadcrumb model

Keep the Phase 3 model deliberately small. A good minimum is:

- runtime-only recovery fields
  - last meaningful progress timestamp
  - degraded-incident start timestamp
  - stable-since timestamp
  - startup-guard active flag
  - runtime supervision active flag
  - task watchdog channel ids

- preserved across reboot
  - whether the last reboot was recovery-triggered
  - trigger type (`confirmed-stuck` vs `watchdog-starvation`)
  - last failure stage
  - failure reason
  - last published connectivity state

A small `.noinit` breadcrumb inside `recovery.c`, optionally paired with `hwinfo_get_reset_cause()`, is a better Phase 3 fit than introducing a generic persistence subsystem early.

### Recommended configuration posture

- Add explicit `CONFIG_APP_*` recovery knobs in `app/Kconfig` for:
  - recovery assessment cadence
  - no-progress patience window
  - post-reboot stability window before a prior recovery incident is considered cleared
- Enable the Zephyr/NCS primitives in `app/prj.conf` that make the phase real:
  - `CONFIG_WATCHDOG=y`
  - `CONFIG_WDT_DISABLE_AT_BOOT=y`
  - `CONFIG_TASK_WDT=y`
  - `CONFIG_TASK_WDT_CHANNELS` sized for the startup/runtime channels
  - `CONFIG_TASK_WDT_MIN_TIMEOUT` aligned with the shortest recovery-owned watchdog window
  - `CONFIG_HWINFO=y` if reset-cause reporting is used
- Keep all thresholds build-time for v1. Do not add operator-set recovery tuning or flash-stored policy in this phase.

### Scope guards

- Do not move retry/backoff or reachability policy out of `network_supervisor.c`.
- Do not add `sys_reboot()` or watchdog feeding to `app/src/network/wifi_lifecycle.c`.
- Do not feed a watchdog from the idle loop in `app/src/main.c`.
- Do not introduce panel endpoints, persistence schemas, or reboot-history databases in this phase.

</recommendation>

<validation>
## Validation Architecture

### Existing validation assets to reuse

- `./scripts/validate.sh` is already the canonical automated entrypoint.
- `./scripts/build.sh` remains the canonical automated build signal.
- `scripts/common.sh` already centralizes reusable marker text for device checklists.
- `README.md` already documents the build-first plus blocking hardware sign-off pattern established in Phase 2.
- `./scripts/flash.sh` and `./scripts/console.sh` remain the real-device approval path.

### Recommended Phase 3 validation strategy

- Keep automated feedback build-first through `./scripts/validate.sh`; Phase 3 still does not justify introducing a new host-side test framework.
- Add per-plan static/build checks that prove recovery wiring, scope, and ownership boundaries.
- Reserve device-level validation for stuck detection, reboot semantics, and post-reboot behavior that cannot be proven from build output alone.

### Nyquist-ready automated checks by plan

For `03-01` (implementation and wiring), the validation generator should expect checks like:

1. **Recovery module exists and owns watchdog logic**
   - `test -f app/src/recovery/recovery.c && rg -n "recovery_manager_|task_wdt_|sys_reboot|hwinfo_" app/src/recovery/recovery.c app/src/recovery/recovery.h`
2. **Composition is wired through app bootstrap, not `main.c`**
   - `rg -n "recovery" app/CMakeLists.txt app/src/app/app_context.h app/src/app/bootstrap.c && ! rg -n "task_wdt_feed|sys_reboot" app/src/main.c`
3. **Recovery configuration is explicit**
   - `rg -n "APP_RECOVERY_" app/Kconfig && rg -n "WATCHDOG|TASK_WDT|WDT_DISABLE_AT_BOOT|HWINFO" app/prj.conf`
4. **Networking stays free of escalation policy**
   - `! rg -n "sys_reboot|task_wdt" app/src/network/wifi_lifecycle.c app/src/network/network_supervisor.c && rg -n "network_supervisor_get_status|last_failure|connectivity_state" app/src/network/network_supervisor.h app/src/network/network_supervisor.c`

For `03-02` (validation and sign-off), the generator should expect checks like:

1. **Validation/docs surfaces mention recovery semantics**
   - `bash -n scripts/validate.sh scripts/common.sh && rg -n "watchdog|recovery|reset|stuck" README.md scripts/common.sh scripts/validate.sh`
2. **Phase-level automated gate still runs through the repo entrypoint**
   - `./scripts/validate.sh`
3. **Manual gate instructions mention both non-reset and reset scenarios**
   - `rg -n "healthy|degraded|reset|reboot|recovery" README.md scripts/common.sh scripts/validate.sh`

### Minimum manual scenarios

1. **Healthy boot with supervision armed**
   - Device reaches `APP_READY` without recovery reset and logs the normal startup path.
2. **Transient network disruption does not reboot**
   - AP loss, DHCP churn, or upstream reachability failure inside the patience window stays within Phase 2 self-healing behavior and recovers without device restart.
3. **Confirmed stuck during startup escalates once**
   - Hold the boot path in a no-progress degraded state beyond the patience window and confirm breadcrumb/log plus full device reboot.
4. **Confirmed stuck during runtime escalates once**
   - Force a long-enough no-progress runtime degraded condition and confirm the same recovery path.
5. **Post-reboot suppression prevents rapid loops**
   - After a recovery-triggered reboot, keep the fault present and confirm the device does not immediately thrash into repeated resets before the stability policy allows another escalation.
6. **Recovered stability clears the incident**
   - Once the device has remained acceptably stable for the configured window, confirm the previous recovery incident clears and future true stuck conditions can escalate again.

### Nyquist implications

- Every implementation task should have a fast static/build verification command.
- Hardware-only reset semantics should be concentrated in Plan 2 so Plan 1 can keep tight feedback loops.
- The phase stays Nyquist-viable without a new unit-test framework by combining build-first static checks with explicit manual recovery checkpoints and updated repo-owned validation docs.

</validation>

<planning_implications>
## Planning Implications

- The roadmap's two-plan split is correct and should be preserved.
- Plan 1 should establish the recovery module, watchdog config, boot/runtime supervision channels, progress/stability bookkeeping, and the preserved last recovery-reset cause.
- Plan 2 should update the validation flow and complete real-device sign-off for transient faults, confirmed stuck conditions, and anti-thrash reboot behavior.
- Plan 2 should depend on Plan 1.
- Phase 3 should deliberately leave panel consumption, flash-backed incident history, runtime recovery tuning, and wider persistence concerns to later phases.

</planning_implications>

---
*Phase: 03-recovery-watchdog*
*Research completed: 2026-03-09*
