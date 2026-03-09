# Phase 3: Recovery & Watchdog - Context

**Gathered:** 2026-03-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Add layered watchdog and recovery behavior that supervises critical execution paths, distinguishes true stuck conditions from ordinary degraded operation, and escalates to a full-device restart only for confirmed unrecoverable states. This phase builds on the Phase 2 network supervisor and status contract, and it should not add new operator-facing capabilities beyond conservative fault recovery.

</domain>

<decisions>
## Implementation Decisions

### Stuck criteria
- Ordinary Wi-Fi disruption, DHCP churn, or failed reachability should not count as “stuck” on their own; Phase 2 background retry behavior remains the normal response.
- Phase 3 may treat a sufficiently long degraded period as a true stuck condition even if the device is still technically alive, as long as the same rule applies to startup and post-boot runtime.
- A path that repeatedly fails and then recovers should not be escalated just for being noisy; it only becomes unrecoverable when it stops making acceptable forward progress or remains degraded beyond the allowed patience window.
- The user prefers a simple, predictable definition of liveness for Phase 3 rather than a dense set of custom progress semantics for each supervised path.

### Escalation ladder
- Once Phase 3 decides a condition is genuinely stuck, it should not spend time on a multi-stage local recovery ladder.
- The preferred escalation is a direct device-wide restart rather than targeted subsystem-only recovery.
- The restart decision should still be tied to a no-meaningful-progress bar, not to every transient failure or every repeat error.
- Before the forced restart, the device should preserve or log the current failure stage, reason, and recovery path so later phases can explain why the reset happened.

### After reboot behavior
- After a recovery-triggered reboot, the device should attempt the normal Phase 2 startup contract again rather than inventing a separate recovery boot mode.
- Phase 3 should avoid rapid reboot loops; if the same fault returns, the system should favor a non-thrashing degraded posture over endless immediate resets.
- Recovery memory that survives the reboot only needs to preserve the most recent recovery-reset cause; a richer cross-boot streak counter is not required by the user.
- A recovery incident should clear only after the device both settles and remains meaningfully stable for a while, not immediately at the first settled state and not only when full healthy connectivity returns.

### Claude's Discretion
- Exact duration thresholds for “long degraded,” “stable run,” and the confirmation window before declaring a condition stuck.
- Exact watchdog and supervision mechanism, including whether it uses task watchdog, hardware watchdog, or a layered combination, as long as behavior matches the decisions above.
- Exact retention method for preserving the most recent recovery-reset cause across reboot.
- Exact log field names, status payload shape, and breadcrumb format for the preserved stage history.

</decisions>

<specifics>
## Specific Ideas

- No external product references or UI metaphors were given for this phase.
- Predictability matters more than sophistication: ordinary network trouble should still self-heal in the background, but a long-enough degraded state can eventually be treated as truly stuck.
- Once a condition is confirmed as stuck, the user is comfortable with a direct whole-device recovery path as long as the device preserves enough evidence to explain the reset later.

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `app/src/network/network_supervisor.c` already owns startup settlement, background retry scheduling, connectivity-state transitions, and structured last-failure recording; Phase 3 should supervise this existing runtime path rather than re-creating network policy elsewhere.
- `app/src/network/network_state.h` already centralizes mutable runtime state, semaphores, retry work, and the published failure contract; it is the natural carrier for recovery-visible state that must survive normal runtime transitions.
- `app/src/app/bootstrap.c` already acts as the boot orchestration seam and logs the current startup outcome; it is the natural place to initialize recovery supervision and connect boot-time escalation decisions to runtime behavior.
- `app/src/recovery/recovery.h` already reserves the subsystem boundary for a recovery manager, so Phase 3 can add real behavior without inventing a new top-level seam.
- `README.md` plus `scripts/validate.sh` already establish the project’s build-first default and separate device-only sign-off pattern, which fits a recovery phase that will still need manual hardware verification.

### Established Patterns
- `main.c` remains a composition root followed by a simple idle loop, so long-lived recovery policy should stay behind subsystem boundaries rather than moving business logic back into `main.c`.
- Phase 2 already locked a named connectivity-state model plus persistent `last_failure` reporting; Phase 3 should extend that contract instead of introducing a parallel ad hoc fault-reporting path.
- Current long-lived runtime behavior already uses Zephyr semaphores, delayable work, and structured `LOG_INF` / `LOG_WRN` / `LOG_ERR` logging; recovery work should fit that operating style.
- Boot now continues after healthy or explicit degraded startup outcomes, so Phase 3 should not reintroduce indefinite startup blocking while adding watchdog logic.
- The current project config enables stack sentinels and init-stack diagnostics but does not yet define watchdog-specific configuration, making Phase 3 the first place where watchdog policy becomes an explicit project concern.

### Integration Points
- `app_boot()` is the primary seam for wiring recovery initialization, boot-time supervision, and any escalation handoff.
- `network_supervisor_start()` and the supervisor retry work are the live critical execution paths that Phase 3 can supervise immediately.
- `struct app_context` currently carries config plus network runtime state by value; Phase 3 can extend this shared context for recovery ownership without reopening the Phase 1 boundary decisions.
- Future panel and auth phases are already positioned to consume published status, so any preserved recovery-reset cause should remain accessible through shared runtime contracts rather than hidden inside isolated implementation details.

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 03-recovery-watchdog*
*Context gathered: 2026-03-09*
